#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_buffers.h>
#include <lh_compress.h>

#include "mcp_gamestate.h"

gamestate gs;
static int gs_used = 0;

////////////////////////////////////////////////////////////////////////////////
// entity tracking

static inline int find_entity(int eid) {
    int i;
    for(i=0; i<C(gs.entity); i++)
        if (P(gs.entity)[i].id == eid)
            return i;
    return -1;
}

void dump_entities() {
    printf("Tracking %zd entities:\n",C(gs.entity));
    int i;
    for(i=0; i<C(gs.entity); i++) {
        entity *e = P(gs.entity)+i;
        printf("  %4d eid=%08x type=%-7s coord=%.1f,%.1f,%.1f\n",
               i,e->id, ENTITY_TYPES[e->type],
               (float)e->x/32,(float)e->y/32,(float)e->z/32);
    }
}

////////////////////////////////////////////////////////////////////////////////
// chunk storage

// extend world in blocks of this many chunks 
#define WORLDALLOC (1<<4)

int32_t find_chunk(gsworld *w, int32_t X, int32_t Z) {
    if (w->Xs==0 || w->Zs==0) {
        //printf("%d,%d\n",X,Z);
        // this is the very first allocation, just set our offset coords
        w->Xo = X&~(WORLDALLOC-1);
        w->Zo = Z&~(WORLDALLOC-1);
        w->Xs = WORLDALLOC;
        w->Zs = WORLDALLOC;
        lh_alloc_num(w->chunks,w->Xs*w->Zs);
        return CHUNKIDX(w, X, Z);
    }

    int nXo=w->Xo,nZo=w->Zo;
    int nXs=w->Xs,nZs=w->Zs;

    // check if we need to extend our world westward
    if (X < w->Xo) {
        nXo = X&~(WORLDALLOC-1);
        nXs+=(w->Xo-nXo);
        printf("extending WEST: X=%d: %d->%d , %d->%d\n",X,w->Xo,nXo,w->Xs,nXs);
    }

    // extending eastwards
    else if (X >= w->Xo+w->Xs) {
        nXs = (X|(WORLDALLOC-1))+1 - w->Xo;
        printf("extending EAST: X=%d: %d->%d , %d->%d\n",X,w->Xo,nXo,w->Xs,nXs);
    }

    // extending to the north
    if (Z < w->Zo) {
        nZo = Z&~(WORLDALLOC-1);
        nZs+=(w->Zo-nZo);
        printf("extending NORTH: Z=%d: %d->%d , %d->%d\n",Z,w->Zo,nZo,w->Zs,nZs);
    }

    // extending to the south
    else if (Z >= w->Zo+w->Zs) {
        nZs = (Z|(WORLDALLOC-1))+1 - w->Zo;
        printf("extending SOUTH: Z=%d: %d->%d , %d->%d\n",Z,w->Zo,nZo,w->Zs,nZs);
    }

    // if the size has changed, we need to resize the chunks array
    if (w->Xs != nXs || w->Zs != nZs) {
        //printf("%d,%d\n",X,Z);
        // allocate a new array for the chunk pointers
        lh_create_num(gschunk *, chunks, nXs*nZs);

        // this is the offset in the _new_ array, from
        // where we will start copying the chunks from the old one
        int32_t offset = (w->Xo-nXo) + (w->Zo-nZo)*nXs;

        int i;
        for(i=0; i<w->Zs; i++) {
            //printf("row %d, offset=%d\n",i,offset);
            gschunk ** oldrow = w->chunks+i*w->Xs;
            //printf("moving %d*%zd bytes from %p (w->chunks[%d]) to %p (chunks[%d])\n",
            //       w->Xs,sizeof(gschunk *),oldrow,i*w->Xs,chunks+offset,offset);
            memmove(chunks+offset, oldrow, w->Xs*sizeof(gschunk *));
            offset += nXs;
        }
        lh_free(w->chunks);
        w->chunks = chunks;

        w->Xo = nXo; w->Xs = nXs;
        w->Zo = nZo; w->Zs = nZs;
    }

    return CHUNKIDX(w, X, Z);
}

