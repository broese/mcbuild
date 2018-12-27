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
#include <math.h>

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
    int     pid;
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
#define SUPPORT_DEDF(id,name,version)                                          \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        encode_##name##version,                                                \
        dump_##name,                                                           \
        free_##name,                                                           \
        #name,                                                                 \
        name,                                                                  \
    }

// decode, dump and free
#define SUPPORT_DDF(id,name,version)                                           \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        NULL,                                                                  \
        dump_##name,                                                           \
        free_##name,                                                           \
        #name,                                                                 \
        name,                                                                  \
    }

// decode, encode and free
#define SUPPORT_DEF(id,name,version)                                           \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        encode_##name##version,                                                \
        NULL,                                                                  \
        free_##name,                                                           \
        #name,                                                                 \
        name,                                                                  \
    }

// decode and free
#define SUPPORT_DF(id,name,version)                                            \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        NULL,                                                                  \
        NULL,                                                                  \
        free_##name,                                                           \
        #name,                                                                 \
        name,                                                                  \
    }

// decode, encode and dump
#define SUPPORT_DED(id,name,version)                                           \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        encode_##name##version,                                                \
        dump_##name,                                                           \
        NULL,                                                                  \
        #name,                                                                 \
        name,                                                                  \
    }

// decode and dump
#define SUPPORT_DD(id,name,version)                                            \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        NULL,                                                                  \
        dump_##name,                                                           \
        NULL,                                                                  \
        #name,                                                                 \
        name,                                                                  \
    }

// decode and encode
#define SUPPORT_DE(id,name,version)                                            \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        encode_##name##version,                                                \
        NULL,                                                                  \
        NULL,                                                                  \
        #name,                                                                 \
        name,                                                                  \
    }

// decode only
#define SUPPORT_D(id,name,version)                                             \
    [id] = {                                                                   \
        decode_##name##version,                                                \
        NULL,                                                                  \
        NULL,                                                                  \
        NULL,                                                                  \
        #name,                                                                 \
        name,                                                                  \
    }

