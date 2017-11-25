#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>

#include<asm/uaccess.h>
#include<linux/cdev.h>
#include<linux/proc_fs.h>

#define MAX_SIZE 100

static char proc_buf[MAX_SIZE];
static struct proc_dir_entry *proc_entry;

MODULE_DESCRIPTION("My hello module");
MODULE_AUTHOR("claude51315@gmail.com");
MODULE_LICENSE("MIT");


static int read_proc(char* buf, char **start,off_t offset, int count, int *eof, void *data)
{
    int len=0;
    len = sprintf(buf,"%s\n", proc_buf);
    return len;
}
static int write_proc(struct file *file, const char *buf, int count, void *data)
{
    if(count > MAX_SIZE)
        count = MAX_SIZE;
    if(copy_from_user(proc_buf, buf, count))
        return -EFAULT;
    return count;
}


static int init_proc(void)
{
    proc_entry=create_proc_entry("miao_proc", 0666, NULL);
    if(!proc_entry) {
        printk("Initial proc entry fail\n");
        return -ENOMEM;
    }
    memset(proc_buf, '\0', sizeof(proc_buf));
    proc_entry->read_proc = read_proc;
    proc_entry->write_proc = write_proc;
    printk("initial proc entry success!\n");
    return 0;
}


static int init_qq(void)
{
    init_proc();
    printk("hello kernel!\n");
    return 0;
}

static void cleanup_qq(void)
{
    remove_proc_entry("maio_proc", NULL);
    printk("exit kernel\n");
}
module_init(init_qq);
module_exit(cleanup_qq);

/*
    issue: netpoll doesn't supported. 
*/
/*
#include<linux/netpoll.h>
static struct netpoll* np = NULL;
static struct netpoll np_t;
static void init_netpoll(void)
{
    np_t.name= "Claude";
    strlcpy(np_t.dev_name, "wlan0", IFNAMSIZ);
    np_t.local_ip=htonl((unsigned long int) 0xc0a80128);
    //np_t.local_ip.in.s_addr=htonl((unsigned long int) 0xc0a80128);
    // 140.113.210.122
    np_t.remote_ip=htonl((unsigned long int) 0x8c71d27a);
    //np_t.remote_ip.in.s_addr=htonl((unsigned long int) 0x8c71d27a);

    //np_t.ipv6=0;
    np_t.local_port=5566;
    np_t.remote_port=5566;
    memset(np_t.remote_mac, 0xff, ETH_ALEN);
    netpoll_print_options(&np_t);
    netpoll_setup(&np_t);
    np = &np_t;
    return;
}
void sendUdp(const char* buf)
{
    int len = strlen(buf);
    netpoll_send_udp(np, buf, len);
}
*/
