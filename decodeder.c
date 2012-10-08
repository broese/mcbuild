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
#include <openssl/aes.h>

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
}

void process_cmd(const char *msg, char *answer) {
    //if (msg[0] != '#') return 0;

    // tokenize
    char *words[256];
    int w=0;

    char wbuf[4096];
    strncpy(wbuf, msg, sizeof(wbuf));
    char *wsave;

    char *wstr = wbuf;
    do {
        words[w++] = strtok_r(wstr, " ", &wsave);
        wstr = NULL;
    } while(words[w-1]);

    w=0;
    while(words[w]) {
        printf("%3d : >%s<\n",w,words[w]);
        w++;
    }

    printf("-----------------------\n");
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

#if 0
    unsigned char md[SHA_DIGEST_LENGTH];
    //SHA_CTX sha; CLEAR(sha);

    SHA1("simon", strlen("simon"), md);
    //SHA1("jeb_", strlen("jeb_"), md);
    char buf[4096];
    print_hex(buf, md, SHA_DIGEST_LENGTH);
#endif

#if 0
    AES_KEY aes;
    AES_KEY aesd;
    CLEAR(aes);
    CLEAR(aesd);
    char ive[16];
    char ivd[16];

    //char *key = "fuckfuckfuckfuck";
    char *key = "\xc3\xc2\x3c\xea\x4a\xe9\x69\xc6\x0d\xda\x7b\x40\xde\xa6\x45\x93";

    AES_set_encrypt_key(key, 128, &aes);
    AES_set_decrypt_key(key, 128, &aesd);
    memcpy(ive, key, 16);
    memcpy(ivd, key, 16);

    //char *plaintext = "This is an example of a plaintext\n";
    char ok[2] = { 0xCD, 0x00 };

    char encoded[4096];
    CLEAR(encoded);
    char decoded[4096];
    CLEAR(decoded);
    int nume = 0;
    int numd = 0;
    
    printf("Key:\n");
    hexdump(key, 16);
    printf("IV(e):\n");
    hexdump(ive, 16);
    printf("IV(d):\n");
    hexdump(ivd, 16);
    AES_cfb8_encrypt(ok, encoded, 2, &aes, ive, &nume, AES_ENCRYPT);
    AES_cfb8_encrypt(encoded, decoded, 2, &aesd, ivd, &numd, AES_DECRYPT);
    
    hexdump(ok, 2);
    hexdump(encoded, 2);
    hexdump(decoded, 2);
#endif



#if 0
    hexdump(ive, 16);

    hexdump(ivd, 16);
    int numd = -1;
    AES_cfb8_encrypt(encoded, decoded, strlen(plaintext)+1,
                     &aes, ivd, &numd, AES_DECRYPT);
    hexdump(ivd, 16);

    printf("nume=%d numd=%d\n",nume,numd);
    hexdump(plaintext, strlen(plaintext)+10);
    hexdump(encoded, strlen(plaintext)+10);
    hexdump(decoded, strlen(plaintext)+10);
#endif

    process_cmd("build", NULL);
    process_cmd("build shit", NULL);
    process_cmd("build  shit", NULL);
    process_cmd("build additional pylons", NULL);
    process_cmd("build additional pylons ", NULL);
    process_cmd(" build additional pylons", NULL);
    process_cmd("build    additional    pylons", NULL);


    return 0;
}

