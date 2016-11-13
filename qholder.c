/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_debug.h"
#include "lh_buffers.h"
#include "lh_bytes.h"
#include "lh_files.h"
#include "lh_net.h"
#include "lh_event.h"
#include "lh_compress.h"
#include "lh_arr.h"

#include "mcp_packet.h"

// forward declaration
int query_auth_server();

#define DEFAULT_REMOTE_ADDR "2b2t.org"
#define DEFAULT_REMOTE_PORT 25565

char     *o_server      = DEFAULT_REMOTE_ADDR;
uint16_t  o_port        = DEFAULT_REMOTE_PORT;
char     *o_user        = "Dorquemada_";
int       o_interval    = 20;
int       o_protover    = 210;
uint32_t  o_server_ip   = 0xffffffff;

#define RES_QUEUE       0
#define RES_NOCONNECT   1
#define RES_AUTHFAIL    2
#define RES_TXERROR     3
#define RES_RXERROR     4
#define RES_OTHERERROR  5
#define RES_LOGIN       6

int queue_pos = -1;
int queue_size = -1;

uint8_t skey[16];
uint8_t enc_iv[16];
uint8_t dec_iv[16];
AES_KEY aes;


int write_packet(uint8_t *data, ssize_t len, int s) {
    uint8_t buf[65536];
    uint8_t *w = buf;

    write_varint(w, len);
    memmove(w, data, len);
    w+=len;

    ssize_t nbytes = w-buf;
    ssize_t wb = write(s, buf, nbytes);
    if (wb != nbytes) return RES_TXERROR;

    return 0;
}

int read_packet(uint8_t *rbuf, uint32_t *plen, int s) {
    uint8_t buf[65536];
    uint8_t *p = buf;

    ssize_t rb = read(s, buf, 2);
    if (rb != 2) return RES_TXERROR;

    *plen = lh_read_varint(p);
    ssize_t ll = p-buf;
    rb = read(s, buf+2, *plen+ll-2);
    if (rb != *plen+ll-2) return RES_TXERROR;

    memmove(rbuf, buf+ll, *plen);

    return 0;
}

int read_enc_packet(uint8_t *rbuf, uint32_t *plen, int s) {
    uint8_t buf[65536];

    ssize_t rb = read(s, buf, 2);
    if (rb != 2) return RES_TXERROR;

    uint8_t ubuf[65536];
    int num = 0;
    AES_cfb8_encrypt(buf, ubuf, 2, &aes, dec_iv, &num, AES_DECRYPT);

    uint8_t *p = ubuf;
    *plen = lh_read_varint(p);
    ssize_t ll = p-ubuf;
    rb = read(s, buf+2, *plen+ll-2);
    if (rb != *plen+ll-2) return RES_TXERROR;

    num = 0;
    AES_cfb8_encrypt(buf+2, ubuf+2, *plen+ll-2, &aes, dec_iv, &num, AES_DECRYPT);

    memmove(rbuf, ubuf+ll, *plen);

    return 0;
}

// void AES_cfb8_encrypt(*in, *out, length, *key, *ivec, *num, enc);

////////////////////////////////////////////////////////////////////////////////

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


#define PROFILE_PATH ".minecraft/launcher_profiles.json"

#define JSON_PARSE(json, key, var, type)                                       \
    if (!json_object_object_get_ex(json, key, &var) ||                         \
         json_object_get_type(var) != type) {                                  \
             printf("Failed to parse key %s\n", key);                          \
             return 0;                                                         \
    }

int parse_profile(char *accessToken, char *userId, char *userName) {
    const char * homedir = getenv("APPDATA");
    if (!homedir) homedir = getenv("HOME");
    if (!homedir) {
        printf("Failed to locate Minecraft profile directory - check your environment\n");
        return 0;
    }

    char path[PATH_MAX];
    sprintf(path, "%s/%s",homedir,PROFILE_PATH);

    uint8_t * buf;
    ssize_t sz = lh_load_alloc(path, &buf);
    if (sz<=0) {
        printf("Failed to open launcher profile.\n"
               "Make sure you have started Minecraft and logged in at least once\n");
        return 0;
    }

    lh_resize(buf, sz+1);
    buf[sz] = 0;

    json_object *json = json_tokener_parse((char *)buf);
    json_object *su, *adb, *prof, *dn, *at;

    JSON_PARSE(json, "selectedUser", su, json_type_string);
    sprintf(userId, "%s", json_object_get_string(su));

    JSON_PARSE(json, "authenticationDatabase", adb, json_type_object);
    JSON_PARSE(adb, userId, prof, json_type_object);

    JSON_PARSE(prof, "displayName", dn, json_type_string);
    sprintf(userName, "%s", json_object_get_string(dn));

    JSON_PARSE(prof, "accessToken", at, json_type_string);
    sprintf(accessToken, "%s", json_object_get_string(at));

    return 1;
}

