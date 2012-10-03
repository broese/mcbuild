#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/aes.h>

#include "lh_buffers.h"
#include "lh_files.h"
#include "lh_net.h"
#include "lh_event.h"

#define SERVER_IP 0x0a000001
//#define SERVER_IP 0xc7a866c2 // Beciliacraft
#define SERVER_PORT 25565

////////////////////////////////////////////////////////////////////////////////

int handle_server(int sfd, uint32_t ip, uint16_t port, int *cs, int *ms) {

    //// Handle Client-Side Connection

    // accept connection from the client side
    struct sockaddr_in cadr;
    int size = sizeof(cadr);
    *cs = accept(sfd, (struct sockaddr *)&cadr, &size);
    if (*cs < 0) LH_ERROR(0, "Failed to accept the client-side connection",strerror(errno));
    printf("Accepted from %s:%d\n",inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));

    // set non-blocking mode on the client socket
    if (fcntl(*cs, F_SETFL, O_NONBLOCK) < 0) {
        close(*cs);
        LH_ERROR(0, "Failed to set non-blocking mode for the client",strerror(errno));
    }

    //// Handle Server-Side Connection

    // open connection to the remote server
    *ms = sock_client_ipv4_tcp(ip, port);
    if (*ms < 0) {
        close(*cs);
        LH_ERROR(0, "Failed to open the client-side connection",strerror(errno));
    }

    // set non-blocking mode
    if (fcntl(*ms, F_SETFL, O_NONBLOCK) < 0) {
        close(*cs);
        close(*ms);
        LH_ERROR(0, "Failed to set non-blocking mode for the server",strerror(errno));
    }

    return 1;
}



////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

#define MCP_KeepAlive           0x00
#define MCP_LoginRequest        0x01
#define MCP_Handshake           0x02
#define MCP_ChatMessage         0x03
#define MCP_TimeUpdate          0x04
#define MCP_EntityEquipment     0x05
#define MCP_SpawnPosition       0x06
#define MCP_UseEntity           0x07
#define MCP_UpdateHealth        0x08
#define MCP_Respawn             0x09
#define MCP_Player              0x0a
#define MCP_PlayerPosition      0x0b
#define MCP_PlayerLook          0x0c
#define MCP_PlayerPositionLook  0x0d
#define MCP_PlayerDigging       0x0e
#define MCP_PlayerBlockPlace    0x0f
#define MCP_HeldItemChange      0x10
#define MCP_UseBed              0x11
#define MCP_Animation           0x12
#define MCP_EntityAction        0x13
#define MCP_SpawnNamedEntity    0x14
#define MCP_SpawnDroppedItem    0x15
#define MCP_CollectItem         0x16
#define MCP_SpawnObject         0x17
#define MCP_SpawnMob            0x18
#define MCP_SpawnPainting       0x19
#define MCP_SpawnExperienceOrb  0x1a
#define MCP_EntityVelocity      0x1c
#define MCP_DestroyEntity       0x1d
#define MCP_Entity              0x1e
#define MCP_EntityRelativeMove  0x1f
#define MCP_EntityLook          0x20
#define MCP_EntityLookRelmove   0x21
#define MCP_EntityTeleport      0x22
#define MCP_EntityHeadLook      0x23
#define MCP_EntityStatus        0x26
#define MCP_AttachEntity        0x27
#define MCP_EntityMetadata      0x28
#define MCP_EntityEffect        0x29
#define MCP_RemoveEntityEffect  0x2a
#define MCP_SetExperience       0x2b
#define MCP_ChunkAllocation     0x32
#define MCP_ChunkData           0x33
#define MCP_MultiBlockChange    0x34
#define MCP_BlockChange         0x35
#define MCP_BlockAction         0x36
#define MCP_BlockBreakAnimation 0x37
#define MCP_MapChunkBulk        0x38
#define MCP_Explosion           0x3c
#define MCP_SoundParticleEffect 0x3d
#define MCP_NamedSoundEffect    0x3e
#define MCP_ChangeGameState     0x46
#define MCP_Thunderbolt         0x47
#define MCP_OpenWindow          0x64
#define MCP_CloseWindow         0x65
#define MCP_ClickWindow         0x66
#define MCP_SetSlot             0x67
#define MCP_SetWindowItems      0x68
#define MCP_UpdateWindowProp    0x69
#define MCP_ConfirmTransaction  0x6a
#define MCP_CreativeInventory   0x6b
#define MCP_EnchanItem          0x6c
#define MCP_UpdateSign          0x82
#define MCP_ItemData            0x83
#define MCP_UpdateTileEntity    0x84
#define MCP_IncrementStatistic  0xc8
#define MCP_PlayerListItem      0xc9
#define MCP_PlayerAbilities     0xca
#define MCP_TabComplete         0xcb
#define MCP_LocaleAndViewdist   0xcc
#define MCP_ClientStatuses      0xcd
#define MCP_PluginMessage       0xfa
#define MCP_EncryptionKeyResp   0xfc
#define MCP_EncryptionKeyReq    0xfd
#define MCP_ServerListPing      0xfe
#define MCP_Disconnect          0xff

