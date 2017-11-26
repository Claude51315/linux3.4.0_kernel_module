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

MODULE_DESCRIPTION("My hello module");
MODULE_AUTHOR("claude51315@gmail.com");


#define NETLINK_MYTRACE 30
/* netlink interface */
#define NETLINK_USER 31

static struct sock *nl_sk = NULL;

static void nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid, res;
    pid  = res = 0;
    nlh = (struct nlmsghdr*)skb->data;
    printk("%s : msg = %s", __FUNCTION__, (char *)nlmsg_data(nlh));
}


static int init_netlink(void)
{
    printk("%s\n", __FUNCTION__);
    nl_sk = netlink_kernel_create(&init_net, NETLINK_MYTRACE, 0, nl_recv_msg, NULL, THIS_MODULE);
    if(!nl_sk){
        printk("Netlink initialization failed.\n");
        return -1;
    }
    return 0;
}

/* /proc interface */
static char proc_buf[MAX_SIZE];
static struct proc_dir_entry *proc_entry;
static int read_proc(char* buf, char **start,off_t offset, int count, int *eof, void *data)
{
    int len=0;
    len = sprintf(buf,"%s\n", proc_buf);
    return len;
}
static int write_proc(struct file *file, const char *buf, unsigned long count, void *data)
{
    if(count > MAX_SIZE)
        count = MAX_SIZE;
    if(copy_from_user(proc_buf, buf, count))
        return -EFAULT;
    return count;
}
/*
    jprobe function
*/

static void my_end_io_probe(struct bio *bio, int error)
{
    printk("HIIII, this called by probing!\n");
    jprobe_return();
}
static struct jprobe my_probe = {
    .entry = my_end_io_probe,
    .kp = {
        .symbol_name = "jprobe_end_io_t",
        .addr = NULL,
    },

};
static int init_jprobe(void)
{
    int ret;

    ret = register_jprobe(&my_probe);
    if(ret <0){
        printk("%s:%d jprobe fail\n", __FUNCTION__, __LINE__);
        return ret;
    }
    return 0;
}

static int init_proc(void)
{
    
    printk("%s\n", __FUNCTION__ );
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


static int init_main(void)
{
    printk("hello kernel!\n");
    init_proc();
    init_netlink();
    init_jprobe();
    
    return 0;
}

static void cleanup_main(void)
{
    remove_proc_entry("maio_proc", NULL);
    netlink_kernel_release(nl_sk);
     unregister_jprobe(&my_probe);
    printk("exit kernel\n");
}


module_init(init_main);
module_exit(cleanup_main);
MODULE_LICENSE("GPL");
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