// no supported methods
#define SUPPORT_(id,name)                                                      \
    [id] = {                                                                   \
        NULL,                                                                  \
        NULL,                                                                  \
        NULL,                                                                  \
        NULL,                                                                  \
        #name,                                                                 \
        name,                                                                  \
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
// 0x01 SP_SpawnExperienceOrb

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

DECODE_BEGIN(SP_SpawnMob,_1_11) {
    Pvarint(eid);
    Puuid(uuid);
    Pvarint(mobtype);
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
// 0x0B SP_BlockChange

// Note: the legacy 1.8.1 decoder would still work with 1.13, but the block ID
// value now should be interpreted only as raw 16-bit value, ignoring .bid and
// .meta values still present in the struct
// atm there are ~8000 IDs defined, so 16 bits should be sufficient
//TODO: redefine bid_t as uint16_t
DECODE_BEGIN(SP_BlockChange,_1_13) {
    Plong(pos.p);
    Rvarint(bid);
    tpkt->block.raw = (uint16_t)bid;
} DECODE_END;

ENCODE_BEGIN(SP_BlockChange,_1_13) {
    Wlong(pos.p);
    Wvarint(block.raw);
} ENCODE_END;

DUMP_BEGIN(SP_BlockChange) {
    printf("coord=%d,%d,%d bid=%x(%d)",
           tpkt->pos.x, tpkt->pos.y, tpkt->pos.z,
           tpkt->block.raw, tpkt->block.raw);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x0E SP_ChatMessage

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
// 0x0F SP_MultiBlockChange

DECODE_BEGIN(SP_MultiBlockChange,_1_13) {
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

ENCODE_BEGIN(SP_MultiBlockChange,_1_13) {
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
        printf("\n    coord=%d,%d,%d bid=%x(%d)",
               b->x,b->z,b->y,b->bid.raw,b->bid.raw);
    }
} DUMP_END;

FREE_BEGIN(SP_MultiBlockChange) {
    lh_free(tpkt->blocks);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x12 SP_ConfirmTransaction

DECODE_BEGIN(SP_ConfirmTransaction,_1_8_1) {
    Pchar(wid);
    Pshort(aid);
    Pchar(accepted);
} DECODE_END;

DUMP_BEGIN(SP_ConfirmTransaction) {
    printf("wid=%d action=%d accepted=%d", tpkt->wid, tpkt->aid, tpkt->accepted);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x13 SP_CloseWindow

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
// 0x14 SP_OpenWindow

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
// 0x15 SP_WindowItems

DECODE_BEGIN(SP_WindowItems,_1_8_1) {
    Pchar(wid);
    Pshort(count);
    lh_alloc_num(tpkt->slots, tpkt->count);
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
    lh_free(tpkt->slots);
} FREE_END;

////////////////////////////////////////////////////////////////////////////////
// 0x17 SP_SetSlot

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
// 0x1e SP_Explosion

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
// 0x1f SP_UnloadChunk

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
// 0x20 SP_ChangeGameState

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
// 0x22 SP_ChunkData

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

DECODE_BEGIN(SP_ChunkData,_1_9) {
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

    if (tpkt->cont)
        memmove(tpkt->chunk.biome, p, 256);

    // to make it compatible with the new packet format,
    // add an empty tile entities list
    tpkt->te = nbt_new(NBT_LIST, "TileEntities", 0);

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
    for(i=0; i<tpkt->te->count; i++) {
        tpkt->te->li[i]->name = ""; // Tile Entity compounds must have name
        nbt_write(&w, tpkt->te->li[i]);
        tpkt->te->li[i]->name = NULL;
    }
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
// 0x23 SP_Effect

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
// 0x25 SP_JoinGame

DECODE_BEGIN(SP_JoinGame,_1_8_1) {
    Pint(eid);
    Pchar(gamemode);
    Rchar(dimension);
    tpkt->dimension = (int32_t)((char)dimension);
    Pchar(difficulty);
    Pchar(maxplayers);
    Pstr(leveltype);
    Pchar(reduced_debug_info);

    // track dimension changes - needed for correct SP_ChunkData decoding
    is_overworld = (tpkt->dimension == 0);
} DECODE_END;

DECODE_BEGIN(SP_JoinGame,_1_9_2) {
    Pint(eid);
    Pchar(gamemode);
    Pint(dimension);
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
// 0x26 SP_Map

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

ENCODE_BEGIN(SP_Map,_1_9) {
    Wvarint(mapid);
    Wchar(scale);
    Wchar(trackpos);
    Wvarint(nicons);

    int i;
    for(i=0; i<tpkt->nicons; i++) {
        Wchar(icons[i].type);
        Wchar(icons[i].x);
        Wchar(icons[i].z);
    }
    Wchar(ncols);
    if (tpkt->ncols > 0) {
        Wchar(nrows);
        Wchar(X);
        Wchar(Z);
        Wvarint(len);
        Wdata(data, tpkt->len);
    }
} ENCODE_END;

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
// 0x28 SP_EntityRelMove

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
// 0x29 SP_EntityLookRelMove

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
// 0x2e SP_PlayerAbilities

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
// 0x30 SP_PlayerListItem

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
// 0x32 SP_PlayerPositionLook

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
// 0x33 SP_UseBed

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
// 0x35 SP_DestroyEntities

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
// 0x38 SP_Respawn

DECODE_BEGIN(SP_Respawn,_1_8_1) {
    Pint(dimension);
    Pchar(difficulty);
    Pchar(gamemode);

    // workaround for different world IDs leaking in TotalFreedom mod
    if (!(tpkt->dimension>=-1 && tpkt->dimension<=1)) {
        printf("Warning: applying TotalFreedom/Spigot 1.10 workaround! reported dimension=%d\n", tpkt->dimension);
        tpkt->dimension = 0;
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
// 0x3d SP_HeldItemChange

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
// 0x3f SP_EntityMetadata

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
// 0x43 SP_SetExperience

DECODE_BEGIN(SP_SetExperience,_1_8_1) {
    Pfloat(bar);
    Pvarint(level);
    Pvarint(exp);
} DECODE_END;

DUMP_BEGIN(SP_SetExperience) {
    printf("bar=%.2f level=%d exp=%d",tpkt->bar, tpkt->level, tpkt->exp);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x44 SP_UpdateHealth

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
// 0x4d SP_SoundEffect

DECODE_BEGIN(SP_SoundEffect,_1_10) {
    Pvarint(id);
    Pvarint(category);
    Pint(x);
    Pint(y);
    Pint(z);
    Pfloat(vol);
    Pfloat(pitch);
} DECODE_END;

DECODE_BEGIN(SP_SoundEffect,_1_9) {
    Pvarint(id);
    Pvarint(category);
    Pint(x);
    Pint(y);
    Pint(z);
    Pfloat(vol);
    Rchar(pitch);
    tpkt->pitch = (float)pitch / 63.5;
} DECODE_END;

ENCODE_BEGIN(SP_SoundEffect,_1_10) {
    Wvarint(id);
    Wvarint(category);
    Wint(x);
    Wint(y);
    Wint(z);
    Wfloat(vol);
    Wfloat(pitch);
} ENCODE_END;

ENCODE_BEGIN(SP_SoundEffect,_1_9) {
    Wvarint(id);
    Wvarint(category);
    Wint(x);
    Wint(y);
    Wint(z);
    Wfloat(vol);
    lh_write_char(w,(char)(tpkt->pitch*63.5));
} ENCODE_END;

DUMP_BEGIN(SP_SoundEffect) {
    printf("id=%d, category=%d, coord=%.1f,%.1f,%.1f, vol=%.2f, pitch=%.2f",
           tpkt->id, tpkt->category,
           (float)tpkt->x/8,(float)tpkt->y/8,(float)tpkt->z/8,
           tpkt->vol, tpkt->pitch);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x50 SP_EntityTeleport

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
// SP_UpdateSign - removed in 1.9.4

DECODE_BEGIN(SP_UpdateSign,_1_8_1) {
    Plong(pos.p);
    Pstr(line1);
    Pstr(line2);
    Pstr(line3);
    Pstr(line4);
} DECODE_END;

DUMP_BEGIN(SP_UpdateSign) {
    printf("pos=%d,%d,%d, line1=\"%s\", line2=\"%s\", line3=\"%s\", line4=\"%s\"",
           tpkt->pos.x,tpkt->pos.y,tpkt->pos.z,
           tpkt->line1,tpkt->line2,tpkt->line3,tpkt->line4);
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
// 0x08 CP_ClickWindow

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
// 0x09 CP_CloseWindow

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
// 0x0d CP_UseEntity

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
// 0x0f CP_Player

DECODE_BEGIN(CP_Player,_1_8_1) {
    Pchar(onground);
} DECODE_END;

DUMP_BEGIN(CP_Player) {
    printf("onground=%d",tpkt->onground);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x10 CP_PlayerPosition

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
// 0x11 CP_PlayerPositionLook

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
// 0x12 CP_PlayerLook

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
// 0x18 CP_PlayerDigging

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
// 0x19 CP_EntityAction

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
// 0x21 CP_HeldItemChange

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
// 0x27 CP_Animation

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
// 0x29 CP_PlayerBlockPlacement

DECODE_BEGIN(CP_PlayerBlockPlacement,_1_11) {
    Plong(bpos.p);
    Pvarint(face);
    Pvarint(hand);
    Pfloat(cx);
    Pfloat(cy);
    Pfloat(cz);
} DECODE_END;

ENCODE_BEGIN(CP_PlayerBlockPlacement,_1_11) {
    Wlong(bpos.p);
    Wvarint(face);
    Wvarint(hand);
    Wfloat(cx);
    Wfloat(cy);
    Wfloat(cz);
} ENCODE_END;

DUMP_BEGIN(CP_PlayerBlockPlacement) {
    printf("bpos=%d,%d,%d, face=%d, cursor=%.2f,%.2f,%.2f, hand=%d",
           tpkt->bpos.x,  tpkt->bpos.y,  tpkt->bpos.z,
           tpkt->face, tpkt->cx, tpkt->cy, tpkt->cz, tpkt->hand);
} DUMP_END;

////////////////////////////////////////////////////////////////////////////////
// 0x2a CP_UseItem

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
// Packet ID mapping to packet handlers

// Note: keep entries for unsupported packets (SUPPORT_) in the table
// they are needed to properly look up names for raw packets in dump_packet()
// and in case the ID has shifted or was removed between the versions

// MC protocol v393/v403/v404 - clients 1.13/1.13.1/1.13.2
// https://wiki.vg/index.php?title=Pre-release_protocol&oldid=14150
const static packet_methods SUPPORT_1_13[2][MAXPACKETTYPES] = {
    {
        SUPPORT_    (0x00,SP_SpawnObject),
        SUPPORT_    (0x01,SP_SpawnExperienceOrb),
        SUPPORT_    (0x02,SP_SpawnGlobalEntity),
        SUPPORT_    (0x03,SP_SpawnMob),
        SUPPORT_    (0x04,SP_SpawnPainting),
        SUPPORT_    (0x05,SP_SpawnPlayer),
        SUPPORT_    (0x06,SP_Animation),
        SUPPORT_    (0x07,SP_Statistics),
        SUPPORT_    (0x08,SP_BlockBreakAnimation),
        SUPPORT_    (0x09,SP_UpdateBlockEntity),
        SUPPORT_    (0x0a,SP_BlockAction),
        SUPPORT_DED (0x0b,SP_BlockChange,_1_13),
        SUPPORT_    (0x0c,SP_BossBar),
        SUPPORT_    (0x0d,SP_ServerDifficulty),
        SUPPORT_    (0x0e,SP_ChatMessage),
        SUPPORT_DEDF(0x0f,SP_MultiBlockChange,_1_13),

        SUPPORT_    (0x10,SP_TabComplete),
        SUPPORT_    (0x11,SP_DeclareCommands),
        SUPPORT_    (0x12,SP_ConfirmTransaction),
        SUPPORT_    (0x13,SP_CloseWindow),
        SUPPORT_    (0x14,SP_OpenWindow),
        SUPPORT_    (0x15,SP_WindowItems),
        SUPPORT_    (0x16,SP_WindowProperty),
        SUPPORT_    (0x17,SP_SetSlot),
        SUPPORT_    (0x18,SP_SetCooldown),
        SUPPORT_    (0x19,SP_PluginMessage),
        SUPPORT_    (0x1a,SP_NamedSoundEffect),
        SUPPORT_    (0x1b,SP_Disconnect),
        SUPPORT_    (0x1c,SP_EntityStatus),
        SUPPORT_    (0x1d,SP_NbtQueryResponse),
        SUPPORT_DDF (0x1e,SP_Explosion,_1_8_1),
        SUPPORT_DED (0x1f,SP_UnloadChunk,_1_9),

        SUPPORT_    (0x20,SP_ChangeGameState),
        SUPPORT_    (0x21,SP_KeepAlive),
        SUPPORT_    (0x22,SP_ChunkData),
        SUPPORT_    (0x23,SP_Effect),
        SUPPORT_    (0x24,SP_Particle),
        SUPPORT_    (0x25,SP_JoinGame),
        SUPPORT_    (0x26,SP_Map),
        SUPPORT_    (0x27,SP_Entity),
        SUPPORT_    (0x28,SP_EntityRelMove),
        SUPPORT_    (0x29,SP_EntityLookRelMove),
        SUPPORT_    (0x2a,SP_EntityLook),
        SUPPORT_    (0x2b,SP_VehicleMove),
        SUPPORT_    (0x2c,SP_OpenSignEditor),
        SUPPORT_    (0x2d,SP_CraftRecipeResponse),
        SUPPORT_    (0x2e,SP_PlayerAbilities),
        SUPPORT_    (0x2f,SP_CombatEffect),

        SUPPORT_    (0x30,SP_PlayerListItem),
        SUPPORT_    (0x31,SP_FacePlayer),
        SUPPORT_DED (0x32,SP_PlayerPositionLook,_1_9),
        SUPPORT_    (0x33,SP_UseBed),
        SUPPORT_    (0x34,SP_UnlockRecipes),
        SUPPORT_    (0x35,SP_DestroyEntities),
        SUPPORT_    (0x36,SP_RemoveEntityEffect),
        SUPPORT_    (0x37,SP_ResourcePackSent),
        SUPPORT_    (0x38,SP_Respawn),
        SUPPORT_    (0x39,SP_EntityHeadLook),
        SUPPORT_    (0x3a,SP_SelectAdvancementTab),
        SUPPORT_    (0x3b,SP_WorldBorder),
        SUPPORT_    (0x3c,SP_Camera),
        SUPPORT_    (0x3d,SP_HeldItemChange),
        SUPPORT_    (0x3e,SP_DisplayScoreboard),
        SUPPORT_    (0x3f,SP_EntityMetadata),

        SUPPORT_    (0x40,SP_AttachEntity),
        SUPPORT_    (0x41,SP_EntityVelocity),
        SUPPORT_    (0x42,SP_EntityEquipment),
        SUPPORT_    (0x43,SP_SetExperience),
        SUPPORT_    (0x44,SP_UpdateHealth),
        SUPPORT_    (0x45,SP_ScoreboardObjective),
        SUPPORT_    (0x46,SP_SetPassengers),
        SUPPORT_    (0x47,SP_Teams),
        SUPPORT_    (0x48,SP_UpdateScore),
        SUPPORT_    (0x49,SP_SpawnPosition),
        SUPPORT_    (0x4a,SP_TimeUpdate),
        SUPPORT_    (0x4b,SP_Title),
        SUPPORT_    (0x4c,SP_StopSound),
        SUPPORT_    (0x4d,SP_SoundEffect),
        SUPPORT_    (0x4e,SP_PlayerListHeader),
        SUPPORT_    (0x4f,SP_CollectItem),

        SUPPORT_    (0x50,SP_EntityTeleport),
        SUPPORT_    (0x51,SP_Advancements),
        SUPPORT_    (0x52,SP_EntityProperties),
        SUPPORT_    (0x53,SP_EntityEffect),
        SUPPORT_    (0x54,SP_DeclareRecipes),
        SUPPORT_    (0x55,SP_Tags),
        SUPPORT_    (0x60,SP___),
    },
    {
        SUPPORT_    (0x00,CP_TeleportConfirm),
        SUPPORT_    (0x01,CP_QueryBlockNbt),
        SUPPORT_    (0x02,CP_ChatMessage),
        SUPPORT_    (0x03,CP_ClientStatus),
        SUPPORT_    (0x04,CP_ClientSettings),
        SUPPORT_    (0x05,CP_TabComplete),
        SUPPORT_    (0x06,CP_ConfirmTransaction),
        SUPPORT_    (0x07,CP_EnchantItem),
        SUPPORT_    (0x08,CP_ClickWindow),
        SUPPORT_    (0x09,CP_CloseWindow),
        SUPPORT_    (0x0a,CP_PluginMessage),
        SUPPORT_    (0x0b,CP_EditBook),
        SUPPORT_    (0x0c,CP_QueryEntityNbt),
        SUPPORT_    (0x0d,CP_UseEntity),
        SUPPORT_    (0x0e,CP_KeepAlive),
        SUPPORT_DD  (0x0f,CP_Player,_1_8_1),

        SUPPORT_DD  (0x10,CP_PlayerPosition,_1_8_1),
        SUPPORT_    (0x11,CP_PlayerPositionLook),
        SUPPORT_    (0x12,CP_PlayerLook),
        SUPPORT_    (0x13,CP_VehicleMove),
        SUPPORT_    (0x14,CP_SteerBoat),
        SUPPORT_    (0x15,CP_PickItem),
        SUPPORT_    (0x16,CP_CraftRecipeRequest),
        SUPPORT_    (0x17,CP_PlayerAbilities),
        SUPPORT_    (0x18,CP_PlayerDigging),
        SUPPORT_    (0x19,CP_EntityAction),
        SUPPORT_    (0x1a,CP_SteerVehicle),
        SUPPORT_    (0x1b,CP_RecipeBookData),
        SUPPORT_    (0x1c,CP_NameItem),
        SUPPORT_    (0x1d,CP_ResourcePackStatus),
        SUPPORT_    (0x1e,CP_AdvancementTab),
        SUPPORT_    (0x1f,CP_SelectTrade),

        SUPPORT_    (0x20,CP_SetBeaconEffect),
        SUPPORT_    (0x21,CP_HeldItemChange),
        SUPPORT_    (0x22,CP_UpdateCommandBlock),
        SUPPORT_    (0x23,CP_UpdateCmdMinecart),
        SUPPORT_    (0x24,CP_CreativeInventoryAct),
        SUPPORT_    (0x25,CP_UpdateStructureBlock),
        SUPPORT_    (0x26,CP_UpdateSign),
        SUPPORT_    (0x27,CP_Animation),
        SUPPORT_    (0x28,CP_Spectate),
        SUPPORT_DED (0x29,CP_PlayerBlockPlacement,_1_11),
        SUPPORT_    (0x2a,CP_UseItem),
        SUPPORT_    (0x30,CP___),
    },
};

////////////////////////////////////////////////////////////////////////////////

// Uncomment packet IDs that should be dumped
uint32_t DUMP_ENABLED[] = {
    // SP_SpawnObject,
    // SP_SpawnExperienceOrb,
    // SP_SpawnGlobalEntity,
    // SP_SpawnMob,
    // SP_SpawnPainting,
    // SP_SpawnPlayer,
    // SP_Animation,
    // SP_Statistics,
    // SP_BlockBreakAnimation,
    // SP_UpdateBlockEntity,
    // SP_BlockAction,
    // SP_BlockChange,
    // SP_BossBar,
    // SP_ServerDifficulty,
    // SP_ChatMessage,
    // SP_MultiBlockChange,
    // SP_TabComplete,
    // SP_DeclareCommands,
    // SP_ConfirmTransaction,
    // SP_CloseWindow,
    // SP_OpenWindow,
    // SP_WindowItems,
    // SP_WindowProperty,
    // SP_SetSlot,
    // SP_SetCooldown,
    // SP_PluginMessage,
    // SP_NamedSoundEffect,
    // SP_Disconnect,
    // SP_EntityStatus,
    // SP_NbtQueryResponse,
    // SP_Explosion,
    // SP_UnloadChunk,
    // SP_ChangeGameState,
    // SP_KeepAlive,
    // SP_ChunkData,
    // SP_Effect,
    // SP_Particle,
    // SP_JoinGame,
    // SP_Map,
    // SP_Entity,
    // SP_EntityRelMove,
    // SP_EntityLookRelMove,
    // SP_EntityLook,
    // SP_VehicleMove,
    // SP_OpenSignEditor,
    // SP_CraftRecipeResponse,
    // SP_PlayerAbilities,
    // SP_CombatEffect,
    // SP_PlayerListItem,
    // SP_FacePlayer,
    // SP_PlayerPositionLook,
    // SP_UseBed,
    // SP_UnlockRecipes,
    // SP_DestroyEntities,
    // SP_RemoveEntityEffect,
    // SP_ResourcePackSent,
    // SP_Respawn,
    // SP_EntityHeadLook,
    // SP_SelectAdvancementTab,
    // SP_WorldBorder,
    // SP_Camera,
    // SP_HeldItemChange,
    // SP_DisplayScoreboard,
    // SP_EntityMetadata,
    // SP_AttachEntity,
    // SP_EntityVelocity,
    // SP_EntityEquipment,
    // SP_SetExperience,
    // SP_UpdateHealth,
    // SP_ScoreboardObjective,
    // SP_SetPassengers,
    // SP_Teams,
    // SP_UpdateScore,
    // SP_SpawnPosition,
    // SP_TimeUpdate,
    // SP_Title,
    // SP_StopSound,
    // SP_SoundEffect,
    // SP_PlayerListHeader,
    // SP_CollectItem,
    // SP_EntityTeleport,
    // SP_Advancements,
    // SP_EntityProperties,
    // SP_EntityEffect,
    // SP_DeclareRecipes,
    // SP_Tags,

    // CP_TeleportConfirm,
    // CP_QueryBlockNbt,
    // CP_ChatMessage,
    // CP_ClientStatus,
    // CP_ClientSettings,
    // CP_TabComplete,
    // CP_ConfirmTransaction,
    // CP_EnchantItem,
    // CP_ClickWindow,
    // CP_CloseWindow,
    // CP_PluginMessage,
    // CP_EditBook,
    // CP_QueryEntityNbt,
    // CP_UseEntity,
    // CP_KeepAlive,
    // CP_Player,
    // CP_PlayerPosition,
    // CP_PlayerPositionLook,
    // CP_PlayerLook,
    // CP_VehicleMove,
    // CP_SteerBoat,
    // CP_PickItem,
    // CP_CraftRecipeRequest,
    // CP_PlayerAbilities,
    // CP_PlayerDigging,
    // CP_EntityAction,
    // CP_SteerVehicle,
    // CP_RecipeBookData,
    // CP_NameItem,
    // CP_ResourcePackStatus,
    // CP_AdvancementTab,
    // CP_SelectTrade,
    // CP_SetBeaconEffect,
    // CP_HeldItemChange,
    // CP_UpdateCommandBlock,
    // CP_UpdateCmdMinecart,
    // CP_CreativeInventoryAct,
    // CP_UpdateStructureBlock,
    // CP_UpdateSign,
    // CP_Animation,
    // CP_Spectate,
    // CP_PlayerBlockPlacement,
    // CP_UseItem,
    0xffffffff // Terminator
};

static inline int is_packet_dumpable(int pid) {
    int i;
    for(i=0; DUMP_ENABLED[i]!=0xffffffff; i++)
        if (DUMP_ENABLED[i] == pid) return 1;
    return 0;
}


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

uint8_t * encode_disconnect(SL_Disconnect_pkt *tpkt, uint8_t *w) {
    Wstr(reason);
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
    return w;
}

void decode_encryption_response(CL_EncryptionResponse_pkt *tpkt, uint8_t *p) {
    Pvarint(sklen);
    Pdata(skey,tpkt->sklen);
    Pvarint(tklen);
    Pdata(token,tpkt->tklen);
}








////////////////////////////////////////////////////////////////////////////////

int currentProtocol = PROTO_NONE;

const static packet_methods (* SUPPORT)[MAXPACKETTYPES] = NULL;

typedef struct {
    int         protocolVersion;
    int         protocolId;
    char *      minecraftVersion;
    const packet_methods (* supportTable)[MAXPACKETTYPES];
} protocol_support_t;

static protocol_support_t supported[] = {
    { 393, PROTO_1_13,      "1.13",     SUPPORT_1_13 },
    { 401, PROTO_1_13_1,    "1.13.1",   SUPPORT_1_13 },
    { 404, PROTO_1_13_2,    "1.13.2",   SUPPORT_1_13 },
    {  -1, PROTO_NONE,  NULL,       NULL },
};

int set_protocol(int protocol, char * reply) {
    SUPPORT = NULL;

    int i;
    for(i=0; supported[i].protocolVersion >= 0; i++) {
        if (supported[i].protocolVersion == protocol && supported[i].supportTable) {
            SUPPORT = supported[i].supportTable;
            currentProtocol = supported[i].protocolId;
            printf("Selecting protocol %d (%s) ID=%08x\n", protocol, supported[i].minecraftVersion, currentProtocol);
            return 1;
        }
    }

    // protocol is not supported
    if (reply) {
        int p = sprintf(reply, "{ text:\"The Minecraft protocol version of your client (%d) is not supported by this release of MCBuild\nSupported protocols are: ", protocol);
        int nsupp = 0;
        for(i=0; supported[i].protocolVersion >= 0; i++) {
            if (supported[i].supportTable) {
                p += sprintf(reply+p, "%s%d(%s)", (nsupp>0)?", ":"",
                    supported[i].protocolVersion, supported[i].minecraftVersion);
                nsupp++;
            }
        }
        sprintf(reply+p, "%s\" }", nsupp?"":"none");
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int restore_rawtype(MCPacket * pkt) {
    if (pkt->modified || !pkt->raw) {
        int i;
        for(i=0; i<0x100 && ((SUPPORT[pkt->cl][i].pid&0xff) < 0xff); i++) {
            if (SUPPORT[pkt->cl][i].pid == pkt->pid) {
                pkt->rawtype = i;
                break;
            }
        }
    }
    return pkt->rawtype;
}

MCPacket * decode_packet(int is_client, uint8_t *data, ssize_t len) {
    if (len <= 0) return NULL;  // some servers send empty packets

    uint8_t * p = data;
    Rvarint(rawtype);           // on-wire packet type

    lh_create_obj(MCPacket, pkt);

    // fill in basic data
    pkt->rawtype = rawtype;
    pkt->pid  = SUPPORT[is_client][rawtype].pid;
    pkt->ver  = PROTO_NONE;

    // make a raw data copy
    pkt->rawlen = data+len-p;
    if (pkt->rawlen <= 0) {
        printf("Incorrect length in decode_packet : data=%p, len=%zd, rawtype=%02x, pid=%08x, ver=%08x, rawlen=%p+%zd-%p=%zd\n",
               data, len, rawtype, pkt->pid, pkt->ver, data, len, p, pkt->rawlen);
        hexdump(data, len);
        return NULL;
    }
    pkt->raw = malloc(pkt->rawlen);
    memmove(pkt->raw, p, pkt->rawlen);

    // decode packet if supported
    if (SUPPORT[pkt->cl][rawtype].decode_method) {
        SUPPORT[pkt->cl][rawtype].decode_method(pkt);
    }

    return pkt;
}

//FIXME: for now we assume static buffer allocation and sufficient buffer size
//FIXME: we should convert this to lh_buf_t or a resizeable buffer later
ssize_t encode_packet(MCPacket *pkt, uint8_t *buf) {
    uint8_t * p = buf;

    restore_rawtype(pkt);

    // write packet type
    lh_write_varint(p, pkt->rawtype);
    ssize_t ll = p-buf;

    if (!pkt->modified && pkt->raw) {
        memmove(p, pkt->raw, pkt->rawlen);
        return ll+pkt->rawlen;
    }
    else if ( SUPPORT[pkt->cl][pkt->rawtype].encode_method ) {
        return ll+SUPPORT[pkt->cl][pkt->rawtype].encode_method(pkt, p);
    }
    else {
        assert(0);
    }
}

void dump_packet(MCPacket *pkt) {
    char *states="ISLP";

    restore_rawtype(pkt);

    if (is_packet_dumpable(pkt->pid)) {
        if (SUPPORT[pkt->cl][pkt->rawtype].dump_method) {
            printf("%c %c %2x %08x ",pkt->cl?'C':'S',states[pkt->mode],pkt->rawtype, pkt->pid);
            printf("%-24s    ",SUPPORT[pkt->cl][pkt->type].dump_name);
            SUPPORT[pkt->cl][pkt->type].dump_method(pkt);
            printf("\n");
        }
        else if (pkt->raw) {
            printf("%c %c %2x %08x ",pkt->cl?'C':'S',states[pkt->mode],pkt->rawtype,pkt->pid);
            printf("%-24s    len=%6zd, raw=%s","Raw",pkt->rawlen,limhex(pkt->raw,pkt->rawlen,64));
            printf("\n");
        }
        else {
            //printf("(unknown)");
        }
    }
}

void free_packet(MCPacket *pkt) {
    restore_rawtype(pkt);

    lh_free(pkt->raw);

    if (SUPPORT[pkt->cl][pkt->rawtype].free_method) {
        SUPPORT[pkt->cl][pkt->rawtype].free_method(pkt);
    }

    free(pkt);
}

////////////////////////////////////////////////////////////////////////////////

void queue_packet (MCPacket *pkt, MCPacketQueue *q) {
    *lh_arr_new(GAR(q->queue)) = pkt;
}

void packet_queue_transmit(MCPacketQueue *q, MCPacketQueue *pq, tokenbucket *tb) {
    if (!tb_event(tb, 1)) return;
    if (!C(pq->queue)) return; // no preview packets queued
    queue_packet(P(pq->queue)[0], q);
    lh_arr_delete(GAR(pq->queue),0);
}
