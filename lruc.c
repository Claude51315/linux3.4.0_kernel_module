#include "common.h"



// error helpers
#define error_for(conditions, error)  if(conditions) {return error;}
#define test_for_missing_cache()      error_for(!cache, LRUC_MISSING_CACHE)
#define test_for_missing_key()        error_for(!key || key_length == 0, LRUC_MISSING_KEY)
#define test_for_missing_value()      error_for(!value || value_length == 0, LRUC_MISSING_VALUE)
#define test_for_value_too_large()    error_for(value_length > cache->total_memory, LRUC_VALUE_TOO_LARGE)

// lock helpers
#define lock_cache()   ;
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
int lruc_hash(lruc *cache, void *key, int key_length) {
  int m = 0x5bd1e995;
  int r = 24;
  int h = cache->seed ^ key_length;
  char* data = (char *)key;

  while(key_length >= 4) {
    int k = *(int *)data;
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
  return h % cache->hash_table_size;
}

// compare a key against an existing item's key
int lruc_cmp_keys(lruc_item *item, void *key, int key_length) {
  if(key_length != item->key_length)
    return 1;
  else
    return memcmp(key, item->key, key_length);
}

// remove an item and push it to the free items queue
void lruc_remove_item(lruc *cache, lruc_item *prev, lruc_item *item, int hash_index) {
  if(prev)
    prev->next = item->next;
  else
    cache->items[hash_index] = (lruc_item *) item->next;
  
  // free memory and update the free memory counter
  cache->free_memory += item->value_length;
  //free(item->value);
  //free(item->key);
  
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
  int i = 0, min_index = -1;
  uint64_t min_access_count = -1;
  
  for(; i < cache->hash_table_size; i++) {
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
  
  if(min_item)
    lruc_remove_item(cache, min_prev, min_item, min_index);
}

// pop an existing item off the free queue, or create a new one
lruc_item *lruc_pop_or_create_item(lruc *cache) {
  lruc_item *item = NULL;
  
  if(cache->free_items) {
    item = cache->free_items;
    cache->free_items = item->next;
  } else {
    item = (lruc_item *) lru_alloc(sizeof(lruc_item), 1);
  }
  
  return item;
}


// ------------------------------------------
// public api
// ------------------------------------------
lruc *lruc_new(uint64_t cache_size, int average_length) {
  // create the cache
  lruc *cache = (lruc *) lru_alloc(sizeof(lruc), 1);
  if(!cache) {
    printk("LRU Cache unable to create cache object");
    return NULL;
  }
  //cache->hash_table_size      = cache_size / average_length;
  cache->hash_table_size      = cache_size;
  cache->average_item_length  = average_length;
  cache->free_memory          = cache_size;
  cache->total_memory         = cache_size;
  cache->seed                 = 0;
  
  // size the hash table to a guestimate of the number of slots required (assuming a perfect hash)
  cache->items = (lruc_item **) lru_alloc(sizeof(lruc_item *), cache->hash_table_size);
  if(!cache->items) {
    printk("LRU Cache unable to create cache hash table");
    lru_free(cache);
    return NULL;
  }
  
  // all cache calls are guarded by a mutex
  /*
  cache->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  if(pthread_mutex_init(cache->mutex, NULL)) {
    printk("LRU Cache unable to initialise mutex");
    free(cache->items);
    free(cache);
    return NULL;
  }
  */
  return cache;
}


lruc_error lruc_free(lruc *cache) {
  int i = 0;
  lruc_item *item = NULL, *next = NULL;
  
  test_for_missing_cache();
  // free each of the cached items, and the hash table
  if(cache->items) {
    for(; i < cache->hash_table_size; i++) {
      item = cache->items[i];    
      while(item) {
        next = (lruc_item *) item->next;
        lru_free(item);
        item = next;
      }
    }
    lru_free(cache->items);
  }
  
  // free the cache
  /*
  if(cache->mutex) {
    if(pthread_mutex_destroy(cache->mutex)) {
      printk("LRU Cache unable to destroy mutex");
      return LRUC_PTHREAD_ERROR;
    }
  }
  */
  lru_free(cache);
  
  return LRUC_NO_ERROR;
}


lruc_error lruc_set(lruc *cache, void *key, int key_length, void *value, int value_length) {
  int hash_index , required;
  lruc_item *item = NULL, *prev = NULL;
  test_for_missing_cache();
  test_for_missing_key();
  test_for_missing_value();
  test_for_value_too_large();
  lock_cache();
  
  // see if the key already exists
  hash_index = lruc_hash(cache, key, key_length);
  required = 0;
  item = cache->items[hash_index];
  
  while(item && lruc_cmp_keys(item, key, key_length)) {
    prev = item;
    item = (lruc_item *) item->next;
  }
  
  if(item) {
    // update the value and value_lengths
    required = value_length - item->value_length;
    //free(item->value);
    item->value = value;
    item->value_length = value_length;
    
  } else {
    // insert a new item
    item = lruc_pop_or_create_item(cache);
    item->value = value;
    item->key = key;
    item->value_length = value_length;
    item->key_length = key_length;
    required = value_length;
    
    if(prev)
      prev->next = item;
    else
      cache->items[hash_index] = item;
  }
  item->access_count = ++cache->access_count;
  
  // remove as many items as necessary to free enough space
  if(required > 0 && required > cache->free_memory) {
    while(cache->free_memory < required)
      lruc_remove_lru_item(cache);
  }
  cache->free_memory -= required;
  unlock_cache();
  return LRUC_NO_ERROR;
}


lruc_error lruc_get(lruc *cache, void *key, int key_length, void **value) {
  int hash_index;
  lruc_item *item = NULL;
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


lruc_error lruc_delete(lruc *cache, void *key, int key_length) {
  int hash_index;
  lruc_item *item = NULL, *prev = NULL;
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
