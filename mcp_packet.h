/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

/*
 mcp_packet : Minecraft protocol packet definitions and decoding/encoding
*/

#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include <lh_arr.h>

#include "mcp_ids.h"
#include "mcp_types.h"
#include "slot.h"
#include "entity.h"

////////////////////////////////////////////////////////////////////////////////
// Misc

int decode_chat_json(const char *json, char *name, char *message);

////////////////////////////////////////////////////////////////////////////////
// Server -> Client

// 0x00
typedef struct {
    uint32_t    eid;
    uuid_t      uuid;
    uint8_t     objtype;
    double      x;
    double      y;
    double      z;
    angle_t     pitch;
    angle_t     yaw;
    //TODO: object data
} SP_SpawnObject_pkt;

// 0x01
typedef struct {
    uint32_t eid;
    double      x;
    double      y;
    double      z;
    uint16_t count;
} SP_SpawnExperienceOrb_pkt;

// 0x03
typedef struct {
    uint32_t    eid;
    uuid_t      uuid;
    uint8_t     mobtype;
    double      x;
    double      y;
    double      z;
    angle_t     yaw;
    angle_t     pitch;
    angle_t     headpitch;
    int16_t     vx;
    int16_t     vy;
    int16_t     vz;
    metadata *meta;
} SP_SpawnMob_pkt;

// 0x04
typedef struct {
    uint32_t    eid;
    uuid_t      uuid;
    char        title[32];
    pos_t       pos;
    uint8_t     dir;
} SP_SpawnPainting_pkt;

// 0x05
typedef struct {
    uint32_t    eid;
    uuid_t      uuid;
    double      x;
    double      y;
    double      z;
    angle_t     yaw;
    angle_t     pitch;
    int16_t     item;
    metadata *meta;
} SP_SpawnPlayer_pkt;

// 0x0f
typedef struct {
    char       *json;
    uint8_t     pos;
} SP_ChatMessage_pkt;

// 0x30
typedef struct {
    uint32_t    count;
    uint32_t   *eids;
} SP_DestroyEntities_pkt;






// 0x00
typedef struct {
    uint32_t id;
} SP_KeepAlive_pkt;

// 0x01
typedef struct {
    uint32_t eid;
    uint8_t  gamemode;
    int8_t   dimension;
    uint8_t  difficulty;
    uint8_t  maxplayers;
    char     leveltype[32];
    uint8_t  reduced_debug_info;
} SP_JoinGame_pkt;

// 0x03
typedef struct {
    uint64_t worldage;
    uint64_t time;
} SP_TimeUpdate_pkt;

// 0x04
typedef struct {
    uint32_t eid;
    int16_t  sid;
    slot_t   item;
} SP_EntityEquipment_pkt;

// 0x06
typedef struct {
    float    health;
    int32_t  food;
    float    saturation;
} SP_UpdateHealth_pkt;

// 0x07
typedef struct {
    int32_t dimension;
    uint8_t difficulty;
    uint8_t gamemode;
    char    leveltype[32];
} SP_Respawn_pkt;

// 0x08
typedef struct {
    double x,y,z;
    float  yaw,pitch;
    char   flags;
} SP_PlayerPositionLook_pkt;

// 0x09
typedef struct {
    int8_t sid;
} SP_HeldItemChange_pkt;

// 0x0d
typedef struct {
    uint32_t eid;
    uint32_t collector;
} SP_CollectItem_pkt;

// 0x12
typedef struct {
    uint32_t eid;
    int16_t  vx;
    int16_t  vy;
    int16_t  vz;
} SP_EntityVelocity_pkt;

// 0x14
typedef struct {
    uint32_t eid;
} SP_Entity_pkt;

// 0x15
typedef struct {
    uint32_t eid;
    int8_t   dx;
    int8_t   dy;
    int8_t   dz;
    uint8_t  onground;
} SP_EntityRelMove_pkt;

// 0x16
typedef struct {
    uint32_t eid;
    angle_t  yaw;
    angle_t  pitch;
    uint8_t  onground;
} SP_EntityLook_pkt;

// 0x17
typedef struct {
    uint32_t eid;
    int8_t   dx;
    int8_t   dy;
    int8_t   dz;
    angle_t  yaw;
    angle_t  pitch;
    uint8_t  onground;
} SP_EntityLookRelMove_pkt;

// 0x18
typedef struct {
    uint32_t eid;
    fixp     x;
    fixp     y;
    fixp     z;
    angle_t  yaw;
    angle_t  pitch;
    uint8_t  onground;
} SP_EntityTeleport_pkt;

// 0x1c
typedef struct {
    uint32_t eid;
    metadata *meta;
} SP_EntityMetadata_pkt;

// 0x1f
typedef struct {
    float    bar;   // state of the experience bar
    int32_t  level; // player's level
    int32_t  exp;   // total experience
} SP_SetExperience_pkt;