static inline int read_string(char **p, char *s, char *limit) {
    if (limit-*p < 2) return 1;
    uint16_t slen = read_short(*p);
    if (limit-*p < slen*2) return 1;

    int i;
    for(i=0; i<slen; i++) {
        (*p)++;
        s[i] = read_char(*p);
    }
    s[i] = 0;
    return 0;
}


#define Rlim(l) if (limit-p < l) { atlimit=1; break; }

#define Rx(n,type,fun) type n; Rlim(sizeof(type)) else { n = fun(p); }

#define Rchar(n)  Rx(n,char,read_char)
#define Rshort(n) Rx(n,short,read_short)
#define Rint(n)   Rx(n,int,read_int)
#define Rlong(n)  Rx(n,long long,read_long);
#define Rfloat(n) Rx(n,float,read_float)
#define Rdouble(n) Rx(n,double,read_double)
#define Rstr(n)   char n[4096]; if (read_string((char **)&p,n,(char *)limit)) { atlimit=1; break; }
#define Rskip(n)  Rlim(n) else { p+=n; }

typedef struct {
    short id;
    char  count;
    short damage;
    char  meta[256];
} slot_t;

#define Rslot(n)                                            \
    slot_t n = {0,0,0};                                     \
    Rshort(id); n.id = id;                                  \
    if (id != -1) {                                         \
        Rchar(count); n.count = count;                      \
        Rshort(damage); n.damage = damage;                  \
        if (istool(id)) {                                   \
            Rshort(metalen);                                \
            if (metalen != -1)                              \
                Rskip(metalen);                             \
        }                                                   \
    }
//TODO: extract metadata into buffer

#define Rmobmeta(n)                                                     \
    while (1) {                                                         \
        Rchar(type);                                                    \
        if (type == 0x7f) break;                                        \
        type = (type>>5)&0x07;                                          \
        switch (type) {                                                 \
            case 0: Rskip(1); break;                                    \
            case 1: Rskip(2); break;                                    \
            case 2: Rskip(4); break;                                    \
            case 3: Rskip(4); break;                                    \
            case 4: { Rstr(s); break; }                                 \
            case 5: { Rslot(s); break; }                                \
            case 6: Rskip(12); break;                                   \
            default: printf("Unsupported type %d\n",type); exit(1);     \
        }                                                               \
    }

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    uint8_t * crbuf; ssize_t crlen;
    uint8_t * cwbuf; ssize_t cwlen;
    uint8_t * srbuf; ssize_t srlen;
    uint8_t * swbuf; ssize_t swlen;
} flbuffers;

#define WRITEBUF_GRAN 65536

ssize_t inline Wstr(uint8_t *p, const char *str) {
    int slen = strlen(str);
    *p++ = slen>>8;
    *p++ = slen&0xff;

    int i;
    for(i=0; i<slen; i++) {
        *p++ = 0;
        *p++ = str[i];
    }

    return (slen+1)*2;
}

