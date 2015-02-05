#include <string.h>
#include <assert.h>

#define LH_DECLARE_SHORT_NAMES 1
#include <lh_buffers.h>
#include <lh_bytes.h>
#include <lh_debug.h>

#include "mcp_packet.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////

static uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

static uint8_t * write_string(uint8_t *w, const char *s) {
    uint32_t len = (uint32_t)strlen(s);
    lh_write_varint(w, len);
    memmove(w, s, len);
    w+=len;
    return w;
}

#if 0
static uint8_t * read_slot(uint8_t *p, slot_t *s) {
    lh_clear_ptr(s);

    s->id     = lh_read_short_be(p);
    if (s->id != 0xffff) {
        s->count  = lh_read_char(p);
        s->damage = lh_read_short_be(p);
        s->dlen   = lh_read_short_be(p);
        if (s->dlen!=0 && s->dlen!=0xffff) {
            memcpy(s->data, p, s->dlen);
            p += s->dlen;
        }
    }
    else {
        s->count = 0;
        s->damage= 0;
        s->dlen  = 0;
    }
    return p;
}
#endif
    

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p)

#define Rchar(n)    Rx(n,uint8_t,char)
#define Rshort(n)   Rx(n,uint16_t,short)
#define Rint(n)     Rx(n,uint32_t,int)
#define Rlong(n)    Rx(n,uint64_t,long)
#define Rfloat(n)   Rx(n,float,float)
#define Rdouble(n)  Rx(n,double,double)
#define Rstr(n)     char n[4096]; p=read_string(p,n)
#define Rskip(n)    p+=n;
#define Rvarint(n)  uint32_t n = lh_read_varint(p)
//#define Rslot(n)    slot_t n; p=read_slot(p,&n)



#define Px(n,fun)   tpkt->n = lh_read_ ## fun ## _be(p)

#define Pchar(n)    Px(n,char)
#define Pshort(n)   Px(n,short)
#define Pint(n)     Px(n,int)
#define Plong(n)    Px(n,long)
#define Pfloat(n)   Px(n,float)
#define Pdouble(n)  Px(n,double)
#define Pstr(n)     p=read_string(p,tpkt->n)
#define Pvarint(n)  tpkt->n = lh_read_varint(p)
//#define Pslot(n)    p=read_slot(p,tpkt->n)
#define Pdata(n,l)  memmove(tpkt->n,p,l); p+=l
#define Puuid(n)    Pdata(n,sizeof(uuid_t))



#define Wx(n,fun)   lh_write_ ## fun ## _be(w, tpkt->n)

#define Wchar(n)    Wx(n,char)
#define Wshort(n)   Wx(n,short)
#define Wint(n)     Wx(n,int)
#define Wlong(n)    Wx(n,long)
#define Wfloat(n)   Wx(n,float)
#define Wdouble(n)  Wx(n,double)
#define Wstr(n)     w=write_string(w, tpkt->n)
#define Wvarint(n)  lh_write_varint(w, tpkt->n)
//#define Pslot(n)    p=read_slot(p,tpkt->n)
#define Wdata(n,l)  memmove(w,tpkt->n,l); w+=l
#define Wuuid(n)    Wdata(n,sizeof(uuid_t))

////////////////////////////////////////////////////////////////////////////////

char limhexbuf[4100];
static const char * limhex(uint8_t *data, ssize_t len, ssize_t maxbyte) {
    //assert(len<(sizeof(limhexbuf)-4)/2);
    assert(maxbyte >= 4);

    int i;
    //TODO: implement aaaaaa....bbbbbb - type of printing
    if (len > maxbyte) len = maxbyte;
    for(i=0;i<len;i++)
        sprintf(limhexbuf+i*2,"%02x ",data[i]);
    return limhexbuf;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    void    (*decode_method)(MCPacket *);
    ssize_t (*encode_method)(MCPacket *, uint8_t *buf);
    void    (*dump_method)(MCPacket *);
    void    (*free_method)(MCPacket *);
    const char * dump_name;
} packet_methods;

#define DECODE_BEGIN(name,version)                                  \
    void decode_##name##version(MCPacket *pkt) {                    \
        name##_pkt * tpkt = &pkt->_##name;                          \
        assert(pkt->raw);                                           \
        uint8_t *p = pkt->raw;                                      \
        pkt->ver = PROTO##version;

#define DECODE_END                              \
    }

#define ENCODE_BEGIN(name,version)                                  \
    ssize_t encode_##name##version(MCPacket *pkt, uint8_t *buf) {   \
        name##_pkt * tpkt = &pkt->_##name;                          \
        uint8_t *w = buf;

