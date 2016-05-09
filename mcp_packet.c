/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <string.h>
#include <assert.h>

#define LH_DECLARE_SHORT_NAMES 1
#include <lh_buffers.h>
#include <lh_bytes.h>
#include <lh_debug.h>
#include <lh_arr.h>

#include "mcp_packet.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////
// Helpers

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

static inline int count_bits(uint16_t bitmask) {
    int c=0;
    for(c=0; bitmask; bitmask>>=1)
        c += (bitmask&1);
    return c;
}

////////////////////////////////////////////////////////////////////////////////
// String

/*
  MCP string format:
  varint length
  char[length] string, no terminator
*/

static uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    assert(len<MCP_MAXSTR);
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

int decode_chat_json(const char *json, char *name, char *message) {
    if (strncmp(json, "{\"extra\":[\"\\u003c",17)) return 0;

    const char *nptr = json+17;
    const char *tptr = index(nptr, '\\');
    if (tptr) {
        strncpy(name, nptr, tptr-nptr);
        name[tptr-nptr]=0;

        const char * mptr = tptr+7;
        tptr = index(mptr, '"');
        if (tptr) {
            strncpy(message, mptr, tptr-mptr);
            message[tptr-mptr]=0;
            return 1;
        }
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

static uint8_t * read_slot(uint8_t *p, slot_t *s);

static uint8_t * read_metadata(uint8_t *p, metadata **meta) {
    assert(meta);
    assert(!*meta);
    ssize_t mc = 0;

    // allocate a whole set of 32 values
    lh_alloc_num(*meta, 32);
    metadata *m = *meta;

    int i;

    // mark all entries as not present - we use the same 0x7f value
    // that Mojang uses as terminator
    for(i=0; i<32; i++) m[i].h = 0x7f;

    while (1) {
        uint8_t kv = read_char(p);
        if (kv == 0x7f) break; // terminator

        uint8_t k = kv&31;
        uint8_t type = (kv>>5)&7;
        metadata *mm = &m[k];
        mm->h = kv;

        switch (mm->type) {
            case META_BYTE:   mm->b = read_char(p);    break;
            case META_SHORT:  mm->s = read_short(p);   break;
            case META_INT:    mm->i = read_int(p);     break;
            case META_FLOAT:  mm->f = read_float(p);   break;
            case META_STRING: p = read_string(p,mm->str); break;
            case META_SLOT:   p = read_slot(p,&mm->slot); break;
            case META_COORD:
                mm->x = read_int(p);
                mm->y = read_int(p);
                mm->z = read_int(p);
                break;
            case META_ROT:
                mm->pitch = read_float(p);
                mm->yaw   = read_float(p);
                mm->roll  = read_float(p);
                break;
        }
    }

    return p;
}

static void dump_metadata(metadata *meta, EntityType et) {
    if (!meta) return;

    int i;
    for (i=0; i<32; i++) {
        metadata *mm = meta+i;
        if (mm->h==0x7f) continue;

        printf("\n    ");

        const char * name = NULL;
        EntityType ett = et;
        while ((!name) && (ett!=IllegalEntityType)) {
            name = METANAME[ett][mm->key];
            ett = ENTITY_HIERARCHY[ett];
        }

        printf("%2d %-24s [%-6s] = ", mm->key, name?name:"Unknown",METATYPES[mm->type]);
        switch (mm->type) {
            case META_BYTE:   printf("%d",  mm->b);   break;
            case META_SHORT:  printf("%d",  mm->s);   break;
            case META_INT:    printf("%d",  mm->i);   break;
            case META_FLOAT:  printf("%.1f",mm->f);   break;
            case META_STRING: printf("\"%s\"", mm->str); break;
            case META_SLOT:   dump_slot(&mm->slot); break;
            case META_COORD:
                printf("(%d,%d,%d)",mm->x,mm->y,mm->z); break;
            case META_ROT:
                printf("%.1f,%.1f,%.1f",mm->pitch,mm->yaw,mm->roll); break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Map Chunk

// print block data slice-wise in hex and ANSI
void blkdump(const bid_t * data, int count) {
    int r,c;
    for(r=0; r<count; r+=16) {
        printf("%p %4d (%3d:%2d)   ", data+r, r>>4, r>>8, (r>>4)&15);

        for(c=0; c<16; c++)
            printf("%3x ",data[r+c].bid);

        printf("   ");

        for(c=0; c<16; c++)
            printf("%s",(data[r+c].bid<256) ? ANSI_BLOCK[data[r+c].bid] : ANSI_ILLBLOCK );

        printf("\n%s",((r&0xff) == 0xf0)?"\n":"");
    }
}

uint8_t * read_chunk(uint8_t *p, int8_t skylight, chunk_t *chunk) {
    int i;

    // block data
    uint16_t tmask = chunk->mask;
    for (i=0; tmask; tmask>>=1, i++) {
        if (tmask&1) {
            lh_alloc_obj(chunk->cubes[i]);
            memmove(chunk->cubes[i]->blocks, p, 8192);
            p+=8192;
        }
    }

    // light data
    tmask = chunk->mask;
    for (i=0; tmask; tmask>>=1, i++) {
        if (tmask&1) {
            memmove(chunk->cubes[i]->light, p, 2048);
            p+=2048;
        }
    }

    // skylight data (if available)
    if (skylight) {
        tmask = chunk->mask;
        for (i=0; tmask; tmask>>=1, i++) {
            if (tmask&1) {
                memmove(chunk->cubes[i]->skylight, p, 2048);
                p+=2048;
            }
        }
    }

    // biome data (only once per chunk)
    memmove(chunk->biome, p, 256);
    p+=256;

    return p;
}

uint8_t * write_chunk(uint8_t *w, int8_t skylight, chunk_t *chunk) {
    int i;

    // block data
    uint16_t tmask = chunk->mask;
    for (i=0; tmask; tmask>>=1, i++) {
        if (tmask&1) {
            memmove(w, chunk->cubes[i]->blocks, 8192);
            w+=8192;
        }
    }

    // light data
    tmask = chunk->mask;
    for (i=0; tmask; tmask>>=1, i++) {
        if (tmask&1) {
            memmove(w, chunk->cubes[i]->light, 2048);
            w+=2048;
        }
    }

    // skylight data (if available)
    if (skylight) {
        tmask = chunk->mask;
        for (i=0; tmask; tmask>>=1, i++) {
            if (tmask&1) {
                memmove(w, chunk->cubes[i]->skylight, 2048);
                w+=2048;
            }
        }
    }

    // biome data (only once per chunk)
    memmove(w, chunk->biome, 256);
    w+=256;

    return w;
}

////////////////////////////////////////////////////////////////////////////////
// Inventory Slot

static uint8_t * read_slot(uint8_t *p, slot_t *s) {
    s->item = lh_read_short_be(p);

    if (s->item != -1) {
        s->count  = lh_read_char(p);
        s->damage = lh_read_short_be(p);
        s->nbt    = nbt_parse(&p);
    }
    else {
        s->count = 0;
        s->damage= 0;
        s->nbt = NULL;
    }
    return p;
}

static uint8_t * write_slot(uint8_t *w, slot_t *s) {
    lh_write_short_be(w, s->item);
    if (s->item == -1) return w;

    lh_write_char(w, s->count);
    lh_write_short_be(w, s->damage);

    nbt_write(&w, s->nbt);

    return w;
}

////////////////////////////////////////////////////////////////////////////////

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p)

#define Rchar(n)    Rx(n,uint8_t,char)
#define Rshort(n)   Rx(n,uint16_t,short)
#define Rint(n)     Rx(n,uint32_t,int)
#define Rlong(n)    Rx(n,uint64_t,long)
#define Rfloat(n)   Rx(n,float,float)
#define Rdouble(n)  Rx(n,double,double)
#define Rstr(n)     char n[65536]; p=read_string(p,n)
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
//#define Pslot(n)    p=read_slot(p,&tpkt->n)
#define Pdata(n,l)  memmove(tpkt->n,p,l); p+=l
#define Puuid(n)    Pdata(n,sizeof(uuid_t))
#define Pmeta(n)    p=read_metadata(p, &tpkt->n)



#define Wx(n,fun)   lh_write_ ## fun ## _be(w, tpkt->n)

#define Wchar(n)    Wx(n,char)
#define Wshort(n)   Wx(n,short)
#define Wint(n)     Wx(n,int)
#define Wlong(n)    Wx(n,long)
#define Wfloat(n)   Wx(n,float)
#define Wdouble(n)  Wx(n,double)
#define Wstr(n)     w=write_string(w, tpkt->n)
#define Wvarint(n)  lh_write_varint(w, tpkt->n)
//#define Wslot(n)    p=read_slot(p,&tpkt->n)
#define Wdata(n,l)  memmove(w,tpkt->n,l); w+=l
#define Wuuid(n)    Wdata(n,sizeof(uuid_t))

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
// 0x00 SP_KeepAlive

DECODE_BEGIN(SP_KeepAlive,_1_8_1) {
    Pvarint(id);
} DECODE_END;

DUMP_BEGIN(SP_KeepAlive) {
    printf("id=%08x",tpkt->id);
} DUMP_END;

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
    Rstr(json);
    tpkt->json = strdup(json);
    Pchar(pos);
} DECODE_END;

ENCODE_BEGIN(SP_ChatMessage,_1_8_1) {
    Wstr(json);
    Wchar(pos);
} ENCODE_END;

DUMP_BEGIN(SP_ChatMessage) {
    printf("json=%s",tpkt->json);

    char name[256], message[256];
    if (decode_chat_json(tpkt->json, name, message)) {
        printf(" name=%s message=\"%s\"",name,message);
    }
} DUMP_END;

FREE_BEGIN(SP_ChatMessage) {
    lh_free(tpkt->json);
} FREE_END;

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
// 0x04 SP_EntityEquipment

DECODE_BEGIN(SP_EntityEquipment,_1_8_1) {
    Pvarint(eid);
    Pshort(sid);
    p = read_slot(p, &tpkt->item);
} DECODE_END;

DUMP_BEGIN(SP_EntityEquipment) {
    printf("eid=%08x, sid=%d, item:", tpkt->eid, tpkt->sid);
    dump_slot(&tpkt->item);
} DUMP_END;

FREE_BEGIN(SP_EntityEquipment) {
    clear_slot(&tpkt->item);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x06 SP_UpdateHealth

DECODE_BEGIN(SP_UpdateHealth,_1_8_1) {
    Pfloat(health);
    Pvarint(food);
    Pfloat(saturation);
} DECODE_END;

DUMP_BEGIN(SP_UpdateHealth) {
    printf("health=%.1f, food=%d, saturation=%.1f",
           tpkt->health, tpkt->food, tpkt->saturation);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x07 SP_Respawn

DECODE_BEGIN(SP_Respawn,_1_8_1) {
    Pint(dimension);
    Pchar(difficulty);
    Pchar(gamemode);
    Pstr(leveltype);
} DECODE_END;

DUMP_BEGIN(SP_Respawn) {
    const char *GM[]   = { "Survival", "Creative", "Adventure", "Spectator" };
    const char *DIM[]  = { "Overworld", "End", "Unknown", "Nether" };
    const char *DIFF[] = { "Peaceful", "Easy", "Normal", "Hard" };

    printf("gamemode=%s%s, dimension=%s, difficulty=%s, leveltype=%s",
           GM[tpkt->gamemode&3], (tpkt->gamemode&8)?"(hardcore)":"",
           DIM[tpkt->dimension&3], DIFF[tpkt->difficulty&3],
           tpkt->leveltype);
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
// 0x09 SP_HeldItemChange

DECODE_BEGIN(SP_HeldItemChange,_1_8_1) {
    Pchar(sid);
} DECODE_END;

ENCODE_BEGIN(SP_HeldItemChange,_1_8_1) {
    Wchar(sid);
} ENCODE_END;

DUMP_BEGIN(SP_HeldItemChange) {
    printf("sid=%d", tpkt->sid);
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
    Pmeta(meta);
} DECODE_END;

DUMP_BEGIN(SP_SpawnPlayer) {
    printf("eid=%08x, uuid=%s, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, item=%d",
           tpkt->eid, limhex(tpkt->uuid,16,16),
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->item);
    dump_metadata(tpkt->meta, Human);
} DUMP_END;

FREE_BEGIN(SP_SpawnPlayer) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0d SP_CollectItem

DECODE_BEGIN(SP_CollectItem,_1_8_1) {
    Pvarint(eid);
    Pvarint(collector);
} DECODE_END;

ENCODE_BEGIN(SP_CollectItem,_1_8_1) {
    Wvarint(eid);
    Wvarint(collector);
} ENCODE_END;

DUMP_BEGIN(SP_CollectItem) {
    printf("eid=%08x, collector=%d", tpkt->eid, tpkt->collector);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0e SP_SpawnObject

DECODE_BEGIN(SP_SpawnObject,_1_8_1) {
    Pvarint(eid);
    Pchar(objtype);
    Pint(x);
    Pint(y);
    Pint(z);
    Pchar(pitch);
    Pchar(yaw);
    //TODO: object data
} DECODE_END;

DUMP_BEGIN(SP_SpawnObject) {
    printf("eid=%08x, objtype=%d, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f",
           tpkt->eid, tpkt->objtype,
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256);
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
    Pmeta(meta);
} DECODE_END;

DUMP_BEGIN(SP_SpawnMob) {
    char buf[256];
    printf("eid=%08x, mobtype=%d (%s), coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f,%.1f, vel=%d,%d,%d",
           tpkt->eid, tpkt->mobtype, get_entity_name(buf, tpkt->mobtype),
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,(float)tpkt->headpitch/256,
           tpkt->vx,tpkt->vy,tpkt->vz);
    dump_metadata(tpkt->meta, tpkt->mobtype);
} DUMP_END;

FREE_BEGIN(SP_SpawnMob) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x10 SP_SpawnPainting

DECODE_BEGIN(SP_SpawnPainting,_1_8_1) {
    Pvarint(eid);
    Pstr(title);
    Plong(pos.p);
    Pchar(dir);
} DECODE_END;

DUMP_BEGIN(SP_SpawnPainting) {
    printf("eid=%08x, title=%s, location=%d,%d,%d direction=%d",
           tpkt->eid, tpkt->title,
           tpkt->pos.x,  tpkt->pos.y,  tpkt->pos.z,
           tpkt->dir);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x11 SP_SpawnExperienceOrb

DECODE_BEGIN(SP_SpawnExperienceOrb,_1_8_1) {
    Pvarint(eid);
    Pint(x);
    Pint(y);
    Pint(z);
    Pshort(count);
} DECODE_END;

DUMP_BEGIN(SP_SpawnExperienceOrb) {
    printf("eid=%08x, coord=%.1f,%.1f,%.1f, count=%d",
           tpkt->eid,
           (float)tpkt->x/32,(float)tpkt->y/32,(float)tpkt->z/32,
           tpkt->count);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x12 SP_EntityVelocity

DECODE_BEGIN(SP_EntityVelocity,_1_8_1) {
    Pvarint(eid);
    Pshort(vx);
    Pshort(vy);
    Pshort(vz);
} DECODE_END;

DUMP_BEGIN(SP_EntityVelocity) {
    printf("eid=%08x, vel=%d,%d,%d", tpkt->eid, tpkt->vx,tpkt->vy,tpkt->vz);
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
    Pint(x);
    Pint(y);
    Pint(z);
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
// 0x1c SP_EntityMetadata

DECODE_BEGIN(SP_EntityMetadata,_1_8_1) {
    Pvarint(eid);
    Pmeta(meta);
} DECODE_END;

DUMP_BEGIN(SP_EntityMetadata) {
    printf("eid=%08x", tpkt->eid);
    // unfortunately we don't have access to proper entity type here
    dump_metadata(tpkt->meta, Entity);
} DUMP_END;

FREE_BEGIN(SP_EntityMetadata) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x1f SP_SetExperience

DECODE_BEGIN(SP_SetExperience,_1_8_1) {
    Pfloat(bar);
    Pvarint(level);
    Pvarint(exp);
} DECODE_END;

DUMP_BEGIN(SP_SetExperience) {
    printf("bar=%.2f level=%d exp=%d",tpkt->bar, tpkt->level, tpkt->exp);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x21 SP_ChunkData

DECODE_BEGIN(SP_ChunkData,_1_8_1) {
    Pint(chunk.X);
    Pint(chunk.Z);
    Pchar(cont);
    Pshort(chunk.mask);
    Rvarint(size);

    tpkt->skylight=0;

    if (tpkt->chunk.mask) {
        int nblk = count_bits(tpkt->chunk.mask);
        tpkt->skylight = ((size-256)/nblk == 3*4096);
    }

    p=read_chunk(p, tpkt->skylight, &tpkt->chunk);
} DECODE_END;

ENCODE_BEGIN(SP_ChunkData,_1_8_1) {
    Wint(chunk.X);
    Wint(chunk.Z);
    Wchar(cont);

    Wshort(chunk.mask);

    int nblk = count_bits(tpkt->chunk.mask);
    uint32_t size = 256 + nblk*(10240+2048*tpkt->skylight);
    lh_write_varint(w, size);

    w = write_chunk(w, tpkt->skylight, &tpkt->chunk);
} ENCODE_END;

DUMP_BEGIN(SP_ChunkData) {
    printf("coord=%4d:%4d, cont=%d, skylight=%d, mask=%04x",
           tpkt->chunk.X, tpkt->chunk.Z, tpkt->cont,
           tpkt->skylight, tpkt->chunk.mask);
} DUMP_END;

FREE_BEGIN(SP_ChunkData) {
    int i;
    for(i=0; i<16; i++) {
        lh_free(tpkt->chunk.cubes[i]);
    }
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x22 SP_MultiBlockChange

DECODE_BEGIN(SP_MultiBlockChange,_1_8_1) {
    Pint(X);
    Pint(Z);
    Pvarint(count);
    lh_alloc_num(tpkt->blocks, tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        Pchar(blocks[i].pos);
        Pchar(blocks[i].y);
        Rvarint(bid);
        tpkt->blocks[i].bid.raw = (uint16_t)bid;
    }
} DECODE_END;

ENCODE_BEGIN(SP_MultiBlockChange,_1_8_1) {
    Wint(X);
    Wint(Z);
    Wvarint(count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        Wchar(blocks[i].pos);
        Wchar(blocks[i].y);
        Wvarint(blocks[i].bid.raw);
    }
} ENCODE_END;

DUMP_BEGIN(SP_MultiBlockChange) {
    printf("chunk=%d:%d, count=%d",
           tpkt->X, tpkt->Z, tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        blkrec *b = tpkt->blocks+i;
        printf("\n    coord=%2d,%3d,%2d bid=%3x meta=%d",
               b->x,b->z,b->y,b->bid.bid,b->bid.meta);
    }
} DUMP_END;

FREE_BEGIN(SP_MultiBlockChange) {
    lh_free(tpkt->blocks);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x23 SP_BlockChange

DECODE_BEGIN(SP_BlockChange,_1_8_1) {
    Plong(pos.p);
    Rvarint(bid);
    tpkt->block.raw = (uint16_t)bid;
} DECODE_END;

ENCODE_BEGIN(SP_BlockChange,_1_8_1) {
    Wlong(pos.p);
    Wvarint(block.raw);
} ENCODE_END;

DUMP_BEGIN(SP_BlockChange) {
    printf("coord=%d,%d,%d bid=%3x meta=%d",
           tpkt->pos.x, tpkt->pos.y, tpkt->pos.z,
           tpkt->block.bid,tpkt->block.meta);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x26 SP_MapChunkBulk

DECODE_BEGIN(SP_MapChunkBulk,_1_8_1) {
    Pchar(skylight);
    Pvarint(nchunks);
    lh_alloc_num(tpkt->chunk, tpkt->nchunks);

    int i;
    // read chunk headers
    for(i=0; i<tpkt->nchunks; i++) {
        Pint(chunk[i].X);
        Pint(chunk[i].Z);
        Pshort(chunk[i].mask);
    }
    // read chunk data
    for(i=0; i<tpkt->nchunks; i++) {
        p=read_chunk(p, tpkt->skylight, &tpkt->chunk[i]);
    }
} DECODE_END;

ENCODE_BEGIN(SP_MapChunkBulk,_1_8_1) {
    Wchar(skylight);
    Wvarint(nchunks);

    int i;
    // write chunk headers
    for(i=0; i<tpkt->nchunks; i++) {
        Wint(chunk[i].X);
        Wint(chunk[i].Z);
        Wshort(chunk[i].mask);
    }
    // write chunk data
    for(i=0; i<tpkt->nchunks; i++) {
        w=write_chunk(w, tpkt->skylight, &tpkt->chunk[i]);
    }
} ENCODE_END;

DUMP_BEGIN(SP_MapChunkBulk) {
    int i;
    printf("nchunks=%d, skylight=%d",tpkt->nchunks,tpkt->skylight);
    for(i=0; i<tpkt->nchunks; i++) {
        printf("\n  coord=%4d:%4d mask=%04x",
               tpkt->chunk[i].X, tpkt->chunk[i].Z, tpkt->chunk[i].mask);
    }
} DUMP_END;

FREE_BEGIN(SP_MapChunkBulk) {
    int i,j;

    for(j=0; j<tpkt->nchunks; j++) {
        for(i=0; i<16; i++) {
            lh_free(tpkt->chunk[j].cubes[i]);
        }
    }
    lh_free(tpkt->chunk);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x27 SP_Explosion

DECODE_BEGIN(SP_Explosion,_1_8_1) {
    Pfloat(x);
    Pfloat(y);
    Pfloat(z);
    Pfloat(radius);
    Pint(count);
    lh_alloc_num(tpkt->blocks, tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        boff_t *b = tpkt->blocks+i;
        Rchar(dx);
        Rchar(dy);
        Rchar(dz);
        b->dx = dx;
        b->dy = dy;
        b->dz = dz;
    }
    Pfloat(vx);
    Pfloat(vy);
    Pfloat(vz);
} DECODE_END;

DUMP_BEGIN(SP_Explosion) {
    printf("coord=%.1f,%.1f,%.1f, radius=%.1f, velocity=%.1f,%.1f,%.1f, count=%d",
           tpkt->x, tpkt->y, tpkt->z, tpkt->radius,
           tpkt->vx, tpkt->vy, tpkt->vz, tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        boff_t *b = tpkt->blocks+i;
        printf("\n  offset=%d,%d,%d",b->dx,b->dy,b->dz);
    }
} DUMP_END;

FREE_BEGIN(SP_Explosion) {
    lh_free(tpkt->blocks);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x28 SP_Effect

DECODE_BEGIN(SP_Effect,_1_8_1) {
    Pint(id);
    Plong(loc.p);
    Pint(data);
    Pchar(disvol);
} DECODE_END;

DUMP_BEGIN(SP_Effect) {
    printf("id=%d loc=%d,%d,%d data=%d disvol=%d",
           tpkt->id, tpkt->loc.x, tpkt->loc.y, tpkt->loc.z,
           tpkt->data, tpkt->disvol);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x29 SP_SoundEffect

DECODE_BEGIN(SP_SoundEffect,_1_8_1) {
    Pstr(name);
    Pint(x);
    Pint(y);
    Pint(z);
    Pfloat(vol);
    Pchar(pitch);
} DECODE_END;

DUMP_BEGIN(SP_SoundEffect) {
    printf("name=%s, coord=%.1f,%.1f,%.1f, vol=%.2f, pitch=%d",
           tpkt->name,
           (float)tpkt->x/8,(float)tpkt->y/8,(float)tpkt->z/8,
           tpkt->vol, tpkt->pitch);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2d SP_OpenWindow

DECODE_BEGIN(SP_OpenWindow,_1_8_1) {
    Pchar(wid);
    Pstr(wtype);
    Rstr(title);
    tpkt->title = strdup(title);
    Pchar(nslots);
    if (!strcmp(tpkt->wtype, "EntityHorse")) {
        Pint(eid);
    }
} DECODE_END;

ENCODE_BEGIN(SP_OpenWindow,_1_8_1) {
    Wchar(wid);
    Wstr(wtype);
    Wstr(title);
    Wchar(nslots);
    if (!strcmp(tpkt->wtype, "EntityHorse")) {
        Wint(eid);
    }
} ENCODE_END;

DUMP_BEGIN(SP_OpenWindow) {
    printf("wid=%d wtype=%s title=%s nslots=%d eid=%d",
           tpkt->wid,tpkt->wtype,tpkt->title,tpkt->nslots,tpkt->eid);
} DUMP_END;

FREE_BEGIN(SP_OpenWindow) {
    lh_free(tpkt->title);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2e SP_CloseWindow

DECODE_BEGIN(SP_CloseWindow,_1_8_1) {
    Pchar(wid);
} DECODE_END;

ENCODE_BEGIN(SP_CloseWindow,_1_8_1) {
    Wchar(wid);
} ENCODE_END;

DUMP_BEGIN(SP_CloseWindow) {
    printf("wid=%d", tpkt->wid);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2f SP_SetSlot

DECODE_BEGIN(SP_SetSlot,_1_8_1) {
    Pchar(wid);
    Pshort(sid);
    p = read_slot(p, &tpkt->slot);
} DECODE_END;

ENCODE_BEGIN(SP_SetSlot,_1_8_1) {
    Wchar(wid);
    Wshort(sid);
    w = write_slot(w, &tpkt->slot);
} ENCODE_END;

DUMP_BEGIN(SP_SetSlot) {
    printf("wid=%d sid=%d slot:",tpkt->wid,tpkt->sid);
    dump_slot(&tpkt->slot);
} DUMP_END;

FREE_BEGIN(SP_SetSlot) {
    clear_slot(&tpkt->slot);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x30 SP_WindowItems

DECODE_BEGIN(SP_WindowItems,_1_8_1) {
    Pchar(wid);
    Pshort(count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        p = read_slot(p, &tpkt->slots[i]);
    }
} DECODE_END;

ENCODE_BEGIN(SP_WindowItems,_1_8_1) {
    Wchar(wid);
    Wshort(count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        w = write_slot(w, &tpkt->slots[i]);
    }
} ENCODE_END;

DUMP_BEGIN(SP_WindowItems) {
    printf("wid=%d count=%d\n",tpkt->wid,tpkt->count);
    int i;
    for(i=0; i<tpkt->count; i++) {
        printf("  %d : ",i);
        dump_slot(&tpkt->slots[i]);
        printf("\n");
    }
} DUMP_END;

FREE_BEGIN(SP_WindowItems) {
    int i;
    for(i=0; i<tpkt->count; i++)
        clear_slot(&tpkt->slots[i]);
} FREE_END;


////////////////////////////////////////////////////////////////////////////////
// 0x32 SP_ConfirmTransaction

DECODE_BEGIN(SP_ConfirmTransaction,_1_8_1) {
    Pchar(wid);
    Pshort(aid);
    Pchar(accepted);
} DECODE_END;

DUMP_BEGIN(SP_ConfirmTransaction) {
    printf("wid=%d action=%d accepted=%d", tpkt->wid, tpkt->aid, tpkt->accepted);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x34 SP_Map

DECODE_BEGIN(SP_Maps,_1_8_1) {
    Pvarint(mapid);
    Pchar(scale);
    Pvarint(nicons);
    lh_alloc_num(tpkt->icons, tpkt->nicons);
    int i;
    for(i=0; i<tpkt->nicons; i++) {
        Pchar(icons[i].type);
        Pchar(icons[i].x);
        Pchar(icons[i].z);
    }
    Pchar(ncols);
    if (tpkt->ncols > 0) {
        Pchar(nrows);
        Pchar(X);
        Pchar(Z);
        Pvarint(len);
        lh_alloc_num(tpkt->data, tpkt->len);
        Pdata(data, tpkt->len);
    }
} DECODE_END;

DUMP_BEGIN(SP_Maps) {
    printf("id=%d, scale=%d, icons=%d, size=%d,%d, at=%d,%d, len=%d",
           tpkt->mapid, tpkt->scale, tpkt->nicons,
           tpkt->ncols, tpkt->nrows, tpkt->X, tpkt->Z, tpkt->len);
} DUMP_END;

FREE_BEGIN(SP_Maps) {
    lh_free(tpkt->icons);
    lh_free(tpkt->data);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x35 SP_UpdateBlockEntity

DECODE_BEGIN(SP_UpdateBlockEntity,_1_8_1) {
    Plong(loc.p);
    Pchar(action);
    tpkt->nbt = nbt_parse(&p);
} DECODE_END;

DUMP_BEGIN(SP_UpdateBlockEntity) {
    printf("pos=%d,%d,%d action=%d nbt=%s\n", tpkt->loc.x, tpkt->loc.z, tpkt->loc.y,
           tpkt->action, tpkt->nbt ? "present" : "none");
    if (tpkt->nbt)
        nbt_dump(tpkt->nbt);
} DUMP_END;

FREE_BEGIN(SP_UpdateBlockEntity) {
    nbt_free(tpkt->nbt);
    tpkt->nbt = NULL;
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x38 SP_PlayerListItem

DECODE_BEGIN(SP_PlayerListItem,_1_8_1) {
    Pvarint(action);
    Rvarint(np); // number of player entries that follow
    lh_arr_allocate_c(GAR(tpkt->list), np);

    int i,j;
    for(i=0; i<np; i++) {
        pli_t * entry = P(tpkt->list)+i;
        memmove(entry->uuid,p,16); p+=16;

        switch(tpkt->action) {
            case 0: { // add player
                p=read_string(p,entry->name);
                Rvarint(nprop);
                lh_arr_allocate_c(GAR(entry->prop), nprop);
                for(j=0; j<nprop; j++) {
                    pli_prop * pp = P(entry->prop)+j;
                    p=read_string(p,pp->pname);
                    p=read_string(p,pp->pval);
                    Rchar(is_signed);
                    pp->is_signed = is_signed;
                    if (is_signed) {
                        p=read_string(p,pp->signature);
                    }
                }
                Rvarint(gamemode); entry->gamemode = gamemode;
                Rvarint(ping); entry->ping = ping;
                Rchar(has_dispname); entry->has_dispname = has_dispname;
                if (has_dispname) {
                    p=read_string(p,entry->dispname);
                }
                break;
            }

            case 1: { // update gamemode
                Rvarint(gamemode); entry->gamemode = gamemode;
                break;
            }

            case 2: { // update ping
                Rvarint(ping); entry->ping = ping;
                break;
            }

            case 3: { // update display name
                Rchar(has_dispname); entry->has_dispname = has_dispname;
                if (has_dispname) {
                    p=read_string(p,entry->dispname);
                }
                break;
            }

            case 4: { // remove player
                break;
            }
        }
    }
} DECODE_END;

DUMP_BEGIN(SP_PlayerListItem) {
    printf("action=%d, np=%zd\n", tpkt->action, C(tpkt->list));
    int i,j;
    for(i=0; i<C(tpkt->list); i++) {
        pli_t * entry = P(tpkt->list)+i;
        printf("  %d: uuid=%s",  i, limhex(entry->uuid,16,16));
        switch(tpkt->action) {
            case 0:
                printf(" ADD name=%s, gamemode=%d, ping=%d", entry->name, entry->gamemode, entry->ping);
                if (entry->has_dispname)
                    printf(", dispname=%s\n", entry->dispname);
                else
                    printf("\n");

                if (C(entry->prop) > 0) {
                    for(j=0; j<C(entry->prop); j++) {
                        pli_prop * pp = P(entry->prop)+j;
                        printf("    PROPERTY %d : %s = %s\n", j, pp->pname, pp->pval);
                        //if (pp->is_signed) printf("      SIGNED %s\n", pp->signature);
                    }
                }
                break;

            case 1:
                printf(" UPD gamemode=%d\n", entry->gamemode);
                break;

            case 2:
                printf(" UPD ping=%d\n", entry->ping);
                break;

            case 3:
                printf(" UPD dispname=%s\n", entry->has_dispname ? entry->dispname : "N/A");
                break;

            case 4:
                printf(" DEL\n");
                break;
        }
    }
} DUMP_END;

FREE_BEGIN(SP_PlayerListItem) {
    int i,j;
    for(i=0; i<C(tpkt->list); i++) {
        pli_t * entry = P(tpkt->list)+i;
        lh_arr_free(GAR(entry->prop));
    }
    lh_arr_free(GAR(tpkt->list));
} FREE_END;

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
// 0x02 CP_UseEntity

DECODE_BEGIN(CP_UseEntity,_1_8_1) {
    Pvarint(target);
    Pvarint(action);
    if (tpkt->action==2) {
        Pfloat(x);
        Pfloat(y);
        Pfloat(z);
    }
} DECODE_END;

ENCODE_BEGIN(CP_UseEntity,_1_8_1) {
    Wvarint(target);
    Wvarint(action);
    if (tpkt->action==2) {
        Wfloat(x);
        Wfloat(y);
        Wfloat(z);
    }
} ENCODE_END;

DUMP_BEGIN(CP_UseEntity) {
    printf("target=%08x action=%d", tpkt->target,tpkt->action);
    if (tpkt->action == 2)
        printf(" coord=%.1f,%.1f,%.1f", tpkt->x,tpkt->y,tpkt->z);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x03 CP_Player

DECODE_BEGIN(CP_Player,_1_8_1) {
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(CP_Player) {
    printf("onground=%d",tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x04 CP_PlayerPosition

DECODE_BEGIN(CP_PlayerPosition,_1_8_1) {
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(CP_PlayerPosition) {
    printf("coord=%.1f,%.1f,%.1f, onground=%d",
           tpkt->x,tpkt->y,tpkt->z,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x05 CP_PlayerLook

DECODE_BEGIN(CP_PlayerLook,_1_8_1) {
    Pfloat(yaw);
    Pfloat(pitch);
    Pchar(onground);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerLook,_1_8_1) {
    Wfloat(yaw);
    Wfloat(pitch);
    Wchar(onground);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerLook) {
    printf("rot=%.1f,%.1f, onground=%d",
           tpkt->yaw,tpkt->pitch,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x06 CP_PlayerPositionLook

DECODE_BEGIN(CP_PlayerPositionLook,_1_8_1) {
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pfloat(yaw);
    Pfloat(pitch);
    Pchar(onground);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerPositionLook,_1_8_1) {
    Wdouble(x);
    Wdouble(y);
    Wdouble(z);
    Wfloat(yaw);
    Wfloat(pitch);
    Wchar(onground);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerPositionLook) {
    printf("coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",
           tpkt->x,tpkt->y,tpkt->z,tpkt->yaw,tpkt->pitch,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x07 CP_PlayerDigging

DECODE_BEGIN(CP_PlayerDigging,_1_8_1) {
    Pchar(status);
    Plong(loc.p);
    Pchar(face);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerDigging,_1_8_1) {
    Wchar(status);
    Wlong(loc.p);
    Wchar(face);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerDigging) {
    printf("status=%d, location=%d,%d,%d, face=%d",
           tpkt->status, tpkt->loc.x, tpkt->loc.y, tpkt->loc.z, tpkt->face);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x08 CP_PlayerBlockPlacement

DECODE_BEGIN(CP_PlayerBlockPlacement,_1_8_1) {
    Plong(bpos.p);
    Pchar(face);
    p = read_slot(p, &tpkt->item);
    Pchar(cx);
    Pchar(cy);
    Pchar(cz);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerBlockPlacement,_1_8_1) {
    Wlong(bpos.p);
    Wchar(face);
    w = write_slot(w, &tpkt->item);
    Wchar(cx);
    Wchar(cy);
    Wchar(cz);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerBlockPlacement) {
    printf("bpos=%d,%d,%d, face=%d, cursor=%d,%d,%d, item=",
           tpkt->bpos.x,  tpkt->bpos.y,  tpkt->bpos.z,
           tpkt->face, tpkt->cx, tpkt->cy, tpkt->cz);
    dump_slot(&tpkt->item);
} DUMP_END;

FREE_BEGIN(CP_PlayerBlockPlacement) {
    clear_slot(&tpkt->item);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x09 CP_HeldItemChange

DECODE_BEGIN(CP_HeldItemChange,_1_8_1) {
    Pshort(sid);
} DECODE_END;

ENCODE_BEGIN(CP_HeldItemChange,_1_8_1) {
    Wshort(sid);
} ENCODE_END;

DUMP_BEGIN(CP_HeldItemChange) {
    printf("sid=%d", tpkt->sid);
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
// 0x0b CP_EntityAction

DECODE_BEGIN(CP_EntityAction,_1_8_1) {
    Pvarint(eid);
    Pvarint(action);
    Pvarint(jumpboost);
} DECODE_END;

ENCODE_BEGIN(CP_EntityAction,_1_8_1) {
    Wvarint(eid);
    Wvarint(action);
    Wvarint(jumpboost);
} ENCODE_END;

DUMP_BEGIN(CP_EntityAction) {
    printf("eid=%08x, action=%d, jumpboost=%d",
           tpkt->eid, tpkt->action, tpkt->jumpboost);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0d CP_CloseWindow

DECODE_BEGIN(CP_CloseWindow,_1_8_1) {
    Pchar(wid);
} DECODE_END;

ENCODE_BEGIN(CP_CloseWindow,_1_8_1) {
    Wchar(wid);
} ENCODE_END;

DUMP_BEGIN(CP_CloseWindow) {
    printf("wid=%d", tpkt->wid);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0e CP_ClickWindow

DECODE_BEGIN(CP_ClickWindow,_1_8_1) {
    Pchar(wid);
    Pshort(sid);
    Pchar(button);
    Pshort(aid);
    Pchar(mode);
    p = read_slot(p, &tpkt->slot);
} DECODE_END;

ENCODE_BEGIN(CP_ClickWindow,_1_8_1) {
    Wchar(wid);
    Wshort(sid);
    Wchar(button);
    Wshort(aid);
    Wchar(mode);
    w = write_slot(w, &tpkt->slot);
} ENCODE_END;

DUMP_BEGIN(CP_ClickWindow) {
    printf("wid=%d, sid=%d, aid=%d, button=%d, mode=%d, slot:",
           tpkt->wid, tpkt->sid, tpkt->aid, tpkt->button, tpkt->mode);
    dump_slot(&tpkt->slot);
} DUMP_END;

FREE_BEGIN(CP_ClickWindow) {
    clear_slot(&tpkt->slot);
} FREE_END;




////////////////////////////////////////////////////////////////////////////////

const static packet_methods SUPPORT_1_8_1[2][MAXPACKETTYPES] = {
    {
        SUPPORT_D   (SP_KeepAlive,_1_8_1),
        SUPPORT_D   (SP_JoinGame,_1_8_1),
        SUPPORT_DEF (SP_ChatMessage,_1_8_1),
        SUPPORT_D   (SP_TimeUpdate,_1_8_1),
        SUPPORT_DF  (SP_EntityEquipment,_1_8_1),
        SUPPORT_D   (SP_UpdateHealth,_1_8_1),
        SUPPORT_D   (SP_Respawn,_1_8_1),
        SUPPORT_DE  (SP_PlayerPositionLook,_1_8_1),
        SUPPORT_DE  (SP_HeldItemChange,_1_8_1),
        SUPPORT_DF  (SP_SpawnPlayer,_1_8_1),
        SUPPORT_DE  (SP_CollectItem,_1_8_1),
        SUPPORT_D   (SP_SpawnObject,_1_8_1),
        SUPPORT_DF  (SP_SpawnMob,_1_8_1),
        SUPPORT_D   (SP_SpawnPainting,_1_8_1),
        SUPPORT_D   (SP_SpawnExperienceOrb,_1_8_1),
        SUPPORT_D   (SP_EntityVelocity,_1_8_1),
        SUPPORT_DF  (SP_DestroyEntities,_1_8_1),
        SUPPORT_D   (SP_Entity,_1_8_1),
        SUPPORT_D   (SP_EntityRelMove,_1_8_1),
        SUPPORT_D   (SP_EntityLook,_1_8_1),
        SUPPORT_D   (SP_EntityLookRelMove,_1_8_1),
        SUPPORT_D   (SP_EntityTeleport,_1_8_1),
        SUPPORT_DF  (SP_EntityMetadata,_1_8_1),
        SUPPORT_D   (SP_SetExperience,_1_8_1),
        SUPPORT_DEF (SP_ChunkData,_1_8_1),
        SUPPORT_DEF (SP_MultiBlockChange,_1_8_1),
        SUPPORT_DE  (SP_BlockChange,_1_8_1),
        SUPPORT_DEF (SP_MapChunkBulk,_1_8_1),
        SUPPORT_DF  (SP_Explosion,_1_8_1),
        SUPPORT_D   (SP_Effect,_1_8_1),
        SUPPORT_D   (SP_SoundEffect,_1_8_1),
        SUPPORT_DEF (SP_OpenWindow,_1_8_1),
        SUPPORT_DE  (SP_CloseWindow,_1_8_1),
        SUPPORT_DEF (SP_SetSlot,_1_8_1),
        SUPPORT_DEF (SP_WindowItems,_1_8_1),
        SUPPORT_D   (SP_ConfirmTransaction,_1_8_1),
        SUPPORT_DF  (SP_Maps,_1_8_1),
        SUPPORT_DF  (SP_UpdateBlockEntity,_1_8_1),
        SUPPORT_DF  (SP_PlayerListItem,_1_8_1),
        SUPPORT_DE  (SP_SetCompression,_1_8_1),
    },
    {
        SUPPORT_D   (CP_ChatMessage,_1_8_1),
        SUPPORT_DE  (CP_UseEntity,_1_8_1),
        SUPPORT_D   (CP_Player,_1_8_1),
        SUPPORT_D   (CP_PlayerPosition,_1_8_1),
        SUPPORT_DE  (CP_PlayerLook,_1_8_1),
        SUPPORT_DE  (CP_PlayerPositionLook,_1_8_1),
        SUPPORT_DE  (CP_PlayerDigging,_1_8_1),
        SUPPORT_DEF (CP_PlayerBlockPlacement,_1_8_1),
        SUPPORT_DE  (CP_HeldItemChange,_1_8_1),
        SUPPORT_DE  (CP_Animation,_1_8_1),
        SUPPORT_DE  (CP_EntityAction,_1_8_1),
        SUPPORT_DE  (CP_CloseWindow,_1_8_1),
        SUPPORT_DEF (CP_ClickWindow,_1_8_1),
    },
};




////////////////////////////////////////////////////////////////////////////////


void decode_handshake(CI_Handshake_pkt *tpkt, uint8_t *p) {
    Pvarint(protocolVer);
    Pstr(serverAddr);
    Pshort(serverPort);
    Pvarint(nextState);
}

uint8_t * encode_handshake(CI_Handshake_pkt *tpkt, uint8_t *w) {
    Wvarint(protocolVer);
    Wstr(serverAddr);
    Wshort(serverPort);
    Wvarint(nextState);
    return w;
}

void decode_encryption_request(SL_EncryptionRequest_pkt *tpkt, uint8_t *p) {
    Pstr(serverID);
    Pvarint(klen);
    Pdata(pkey,tpkt->klen);
    Pvarint(tlen);
    Pdata(token,tpkt->tlen);
}

void decode_encryption_response(CL_EncryptionResponse_pkt *tpkt, uint8_t *p) {
    Pvarint(sklen);
    Pdata(skey,tpkt->sklen);
    Pvarint(tklen);
    Pdata(token,tpkt->tklen);
}







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
