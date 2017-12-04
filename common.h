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
#include<linux/lzo.h>

#ifndef COMMON_H
#define COMMON_H
#define DEBUG 1

#define debug_print(fmt, ...) \
    do {if(DEBUG) printk("%s:%d" fmt, __FUNCTION__, __LINE__, __VA_ARGS__); \
    }while(0)

int diff_4KB(uint32_t sector, unsigned char* a, unsigned char* b);
int lzo_compress(char *data, int len, unsigned char* wrkmem, unsigned char* dst_buf);

#endif