static void insert_chunk(chunk_t *c) {
    gsworld *w = gs.world;
    int32_t idx = find_chunk(w, c->X, c->Z);

    if (!w->chunks[idx])
        lh_alloc_obj(w->chunks[idx]);
    gschunk *gc = w->chunks[idx];

    int i;
    for(i=0; i<16; i++) {
        if (c->cubes[i]) {
            memmove(gc->blocks+i*4096,   c->cubes[i]->blocks,   4096);
            memmove(gc->light+i*2048,    c->cubes[i]->light,    2048);
            memmove(gc->skylight+i*2048, c->cubes[i]->skylight, 2048);
        }
        else {
            memset(gc->blocks+i*4096, 0, 4096);
            memset(gc->light+i*2048, 0, 2048);
            memset(gc->skylight+i*2048, 0, 2048);
        }
        memmove(gc->biome, c->biome, 256);
    }
}

static void remove_chunk(int32_t X, int32_t Z) {
    int32_t idx = find_chunk(gs.world, X, Z);
    lh_free(gs.world->chunks[idx]);
    //TODO: resize the chunk array
}

static void free_chunks(gsworld *w) {
    if (!w) return;

    if (w->chunks) {
        int32_t sz = w->Xs*w->Zs;
        int i;
        for(i=0; i<sz; i++)
            lh_free(w->chunks[i]);

        lh_free(w->chunks);
    }
}

static void dump_chunks(gsworld *w) {
    if (!w) return;

    if (w->chunks) {
        int x,z;
        for(z=0; z<w->Zs; z++) {
            int32_t off = w->Xs*z;
            printf("%4d ",z+w->Zo);
            for(x=0; x<w->Xs; x++)
                printf("%c", w->chunks[off+x]?'#':'.');
            printf("\n");
        }
    }
}

static void change_dimension(int dimension) {
    printf("Switching to dimension %d\n",dimension);

    // prune all chunks of the current dimension
    if (gs.opt.prune_chunks)
        free_chunks(gs.world);

    switch(dimension) {
        case 0:  gs.world = &gs.overworld; break;
        case -1: gs.world = &gs.nether; break;
        case 1:  gs.world = &gs.end; break;
    }
}

static void modify_blocks(int32_t X, int32_t Z, blkrec *blocks, int32_t count) {
    int32_t idx = find_chunk(gs.world, X, Z);

    // get the chunk and allocate if it's not allocated yet
    gschunk * gc = gs.world->chunks[idx];
    if (!gc) {
        lh_alloc_obj(gs.world->chunks[idx]);
        gc = gs.world->chunks[idx];
    }

    int i;
    for(i=0; i<count; i++) {
        blkrec *b = blocks+i;
        gc->blocks[(int32_t)b->y*16+b->pos] = b->bid;
    }
}

////////////////////////////////////////////////////////////////////////////////

