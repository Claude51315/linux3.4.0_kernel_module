#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>


#include<asm/uaccess.h>
#include<linux/cdev.h>
#include<linux/proc_fs.h>

#define MAX_SIZE 100


#include<net/sock.h>
#include<linux/netlink.h>
#include<linux/skbuff.h>
#include<linux/bio.h>
#include<linux/kprobes.h>

#include<linux/time.h>

#include<linux/mutex.h>


#ifndef COMMON_H
#define COMMON_H
#define DEBUG 1

#define debug_print(fmt, ...) \
    do {if(DEBUG) printk("%s:%d" fmt, __FUNCTION__, __LINE__, __VA_ARGS__); \
    }while(0)




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
  LRUC_VALUE_TOO_LARGE
} lruc_error;

// ------------------------------------------
// types
// ------------------------------------------

typedef struct {
  void      *value;
  void      *key;
  int  value_length;
  int  key_length;
  int  access_count;
  void      *next;
} lruc_item;

typedef struct {
  lruc_item **items;
  int  access_count;
  int  free_memory;
  int  total_memory;
  int  average_item_length;
  int  hash_table_size;
  time_t    seed;
  lruc_item *free_items;
  /* use mutex in linux kernel */
  //pthread_mutex_t *mutex;
  struct mutex *mutex;
} lruc;




#endif
