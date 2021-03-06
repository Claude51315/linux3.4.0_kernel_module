#include "common.h"
#include "sha1.h"
#include "crc.h"
#include "adler.h"
#include "lruc.h"
MODULE_DESCRIPTION("My hello module");
MODULE_AUTHOR("claude51315@gmail.com");

static struct timespec start_time;

static lruc *lru_cache;
typedef struct {
    unsigned char buffer[PAGE_SIZE];
}page_buf;

static page_buf *cache_memory;

static unsigned char* lzo_wrkmem;
static unsigned char* lzo_dstbuf;

#define CACHE_SIZE 512
static int get_page_data(struct page *p, char *output)
{
    void *vpp = NULL;
    if(!p)
        return -1;
    vpp = kmap(p);
    if(!vpp)
        return -1;
    memcpy(output, (char *)vpp, PAGE_SIZE);
    kunmap(p);
    vpp = NULL;
    return 0;
}

static void get_filename(struct bio *print_bio, char *output)
{
    int i, j, k; 
    struct bio_vec *biovec;
    void *vpp;
    struct page *bio_page;    
    struct dentry *p;
    unsigned char unknown[] = "unknown", null[]="NULL";
    unsigned char *pname;
    biovec = print_bio->bi_io_vec;
    bio_page = biovec->bv_page;
    pname = null;
    vpp = NULL;
    i = j = k = 0;
    /* get filename */
    if(bio_page && 
            bio_page->mapping && 
            ((unsigned long) bio_page->mapping & PAGE_MAPPING_ANON) == 0  && 
            bio_page->mapping->host )
    {
        p = NULL ; 
        if( !list_empty(&(bio_page->mapping->host->i_dentry)))
            p = list_first_entry(&(bio_page->mapping->host->i_dentry), struct dentry, d_alias);
        if(p != NULL ) {
            pname = p->d_iname;
            for(j = 0 ; j < strlen(p->d_iname); j++){
                if( (p->d_iname[j]!= '\0')  &&  ( (p->d_iname[j] < 32) || (p->d_iname[j] > 126))){ 
                    pname = unknown;
                    break;
                }
            } 
        }
    }
    if(pname != unknown &&  pname != null)
    {
        memcpy(output, pname, DNAME_INLINE_LEN);
    }
    if(pname == unknown)
        memcpy(output, pname, 7 + 1);
    if(pname == null)
        memcpy(output, pname, 7 + 1);

    return;    
}

#define NETLINK_MYTRACE 30
/* netlink interface */
#define NETLINK_USER 31

static struct sock *nl_sk = NULL;
unsigned int user_pid; 
static int trace_flag;
static void nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid, res;
    pid  = res = 0;
    nlh = (struct nlmsghdr*)skb->data;
    user_pid = nlh->nlmsg_pid;
    printk("%s : msg = %s\n", __FUNCTION__, (char *)nlmsg_data(nlh));
    if(strcmp((char*) nlmsg_data(nlh), "start") == 0) {
        trace_flag = 1;
        start_time = current_kernel_time();
    } else {
        trace_flag = 0;
    }
}
static void nl_send_msg(char *msg, int msg_len,int dstPID)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    if(!msg || !nl_sk)
        return;
    skb = alloc_skb(NLMSG_SPACE(msg_len), GFP_KERNEL);
    if(!skb) {
        debug_print("%s\n", "allocate skb fail"); 
    }
    nlh = nlmsg_put(skb, 0, 0, 0, msg_len,0);
    NETLINK_CB(skb).pid = NETLINK_CB(skb).dst_group = 0;
    memcpy(NLMSG_DATA(nlh), msg, msg_len);
    netlink_unicast(nl_sk, skb, dstPID, 1);
    return ;
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
static void check_cache(int key, char* data, uint32_t sector) 
{
    
    uint32_t hash_index;
    hash_index = lruc_hash(lru_cache, sector, sizeof(sector));
    //if(lruc_find(lru_cache, sector, sizeof(sector)))
    //{
        diff_4KB(key, sector, data, (unsigned char*)(cache_memory + hash_index));
        memcpy((char *)(cache_memory + hash_index), data, PAGE_SIZE);
    //}
    //lruc_set(lru_cache, sector, sizeof(sector), (void*)(cache_memory + hash_index), PAGE_SIZE);
}