#define GSP(name)                               \
    case name: {                                \
        name##_pkt *tpkt = &pkt->_##name;

#define _GSP break; }

int gs_packet(MCPacket *pkt) {
    switch (pkt->pid) {

        ////////////////////////////////////////////////////////////////
        // Entities tracking

        GSP(SP_SpawnPlayer) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_PLAYER;
            //TODO: name
            //TODO: mark players hostile/neutral/friendly depending on the faglist
        } _GSP;

        GSP(SP_SpawnObject) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OBJECT;
        } _GSP;

        GSP(SP_SpawnMob) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_MOB;

            e->mtype = tpkt->mobtype;
            // mark all monster mobs hostile except pigmen (too dangerous)
            // and bats (too cute)
            if (e->mtype >= 50 && e->mtype <90 && e->mtype!=57 && e->mtype!=65)
                e->hostile = 1;
            // mark creepers extra hostile to make them priority targets
            if (e->mtype == 50)
                e->hostile = 2;
        } _GSP;

        GSP(SP_SpawnPainting) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->pos.x*32;
            e->y  = tpkt->pos.y*32;
            e->z  = tpkt->pos.z*32;
            e->type = ENTITY_OTHER;
        } _GSP;

        GSP(SP_SpawnExperienceOrb) {
            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = tpkt->eid;
            e->x  = tpkt->x;
            e->y  = tpkt->y;
            e->z  = tpkt->z;
            e->type = ENTITY_OTHER;
        } _GSP;

        GSP(SP_DestroyEntities) {
            int i;
            for(i=0; i<tpkt->count; i++) {
                int idx = find_entity(tpkt->eids[i]);
                if (idx<0) continue;
                lh_arr_delete(GAR(gs.entity),idx);
            }
        } _GSP;

        GSP(SP_EntityRelMove) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x += tpkt->dx;
            e->y += tpkt->dy;
            e->z += tpkt->dz;
        } _GSP;

        GSP(SP_EntityLookRelMove) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x += tpkt->dx;
            e->y += tpkt->dy;
            e->z += tpkt->dz;
        } _GSP;

        GSP(SP_EntityTeleport) {
            int idx = find_entity(tpkt->eid);
            if (idx<0) break;
            entity *e = P(gs.entity)+idx;
            e->x = tpkt->x;
            e->y = tpkt->y;
            e->z = tpkt->z;
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Player coordinates

        GSP(SP_PlayerPositionLook) {
            if (tpkt->flags != 0) {
                // more Mojang retardedness
                printf("SP_PlayerPositionLook with relative values, ignoring packet\n");
                break;
            }
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
        } _GSP;

        GSP(CP_Player) {
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerPosition) {
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerLook) {
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(CP_PlayerPositionLook) {
            gs.own.x     = (int)(tpkt->x*32);
            gs.own.y     = (int)(tpkt->y*32);
            gs.own.z     = (int)(tpkt->z*32);
            gs.own.yaw   = tpkt->yaw;
            gs.own.pitch = tpkt->pitch;
            gs.own.onground = tpkt->onground;
        } _GSP;

        GSP(SP_JoinGame) {
            change_dimension(tpkt->dimension);
        } _GSP;

        GSP(SP_Respawn) {
            change_dimension(tpkt->dimension);
        } _GSP;

        ////////////////////////////////////////////////////////////////
        // Chunks

        GSP(SP_ChunkData) {
            if (!tpkt->chunk.mask) {
                if (gs.opt.prune_chunks) {
                    remove_chunk(tpkt->chunk.X,tpkt->chunk.Z);
                }
            }
            else {
                insert_chunk(&tpkt->chunk);
            }
        } _GSP;

        GSP(SP_MapChunkBulk) {
            int i;
            for(i=0; i<tpkt->nchunks; i++)
                insert_chunk(&tpkt->chunk[i]);
        } _GSP;

        GSP(SP_BlockChange) {
            blkrec block = {
                .x = tpkt->pos.x&0xf,
                .z = tpkt->pos.z&0xf,
                .y = (uint8_t)tpkt->pos.y,
                .bid = tpkt->block,
            };
            modify_blocks(tpkt->pos.x>>4,tpkt->pos.z>>4,&block,1);
        } _GSP;

        GSP(SP_MultiBlockChange) {
            modify_blocks(tpkt->X,tpkt->Z,tpkt->blocks,tpkt->count);
        } _GSP;

    }
}

////////////////////////////////////////////////////////////////////////////////

void gs_reset() {
    int i;

    if (gs_used)
        gs_destroy();

    CLEAR(gs);

#if 0
    // set all items in the inventory to -1 to define them as empty
    for(i=0; i<64; i++) {
        gs.inventory.slots[i].id = 0xffff;
    }
    gs.drag.id = 0xffff;
#endif

    gs_used = 1;
}

void gs_destroy() {
    // delete tracked entities
    lh_free(P(gs.entity));

    //dump_chunks(&gs.overworld);

    free_chunks(&gs.overworld);
    free_chunks(&gs.nether);
    free_chunks(&gs.end);
}

int gs_setopt(int optid, int value) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            gs.opt.prune_chunks = value;
            break;
        case GSOP_SEARCH_SPAWNERS:
            gs.opt.search_spawners = value;
            break;
        case GSOP_TRACK_ENTITIES:
            gs.opt.track_entities = value;
            break;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }

    return 0;
}

int gs_getopt(int optid) {
    switch (optid) {
        case GSOP_PRUNE_CHUNKS:
            return gs.opt.prune_chunks;
        case GSOP_SEARCH_SPAWNERS:
            return gs.opt.search_spawners;
        case GSOP_TRACK_ENTITIES:
            return gs.opt.track_entities;

        default:
            LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }
}
