/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <stdint.h>

#include "mcp_types.h"
#include "slot.h"

////////////////////////////////////////////////////////////////////////////////
//// Protocol IDs

#define PROTO_NONE   0x00000000
#define PROTO_1_8_1  0x00010801
#define PROTO_1_9    0x00010900

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
#define SP_TabComplete          SP(0e)
#define SP_ChatMessage          SP(0f)
#define SP_MultiBlockChange     SP(10)
#define SP_ConfirmTransaction   SP(11)
#define SP_CloseWindow          SP(12)
#define SP_OpenWindow           SP(13)
#define SP_WindowItems          SP(14)
#define SP_WindowProperty       SP(15)
#define SP_SetSlot              SP(16)
#define SP_SetCooldown          SP(17)
#define SP_PluginMessage        SP(18)
#define SP_NamedSoundEffect     SP(19)
#define SP_Disconnect           SP(1a)
#define SP_EntityStatus         SP(1b)
#define SP_Explosion            SP(1c)
#define SP_UnloadChunk          SP(1d)
#define SP_ChangeGameState      SP(1e)
#define SP_KeepAlive            SP(1f)
#define SP_ChunkData            SP(20)
#define SP_Effect               SP(21)
#define SP_Particle             SP(22)
#define SP_JoinGame             SP(23)
#define SP_Map                  SP(24)
#define SP_EntityRelMove        SP(25)
#define SP_EntityLookRelMove    SP(26)
#define SP_EntityLook           SP(27)
#define SP_Entity               SP(28)
#define SP_VehicleMove          SP(29)
#define SP_SignEditorOpen       SP(2a)
#define SP_PlayerAbilities      SP(2b)
#define SP_CombatEffect         SP(2c)
#define SP_PlayerListItem       SP(2d)
#define SP_PlayerPositionLook   SP(2e)
#define SP_UseBed               SP(2f)
#define SP_DestroyEntities      SP(30)
#define SP_RemoveEntityEffect   SP(31)
#define SP_ResourcePackSent     SP(32)
#define SP_Respawn              SP(33)
#define SP_EntityHeadLook       SP(34)
#define SP_WorldBorder          SP(35)
#define SP_Camera               SP(36)
#define SP_HeldItemChange       SP(37)
#define SP_DisplayScoreboard    SP(38)
#define SP_EntityMetadata       SP(39)
#define SP_AttachEntity         SP(3a)
#define SP_EntityVelocity       SP(3b)
#define SP_EntityEquipment      SP(3c)
#define SP_SetExperience        SP(3d)
#define SP_UpdateHealth         SP(3e)
#define SP_ScoreboardObjective  SP(3f)
#define SP_SetPassengers        SP(40)
#define SP_Teams                SP(41)
#define SP_UpdateScore          SP(42)
#define SP_SpawnPosition        SP(43)
#define SP_TimeUpdate           SP(44)
#define SP_Title                SP(45)
#define SP_UpdateSign           SP(46)
#define SP_SoundEffect          SP(47)
#define SP_PlayerListHeader     SP(48)
#define SP_CollectItem          SP(49)
#define SP_EntityTeleport       SP(4a)
#define SP_EntityProperties     SP(4b)
#define SP_EntityEffect         SP(4c)

#define CP_TeleportConfirm      CP(00)
#define CP_TabComplete          CP(01)
#define CP_ChatMessage          CP(02)
#define CP_ClientStatus         CP(03)
#define CP_ClientSettings       CP(04)
#define CP_ConfirmTransaction   CP(05)
#define CP_EnchantItem          CP(06)
#define CP_ClickWindow          CP(07)
#define CP_CloseWindow          CP(08)
#define CP_PluginMessage        CP(09)
#define CP_UseEntity            CP(0a)
#define CP_KeepAlive            CP(0b)
#define CP_PlayerPosition       CP(0c)
#define CP_PlayerPositionLook   CP(0d)
#define CP_PlayerLook           CP(0e)
#define CP_Player               CP(0f)
#define CP_VehicleMode          CP(10)
#define CP_SteerBoat            CP(11)
#define CP_PlayerAbilities      CP(12)
#define CP_PlayerDigging        CP(13)
#define CP_EntityAction         CP(14)
#define CP_SteerVehicle         CP(15)
#define CP_ResourcePackStatus   CP(16)
#define CP_HeldItemChange       CP(17)
#define CP_CreativeInventoryAct CP(18)
#define CP_UpdateSign           CP(19)
#define CP_Animation            CP(1a)
#define CP_Spectate             CP(1b)
#define CP_PlayerBlockPlacement CP(1c)

#define MAXPACKETTYPES          0x100

////////////////////////////////////////////////////////////////////////////////
// Block type flags

// bitmasks of the meta bits used as a state (i.e. affected by the game
// but not directly adjustable by the player)
#define I_STATE_MASK  15

#define I_STATE_1 1
#define I_STATE_4 4
#define I_STATE_8 8
#define I_STATE_C 12
#define I_STATE_F 15