#define ENCODE_END                              \
        return w-buf;                           \
    }

#define DUMP_BEGIN(name)                        \
    void dump_##name(MCPacket *pkt) {           \
        name##_pkt * tpkt = &pkt->_##name;      \

#define DUMP_END }

#define FREE_BEGIN(name)                        \
    void free_##name(MCPacket *pkt) {           \
        name##_pkt * tpkt = &pkt->_##name;      \

#define FREE_END }


////////////////////////////////////////////////////////////////////////////////
// macros to fill the SUPPORT table

// decode, encode, dump and free
#define SUPPORT_DEDF(name,version)              \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        encode_##name##version,                 \
        dump_##name,                            \
        free_##name,                            \
        #name                                   \
    }

// decode, dump and free
#define SUPPORT_DDF(name,version)               \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        NULL,                                   \
        dump_##name,                            \
        free_##name,                            \
        #name                                   \
    }

// decode, encode and free
#define SUPPORT_DEF(name,version)               \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        encode_##name##version,                 \
        NULL,                                   \
        free_##name,                            \
        #name                                   \
    }

// decode and free
#define SUPPORT_DF(name,version)                \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        NULL,                                   \
        NULL,                                   \
        free_##name,                            \
        #name                                   \
    }

// decode, encode and dump
#define SUPPORT_DED(name,version)               \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        encode_##name##version,                 \
        dump_##name,                            \
        NULL,                                   \
        #name                                   \
    }

// decode and dump
#define SUPPORT_DD(name,version)                \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        NULL,                                   \
        dump_##name,                            \
        NULL,                                   \
        #name                                   \
    }

// decode and encode
#define SUPPORT_DE(name,version)                \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        encode_##name##version,                 \
        NULL,                                   \
        NULL,                                   \
        #name                                   \
    }

// decode only
#define SUPPORT_D(name,version)                 \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        NULL,                                   \
        NULL,                                   \
        NULL,                                   \
        #name                                   \
    }

////////////////////////////////////////////////////////////////////////////////
// 0x01 SP_JoinGame

DECODE_BEGIN(SP_JoinGame,_1_8_1) {
    Pint(eid);
    Pchar(gamemode);
    Pchar(dimension);
    Pchar(difficulty);
    Pchar(maxplayers);
    Pstr(leveltype);
    Pchar(reduced_debug_info);
} DECODE_END;

