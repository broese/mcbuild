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
#include "helpers.h"
#include "mcp_database.h"

////////////////////////////////////////////////////////////////////////////////
//// Protocol IDs

#define PROTO_NONE   0x00000000
#define PROTO_1_8_1  0x00010801
#define PROTO_1_9    0x00010900
#define PROTO_1_9_2  0x00010902
#define PROTO_1_9_4  0x00010904
#define PROTO_1_10   0x00011000
#define PROTO_1_11   0x00011100
#define PROTO_1_11_2 0x00011102
#define PROTO_1_12   0x00011200
#define PROTO_1_12_1 0x00011201
#define PROTO_1_12_2 0x00011202
#define PROTO_1_13   0x00011300
#define PROTO_1_13_1 0x00011301
#define PROTO_1_13_2 0x00011302

////////////////////////////////////////////////////////////////////////////////
//// Protocol Messages

#define STATE_IDLE     0
#define STATE_STATUS   1
#define STATE_LOGIN    2
#define STATE_PLAY     3

#define SI(x) ((0x00<<24)|(0x##x))
#define CI(x) ((0x10<<24)|(0x##x))
#define SS(x) ((0x01<<24)|(0x##x))
#define CS(x) ((0x11<<24)|(0x##x))
#define SL(x) ((0x02<<24)|(0x##x))
#define CL(x) ((0x12<<24)|(0x##x))
#define SP(x) ((0x03<<24)|(0x##x))
#define CP(x) ((0x13<<24)|(0x##x))

#define PID(x)     ((x)&0xffffff)
#define PCLIENT(x) (((x)&0x10000000)!=0)
#define PSTATE(x)  (((x)>>24)&0x0f)

// Handshakes

#define CI_Handshake            CI(00)

// Status Query

#define SS_Response             SS(00)
#define SS_PingResponse         SS(01)

#define CS_Request              CS(00)
#define CS_PingRequest          CS(01)

// Login Process

#define SL_Disconnect           SL(00)
#define SL_EncryptionRequest    SL(01)
#define SL_LoginSuccess         SL(02)
#define SL_SetCompression       SL(03)

#define CL_LoginStart           CL(00)
#define CL_EncryptionResponse   CL(01)

// Play
// Note: the IDs are arbitrary, but we keep them matching the latest protocol
// version. IDs removed by an MC update are kept and moved to the bottom (0x80+)

#define SP_SpawnObject          SP(00)
#define SP_SpawnExperienceOrb   SP(01)
#define SP_SpawnGlobalEntity    SP(02)
#define SP_SpawnMob             SP(03)
#define SP_SpawnPainting        SP(04)
#define SP_SpawnPlayer          SP(05)
#define SP_Animation            SP(06)
#define SP_Statistics           SP(07)
#define SP_BlockBreakAnimation  SP(08)
#define SP_UpdateBlockEntity    SP(09)
#define SP_BlockAction          SP(0a)
#define SP_BlockChange          SP(0b)
#define SP_BossBar              SP(0c)
#define SP_ServerDifficulty     SP(0d)
#define SP_ChatMessage          SP(0e)
#define SP_MultiBlockChange     SP(0f)
#define SP_TabComplete          SP(10)
#define SP_DeclareCommands      SP(11)
#define SP_ConfirmTransaction   SP(12)
#define SP_CloseWindow          SP(13)
#define SP_OpenWindow           SP(14)
#define SP_WindowItems          SP(15)
#define SP_WindowProperty       SP(16)
#define SP_SetSlot              SP(17)
#define SP_SetCooldown          SP(18)
#define SP_PluginMessage        SP(19)
#define SP_NamedSoundEffect     SP(1a)
#define SP_Disconnect           SP(1b)
#define SP_EntityStatus         SP(1c)
#define SP_NbtQueryResponse     SP(1d)
#define SP_Explosion            SP(1e)
#define SP_UnloadChunk          SP(1f)
#define SP_ChangeGameState      SP(20)
#define SP_KeepAlive            SP(21)
#define SP_ChunkData            SP(22)
#define SP_Effect               SP(23)
#define SP_Particle             SP(24)
#define SP_JoinGame             SP(25)
#define SP_Map                  SP(26)
#define SP_Entity               SP(27)
#define SP_EntityRelMove        SP(28)
#define SP_EntityLookRelMove    SP(29)
#define SP_EntityLook           SP(2a)
#define SP_VehicleMove          SP(2b)
#define SP_OpenSignEditor       SP(2c)
#define SP_CraftRecipeResponse  SP(2d)
#define SP_PlayerAbilities      SP(2e)
#define SP_CombatEffect         SP(2f)
#define SP_PlayerListItem       SP(30)
#define SP_FacePlayer           SP(31)
#define SP_PlayerPositionLook   SP(32)
#define SP_UseBed               SP(33)
#define SP_UnlockRecipes        SP(34)
#define SP_DestroyEntities      SP(35)
#define SP_RemoveEntityEffect   SP(36)
#define SP_ResourcePackSent     SP(37)
#define SP_Respawn              SP(38)
#define SP_EntityHeadLook       SP(39)
#define SP_SelectAdvancementTab SP(3a)
#define SP_WorldBorder          SP(3b)
#define SP_Camera               SP(3c)
#define SP_HeldItemChange       SP(3d)
#define SP_DisplayScoreboard    SP(3e)
#define SP_EntityMetadata       SP(3f)
#define SP_AttachEntity         SP(40)
#define SP_EntityVelocity       SP(41)
#define SP_EntityEquipment      SP(42)
#define SP_SetExperience        SP(43)
#define SP_UpdateHealth         SP(44)
#define SP_ScoreboardObjective  SP(45)
#define SP_SetPassengers        SP(46)
#define SP_Teams                SP(47)
#define SP_UpdateScore          SP(48)
#define SP_SpawnPosition        SP(49)
#define SP_TimeUpdate           SP(4a)
#define SP_Title                SP(4b)
#define SP_StopSound            SP(4c)
#define SP_SoundEffect          SP(4d)
#define SP_PlayerListHeader     SP(4e)
#define SP_CollectItem          SP(4f)
#define SP_EntityTeleport       SP(50)
#define SP_Advancements         SP(51)
#define SP_EntityProperties     SP(52)
#define SP_EntityEffect         SP(53)
#define SP_DeclareRecipes       SP(54)
#define SP_Tags                 SP(55)
#define SP_UpdateSign           SP(80) // removed since 1.9.4
#define SP___                   SP(ff)

