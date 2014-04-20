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

#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/sha.h>
#include <openssl/aes.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_buffers.h"
#include "lh_bytes.h"
#include "lh_files.h"
#include "lh_net.h"
#include "lh_event.h"
#include "lh_compress.h"

#define SERVER_IP 0x0a000001   // Local server on Peorth
//#define SERVER_IP 0xc7a866c2 // Beciliacraft 199.168.102.194
//#define SERVER_IP 0x53e9ed40   // 2B2T 83.233.237.64
//#define SERVER_IP 0x53b3151f   // rujush.org   83.179.21.31
//#define SERVER_IP 0x6c3d10d3   // WC Minecraft 108.61.16.211
#define SERVER_PORT 25565

#define ASYNC_THRESHOLD 500000
#define NEAR_THRESHOLD 40000

#define G_MCSERVER  1
#define G_WEBSERVER 2
#define G_PROXY     3

#define STATE_IDLE     0
#define STATE_STATUS   1
#define STATE_LOGIN    2
#define STATE_PLAY     3


////////////////////////////////////////////////////////////////////////////////



#if 0


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
#define MCP_CreativeInvAction   0x6b
#define MCP_EnchantItem         0x6c
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

typedef struct {
    int    type;
    char * name;
    int    metaadd;
} mobmeta;

mobmeta MOBS[] = {
    { 50, "Creeper",      2 },
    { 51, "Skeleton",     0 },
    { 52, "Spider",       1 },
    { 53, "Giant Zombie", 0 },
    { 54, "Zombie",       0 },
    { 55, "Slime",        1 },
    { 56, "Ghast",        1 },
    { 57, "Zombie Pigman",0 },
    { 58, "Enderman",     2 },
    { 59, "Cave Spider",  1 },
    { 60, "Silverfish",   0 },
    { 61, "Blaze",        1 },
    { 62, "Magma Cube",   1 },
    { 63, "Ender Dragon", 2 },
    { 64, "Wither",       1000 },
    { 65, "Bat",          1000 },
    { 66, "Witch",        1000 },
    { 90, "Pig",          1 },
    { 91, "Sheep",        1 },
    { 92, "Cow",          0 },
    { 93, "Chicken",      0 },
    { 94, "Squid",        0 },
    { 95, "Wolf",         7 },
    { 96, "Mooshoom",     0 },
    { 97, "Snowman",      0 },
    { 98, "Ocelot",       7 },
    { 99, "Iron Golem",   1 },
    { 120, "Villager",    4 },
    { -1, "",             0 }
};

// List of enchantable items
// Ref: http://wiki.vg/wiki/index.php?title=Slot_Data&oldid=2152
short TOOLS[] = {
    256,257,258,        // iron tools
    259,                // flint and steel
    261,                // bow
    267,                // iron sword
    268,269,270,271,    // wooden tools
    272,273,274,275,    // stone tools
    276,277,278,279,    // diamond tools
    283,284,285,286,    // golden tools
    290,291,292,293,294,// hoes
    298,299,300,301,    // leather equipment
    302,303,304,305,    // chainmail equipment
    306,307,308,309,    // iron equipment
    310,311,312,313,    // diamond equipment
    314,315,316,317,    // golden equipment
    346,                // fishing rod
    359,                // shears
    -1,
};

static inline int istool(int id) {
    int i;
    for(i=0; TOOLS[i]>=0; i++)
        if (TOOLS[i] == id)
            return 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

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
        Rshort(metalen);                                    \
        if (metalen != -1)                                  \
            Rskip(metalen);                                 \
    }
//TODO: extract metadata into buffer

