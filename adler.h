#ifndef ADLER_H
#define ADLER_H
/* adler 32*/
#define BASE 65521U
#define NMAX 5552
#define MOD28(a) a %= BASE
#define MOD(a) a %= BASE
#define DO1(buf,i)  {adler += (buf)[i]; sum2 += adler;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

unsigned long adler32_z(unsigned long adler,char* buf,int  len);
unsigned long adler_hash(char *in_buf, int in_len);

#endif