#define CP_TeleportConfirm      CP(00)
#define CP_QueryBlockNbt        CP(01)
#define CP_ChatMessage          CP(02)
#define CP_ClientStatus         CP(03)
#define CP_ClientSettings       CP(04)
#define CP_TabComplete          CP(05)
#define CP_ConfirmTransaction   CP(06)
#define CP_EnchantItem          CP(07)
#define CP_ClickWindow          CP(08)
#define CP_CloseWindow          CP(09)
#define CP_PluginMessage        CP(0a)
#define CP_EditBook             CP(0b)
#define CP_QueryEntityNbt       CP(0c)
#define CP_UseEntity            CP(0d)
#define CP_KeepAlive            CP(0e)
#define CP_Player               CP(0f)
#define CP_PlayerPosition       CP(10)
#define CP_PlayerPositionLook   CP(11)
#define CP_PlayerLook           CP(12)
#define CP_VehicleMove          CP(13)
#define CP_SteerBoat            CP(14)
#define CP_PickItem             CP(15)
#define CP_CraftRecipeRequest   CP(16)
#define CP_PlayerAbilities      CP(17)
#define CP_PlayerDigging        CP(18)
#define CP_EntityAction         CP(19)
#define CP_SteerVehicle         CP(1a)
#define CP_RecipeBookData       CP(1b)
#define CP_NameItem             CP(1c)
#define CP_ResourcePackStatus   CP(1d)
#define CP_AdvancementTab       CP(1e)
#define CP_SelectTrade          CP(1f)
#define CP_SetBeaconEffect      CP(20)
#define CP_HeldItemChange       CP(21)
#define CP_UpdateCommandBlock   CP(22)
#define CP_UpdateCmdMinecart    CP(23)
#define CP_CreativeInventoryAct CP(24)
#define CP_UpdateStructureBlock CP(25)
#define CP_UpdateSign           CP(26)
#define CP_Animation            CP(27)
#define CP_Spectate             CP(28)
#define CP_PlayerBlockPlacement CP(29)
#define CP_UseItem              CP(2a)
#define CP_PrepareCraftingGrid  CP(80) // removed since 1.12.1
#define CP___                   CP(ff)



#define MAXPACKETTYPES          0x100

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
    int         mobtype;
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
    char        title[32];  // obsolete since 1.13
    uint32_t    motive;
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

// 0x09
typedef struct {
    pos_t    loc;
    uint8_t  action;
    nbt_t    *nbt;
} SP_UpdateBlockEntity_pkt;

// 0x0a
typedef struct {
    pos_t    loc;
    uint8_t  b1;
    uint8_t  b2;
    int32_t  type;
} SP_BlockAction_pkt;