DUMP_BEGIN(SP_JoinGame) {
    const char *GM[]   = { "Survival", "Creative", "Adventure", "Spectator" };
    const char *DIM[]  = { "Overworld", "End", "Unknown", "Nether" };
    const char *DIFF[] = { "Peaceful", "Easy", "Normal", "Hard" };

    printf("eid=%08x, gamemode=%s%s, dimension=%s, difficulty=%s, "
           "maxplayers=%d, leveltype=%s, reduced_debug_info=%c",
           tpkt->eid, GM[tpkt->gamemode&3], (tpkt->gamemode&8)?"(hardcore)":"",
           DIM[tpkt->dimension&3], DIFF[tpkt->difficulty&3],
           tpkt->maxplayers, tpkt->leveltype, tpkt->reduced_debug_info?'T':'F');
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x02 SP_ChatMessage

DECODE_BEGIN(SP_ChatMessage,_1_8_1) {
    Pstr(json);
    Pchar(pos);
} DECODE_END;

ENCODE_BEGIN(SP_ChatMessage,_1_8_1) {
    Wstr(json);
    Wchar(pos);
} ENCODE_END;

DUMP_BEGIN(SP_ChatMessage) {
    printf("json=%s",tpkt->json);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x03 SP_TimeUpdate

DECODE_BEGIN(SP_TimeUpdate,_1_8_1) {
    Plong(worldage);
    Plong(time);
} DECODE_END;

DUMP_BEGIN(SP_TimeUpdate) {
    printf("worldage=%jd ticks (%jdd %02jdh%02jdm), "
           "time=%jd ticks (%jdd %02jd:%02jd)",
           tpkt->worldage,tpkt->worldage/24000,(tpkt->worldage%24000)/1000,(tpkt->worldage%1000)/60,
           tpkt->time, tpkt->time/24000, (tpkt->time%24000)/1000, (tpkt->time%1000)/60);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x08 SP_PlayerPositionLook

DECODE_BEGIN(SP_PlayerPositionLook,_1_8_1) {
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pfloat(yaw);
    Pfloat(pitch);
    Pchar(flags);
} DECODE_END;

ENCODE_BEGIN(SP_PlayerPositionLook,_1_8_1) {
    Wdouble(x);
    Wdouble(y);
    Wdouble(z);
    Wfloat(yaw);
    Wfloat(pitch);
    Wchar(flags);
} ENCODE_END;

DUMP_BEGIN(SP_PlayerPositionLook) {
    printf("coord=%.1f,%.1f,%.1f rot=%.1f,%.1f flags=%02x",
           tpkt->x,tpkt->y,tpkt->z,tpkt->yaw,tpkt->pitch,tpkt->flags);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0c SP_SpawnPlayer

DECODE_BEGIN(SP_SpawnPlayer,_1_8_1) {
    Pvarint(eid);
    Puuid(uuid);
    Pint(x);
    Pint(y);
    Pint(z);
    Pchar(yaw);
    Pchar(pitch);
    Pshort(item);
    //TODO: Metadata
} DECODE_END;

DUMP_BEGIN(SP_SpawnPlayer) {
    printf("eid=%08x, uuid=%s, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, item=%d",
           tpkt->eid, limhex(tpkt->uuid,16,16),
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->item);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0f SP_SpawnMob

DECODE_BEGIN(SP_SpawnMob,_1_8_1) {
    Pvarint(eid);
    Pchar(mobtype);
    Pint(x);
    Pint(y);
    Pint(z);
    Pchar(yaw);
    Pchar(pitch);
    Pchar(headpitch);
    Pshort(vx);
    Pshort(vy);
    Pshort(vz);
    //TODO: Metadata
} DECODE_END;

DUMP_BEGIN(SP_SpawnMob) {
    printf("eid=%08x, mobtype=%d, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f,%.1f, vel=%d,%d,%d",
           tpkt->eid, tpkt->mobtype,
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,(float)tpkt->headpitch/256,
           tpkt->vx,tpkt->vy,tpkt->vz);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x13 SP_DestroyEntities

DECODE_BEGIN(SP_DestroyEntities,_1_8_1) {
    Pvarint(count);
    lh_alloc_num(tpkt->eids,tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        Pvarint(eids[i]);
    }
} DECODE_END;

DUMP_BEGIN(SP_DestroyEntities) {
    printf("count=%d eids=[",tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        printf("%08x%s",tpkt->eids[i],(i==tpkt->count-1)?"]":",");
    }
} DUMP_END;

FREE_BEGIN(SP_DestroyEntities) {
    lh_free(tpkt->eids);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x14 SP_Entity

DECODE_BEGIN(SP_Entity,_1_8_1) {
    Rvarint(eid);
} DECODE_END;

DUMP_BEGIN(SP_Entity) {
    printf("eid=%08x",tpkt->eid);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x15 SP_EntityRelMove

DECODE_BEGIN(SP_EntityRelMove,_1_8_1) {
    Pvarint(eid);
    Pchar(dx);
    Pchar(dy);
    Pchar(dz);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityRelMove) {
    printf("eid=%08x, delta=%.1f,%.1f,%.1f, onground=%d",tpkt->eid,
           (float)tpkt->dx/32,(float)tpkt->dy/32,(float)tpkt->dz/32,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x16 SP_EntityLook

DECODE_BEGIN(SP_EntityLook,_1_8_1) {
    Pvarint(eid);
    Pchar(yaw);
    Pchar(pitch);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityLook) {
    printf("eid=%08x, rot=%.1f,%.1f, onground=%d",tpkt->eid,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x17 SP_EntityLookRelMove

DECODE_BEGIN(SP_EntityLookRelMove,_1_8_1) {
    Pvarint(eid);
    Pchar(dx);
    Pchar(dy);
    Pchar(dz);
    Pchar(yaw);
    Pchar(pitch);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityLookRelMove) {
    printf("eid=%08x, delta=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",tpkt->eid,
           (float)tpkt->dx/32,(float)tpkt->dy/32,(float)tpkt->dz/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x18 SP_EntityTeleport

DECODE_BEGIN(SP_EntityTeleport,_1_8_1) {
    Pvarint(eid);
    Pchar(x);
    Pchar(y);
    Pchar(z);
    Pchar(yaw);
    Pchar(pitch);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityTeleport) {
    printf("eid=%08x, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",tpkt->eid,
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x46 SP_SetCompression

DECODE_BEGIN(SP_SetCompression,_1_8_1) {
    Pvarint(threshold);
} DECODE_END;

ENCODE_BEGIN(SP_SetCompression,_1_8_1) {
    Wvarint(threshold);
} ENCODE_END;

DUMP_BEGIN(SP_SetCompression) {
    printf("threshold=%d",tpkt->threshold);
} DUMP_END;







////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Client -> Server


////////////////////////////////////////////////////////////////////////////////
// 0x01 CP_ChatMessage

DECODE_BEGIN(CP_ChatMessage,_1_8_1) {
    Pstr(str);
} DECODE_END;

DUMP_BEGIN(CP_ChatMessage) {
    printf("str=%s",tpkt->str);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0a CP_Animation

DECODE_BEGIN(CP_Animation,_1_8_1) {
} DECODE_END;

ENCODE_BEGIN(CP_Animation,_1_8_1) {
} ENCODE_END;

DUMP_BEGIN(CP_Animation) {
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////

const static packet_methods SUPPORT_1_8_1[2][MAXPACKETTYPES] = {
    {
        SUPPORT_DD  (SP_JoinGame,_1_8_1),
        SUPPORT_DED (SP_ChatMessage,_1_8_1),
        SUPPORT_D   (SP_TimeUpdate,_1_8_1),
        SUPPORT_DED (SP_PlayerPositionLook,_1_8_1),
        SUPPORT_DD  (SP_SpawnPlayer,_1_8_1),
        SUPPORT_DD  (SP_SpawnMob,_1_8_1),
        SUPPORT_DDF (SP_DestroyEntities,_1_8_1),
        SUPPORT_DD  (SP_Entity,_1_8_1),
        SUPPORT_D   (SP_EntityRelMove,_1_8_1),
        SUPPORT_D   (SP_EntityLook,_1_8_1),
        SUPPORT_D   (SP_EntityLookRelMove,_1_8_1),
        SUPPORT_D   (SP_EntityTeleport,_1_8_1),
        SUPPORT_DED (SP_SetCompression,_1_8_1),
    },
    {
        SUPPORT_DD  (CP_ChatMessage,_1_8_1),
        SUPPORT_DE  (CP_Animation,_1_8_1),
    },
};

////////////////////////////////////////////////////////////////////////////////

#define SUPPORT SUPPORT_1_8_1

MCPacket * decode_packet(int is_client, uint8_t *data, ssize_t len) {

    uint8_t * p = data;
    Rvarint(type);              // type field

    lh_create_obj(MCPacket, pkt);

    // fill in basic data
    pkt->type = type;
    pkt->cl   = is_client;
    pkt->mode = STATE_PLAY;
    pkt->ver  = PROTO_NONE;

    // make a raw data copy
    pkt->rawlen = data+len-p;
    pkt->raw = malloc(pkt->rawlen);
    memmove(pkt->raw, p, pkt->rawlen);

    // decode packet if supported
    if (SUPPORT[pkt->cl][pkt->type].decode_method) {
        SUPPORT[pkt->cl][pkt->type].decode_method(pkt);
    }

    return pkt;
}

//FIXME: for now we assume static buffer allocation and sufficient buffer size
//FIXME: we should convert this to lh_buf_t or a resizeable buffer later
ssize_t encode_packet(MCPacket *pkt, uint8_t *buf) {
    uint8_t * p = buf;

    // write packet type
    lh_write_varint(p, pkt->type);
    ssize_t ll = p-buf;

    if (!pkt->modified && pkt->raw) {
        memmove(p, pkt->raw, pkt->rawlen);
        return ll+pkt->rawlen;
    }
    else if ( SUPPORT[pkt->cl][pkt->type].encode_method ) {
        return ll+SUPPORT[pkt->cl][pkt->type].encode_method(pkt, p);
    }
    else {
        assert(0);
    }
}

////////////////////////////////////////////////////////////////////////////////

void dump_packet(MCPacket *pkt) {
    char *states="ISLP";

    if (SUPPORT[pkt->cl][pkt->type].dump_method) {
        printf("%c %c %2x ",pkt->cl?'C':'S',states[pkt->mode],pkt->type);
        printf("%-24s    ",SUPPORT[pkt->cl][pkt->type].dump_name);
        SUPPORT[pkt->cl][pkt->type].dump_method(pkt);
        printf("\n");
    }
    else if (pkt->raw) {
#if 0
        printf("%c %c %2x ",pkt->cl?'C':'S',states[pkt->mode],pkt->type);
        printf("%-24s    len=%6zd, raw=%s","Raw",pkt->rawlen,limhex(pkt->raw,pkt->rawlen,64));
        printf("\n");
#endif
    }
    else {
        //printf("(unknown)");
    }

}

////////////////////////////////////////////////////////////////////////////////
 
void free_packet(MCPacket *pkt) {
    lh_free(pkt->raw);

    if (SUPPORT[pkt->cl][pkt->type].free_method)
        SUPPORT[pkt->cl][pkt->type].free_method(pkt);

    free(pkt);
}

void queue_packet (MCPacket *pkt, MCPacketQueue *q) {
    *lh_arr_new(GAR(q->queue)) = pkt;
}
