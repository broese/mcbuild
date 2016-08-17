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

////////////////////////////////////////////////////////////////////////////////
// String

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
#define Wmeta(n)    w=write_metadata(w, tpkt->n)

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

// Server -> Client

////////////////////////////////////////////////////////////////////////////////
// 0x00 SP_SpawnObject

DECODE_BEGIN(SP_SpawnObject,_1_9) {
    Pvarint(eid);
    Puuid(uuid);
    Pchar(objtype);
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pchar(pitch);
    Pchar(yaw);
    //TODO: object data
} DECODE_END;

DUMP_BEGIN(SP_SpawnObject) {
    printf("eid=%08x, objtype=%d, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f",
           tpkt->eid, tpkt->objtype, tpkt->x, tpkt->y, tpkt->z,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x11 SP_SpawnExperienceOrb

DECODE_BEGIN(SP_SpawnExperienceOrb,_1_9) {
    Pvarint(eid);
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pshort(count);
} DECODE_END;

DUMP_BEGIN(SP_SpawnExperienceOrb) {
    printf("eid=%08x, coord=%.1f,%.1f,%.1f, count=%d",
           tpkt->eid, tpkt->x, tpkt->y, tpkt->z, tpkt->count);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x03 SP_SpawnMob

DECODE_BEGIN(SP_SpawnMob,_1_9) {
    Pvarint(eid);
    Puuid(uuid);
    Pchar(mobtype);
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
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
           tpkt->x,tpkt->y,tpkt->z,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,(float)tpkt->headpitch/256,
           tpkt->vx,tpkt->vy,tpkt->vz);
    dump_metadata(tpkt->meta, tpkt->mobtype);
} DUMP_END;

FREE_BEGIN(SP_SpawnMob) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x04 SP_SpawnPainting

DECODE_BEGIN(SP_SpawnPainting,_1_9) {
    Pvarint(eid);
    Puuid(uuid);
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
// 0x05 SP_SpawnPlayer

DECODE_BEGIN(SP_SpawnPlayer,_1_9) {
    Pvarint(eid);
    Puuid(uuid);
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pchar(yaw);
    Pchar(pitch);
    Pmeta(meta);
} DECODE_END;

DUMP_BEGIN(SP_SpawnPlayer) {
    printf("eid=%08x, uuid=%s, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, item=%d",
           tpkt->eid, limhex(tpkt->uuid,16,16), tpkt->x, tpkt->y, tpkt->z,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->item);
    dump_metadata(tpkt->meta, Player);
} DUMP_END;

FREE_BEGIN(SP_SpawnPlayer) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x09 SP_UpdateBlockEntity

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
// 0x0a SP_BlockAction

DECODE_BEGIN(SP_BlockAction,_1_8_1) {
    Plong(loc.p);
    Pchar(b1);
    Pchar(b2);
    Pvarint(type);
} DECODE_END;

ENCODE_BEGIN(SP_BlockAction,_1_8_1) {
    Wlong(loc.p);
    Wchar(b1);
    Wchar(b2);
    Wvarint(type);
} ENCODE_END;

DUMP_BEGIN(SP_BlockAction) {
    char name[256];
    printf("pos=%d,%d,%d b1=%d b2=%d type=%d (%s)",
           tpkt->loc.x, tpkt->loc.y, tpkt->loc.z,
           tpkt->b1, tpkt->b2, tpkt->type,
           get_bid_name(name, BLOCKTYPE(tpkt->type, 0)));
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0b SP_BlockChange

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
// 0x0F SP_ChatMessage

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
// 0x10 SP_MultiBlockChange

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
// 0x11 SP_ConfirmTransaction

DECODE_BEGIN(SP_ConfirmTransaction,_1_8_1) {
    Pchar(wid);
    Pshort(aid);
    Pchar(accepted);
} DECODE_END;

DUMP_BEGIN(SP_ConfirmTransaction) {
    printf("wid=%d action=%d accepted=%d", tpkt->wid, tpkt->aid, tpkt->accepted);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x12 SP_CloseWindow

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
// 0x13 SP_OpenWindow

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
// 0x14 SP_WindowItems

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
// 0x16 SP_SetSlot

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
// 0x1c SP_Explosion

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
        b->dx = (int8_t)dx;
        b->dy = (int8_t)dy;
        b->dz = (int8_t)dz;
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
// 0x1d SP_UnloadChunk

DECODE_BEGIN(SP_UnloadChunk,_1_9) {
    Pint(X);
    Pint(Z);
} DECODE_END;

ENCODE_BEGIN(SP_UnloadChunk,_1_9) {
    Wint(X);
    Wint(Z);
} ENCODE_END;

DUMP_BEGIN(SP_UnloadChunk) {
    printf("chunk=%d,%d", tpkt->X, tpkt->Z);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x1e SP_ChangeGameState

DECODE_BEGIN(SP_ChangeGameState, _1_8_1) {
    Pchar(reason);
    Pfloat(value);
} DECODE_END;

ENCODE_BEGIN(SP_ChangeGameState, _1_8_1) {
    Wchar(reason);
    Wfloat(value);
} ENCODE_END;

DUMP_BEGIN(SP_ChangeGameState) {
    printf("reason=%d, value=%.1f", tpkt->reason, tpkt->value);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x20 SP_ChunkData

static int is_overworld = 1;

// Read a single 16x16x16 chunk section (aka "cube")
// Detailed format description: http://wiki.vg/SMP_Map_Format
static uint8_t * read_cube(uint8_t *p, cube_t *cube) {
    int i,j;
    int npal = -1;

    bid_t pal[4096];
    Rchar(nbits);
    if (nbits==0) { // raw 13-bit values, no palette
        nbits=13;
        npal=0;
    }
    uint64_t mask = ((1<<nbits)-1);

    // read the palette data, if available
    if ( npal<0 ) {
        npal = lh_read_varint(p);
        for(i=0; i<npal; i++) {
            pal[i].raw = (uint16_t)lh_read_varint(p);
            char mat[256];
            //printf("%2d : %03x:%2d (%s)\n", i, pal[i].bid, pal[i].meta, get_bid_name(mat, pal[i]));
        }
    }

    // check if the length of the data matches the expected amount
    Rvarint(nblocks);
    assert(lh_align(512*nbits, 8) == nblocks*8);

    // read block data, packed nbits palette indices
    int abits=0, idx=0;
    uint64_t adata=0;
    for(i=0; i<4096; i++) {
        // load more data from array if we don't have enough bits
        if (abits<nbits) {
            idx = adata; // save the remaining bits from adata
            adata = lh_read_long_be(p);
            idx |= (adata<<abits)&mask;
            adata >>= (nbits-abits);
            abits = 64-(nbits-abits);
        }
        else {
            idx = adata&mask;
            adata>>=nbits;
            abits-=nbits;
        }

        if (npal > 0) {
            assert(idx<npal);
            cube->blocks[i] = pal[idx];
        }
        else {
            cube->blocks[i].raw = idx;
        }
    }

    // read block light and skylight data
    memmove(cube->light, p, sizeof(cube->light));
    p += sizeof(cube->light);
    if (is_overworld) {
        memmove(cube->skylight, p, sizeof(cube->skylight));
        p += sizeof(cube->skylight);
    }

    return p;
}

DECODE_BEGIN(SP_ChunkData,_1_9_4) {
    Pint(chunk.X);
    Pint(chunk.Z);
    Pchar(cont);
    Pvarint(chunk.mask);
    assert(tpkt->chunk.mask <= 0xffff);
    Rvarint(size);

    int i,j;
    for(i=tpkt->chunk.mask,j=0; i; i>>=1,j++) {
        if (i&1) {
            lh_alloc_obj(tpkt->chunk.cubes[j]);
            p=read_cube(p, tpkt->chunk.cubes[j]);
        }
    }

    if (tpkt->cont) {
        memmove(tpkt->chunk.biome, p, 256);
        p+=256;
    }

    Rvarint(nte); // number of tile entities
    nbt_t *te = nbt_new(NBT_LIST, "TileEntities", 0);
    for(i=0; i<nte; i++) {
        nbt_t * tent = nbt_parse(&p);
        if (tent) {
            nbt_add(te, tent);
        }
    }
    tpkt->te = te;

    tpkt->skylight = is_overworld;
} DECODE_END;

uint8_t * write_cube(uint8_t *w, cube_t *cube) {
    int i;

    // construct the reverse palette - the index in this array
    // is the raw 13 bit block+meta value, the data is the
    // resulting palette index. -1 means this block+meta
    // does not occur in the cube
    int32_t rpal[8192];
    memset(rpal, 0xff, sizeof(rpal));
    int idx=1;
    rpal[0] = 0; // first index in the pallette is always Air
    for(i=0; i<4096; i++) {
        int32_t bid = cube->blocks[i].raw;
        if (rpal[bid] < 0) rpal[bid] = idx++;
    }

    // construct the forward palette
    int32_t pal[256];
    for(i=0; i<8192; i++)
        if (rpal[i]>=0)
            pal[rpal[i]] = i;

    // determine the necessary number of bits per block, to stay
    // compatible with notchian client (http://wiki.vg/SMP_Map_Format)
    int bpb = 4; // minimum number of bits per block
    while ( idx > (1<<bpb)) bpb++;
    if (bpb > 8) bpb = 13; // at more than 256 block types, just use unpalettized coding

    // write cube header
    lh_write_char(w, bpb);
    if (bpb<13) {
        lh_write_varint(w, idx);
        for(i=0; i<idx; i++)
            lh_write_varint(w, pal[i]);
    }
    else {
        lh_write_varint(w, 0);
    }
    int nlongs = lh_align(4096*bpb, 64)/64;
    lh_write_varint(w, nlongs);

    // write block data
    uint64_t data = 0;
    int nbits = 0;
    for(i=0; i<4096; i++) {
        uint64_t j = cube->blocks[i].raw;
        if (bpb<13) {
            j = rpal[j];
            assert(j>=0 && j<idx);
        }
        data |= (j<<nbits);
        nbits+=bpb;
        if (nbits >= 64) {
            lh_write_long_be(w, data);
            nbits -= 64;
            data = j>>(bpb-nbits);
        }
    }
    if (nbits > 0)
        lh_write_long_be(w, data);

    // write block light and skylight data
    memmove(w, cube->light, sizeof(cube->light));
    w += sizeof(cube->light);
    if (is_overworld) {
        memmove(w, cube->skylight, sizeof(cube->skylight));
        w += sizeof(cube->skylight);
    }

    return w;
}

ENCODE_BEGIN(SP_ChunkData,_1_9_4) {
    int i;

    Wint(chunk.X);
    Wint(chunk.Z);
    Wchar(cont);

    uint16_t mask = 0;
    for(i=0; i<16; i++)
        if (tpkt->chunk.cubes[i])
            mask |= (1<<i);
    lh_write_varint(w, mask);

    uint8_t cubes[256*1024];
    uint8_t *cw = cubes;

    for(i=0; i<16; i++)
        if (tpkt->chunk.cubes[i])
            cw = write_cube(cw, tpkt->chunk.cubes[i]);
    int32_t size = (int32_t)(cw-cubes);

    lh_write_varint(w, size+((tpkt->cont)?256:0));
    memmove(w, cubes, size);
    w+=size;

    if (tpkt->cont) {
        memmove(w, tpkt->chunk.biome, 256);
        w+=256;
    }

    assert(tpkt->te->type == NBT_LIST);
    assert(tpkt->te->ltype == NBT_COMPOUND || tpkt->te->count==0);
    lh_write_varint(w, tpkt->te->count);
    for(i=0; i<tpkt->te->count; i++)
        nbt_write(&w, tpkt->te->li[i]);
} ENCODE_END;

DUMP_BEGIN(SP_ChunkData) {
    printf("coord=%4d:%4d, cont=%d, skylight=%d, mask=%04x",
           tpkt->chunk.X, tpkt->chunk.Z, tpkt->cont,
           tpkt->skylight, tpkt->chunk.mask);
    nbt_dump(tpkt->te);
} DUMP_END;

FREE_BEGIN(SP_ChunkData) {
    int i;
    for(i=0; i<16; i++) {
        lh_free(tpkt->chunk.cubes[i]);
    }
    nbt_free(tpkt->te);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x21 SP_Effect

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
// 0x23 SP_JoinGame

DECODE_BEGIN(SP_JoinGame,_1_8_1) {
    Pint(eid);
    Pchar(gamemode);
    Pchar(dimension);
    Pchar(difficulty);
    Pchar(maxplayers);
    Pstr(leveltype);
    Pchar(reduced_debug_info);

    // track dimension changes - needed for correct SP_ChunkData decoding
    is_overworld = (tpkt->dimension == 0);
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
// 0x24 SP_Map

DECODE_BEGIN(SP_Map,_1_9) {
    Pvarint(mapid);
    Pchar(scale);
    Pchar(trackpos);
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

DUMP_BEGIN(SP_Map) {
    printf("id=%d, scale=%d, trackpos=%d, icons=%d, size=%d,%d, at=%d,%d, len=%d",
           tpkt->mapid, tpkt->scale, tpkt->trackpos, tpkt->nicons,
           tpkt->ncols, tpkt->nrows, tpkt->X, tpkt->Z, tpkt->len);
} DUMP_END;

FREE_BEGIN(SP_Map) {
    lh_free(tpkt->icons);
    lh_free(tpkt->data);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x25 SP_EntityRelMove

DECODE_BEGIN(SP_EntityRelMove,_1_9) {
    Pvarint(eid);
    Pshort(dx);
    Pshort(dy);
    Pshort(dz);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityRelMove) {
    printf("eid=%08x, delta=%.1f,%.1f,%.1f, onground=%d",tpkt->eid,
           (float)tpkt->dx/32,(float)tpkt->dy/32,(float)tpkt->dz/32,tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x26 SP_EntityLookRelMove

DECODE_BEGIN(SP_EntityLookRelMove,_1_9) {
    Pvarint(eid);
    Pshort(dx);
    Pshort(dy);
    Pshort(dz);
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
// 0x2b SP_PlayerAbilities

DECODE_BEGIN(SP_PlayerAbilities,_1_8_1) {
    Pchar(flags);
    Pfloat(speed);
    Pfloat(fov);
} DECODE_END;

ENCODE_BEGIN(SP_PlayerAbilities,_1_8_1) {
    Wchar(flags);
    Wfloat(speed);
    Wfloat(fov);
} ENCODE_END;

DUMP_BEGIN(SP_PlayerAbilities) {
    printf("flags=%02x, speed=%.1f, fov=%.1f",
           tpkt->flags, tpkt->speed, tpkt->fov);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2d SP_PlayerListItem

DECODE_BEGIN(SP_PlayerListItem,_1_9) {
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

ENCODE_BEGIN(SP_PlayerListItem,_1_9) {
    Wvarint(action);
    int np = C(tpkt->list);
    lh_write_varint(w, np);

    int i,j;
    for(i=0; i<np; i++) {
        pli_t * entry = P(tpkt->list)+i;
        memmove(w, entry->uuid,16); w+=16;

        switch(tpkt->action) {
            case 1: { // update gamemode
                lh_write_varint(w,entry->gamemode);
                break;
            }
            default:
                assert(0);
                break;
        }
    }
} ENCODE_END;

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
// 0x2e SP_PlayerPositionLook

DECODE_BEGIN(SP_PlayerPositionLook,_1_9) {
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pfloat(yaw);
    Pfloat(pitch);
    Pchar(flags);
    Pvarint(tpid);
} DECODE_END;

ENCODE_BEGIN(SP_PlayerPositionLook,_1_9) {
    Wdouble(x);
    Wdouble(y);
    Wdouble(z);
    Wfloat(yaw);
    Wfloat(pitch);
    Wchar(flags);
    Wvarint(tpid);
} ENCODE_END;

DUMP_BEGIN(SP_PlayerPositionLook) {
    printf("coord=%.1f,%.1f,%.1f rot=%.1f,%.1f flags=%02x tpid=%08x",
           tpkt->x,tpkt->y,tpkt->z,tpkt->yaw,tpkt->pitch,tpkt->flags,tpkt->tpid);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2f SP_UseBed

DECODE_BEGIN(SP_UseBed,_1_9) {
    Pvarint(eid);
    Plong(pos.p);
} DECODE_END;

ENCODE_BEGIN(SP_UseBed,_1_9) {
    Wvarint(eid);
    Wlong(pos.p);
} ENCODE_END;

DUMP_BEGIN(SP_UseBed) {
    printf("eid=%d, pos=%d,%d,%d", tpkt->eid, tpkt->pos.x, tpkt->pos.y, tpkt->pos.z);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x30 SP_DestroyEntities

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
// 0x33 SP_Respawn

DECODE_BEGIN(SP_Respawn,_1_8_1) {
    uint8_t *pp = p;

    Pint(dimension);
    Pchar(difficulty);
    Pchar(gamemode);

    // workaround for a buggy encoding in Spigot 1.10
    if (!(tpkt->dimension>=-1 && tpkt->dimension<=1)) {
        printf("Warning: applying Spigot 1.10 workaround! reported dimension=%d\n", tpkt->dimension);
        p = pp;
        Pshort(dimension);
        Pshort(difficulty);
        Pshort(gamemode);
    }

    Pstr(leveltype);

    // track dimension changes - needed for correct SP_ChunkData decoding
    is_overworld = (tpkt->dimension == 0);
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
// 0x37 SP_HeldItemChange

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
// 0x39 SP_EntityMetadata

DECODE_BEGIN(SP_EntityMetadata,_1_8_1) {
    Pvarint(eid);
    Pmeta(meta);
} DECODE_END;

ENCODE_BEGIN(SP_EntityMetadata,_1_8_1) {
    Wvarint(eid);
    Wmeta(meta);
} ENCODE_END;

DUMP_BEGIN(SP_EntityMetadata) {
    printf("eid=%08x", tpkt->eid);
    // unfortunately we don't have access to proper entity type here
    dump_metadata(tpkt->meta, Entity);
} DUMP_END;

FREE_BEGIN(SP_EntityMetadata) {
    free_metadata(tpkt->meta);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x3d SP_SetExperience

DECODE_BEGIN(SP_SetExperience,_1_8_1) {
    Pfloat(bar);
    Pvarint(level);
    Pvarint(exp);
} DECODE_END;

DUMP_BEGIN(SP_SetExperience) {
    printf("bar=%.2f level=%d exp=%d",tpkt->bar, tpkt->level, tpkt->exp);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x3e SP_UpdateHealth

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
// 0x46 SP_SoundEffect

DECODE_BEGIN(SP_SoundEffect,_1_10) {
    Pvarint(id);
    Pvarint(category);
    Pint(x);
    Pint(y);
    Pint(z);
    Pfloat(vol);
    Pfloat(pitch);
} DECODE_END;

DUMP_BEGIN(SP_SoundEffect) {
    printf("id=%d, category=%d, coord=%.1f,%.1f,%.1f, vol=%.2f, pitch=%.2f",
           tpkt->id, tpkt->category,
           (float)tpkt->x/8,(float)tpkt->y/8,(float)tpkt->z/8,
           tpkt->vol, tpkt->pitch);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x49 SP_EntityTeleport

DECODE_BEGIN(SP_EntityTeleport,_1_9) {
    Pvarint(eid);
    Pdouble(x);
    Pdouble(y);
    Pdouble(z);
    Pchar(yaw);
    Pchar(pitch);
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(SP_EntityTeleport) {
    printf("eid=%08x, coord=%.1f,%.1f,%.1f, rot=%.1f,%.1f, onground=%d",tpkt->eid,
           tpkt->x, tpkt->y, tpkt->z,
           (float)tpkt->yaw/256,(float)tpkt->pitch/256,tpkt->onground);
} DUMP_END;






////////////////////////////////////////////////////////////////////////////////
// Client -> Server

////////////////////////////////////////////////////////////////////////////////
// 0x00 CP_TeleportConfirm

DECODE_BEGIN(CP_TeleportConfirm,_1_9) {
    Pvarint(tpid);
} DECODE_END;

ENCODE_BEGIN(CP_TeleportConfirm,_1_9) {
    Wvarint(tpid);
} ENCODE_END;

DUMP_BEGIN(CP_TeleportConfirm) {
    printf("tpid=%d", tpkt->tpid);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x02 CP_ChatMessage

DECODE_BEGIN(CP_ChatMessage,_1_8_1) {
    Pstr(str);
} DECODE_END;

DUMP_BEGIN(CP_ChatMessage) {
    printf("str=%s",tpkt->str);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x07 CP_ClickWindow

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
// 0x08 CP_CloseWindow

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
// 0x0a CP_UseEntity

DECODE_BEGIN(CP_UseEntity,_1_9) {
    Pvarint(target);
    Pvarint(action);
    switch(tpkt->action) {
        case 0:
            Pvarint(hand);
            break;
        case 2:
            Pfloat(x);
            Pfloat(y);
            Pfloat(z);
            Pvarint(hand);
            break;
    }
} DECODE_END;

ENCODE_BEGIN(CP_UseEntity,_1_9) {
    Wvarint(target);
    Wvarint(action);
    switch(tpkt->action) {
        case 0:
            Wvarint(hand);
            break;
        case 2:
            Wfloat(x);
            Wfloat(y);
            Wfloat(z);
            Wvarint(hand);
            break;
    }
} ENCODE_END;

DUMP_BEGIN(CP_UseEntity) {
    printf("target=%08x action=%d", tpkt->target,tpkt->action);
    switch(tpkt->action) {
        case 0:
            printf(" hand=%d", tpkt->hand);
            break;
        case 2:
            printf(" coord=%.1f,%.1f,%.1f hand=%d",
                   tpkt->x,tpkt->y,tpkt->z,tpkt->hand);
            break;
    }
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0c CP_PlayerPosition

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
// 0x0d CP_PlayerPositionLook

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
// 0x0e CP_PlayerLook

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
// 0x0f CP_Player

DECODE_BEGIN(CP_Player,_1_8_1) {
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(CP_Player) {
    printf("onground=%d",tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x13 CP_PlayerDigging

DECODE_BEGIN(CP_PlayerDigging,_1_9) {
    Pvarint(status);
    Plong(loc.p);
    Pchar(face);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerDigging,_1_9) {
    Wvarint(status);
    Wlong(loc.p);
    Wchar(face);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerDigging) {
    printf("status=%d, location=%d,%d,%d, face=%d",
           tpkt->status, tpkt->loc.x, tpkt->loc.y, tpkt->loc.z, tpkt->face);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x14 CP_EntityAction

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
// 0x17 CP_HeldItemChange

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
// 0x1a CP_Animation

DECODE_BEGIN(CP_Animation,_1_9) {
    Pvarint(hand);
} DECODE_END;

ENCODE_BEGIN(CP_Animation,_1_9) {
    Wvarint(hand);
} ENCODE_END;

DUMP_BEGIN(CP_Animation) {
    printf("hand=%d", tpkt->hand);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x1c CP_PlayerBlockPlacement

DECODE_BEGIN(CP_PlayerBlockPlacement,_1_9) {
    Plong(bpos.p);
    Pvarint(face);
    Pvarint(hand);
    Pchar(cx);
    Pchar(cy);
    Pchar(cz);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerBlockPlacement,_1_9) {
    Wlong(bpos.p);
    Wvarint(face);
    Wvarint(hand);
    Wchar(cx);
    Wchar(cy);
    Wchar(cz);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerBlockPlacement) {
    printf("bpos=%d,%d,%d, face=%d, cursor=%d,%d,%d, hand=%d",
           tpkt->bpos.x,  tpkt->bpos.y,  tpkt->bpos.z,
           tpkt->face, tpkt->cx, tpkt->cy, tpkt->cz, tpkt->hand);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x1d CP_UseItem

DECODE_BEGIN(CP_UseItem,_1_9) {
    Pvarint(hand);
} DECODE_END;

ENCODE_BEGIN(CP_UseItem,_1_9) {
    Wvarint(hand);
} ENCODE_END;

DUMP_BEGIN(CP_UseItem) {
    printf("hand=%d", tpkt->hand);
} DUMP_END;







////////////////////////////////////////////////////////////////////////////////

const static packet_methods SUPPORT_1_9[2][MAXPACKETTYPES] = {
    {
        SUPPORT_D   (SP_SpawnObject,_1_9),          // 00
        SUPPORT_D   (SP_SpawnExperienceOrb,_1_9),   // 01
        SUPPORT_DF  (SP_SpawnMob,_1_9),             // 03
        SUPPORT_D   (SP_SpawnPainting,_1_9),        // 04
        SUPPORT_DF  (SP_SpawnPlayer,_1_9),          // 05
        SUPPORT_DF  (SP_UpdateBlockEntity,_1_8_1),  // 09
        SUPPORT_DE  (SP_BlockAction,_1_8_1),        // 0a
        SUPPORT_DE  (SP_BlockChange,_1_8_1),        // 0b
        SUPPORT_DEF (SP_ChatMessage,_1_8_1),        // 0f
        SUPPORT_DEF (SP_MultiBlockChange,_1_8_1),   // 10
        SUPPORT_D   (SP_ConfirmTransaction,_1_8_1), // 11
        SUPPORT_DE  (SP_CloseWindow,_1_8_1),        // 12
        SUPPORT_DEF (SP_OpenWindow,_1_8_1),         // 13
        SUPPORT_DEF (SP_WindowItems,_1_8_1),        // 14
        SUPPORT_DEF (SP_SetSlot,_1_8_1),            // 16
        SUPPORT_DF  (SP_Explosion,_1_8_1),          // 1c
        SUPPORT_DE  (SP_UnloadChunk,_1_9),          // 1d
        SUPPORT_DE  (SP_ChangeGameState,_1_8_1),    // 1e
        SUPPORT_DEF (SP_ChunkData,_1_9_4),          // 20
        SUPPORT_D   (SP_Effect,_1_8_1),             // 21
        SUPPORT_D   (SP_JoinGame,_1_8_1),           // 23
        SUPPORT_DF  (SP_Map,_1_9),                  // 24
        SUPPORT_D   (SP_EntityRelMove,_1_9),        // 25
        SUPPORT_D   (SP_EntityLookRelMove,_1_9),    // 26
        SUPPORT_DE  (SP_PlayerAbilities,_1_8_1),    // 2b
        SUPPORT_DEF (SP_PlayerListItem,_1_9),       // 2d
        SUPPORT_DE  (SP_PlayerPositionLook,_1_9),   // 2e
        SUPPORT_DE  (SP_UseBed,_1_9),               // 2f
        SUPPORT_DF  (SP_DestroyEntities,_1_8_1),    // 30
        SUPPORT_D   (SP_Respawn,_1_8_1),            // 33
        SUPPORT_DE  (SP_HeldItemChange,_1_8_1),     // 37
        SUPPORT_DEF (SP_EntityMetadata,_1_8_1),     // 39
        SUPPORT_D   (SP_SetExperience,_1_8_1),      // 3d
        SUPPORT_D   (SP_UpdateHealth,_1_8_1),       // 3e
        SUPPORT_D   (SP_SoundEffect,_1_10),         // 46
        SUPPORT_D   (SP_EntityTeleport,_1_9),       // 49
    },
    {
        SUPPORT_DE  (CP_TeleportConfirm,_1_9),      // 00
        SUPPORT_D   (CP_ChatMessage,_1_8_1),        // 02
        SUPPORT_DEF (CP_ClickWindow,_1_8_1),        // 07
        SUPPORT_DE  (CP_CloseWindow,_1_8_1),        // 08
        SUPPORT_DE  (CP_UseEntity,_1_9),            // 0a
        SUPPORT_D   (CP_PlayerPosition,_1_8_1),     // 0c
        SUPPORT_DE  (CP_PlayerPositionLook,_1_8_1), // 0d
        SUPPORT_DE  (CP_PlayerLook,_1_8_1),         // 0e
        SUPPORT_D   (CP_Player,_1_8_1),             // 0f
        SUPPORT_DE  (CP_PlayerDigging,_1_9),        // 13
        SUPPORT_DE  (CP_EntityAction,_1_8_1),       // 14
        SUPPORT_DE  (CP_HeldItemChange,_1_8_1),     // 17
        SUPPORT_DE  (CP_Animation,_1_9),            // 1a
        SUPPORT_DE  (CP_PlayerBlockPlacement,_1_9), // 1c
        SUPPORT_DE  (CP_UseItem,_1_9),              // 1d
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

uint8_t * encode_loginstart(CL_LoginStart_pkt *tpkt, uint8_t *w) {
    Wstr(username);
    return w;
}

void decode_encryption_request(SL_EncryptionRequest_pkt *tpkt, uint8_t *p) {
    Pstr(serverID);
    Pvarint(klen);
    Pdata(pkey,tpkt->klen);
    Pvarint(tlen);
    Pdata(token,tpkt->tlen);
}

uint8_t * encode_encryption_response(CL_EncryptionResponse_pkt *tpkt, uint8_t *w) {
    Wvarint(sklen);
    Wdata(skey,tpkt->sklen);
    Wvarint(tklen);
    Wdata(token,tpkt->tklen);
}

void decode_encryption_response(CL_EncryptionResponse_pkt *tpkt, uint8_t *p) {
    Pvarint(sklen);
    Pdata(skey,tpkt->sklen);
    Pvarint(tklen);
    Pdata(token,tpkt->tklen);
}








////////////////////////////////////////////////////////////////////////////////

#define SUPPORT SUPPORT_1_9

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