// 0x21
typedef struct {
    int8_t   cont;          // ground-up continuous
    int8_t   skylight;      // whether skylight was sent;
    chunk_t  chunk;
} SP_ChunkData_pkt;

// 0x22
typedef struct {
    int32_t  X,Z;
    int32_t  count;
    blkrec  *blocks;
} SP_MultiBlockChange_pkt;

// 0x23
typedef struct {
    pos_t    pos;
    bid_t    block;
} SP_BlockChange_pkt;

// 0x26
typedef struct {
    int8_t   skylight;
    int32_t  nchunks;
    chunk_t *chunk;
} SP_MapChunkBulk_pkt;

// 0x27
typedef struct {
    float    x,y,z;
    float    radius;
    int32_t  count;
    boff_t  *blocks;
    float    vx,vy,vz;
} SP_Explosion_pkt;

// 0x28
typedef struct {
    uint32_t id;
    pos_t    loc;
    uint32_t data;
    uint8_t  disvol;
} SP_Effect_pkt;

// 0x29
typedef struct {
    char     name[256];
    int32_t  x;     // multiplied by 8
    int32_t  y;     // multiplied by 8
    int32_t  z;     // multiplied by 8
    float    vol;
    uint8_t  pitch;
} SP_SoundEffect_pkt;

// 0x2d
typedef struct {
    uint8_t  wid;
    char     wtype[256];
    char    *title;
    uint8_t  nslots;
    uint32_t eid;   // horse's ID - only used if the window type is a EntityHorse
} SP_OpenWindow_pkt;

// 0x2e
typedef struct {
    uint8_t wid;
} SP_CloseWindow_pkt;

// 0x2f
typedef struct {
    uint8_t  wid;
    int16_t  sid;
    slot_t   slot;
} SP_SetSlot_pkt;

// 0x30
typedef struct {
    uint8_t  wid;
    int16_t  count;
    slot_t   slots[256];
} SP_WindowItems_pkt;

// 0x32
typedef struct {
    uint8_t  wid;
    uint16_t aid;
    uint8_t  accepted;
} SP_ConfirmTransaction_pkt;

// 0x34
typedef struct {
    uint8_t  type;
    uint8_t  x;
    uint8_t  z;
} map_icon;

typedef struct {
    uint32_t mapid;
    uint8_t  scale;
    uint32_t nicons;
    map_icon *icons;
    uint8_t  ncols;
    uint8_t  nrows;
    uint8_t  X;
    uint8_t  Z;
    uint32_t len;
    uint8_t  *data;
} SP_Maps_pkt;

// 0x35
typedef struct {
    pos_t    loc;
    uint8_t  action;
    nbt_t    *nbt;
} SP_UpdateBlockEntity_pkt;

// 0x46
typedef struct {
    int32_t     threshold;
} SP_SetCompression_pkt;

////////////////////////////////////////////////////////////////////////////////
// Client -> Server

// 0x02
typedef struct {
    char str[256];
} CP_ChatMessage_pkt;









// 0x02
typedef struct {
    uint32_t target;
    uint32_t action;
    float    x,y,z;
} CP_UseEntity_pkt;

// 0x03
typedef struct {
    uint8_t onground;
} CP_Player_pkt;

// 0x04
typedef struct {
    double x;
    double y;
    double z;
    uint8_t onground;
} CP_PlayerPosition_pkt;

// 0x05
typedef struct {
    float  yaw;
    float  pitch;
    uint8_t onground;
} CP_PlayerLook_pkt;

// 0x06
typedef struct {
    double x;
    double y;
    double z;
    float  yaw;
    float  pitch;
    uint8_t onground;
} CP_PlayerPositionLook_pkt;

// 0x07
typedef struct {
    uint8_t status;
    pos_t   loc;
    uint8_t face;
} CP_PlayerDigging_pkt;

// 0x08
typedef struct {
    pos_t   bpos;
    int8_t  face;
    slot_t  item;
    int8_t  cx,cy,cz;
} CP_PlayerBlockPlacement_pkt;

// 0x09
typedef struct {
    int16_t sid;
} CP_HeldItemChange_pkt;

// 0x0a
typedef struct {
} CP_Animation_pkt;

// 0x0b
typedef struct {
    uint32_t eid;
    uint32_t action;
    uint32_t jumpboost;
} CP_EntityAction_pkt;

// 0x0d
typedef struct {
    uint8_t wid;
} CP_CloseWindow_pkt;

// 0x0e
typedef struct {
    uint8_t  wid;
    int16_t  sid;
    uint8_t  button;
    uint16_t aid;
    uint8_t  mode;
    slot_t   slot;
} CP_ClickWindow_pkt;










////////////////////////////////////////////////////////////////////////////////
// Packet parsing for the login routines in mcproxy