// this is the server-side handling of the session authentication
// we use Curl to send an HTTPS request to Mojangs session server
int query_auth_server(const char *id, uint8_t *pkey, uint32_t pklen) {
    // the final touch - send the authentication token to the session server
    unsigned char md[SHA_DIGEST_LENGTH];
    SHA_CTX sha; CLEAR(sha);

    SHA1_Init(&sha);
    SHA1_Update(&sha, id, strlen(id));
    SHA1_Update(&sha, skey, sizeof(skey));
    SHA1_Update(&sha, pkey, pklen);
    SHA1_Final(md, &sha);

    char auth[4096];
    //hexdump(md, SHA_DIGEST_LENGTH);
    print_hex(auth, (char *)md, SHA_DIGEST_LENGTH);
    //printf("sessionId : %s\n", auth);

    char accessToken[256], userId[256], userName[256];
    if (!parse_profile(accessToken, userId, userName)) {
        printf("Failed to parse user profile\n");
        return 0;
    }

#if 0
    printf("selectedProfile: >%s<\n",userId);
    printf("userName:        >%s<\n",userName);
    printf("accessToken:     >%s<\n",accessToken);
    printf("serverId:        >%s<\n",auth);
#endif

    char buf[4096];
    sprintf(buf,"{\"accessToken\":\"%s\",\"selectedProfile\":\"%s\",\"serverId\":\"%s\"}",
            accessToken, userId, auth);

    //printf("request to session server: %s\n",buf);

    // perform a request with a cURL client

    // init curl
    CURL *curl = curl_easy_init();
    CURLcode res;

    // set header options
    curl_easy_setopt(curl, CURLOPT_URL, "https://sessionserver.mojang.com/session/minecraft/join");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Java/1.6.0_27");

    struct curl_slist *headerlist=NULL;
    headerlist = curl_slist_append(headerlist, "Content-Type: application/json; charset=utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);

    // set body - our JSON blob
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);

    // make a request
    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    curl_slist_free_all(headerlist);
    curl_easy_cleanup(curl);

    return 1;
}



////////////////////////////////////////////////////////////////////////////////

// Send Login/Handshake to the server
int send_handshake(int s) {
    uint8_t buf[4096];
    uint8_t *w = buf;
    CI_Handshake_pkt pkt;
    pkt.protocolVer = o_protover;
    sprintf(pkt.serverAddr, "%s", o_server);
    pkt.serverPort = o_port;
    pkt.nextState = 2; // login

    write_varint(w, PID(CI_Handshake));
    w = encode_handshake(&pkt, w);

    return write_packet(buf, w-buf, s);
}

int send_loginstart(int s) {
    uint8_t buf[4096];
    uint8_t *w = buf;
    CL_LoginStart_pkt pkt;
    sprintf(pkt.username, "%s", o_user);

    write_varint(w, PID(CL_LoginStart));
    w = encode_loginstart(&pkt, w);

    return write_packet(buf, w-buf, s);
}

