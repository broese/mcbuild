#pragma once

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

#define PID(x) ((x)&0xffffff)

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

////////////////////////////////////////////////////////////////////////////////
// Block IDs

#define I_ITEM   (1<<0)
#define I_MTYPE  (1<<1)
#define I_MSIZE  (1<<2)
#define I_DAMAGE (1<<3)
#define I_64     (0<<4)
#define I_16     (1<<4)
#define I_1      (2<<4)

typedef struct _item_id {
    uint16_t     id;
    uint16_t     flags;
    const char * name;
    const char * mname[16];
} item_id;

static const item_id ITEMS[] = {
    { 0x00, 0, "Air"          },
    { 0x01, 0, "Stone"        }, //new metatypes in 1.8
    { 0x02, 0, "Grass"        },
    { 0x03, 0, "Dirt",        
      { "Normal", "Grassless", "Podzol" }, },
    { 0x04, 0, "Cobblestone", },
    { 0x05, 2, "Woodplanks",  
      { "Oak", "Spruce", "Birch", "Jungle", "Acacia", "Dark Oak" }, },
    { 0x06, 2, "Sapling",     
      { "Oak", "Spruce", "Birch", "Jungle", "Acacia", "Dark Oak" }, },
    { 0x07, 0, "Bedrock",     },
    { 0x08, 4, "Waterflow",   },
    { 0x09, 0, "Water",       },
    { 0x0a, 4, "Lavaflow",    },
    { 0x0b, 0, "Lava",        },
    { 0x0c, 0, "Sand",        
      { "Normal", "Red", }, },
    { 0x0d, 0, "Gravel",      },
    { 0x0e, 0, "Gold ore",    },
    { 0x0f, 0, "Iron ore",    },

    { 0x10, 0, "Coal ore", },
    { 0x11, 6, "Wood",        
      { "Oak UD", "Spruce UD", "Birch UD", "Jungle UD",
        "Oak EW", "Spruce EW", "Birch EW", "Jungle EW",
        "Oak NS", "Spruce NS", "Birch NS", "Jungle NS",
        "Oak bark", "Spruce bark", "Birch bark", "Jungle bark", }, },
    { 0x12, 6, "Leaves", 
      { "Oak", "Spruce", "Birch", "Jungle",
        "Oak ND", "Spruce ND", "Birch ND", "Jungle ND",
        "Oak CD", "Spruce CD", "Birch CD", "Jungle CD",
        "Oak CD ND", "Spruce CD ND", "Birch CD ND", "Jungle CD ND", }, },
    { 0x13, 0, "Sponge", },
    { 0x14, 0, "Glass", },
    { 0x15, 0, "Lapis ore", },
    { 0x16, 0, "Lapis block", },
    { 0x17, 0, "Dispenser", },
    { 0x18, 0, "Sandstone", 
      { "Normal", "Chiseled", "Smooth", }, },
    { 0x19, 0, "Noteblock", },
    { 0x1a, 0, "Bed", },
    { 0x1b, 0, "Powered Rail", },
    { 0x1c, 0, "Detector Rail", },
    { 0x1d, 0, "Sticky Piston", },
    { 0x1e, 0, "Cobweb", },
    { 0x1f, 0, "Tallgrass", },

    { 0x20, 0, "Deadbush", },
    { 0x21, 0, "Piston", },
    { 0x22, 0, "Piston Head", },
    { 0x23, 2, "Wool", 
      { "White", "Orange", "Magenta", "LBlue",
        "Yellow", "Lime", "Pink", "Grey",
        "LGrey", "Cyan", "Purple", "Blue",
        "Brown", "Green", "Red", "Black", }, },
    { 0x24, 0, "Pushed block", },
    { 0x25, 0, "Dandelion", },
    { 0x26, 2, "Flower", 
      { "Poppy", "Blue orchid", "Allium", "Azure Bluet", 
        "Red Tulip", "Orange Tulip", "White Tulip", "Pink Tulip",
        "Oxeye Daisy", }, },
    { 0x27, 0, "Brown Mushroom", },
    { 0x28, 0, "Red Mushroom", },
    { 0x29, 0, "Gold Block", },
    { 0x2a, 0, "Iron Block", },
    { 0x2b, 2, "Double Stone Slab", 
      { "Stone", "Sandstone", "Wood", "Cobble",
        "Brick", "Stone Brick", "Netherbrick", "Quartz",
        "Smooth Stone", "Smooth Sandstone", "Tile Quartz", }, },
    { 0x2c, 2, "Stone Slab",
      { "Stone", "Sandstone", "Wood", "Cobble",
        "Brick", "Stone Brick", "Netherbrick", "Quartz",
        "Smooth Stone", "Smooth Sandstone", "Tile Quartz", }, },
    { 0x2d, 0, "Brick", },
    { 0x2e, 0, "TNT", },
    { 0x2f, 0, "Bookshelf", },

    { 0x30, 0, "Mossy Cobblestone", },
    { 0x31, 0, "Obsidian", },
    { 0x32, 0, "Torch", },
    { 0x33, 0, "Fire", },
    { 0x34, 0, "Spawner", },
    { 0x35, 0, "Wooden Stairs", },
    { 0x36, 0, "Chest", },
    { 0x37, 0, "Redstone Wire", },
    { 0x38, 0, "Diamond Ore", },
    { 0x39, 0, "Diamond Block", },
    { 0x3a, 0, "Workbench", },
    { 0x3b, 0, "Wheat", },
    { 0x3c, 0, "Farmland", },
    { 0x3d, 0, "Furnace", },
    { 0x3e, 0, "Lit Furnace", },
    { 0x3f, 0, "Sign", },

    { 0xffff, 0, NULL, },
};
