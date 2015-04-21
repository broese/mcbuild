#pragma once

#include <stdint.h>
#include "mcp_packet.h"

////////////////////////////////////////////////////////////////////////////////
//// Protocol IDs

#define PROTO_NONE   0x00000000
#define PROTO_1_8_1  0x00010801

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

#define SP_KeepAlive            SP(00)
#define SP_JoinGame             SP(01)
#define SP_ChatMessage          SP(02)
#define SP_TimeUpdate           SP(03)
#define SP_EntityEquipment      SP(04)
#define SP_SpawnPosition        SP(05)
#define SP_UpdateHealth         SP(06)
#define SP_Respawn              SP(07)
#define SP_PlayerPositionLook   SP(08)
#define SP_HeldItemChange       SP(09)
#define SP_UseBed               SP(0a)
#define SP_Animation            SP(0b)
#define SP_SpawnPlayer          SP(0c)
#define SP_CollectItem          SP(0d)
#define SP_SpawnObject          SP(0e)
#define SP_SpawnMob             SP(0f)
#define SP_SpawnPainting        SP(10)
#define SP_SpawnExperienceOrb   SP(11)
#define SP_EntityVelocity       SP(12)
#define SP_DestroyEntities      SP(13)
#define SP_Entity               SP(14)
#define SP_EntityRelMove        SP(15)
#define SP_EntityLook           SP(16)
#define SP_EntityLookRelMove    SP(17)
#define SP_EntityTeleport       SP(18)
#define SP_EntityHeadLook       SP(19)
#define SP_EntityStatus         SP(1a)
#define SP_AttachEntity         SP(1b)
#define SP_EntityMetadata       SP(1c)
#define SP_EntityEffect         SP(1d)
#define SP_RemoveEntityEffect   SP(1e)
#define SP_SetExperience        SP(1f)
#define SP_EntityProperties     SP(20)
#define SP_ChunkData            SP(21)
#define SP_MultiBlockChange     SP(22)
#define SP_BlockChange          SP(23)
#define SP_BlockAction          SP(24)
#define SP_BlockBreakAnimation  SP(25)
#define SP_MapChunkBulk         SP(26)
#define SP_Explosion            SP(27)
#define SP_Effect               SP(28)
#define SP_SoundEffect          SP(29)
#define SP_Particle             SP(2a)
#define SP_ChangeGameState      SP(2b)
#define SP_SpawnGlobalEntity    SP(2c)
#define SP_OpenWindow           SP(2d)
#define SP_CloseWindow          SP(2e)
#define SP_SetSlot              SP(2f)
#define SP_WindowItems          SP(30)
#define SP_WindowProperty       SP(31)
#define SP_ConfirmTransaction   SP(32)
#define SP_UpdateSign           SP(33)
#define SP_Maps                 SP(34)
#define SP_UpdateBlockEntity    SP(35)
#define SP_SignEditorOpen       SP(36)
#define SP_Statistics           SP(37)
#define SP_PlayerListItem       SP(38)
#define SP_PlayerAbilities      SP(39)
#define SP_TabComplete          SP(3a)
#define SP_ScoreboardObjective  SP(3b)
#define SP_UpdateScore          SP(3c)
#define SP_DisplayScoreboard    SP(3d)
#define SP_Teams                SP(3e)
#define SP_PluginMessage        SP(3f)
#define SP_Disconnect           SP(40)
#define SP_ServerDifficulty     SP(41)
#define SP_CombatEffect         SP(42)
#define SP_Camera               SP(43)
#define SP_WorldBorder          SP(44)
#define SP_Title                SP(45)
#define SP_SetCompression       SP(46)
#define SP_PlayerListHeader     SP(47)
#define SP_ResourcePackSent     SP(48)
#define SP_UpdateEntityNbt      SP(49)

#define CP_KeepAlive            CP(00)
#define CP_ChatMessage          CP(01)
#define CP_UseEntity            CP(02)
#define CP_Player               CP(03)
#define CP_PlayerPosition       CP(04)
#define CP_PlayerLook           CP(05)
#define CP_PlayerPositionLook   CP(06)
#define CP_PlayerDigging        CP(07)
#define CP_PlayerBlockPlacement CP(08)
#define CP_HeldItemChange       CP(09)
#define CP_Animation            CP(0a)
#define CP_EntityAction         CP(0b)
#define CP_SteerVehicle         CP(0c)
#define CP_CloseWindow          CP(0d)
#define CP_ClickWindow          CP(0e)
#define CP_ConfirmTransaction   CP(0f)
#define CP_CreativeInventoryAct CP(10)
#define CP_EnchantItem          CP(11)
#define CP_UpdateSign           CP(12)
#define CP_PlayerAbilities      CP(13)
#define CP_TabComplete          CP(14)
#define CP_ClientSettings       CP(15)
#define CP_ClientStatus         CP(16)
#define CP_PluginMessage        CP(17)
#define CP_Spectate             CP(18)
#define CP_ResourcePackStatus   CP(19)

#define MAXPACKETTYPES          0x100

////////////////////////////////////////////////////////////////////////////////
// Block type flags

// the ID is an item, i.e. cannot be placed as a block
#define I_ITEM   (1<<0)

// Item does not stack (or stack size=1)
#define I_NSTACK (1<<1)

// item stacks only by 16 (e.g. enderpearls)
#define I_S16    (1<<2)

// (blocks or inventory) the metadata defines the subtype likr type, color or
// material. The subtype names are available in mname array
#define I_MTYPE  (1<<3)

// the metadata defines the block's state - e.g. gate open/close
#define I_STATE  (1<<4)

