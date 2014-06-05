#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "lh_arr.h"
#include "lh_bytes.h"

#define GSOP_PRUNE_CHUNKS       1
#define GSOP_SEARCH_SPAWNERS    2
#define GSOP_TRACK_ENTITIES     3

#define SPAWNER_TYPE_UNKNOWN    0
#define SPAWNER_TYPE_ZOMBIE     1
#define SPAWNER_TYPE_SKELETON   2
#define SPAWNER_TYPE_SPIDER     3
#define SPAWNER_TYPE_CAVESPIDER 4

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    int32_t x,z;
    uint8_t y;
} bcoord; // block coordinate

typedef struct {
    int32_t X,Z;
    uint32_t i;   // full-byte offset (i.e. in .blocks)
} ccoord; // chunk coordinate

static inline bcoord c2b(ccoord c) {
    bcoord b = {
        .x = c.X*16 + (c.i&0x0f),
        .z = c.Z*16 + ((c.i>>4)&0x0f),
        .y = (c.i>>8)
    };
    return b;
}

static inline ccoord b2c(bcoord b) {
    ccoord c = {
        .X = b.x>>4,
        .Z = b.z>>4,
        .i = (b.x&0xf) + ((b.z&0xf)<<4) + (b.y<<8)
    };
    return c;
}

typedef struct _chunk {
    uint8_t blocks[4096*16];
    uint8_t meta[2048*16];
    uint8_t add[2048*16];
    uint8_t light[2048*16];
    uint8_t skylight[2048*16];
    uint8_t biome[256];
    int32_t X,Z;
} chunk;

typedef struct _chunkid {
    int32_t X,Z;
    chunk * c;
} chunkid;

typedef struct _spawner {
    ccoord co;
    int type;
    float nearest;
} spawner;

#define ENTITY_UNKNOWN  0
#define ENTITY_SELF     1
#define ENTITY_PLAYER   2
#define ENTITY_MOB      3
#define ENTITY_OBJECT   4
#define ENTITY_OTHER    5

#define DIM_OVERWORLD   0x00
#define DIM_NETHER      0xff
#define DIM_END         0x01

typedef struct _entity {
    int32_t id;
    int32_t x,y,z;      // note: fixed-point coords, shift by ???
    int  type;          // one of the ENTITY_ variables
    int  mtype;         // mob/object type as used natively
    int  hostile;       // whether marked hostile
    uint64_t lasthit;   // timestamp when this entity was last attacked - for limiting the attack rate
    char name[256];     // only valid for players
} entity;

typedef struct _slot_t {
    uint16_t id;
    uint8_t  count;
    uint16_t damage;
    uint16_t dlen;
    uint8_t  data[65536];
} slot_t;

typedef struct _window {
    int nslots;
    int id;
    slot_t slots[64];
} window;

typedef struct _gamestate {
    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
    } opt;

    struct {
        uint32_t id;
        int32_t x,y,z;
        double  dx,dheady,dfeety,dz;
        uint8_t ground;
        float yaw,pitch;
    } own;

    window inventory;
    int    held;
            
    uint8_t current_dimension;
    // current chunks
    lh_arr_declare(chunkid, chunk);

    // Overworld chunks
    lh_arr_declare(chunkid, chunko);

    // Nether chunks
    lh_arr_declare(chunkid, chunkn);

    // Nether chunks
    lh_arr_declare(chunkid, chunke);

    // spawners
    lh_arr_declare(spawner, spawner);

    // entities
    lh_arr_declare(entity, entity);

    uint64_t last_attack;   

} gamestate;

extern gamestate gs;

////////////////////////////////////////////////////////////////////////////////

static uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

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
#define Rslot(n)  slot_t n; p=read_slot(p,&n)


////////////////////////////////////////////////////////////////////////////////

int reset_gamestate();

int set_option(int optid, int value);
int get_option(int optid);

int import_packet(uint8_t *p, ssize_t size, int is_client);
int search_spawners();

int get_entities_in_range(int *dst, int max, float range,
    int (*filt_pred)(entity *), int (*sort_pred)(entity *, entity *) );

int is_in_range(entity * e, float range);
int import_clpacket(uint8_t *ptr, ssize_t size);

uint8_t * export_cuboid(int Xl, int Xh, int Zl, int Zh, int yl, int yh);
int get_chunks_dim(int *Xl, int *Xh, int *Zl, int *Zh);