// 0x0b
typedef struct {
    pos_t    pos;
    bid_t    block;
} SP_BlockChange_pkt;

// 0x0e
typedef struct {
    char       *json;
    uint8_t     pos;
} SP_ChatMessage_pkt;

// 0x0f
typedef struct {
    int32_t  X,Z;
    int32_t  count;
    blkrec  *blocks;
} SP_MultiBlockChange_pkt;

// 0x12
typedef struct {
    uint8_t  wid;
    uint16_t aid;
    uint8_t  accepted;
} SP_ConfirmTransaction_pkt;

// 0x13
typedef struct {
    uint8_t wid;
} SP_CloseWindow_pkt;

// 0x14
typedef struct {
    uint8_t  wid;
    char     wtype[256];
    char    *title;
    uint8_t  nslots;
    uint32_t eid;   // horse's ID - only used if the window type is a EntityHorse
} SP_OpenWindow_pkt;

// 0x15
typedef struct {
    uint8_t  wid;
    int16_t  count;
    slot_t   *slots;
} SP_WindowItems_pkt;

// 0x17
typedef struct {
    uint8_t  wid;
    int16_t  sid;
    slot_t   slot;
} SP_SetSlot_pkt;

// 0x1e
typedef struct {
    float    x,y,z;
    float    radius;
    int32_t  count;
    boff_t  *blocks;
    float    vx,vy,vz;
} SP_Explosion_pkt;

// 0x1f
typedef struct {
    int32_t  X,Z;
} SP_UnloadChunk_pkt;

// 0x20
typedef struct {
    uint8_t     reason;
    float       value;
} SP_ChangeGameState_pkt;

// 0x22
typedef struct {
    int8_t   cont;          // ground-up continuous
    int8_t   skylight;      // whether skylight was sent;
    chunk_t  chunk;
    nbt_t   *te;            // tile entities
} SP_ChunkData_pkt;

// 0x23
typedef struct {
    uint32_t id;
    pos_t    loc;
    uint32_t data;
    uint8_t  disvol;
} SP_Effect_pkt;

// 0x25
typedef struct {
    uint32_t eid;
    uint8_t  gamemode;
    int32_t  dimension;
    uint8_t  difficulty;
    uint8_t  maxplayers;
    char     leveltype[32];
    uint8_t  reduced_debug_info;
} SP_JoinGame_pkt;

// 0x26
typedef struct {
    uint8_t  type;
    uint8_t  x;
    uint8_t  z;
} map_icon;

typedef struct {
    uint32_t mapid;
    uint8_t  scale;
    uint8_t  trackpos;
    uint32_t nicons;
    map_icon *icons;
    uint8_t  ncols;
    uint8_t  nrows;
    uint8_t  X;
    uint8_t  Z;
    uint32_t len;
    uint8_t  *data;
} SP_Map_pkt;

// 0x28
typedef struct {
    uint32_t    eid;
    int16_t     dx;
    int16_t     dy;
    int16_t     dz;
    uint8_t     onground;
} SP_EntityRelMove_pkt;

// 0x29
typedef struct {
    uint32_t    eid;
    int16_t     dx;
    int16_t     dy;
    int16_t     dz;
    angle_t     yaw;
    angle_t     pitch;
    uint8_t     onground;
} SP_EntityLookRelMove_pkt;

// 0x2e
typedef struct {
    uint8_t     flags;
    float       speed;
    float       fov;
} SP_PlayerAbilities_pkt;

// 0x30
typedef struct {
    char        pname[64];
    char        pval[MCP_MAXSTR];
    uint8_t     is_signed;
    char        signature[MCP_MAXSTR];
} pli_prop;

typedef struct {
    uuid_t      uuid;
    char        name[64];
    lh_arr_declare(pli_prop,prop);
    int32_t     gamemode;
    int32_t     ping;
    uint8_t     has_dispname;
    char        dispname[64];
} pli_t;

typedef struct {
    int32_t     action;
    lh_arr_declare(pli_t,list);
} SP_PlayerListItem_pkt;

// 0x32
typedef struct {
    double      x,y,z;
    float       yaw,pitch;
    char        flags;
    uint32_t    tpid;
} SP_PlayerPositionLook_pkt;

// 0x33
typedef struct {
    uint32_t    eid;
    pos_t       pos;
} SP_UseBed_pkt;

// 0x35
typedef struct {
    uint32_t    count;
    uint32_t   *eids;
} SP_DestroyEntities_pkt;