////////////////////////////////////////////////////////////////////////////////

struct {
    // RSA structures/keys for server-side and client-side
    RSA *s_rsa; // public key only - must be freed by RSA_free
    RSA *c_rsa; // public+private key - must be freed by RSA_free

    // verification tokens
    char s_token[4]; // what we received from the server
    char c_token[4]; // generated by us, sent to client

    // AES encryption keys (128 bit)
    char s_skey[16]; // generated by us, sent to the server
    char c_skey[16]; // received from the client

    // server ID - we forward this to client as is, so no need
    // for client- and server side versions.
    // zero-temrinated string, so no need for length value
    // Note: serverID is received as UTF-16 string, but converted to ASCII
    // for hashing. The string itself is a hexstring, but it's not converted
    // to bytes or anything
    char s_id[256];

    // session ID received from the client via HTTP
    // zero-terminated string
    char c_session[256]; 

    // server ID hash received from the client, for verification only
    char server_id_hash[256];

    // DER-encoded public key from the server
    char s_pkey[1024];
    int s_pklen;

    // DER-encoded public key sent to the client
    char c_pkey[1024];
    int c_pklen;

    int encstate;
    int passfirst;

    AES_KEY c_aes;
    char c_enc_iv[16];
    char c_dec_iv[16];

    AES_KEY s_aes;
    char s_enc_iv[16];
    char s_dec_iv[16];
} mitm;

void init_mitm() {
    if (mitm.s_rsa) RSA_free(mitm.s_rsa);
    if (mitm.c_rsa) RSA_free(mitm.c_rsa);
    CLEAR(mitm);
}