// the ID is an item, i.e. cannot be placed as a block
#define I_ITEM   (1<<4)

// Item does not stack (or stack size=1)
#define I_NSTACK (1<<5)

// item stacks only by 16 (e.g. enderpearls)
#define I_S16    (1<<6)

// (blocks or inventory) the metadata defines the subtype like type, color or
// material. The subtype names are available in mname array
#define I_MTYPE  (1<<7)

// the block type normally has a block entity data attached to it, e.g. signs
#define I_BENTITY (1<<8)

// blocks that are completely opaque - i.e. fill out the entire block
// and do not have transparent areas
#define I_OPAQUE (1<<9)

// edible (and preferred) foods
#define I_FOOD (1<<10)

// metadata defines the placement orientation
#define I_MPOS   (1<<11)

// Container blocks (chests, furnaces, hoppers, etc. - dialog opens if right-clicked)
#define I_CONT   (1<<12)

// Blocks with adjustable setting (through right-click)
#define I_ADJ    (1<<13)

// slab-type block - I_MPOS lower/upper placement in the meta bit 3
#define I_SLAB   (1<<16)

// stair-type block - I_MPOS straight/upside-down in the meta bit 2, direction in bits 0-1
#define I_STAIR (1<<17)

// wood log type blocks
#define I_LOG (1<<18)

// torches and redstone torches
#define I_TORCH (1<<19)

// ladders, wall signs and wall banners
#define I_ONWALL (1<<20)

// double-slabs
#define I_DSLAB (1<<21)

// redstone switches
#define I_RSRC (1<<22)

// redstone devices (hoppers, dispensers, droppers, pistons)
#define I_RSDEV (1<<23)

// doors
#define I_DOOR (1<<24)

// trapdoors
#define I_TDOOR (1<<25)

// crops (planted on farmland)
#define I_PLANT (1<<26)

// oriented containers - chests and furnaces
#define I_CHEST (1<<27)

// fence gates
#define I_GATE (1<<28)

// macros to determine armor type
#define I_HELMET(id)     ((id)==0x12a || (id)==0x12e || (id)==0x132 || (id)==0x136 || (id)==0x13a)
#define I_CHESTPLATE(id) ((id)==0x12b || (id)==0x12f || (id)==0x133 || (id)==0x137 || (id)==0x13b)
#define I_LEGGINGS(id)   ((id)==0x12c || (id)==0x130 || (id)==0x134 || (id)==0x138 || (id)==0x13c)
#define I_BOOTS(id)      ((id)==0x12d || (id)==0x131 || (id)==0x135 || (id)==0x139 || (id)==0x13d)
#define I_ELYTRA(id)     ((id)==0x1bb)
#define I_ARMOR(id)      (I_HELMET(id) || I_CHESTPLATE(id) || I_LEGGINGS(id) || I_BOOTS(id) || I_ELYTRA(id))

typedef struct _item_id {
    const char * name;
    uint64_t     flags;
    const char * mname[16];
} item_id;

extern const item_id ITEMS[];

#define STACKSIZE(item) \
    ( ITEMS[item].flags&I_NSTACK ? 1 : ( ITEMS[item].flags&I_S16 ? 16 : 64 ) )

#define BLOCKTYPE(b,m) (bid_t){ { { .bid = b, .meta = m } } }

////////////////////////////////////////////////////////////////////////////////

// block types we should exclude from scanning
static inline int NOSCAN(int bid) {
    return ( bid==0x00 ||               // air
             bid==0x08 || bid==0x09 ||  // water
             bid==0x0a || bid==0x0b ||  // lava
             bid==0x1f ||               // tallgrass
             bid==0x22 ||               // piston head
             bid==0x24 ||               // piston extension
             bid==0x33 ||               // fire
             bid==0x3b ||               // wheat
             bid==0x4e ||               // snow layer
             bid==0x5a ||               // portal field
             //bid==0x63 || bid==0x64 || // giant mushrooms
             bid==0x8d || bid==0x8e     // carrots, potatoes
             );
}

// block types that are considered 'empty' for the block placement
static inline int ISEMPTY(int bid) {
    return ( bid==0x00 ||               // air
             bid==0x08 || bid==0x09 ||  // water
             bid==0x0a || bid==0x0b ||  // lava
             bid==0x1f ||               // tallgrass
             bid==0x33 ||               // fire
             bid==0x4e                  // snow layer
    );
}

////////////////////////////////////////////////////////////////////////////////

#include "mcp_types.h"

const char * get_item_name(char *buf, slot_t *s);
const char * get_bid_name(char *buf, bid_t b);

int find_bid_name(const char *name);
int find_meta_name(int bid, const char *name);

bid_t get_base_material(bid_t mat);
uint8_t get_state_mask(int bid);
bid_t get_base_block_material(bid_t mat);

bid_t rotate_meta(bid_t b, int times);
int numrot(int from_dir, int to_dir);
bid_t flip_meta(bid_t b, char mode);
bid_t flip_meta_y(bid_t b);
