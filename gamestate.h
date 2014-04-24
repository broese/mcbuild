#pragma once

#define GSOP_PRUNE_CHUNKS       1
#define GSOP_SEARCH_SPAWNERS    2

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

////////////////////////////////////////////////////////////////////////////////

int reset_gamestate();

int set_option(int optid, int value);
int get_option(int optid);

int import_packet(uint8_t *p, ssize_t size);
int search_spawners();

