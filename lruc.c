#include "lruc.h"



// error helpers
#define error_for(conditions, error)  if(conditions) {return error;}
#define test_for_missing_cache()      error_for(!cache, LRUC_MISSING_CACHE)
#define test_for_missing_key()        error_for(key_length == 0, LRUC_MISSING_KEY)
#define test_for_missing_value()      error_for(!value || value_length == 0, LRUC_MISSING_VALUE)
#define test_for_value_too_large()    error_for(value_length > cache->total_memory, LRUC_VALUE_TOO_LARGE)

// lock helpers
#define lock_cache();
/*
if(pthread_mutex_lock(cache->mutex)) {\
  printk("LRU Cache unable to obtain mutex lock");\
  return LRUC_PTHREAD_ERROR;\
}
*/
#define unlock_cache() ;
/*
if(pthread_mutex_unlock(cache->mutex)) {\
  printk("LRU Cache unable to release mutex lock");\
  return LRUC_PTHREAD_ERROR;\
}
*/

// memory allocating helper
#define lru_alloc(unit_size, count)   \
        kmalloc(unit_size * count , GFP_KERNEL)
#define lru_free(ptr) \
        kfree(ptr)

// ------------------------------------------
// private functions
// ------------------------------------------
// MurmurHash2, by Austin Appleby
// http://sites.google.com/site/murmurhash/
uint32_t lruc_hash(lruc *cache,  uint32_t key, uint32_t key_length) {
  uint32_t m = 0x5bd1e995;
  uint32_t r = 24;
  uint32_t h = cache->seed ^ key_length;
  char * data = (char *)&key;

  while(key_length >= 4) {
    uint32_t k = key;
    k *= m;
    k ^= k >> r;
    k *= m;
    h *= m;
    h ^= k;
    data += 4;
    key_length -= 4;
  }
  
  switch(key_length) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0];
            h *= m;
  };

  h ^= h >> 13;
  h *= m;
  h ^= h >> 15;
  return h % cache->cache_size;
}
// compare a key against an existing item's key
int lruc_cmp_keys(lruc_item *item, uint32_t key, uint32_t key_length) {
  if(key_length != item->key_length || key != item->key)
    return 1;
  return 0;
}

// remove an item and push it to the free items queue
void lruc_remove_item(lruc *cache, lruc_item *prev, lruc_item *item, uint32_t hash_index) {
  if(prev)
    prev->next = item->next;
  else
    cache->items[hash_index] = (lruc_item *) item->next;
  // push the item to the free items queue  
  memset(item, 0, sizeof(lruc_item));
  item->next = cache->free_items;
  cache->free_items = item;
}

// remove the least recently used item
// TODO: we can optimise this by finding the n lru items, where n = required_space / average_length
void lruc_remove_lru_item(lruc *cache) {
  lruc_item *min_item = NULL, *min_prev = NULL;
  lruc_item *item = NULL, *prev = NULL;
  uint32_t i = 0, min_index = -1;
  uint64_t min_access_count = -1;
  if(cache->free_count == cache->cache_size)
    return;

  for(; i < cache->cache_size; i++) {
    item = cache->items[i];
    prev = NULL;
    
    while(item) {
      if(item->access_count < min_access_count || min_access_count == -1) {
        min_access_count = item->access_count;
        min_item  = item;
        min_prev  = prev;
        min_index = i;
      }
      prev = item;
      item = item->next;
    }
  }
  
  if(min_item) {
    lruc_remove_item(cache, min_prev, min_item, min_index);
    cache->free_count ++;
  }
}

// pop an existing item off the free queue, or create a new one
lruc_item *lruc_get_free_item(lruc *cache) {
  lruc_item *item = NULL;
  if(cache->free_count == 0){
    lruc_remove_lru_item(cache);
  }
  if(cache->free_items) {
    item = cache->free_items;
    cache->free_items = item->next;
    item->next = NULL;
    cache->free_count --;
  } 
  return item;
}


