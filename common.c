#include "common.h"


int diff_4KB(int key, uint32_t sector, unsigned char* a, unsigned char* b)
{


    uint32_t *p1, *p2;
    uint32_t tmp;
    int start, length;
    char comment[500];
    char log[20];
    int i;
    int offset, log_length;
    p1 =(uint32_t*) a;
    p2 =(uint32_t*) b;


    memset(comment, '\0', sizeof(comment));
    offset = sprintf(comment, "%d&%u", key, sector);
    start = length = 0;
    for(i = 0 ; i < (PAGE_SIZE/4); i++) {
        tmp = (*p1 ^ *p2);
        if(tmp != 0) { // diff
            if( length == 0 ) {
                start = i;
                length = 1;
            } else {
                length++;
            }
        } else {
            if(length > 0)
            {
                memset(log, '\0', sizeof(log));
                sprintf(log, "&%d#%d&", start, length);
                log_length = strlen(log);
                if(log_length + offset >= sizeof(comment)) {
                    printk("asdfg&%s\n", comment);
                    memset(comment, '\0', sizeof(comment));
                    offset = sprintf(comment, "%d&%u", key, sector);
                }
                sprintf(comment + offset, "%s", log);
                offset += log_length;
            }
            length = 0;
        }
        p1++;
        p2++;
    }
    return 0;
}

int lzo_compress(char *data, int len, unsigned char* wrkmem, unsigned char* dst_buf)
{
    int out_len = 0;
    int ret = -1;
    if(!wrkmem || !dst_buf) {
        goto END;
    }
    memset(wrkmem, 0, LZO1X_MEM_COMPRESS);
    memset(dst_buf, 0, lzo1x_worst_compress(len));
    ret = lzo1x_1_compress(data, len, dst_buf, &out_len, wrkmem);
END:
    /* kfree is NULL-safe */
    if(ret == LZO_E_OK)
        return out_len;
    return ret;
}
