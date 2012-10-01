#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <libtasn1.h>

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

            write_file("public_key",pkey,pklen);
            write_file("token",token,tklen);

            printf("Encryption Key Request : server=%s\n",serverid);
            hexdump(pkey,pklen);
            hexdump(token,tklen);
            break;
        }

        case MCP_ServerListPing: { //FE
            printf("Server List Ping\n");
            break;
        }

        case MCP_Disconnect: { //FF
            Rstr(s);
            printf("Disconnect: %s\n",s);

            msgbuf[0] = 0xff;
            ssize_t len = Wstr(msgbuf+1,"Barrels of Dicks!\xa7""12""\xa7""34");
            ssize_t wpos = *dlen;
            ARRAY_ADDG(*dbuf,*dlen,len+1,WRITEBUF_GRAN);
            memcpy(*dbuf+wpos,msgbuf,len+1);
            modified = 1;
            break;
        }

        default:
            printf("Unknown message %02x\n",mtype);
            hexdump(msg_start, *slen-consumed);
            exit(1);
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
    printf("pumdata %d -> %d\n",src, dst);

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
    int res = evfile_read(src, sbuf, slen, 1048576);

    switch (res) {
        case EVFILE_OK:
        case EVFILE_WAIT: {
            // call the external method to process the input data
            // it will copy the data to dbuf as is, or modified, or it may
            // also insert other data
            ssize_t consumed = process_data(is_client, sbuf, slen, dbuf, dlen);
            if (consumed > 0) {
                if (*slen-consumed > 0)
                    memcpy(*sbuf, *sbuf+consumed, *slen-consumed);
                *slen -= consumed;
            }
            
            // send out the contents of the write buffer
            uint8_t *buf = *dbuf;
            ssize_t len = *dlen;
            while (len > 0) {
                //hexdump(buf,len);
                printf("sending %d bytes to %s\n",len,is_client?"server":"client");
                ssize_t sent = write(dst,buf,len);
                printf("sent %d bytes to %s\n",sent,is_client?"server":"client");
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

    // setup server
    int ss = sock_server_ipv4_tcp_any(port);
    if (ss<0) return -1;

    pollarray pa; // common poll array for all sockets/files
    CLEAR(pa);

    pollgroup sg; // server listening socket
    CLEAR(sg);

    pollgroup cg; // client side socket
    CLEAR(cg);

    pollgroup mg; // server side socket
    CLEAR(mg);

    // add the server to listen for new client connections
    pollarray_add(&pa, &sg, ss, MODE_R, NULL);

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