#define Rmobmeta(n)                                                     \
    while (!atlimit) {                                                         \
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

#endif

lh_pollarray pa;

struct {
    int state;          // handshake state

    int cs;             // connected socket to client
    int ms;             // connected socket to server

    // connections
    lh_conn * cs_conn; // to the client
    lh_conn * ms_conn; // to the server

    // decoded buffers
    lh_buf_t  cs_rx;   // client -> proxy
    lh_buf_t  cs_tx;   // proxy -> client
    lh_buf_t  ms_rx;   // server -> proxy
    lh_buf_t  ms_tx;   // proxy -> server

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
    // zero-terminated string, so no need for length value
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

    FILE * output;

    // Various options
    int cowradar;
    int grinding;
    int maxlevel;

    int64_t last[2];
} mitm;

#include <time.h>


#if 0


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

//#define SESSION_SERVER_IP 0xb849df28
//#define SESSION_SERVER_IP 0x3213EA8A
#define SESSION_SERVER_IP 0x36F34FE8

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

#if 1
#define PRALL if (mprio<1)
#else
#define PRALL
#endif

static int msg_prio[256] = {
 // 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F

    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, // 0
    0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, // 1
    1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, // 2
    0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, // 3

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 7

    0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // C
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // D
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // E
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F
};

void process_chunks(int count, uint32_t *XX, uint32_t *ZZ, uint16_t *bm, uint8_t * cdata, ssize_t clen) {
    ssize_t len;
    unsigned char * data = zlib_decode(cdata, clen, &len);
    if (!data) { printf("zlib_decode failed!\n"); exit(1); }
    uint8_t *ptr = data;

    //printf("Decoded bulk chunk: %d bytes, %d chunks\n",len, count);

    int i;
    uint8_t *pdata = data;

    for(i=0; i<count; i++) {
        int32_t X = XX[i];
        int32_t Z = ZZ[i];

        uint32_t Y,n=0;
        for (Y=0;Y<16;Y++)
            if (bm[i] & (1 << Y))
                n++;
        if (n==0) continue; // ignore "empty" updates

        //printf("Chunk %d at %d\n",i,pdata-data);
        char fname[256];
        sprintf(fname, "chunks/chunk_%04x_%04x.dat",(unsigned short)X,(unsigned short)Z);
        FILE *fd = open_file_w(fname);

        printf("CHUNK %3d,%3d,%d\n",X,Z,n);

        write_file_f(fd, (unsigned char *)&n, sizeof(n));
        write_file_f(fd, (unsigned char *)&X, sizeof(X));
        write_file_f(fd, (unsigned char *)&Z, sizeof(Z));
        write_file_f(fd, pdata, n*4096);
        fclose(fd);

        pdata += 10240*n+256;
    }

    free (data);
}

#endif

typedef struct {
    int x,y,z;
} bcoord;

struct {
    int state;      // current building state: 0 - stopped, 1 - waiting for trigger, 2 - building
    int nb;         // number of blocks still left
    bcoord *b;      // array of coordinates

    int slot;
    int count;
    bcoord cur;
} build;

typedef struct {
    int eid;
    int type;
    char * name;

    int x;
    int y;
    int z;

} entity;

// tracking of mobs and players
struct {
    // tracking self
    int32_t pid;
    bcoord self;

    // tracking entities
    entity *ent;
    int nent;
} track;

#if 0

void trigger_build(int x, int y, int z, int dir, int slot, int count, int cx, int cy, int cz) {
    build.state = 2;
    ARRAY_ADD(build.b, build.nb, 7);
    build.cur.x = cx; build.cur.y = cy; build.cur.z = cz;
    build.slot = slot;
    build.count = count;

    build.b[0].x=x+0; build.b[0].y=y+0; build.b[0].z=z+1; 
    build.b[1].x=x+0; build.b[1].y=y+1; build.b[1].z=z+0; 
    build.b[2].x=x+0; build.b[2].y=y+1; build.b[2].z=z+1; 
    build.b[3].x=x+1; build.b[3].y=y+0; build.b[3].z=z+0; 
    build.b[4].x=x+1; build.b[4].y=y+0; build.b[4].z=z+1; 
    build.b[5].x=x+1; build.b[5].y=y+1; build.b[5].z=z+0; 
    build.b[6].x=x+1; build.b[6].y=y+1; build.b[6].z=z+1;
}

#define SQR(x) ((x)*(x))

void progress_build(int x, int y, int z, uint8_t **buf, ssize_t *len) {
    if (build.state != 2) return;
    printf("Checking build state, pos=%d,%d,%d blocks=%d slot=%d,%d\n",x,y,z,build.nb,build.slot,build.count);

    int i;
    for(i=0; i<build.nb; i++) {
        bcoord *b = &build.b[i];
        int dist = SQR(x-b->x)+SQR(y-b->y)+SQR(z-b->z);
        if (dist <= 9) {
            printf(" Place Cube %d %d,%d,%d\n",i,b->x,b->y,b->z);

            uint8_t mbuf[4096];
            uint8_t *p = mbuf;
            write_char(p,0x0f);
            write_int(p, b->x);
            write_char(p, (unsigned char)b->y);
            write_int(p, b->z);
            write_char(p,1);

            write_short(p,4);  // ID=Cobblestone
            write_char(p,1);   // count
            write_short(p,0);  // damage
            write_short(p,-1);
            
            write_char(p,0);
            write_char(p,0);
            write_char(p,0);

            ssize_t mlen = p-mbuf;
            ssize_t wpos = *len;
            ARRAY_ADDG(*buf,*len,mlen,WRITEBUF_GRAN);
            memcpy(*buf+wpos, mbuf, mlen);

            ARRAY_DELETE(build.b, build.nb, i);
            i--;
            break;
        }
    }

    if (build.nb == 0)
        build.state = 0;
}

int process_cmd(const char *msg, char *answer) {
    if (msg[0] != '#') return 0;
    answer[0] = 0;

    // tokenize
    char *words[256];
    CLEAR(words);
    int w=0;

    char wbuf[4096];
    strncpy(wbuf, msg+1, sizeof(wbuf));
    char *wsave;

    char *wstr = wbuf;
    do {
        words[w++] = strtok_r(wstr, " ", &wsave);
        wstr = NULL;
    } while(words[w-1]);
    w--;

    if (w==0) return 0;

    if (!strcmp(words[0],"build")) {
        if (!words[1]) return 0;
        if (!strcmp(words[1],"abort")) {
            sprintf(answer, "<MC Proxy> Aborting build");
            if (build.b) free(build.b);
            CLEAR(build);
            return 1;
        }

        if (build.state) {
            sprintf(answer, "<MC Proxy> Another building is still in progress (%d blocks remaining). Abort it first.", build.nb);
            return 1;
        }

        if (!strcmp(words[1],"cube")) {
            sprintf(answer, "<MC Proxy> Building 3x3x3 cube. Place the bottom NW block to start.");
            build.state = 1;
            return 1;
        }
        if (!strcmp(words[1],"bridge")) {
            sprintf(answer, "<MC Proxy> Building bridge");
            return 1;
        }
        return 0;
    }
    if (!strcmp(words[0],"cowradar")) {
        mitm.cowradar = !mitm.cowradar;
        sprintf(answer, "<MC Proxy> Cowradar is %s",mitm.cowradar?"enabled":"disabled");
        return 1;
    }
    if (!strcmp(words[0],"check")) {
        sprintf(answer, "<MC Proxy> Coords:%d,%d (%d)",track.self.x/32,track.self.z/32,track.self.y/32);
        return 1;
    }
    if (!strcmp(words[0],"entities")) {
        sprintf(answer, "<MC Proxy> Tracking %d entities",track.nent);
        return 1;
    }
    if (!strcmp(words[0],"near")) {
        int32_t res[4096];
        int n = near_entities(51,res,4096);
        sprintf(answer, "<MC Proxy> %d entities nearby",n);
        return 1;
    }
    if (!strcmp(words[0],"grind")) {
        mitm.grinding = !mitm.grinding;
        int maxlevel=0;

        if (words[1] && sscanf(words[1],"%d",&maxlevel)==1)
            mitm.maxlevel = maxlevel;
        else
            mitm.maxlevel = 30;

        if (mitm.grinding)
            sprintf(answer, "[MC Proxy] Grinding is enabled, grinding to level %d",mitm.maxlevel);
        else
            sprintf(answer, "[MC Proxy] Grinding is disabled");
        return 1;
    }
    else
        return 0;
}

void queue_message(uint8_t **mbuf, ssize_t *mlen, uint8_t *data, ssize_t len) {
    ssize_t _mlen = *mlen;
    ARRAY_ADDG(*mbuf,*mlen,len,WRITEBUF_GRAN);
    memmove(*mbuf+_mlen, data, len);
}

void queue_chat_message(uint8_t **mbuf, ssize_t *mlen, char *message) {
    char msg[4096];
    char *wp = msg;
    write_char(wp,MCP_ChatMessage);
    wp += Wstr(wp,message);
    queue_message(mbuf, mlen, msg, wp-msg);
}

////////////////////////////////////////////////////////////////////////////////

entity * find_entity(int32_t eid) {
    int i;
    for(i=0; i<track.nent; i++)
        if (track.ent[i].eid == eid)
            return &track.ent[i];
    return NULL;
}

void remove_entity(int32_t eid) {
    int i;
    for(i=0; i<track.nent; i++)
        if (track.ent[i].eid == eid)
            ARRAY_DELETE(track.ent, track.nent, i);
}

entity * add_entity(int32_t eid, int type, int x, int y, int z) {
    if (find_entity(eid))
        remove_entity(eid);
    ARRAY_ADD(track.ent, track.nent,1);
    entity *e = &track.ent[track.nent-1];
    e->eid = eid;
    e->type = type;
    e->name = NULL;
    e->x = x;
    e->y = y;
    e->z = z;
    return e;
}

entity * add_named_entity(int32_t eid, int x, int y, int z, const char *pname) {
    entity *e = add_entity(eid,0,x,y,z);
    e->name = strdup(pname);
    return e;
}

int near_entities(int type, int32_t *res, int max) {
    int i,j=0;
    for(i=0; i<track.nent && j<max; i++)
        if (type<0 || track.ent[i].type == type) {
            int dx = track.ent[i].x - track.self.x;
            int dy = track.ent[i].y - track.self.y;
            int dz = track.ent[i].z - track.self.z;
            int dist = dx*dx+dy*dy+dz*dz;
            //printf("%d %d %4d %d,%d,%d\n",track.ent[i].type,track.ent[i].eid,dist,dx,dy,dz);
            if (dist < NEAR_THRESHOLD)
                res[j++] = track.ent[i].eid;
        }
    return j;
}

////////////////////////////////////////////////////////////////////////////////

void process_async(int is_client, uint8_t **sbuf, ssize_t *slen, 
                                  uint8_t **dbuf, ssize_t *dlen,
                                  uint8_t **abuf, ssize_t *alen) {
    //printf("process_async: client=%d\n",is_client);

    if (is_client && mitm.grinding) {
        int ent;
        int n = near_entities(51, &ent, 1);
        if (n>0) {
            printf("Hitting entity %d\n",ent);
            char msg[4096];
            char *wp = msg;

            // 1. Animation/swing arm
            write_char(wp,MCP_Animation);
            write_int(wp,track.pid);
            write_char(wp,1);

            // 2. UseEntity
            write_char(wp,MCP_UseEntity);
            write_int(wp,track.pid);
            write_int(wp,ent);
            write_char(wp,1);

            // 3. Player / on ground
            write_char(wp,MCP_Player);
            write_char(wp,1);

            queue_message(dbuf, dlen, msg, wp-msg);
        }
    }
}

ssize_t process_data(int is_client, uint8_t **sbuf, ssize_t *slen, 
                                    uint8_t **dbuf, ssize_t *dlen,
                                    uint8_t **abuf, ssize_t *alen) {
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
        int mprio = msg_prio[mtype];

        PRALL printf("%c %02x ",is_client?'C':'S',mtype);
        switch(mtype) {

        case MCP_KeepAlive: { // 00
            Rint(id);
            PRALL printf("Keepalive: id=%08x\n",id);
            break;
        }
       
        case MCP_LoginRequest: { // 01
            Rint(eid);
            Rstr(leveltype);
            Rchar(gamemode);
            Rchar(dimension);
            Rchar(difficulty);
            Rchar(unused);
            Rchar(maxplayers);
            PRALL printf("Login Request eid=%d ltype=%s mode=%d dim=%d diff=%d maxplayers=%d\n",
                   eid,leveltype,gamemode,dimension,difficulty,maxplayers);
            track.pid = eid;
            break;
        }

        case MCP_Handshake: { // 02
            Rchar(proto);
            Rstr(username);
            Rstr(host);
            Rint(port);
            PRALL printf("Handshake: proto=%d user=%s server=%s:%d\n",proto,username,host,port);
            break;
        }

        case MCP_ChatMessage: { // 03
            Rstr(msg);
            PRALL printf("Chat Message: %s\n",msg);

            if (is_client && msg[0]=='#') {
                char answer[4096];
                if (process_cmd(msg,answer)) {
                    modified = 1; // so the message won't be forwarded to the server
                    if (answer[0]) queue_chat_message(abuf, alen, answer);
                }
            }

            break;
        }

        case MCP_TimeUpdate: { // 04
            Rlong(age);
            Rlong(tm);
            PRALL printf("Time Update: age=%lld time=%lld (%dd %dh)\n",age,tm,(int)(tm/24000L),(int)(tm%24000)/1000);
            break;
        }

        case MCP_EntityEquipment: { //05
            Rint(eid);
            Rshort(sid);
            Rslot(s);
            printf("Entity Equipment: EID=%d slot=%d item=\n",eid,sid);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_SpawnPosition: { //06
            Rint(x);
            Rint(y);
            Rint(z);
            printf("Spawn Position: %d,%d,%d\n",x,y,z);
            track.self.x = x;
            track.self.y = y;
            track.self.z = z;
            break;
        }

        case MCP_UseEntity: { //07
            Rint(user);
            Rint(target);
            Rchar(button);
            printf("Use Item: user=%d target=%d %c\n",user,target,button?'L':'R');
            break;
        }

        case MCP_UpdateHealth: { //08
            Rshort(health);
            Rshort(food);
            Rfloat(saturation);
            printf("UpdateHealth: HP=%d FP=%d SP=%.1f\n",health,food,saturation);
            break;
        }

        case MCP_Respawn: { // 09
            Rint(dimension);
            Rchar(difficulty);
            Rchar(gamemode);
            Rshort(height);
            Rstr(leveltype);
            printf("Respawn: dim=%d diff=%d gamemode=%d height=%d leveltype=%s\n",
                   dimension,difficulty,gamemode,height,leveltype);
            break;
        }

        case MCP_Player: { //0A
            Rchar(ground);
            PRALL printf("Player: ground=%d\n",ground);
            break;
        }

        case MCP_PlayerPosition: { //0B
            int px,py,pz;
            uint8_t **buf;
            ssize_t *len;

            Rdouble(x);
            Rdouble(y);
            Rdouble(stance);
            Rdouble(z);
            Rchar(ground);
            PRALL printf("Player Position: coord=%.1lf,%.1lf,%.1lf stance=%.1lf ground=%d\n",
                   x,y,z,stance,ground);

            px = (int)x;
            py = (int)y;
            pz = (int)z;
            
            if (is_client) {
                buf = dbuf;
                len = dlen;
            }
            else {
                buf = abuf;
                len = alen;
            }

            // copy the message so we can ensure it goes out first
            queue_message(dbuf, dlen, msg_start, p-msg_start);
            modified = 1;

            // save own position for track
            track.self.x = (int)(x*32);
            track.self.y = (int)(y*32);
            track.self.z = (int)(z*32);

            if (build.state == 2)
                progress_build(px, py, pz, buf, len);

            break;
        }

        case MCP_PlayerLook: { //0C
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);
            PRALL printf("Player Look: rot=%.1f,%.1f ground=%d\n",yaw,pitch,ground);
            break;
        }

        case MCP_PlayerPositionLook: { //0D
            int px,py,pz;
            uint8_t **buf;
            ssize_t *len;

            if (is_client) {
                Rdouble(x);
                Rdouble(y);
                Rdouble(stance);
                Rdouble(z);
                Rfloat(yaw);
                Rfloat(pitch);
                Rchar(ground);
                PRALL printf("Player Position & Look: coord=%.1f,%.1f,%.1f stance=%.1f "
                       "rot=%.1f,%.1f ground=%d\n",x,y,z,stance,yaw,pitch,ground);
                px = (int)x;
                py = (int)y;
                pz = (int)z;

                buf = dbuf;
                len = dlen;
            }
            else {
                Rdouble(x);
                Rdouble(stance);
                Rdouble(y);
                Rdouble(z);
                Rfloat(yaw);
                Rfloat(pitch);
                Rchar(ground);
                PRALL printf("Player Position & Look: coord=%.1f,%.1f,%.1f stance=%.1f "
                       "rot=%.1f,%.1f ground=%d\n",x,y,z,stance,yaw,pitch,ground);

                px = (int)x;
                py = (int)y;
                pz = (int)z;

                // save own position for tracking
                track.self.x = (int)(x*32);
                track.self.y = (int)(y*32);
                track.self.z = (int)(z*32);

                buf = abuf;
                len = alen;
            }

            // copy the message so we can ensure it goes out first
            queue_message(dbuf, dlen, msg_start, p-msg_start);
            modified = 1;

            if (build.state == 2)
                progress_build(px, py, pz, buf, len);

            break;
        }

        case MCP_PlayerDigging: {//0e
            Rchar(status);
            Rint(x);
            Rchar(y);
            Rint(z);
            Rchar(face);
            printf("Player Digging: coord=%d,%d,%d face=%d\n",x,(unsigned int)y,z,face);
            break;
        }

        case MCP_PlayerBlockPlace: { //0f
            Rint(x);
            Rchar(y);
            Rint(z);
            Rchar(dir);
            Rslot(s);
            
            printf("Player Block Placement: coord=%d,%d,%d dir=%d item:",x,(unsigned int)y,z,dir);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }

            Rchar(cx);
            Rchar(cy);
            Rchar(cz);

            printf(" cursor: %d,%d,%d\n",cx,cy,cz);

            if (build.state == 1 && s.id == 4)
                trigger_build(x,y,z,dir,s.id,s.count,cx,cy,cz);

            break;
        }

        case MCP_HeldItemChange: {//10
            Rshort(sid);
            printf("Held Item Change: slot=%d\n",sid);
            break;
        }

        case MCP_UseBed: {//11
            Rint(eid);
            Rchar(unk);
            Rint(x);
            Rchar(y);
            Rint(z);
            printf("Use Bed: EID=%d coord=%d,%d,%d\n",eid,x,(unsigned int)y,z);
            break;
        }

        case MCP_Animation: {//12
            Rint(eid);
            Rchar(aid);
            PRALL printf("Animation: EID=%d anim=%d\n",eid,aid);
            break;
        }

        case MCP_EntityAction: { //13
            Rint(eid);
            Rchar(action);
            PRALL printf("Entity Action: eid=%d action=%d\n",eid,action);
            break;
        }

        case MCP_SpawnNamedEntity: {//14
            Rint(eid);
            Rstr(pname);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rshort(item);

            Rmobmeta(m);

            printf("Spawn Named Entity: EID=%d %s coords=%d,%d,%d rot=%d,%d item=%d\n",
                   eid,pname,x/32,y/16,z/32,yaw,pitch,item);

            // prepare notification to be inserted into chat
            char notify[4096];
            sprintf(notify, "§c§l[MCProxy] NAMED ENTITY: %s coords=%d,%d,%d",pname,x/32,y/32,z/32);
            queue_chat_message(dbuf, dlen, notify);

            add_named_entity(eid,x,y,z,pname);

            break;
        }

        case MCP_SpawnDroppedItem: { //15
            Rint(eid);
            Rslot(s);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(rot);
            Rchar(pitch);
            Rchar(roll);
            printf("Spawn Dropped Item: EID=%d %d,%d,%d ",eid,x/32,y/16,z/32);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_CollectItem: { //16
            Rint(ieid);
            Rint(ceid);
            PRALL printf("Collect Item: item=%d collector=%d\n",ieid,ceid);
            break;
        }

        case MCP_SpawnObject: {//17
            Rint(eid);
            Rchar(type);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rint(eeid);
            //NOTE: The following segment was replaced by "ObjectData" in the wiki,
            //although the contents stay the same - check if it works correctly
            if (eeid == 0) {
                printf("Spawn Object: eid=%d type=%d coord=%d,%d,%d\n",eid,type,x/32,y/16,z/32);
            }
            else {
                Rshort(vx);
                Rshort(vy);
                Rshort(vz);
                printf("Spawn Object: eid=%d type=%d coord=%d,%d,%d Fireball eeid=%d speed=%d,%d,%d\n",eid,type,x/32,y/16,z/32,eeid,vx,vy,vz);
            }

            break;
        }

        case MCP_SpawnMob: { //18
            Rint(eid);
            Rchar(type);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rchar(hyaw);
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);

            int i;
            for(i=0; MOBS[i].type>0 && MOBS[i].type!=type; i++);
            if (MOBS[i].type<0) {
                printf("Unknown mob type %d\n",type);
                exit(1);
            }

            Rmobmeta(m);
            printf("Spawn Mob: EID=%d type=%d %s %d,%d,%d\n",eid,type,MOBS[i].name,x/32,y/16,z/32);

            if (mitm.cowradar && (type==92 || type==96)) {
                // prepare notification to be inserted into chat
                char notify[4096];
                sprintf(notify, "§c§l[MCProxy] COW: %s coords=%d,%d,%d",MOBS[i].name,x/32,y/32,z/32);
                queue_chat_message(dbuf, dlen, notify);
            }
            add_entity(eid,type,x,y,z);


            break;
        }

        case MCP_SpawnPainting: { //19
            Rint(eid);
            Rstr(title);
            Rint(x);
            Rint(y);
            Rint(z);
            Rint(dir);
            printf("Spawn Painting: EID=%d %s %d,%d,%d %d\n",eid,title,x,y,z,dir);
            break;
        }

        case MCP_SpawnExperienceOrb: { //1A
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rshort(count);
            printf("Spawn Experience Orb: EID=%d coord=%d,%d,%d count=%d\n",eid,x,y,z,count);
            break;
        }

        case MCP_EntityVelocity: { //1C
            Rint(eid);
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);
            PRALL printf("Entity Velocity: EID=%d %d,%d,%d\n",eid,vx,vy,vz);
            break;
        }

        case MCP_DestroyEntity: { //1D
            Rchar(count);
            printf("Destroy Entity: %d EIDs:",count);
            int i;
            for(i=0; i<count; i++) {
                Rint(eid);
                printf(" %d",eid);
                remove_entity(eid);
            }
            printf("\n");
            break;
        }

        case MCP_Entity: { //1E
            Rint(eid);
            PRALL printf("Entity: EID=%d\n",eid);
            break;
        }

        case MCP_EntityRelativeMove: { //1F
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);
            PRALL printf("Entity Relative Move: EID=%d move=%d,%d,%d\n",eid,dx,dy,dz);
            entity *e = find_entity(eid);
            if (e) {
                e->x += dx;
                e->y += dx;
                e->z += dx;
            }
            break;
        }



        case MCP_EntityLook: { //20
            Rint(eid);
            Rchar(yaw);
            Rchar(pitch);
            PRALL printf("Entity Look: EID=%d, yaw=%d pitch=%d\n",eid,yaw,pitch);
            break;
        }

        case MCP_EntityLookRelmove: { //21
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);
            Rchar(yaw);
            Rchar(pitch);
            PRALL printf("Entity Look and Relative Move: EID=%d move=%d,%d,%d look=%d,%d\n",eid,dx,dy,dz,yaw,pitch);
            entity *e = find_entity(eid);
            if (e) {
                e->x += dx;
                e->y += dx;
                e->z += dx;
            }
            break;
        }

        case MCP_EntityTeleport: { //22
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            PRALL printf("Entity Teleport: EID=%d move=%d,%d,%d look=%d,%d\n",eid,x/32,y/16,z/32,yaw,pitch);
            entity *e = find_entity(eid);
            if (e) {
                e->x = x;
                e->y = y;
                e->z = z;
            }
            break;
        }

        case MCP_EntityHeadLook: { //23
            Rint(eid);
            Rchar(hyaw);
            PRALL printf("Entity Head Look: EID=%d %d\n",eid,hyaw);
            break;
        }

        case MCP_EntityStatus: { //26
            Rint(eid);
            Rchar(status);
            PRALL printf("Entity Status: EID=%d status=%d\n",eid,status);
            break;
        }

        case MCP_AttachEntity: { //27
            Rint(eid);
            Rint(vid);
            PRALL printf("Attach Entity: EID=%d vehicle=%d\n",eid,vid);
            break;
        }

        case MCP_EntityMetadata: { //28
            //printf("Entity Metadata, dump follows:\n");
            //hexdump(msg_start, *slen-consumed);
            Rint(eid);
            Rmobmeta(m);
            PRALL printf("Entity Metadata: EID=%d\n",eid);
            break;
        }

        case MCP_EntityEffect: { //29
            Rint(eid);
            Rchar(effect);
            Rchar(amp);
            Rshort(duration);
            PRALL printf("Entity Effect: EID=%d effect=%d amp=%d dur=%d\n",
                   eid,effect,amp,duration);
            break;
        }

        case MCP_RemoveEntityEffect: { //2A
            Rint(eid);
            Rchar(effect);
            PRALL printf("Remove Entity Effect: EID=%d effect=%d\n",eid,effect);
            break;
        }

        case MCP_SetExperience: { //2B
            Rfloat(expbar);
            Rshort(level);
            Rshort(exp);
            PRALL printf("Set Experience: EXP=%d LVL=%d bar=%.1f\n",exp,level,expbar);

            if (mitm.grinding && level >= mitm.maxlevel) {
                mitm.grinding = 0;
                char notify[4096];
                sprintf(notify, "§c[MCProxy] Grinding finished with level %d",level);
                queue_chat_message(dbuf, dlen, notify);                
            }

            break;
        }

        case MCP_ChunkData: { //33
            Rint(X);
            Rint(Z);
            Rchar(cont);
            Rshort(primary_bitmap);
            Rshort(add_bitmap);
            Rint(compsize);
            uint8_t *data = p;
            Rskip(compsize);
            PRALL printf("Chunk Data: %d,%d cont=%d PBM=%04x ABM=%04x compsize=%d\n",X,Z,cont,primary_bitmap,add_bitmap,compsize);
            //process_chunks(1, &X, &Z, &primary_bitmap, data, compsize);
            break;
        }

        case MCP_MultiBlockChange: { //34
            Rint(X);
            Rint(Z);
            Rshort(count);
            Rint(dsize);
            Rskip(dsize);
            PRALL printf("Multi Block Change: %d,%d count=%d (%d bytes)\n",X,Z,count,dsize);
            break;
        }

        case MCP_BlockChange: { //35
            Rint(x);
            Rchar(y);
            Rint(z);
            Rshort(bid);
            Rchar(metadata);
            PRALL printf("Block Change: %d,%d,%d block=%d meta=%d\n",
                   x,(unsigned int)y,z,bid,metadata);
            break;
        }

        case MCP_BlockAction: { //36
            Rint(x);
            Rshort(y);
            Rint(z);
            Rchar(b1);
            Rchar(b2);
            Rshort(bid);
            PRALL printf("Block Action: %d,%d,%d bid=%d %d %d\n",x,y,z,bid,b1,b2);
            break;
        }

        case MCP_BlockBreakAnimation: { //37
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(state);
            PRALL printf("Block Break Animation: eid=%d %d,%d,%d state=%d\n",
                   eid,x,y,z,state);
            break;
        }
           
        case MCP_MapChunkBulk: { //38
            Rshort(count);
            Rint(datalen);
            Rchar(skylight);
            uint8_t * data = p;
            Rskip(datalen);

            int i;
            int XX[1024];
            int ZZ[1024];
            uint16_t BM[1024];
            for(i=0; i<count; i++) {
                Rint(X);
                Rint(Z);
                Rshort(PBM);
                Rshort(ABM);
                XX[i] = X;
                ZZ[i] = Z;
                BM[i] = PBM;
            }
            if (atlimit) break;

            printf("Map Chunk Bulk: count=%d %d bytes", count, datalen);
            for(i=0; i<count; i++) 
                printf(" %d,%d,%04x",XX[i],ZZ[i],BM[i]);
            printf("\n");

            //process_chunks(count, XX, ZZ, BM, data, datalen);
            break;
        }
           
        case MCP_Explosion: { //3C
            Rdouble(x);
            Rdouble(y);
            Rdouble(z);
            Rfloat(radius);
            Rint(count);
            Rskip(count*3);
            Rskip(12); // 3x unknown float
            printf("Explosion: %f,%f,%f r=%f count=%d\n",x,y,z,radius,count);
            break;
        }

        case MCP_SoundParticleEffect: { //3D
            Rint(effect);
            Rint(x);
            Rchar(y);
            Rint(z);
            Rint(data);
            Rchar(novol);
            PRALL printf("Sound/Particle Effect: %d,%d,%d effect=%d,%d,%d\n",x,(unsigned int)y,z,effect,data,novol);
            break;
        }

        case MCP_NamedSoundEffect: { //3E
            Rstr(name);
            Rint(x);
            Rint(y);
            Rint(z);
            Rfloat(volume);
            Rchar(pitch);
            PRALL printf("Named Sound Effect: %s %d,%d,%d vol=%.2f pitch=%d\n",
                   name,x,y,z,volume,pitch);
            break;
        }

        case MCP_ChangeGameState: { //46
            Rchar(reason);
            Rchar(mode);
            printf("Change Game State: reason=%d mode=%d\n",reason, mode);
            break;
        }

        case MCP_Thunderbolt: { //47
            Rint(eid);
            Rchar(unk);
            Rint(x);
            Rint(y);
            Rint(z);
            printf("Thunderbolt EID=%d coord=%d,%d,%d\n",eid,x,y,z);
            break;
        }

        case MCP_OpenWindow: {//64
            Rchar(wid);
            Rchar(invtype);
            Rstr(title);
            Rchar(nslots);
            printf("Open Window: wid=%d invtype=%d nslots=%d Title=%s\n",wid,invtype,nslots,title);
            break;
        }

        case MCP_CloseWindow: {//65
            Rchar(wid);
            printf("Close Window: wid=%d\n",wid);
            break;
        }

        case MCP_ClickWindow: {//66
            Rchar(wid);
            Rshort(sid);
            Rchar(rclick);
            Rshort(action);
            Rchar(shift);
            Rslot(s);

            printf("Click Window: wid=%d slot=%d action=[%c %c %d]",wid,sid,rclick?'R':'L',shift?'S':'-',action);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_SetSlot: { //67
            Rchar(wid);
            Rshort(sid);
            Rslot(s);

            printf("Set Slot: wid=%d slot=%d item=",wid,sid);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_SetWindowItems: { //68
            Rchar(wid);
            Rshort(nslots);
            printf("Set Window Items: wid=%d nslots=%d\n",wid,nslots);
            int slot;
            for(slot=0; slot<nslots; slot++) {
                Rslot(s);
                printf("    Slot %2d :",slot);
                if (s.id == -1)
                    printf("-");
                else {
                    printf("%3d x%2d",s.id,s.count);
                    if (s.damage != 0)
                        printf(" dmg=%d",s.damage);
                }
                printf("\n");
            }
            break;
        }

        case MCP_UpdateWindowProp: { //69
            Rchar(wid);
            Rshort(pid);
            Rshort(pvalue);
            printf("Update Window Property: wid=%d prop %d => %d\n",wid,pid,pvalue);
            break;
        }

        case MCP_ConfirmTransaction: { //6A
            Rchar(wid);
            Rshort(action);
            Rchar(accepted);
            printf("Confirm Transaction: wid=%d action=%d accepted=%d\n",wid,action,accepted);
            break;
        }
        
        case MCP_CreativeInvAction: { //6B
            Rshort(sid);
            Rslot(s);

            printf("Creative Inventory Action: slot=%d item=",sid);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }
        
        case MCP_EnchantItem: { //6C
            Rchar(wid);
            Rchar(enchant);
            printf("Enchant Item: wid=%d enchantment=%02x\n",wid,enchant);
            break;
        }
        
        case MCP_UpdateSign: { //82
            Rint(x);
            Rshort(y);
            Rint(z);
            Rstr(line1);
            Rstr(line2);
            Rstr(line3);
            Rstr(line4);
            printf("Update Sign: %d,%d,%d\n",x,y,z);
            printf("%s\n%s\n%s\n%s\n",line1,line2,line3,line4);
            break;
        }

        case MCP_ItemData: { //83
            Rshort(type);
            Rshort(iid);
            Rshort(tlen);
            char text[66000];
            memcpy(text, p, tlen);
            text[tlen] = 0;
            Rskip(tlen);
            PRALL printf("Item Data: type=%d iid=%d >%s<\n",type, iid, text);
            break;
        }

        case MCP_UpdateTileEntity: { //84
            Rint(x);
            Rshort(y);
            Rint(z);
            Rchar(action);
            Rshort(dlen);
            if (dlen > 0)
                Rskip(dlen);
            printf("Update Tile Entry: %d,%d,%d action=%d custom=%d bytes\n",x,y,z,action,dlen);
            break;
        }

        case MCP_IncrementStatistic: { //c8
            Rint(stid);
            Rchar(amount);
            printf("Increment Statistic: id=%d by %d\n",stid,amount);
            break;
        }

        case MCP_PlayerListItem: { //C9
            Rstr(s);
            Rchar(online);
            Rshort(ping);
            printf("Player List Item: %-24s ping: %4dms online:%d\n",s,ping,online);
            break;
        }

        case MCP_PlayerAbilities: { //CA
            Rchar(flags);
            Rchar(fspeed);
            Rchar(wspeed);
            printf("Player Abilities: flags=%02x fspeed=%d wspeed=%d\n",flags,fspeed,wspeed);
            break;
        }

        case MCP_TabComplete: { //CB
            Rstr(str);
            printf("Tab Complete: %s\n",str);
            break;
        }

        case MCP_LocaleAndViewdist: { //CC
            Rstr(locale);
            Rchar(view);
            Rchar(chat);
            Rchar(diff);
            Rchar(cape);
            printf("Locale And View Distance: %s view=%d chat=%02x diff=%d cape=%d\n",locale,view,chat,diff,cape);
            break;
        }

        case MCP_ClientStatuses: { //CD
            Rchar(status);
            printf("Client Statuses: %s\n",status?"respawn":"login");
            break;
        }

        case MCP_PluginMessage: { //FA
            Rstr(channel);
            Rshort(len);
            Rskip(len);
            printf("Plugin Message: channel=%s len=%d\n",channel,len);
            break;
        }

        case MCP_EncryptionKeyResp: { // FC
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
            d2i_RSA_PUBKEY(&mitm.s_rsa, (const unsigned char **)&pp, pklen);
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

        case MCP_ServerListPing: { //FE
            Rchar(magic);
            printf("Server List Ping\n");
            break;
        }

        case MCP_Disconnect: { //FF
            Rstr(s);
            printf("Disconnect: %s\n",s);
            break;
        }

        default:
            printf("Unknown message %02x\n",mtype);
            hexdump(msg_start, *slen-consumed);
            exit(1);
        }

        if (!atlimit) {
            ssize_t msg_len = p-msg_start;

            if (mitm.output) {
                // write packet to the MCS file
                struct timeval tv;
                gettimeofday(&tv, NULL);

                uint8_t header[4096];
                uint8_t *hp = header;
                write_int(hp, is_client);
                write_int(hp, tv.tv_sec);
                write_int(hp, tv.tv_usec);
                write_int(hp, msg_len);
                write_file_f(mitm.output, header, hp-header);
                write_file_f(mitm.output, msg_start, msg_len);
                fflush(mitm.output);
            }

            // the message was ingested successfully
            if (!modified) {
                // the handler only ingested the data but did not modify it
                // we need to copy it 1-1 to the output
                ssize_t wpos = *dlen;
                ARRAY_ADDG(*dbuf,*dlen,msg_len,WRITEBUF_GRAN);
                memcpy(*dbuf+wpos,msg_start,msg_len);
                //printf("Copied %d bytes\n",msg_len);
                //hexdump(msg_start,msg_len);
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
    uint8_t **sbuf, **dbuf, **abuf;
    ssize_t *slen, *dlen, *alen;
    // sbuf - coming from the source, dbuf - forward to destination, abuf - response back to source

    if (is_client) {
        sbuf = &fb->crbuf;
        slen = &fb->crlen;
        dbuf = &fb->swbuf;
        dlen = &fb->swlen;
        abuf = &fb->cwbuf;
        alen = &fb->cwlen;
    }
    else {
        sbuf = &fb->srbuf;
        slen = &fb->srlen;
        dbuf = &fb->cwbuf;
        dlen = &fb->cwlen;
        abuf = &fb->swbuf;
        alen = &fb->swlen;
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

            ssize_t consumed = process_data(is_client, sbuf, slen, dbuf, dlen, abuf, alen);
            if (consumed > 0) {
                if (*slen-consumed > 0)
                    memcpy(*sbuf, *sbuf+consumed, *slen-consumed);
                *slen -= consumed;
            }

            // check if asynchronous events can be processed
            struct timeval tv;
            gettimeofday(&tv, NULL);
            int64_t now = ((int64_t)tv.tv_sec)*1000000+tv.tv_usec;
            if (now-mitm.last[is_client] > ASYNC_THRESHOLD) {
                mitm.last[is_client] = now;
                process_async(is_client, sbuf, slen, dbuf, dlen, abuf, alen);
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

                ebuf = *abuf;
                elen = *alen;
                
                if (elen > 0) {
                    int num=0;
                    if (is_client)
                        AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.c_aes, mitm.c_enc_iv, &num, AES_ENCRYPT);
                    else
                        AES_cfb8_encrypt(ebuf, ebuf, elen, &mitm.s_aes, mitm.s_enc_iv, &num, AES_ENCRYPT);
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

            // send out the contents of the answer buffer
            buf = *abuf;
            len = *alen;
            while (len > 0) {
                //printf("sending %d bytes to %s\n",len,is_client?"server":"client");
                ssize_t sent = write(src,buf,len);
                buf += sent;
                len -= sent;
            }
            *alen = 0;

            return 0;
        }         
        case EVFILE_ERROR:
        case EVFILE_EOF:
            return -1;
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

void write_packet(uint8_t *ptr, ssize_t len, lh_buf_t *buf) {
    uint8_t hbuf[16]; CLEAR(hbuf);
    ssize_t ll = lh_place_varint(hbuf,len) - hbuf;

    ssize_t widx = buf->data_cnt;
    lh_arr_add(AR(buf->data),buf->data_gran,(len+ll));

    memmove(buf->data_ptr+widx, hbuf, ll);
    memmove(buf->data_ptr+widx+ll, ptr, len);
}

uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p);

#define Rchar(n)  Rx(n,uint8_t,char)
#define Rshort(n) Rx(n,uint16_t,short)
#define Rint(n)   Rx(n,uint32_t,int)
#define Rlong(n)  Rx(n,uint64_t,long);
#define Rfloat(n) Rx(n,float,float)
#define Rdouble(n) Rx(n,double,double)
#define Rstr(n)   char n[4096]; p=read_string(p,n)
#define Rskip(n)  p+=n;
#define Rvarint(n) uint32_t n = lh_read_varint(p);

////////////////////////////////////////////////////////////////////////////////

#define CI(x) ((0x10<<24)|(0x##x))
#define SI(x) ((0x00<<24)|(0x##x))
#define CS(x) ((0x11<<24)|(0x##x))
#define SS(x) ((0x01<<24)|(0x##x))
#define CL(x) ((0x12<<24)|(0x##x))
#define SL(x) ((0x02<<24)|(0x##x))
#define CP(x) ((0x13<<24)|(0x##x))
#define SP(x) ((0x03<<24)|(0x##x))

// Handshakes

#define CI_Handshake            CI(00)

// Status Query

#define CS_Request              CS(00)
#define CS_PingRequest          CS(01)
#define SS_Response             SS(00)
#define SS_PingResponse         SS(01)

// Login Process

#define CL_LoginStart           CL(00)
#define CL_EncryptionResponse   CL(01)
#define SL_Disconnect           SL(00)
#define SL_EncryptionRequest    SL(01)
#define SL_LoginSuccess         SL(02)

void process_packet(int is_client, uint8_t *ptr, ssize_t len,
                    lh_buf_t *forw, lh_buf_t *retour) {

    // one nice advantage - we can be sure that we have all data in the buffer,
    // so there's no need for limit checking with the new protocol

    uint8_t *p = ptr;
    uint32_t type = lh_read_varint(p);
    uint32_t stype = ((mitm.state<<24)|(is_client<<28)|(type&0xffffff));

    char *states = "ISLP";

    printf("%c %c type=%02x, len=%zd\n", is_client?'C':'S',
           states[mitm.state],type,len);
    hexdump(ptr, len);

    uint8_t output[65536];
    uint8_t *w = output;

    switch (stype) {
        ////////////////////////////////////////////////////////////////////////
        // Idle state
        case CI_Handshake: {
            LH_HERE;
            Rvarint(protocolVer);
            Rstr(serverAddr);
            Rshort(serverPort);
            Rvarint(nextState);
            LH_HERE;
            mitm.state = nextState;
            printf("C %-30s protocol=%d server=%s:%d nextState=%d\n",
                   "Handshake",protocolVer,serverAddr,serverPort,nextState);
            LH_HERE;
            write_packet(ptr, len, forw);
            LH_HERE;
            break;
        }

        ////////////////////////////////////////////////////////////////////////
#if 0
        case CS_Request: {
            printf("C %-30s\n", "Status Request");
            write_packet(ptr, len, forw);
            break;
        }

        case CS_PingRequest: {
            Rlong(time);
            printf("C %-30s time=%jd\n", "Ping Request",time);
            write_packet(ptr, len, forw);
            break;
        }

        case SS_Response: {
            Rstr(serverStatus);
            printf("S %-30s : %s\n","Status Response",serverStatus);
            write_packet(ptr, len, forw);
            break;
        }

        case SS_PingResponse: {
            Rlong(time);
            printf("S %-30s time=%jd\n","Ping Response",time);
            write_packet(ptr, len, forw);
            break;
        }
#endif

        ////////////////////////////////////////////////////////////////////////    
#if 0
        case MC_EncryptionResponse: {
            if (mitm.pstate != 2) {
                write_packet(ptr, len, forw);
                break;
            }
            
            Rshort(sklen);
            uint8_t *skey = p;
            Rskip(sklen);
            Rshort(tklen);
            uint8_t *token = p;
            Rskip(tklen);
            
            LH_HERE;
            char buf[4096];
            int dklen = RSA_private_decrypt(sklen, skey, buf, mitm.c_rsa, RSA_PKCS1_PADDING);
            if (dklen < 0) {
                    printf("Failed to decrypt the shared key received from the client\n");
                    exit(1);
            }
            printf("Decrypted client shared key, keylen=%d ",dklen);
            hexprint(buf, dklen);
            memcpy(mitm.c_skey, buf, 16);
            
            int dtlen = RSA_private_decrypt(tklen, token, buf, mitm.c_rsa, RSA_PKCS1_PADDING);
            if (dtlen < 0) {
                    printf("Failed to decrypt the verification token received from the client\n");
                    exit(1);
            }
            printf("Decrypted client token, len=%d ",dtlen);
            hexprint(buf, dtlen);
            printf("Original token: ");
            hexprint(mitm.c_token,4);
            if (memcmp(buf, mitm.c_token, 4)) {
                printf("Token does not match!\n");
                    exit(1);
            }
            
            LH_HERE;
            // at this point, the client side is verified and the key is established
            // now send our response to the server
            write_varint(w, MC_EncryptionResponse);

            int eklen = RSA_public_encrypt(sizeof(mitm.s_skey), mitm.s_skey, buf, mitm.s_rsa, RSA_PKCS1_PADDING);
            write_short(w,(short)eklen);
            memcpy(w, buf, eklen);
            w += eklen;
            
            int etlen = RSA_public_encrypt(sizeof(mitm.s_token), mitm.s_token, buf, mitm.s_rsa, RSA_PKCS1_PADDING);
            write_short(w,(short)etlen);
            memcpy(w, buf, etlen);
            w += etlen;
                
            write_packet(output, w-output, forw);
            LH_HERE;
                
#if 0
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
#endif
        }

        case MS_EncryptionRequest: {
            LH_HERE;
            Rstr(serverID);
            
            LH_HERE;
            Rshort(klen);
            memmove(mitm.s_pkey,p,klen);
            Rskip(klen);
            
            LH_HERE;
            Rshort(tlen);
            memmove(mitm.s_token,p,tlen);
            Rskip(tlen);
                
            LH_HERE;
            printf("Encryption Request\n");
                
            sprintf(mitm.s_id,"%s",serverID);
            mitm.s_pklen = klen;
                
            LH_HERE;
            // decode server PUBKEY to an RSA struct
            unsigned char *pp = mitm.s_pkey;
            d2i_RSA_PUBKEY(&mitm.s_rsa, (const unsigned char **)&pp, klen);
            if (mitm.s_rsa == NULL) {
                printf("Failed to decode the server's public key\n");
                exit(1);
            }
            RSA_print_fp(stdout, mitm.s_rsa, 4);

            // generate the server-side shared key pair
            RAND_pseudo_bytes(mitm.s_skey, 16);
            printf("Server-side shared key: ");
            hexprint(mitm.s_skey, 16);

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

            LH_HERE;
            // combine it to a MCP message to the client
            write_varint(w, MS_EncryptionRequest);
            write_varint(w, strlen(serverID));
            memmove(w, serverID, strlen(serverID));
            w+=strlen(serverID);
            write_short(w, mitm.c_pklen);
            memmove(w, mitm.c_pkey, mitm.c_pklen);
            w+=mitm.c_pklen;
            write_short(w, 4);
            memmove(w, mitm.c_token, 4);
            w+=4;

            LH_HERE;
            write_packet(output, w-output, forw);

            LH_HERE;
            break;
        }
#endif



        ////////////////////////////////////////////////////////////////////////
        default: {
            // by default, just forward the packet as is
            write_packet(ptr, len, forw);
        }
    }

}

ssize_t handle_proxy(lh_conn *conn) {
    int is_client = (conn->priv != NULL);

    if (conn->status&CONN_STATUS_REMOTE_EOF) {
        // one of the parties has closed the connection.
        // close both sides, deinit the MITM state and
        // remove our descriptors from the pollarray

        close(mitm.cs);
        close(mitm.ms);
        mitm.state = STATE_IDLE;
        mitm.cs = mitm.ms = -1;
        lh_conn_remove(mitm.cs_conn);
        lh_conn_remove(mitm.ms_conn);

        return 0;
    }

    // determine decoded buffers for input (rx) and output (tx)
    lh_buf_t *rx = is_client ? &mitm.cs_rx : &mitm.ms_rx;
    lh_buf_t *tx = is_client ? &mitm.cs_tx : &mitm.ms_tx;

    assert(conn->rbuf.data_ptr);

    // sptr,slen - pointer and length of data in the receive buffer
    ssize_t slen = conn->rbuf.data_cnt - conn->rbuf.data_ridx;
    uint8_t *sptr = conn->rbuf.data_ptr + conn->rbuf.data_ridx;

    // provide necessary space in the decoded receive buffer
    ssize_t widx = rx->data_cnt;
    lh_arr_add(AR(rx->data),rx->data_gran,slen);

    //TODO: decrypt data into rx
    memmove(rx->data_ptr+widx, sptr, slen);

    // try to extract as many packets from the stream as we can in a loop
    while(rx->data_cnt > 0) {
        //hexdump(AR(rx->data));
        // do we have a complete packet?
        uint8_t *p = rx->data_ptr;

        // large varint, data is definitely too short
        if (((*p)&0x80)&&(rx->data_cnt<129)) break;

        uint32_t plen = lh_read_varint(p);
        ssize_t ll = p-rx->data_ptr; // length of the varint
        if (plen+ll > rx->data_cnt) break; // packet is incomplete

        process_packet(is_client, p, plen, tx, NULL);
        lh_arr_delete_range(AR(rx->data),0,ll+plen);
    }

    //TODO: encrypt

    lh_conn_write(is_client?mitm.ms_conn:mitm.cs_conn, AR(tx->data));
    tx->data_cnt = tx->data_ridx = 0;

    return slen;
}

////////////////////////////////////////////////////////////////////////////////

int signal_caught;

void signal_handler(int signum) {
    printf("Caught signal %d, stopping main loop\n",signum);
    signal_caught = 1;
}

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

void * sslerr(char * str) {
    printf("SSL error: %s\n",str);
    while(ERR_peek_error()) {
        char error[4096];
        printf("  %s\n", ERR_error_string(ERR_get_error(), error));
    }
    return NULL;
}

SSL_CTX * create_ssl_context() {
    // init library
    SSL_library_init();
    OpenSSL_add_all_algorithms();       // load & register cryptos
    SSL_load_error_strings();           // load all error messages

    // init context
    SSL_METHOD *method = SSLv23_server_method();    // create server instance
    if (!method)
        return sslerr("Failed to create SSLv23 server method");

    SSL_CTX *ctx = SSL_CTX_new(method);             // create context
    if (!ctx)
        return sslerr("Failed to create SSL context");
    
    // load server certificate
    // set the local certificate from CertFile
    if (!SSL_CTX_use_certificate_file(ctx, "server.pem", SSL_FILETYPE_PEM))
        return sslerr("Failed to load the server certificate");
    // set the private key from KeyFile
    if (!SSL_CTX_use_PrivateKey_file(ctx, "server.pem", SSL_FILETYPE_PEM))
        return sslerr("Failed to load the server private key");
    // verify private key
    if ( !SSL_CTX_check_private_key(ctx) )
        return sslerr("Checking the server private key failed");
    return ctx;
}

int handle_session_server(int sfd, SSL_CTX *ctx) {
    // accept connection from the client side
    struct sockaddr_in cadr;
    int cs = lh_accept_tcp4(sfd, &cadr);
    lh_net_blocking(cs);
    if (cs < 0)
        LH_ERROR(0, "Failed to accept the client-side connection",strerror(errno));
    printf("Accepted from %s:%d (Webserver)\n",
           inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));

    // establish the SSL context on this conenction
    SSL *ssl = SSL_new(ctx);        // get new SSL state with context
    if (!ssl) return (int) sslerr("Failed to create an SSL state");

    if (!SSL_set_fd(ssl, cs))       // assign connection to the SSL state
        return (int) sslerr("Failed to set the fd");

    if (!SSL_accept(ssl))           // start the handshake process
        return (int) sslerr("Handshaking failed");

    // handle client-side request
    char buf[262144];
    int pos = 0;
    int bytes0 = 10;

    do {
        int bytes = SSL_read(ssl, buf+pos, sizeof(buf)-pos-1);
        if (bytes == 0) {
            if (--bytes0 > 0)
                printf("read %d bytes\n",bytes);
        }
        else
            printf("read %d bytes\n",bytes);

        pos += bytes;
        buf[pos] = 0;
    } while(!strstr(buf,"\r\n\r\n") && !signal_caught);

    char *post = strstr(buf,"\r\n\r\n")+4;
    hexdump(post,256);
    while(!strstr(post,"}") && !signal_caught) {
        int bytes = SSL_read(ssl, buf+pos, sizeof(buf)-pos-1);
        if (bytes > 0) printf("(2) read %d bytes\n",bytes);
        pos += bytes;
        buf[pos] = 0;
    }

    //TODO: parse json

    printf("Received a request:\n");
    hexdump(buf, pos);
    lh_save("mcrequest", buf, pos);
    
    SSL_free(ssl);
    close(cs);

    return 1;
}



////////////////////////////////////////////////////////////////////////////////

int handle_server(int sfd, uint32_t ip, uint16_t port) {
    LH_HERE;
    // accept connection from the local client
    struct sockaddr_in cadr;
    int cs = lh_accept_tcp4(sfd, &cadr);
    if (cs < 0)
        LH_ERROR(0, "Failed to accept the client-side connection");

    printf("Accepted from %s:%d\n",
           inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));
    
    // open connection to the remote server
    int ms = lh_connect_tcp4(ip, port);
    if (ms < 0) {
        close(mitm.cs);
        LH_ERROR(0, "Failed to open the client-side connection");
    }

    // TCP connections established, 
    printf("New connection: cs=%d ms=%d\n", cs, ms);
    
    // initialize mitm struct, terminate old state if any
    if (mitm.output) fclose(mitm.output);
    if (mitm.s_rsa) RSA_free(mitm.s_rsa);
    if (mitm.c_rsa) RSA_free(mitm.c_rsa);
    CLEAR(mitm);

    mitm.cs_rx.data_gran = mitm.cs_tx.data_gran = 65536;
    mitm.ms_rx.data_gran = mitm.ms_tx.data_gran = 65536;

    char fname[4096];
    time_t t;
    time(&t);
    strftime(fname, sizeof(fname), "saved/%Y%m%d_%H%M%S.mcs",localtime(&t));
    mitm.output = fopen(fname, "w");

    // handle_server was able to accept the client connection and
    // also open the server-side connection, we need to add these
    // new sockets to the groups cg and mg respectively
    mitm.cs = cs;
    mitm.ms = ms;
    mitm.cs_conn = lh_conn_add(&pa, cs, G_PROXY, (void*)1);
    mitm.ms_conn = lh_conn_add(&pa, ms, G_PROXY, (void*)0);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

int proxy_pump(uint32_t ip, uint16_t port) {
    CLEAR(pa);

    SSL_CTX * ctx = create_ssl_context();
    if (!ctx) return -1;

    CLEAR(mitm);
    mitm.cs = mitm.ms = -1;

    // Minecraft proxy server
    int ss = lh_listen_tcp4_any(port);
    if (ss<0) return -1;
    lh_poll_add(&pa, ss, POLLIN, G_MCSERVER, NULL);

    // fake session.minecraft.com web server
    int ws = lh_listen_tcp4_any(8880);
    if (ws<0) return -1;
    lh_poll_add(&pa, ws, POLLIN, G_WEBSERVER, NULL);

    // prepare signal handling
    signal_caught = 0;
    struct sigaction sa;
    CLEAR(sa);
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL))
        LH_ERROR(1,"Failed to set sigaction\n");

    // main pump
    int i;
    while(!signal_caught) {
        lh_poll(&pa, 1000);

        lh_polldata *pd;
        int pos;

        // handle connection requests on the web server
        if ( pd=lh_poll_getfirst(&pa, G_WEBSERVER, POLLIN))
            handle_session_server(pd->fd, ctx);

        if ( pd=lh_poll_getfirst(&pa, G_MCSERVER, POLLIN))
            handle_server(pd->fd, ip, port);

        // handle client- and server-side connection
        lh_conn_process(&pa, G_PROXY, handle_proxy);
    }

    printf("Terminating...\n");
    if (mitm.output) {
        fflush(mitm.output);
        fclose(mitm.output);
        mitm.output = NULL;
    }
}

int main(int ac, char **av) {
    proxy_pump(SERVER_IP, SERVER_PORT);

    return 0;
}
