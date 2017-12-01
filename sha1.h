/* sha1*/
#define SHA1CircularShift(bits,word) \
    (((word) << (bits)) | ((word) >> (32-(bits))))

#ifndef _SHA_enum_
#define _SHA_enum_
enum
{
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError       /* called Input after Result */
};
#endif
#define SHA1HashSize 20

typedef struct SHA1Context
{
    unsigned int  Intermediate_Hash[SHA1HashSize/4]; /* Message Digest  */

    unsigned int Length_Low;            /* Message length in bits      */
    unsigned int Length_High;           /* Message length in bits      */

    /* Index into message block array   */
    unsigned int Message_Block_Index;
    unsigned char Message_Block[64];      /* 512-bit message blocks      */

    int Computed;               /* Is the digest computed?         */
    int Corrupted;             /* Is the message digest corrupted? */
} SHA1Context;

void SHA1PadMessage(SHA1Context *);
void SHA1ProcessMessageBlock(SHA1Context *);

int SHA1Reset(SHA1Context *context);
int SHA1Result( SHA1Context *context,
        unsigned char Message_Digest[SHA1HashSize]);
int SHA1Input(    SHA1Context    *context,
        const unsigned char  *message_array,
        unsigned       length);
void compute_sha(unsigned char* input, int size, unsigned char* output);