int receive_encryption_req(int s) {
    uint8_t buf[4096];
    uint32_t len;

    int res = read_packet(buf, &len, s);
    SL_EncryptionRequest_pkt erq;
    uint8_t *p = buf;
    uint32_t pid = read_varint(p);
    decode_encryption_request(&erq, p);

    // decode server PUBKEY to an RSA struct
    unsigned char *pp = erq.pkey;
    RSA *s_rsa = NULL;
    d2i_RSA_PUBKEY(&s_rsa, (const unsigned char **)&pp, erq.klen);
    if (s_rsa == NULL) {
        printf("Failed to decode the server's public key\n");
        return 1;
    }
    //RSA_print_fp(stdout, s_rsa, 4);

    // generate our random key
    RAND_pseudo_bytes(skey, 16);

    // generate encryption response
    uint8_t *w = buf;
    write_varint(w, PID(CL_EncryptionResponse));

    uint8_t e_skey[1024];
    int eklen = RSA_public_encrypt(sizeof(skey), skey, e_skey, s_rsa, RSA_PKCS1_PADDING);
    write_varint(w,eklen);
    memcpy(w, e_skey, eklen);
    w += eklen;

    uint8_t e_token[1024];
    int etlen = RSA_public_encrypt(erq.tlen, erq.token, e_token, s_rsa, RSA_PKCS1_PADDING);
    write_varint(w,etlen);
    memcpy(w, e_token, etlen);
    w += etlen;

    AES_set_encrypt_key(skey, 128, &aes);
    memmove(enc_iv, skey, 16);
    memmove(dec_iv, skey, 16);

    query_auth_server(erq.serverID, erq.pkey, erq.klen);

    return write_packet(buf, w-buf, s);
}

int receive_disconnect(int s) {
    uint8_t buf[4096];
    uint32_t len;

    int res = read_enc_packet(buf, &len, s);

    uint8_t *p = buf;
    uint32_t pid = read_varint(p);
    if (pid == PID(SL_LoginSuccess)) return RES_LOGIN;
    if (pid != PID(SL_Disconnect)) return RES_RXERROR;

    int i,j=0;
    char str[64];
    lh_clear_obj(str);
    for(i=0; i<len-3; i++) {
        if (buf[i] == 0xc2 && buf[i+1] == 0xa7 && buf[i+2] == 0x6c) {
            i+=3;
            while(i<len-3 && buf[i]!=0xc2)
                str[j++] = buf[i++];
            str[j] = 0;
            if (sscanf(str, "%u", &queue_pos) != 1) return RES_OTHERERROR;
            break;
        }
    }

    j = 0;
    for(; i<len-3; i++) {
        if (buf[i] == 0xc2 && buf[i+1] == 0xa7 && buf[i+2] == 0x6c) {
            i+=3;
            while(i<len-3 && buf[i]!=0xc2)
                str[j++] = buf[i++];
            str[j] = 0;
            if (sscanf(str, "%u", &queue_size) != 1) return RES_OTHERERROR;
            break;
        }
    }

    if (i>=len-3) return RES_OTHERERROR;

    return 0;
}

int try_connection() {
    int s = lh_connect_tcp4(o_server_ip, o_port);
    if (s<0) return RES_NOCONNECT;

    if (fcntl(s, F_SETFL, 0) < 0) {
        close(s);
        LH_ERROR(-1,"Failed to reset O_NONBLOCK on client socket");
    }

    if (send_handshake(s)) { close(s); return RES_TXERROR; }
    if (send_loginstart(s)) { close(s); return RES_TXERROR; }
    if (receive_encryption_req(s)) { close(s); return RES_RXERROR; }
    if (receive_disconnect(s)) { close(s); return RES_RXERROR; }

    close(s);

    return RES_QUEUE;
}

int main(int ac, char **av) {
    if (av[1]) o_server = av[1];


    o_server_ip = lh_dns_addr_ipv4(o_server);
    if (o_server_ip == 0xffffffff)
        LH_ERROR(1, "Failed to resolve remote server address %s",o_server);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    int attempt = 0;
    while (1) {
        attempt++;
        time_t t;
        time(&t);
        char ts[256];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S",localtime(&t));
        printf("Attempt %4d , %s :  ", attempt, ts);

        switch (try_connection()) {
            case RES_QUEUE:
                printf("queued %d / %d\n", queue_pos, queue_size);
                sleep(o_interval);
                break;
            case RES_NOCONNECT:
                printf("connection failed: %s\n", strerror(errno));
                sleep(2);
                break;
            case RES_TXERROR:
                printf("failed to send data: %s\n", strerror(errno));
                sleep(2);
                break;
            case RES_RXERROR:
                printf("failed to send data: %s\n", strerror(errno));
                sleep(2);
                break;
        }
    }
}