// CI_Handshake
typedef struct {
    int32_t  protocolVer;
    char     serverAddr[1024];
    uint16_t serverPort;
    int32_t  nextState;
} CI_Handshake_pkt;

// SL_EncryptionRequest
typedef struct {
    char     serverID[4096];
    uint32_t klen;
    uint8_t  pkey[1024];
    uint32_t tlen;
    uint8_t  token[1024];
} SL_EncryptionRequest_pkt;

// CL_EncryptionResponse
typedef struct {
    uint32_t sklen;
    uint8_t  skey[1024];
    uint32_t tklen;
    uint8_t  token[1024];
} CL_EncryptionResponse_pkt;

void decode_handshake(CI_Handshake_pkt *tpkt, uint8_t *p);
uint8_t * encode_handshake(CI_Handshake_pkt *tpkt, uint8_t *w);
void decode_encryption_request(SL_EncryptionRequest_pkt *tpkt, uint8_t *p);
void decode_encryption_response(CL_EncryptionResponse_pkt *tpkt, uint8_t *p);

////////////////////////////////////////////////////////////////////////////////

#define PKT(name) name##_pkt _##name

typedef struct {
    union {
        int32_t pid;                 // for use as Cx_ Sx_ constants defined in mcp_ids.h
        struct {
            unsigned int type  : 24; // raw packet type from the protocol
            unsigned int mode  : 2;  // protocol mode: 0:Idle, 1:Status, 2:Login, 3:Play
            unsigned int _mode : 2;
            unsigned int cl    : 1;  // whether this packet comes from client
            unsigned int _cl   : 3;
        };
    };

    int32_t   ver;      // decoder will mark this with its version, so in
                        // difficult cases you can differentiate between
                        // changed interpretation
    int modified;

    uint8_t * raw;      // raw packet data
    ssize_t   rawlen;

    struct timeval ts;  // timestamp when the packet was recevied

    // various packet types depending on pid
    union {
        PKT(SP_SpawnObject);        // 00
        PKT(SP_SpawnExperienceOrb); // 01
        PKT(SP_SpawnMob);           // 03
        PKT(SP_SpawnPainting);      // 04
        PKT(SP_SpawnPlayer);        // 05
        PKT(SP_ChatMessage);        // 0f
        PKT(SP_DestroyEntities);    // 30

        PKT(SP_KeepAlive);
        PKT(SP_JoinGame);
        PKT(SP_TimeUpdate);
        PKT(SP_EntityEquipment);
        PKT(SP_UpdateHealth);
        PKT(SP_Respawn);
        PKT(SP_PlayerPositionLook);
        PKT(SP_HeldItemChange);
        PKT(SP_CollectItem);
        PKT(SP_EntityVelocity);
        PKT(SP_Entity);
        PKT(SP_EntityRelMove);
        PKT(SP_EntityLook);
        PKT(SP_EntityLookRelMove);
        PKT(SP_EntityTeleport);
        PKT(SP_EntityMetadata);
        PKT(SP_SetExperience);
        PKT(SP_ChunkData);
        PKT(SP_MultiBlockChange);
        PKT(SP_BlockChange);
        PKT(SP_MapChunkBulk);
        PKT(SP_Explosion);
        PKT(SP_Effect);
        PKT(SP_SoundEffect);
        PKT(SP_OpenWindow);
        PKT(SP_CloseWindow);
        PKT(SP_SetSlot);
        PKT(SP_WindowItems);
        PKT(SP_ConfirmTransaction);
        PKT(SP_Maps);
        PKT(SP_UpdateBlockEntity);
        PKT(SP_SetCompression);

        PKT(CP_ChatMessage);        // 02

        PKT(CP_UseEntity);
        PKT(CP_Player);
        PKT(CP_PlayerPosition);
        PKT(CP_PlayerLook);
        PKT(CP_PlayerPositionLook);
        PKT(CP_PlayerDigging);
        PKT(CP_PlayerBlockPlacement);
        PKT(CP_HeldItemChange);
        PKT(CP_Animation);
        PKT(CP_EntityAction);
        PKT(CP_CloseWindow);
        PKT(CP_ClickWindow);
    };
} MCPacket;

typedef struct {
    lh_arr_declare(MCPacket *,queue);
} MCPacketQueue;

////////////////////////////////////////////////////////////////////////////////

MCPacket * decode_packet(int is_client, uint8_t *p, ssize_t len);
ssize_t    encode_packet(MCPacket *pkt, uint8_t *buf);
void       dump_packet(MCPacket *pkt);
void       free_packet  (MCPacket *pkt);
void       queue_packet (MCPacket *pkt, MCPacketQueue *q);

#define NEWPACKET(type,name)                     \
    lh_create_obj(MCPacket,name);                \
    name->pid = type;                            \
    type##_pkt *t##name = &name->_##type;

////////////////////////////////////////////////////////////////////////////////

