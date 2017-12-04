#include "common.h"


int diff_4KB(uint32_t sector, unsigned char* a, unsigned char* b)
{


    unsigned char *p1, *p2;
    uint32_t tmp;
    int start, length;

    int i;
    p1 = a;
    p2 = b;
    start = length = 0;
    for(i = 0 ; i < (PAGE_SIZE); i++) {
        tmp = (*p1 ^ *p2);
        if(tmp == 0) { // same
            if( length == 0 ) {
                start = i;
                length = 1;
            } else {
                length++;
            }
        } else {
            if(length > 1)
                debug_print(" %u&%d&%d\n", sector, start, length);
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
