#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <lh_arr.h>

// special data types used in the MC packets

typedef uint8_t uuid_t[16];
typedef int32_t fixp;
typedef uint8_t angle_t;
typedef struct {
    union {
        uint64_t p;
        struct {
            int64_t z : 26;
            int64_t y : 12;
            int64_t x : 26;
        };
    };
} pos_t;

// single metadata key-value pair
typedef struct {
    union {
        struct {
            unsigned char key  : 5;
            unsigned char type : 3;
        };
        uint8_t h;
    };
    union {
        int8_t  b;
        int16_t s;
        int32_t i;
        float   f;
        char    str[256];
        //TODO: slot;
        struct {
            int32_t x;
            int32_t y;
            int32_t z;
        };
        struct {
            float   pitch;
            float   yaw;
            float   roll;
        };
    };
} metadata;

typedef struct {
    union {
        struct {
            uint16_t meta : 4;
            uint16_t bid  : 12;
        };
        uint16_t raw;
    };
} bid_t;

// used for skylight and blocklight
typedef struct {
    union {
        struct {
            uint8_t l : 4;
            uint8_t h : 4;
        };
        uint8_t b;
    };
} light_t;

// represents a single 16x16x16 cube of blocks within a chunk
typedef struct {
    bid_t   blocks[4096];
    light_t skylight[2048];
    light_t light[2048];
} cube_t;

typedef struct {
    int32_t  X;
    int32_t  Z;
    uint16_t mask;
    cube_t   *cubes[16];    // pointers to cubes. The pointers may be NULL meaning air
    uint8_t  biome[256];
} chunk_t;

typedef struct {
    union {
        uint8_t pos;
        struct {
            uint8_t z : 4;
            uint8_t x : 4;
        };
    };
    uint8_t y;
    bid_t bid;
} blkrec;

typedef struct {
    int8_t dx,dy,dz;
} boff_t;

////////////////////////////////////////////////////////////////////////////////
// Server -> Client

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

// 0x02
typedef struct {
    char    *json;
    uint8_t  pos;
} SP_ChatMessage_pkt;

// 0x03
typedef struct {
    uint64_t worldage;
    uint64_t time;
} SP_TimeUpdate_pkt;

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
    uint8_t sid;
} SP_HeldItemChange_pkt;

// 0x0c
typedef struct {
    uint32_t eid;
    uuid_t   uuid;
    fixp     x;
    fixp     y;
    fixp     z;
    angle_t  yaw;
    angle_t  pitch;
    int16_t  item;
    metadata *meta;
} SP_SpawnPlayer_pkt;

// 0x0e
typedef struct {
    uint32_t eid;
    uint8_t  objtype;
    fixp     x;
    fixp     y;
    fixp     z;
    angle_t  pitch;
    angle_t  yaw;
    //TODO: object data
} SP_SpawnObject_pkt;

// 0x0f
typedef struct {
    uint32_t eid;
    uint8_t  mobtype;
    fixp     x;
    fixp     y;
    fixp     z;
    angle_t  yaw;
    angle_t  pitch;
    angle_t  headpitch;
    int16_t  vx;
    int16_t  vy;
    int16_t  vz;
    metadata *meta;
} SP_SpawnMob_pkt;

// 0x10
typedef struct {
    uint32_t eid;
    char     title[32];
    pos_t    pos;
    uint8_t  dir;
} SP_SpawnPainting_pkt;

// 0x11
typedef struct {
    uint32_t eid;
    fixp     x;
    fixp     y;
    fixp     z;
    uint16_t count;
} SP_SpawnExperienceOrb_pkt;

// 0x12
typedef struct {
    uint32_t eid;
    int16_t  vx;
    int16_t  vy;
    int16_t  vz;
} SP_EntityVelocity_pkt;    

// 0x13
typedef struct {
    uint32_t count;
    uint32_t *eids;
} SP_DestroyEntities_pkt;

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
    fixp      x;
    fixp      y;
    fixp      z;
    angle_t  yaw;
    angle_t  pitch;
    uint8_t  onground;
} SP_EntityTeleport_pkt;

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

// 0x46
typedef struct {
    int32_t     threshold;
} SP_SetCompression_pkt;

////////////////////////////////////////////////////////////////////////////////
// Client -> Server

// 0x01
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

// 0x09
typedef struct {
    int16_t sid;
} CP_HeldItemChange_pkt;

// 0x0a
typedef struct {
} CP_Animation_pkt;

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
    char     pkey[1024];
    uint32_t tlen;
    char     token[1024];
} SL_EncryptionRequest_pkt;

// CL_EncryptionResponse
typedef struct {
    uint32_t sklen;
    char     skey[1024];
    uint32_t tklen;
    char     token[1024];
} CL_EncryptionResponse_pkt;

void decode_handshake(CI_Handshake_pkt *tpkt, uint8_t *p);
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

    // various packet types depending on pid
    union {
        PKT(SP_JoinGame);
        PKT(SP_ChatMessage);
        PKT(SP_TimeUpdate);
        PKT(SP_Respawn);
        PKT(SP_PlayerPositionLook);
        PKT(SP_HeldItemChange);
        PKT(SP_SpawnPlayer);
        PKT(SP_SpawnObject);
        PKT(SP_SpawnMob);
        PKT(SP_SpawnPainting);
        PKT(SP_SpawnExperienceOrb);
        PKT(SP_EntityVelocity);
        PKT(SP_DestroyEntities);
        PKT(SP_Entity);
        PKT(SP_EntityRelMove);
        PKT(SP_EntityLook);
        PKT(SP_EntityLookRelMove);
        PKT(SP_EntityTeleport);
        PKT(SP_SetExperience);
        PKT(SP_ChunkData);
        PKT(SP_MultiBlockChange);
        PKT(SP_BlockChange);
        PKT(SP_MapChunkBulk);
        PKT(SP_Explosion);
        PKT(SP_Effect);
        PKT(SP_SoundEffect);
        PKT(SP_SetCompression);

        PKT(CP_ChatMessage);
        PKT(CP_UseEntity);
        PKT(CP_Player);
        PKT(CP_PlayerPosition);
        PKT(CP_PlayerLook);
        PKT(CP_PlayerPositionLook);
        PKT(CP_HeldItemChange);
        PKT(CP_Animation);
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

////////////////////////////////////////////////////////////////////////////////

