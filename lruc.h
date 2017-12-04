
#include "common.h"
#ifndef LRUC_H
#define LRUC_H
/* LRU C implementaion in https://github.com/willcannings/C-LRU-Cache */

// ------------------------------------------
// errors
// ------------------------------------------
typedef enum {
  LRUC_NO_ERROR = 0,
  LRUC_MISSING_CACHE,
  LRUC_MISSING_KEY,
  LRUC_MISSING_VALUE,
  LRUC_PTHREAD_ERROR,
  LRUC_VALUE_TOO_LARGE,
  LRUC_SET_ERROR
} lruc_error;

// ------------------------------------------
// types
// ------------------------------------------

typedef struct {
  void      *value;
  uint32_t      key;
  int  value_length;
  int  key_length;
  int  access_count;
  void      *next;
} lruc_item;

typedef struct {
  lruc_item **items;
  int  access_count;
  int  free_count;
  int  total_memory;
  int  cache_size;
  time_t    seed;
  lruc_item *free_items, *head;
  /* use mutex in linux kernel */
  //pthread_mutex_t *mutex;
  struct mutex *mutex;
} lruc;
// ------------------------------------------
// public api
// ------------------------------------------
uint32_t lruc_hash(lruc *cache,  uint32_t key, uint32_t key_length);
lruc *lruc_new(uint64_t cache_size);
lruc_error lruc_set(lruc *cache, uint32_t key, uint32_t key_length, void *value, uint32_t value_length);
lruc_error lruc_get(lruc *cache, uint32_t key, uint32_t key_length, void **value);
lruc_error lruc_find(lruc *cache, uint32_t key, uint32_t key_length);
lruc_error lruc_delete(lruc *cache, uint32_t key, uint32_t key_length);
lruc_error lruc_free(lruc *cache);
#endif
