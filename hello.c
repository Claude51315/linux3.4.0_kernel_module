#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/module.h>
#include<linux/netpoll.h>



MODULE_DESCRIPTION("My hello module");
MODULE_AUTHOR("Miao");
MODULE_LICENSE("MIT");


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

static int init_qq(void)
{
    printk("hello kernel!\n");
    init_netpoll();
    sendUdp("HELLO!1\n");
    sendUdp("HELLO!2\n");
    sendUdp("HELLO!3\n");
    return 0;
}

static void cleanup_qq(void)
{
    printk("exit kernel\n");
}
module_init(init_qq);
module_exit(cleanup_qq);