// 0x38
typedef struct {
    int32_t dimension;
    uint8_t difficulty;
    uint8_t gamemode;
    char    leveltype[32];
} SP_Respawn_pkt;

// 0x3d
typedef struct {
    int8_t sid;
} SP_HeldItemChange_pkt;

// 0x3f
typedef struct {
    uint32_t    eid;
    metadata *  meta;
} SP_EntityMetadata_pkt;

// 0x43
typedef struct {
    float    bar;
    int32_t  level;
    int32_t  exp;
} SP_SetExperience_pkt;

// 0x44
typedef struct {
    float    health;
    int32_t  food;
    float    saturation;
} SP_UpdateHealth_pkt;

// 0x4d
typedef struct {
    int32_t     id;
    int32_t     category;
    int32_t     x;     // multiplied by 8
    int32_t     y;     // multiplied by 8
    int32_t     z;     // multiplied by 8
    float       vol;
    float       pitch;
} SP_SoundEffect_pkt;

// 0x50
typedef struct {
    uint32_t    eid;
    double      x;
    double      y;
    double      z;
    angle_t     yaw;
    angle_t     pitch;
    uint8_t     onground;
} SP_EntityTeleport_pkt;

// removed in 1.9.4
typedef struct {
    pos_t       pos;
    char        line1[64];
    char        line2[64];
    char        line3[64];
    char        line4[64];
} SP_UpdateSign_pkt;




////////////////////////////////////////////////////////////////////////////////
// Client -> Server

// 0x00
typedef struct {
    uint32_t    tpid;
} CP_TeleportConfirm_pkt;

// 0x02
typedef struct {
    char        str[320];
} CP_ChatMessage_pkt;

// 0x06
typedef struct {
    uint8_t     wid;
    uint16_t    aid;
    uint8_t     accepted;
} CP_ConfirmTransaction_pkt;

// 0x08
typedef struct {
    uint8_t  wid;
    int16_t  sid;
    uint8_t  button;
    uint16_t aid;
    uint32_t mode;
    slot_t   slot;
} CP_ClickWindow_pkt;

// 0x09
typedef struct {
    uint8_t wid;
} CP_CloseWindow_pkt;

// 0x0d
typedef struct {
    uint32_t    target;
    uint32_t    action;
    float       x,y,z;
    uint32_t    hand;
} CP_UseEntity_pkt;

// 0x0f
typedef struct {
    uint8_t     onground;
} CP_Player_pkt;

// 0x10
typedef struct {
    double      x;
    double      y;
    double      z;
    uint8_t     onground;
} CP_PlayerPosition_pkt;

// 0x11
typedef struct {
    double      x;
    double      y;
    double      z;
    float       yaw;
    float       pitch;
    uint8_t     onground;
} CP_PlayerPositionLook_pkt;

// 0x12
typedef struct {
    float       yaw;
    float       pitch;
    uint8_t     onground;
} CP_PlayerLook_pkt;

// 0x15
typedef struct {
    uint32_t    sid;
} CP_PickItem_pkt;

// 0x18
typedef struct {
    uint32_t    status;
    pos_t       loc;
    uint8_t     face;
} CP_PlayerDigging_pkt;

// 0x19
typedef struct {
    uint32_t    eid;
    uint32_t    action;
    uint32_t    jumpboost;
} CP_EntityAction_pkt;

// 0x21
typedef struct {
    int16_t sid;
} CP_HeldItemChange_pkt;

// 0x27
typedef struct {
    uint32_t    hand;
} CP_Animation_pkt;

// 0x29
typedef struct {
    pos_t   bpos;
    int32_t face;
    int32_t hand;
    float   cx,cy,cz;
} CP_PlayerBlockPlacement_pkt;

// 0x2a
typedef struct {
    int32_t     hand;
} CP_UseItem_pkt;



////////////////////////////////////////////////////////////////////////////////
// Packet parsing for the login routines in mcproxy

// CI_Handshake ( 0/0 )
typedef struct {
    int32_t     protocolVer;
    char        serverAddr[1024];
    uint16_t    serverPort;
    int32_t     nextState;
} CI_Handshake_pkt;

// SL_Disconnect ( 2/0 )
typedef struct {
    char        reason[4096];
} SL_Disconnect_pkt;

// SL_EncryptionRequest ( 2/1 )
typedef struct {
    char        serverID[4096];
    uint32_t    klen;
    uint8_t     pkey[1024];
    uint32_t    tlen;
    uint8_t     token[1024];
} SL_EncryptionRequest_pkt;

// SL_LoginSuccess ( 2/2 )
typedef struct {
    char        uuid[64];
    char        username[64];
} SL_LoginSuccess_pkt;