int handle_session_server(int sfd) {

    //// Handle Client-Side Connection

    // accept connection from the client side
    struct sockaddr_in cadr;
    int size = sizeof(cadr);
    int cs = accept(sfd, (struct sockaddr *)&cadr, &size);
    if (cs < 0) LH_ERROR(0, "Failed to accept the client-side connection",strerror(errno));
    printf("Accepted from %s:%d (Webserver)\n",inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));

    FILE * client = fdopen(cs, "r+");

    char buf[4096];
    while(fgets(buf, sizeof(buf), client)) {
        printf("%s\n",buf);
        if (!strncmp(buf, "GET /game/joinserver.jsp?user=", 30)) {
            char *ssid = index(buf, '&')+11; // skip to sessionId=
            char *wp = mitm.c_session;
            while(*ssid != '&') *wp++ = *ssid++;
            *wp++ = 0;

            ssid += 10; // skip to serverId=
            wp = mitm.server_id_hash;
            while(*ssid != ' ') *wp++ = *ssid++;
            *wp++ = 0;

            printf("session login caught : sessionId=%s serverId=%s\n",
                   mitm.c_session,mitm.server_id_hash);
        }

        //hexdump(buf, strlen(buf));
        if (buf[0] == 0x0d && buf[1] == 0x0a) {
            printf("------ HEADER END ------\n");
            fprintf(client,
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain; charset=UTF-8\r\n"
                    "Server: Redstone\r\n"
                    "Content-Length: 2\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "OK\r\n");
            fflush(client);
            fclose(client);
            break;
        }
    }
    return 1;
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

#define SESSION_SERVER_IP 0xb849df28

int connect_session(const char *user, const char *sessionId, const char *serverId) {
    int ss = sock_client_ipv4_tcp(SESSION_SERVER_IP, 80);
    if (ss<0) return 0;

    FILE *ssock = fdopen(ss, "r+");
    if (!ssock) return 0;

    fprintf(ssock, 
            "GET /game/joinserver.jsp?user=%s&sessionId=%s&serverId=%s HTTP/1.1\r\n"
            "User-Agent: Java/1.6.0_24\r\n"
            "Host: session.minecraft.net\r\n"
            "Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2\r\n"
            "Connection: keep-alive\r\n"
            "\r\n", user, sessionId, serverId);
    fflush(ssock);

    char buf[4096];
    int http_ok=0, mc_ok=0;
    while(fgets(buf, sizeof(buf), ssock)) {
        printf("%s\n",buf);
        if (buf[0]==0x0d && buf[1]==0x0a) {
            // header ends
            char ok[2]; 
            fread(ok, 1, 2, ssock); // we cannot read this with fgets, since it's not a newline-terminated string
            if (ok[0]=='O' && ok[1]=='K') mc_ok=1;
            break;
        }
        if (!strcmp(buf,"HTTP/1.1 200 OK\r\n")) http_ok = 1;
    }

    fclose(ssock);

    return (http_ok && mc_ok);
}

ssize_t process_data(int is_client, uint8_t **sbuf, ssize_t *slen, 
                                    uint8_t **dbuf, ssize_t *dlen) {
    ssize_t consumed = 0;
    uint8_t *limit = *sbuf+*slen;
    int atlimit = 0;

    //char *s; // for strings
    char msgbuf[1048576];

    while(1) {
        uint8_t * p = *sbuf+consumed;
        uint8_t * msg_start = p;
        if (p>=limit) break;

        int modified = 0; 
        // if a message was modified, the handler will set this to 1, so
        // the loop exit routine knows it should not copy the message
        // itself, the handler already wrote necessary data to output

        uint8_t mtype = read_char(p);
        printf("%c %02x ",is_client?'C':'S',mtype);
        switch(mtype) {

        case MCP_Handshake: { // 02
            Rchar(proto);
            Rstr(username);
            Rstr(host);
            Rint(port);
            printf("Handshake: proto=%d user=%s server=%s:%d\n",proto,username,host,port);
            break;
        }

        case MCP_EncryptionKeyReq: { // FD
            Rstr(serverid);
            Rshort(pklen);
            uint8_t *pkey = p;
            Rskip(pklen);
            Rshort(tklen);
            uint8_t *token = p;
            Rskip(tklen);
            printf("Encryption Key Request : server=%s tklen=%d\n",serverid, tklen);
            hexdump(msg_start, *slen-consumed);


            // save relevant data in the MITM structure
            sprintf(mitm.s_id,"%s",serverid);
            memcpy(mitm.s_token, token, tklen);
            mitm.s_pklen = pklen;
            memcpy(mitm.s_pkey, pkey, pklen);

            // decode server PUBKEY to an RSA struct
            unsigned char *pp = pkey;
            d2i_RSA_PUBKEY(&mitm.s_rsa, &pp, pklen);
            if (mitm.s_rsa == NULL) {
                printf("Failed to decode the server's public key\n");
                exit(1);
            }
            RSA_print_fp(stdout, mitm.s_rsa, 4);

            // generate the server-side shared key pair
            RAND_pseudo_bytes(mitm.s_skey, 16);
            printf("Server-side shared key:\n");
            hexdump(mitm.s_skey, 16);


            // create a client-side RSA
            mitm.c_rsa = RSA_generate_key(1024, RSA_F4, NULL, NULL);
            if (mitm.c_rsa == NULL) {
                printf("Failed to generate client-side RSA key\n");
                exit(1);
            }
            RSA_print_fp(stdout, mitm.c_rsa, 4);

            // encode the client-side pubkey as DER
            pp = mitm.c_pkey;
            mitm.c_pklen = i2d_RSA_PUBKEY(mitm.c_rsa, &pp);

            // generate the client-side verification token
            RAND_pseudo_bytes(mitm.c_token, 4);
            

            // combine all to a MCP message to the client
            ssize_t outlen = 7 + strlen(serverid)*2 + mitm.c_pklen + sizeof(mitm.c_token);
            ssize_t wpos = *dlen;
            ARRAY_ADDG(*dbuf,*dlen,outlen,WRITEBUF_GRAN);
            uint8_t * wp = *dbuf+wpos;

            write_char(wp, 0xfd);
            wp += Wstr(wp, serverid);
            write_short(wp, mitm.c_pklen);
            memcpy(wp, mitm.c_pkey, mitm.c_pklen);
            wp += mitm.c_pklen;
            write_short(wp, 4);
            memcpy(wp, mitm.c_token, 4);

            modified = 1;

            break;
        }

        case MCP_EncryptionKeyResp: {
            if (!is_client) {
                Rshort(z1);
                Rshort(z2);
                printf("Server Encryption Start z1=%d z2=%d\n",z1,z2);
                //TODO: enable stream encryption now

                AES_set_encrypt_key(mitm.c_skey, 128, &mitm.c_aes);
                memcpy(mitm.c_enc_iv, mitm.c_skey, 16);
                memcpy(mitm.c_dec_iv, mitm.c_skey, 16);

                AES_set_encrypt_key(mitm.s_skey, 128, &mitm.s_aes);
                memcpy(mitm.s_enc_iv, mitm.s_skey, 16);
                memcpy(mitm.s_dec_iv, mitm.s_skey, 16);
                
                mitm.encstate = 1;
                mitm.passfirst = 1;
                
                printf("c_skey:   "); hexdump(mitm.c_skey,16);
                printf("c_enc_iv: "); hexdump(mitm.c_enc_iv,16);
                printf("c_dec_iv: "); hexdump(mitm.c_dec_iv,16);
                printf("s_skey:   "); hexdump(mitm.s_skey,16);
                printf("s_enc_iv: "); hexdump(mitm.s_enc_iv,16);
                printf("s_dec_iv: "); hexdump(mitm.s_dec_iv,16);

                break;
            }

            Rshort(sklen);
            uint8_t *skey = p;
            Rskip(sklen);
            Rshort(tklen);
            uint8_t *token = p;
            Rskip(tklen);

            char buf[4096];
            int dklen = RSA_private_decrypt(sklen, skey, buf, mitm.c_rsa, RSA_PKCS1_PADDING);
            if (dklen < 0) {
                printf("Failed to decrypt the shared key received from the client\n");
                exit(1);
            }
            printf("Decrypted client shared key, keylen=%d\n",dklen);
            hexdump(buf, dklen);
            memcpy(mitm.c_skey, buf, 16);

            int dtlen = RSA_private_decrypt(tklen, token, buf, mitm.c_rsa, RSA_PKCS1_PADDING);
            if (dtlen < 0) {
                printf("Failed to decrypt the verification token received from the client\n");
                exit(1);
            }
            printf("Decrypted client token, len=%d\n",dtlen);
            hexdump(buf, dtlen);
            printf("Original token:\n");
            hexdump(mitm.c_token,4);
            if (memcmp(buf, mitm.c_token, 4)) {
                printf("Token does not match!\n");
                exit(1);
            }
            
            // at this point, the client side is verified and the key is established
            // now send our response to the server

            ssize_t wpos = *dlen;
            ARRAY_ADDG(*dbuf,*dlen,5+2*RSA_size(mitm.s_rsa),WRITEBUF_GRAN);
            uint8_t * wp = *dbuf+wpos;

            write_char(wp,0xfc);

            int eklen = RSA_public_encrypt(sizeof(mitm.s_skey), mitm.s_skey, buf, mitm.s_rsa, RSA_PKCS1_PADDING);
            write_short(wp,(short)eklen);
            memcpy(wp, buf, eklen);
            wp+= eklen;

            int etlen = RSA_public_encrypt(sizeof(mitm.s_token), mitm.s_token, buf, mitm.s_rsa, RSA_PKCS1_PADDING);
            write_short(wp,(short)etlen);
            memcpy(wp, buf, etlen);
            wp+= etlen;

            modified = 1;

            
            // the final touch - send the authentication token to the session server
            unsigned char md[SHA_DIGEST_LENGTH];
            SHA_CTX sha; CLEAR(sha);

            SHA1_Init(&sha);
            SHA1_Update(&sha, mitm.s_id, strlen(mitm.s_id));
            SHA1_Update(&sha, mitm.s_skey, sizeof(mitm.s_skey));
            SHA1_Update(&sha, mitm.s_pkey, mitm.s_pklen);
            SHA1_Final(md, &sha);

            hexdump(md, SHA_DIGEST_LENGTH);
            print_hex(buf, md, SHA_DIGEST_LENGTH);
            printf("sessionId : %s\n", buf);

            if (!connect_session("Dorquemada", mitm.c_session, buf)) {
                printf("Registering session at session.minecraft.net failed\n");
                exit(1);
            }

            break;
        }

        case MCP_ServerListPing: { //FE
            printf("Server List Ping\n");
            break;
        }

        case MCP_Disconnect: { //FF
            Rstr(s);
            printf("Disconnect: %s\n",s);

#if 0
            msgbuf[0] = 0xff;
            ssize_t len = Wstr(msgbuf+1,"Barrels of Dicks!\xa7""12""\xa7""34");
            ssize_t wpos = *dlen;
            ARRAY_ADDG(*dbuf,*dlen,len+1,WRITEBUF_GRAN);
            memcpy(*dbuf+wpos,msgbuf,len+1);
            modified = 1;
#endif
            break;
        }

        default:
            printf("Unknown message %02x\n",mtype);
            hexdump(msg_start, *slen-consumed);
            //exit(1);
        }

        if (!atlimit) {
            // the message was ingested successfully
            if (!modified) {
                // the handler only ingested the data but did not modify it
                // we need to copy it 1-1 to the output
                ssize_t msg_len = p-msg_start;
                ssize_t wpos = *dlen;
                ARRAY_ADDG(*dbuf,*dlen,msg_len,WRITEBUF_GRAN);
                memcpy(*dbuf+wpos,msg_start,msg_len);
                printf("Copied %d bytes\n",msg_len);
                hexdump(msg_start,msg_len);
            }

            consumed = p-*sbuf;
        }
        else
            break;
            
    }                                           

    return consumed;
}

int pumpdata(int src, int dst, int is_client, flbuffers *fb) {
    // choose correct buffers depending on traffic direction
    uint8_t **sbuf, **dbuf;
    ssize_t *slen, *dlen;

    if (is_client) {
        sbuf = &fb->crbuf;
        slen = &fb->crlen;
        dbuf = &fb->swbuf;
        dlen = &fb->swlen;
    }
    else {
        sbuf = &fb->srbuf;
        slen = &fb->srlen;
        dbuf = &fb->cwbuf;
        dlen = &fb->cwlen;
    }

    // read incoming data to the source buffer
    ssize_t prevlen = *slen;
    int res = evfile_read(src, sbuf, slen, 1048576);

    switch (res) {
        case EVFILE_OK:
        case EVFILE_WAIT: {
            // decrypt read buffer if encryption is enabled
            // but only the new portion that was read
            if (mitm.encstate) {
                uint8_t *ebuf = *sbuf+prevlen;
                ssize_t elen  = *slen-prevlen;

                int num=0;
                if (is_client)
                    AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.c_aes, mitm.c_dec_iv, &num, AES_DECRYPT);
                else
                    AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.s_aes, mitm.s_dec_iv, &num, AES_DECRYPT);
            }

            // call the external method to process the input data
            // it will copy the data to dbuf as is, or modified, or it may
            // also insert other data

            ssize_t consumed = process_data(is_client, sbuf, slen, dbuf, dlen);
            if (consumed > 0) {
                if (*slen-consumed > 0)
                    memcpy(*sbuf, *sbuf+consumed, *slen-consumed);
                *slen -= consumed;
            }

            // encrypt write buffer if encryption is enabled
            if (mitm.encstate) {
                uint8_t *ebuf = *dbuf;
                ssize_t elen  = *dlen;
                
                // when the encryption is enabled for the first time,
                // the last message (0xFC from the server) still goes out unencrypted,
                // so we skip the first byte and clear the flag
                //FIXME: watch out for race conditions
                if (mitm.passfirst)
                    mitm.passfirst=0;
                else {
                    int num=0;
                    if (is_client)
                        AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.s_aes, mitm.s_enc_iv, &num, AES_ENCRYPT);
                    else
                        AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.c_aes, mitm.c_enc_iv, &num, AES_ENCRYPT);
                }
            }


            // send out the contents of the write buffer
            uint8_t *buf = *dbuf;
            ssize_t len = *dlen;
            while (len > 0) {
                //printf("sending %d bytes to %s\n",len,is_client?"server":"client");
                ssize_t sent = write(dst,buf,len);
                buf += sent;
                len -= sent;
            }
            *dlen = 0;
            return 0;
        }         
        case EVFILE_ERROR:
        case EVFILE_EOF:
            return -1;
    }
}