// the block type normally has a block entity data attached to it, e.g. signs
#define I_BENTITY (1<<5)

// (placed blocks only) - metadata defines the placement orientation, but we
// provide no specific support for it, so it can be ignored
// Once support for certain block positioning is implemented, remove I_MPOS
// flag and define a separate one (like I_SLAB)
#define I_MPOS   (1<<8)

// slab-type block - I_MPOS lower/upper placement in the meta bit 3
#define I_SLAB   (1<<9)

// stair-type block - I_MPOS straight/upside-down in the meta bit 2, direction in bits 0-1
#define I_STAIR (1<<10)

// wood log type blocks
#define I_LOG (1<<11)

// torches and redstone torches
#define I_TORCH (1<<12)

// ladders, wall signs and wall banners
#define I_ONWALL (1<<13)

// macros to determine armor type
#define I_HELMET(id)     ((id)==0x12a || (id)==0x12e || (id)==0x132 || (id)==0x136 || (id)==0x13a)
#define I_CHESTPLATE(id) ((id)==0x12b || (id)==0x12f || (id)==0x133 || (id)==0x137 || (id)==0x13b)
#define I_LEGGINGS(id)   ((id)==0x12c || (id)==0x130 || (id)==0x134 || (id)==0x138 || (id)==0x13c)
#define I_BOOTS(id)      ((id)==0x12d || (id)==0x131 || (id)==0x135 || (id)==0x139 || (id)==0x13d)
#define I_ARMOR(id)      (I_HELMET(id) || I_CHESTPLATE(id) || I_LEGGINGS(id) || I_BOOTS(id))

typedef struct _item_id {
    const char * name;
    uint64_t     flags;
    const char * mname[16];
} item_id;

extern const item_id ITEMS[];

#define STACKSIZE(item) \
    ( ITEMS[item].flags&I_NSTACK ? 1 : ( ITEMS[item].flags&I_S16 ? 16 : 64 ) )

////////////////////////////////////////////////////////////////////////////////
// Directions

#define DIR_ANY    -1
#define DIR_UP      0
#define DIR_DOWN    1
#define DIR_SOUTH   2
#define DIR_NORTH   3
#define DIR_EAST    4
#define DIR_WEST    5

extern const char ** DIRNAME;

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

typedef enum {
    //// Superclasses
    IllegalEntityType   = -1,
    Entity              = 0,
    LivingEntity        = 1,
    Ageable             = 2,
    Human               = 3,
    Tameable            = 4,
    Item                = 5,
    Firework            = 6,
    Mob                 = 48,
    Monster             = 49,


    //// Mobs
    Creeper             = 50,
    Skeleton            = 51,
    Spider              = 52,
    GiantZombie         = 53,
    Zombie              = 54,
    Slime               = 55,
    Ghast               = 56,
    ZombiePigman        = 57,
    Enderman            = 58,
    CaveSpider          = 59,

    Silverfish          = 60,
    Blaze               = 61,
    MagmaCube           = 62,
    Enderdragon         = 63,
    Wither              = 64,
    Bat                 = 65,
    Witch               = 66,
    Endermite           = 67,
    Guardian            = 68,

    Pig                 = 90,
    Sheep               = 91,
    Cow                 = 92,
    Chicken             = 93,
    Squid               = 94,
    Wolf                = 95,
    Mooshroom           = 96,
    Snowman             = 97,
    Ocelot              = 98,
    IronGolem           = 99,

    Horse               = 100,
    Rabbit              = 101,

    Villager            = 120,

    //// Objects
    Boat                = 1 +256,
    ItemStack           = 2 +256,
    Minecart            = 10+256,
    ChestMinecart       = 11+256, // deprecated since 1.6
    FurnaceMinecart     = 12+256, // deprecated since 1.6
    EnderCrystal        = 51+256,
    Arrow               = 60+256,
    ItemFrame           = 71+256,
    ArmorStand          = 78+256,
    //TODO: other objects

    //// Terminating ID
    MaxEntityType       = 512,
} EntityType;

extern const char * METANAME[][32];
extern const EntityType ENTITY_HIERARCHY[];
extern const char * ENTITY_NAMES[];
const char * get_entity_name(char *buf, EntityType type);

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

#define META_BYTE    0
#define META_SHORT   1
#define META_INT     2
#define META_FLOAT   3
#define META_STRING  4
#define META_SLOT    5
#define META_COORD   6
#define META_ROT     7

static const char * METATYPES[] = {
    [META_BYTE]       = "byte",
    [META_SHORT]      = "short",
    [META_INT]        = "int",
    [META_FLOAT]      = "float",
    [META_STRING]     = "string",
    [META_SLOT]       = "slot",
    [META_COORD]      = "coord",
    [META_ROT]        = "rot",
};

////////////////////////////////////////////////////////////////////////////////
// ANSI representation of blocks

#define ANSI_CLEAR     "\x1b[0m"
#define ANSI_PLAYER    "\x1b[5;32;44m$\x1b[0m"
#define ANSI_UNKNOWN   "\x1b[45;36m#"
#define ANSI_ILLBLOCK  "\x1b[5;32;44m#\x1b[0m"

extern const char * ANSI_BLOCK[];

////////////////////////////////////////////////////////////////////////////////

const char * get_item_name(char *buf, slot_t *s);
const char * get_bid_name(char *buf, bid_t b);
bid_t get_base_material(bid_t mat);
uint8_t get_state_mask(int bid);

bid_t meta_n2d(bid_t b, int dir);
bid_t meta_d2n(bid_t b, int dir);
