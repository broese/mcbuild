#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//#include "crypto.h"

#include "lh_buffers.h"
#include "lh_files.h"

#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>

static void callback(int p, int n, void *arg)
{
	char c='B';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	fputc(c,stderr);
}

void print_hex(char *buf, const char *data, ssize_t len) {
    int i;
    char *w = buf;
    int f = 0;
    if (data[0] < 0) {
        *w++ = '-';
        f = 1;
    }
    for(i=0; i<len; i++) {
        char d = data[i];
        if (f) {
            d = -d;
            if (i<len-1)
                d--;
        }
        
        sprintf(w,"%02x",(unsigned char)d);
        w+=2;
    }
    *w++ = 0;

    w = buf+f;
    char *z = w;
    while(*z == '0') z++;
    while(*z) *w++ = *z++;
    *w++ = 0;

    printf("%s\n",buf);
}

int main(int ac, char **av) {

#if 0
    char buf[4096];
    char *p = buf;

    encode_len(&p,buf+sizeof(buf),1);
    encode_len(&p,buf+sizeof(buf),10);
    encode_len(&p,buf+sizeof(buf),80);
    encode_len(&p,buf+sizeof(buf),126);
    encode_len(&p,buf+sizeof(buf),127);
    encode_len(&p,buf+sizeof(buf),128);
    encode_len(&p,buf+sizeof(buf),190);
    encode_len(&p,buf+sizeof(buf),1000);
    encode_len(&p,buf+sizeof(buf),70000);

    hexdump(buf,p-buf);

    p = buf;
    int len = sizeof(buf);
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
    printf("%d\n",decode_len(&p,buf+len));
#endif

#if 0
    ssize_t size;
    uint8_t *der = read_file("public_key",&size);
    const unsigned char *p = der;

    RSA *rsa_server = NULL;
    d2i_RSA_PUBKEY(&rsa_server, (const unsigned char **)&p, (long)size);
    RSA_print_fp(stdout, rsa_server, 4);

    RSA *rsa_client = RSA_generate_key(1024, RSA_F4, NULL, NULL);
    RSA_print_fp(stdout, rsa_client, 4);

    char skey_server[16];
    RAND_pseudo_bytes(skey_server, 16);

    char resp_srv[4096];
    int rsa_size = RSA_public_encrypt(sizeof(skey_server), skey_server, resp_srv, rsa_server, RSA_PKCS1_PADDING);
    printf("RSA_public_encrypt returned %d, RSA_size(rsa)=%d\n",rsa_size,RSA_size(rsa_server));
    hexdump(resp_srv, rsa_size);

    unsigned char c_pkey[1024];
    unsigned char *pp = c_pkey;
    int c_pklen = i2d_RSA_PUBKEY(rsa_client, &pp);
    printf("%08p %08p %d\n",c_pkey,pp,c_pklen);
    hexdump(c_pkey,c_pklen);

#endif

    unsigned char md[SHA_DIGEST_LENGTH];
    //SHA_CTX sha; CLEAR(sha);

    SHA1("simon", strlen("simon"), md);
    //SHA1("jeb_", strlen("jeb_"), md);
    char buf[4096];
    print_hex(buf, md, SHA_DIGEST_LENGTH);

    return 0;
}

