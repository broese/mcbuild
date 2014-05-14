#pragma once

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

typedef struct _entity {
    int32_t id;
    int32_t x,y,z;      // note: fixed-point coords, shift by ???
    int  type;          // one of the ENTITY_ variables
    int  mtype;         // mob/object type as used natively
    int  hostile;       // whether marked hostile
    uint64_t lasthit;   // timestamp when this entity was last attacked - for limiting the attack rate
    char name[256];     // only valid for players
} entity;

typedef struct _gamestate {
    // options
    struct {
        int prune_chunks;
        int search_spawners;
        int track_entities;
    } opt;

    struct {
        int32_t x,y,z;
        uint32_t id;
    } own;
            
    // chunks
    lh_arr_declare(chunkid, chunk);

    // spawners
    lh_arr_declare(spawner, spawner);

    // entities
    lh_arr_declare(entity, entity);

    uint64_t last_attack;   

} gamestate;

extern gamestate gs;

////////////////////////////////////////////////////////////////////////////////

int reset_gamestate();

int set_option(int optid, int value);
int get_option(int optid);

int import_packet(uint8_t *p, ssize_t size);
int search_spawners();

int get_entities_in_range(int *dst, int max, float range,
    int (*filt_pred)(entity *), int (*sort_pred)(entity *, entity *) );

int is_in_range(entity * e, float range);
int import_clpacket(uint8_t *ptr, ssize_t size);