int proxy_pump(uint32_t ip, uint16_t port) {
    printf("%s:%d\n",__func__,__LINE__);
    CLEAR(mitm);


    // common poll array for all sockets
    pollarray pa;
    CLEAR(pa);


    // Minecraft proxy server
    pollgroup sg;
    CLEAR(sg);

    int ss = sock_server_ipv4_tcp_any(port);
    if (ss<0) return -1;
    pollarray_add(&pa, &sg, ss, MODE_R, NULL);


    // fake session.minecraft.com web server
    pollgroup wg; 
    CLEAR(wg);

    int ws = sock_server_ipv4_tcp_local(8880);
    if (ws<0) return -1;
    pollarray_add(&pa, &wg, ws, MODE_R, NULL);


    // client side socket
    pollgroup cg;
    CLEAR(cg);


    // server side socket
    pollgroup mg;
    CLEAR(mg);

    int cs=-1, ms=-1;
    flbuffers fb;
    CLEAR(fb);




    // main pump
    int stay = 1, i;
    while(stay) {
        // common poll for all registered sockets
        evfile_poll(&pa, 1000);

#if 0
        printf("%s:%d sg:%d,%d,%d cg:%d,%d,%d mg:%d,%d,%d\n",
               __func__,__LINE__,
               sg.rn,sg.wn,sg.en,
               cg.rn,cg.wn,cg.en,
               mg.rn,mg.wn,mg.en);
#endif

        // handle connection requests from the client side
        if (sg.rn > 0 && cs == -1)
            if (handle_server(pa.p[sg.r[0]].fd, ip, port, &cs, &ms)) {
                printf("%s:%d - handle_server successful cs=%d ms=%d\n",
                       __func__,__LINE__, cs, ms);
                
                // handle_server was able to accept the client connection and
                // also open the server-side connection, we need to add these
                // new sockets to the groups cg and mg respectively
                pollarray_add(&pa, &cg, cs, MODE_R, NULL);
                pollarray_add(&pa, &mg, ms, MODE_R, NULL);

                init_mitm();
            }

        // handle connection requests from the client side
        if (wg.rn > 0) {
            handle_session_server(pa.p[wg.r[0]].fd);
        }

        if (cg.rn > 0) {
            if (pumpdata(cs, ms, 1, &fb)) {
                pollarray_remove(&pa, cs);
                pollarray_remove(&pa, ms);
                close(cs); close(ms);
                cs = ms = -1;
                if (fb.crbuf) free(fb.crbuf);
                if (fb.cwbuf) free(fb.cwbuf);
                if (fb.srbuf) free(fb.srbuf);
                if (fb.swbuf) free(fb.swbuf);
                CLEAR(fb);
            }
        }
               
        if (mg.rn > 0) {
            if (pumpdata(ms, cs, 0, &fb)) {
                pollarray_remove(&pa, cs);
                pollarray_remove(&pa, ms);
                close(cs); close(ms);
                cs = ms = -1;
                if (fb.crbuf) free(fb.crbuf);
                if (fb.cwbuf) free(fb.cwbuf);
                if (fb.srbuf) free(fb.srbuf);
                if (fb.swbuf) free(fb.swbuf);
                CLEAR(fb);
            }
        }               
    }
}

int main(int ac, char **av) {
    proxy_pump(SERVER_IP, SERVER_PORT);

    return 0;
}
