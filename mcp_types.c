#include <stdio.h>
#include <stdlib.h>

#include <lh_buffers.h>

#include "mcp_types.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////
// Coordinates

extent_t ps2extent(pivot_t pv, size3_t sz) {
    int32_t tx,ty,tz;

    // calculate the point opposite of the pivot based on specified size
    switch (pv.dir) {
        case DIR_NORTH: tx=pv.pos.x+(sz.x-1); tz=pv.pos.z-(sz.z-1); break;
        case DIR_SOUTH: tx=pv.pos.x-(sz.x-1); tz=pv.pos.z+(sz.z-1); break;
        case DIR_EAST:  tx=pv.pos.x+(sz.z-1); tz=pv.pos.z+(sz.x-1); break;
        case DIR_WEST:  tx=pv.pos.x-(sz.z-1); tz=pv.pos.z-(sz.x-1); break;
        default: assert(0); break;
    }
    ty = pv.pos.y+(sz.y-1);

    // calculate the extent
    extent_t ex;
    ex.min.x = MIN(pv.pos.x,tx);
    ex.min.y = MIN(pv.pos.y,ty);
    ex.min.z = MIN(pv.pos.z,tz);
    ex.max.x = MAX(pv.pos.x,tx);
    ex.max.y = MAX(pv.pos.y,ty);
    ex.max.z = MAX(pv.pos.z,tz);

    return ex;
}

void free_cuboid(cuboid_t c) {
    int y;
    for(y=0; y<c.sa.y; y++)
        if (c.data[y])
            free(c.data[y]);
}

////////////////////////////////////////////////////////////////////////////////
// Window slots

void dump_slot(slot_t *s) {
    char buf[256];
    printf("item=%d (%s)", s->item, get_item_name(buf,s));
    if (s->item != -1) {
        printf(", count=%d, damage=%d", s->count, s->damage);
        if (s->nbt) {
            printf(", nbt=available:\n");
            nbt_dump(s->nbt);
        }
    }
}

void clear_slot(slot_t *s) {
    if (s->nbt) {
        nbt_free(s->nbt);
        s->nbt = NULL;
    }
    s->item = -1;
    s->count = 0;
    s->damage = 0;
}

slot_t * clone_slot(slot_t *src, slot_t *dst) {
    if (!dst)
        lh_alloc_obj(dst);

    dst->item = src->item;
    dst->count = src->count;
    dst->damage = src->damage;
    dst->nbt = nbt_clone(src->nbt);

    return dst;
}

////////////////////////////////////////////////////////////////////////////////
// Metadata

metadata * clone_metadata(metadata *meta) {
    if (!meta) return NULL;
    lh_create_num(metadata, newmeta, 32);
    memmove(newmeta, meta, 32*sizeof(metadata));
    int i;
    for(i=0; i<32; i++)
        if (newmeta[i].type == META_SLOT)
            clone_slot(&meta[i].slot, &newmeta[i].slot);
    return newmeta;
}

void free_metadata(metadata *meta) {
    if (!meta) return;
    int i;
    for(i=0; i<32; i++)
        if (meta[i].type == META_SLOT)
            clear_slot(&meta[i].slot);
    free(meta);
}