static unsigned char* data_buf;
static void my_end_io_probe(int rw, struct bio *bio)
{

    // variable for printing
    struct timespec timestamp;
    char filename[DNAME_INLINE_LEN];
    unsigned char comment[200];
    char devname[20];
    unsigned char sha1[SHA1HashSize], sha1_hex[SHA1HashSize*2 + 1];
    unsigned long crc32, adler;
    int lzo;
    static int key = 0;
    // variable for lru cache
    uint32_t sector;
    // common usage
    int i, index, ret;
    struct page* bio_page;
    if(!bio_has_data(bio) || (rw &( REQ_DISCARD | REQ_SANITIZE))) {
        jprobe_return();
        return;
    }


    memset(devname, '\0', sizeof(devname));
    bdevname(bio->bi_bdev, devname);    
    /*
       if(strcmp(devname, "mmcblk0p23") != 0){
       jprobe_return();
       return;
       } 
     */
    /*
    if(!trace_flag){
        jprobe_return();
        return;
    }
    */
    timestamp = current_kernel_time();
    timestamp.tv_sec -= start_time.tv_sec;
    timestamp.tv_nsec -= start_time.tv_nsec;
    if(timestamp.tv_nsec < 0){
        timestamp.tv_sec --;
        timestamp.tv_nsec += 1000000000L;
    }
    bio_page = bio->bi_io_vec->bv_page; 
    memset(filename, '\0', sizeof(filename));
    memset(data_buf, 0, PAGE_SIZE);
    memset(sha1, '\0', sizeof(sha1));
    memset(sha1_hex, '\0', sizeof(sha1_hex));
    memset(comment, '&', sizeof(comment));
    get_filename(bio, filename);
    if(strncmp(filename, "trace", 5) == 0) {
        jprobe_return();
        return;
    }
    
    index= 0;
    sector = 0;
    for(index = 0 ; index < bio -> bi_vcnt ; index ++) {
        lzo = -1;
        sector = bio->bi_sector + index * 8;
        bio_page = (&bio->bi_io_vec[index])->bv_page;
        ret = get_page_data(bio_page, data_buf);
        if(ret < 0 ) {
            debug_print("%s\n", "get page data fail" );
            continue;
        }
        check_cache(key, data_buf, sector);
        compute_sha(data_buf, PAGE_SIZE, sha1);
        crc32 = crc32_hash(data_buf, PAGE_SIZE, 1); 
        adler = adler_hash(data_buf, PAGE_SIZE);
        lzo = lzo_compress(data_buf, PAGE_SIZE, lzo_wrkmem, lzo_dstbuf);
        for( i = 0 ; i < SHA1HashSize; i++){
            sprintf(sha1_hex + i*2 , "%02x", sha1[i]);
        }
        sha1_hex[SHA1HashSize *2] = '\0';
        //sprintf(comment, "%5ld.%-10ld&%s&%s&%llu&%s\n",timestamp.tv_sec, timestamp.tv_nsec, devname, filename, bio->bi_sector,sha1_hex);
        //sprintf(comment, "%5ld.%-10ld&%s&%s&%d&%c&%llu&%d&%s&%lu&%lu&%d\n",timestamp.tv_sec, timestamp.tv_nsec, devname, filename, bio->bi_vcnt,(rw & WRITE) ? 'W' : 'R',   bio->bi_sector, index,sha1_hex, crc32, adler, lzo);
        sprintf(comment, "qwert&%5ld.%-10ld&%d&%s&%s&%d&%c&%llu&%d&%s&%lu&%lu&%d\n", timestamp.tv_sec, timestamp.tv_nsec, key, devname, filename, bio->bi_vcnt,(rw & WRITE) ? 'W' : 'R',   bio->bi_sector, index,sha1_hex, crc32, adler, lzo);
        printk("%s", (comment));
        if(user_pid != 99999){
            //nl_send_msg(comment, strlen(comment), user_pid);
            //nl_send_msg(data_buf, PAGE_SIZE, user_pid);
        }
        key++;
    }
    jprobe_return();
}
static struct jprobe my_probe = {
    .entry = my_end_io_probe,
    .kp = {
        .symbol_name = "submit_bio",
        .addr = NULL,
    },

};
static int init_jprobe(void)
{
    int ret;
    data_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if(!data_buf){
        printk("%s:%d malloc jprobe memory fail\n", __FUNCTION__, __LINE__);
    }
    ret = register_jprobe(&my_probe);
    if(ret <0){
        printk("%s:%d jprobe fail\n", __FUNCTION__, __LINE__);
        return ret;
    }
    printk("init jprobe success\n");
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
    trace_flag = 0;
    //init_proc();
    init_netlink();
    init_jprobe();
    lru_cache = lruc_new(CACHE_SIZE); 
    cache_memory = kmalloc(CACHE_SIZE * sizeof(page_buf), GFP_KERNEL);
    if(!cache_memory)
        debug_print("%s\n", "not enough memory for cache");
    lzo_wrkmem = kmalloc(LZO1X_MEM_COMPRESS, GFP_KERNEL);
    lzo_dstbuf = kmalloc(lzo1x_worst_compress(PAGE_SIZE), GFP_KERNEL);
    
   start_time = current_kernel_time();
    user_pid = 99999;
    printk("init done!\n");
    return 0;
}

static void cleanup_main(void)
{
    //remove_proc_entry("maio_proc", NULL);
    unregister_jprobe(&my_probe);
    netlink_kernel_release(nl_sk);
    lruc_free(lru_cache);
    kfree(data_buf);
    kfree(cache_memory);
    kfree(lzo_wrkmem);
    kfree(lzo_dstbuf);
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