// CL_LoginStart ( 2/0 )
typedef struct {
    char        username[64];
} CL_LoginStart_pkt;

// CL_EncryptionResponse ( 2/1 )
typedef struct {
    uint32_t    sklen;
    uint8_t     skey[1024];
    uint32_t    tklen;
    uint8_t     token[1024];
} CL_EncryptionResponse_pkt;

void decode_handshake(CI_Handshake_pkt *tpkt, uint8_t *p);
uint8_t * encode_handshake(CI_Handshake_pkt *tpkt, uint8_t *w);
uint8_t * encode_loginstart(CL_LoginStart_pkt *tpkt, uint8_t *w);
uint8_t * encode_disconnect(SL_Disconnect_pkt *tpkt, uint8_t *w);
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
    int32_t   rawtype;  // on-wire protocol type

    int32_t   ver;      // decoder will mark this with its version, so in
                        // difficult cases you can differentiate between
                        // changed interpretation
    int modified;       // flag to indicate this packet was modified or is new

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
        PKT(SP_UpdateBlockEntity);  // 09
        PKT(SP_BlockAction);        // 0a
        PKT(SP_BlockChange);        // 0b
        PKT(SP_ChatMessage);        // 0e
        PKT(SP_MultiBlockChange);   // 0f
        PKT(SP_ConfirmTransaction); // 12
        PKT(SP_CloseWindow);        // 13
        PKT(SP_OpenWindow);         // 14
        PKT(SP_WindowItems);        // 15
        PKT(SP_SetSlot);            // 17
        PKT(SP_Explosion);          // 1e
        PKT(SP_UnloadChunk);        // 1f
        PKT(SP_ChangeGameState);    // 20
        PKT(SP_ChunkData);          // 22
        PKT(SP_Effect);             // 23
        PKT(SP_JoinGame);           // 25
        PKT(SP_Map);                // 26
        PKT(SP_EntityRelMove);      // 28
        PKT(SP_EntityLookRelMove);  // 29
        PKT(SP_PlayerAbilities);    // 2e
        PKT(SP_PlayerListItem);     // 30
        PKT(SP_PlayerPositionLook); // 32
        PKT(SP_UseBed);             // 33
        PKT(SP_DestroyEntities);    // 35
        PKT(SP_Respawn);            // 38
        PKT(SP_HeldItemChange);     // 3d
        PKT(SP_EntityMetadata);     // 3f
        PKT(SP_SetExperience);      // 43
        PKT(SP_UpdateHealth);       // 44
        PKT(SP_SoundEffect);        // 4d
        PKT(SP_EntityTeleport);     // 50

        PKT(SP_UpdateSign);         // removed in 1.9.4

        PKT(CP_TeleportConfirm);    // 00
        PKT(CP_ChatMessage);        // 02
        PKT(CP_ConfirmTransaction); // 06
        PKT(CP_ClickWindow);        // 08
        PKT(CP_CloseWindow);        // 09
        PKT(CP_UseEntity);          // 0d
        PKT(CP_Player);             // 0f
        PKT(CP_PlayerPosition);     // 10
        PKT(CP_PlayerPositionLook); // 11
        PKT(CP_PlayerLook);         // 12
        PKT(CP_PickItem);           // 15
        PKT(CP_PlayerDigging);      // 18
        PKT(CP_EntityAction);       // 19
        PKT(CP_HeldItemChange);     // 21
        PKT(CP_Animation);          // 27
        PKT(CP_PlayerBlockPlacement);// 29
        PKT(CP_UseItem);            // 2a

    };
} MCPacket;

typedef struct {
    lh_arr_declare(MCPacket *,queue);
} MCPacketQueue;

////////////////////////////////////////////////////////////////////////////////

extern int  currentProtocol;

int         set_protocol(int protocol, char * reply);

MCPacket *  decode_packet(int is_client, uint8_t *p, ssize_t len);
ssize_t     encode_packet(MCPacket *pkt, uint8_t *buf);
void        dump_packet(MCPacket *pkt);
void        free_packet  (MCPacket *pkt);
void        queue_packet (MCPacket *pkt, MCPacketQueue *q);
void        packet_queue_transmit(MCPacketQueue *q, MCPacketQueue *pq, tokenbucket *tb);

#define NEWPACKET(type,name)                                                   \
    lh_create_obj(MCPacket,name);                                              \
    name->pid = type;                                                          \
    name->ver = currentProtocol;                                               \
    type##_pkt *t##name = &name->_##type;

////////////////////////////////////////////////////////////////////////////////
