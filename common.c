#include "common.h"


int diff_4KB(uint32_t sector, unsigned char* a, unsigned char* b)
{


    unsigned char *p1, *p2;
    uint32_t tmp;
    int start, length;
    
    int i;
    p1 = a;
    p2 = b;
    start = length = -1;
    for(i = 0 ; i < (PAGE_SIZE); i++) {
        tmp = (*p1 ^ *p2);
        if(tmp == 0) { // same
            if(start == -1  || length == 0 ) {
                start = i;
                length= 1;
        
        } else {
                length++;
            }
        } else {
            if(length > 0)
                debug_print(" sector = %u, start = %d, len = %d\n", sector, start, length);
            start  = i;
            length = 0;
        }
        p1++;
        p2++;
    }
    return 0;
}