// ------------------------------------------
// public api
// ------------------------------------------
lruc *lruc_new(uint64_t cache_size) {
  // create the cache
  int i;
  lruc *cache = (lruc *) lru_alloc(sizeof(lruc), GFP_KERNEL);
  if(!cache) {
    printk(KERN_DEBUG"LRU Cache unable to create cache object\n");
    return NULL;
  }
  cache->free_count         = cache_size;
  cache->total_memory         = cache_size;
  cache->seed                 = 1512356891;
  cache->cache_size           = cache_size; 
  
  // allocate memory
  cache->items = (lruc_item **) lru_alloc(sizeof(lruc_item *) * cache->cache_size, GFP_KERNEL);
  cache->free_items = lru_alloc(sizeof(lruc_item) * cache_size, GFP_KERNEL);
  cache->mutex = lru_alloc(sizeof(struct mutex), GFP_KERNEL);
  if(!cache->free_items  || ! cache->items || !cache->mutex) {
    printk("memory is not suffiecient!\n");
    goto free; 
  }  
  // initialization
  memset(cache->items, 0, sizeof(lruc_item *) * cache->cache_size);
  memset(cache->free_items, 0, sizeof(lruc_item) * cache_size);
  for( i = 0 ; i < cache_size-1 ; i++) {
    cache->free_items[i].next = &cache->free_items[i+1];
  }
  cache->free_items[cache_size-1].next = NULL;
  cache->head = cache->free_items; 
  mutex_init(cache->mutex);
  return cache;
free:
    lru_free(cache->mutex);
    lru_free(cache->items);
    lru_free(cache);
  return NULL;
}


lruc_error lruc_free(lruc *cache) {
 
  test_for_missing_cache();
  
  // free each of the cached items, and the hash table
  lru_free(cache->items);
  lru_free(cache->head);
  lru_free(cache->mutex);
  lru_free(cache);
  return LRUC_NO_ERROR;
}


lruc_error lruc_set(lruc *cache, uint32_t key, uint32_t key_length, void *value, uint32_t value_length) {
  lruc_item *item = NULL, *prev = NULL;
  uint32_t hash_index;
  test_for_missing_cache();
  //test_for_missing_key();
  test_for_missing_value();
  //test_for_value_too_large();
  lock_cache();
  
  // see if the key already exists
  hash_index = lruc_hash(cache, key, key_length);
  item = cache->items[hash_index];
  
  while(item && lruc_cmp_keys(item, key, key_length)) {
    prev = item;
    item = (lruc_item *) item->next;
    
  }
  
  if(item) {
    item->value = value;
    item->value_length = value_length;
  } else {
    // insert a new item
    item = lruc_get_free_item(cache);
    if(item == NULL) {
        printk("QAQ\n");
    	return LRUC_SET_ERROR;
    }
    item->value = value;
    item->key = key;
    item->value_length = value_length;
    item->key_length = key_length;
    //required = value_length;
    if(prev)
      prev->next = item;
    else
      cache->items[hash_index] = item;
  }
  item->access_count = ++cache->access_count;
  
  unlock_cache();
  return LRUC_NO_ERROR;
}
lruc_error lruc_find(lruc *cache, uint32_t key, uint32_t key_length) 
{
  lruc_item *item = NULL, *prev = NULL;
  uint32_t hash_index;
  test_for_missing_cache();
  test_for_missing_key();
  // see if the key already exists
  hash_index = lruc_hash(cache, key, key_length);
  item = cache->items[hash_index];
  while(item && lruc_cmp_keys(item, key, key_length)) {
    prev = item;
    item = (lruc_item *) item->next;
  }
  if(item) {
    return 1;
  } else {
    return 0;
  }
  return LRUC_NO_ERROR;
}

lruc_error lruc_get(lruc *cache, uint32_t key, uint32_t key_length, void **value) {
  uint32_t hash_index;
  lruc_item *item;
  test_for_missing_cache();
  test_for_missing_key();
  lock_cache();
  
  // loop until we find the item, or hit the end of a chain
  hash_index = lruc_hash(cache, key, key_length);
  item = cache->items[hash_index];
  
  while(item && lruc_cmp_keys(item, key, key_length))
    item = (lruc_item *) item->next;
  if(item) {
    *value = item->value;
    item->access_count = ++cache->access_count;
  } else {
    *value = NULL;
  }
  unlock_cache();
  return LRUC_NO_ERROR;
}


lruc_error lruc_delete(lruc *cache, uint32_t key, uint32_t key_length) {
  lruc_item *item = NULL, *prev = NULL;
  uint32_t hash_index;
  test_for_missing_cache();
  test_for_missing_key();
  lock_cache();
  
  // loop until we find the item, or hit the end of a chain
  hash_index = lruc_hash(cache, key, key_length);
  item = cache->items[hash_index];
  
  while(item && lruc_cmp_keys(item, key, key_length)) {
    prev = item;
    item = (lruc_item *) item->next;
  }
  
  if(item) {
    lruc_remove_item(cache, prev, item, hash_index);
  }
  
  unlock_cache();
  return LRUC_NO_ERROR;
}
