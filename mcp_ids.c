#include "mcp_ids.h"

#include <stdlib.h>
#include <stdio.h>

#define MNAMES_WOOD     { "Oak", "Spruce", "Birch", "Jungle", "Acacia", "Dark Oak" }
#define MNAMES_OLDWOOD  { "Oak", "Spruce", "Birch", "Jungle" }
#define MNAMES_NEWWOOD  { "Acacia", "Dark Oak", NULL, NULL }
#define MNAMES_COLOR    { "White", "Orange", "Magenta", "LBlue", "Yellow", "Lime", "Pink", "Gray", \
                          "LGray", "Cyan", "Purple", "Blue", "Brown", "Green", "Red", "Black" }

const item_id ITEMS[] = {
    ////////////////////////////////////////////////////////////////////////
    // Blocks

    [0x00] = { "Air" },
    [0x01] = { "Stone",                 I_MTYPE|I_OPAQUE,
               { NULL, "Granite", "P. Granite", "Diorite", "P. Diorite",
                 "Andesite", "P. Andesite" } },
    [0x02] = { "Grass",                 I_OPAQUE },
    [0x03] = { "Dirt",                  I_MTYPE|I_OPAQUE,
               { NULL, "Coarse", "Podzol" } },
    [0x04] = { "Cobblestone",           I_OPAQUE },
    [0x05] = { "Woodplanks",            I_MTYPE|I_OPAQUE,
               MNAMES_WOOD },
    [0x06] = { "Sapling",               I_MTYPE|I_STATE,                // S: growth
               MNAMES_WOOD },
    [0x07] = { "Bedrock",               I_OPAQUE },
    [0x08] = { "Waterflow",             I_STATE },                      // S: level/drop
    [0x09] = { "Water",                 I_STATE },                      // S: level
    [0x0a] = { "Lavaflow",              I_STATE },                      // S: level/drop
    [0x0b] = { "Lava",                  I_STATE },                      // S: level
    [0x0c] = { "Sand",                  I_MTYPE|I_OPAQUE,
               { NULL, "Red" } },
    [0x0d] = { "Gravel",                I_OPAQUE },
    [0x0e] = { "Gold ore",              I_OPAQUE },
    [0x0f] = { "Iron ore",              I_OPAQUE },

    [0x10] = { "Coal ore",              I_OPAQUE },
    [0x11] = { "Wood",                  I_MTYPE|I_MPOS|I_LOG|I_OPAQUE,  // P: dir
               MNAMES_OLDWOOD },
    [0x12] = { "Leaves",                I_MTYPE|I_STATE,                // S: decay
               MNAMES_OLDWOOD },
    [0x13] = { "Sponge",                I_MTYPE|I_OPAQUE,
               { NULL, "Wet" }, },
    [0x14] = { "Glass" },
    [0x15] = { "Lapis ore",             I_OPAQUE },
    [0x16] = { "Lapis block",           I_OPAQUE },
    [0x17] = { "Dispenser",             I_MPOS|I_STATE|I_OPAQUE },      // P: dir, S: active
    [0x18] = { "Sandstone",             I_MTYPE|I_OPAQUE,
               { NULL, "Chiseled", "Smooth"} },
    [0x19] = { "Noteblock",             I_OPAQUE },
    [0x1a] = { "Bed",                   I_MPOS|I_STATE },               // P: dir, S: occupied
    [0x1b] = { "Powered Rail",          I_MPOS|I_STATE },               // P: dir, S: active
    [0x1c] = { "Detector Rail",         I_MPOS|I_STATE },               // P: dir, S: active
    [0x1d] = { "Sticky Piston",         I_MPOS|I_STATE },               // P: dir, S: extended
    [0x1e] = { "Cobweb", },
    [0x1f] = { "Tallgrass",             I_MTYPE,
               { "Shrub", NULL, "Fern", }, },

    [0x20] = { "Deadbush" },
    [0x21] = { "Piston",                I_MPOS|I_STATE },               // P: dir, S: extended
    [0x22] = { "Piston Head",           I_MPOS|I_STATE },               // P: dir, S: extended
    [0x23] = { "Wool",                  I_MTYPE, MNAMES_COLOR },
    [0x24] = { "Pushed block", },
    [0x25] = { "Dandelion", },
    [0x26] = { "Flower",                I_MTYPE,
               { "Poppy", "Blue orchid", "Allium", "Azure Bluet", 
                 "Red Tulip", "Orange Tulip", "White Tulip", "Pink Tulip",
                 "Oxeye Daisy", }, },
    [0x27] = { "Brown Mushroom" },
    [0x28] = { "Red Mushroom" },
    [0x29] = { "Gold Block",            I_OPAQUE },
    [0x2a] = { "Iron Block",            I_OPAQUE },
    [0x2b] = { "Double Slab",           I_MTYPE|I_DSLAB|I_OPAQUE,
               { "Stone", "Sandstone", "Stonewood", "Cobble",
                 "Brick", "Stone Brick", "Netherbrick", "Quartz",
                 "Smooth Stone", "Smooth Sandstone",
                 [15] = "Tile Quartz", }, },
    [0x2c] = { "Stone Slab",            I_MTYPE|I_MPOS|I_SLAB,          // P: U/D
               { NULL, "Sandstone", "Stonewood", "Cobble",
                 "Brick", "Stone Brick", "Netherbrick", "Quartz" } },
    [0x2d] = { "Brick",                 I_OPAQUE },
    [0x2e] = { "TNT",                   I_OPAQUE },
    [0x2f] = { "Bookshelf",             I_OPAQUE },

    [0x30] = { "Mossy Stone",           I_OPAQUE },
    [0x31] = { "Obsidian",              I_OPAQUE },
    [0x32] = { "Torch",                 I_MPOS|I_TORCH },               // P: dir
    [0x33] = { "Fire",                  I_STATE },                      // S: age
    [0x34] = { "Spawner" },
    [0x35] = { "Wooden Stairs",         I_MPOS|I_STAIR },               // P: dir
    [0x36] = { "Chest",                 I_MPOS },                       // P: dir
    [0x37] = { "Redstone Wire",         I_STATE },                      // S: power level
    [0x38] = { "Diamond Ore",           I_OPAQUE },
    [0x39] = { "Diamond Block",         I_OPAQUE },
    [0x3a] = { "Workbench",             I_OPAQUE },
    [0x3b] = { "Wheat",                 I_STATE },                      // S: level
    [0x3c] = { "Farmland",              I_STATE },                      // S: wetness
    [0x3d] = { "Furnace",               I_MPOS|I_OPAQUE },              // P: dir
    [0x3e] = { "Lit Furnace",           I_MPOS|I_OPAQUE },              // P: dir
    [0x3f] = { "Standing Sign",         I_MPOS },                       // P: dir

    [0x40] = { "Wooden Door",           I_MPOS|I_STATE },               // P: dir, S: open/close
    [0x41] = { "Ladder",                I_MPOS|I_ONWALL },              // P: dir
    [0x42] = { "Rail",                  I_MPOS },                       // P: dir
    [0x43] = { "Cobblestone Stairs",    I_MPOS|I_STAIR },               // P: dir
    [0x44] = { "Wall Sign",             I_MPOS|I_ONWALL },              // P: dir
    [0x45] = { "Lever",                 I_MPOS|I_STATE },               // P: dir, S: up/down
    [0x46] = { "Stone Pressure Plate",  I_STATE },                      // S: pressed
    [0x47] = { "Iron Door",             I_MPOS|I_STATE },               // P: dir, S: open/close
    [0x48] = { "Wooden Pressure Plate", I_STATE },                      // S: pressed
    [0x49] = { "Redstone Ore",          I_OPAQUE },
    [0x4a] = { "Glowing Redstone Ore" },
    [0x4b] = { "Redstone Torch (off)",  I_MPOS|I_TORCH },               // P: dir
    [0x4c] = { "Redstone Torch (on)",   I_MPOS|I_TORCH },               // P: dir
    [0x4d] = { "Stone Button",          I_MPOS|I_STATE },               // P: dir, S: pressed
    [0x4e] = { "Snow Layer",            I_STATE },                      // P: level
    [0x4f] = { "Ice" },

    [0x50] = { "Snow",                  I_OPAQUE },
    [0x51] = { "Cactus",                I_STATE },                      // S: grow
    [0x52] = { "Clay Block",            I_OPAQUE },
    [0x53] = { "Sugar Cane",            I_STATE },                      // S: grow
    [0x54] = { "Jukebox",               I_STATE|I_OPAQUE },             // S: has a disc
    [0x55] = { "Fence" },
    [0x56] = { "Pumpkin",               I_MPOS|I_OPAQUE },              // P: dir
    [0x57] = { "Netherrack",            I_OPAQUE },
    [0x58] = { "Soul Sand",             I_OPAQUE },
    [0x59] = { "Glowstone Block",       I_OPAQUE },
    [0x5a] = { "Portal",                I_MPOS },
    [0x5b] = { "Jack-o-Lantern",        I_MPOS|I_OPAQUE },
    [0x5c] = { "Cake",                  I_STATE },                      // S: level
    [0x5d] = { "Repeater (off)",        I_MPOS|I_STATE },               // P: dir, S: delay setting
    [0x5e] = { "Repeater (on)",         I_MPOS|I_STATE },               // P: dir, S: delay setting
    [0x5f] = { "Stained Glass",         I_MTYPE, MNAMES_COLOR },

    [0x60] = { "Trapdoor",              I_MPOS|I_STATE },               // P: dir,u/d, S: open/close
    [0x61] = { "Silverfish Block",      I_MTYPE|I_OPAQUE,
               { "Stone", "Cobblestone", "Stonebrick", "Mossy Stonebrick",
                 "Cracked Stonebrick", "Chiseled Stonebrick" }, },
    [0x62] = { "Stonebrick",            I_MTYPE|I_OPAQUE,
               { NULL, "Mossy", "Cracked", "Chiseled" }, },
    [0x63] = { "Brown Mushroom Block",  I_MPOS|I_OPAQUE },              // P: part
    [0x64] = { "Red Mushroom Block",    I_MPOS|I_OPAQUE },              // P: part
    [0x65] = { "Iron Bars" },
    [0x66] = { "Glass Pane" },
    [0x67] = { "Melon Block",           I_OPAQUE },
    [0x68] = { "Pumpkin Stem",          I_STATE },                      // S: level
    [0x69] = { "Melon Stem",            I_STATE },                      // S: level
    [0x6a] = { "Vine",                  I_MPOS },                       // P: dir
    [0x6b] = { "Fence Gate",            I_MPOS|I_STATE },               // P: dir, S: open/close
    [0x6c] = { "Brick Stairs",          I_MPOS|I_STAIR },               // P: dir
    [0x6d] = { "Stone Brick Stairs",    I_MPOS|I_STAIR },               // P: dir
    [0x6e] = { "Mycelium",              I_OPAQUE },
    [0x6f] = { "Lilypad",               I_MPOS },

    [0x70] = { "Netherbrick",           I_OPAQUE },
    [0x71] = { "Netherbrick Fence" },
    [0x72] = { "Netherbrick Stairs",    I_MPOS|I_STAIR },               // P: dir
    [0x73] = { "Nether Wart",           I_STATE },                      // S: level
    [0x74] = { "Enchantment Table" },
    [0x75] = { "Brewing Stand",         I_STATE },                      // S: bottles
    [0x76] = { "Cauldron",              I_STATE },                      // S: level
    [0x77] = { "End Portal",            I_MPOS },
    [0x78] = { "End Portal Block",      I_MPOS },                       // P: dir
    [0x79] = { "End Stone",             I_OPAQUE },
    [0x7a] = { "Dragon Egg" },
    [0x7b] = { "Redstone Lamp",         I_OPAQUE },
    [0x7c] = { "Redstone Lamp (lit)",   I_OPAQUE },
    [0x7d] = { "Wooden Double Slab",    I_MTYPE|I_DSLAB|I_OPAQUE,
               MNAMES_WOOD },
    [0x7e] = { "Wooden Slab",           I_MTYPE|I_MPOS|I_SLAB,          // P: up/down
               MNAMES_WOOD },
    [0x7f] = { "Cocoa",                 I_STATE },                      // S: level

    [0x80] = { "Sandstone Stairs",      I_MPOS|I_STAIR },               // P: dir
    [0x81] = { "Emerald Ore",           I_OPAQUE },
    [0x82] = { "Ender Chest",           I_MPOS },                       // P: dir
    [0x83] = { "Tripwire Hook",         I_MPOS|I_STATE },               // P: dir, S: connected,active
    [0x84] = { "Tripwire",              I_STATE },                      // S: act/susp/att/arm
    [0x85] = { "Emerald Block",         I_OPAQUE },
    [0x86] = { "Spruce Stairs",         I_MPOS|I_STAIR },               // P: dir
    [0x87] = { "Birch Stairs",          I_MPOS|I_STAIR },               // P: dir
    [0x88] = { "Jungle Stairs",         I_MPOS|I_STAIR },               // P: dir
    [0x89] = { "Command Block",         I_OPAQUE },
    [0x8a] = { "Beacon" },
    [0x8b] = { "Cobblestone Wall",      I_MTYPE,
               { NULL, "Mossy" } },
    [0x8c] = { "Flower Pot" },
    [0x8d] = { "Carrot Plant",          I_STATE },                      // S: level
    [0x8e] = { "Potato Plant",          I_STATE },                      // S: level
    [0x8f] = { "Wooden Button",         I_MPOS|I_STATE },               // P: dir, S: pressed

    [0x90] = { "Skull",                 I_MPOS }, //FIXME: different block/item metas
    [0x91] = { "Anvil",                 I_MPOS }, //FIXME: different block/item metas
    [0x92] = { "Trapped Chest",         I_MPOS },                       // P: dir
    [0x93] = { "Pressure Plate Lt",     I_STATE },                      // S: pressed
    [0x94] = { "Pressure Plate Hv",     I_STATE },                      // S: pressed
    [0x95] = { "Comparator (off)",      I_MPOS|I_STATE },               // P: dir, S: mode/power
    [0x96] = { "Comparator (on)",       I_MPOS|I_STATE },               // P: dir, S: mode/power
    [0x97] = { "Daylight Sensor" },
    [0x98] = { "Redstone Block",        I_OPAQUE },
    [0x99] = { "Quartz Ore",            I_OPAQUE },
    [0x9a] = { "Hopper",                I_MPOS|I_STATE },               // P: dir, S: active
    [0x9b] = { "Quartz Block",          I_MTYPE|I_MPOS|I_OPAQUE,        // P: dir (pillar only)
               { NULL, "Chiseled", "Pillar" } },
    [0x9c] = { "Quartz Stairs",         I_MPOS|I_STAIR },               // P: dir
    [0x9d] = { "Activator Rail",        I_MPOS|I_STATE },               // P: dir, S: active
    [0x9e] = { "Dropper",               I_MPOS|I_STATE|I_OPAQUE },      // P: dir, S: active
    [0x9f] = { "Stained Clay",          I_MTYPE|I_OPAQUE,
               MNAMES_COLOR },

    [0xa0] = { "Stained Glass Pane",    I_MTYPE,
               MNAMES_COLOR },
    [0xa1] = { "Leaves2",               I_MTYPE|I_STATE,                // S: decay
               MNAMES_NEWWOOD },
    [0xa2] = { "Wood2",                 I_MTYPE|I_MPOS|I_LOG|I_OPAQUE,  // P: dir
               MNAMES_NEWWOOD },
    [0xa3] = { "Acacia Stairs",         I_MPOS|I_STAIR },               // P: dir
    [0xa4] = { "Dark Oak Stairs",       I_MPOS|I_STAIR },               // P: dir
    [0xa5] = { "Slime Block" },
    [0xa6] = { "Barrier" },
    [0xa7] = { "Iron Trapdoor",         I_MPOS|I_STATE },               // P: dir,u/d, S: open/close
    [0xa8] = { "Prismarine",            I_MTYPE|I_OPAQUE,
               { NULL, "Brick", "Dark" }, },
    [0xa9] = { "Sea Lantern",           I_OPAQUE },
    [0xaa] = { "Hay Bale",              I_MTYPE|I_MPOS|I_LOG|I_OPAQUE },// P: dir
    [0xab] = { "Carpet",                I_MTYPE,
               MNAMES_COLOR },
    [0xac] = { "Hardened Clay",         I_MTYPE|I_OPAQUE },
    [0xad] = { "Coal Block",            I_OPAQUE },
    [0xae] = { "Packed Ice",            I_OPAQUE },
    [0xaf] = { "Large Flower",          I_MTYPE|I_STATE,                // S: top/bottom
               { "Sunflower", "Lilac", "DblTallgrass", "Large Fern",
                 "Rose Bush", "Peony", NULL, NULL }, },

    [0xb0] = { "Standing Banner",       I_MPOS },                       // P: dir
    [0xb1] = { "Wall Banner",           I_MPOS|I_ONWALL },              // P: dir
    [0xb2] = { "Inv. Daylight Sensor" },
    [0xb3] = { "Red Sandstone",         I_MTYPE|I_OPAQUE,
               { NULL, "Chiseled", "Smooth"} },
    [0xb4] = { "Red Sandstone Stairs",  I_MPOS|I_STAIR },               // P: dir
    [0xb5] = { "Red Sandstone D-Slab",  I_MTYPE|I_DSLAB|I_OPAQUE,
               { [8]="Chiseled" }, },
    [0xb6] = { "Red Sandstone Slab",    I_MPOS|I_SLAB },                // P: up/down
    [0xb7] = { "Spruce Fence Gate",     I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xb8] = { "Birch Fence Gate",      I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xb9] = { "Jungle Fence Gate",     I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xba] = { "Dark Oak Fence Gate",   I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xbb] = { "Acacia Fence Gate",     I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xbc] = { "Spruce Fence" },
    [0xbd] = { "Birch Fence" },
    [0xbe] = { "Jungle Fence" },
    [0xbf] = { "Dark Oak Fence" },

    [0xc0] = { "Acacia Fence" },
    [0xc1] = { "Spruce Door",           I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xc2] = { "Birch Door",            I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xc3] = { "Jungle Door",           I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xc4] = { "Dark Oak Door",         I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xc5] = { "Acacia Door",           I_MPOS|I_STATE },               // P: dir, S: open/close

    ////////////////////////////////////////////////////////////////////////
    // Items

    [0x100] = { "Iron Shovel",          I_ITEM|I_NSTACK },
    [0x101] = { "Iron Pickaxe",         I_ITEM|I_NSTACK },
    [0x102] = { "Iron Axe",             I_ITEM|I_NSTACK },
    [0x103] = { "Flint and Steel",      I_ITEM|I_NSTACK },
    [0x104] = { "Apple",                I_ITEM },
    [0x105] = { "Bow",                  I_ITEM|I_NSTACK },
    [0x106] = { "Arrow",                I_ITEM },
    [0x107] = { "Coal",                 I_ITEM|I_MTYPE,
                { NULL, "Charcoal" }, },
    [0x108] = { "Diamond",              I_ITEM },
    [0x109] = { "Iron Ingot",           I_ITEM },
    [0x10a] = { "Gold Ingot",           I_ITEM },
    [0x10b] = { "Iron Sword",           I_ITEM|I_NSTACK },
    [0x10c] = { "Wooden Sword",         I_ITEM|I_NSTACK },
    [0x10d] = { "Wooden Shovel",        I_ITEM|I_NSTACK },
    [0x10e] = { "Wooden Pickaxe",       I_ITEM|I_NSTACK },
    [0x10f] = { "Wooden Axe",           I_ITEM|I_NSTACK },

    [0x110] = { "Stone Sword",          I_ITEM|I_NSTACK },
    [0x111] = { "Stone Shovel",         I_ITEM|I_NSTACK },
    [0x112] = { "Stone Pickaxe",        I_ITEM|I_NSTACK },
    [0x113] = { "Stone Axe",            I_ITEM|I_NSTACK },
    [0x114] = { "Diamond Sword",        I_ITEM|I_NSTACK },
    [0x115] = { "Diamond Shovel",       I_ITEM|I_NSTACK },
    [0x116] = { "Diamond Pickaxe",      I_ITEM|I_NSTACK },
    [0x117] = { "Diamond Axe",          I_ITEM|I_NSTACK },
    [0x118] = { "Stick",                I_ITEM },
    [0x119] = { "Bowl",                 I_ITEM },
    [0x11a] = { "Mushroom Stew",        I_ITEM|I_NSTACK|I_FOOD },
    [0x11b] = { "Golden Sword",         I_ITEM|I_NSTACK },
    [0x11c] = { "Golden Shovel",        I_ITEM|I_NSTACK },
    [0x11d] = { "Golden Pickaxe",       I_ITEM|I_NSTACK },
    [0x11e] = { "Golden Axe",           I_ITEM|I_NSTACK },
    [0x11f] = { "String",               I_ITEM },

    [0x120] = { "Feather",              I_ITEM },
    [0x121] = { "Gunpowder",            I_ITEM },
    [0x122] = { "Wooden Hoe",           I_ITEM|I_NSTACK },
    [0x123] = { "Stone Hoe",            I_ITEM|I_NSTACK },
    [0x124] = { "Iron Hoe",             I_ITEM|I_NSTACK },
    [0x125] = { "Diamond Hoe",          I_ITEM|I_NSTACK },
    [0x126] = { "Golden Hoe",           I_ITEM|I_NSTACK },
    [0x127] = { "Wheat Seeds",          I_ITEM },
    [0x128] = { "Wheat",                I_ITEM },
    [0x129] = { "Bread",                I_ITEM|I_FOOD },
    [0x12a] = { "Leather Helmet",       I_ITEM|I_NSTACK },
    [0x12b] = { "Leather Chestplate",   I_ITEM|I_NSTACK },
    [0x12c] = { "Leather Leggings",     I_ITEM|I_NSTACK },
    [0x12d] = { "Leather Boots",        I_ITEM|I_NSTACK },
    [0x12e] = { "Chainmail Helmet",     I_ITEM|I_NSTACK },
    [0x12f] = { "Chainmail Chestplate", I_ITEM|I_NSTACK },

    [0x130] = { "Chainmail Leggings",   I_ITEM|I_NSTACK },
    [0x131] = { "Chainmail Boots",      I_ITEM|I_NSTACK },
    [0x132] = { "Iron Helmet",          I_ITEM|I_NSTACK },
    [0x133] = { "Iron Chestplate",      I_ITEM|I_NSTACK },
    [0x134] = { "Iron Leggings",        I_ITEM|I_NSTACK },
    [0x135] = { "Iron Boots",           I_ITEM|I_NSTACK },
    [0x136] = { "Diamond Helmet",       I_ITEM|I_NSTACK },
    [0x137] = { "Diamond Chestplate",   I_ITEM|I_NSTACK },
    [0x138] = { "Diamond Leggings",     I_ITEM|I_NSTACK },
    [0x139] = { "Diamond Boots",        I_ITEM|I_NSTACK },
    [0x13a] = { "Golden Helmet",        I_ITEM|I_NSTACK },
    [0x13b] = { "Golden Chestplate",    I_ITEM|I_NSTACK },
    [0x13c] = { "Golden Leggings",      I_ITEM|I_NSTACK },
    [0x13d] = { "Golden Boots",         I_ITEM|I_NSTACK },
    [0x13e] = { "Flint",                I_ITEM },
    [0x13f] = { "Porkchop",             I_ITEM },

    [0x140] = { "Cooked Porkchop",      I_ITEM|I_FOOD },
    [0x141] = { "Painting",             I_ITEM },
    [0x142] = { "Golden Apple",         I_ITEM,
                { NULL, "Enchanted" }, },
    [0x143] = { "Sign",                 I_ITEM|I_S16 },
    [0x144] = { "Oak Door",             I_ITEM },
    [0x145] = { "Bucket",               I_ITEM|I_S16 },
    [0x146] = { "Water Bucket",         I_ITEM|I_NSTACK },
    [0x147] = { "Lava Bucket",          I_ITEM|I_NSTACK },
    [0x148] = { "Minecart",             I_ITEM|I_NSTACK },
    [0x149] = { "Saddle",               I_ITEM|I_NSTACK },
    [0x14a] = { "Iron Door",            I_ITEM },
    [0x14b] = { "Redstone",             I_ITEM },
    [0x14c] = { "Snowball",             I_ITEM|I_S16 },
    [0x14d] = { "Boat",                 I_ITEM|I_NSTACK },
    [0x14e] = { "Leather",              I_ITEM },
    [0x14f] = { "Milk Bucket",          I_ITEM|I_NSTACK },

    [0x150] = { "Brick",                I_ITEM },
    [0x151] = { "Clay Ball",            I_ITEM },
    [0x152] = { "Reeds",                I_ITEM },
    [0x153] = { "Paper",                I_ITEM },
    [0x154] = { "Book",                 I_ITEM },
    [0x155] = { "Slime Ball",           I_ITEM },
    [0x156] = { "Chest Minecart",       I_ITEM|I_NSTACK },
    [0x157] = { "Furnace Minecart",     I_ITEM|I_NSTACK },
    [0x158] = { "Egg",                  I_ITEM|I_S16 },
    [0x159] = { "Compass",              I_ITEM },
    [0x15a] = { "Fishing Rod",          I_ITEM|I_NSTACK },
    [0x15b] = { "Clock",                I_ITEM },
    [0x15c] = { "Glowstone Dust",       I_ITEM },
    [0x15d] = { "Raw Fish",             I_ITEM|I_MTYPE,
                { NULL, "Salmon", "Clownfish", "Pufferfish" }, },
    [0x15e] = { "Cooked Fish",          I_ITEM|I_MTYPE|I_FOOD,
                { NULL, "Salmon", "Clownfish", "Pufferfish" }, },
    [0x15f] = { "Dye",                  I_ITEM|I_MTYPE,
                { "Ink Sac", "Rose Red", "Cactus Green", "Cocoa Beans",
                  "Lapis Lazuli", "Purple", "Cyan", "L.Gray",
                  "Gray", "Pink", "Lime", "Dandelion Yellow",
                  "L.Blue", "Magenta", "Orange", "Bone Meal" }, },

    [0x160] = { "Bone",                 I_ITEM },
    [0x161] = { "Sugar",                I_ITEM },
    [0x162] = { "Cake",                 I_ITEM|I_NSTACK },
    [0x163] = { "Bed",                  I_ITEM|I_NSTACK },
    [0x164] = { "Repeater",             I_ITEM },
    [0x165] = { "Cookie",               I_ITEM },
    [0x166] = { "Map",                  I_ITEM },
    [0x167] = { "Shears",               I_ITEM|I_NSTACK },
    [0x168] = { "Melon",                I_ITEM|I_FOOD },
    [0x169] = { "Pumpkin Seeds",        I_ITEM },
    [0x16a] = { "Melon Seeds",          I_ITEM },
    [0x16b] = { "Raw Beef",             I_ITEM },
    [0x16c] = { "Steak",                I_ITEM|I_FOOD },
    [0x16d] = { "Raw Chicken",          I_ITEM },
    [0x16e] = { "Cooked Chicken",       I_ITEM|I_FOOD },
    [0x16f] = { "Rotten Flesh",         I_ITEM },

    [0x170] = { "Ender Pearl",          I_ITEM|I_S16 },
    [0x171] = { "Blaze Rod",            I_ITEM },
    [0x172] = { "Ghast Tear",           I_ITEM },
    [0x173] = { "Gold Nugget",          I_ITEM },
    [0x174] = { "Nether Wart",          I_ITEM },
    [0x175] = { "Potion",               I_ITEM|I_NSTACK, }, //TODO: potion names
    [0x176] = { "Glass Bottle",         I_ITEM },
    [0x177] = { "Spider Eye",           I_ITEM },
    [0x178] = { "Fermented Eye",        I_ITEM },
    [0x179] = { "Blaze Powder",         I_ITEM },
    [0x17a] = { "Magma Cream",          I_ITEM },
    [0x17b] = { "Brewing Stand",        I_ITEM },
    [0x17c] = { "Cauldron",             I_ITEM },
    [0x17d] = { "Ender Eye",            I_ITEM },
    [0x17e] = { "Glistering Melon",     I_ITEM },
    [0x17f] = { "Spawn Egg",            I_ITEM },

    [0x180] = { "Exp Potion",           I_ITEM },
    [0x181] = { "Fire Charge",          I_ITEM },
    [0x182] = { "Book and Quill",       I_ITEM|I_NSTACK },
    [0x183] = { "Written Book",         I_ITEM|I_NSTACK },
    // Written books are actually stackable (16), but only if identical
    // We'll keep it as NSTACK, until we have a better way to handle it
    [0x184] = { "Emerald",              I_ITEM },
    [0x185] = { "Item Frame",           I_ITEM },
    [0x186] = { "Flower Pot",           I_ITEM },
    [0x187] = { "Carrot",               I_ITEM },
    [0x188] = { "Potato",               I_ITEM },
    [0x189] = { "Baked Potato",         I_ITEM|I_FOOD },
    [0x18a] = { "Poisonous Potato",     I_ITEM },
    [0x18b] = { "Empty Map",            I_ITEM },
    [0x18c] = { "Golden Carrot",        I_ITEM|I_FOOD },
    [0x18d] = { "Mob Head",             I_ITEM },
    [0x18e] = { "Carrot on a Stick",    I_ITEM|I_NSTACK },
    [0x18f] = { "Nether Star",          I_ITEM },

    [0x190] = { "Pumpkin Pie",          I_ITEM },
    [0x191] = { "Firework Rocket",      I_ITEM },
    [0x192] = { "Firework Charge",      I_ITEM },
    [0x193] = { "Enchanted Book",       I_ITEM|I_NSTACK },
    [0x194] = { "Comparator",           I_ITEM },
    [0x195] = { "Netherbrick",          I_ITEM },
    [0x196] = { "Quartz",               I_ITEM },
    [0x197] = { "TNT Minecart",         I_ITEM|I_NSTACK },
    [0x198] = { "Hopper Minecart",      I_ITEM|I_NSTACK },
    [0x199] = { "Prismarine Shard",     I_ITEM },
    [0x19a] = { "Prismarine Crystal",   I_ITEM },
    [0x19b] = { "Raw Rabbit",           I_ITEM },
    [0x19c] = { "Cooked Rabbit",        I_ITEM|I_FOOD },
    [0x19d] = { "Rabbit Stew",          I_ITEM|I_NSTACK|I_FOOD },
    [0x19e] = { "Rabbit Foot",          I_ITEM },
    [0x19f] = { "Rabbit Hide",          I_ITEM },

    [0x1a0] = { "Armor Stand",          I_ITEM|I_S16 },
    [0x1a1] = { "Iron Horse Armor",     I_ITEM|I_NSTACK },
    [0x1a2] = { "Golden Horse Armor",   I_ITEM|I_NSTACK },
    [0x1a3] = { "Diamond Horse Armor",  I_ITEM|I_NSTACK },
    [0x1a4] = { "Lead",                 I_ITEM },
    [0x1a5] = { "Name Tag",             I_ITEM },
    [0x1a6] = { "Cmd Block Minecart",   I_ITEM|I_NSTACK },
    [0x1a7] = { "Mutton",               I_ITEM },
    [0x1a8] = { "Cooked Mutton",        I_ITEM|I_FOOD },
    [0x1a9] = { "Banner",               I_ITEM|I_NSTACK },
    [0x1ab] = { "Spruce Door",          I_ITEM },
    [0x1ac] = { "Birch Door",           I_ITEM },
    [0x1ad] = { "Jungle Door",          I_ITEM },
    [0x1ae] = { "Acacia Door",          I_ITEM },
    [0x1af] = { "Dark Oak Door",        I_ITEM },

    [0x8d0] = { "Record 13",            I_ITEM|I_NSTACK },
    [0x8d1] = { "Record Cat",           I_ITEM|I_NSTACK },
    [0x8d2] = { "Record Blocks",        I_ITEM|I_NSTACK },
    [0x8d3] = { "Record Chirp",         I_ITEM|I_NSTACK },
    [0x8d4] = { "Record Far",           I_ITEM|I_NSTACK },
    [0x8d5] = { "Record Mall",          I_ITEM|I_NSTACK },
    [0x8d6] = { "Record Mellohi",       I_ITEM|I_NSTACK },
    [0x8d7] = { "Record Stal",          I_ITEM|I_NSTACK },
    [0x8d8] = { "Record Strad",         I_ITEM|I_NSTACK },
    [0x8d9] = { "Record Ward",          I_ITEM|I_NSTACK },
    [0x8da] = { "Record 11",            I_ITEM|I_NSTACK },
    [0x8db] = { "Record Wait",          I_ITEM|I_NSTACK },


    [0x8ff] = { NULL }, //Terminator
};

int get_base_meta(int id, int meta) {
    // only accept existing block types
    const item_id * it = &ITEMS[id];
    assert(it->name);

    // if the block has no I_MTYPE subtypes, so base meta is 0
    if (!(it->flags&I_MTYPE)) return 0;

    // block meta is used for I_MTYPE but not used for position/state => base meta as is
    if (!(it->flags&(I_MPOS|I_STATE))) return meta;

    // everything else needs to be determined individually
    switch (id) {
        case 0x06: // Sapling
        case 0x2c: // Stone slab
        case 0x7e: // Wooden slab
        case 0xaf: // Large Flower
            return meta&7;

        case 0x11: // Wood
        case 0x12: // Leaves
        case 0xa1: // Leaves2
        case 0xa2: // Wood2
        case 0xaa: // Haybales
            return meta&3;

        case 0x9b: // Quartz block
            return (meta>2) ? 2 : meta;

        default:
            printf("id=%d\n",id);
            assert(0);
    }
    return meta;
}

// mapping macro for the block types that have different IDs
// for the block and the item used for placement of such block
#define BI(block,item) if (mat.bid == block) return BLOCKTYPE(item,0)

bid_t get_base_material(bid_t mat) {
    assert(mat.bid <0x100);

    // material for doubleslabs is the slab with the next block ID and same meta
    if (ITEMS[mat.bid].flags&I_DSLAB) mat.bid++;

    BI(55,331);   // Redstone Wire -> Redstone
    BI(132,287);  // Tripwire -> String

    BI(92,354);   // Cake
    BI(117,379);  // Brewing Stand
    BI(118,380);  // Cauldron
    BI(140,390);  // Flower pot

    BI(59,295);   // Wheat -> Wheat Seeds
    BI(83,338);   // Sugar Cane
    BI(115,372);  // Nether Wart
    BI(141,391);  // Carrot Plant -> Carrot
    BI(142,392);  // Potato Plant -> Potato
    BI(104,361);  // Pumpkin Stem -> Pumpkin Seeds
    BI(105,362);  // Melon Stem -> Melon Seeds
    if (mat.bid == 127) return BLOCKTYPE(351,3);  // Cocoa -> Cocoa Beans (which is also a Brown Dye, duh)

    BI(63,323);   // Standing Sign -> Sign
    BI(68,323);   // Wall Sign -> Sign
    BI(176,425);  // Standing Banner -> Banner
    BI(177,425);  // Wall Banner -> Banner

    BI(26,355);   // Bed

    BI(64,324);   // Oak Door
    BI(193,427);  // Spruce Door
    BI(194,428);  // Birch Door
    BI(195,429);  // Jungle Door
    BI(196,430);  // Acacia Door
    BI(197,431);  // Dark Oak Door
    BI(71,330);   // Iron Door

    BI(75,76);    // Redstone Torch (inactive -> active)
    BI(93,356);   // Redstone Repeater (inactive)
    BI(94,356);   // Redstone Repeater (active)
    BI(149,404);  // Redstone Comparator

    mat.meta = get_base_meta(mat.bid, mat.meta);
    return mat;
}

// select a block material that matches best a block derived from it,
// e.g. Oak Woodplanks for Oak stairs or slabs
bid_t get_base_block_material(bid_t mat) {
    switch (mat.bid) {
        case 0x2c: // Stone Slab
        case 0x2b: // Double Slab
            switch (mat.meta&0x7) {
                case  0: return BLOCKTYPE(0x01,0); // Stone (plain)
                case  1: return BLOCKTYPE(0x18,0); // Sandstone (plain)
                case  2: return BLOCKTYPE(0x05,0); // Woodplanks (oak)
                case  3: return BLOCKTYPE(0x04,0); // Cobblestone
                case  4: return BLOCKTYPE(0x2d,0); // Brick
                case  5: return BLOCKTYPE(0x62,0); // Stonebrick (plain)
                case  6: return BLOCKTYPE(0x70,0); // Netherbrick
                case  7: return BLOCKTYPE(0x9b,0); // Quartz block (plain)
                default: assert(0);
            }
        case 0x7d: // Double Wooden Slab
        case 0x7e: // Wooden Slab
            return BLOCKTYPE(0x05,(mat.meta&0x7)); // Woodplanks of the same type

        // wooden stairs
        case 0x35: return BLOCKTYPE(0x05,0); // Woodplanks (oak)
        case 0x86: return BLOCKTYPE(0x05,1); // Woodplanks (spruce)
        case 0x87: return BLOCKTYPE(0x05,2); // Woodplanks (birch)
        case 0x88: return BLOCKTYPE(0x05,3); // Woodplanks (jungle)
        case 0xa3: return BLOCKTYPE(0x05,4); // Woodplanks (acacia)
        case 0xa4: return BLOCKTYPE(0x05,5); // Woodplanks (dark oak)

        // stone stairs
        case 0x43: return BLOCKTYPE(0x04,0); // Cobblestone
        case 0x6c: return BLOCKTYPE(0x2d,0); // Brick
        case 0x6d: return BLOCKTYPE(0x62,0); // Stonebrick (plain)
        case 0x72: return BLOCKTYPE(0x70,0); // Netherbrick
        case 0x80: return BLOCKTYPE(0x18,0); // Sandstone (plain)
        case 0x9c: return BLOCKTYPE(0x9b,0); // Quartz block (plain)

        // red sandstone stairs, d-slabs, slabs
        case 0xb4:
        case 0xb5:
        case 0xb6:
            return BLOCKTYPE(0xb3,0); // Red Sandstone (plain)
    }
    return mat;
}

const char * get_mat_name(char *buf, int id, int meta) {
    if (id<0) {
        sprintf(buf, "-");
        return buf;
    }

    int bmeta = (id<0x100) ? get_base_meta(id, meta) : 0;

    const item_id *it = &ITEMS[id];
    if (it->name) {
        int pos = sprintf(buf, "%s", it->name);

        if ((it->flags&I_MTYPE) && it->mname[bmeta])
            pos += sprintf(buf+pos, " (%s)",it->mname[bmeta]);

        //TODO: support other block types with I_MPOS
        if (it->flags&I_SLAB)
            sprintf(buf+pos, " (%s)",(meta&8)?"upper":"lower");
    }
    else {
        printf(buf, "???");
    }
    return buf;

}

const char * get_bid_name(char *buf, bid_t b) {
    return get_mat_name(buf, b.bid, b.meta);
}

const char * get_item_name(char *buf, slot_t *s) {
    return get_mat_name(buf, s->item, s->damage);
}

int find_bid_name(const char *name) {
    int i;
    char n[256];
    for(i=0; i<sizeof(n)-1; i++)
        n[i] = (name[i]=='_') ? ' ' : name[i];

    int id;
    for(id=0; id<0x100; id++)
        if (ITEMS[id].name && !strcasecmp(ITEMS[id].name, n))
            return id;
    return -1;
}

int find_meta_name(int bid, const char *name) {
    int i;
    char n[256];
    for(i=0; i<sizeof(n)-1; i++)
        n[i] = (name[i]=='_') ? ' ' : name[i];

    const item_id *it = &ITEMS[bid];
    assert(it->name);

    int meta;
    for(meta=0; meta<16; meta++)
        if (it->mname[meta] && !strcasecmp(it->mname[meta], n))
            return meta;
    return -1;
}

////////////////////////////////////////////////////////////////////////////////

uint8_t get_state_mask(int bid) {
    assert (bid>=0 && bid<0x100);

    const item_id * it = &ITEMS[bid];

    if (!(it->flags&I_STATE)) return 0; // no I_STATE - no bits for state
    if (!(it->flags&(I_MPOS|I_MTYPE))) return 0x0f; // I_STATE but no I_MPOS or I_MTYPE - all bits for state

    switch (bid) {
        case 0x06: // Sapling
        case 0x17: // Dispenser
        case 0x1d: // Sticky Piston
        case 0x21: // Piston
        case 0x22: // Piston Head
        case 0x1b: // Powered Rail
        case 0x1c: // Detector Rail
        case 0x45: // Lever
        case 0x4d: // Stone Button
        case 0x8f: // Wooden Button
        case 0x9d: // Actiator Rail
        case 0x9a: // Hopper
        case 0x9e: // Dropper
        case 0xaf: // Large Flower
            return 8;
        case 0x1a: // Bed
        case 0x40: // Wooden Door
        case 0x47: // Iron Door
        case 0x60: // Trapdoor
        case 0x6b: // Fence Gate
        case 0xa7: // Iron Trapdoor
        case 0xb7: // SpruceFence Gate
        case 0xb8: // Birch Fence Gate
        case 0xb9: // Jungle Fence Gate
        case 0xba: // Dark Oak Fence Gate
        case 0xbb: // Acacia Fence Gate
        case 0xc1: // Spruce Door
        case 0xc2: // Birch Door
        case 0xc3: // Jungle Door
        case 0xc4: // Dark Oak Door
        case 0xc5: // Acacia Door
            return 4;
        case 0x12: // Leaves
        case 0x5d: // Repeater (off)
        case 0x5e: // Repeater (on)
        case 0x83: // Tripwire hook
        case 0x95: // Comparator (off)
        case 0x96: // Comparator (on)
        case 0xa1: // Leaves2
            return 12;
    }

    printf("get_state_mask: unknown bid=%d\n",bid);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Orientation

static const char * _DIRNAME[] = { "any", "up", "down", "south", "north", "east", "west" };
const char ** DIRNAME = _DIRNAME+1;

// Meta mappings for the rotation-dependent blocks
/*
  How to interpret:

  The array is selected by the functions responsible for meta rotation/flipping,
  according to the block's flags (i.e. I_STAIR)

  The array is a 4*16 int8_t elements. The outer index defines block's original
  meta value (0..15). The inner index defines the 4 meta values corresponding
  to the orientations South, North, East, West, in this order (this index can be
  determined using the standard direction constant DIR_* and substracting 2).
  The int8_t value determines the meta value of this block in this orientation.

  1. Take the current meta of the block and look up the 4-element row.
  This defines a "group" of meta values related to the original value, with
  different azimuth orientations, but sharing all other orientation or type.
  E.g. a meta value corresponding to an upside-down stairs block pointing north
  will reference a group of values for all upside-down stair blocks, pointing
  in the 4 worlds directions.
  If the row pointer is NULL - this meta does not exist or does not specify
  rotational position. E.g. meta value 5 for torches corresponds to "on ground"
  position which is not rotable.

  2. Locate the original meta value in the row.
  The obtained index will tell you the current azimuth orientation of the block.
  E.g. a stairs block with a meta value 5 can be found on index 3 - i.e. it is
  "west-oriented"

  3. Select the desired rotation of the block.
  E.g. if we want to rotate the block counter-clockwise, west should become south.

  4. Look up the meta value at the index corresponding to the new selected direction
  in the same group row.
  E.g. the direction "south" corresponds to the index 0 in the row. Therefore,
  the meta value for the new orientation will be 6
*/

// meta value group - orientation mapping
typedef struct {
    int inuse;
    int8_t meta[4];
} metagroup;

#define METAO(m,s,n,e,w) [m] = {1,{s,n,e,w}}

//          S N E W

// I_STAIR
static metagroup MM_STAIR[16] = {
    METAO(0,2,3,0,1),
    METAO(1,2,3,0,1),
    METAO(2,2,3,0,1),
    METAO(3,2,3,0,1),
    METAO(4,6,7,4,5),
    METAO(5,6,7,4,5),
    METAO(6,6,7,4,5),
    METAO(7,6,7,4,5),
};

// I_TORCH
static metagroup MM_TORCH[16] = {
    METAO(1,3,4,1,2),
    METAO(2,3,4,1,2),
    METAO(3,3,4,1,2),
    METAO(4,3,4,1,2),
};

// I_LOG
// Wood/Wood2 logs - N/S,N/S,E/W,E/W orientation
static metagroup MM_LOG[16] = {
    // Oak/Acacia
    METAO(4,8,8,4,4),
    METAO(8,8,8,4,4),
    // Spruce/Dark Oak
    METAO(5,9,9,5,5),
    METAO(9,9,9,5,5),
    // Birch
    METAO(6,10,10,6,6),
    METAO(10,10,10,6,6),
    //Jungle
    METAO(7,11,11,7,7),
    METAO(11,11,11,7,7),
};

// I_ONWALL (ladders/banners/signs)
static metagroup MM_ONWALL[16] = {
    METAO(2,3,2,5,4),
    METAO(3,3,2,5,4),
    METAO(4,3,2,5,4),
    METAO(5,3,2,5,4),
};

#define GETMETAGROUP(mmname) mmname[b.meta].inuse ? mmname[b.meta].meta : NULL
static inline int8_t *get_metagroup(bid_t b) {
    uint64_t flags = ITEMS[b.bid].flags;
    if (flags&I_STAIR)  return GETMETAGROUP(MM_STAIR);
    if (flags&I_TORCH)  return GETMETAGROUP(MM_TORCH);
    if (flags&I_LOG)    return GETMETAGROUP(MM_LOG);
    if (flags&I_ONWALL) return GETMETAGROUP(MM_ONWALL);
    return NULL; // default - no orientation mapping
}

// get the orientation of the block by its meta - as one of the DIR_xxx constants
static inline int get_orientation(bid_t b) {
    int8_t *group = get_metagroup(b);
    if (!group) return DIR_ANY;

    int i;
    for(i=0; i<4; i++)
        if (group[i] == b.meta)
            return i+2;
    assert(0);
}

// generic rotation map to calculate clockwise rotation for a direction
int ROT_MAP[][4] = {
    [DIR_UP]    = { DIR_UP,    DIR_UP,    DIR_UP,    DIR_UP,    },
    [DIR_DOWN]  = { DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  DIR_DOWN,  },
    [DIR_NORTH] = { DIR_NORTH, DIR_EAST,  DIR_SOUTH, DIR_WEST,  },
    [DIR_EAST]  = { DIR_EAST,  DIR_SOUTH, DIR_WEST,  DIR_NORTH, },
    [DIR_SOUTH] = { DIR_SOUTH, DIR_WEST,  DIR_NORTH, DIR_EAST,  },
    [DIR_WEST]  = { DIR_WEST,  DIR_NORTH, DIR_EAST,  DIR_SOUTH, },
};

// rotate a block's meta clockwise given number of times
bid_t rotate_meta(bid_t b, int times) {
    int8_t * mg = get_metagroup(b);
    if (!mg) return b;

    int mo = get_orientation(b);
    int rmo = ROT_MAP[mo][times&3];
    b.meta = mg[rmo-2];
    return b;
}

// get the number of clockwise 90-degree rotations to get from one direction to another
int numrot(int from_dir, int to_dir) {
    int i;
    for(i=0; i<4; i++)
        if (ROT_MAP[from_dir][i] == to_dir)
            return i;
    return -1;
}

// calculate metas for a flipped version of a block

int FLIP_MAP[][2] = {
    [DIR_UP]    = { DIR_UP,    DIR_UP,    },
    [DIR_DOWN]  = { DIR_DOWN,  DIR_DOWN,  },
    [DIR_NORTH] = { DIR_NORTH, DIR_SOUTH, },
    [DIR_SOUTH] = { DIR_SOUTH, DIR_NORTH, },
    [DIR_EAST]  = { DIR_WEST,  DIR_EAST,  },
    [DIR_WEST]  = { DIR_EAST,  DIR_WEST,  },
};

bid_t flip_meta(bid_t b, char mode) {
    assert(mode=='x' || mode=='z');

    int8_t * mg = get_metagroup(b);
    if (!mg) return b;

    int mo = get_orientation(b);
    int fmo = FLIP_MAP[mo][mode=='z'];
    b.meta = mg[fmo-2];
    return b;
}

bid_t flip_meta_y(bid_t b) {
    uint64_t flags = ITEMS[b.bid].flags;
    if (flags&I_STAIR) b.meta^=4; // flip the up/down bit
    if (flags&I_SLAB)  b.meta^=8; // flip the up/down bit
    //TODO: support other blocks with up/down orientation, like Droppers
    return b;
}

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

const char * METANAME[][32] = {
    [Entity] = {
        [0]  = "Flags",
        [1]  = "Air",
    },
    [LivingEntity] = {
        [0]  = "Flags",
        [1]  = "Air",
        [2]  = "Name tag",
        [3]  = "Always show name tag",
        [6]  = "Health",
        [7]  = "Potion effect color",
        [8]  = "Potion effect ambient",
        [9]  = "Number of arrows",
        [15] = "No AI",
    },
    [Ageable] = {
        [12] = "Age",
    },
    [ArmorStand] = {
        [10] = "Armor stand flags",
        [11] = "Head position",
        [12] = "Body position",
        [13] = "L arm position",
        [14] = "R arm position",
        [15] = "L leg position",
        [16] = "R leg position",
    },
    [Human] = {
        [10] = "Skin flags",
        [16] = "Hide cape",
        [17] = "Absorption hearts",
        [18] = "Score",
    },
    [Horse] = {
        [16] = "Horse flags",
        [19] = "Horse type",
        [20] = "Horse color",
        [21] = "Owner name",
        [22] = "Horse armor",
    },
    [Bat] = {
        [16] = "Is hanging",
    },
    [Tameable] = {
        [16] = "Tameable flags",
        [17] = "Owner name",
    },
    [Ocelot] = {
        [18] = "Ocelot type",
    },
    [Wolf] = {
        [18] = "Health",
        [19] = "Begging",
        [20] = "Collar color",
    },
    [Pig] = {
        [16] = "Has saddle",
    },
    [Rabbit] = {
        [18] = "Rabbit type",
    },
    [Sheep] = {
        [16] = "Sheep color",
    },
    [Villager] = {
        [16] = "Villager type",
    },
    [Enderman] = {
        [16] = "Carried block",
        [17] = "Carried block data",
        [18] = "Is screaming",
    },
    [Zombie] = {
        [12] = "child zombie",
        [13] = "villager zombie",
        [14] = "converting zombie",
    },
    [ZombiePigman] = {
    },
    [Blaze] = {
        [16] = "Blaze it motherfucker",
    },
    [Spider] = {
        [16] = "Climbing",
    },
    [CaveSpider] = {
    },
    [Creeper] = {
        [16] = "Creeper state",
        [17] = "is powered",
    },
    [Ghast] = {
        [16] = "is attacking",
    },
    [Slime] = {
        [16] = "Size",
    },
    [MagmaCube] = {
    },
    [Skeleton] = {
        [13] = "Skeleton type",
    },
    [Witch] = {
        [21] = "is aggressive",
    },
    [IronGolem] = {
        [16] = "created by player",
    },
    [Wither] = {
        [17] = "Target 1",
        [18] = "Target 2",
        [19] = "Target 3",
        [20] = "Invulnerable time",
    },
    [Boat] = {
        [17] = "Time since hit",
        [18] = "Forward direction",
        [19] = "Damage taken",
    },
    [Minecart] = {
        [17] = "Shaking power",
        [18] = "Shaking direction",
        [19] = "Damage taken/shaking multiplier",
        [20] = "Block id/data",
        [21] = "Block y",
        [22] = "Show block",
    },
    [FurnaceMinecart] = {
        [16] = "Is powered",
    },
    [Item] = {
        [10] = "Item",
    },
    [Arrow] = {
        [16] = "Is critical",
    },
    [Firework] = {
        [8] = "Firework data",
    },
    [ItemFrame] = {
        [8] = "Framed item",
        [9] = "Rotation",
    },
    [EnderCrystal] = {
        [8] = "Health",
    },
};

const EntityType ENTITY_HIERARCHY[] = {
    //// Superclasses
    [Entity]          = IllegalEntityType,
    [LivingEntity]    = Entity,
    [Ageable]         = LivingEntity,
    [Human]           = LivingEntity,
    [Tameable]        = Ageable,
    [Item]            = Entity,
    [Firework]        = Entity,
    [Mob]             = LivingEntity,
    [Monster]         = LivingEntity,

    //// Monsters
    [Creeper]         = LivingEntity,
    [Skeleton]        = LivingEntity,
    [Spider]          = LivingEntity,
    [GiantZombie]     = LivingEntity,
    [Zombie]          = LivingEntity,
    [Slime]           = LivingEntity,
    [Ghast]           = LivingEntity,
    [ZombiePigman]    = Zombie,
    [Enderman]        = LivingEntity,
    [CaveSpider]      = Spider,
    [Silverfish]      = LivingEntity,
    [Blaze]           = LivingEntity,
    [MagmaCube]       = Slime,
    [Enderdragon]     = LivingEntity,
    [Wither]          = LivingEntity,
    [Bat]             = LivingEntity,
    [Witch]           = LivingEntity,
    [Endermite]       = LivingEntity,
    [Guardian]        = LivingEntity,

    //// Mobs
    [Pig]             = Ageable,
    [Sheep]           = Ageable,
    [Cow]             = Ageable,
    [Chicken]         = Ageable,
    [Squid]           = LivingEntity,
    [Wolf]            = Tameable,
    [Mooshroom]       = Ageable,
    [Snowman]         = LivingEntity,
    [Ocelot]          = Tameable,
    [IronGolem]       = LivingEntity,
    [Horse]           = Ageable,
    [Rabbit]          = Ageable,
    [Villager]        = Ageable,

    //// Objects
    [Boat]            = Entity,
    [ItemStack]       = Entity,
    [Minecart]        = Entity,
    [ChestMinecart]   = Minecart,
    [FurnaceMinecart] = Minecart,
    [EnderCrystal]    = Entity,
    [Arrow]           = Entity,
    [ItemFrame]       = Entity,
    [ArmorStand]      = LivingEntity,
};

#define ENUMNAME(name) [name] = #name

const char * ENTITY_NAMES[MaxEntityType] = {
    ENUMNAME(Creeper),
    ENUMNAME(Skeleton),
    ENUMNAME(Spider),
    ENUMNAME(GiantZombie),
    ENUMNAME(Zombie),
    ENUMNAME(Slime),
    ENUMNAME(Ghast),
    ENUMNAME(ZombiePigman),
    ENUMNAME(Enderman),
    ENUMNAME(CaveSpider),
    ENUMNAME(Silverfish),
    ENUMNAME(Blaze),
    ENUMNAME(MagmaCube),
    ENUMNAME(Enderdragon),
    ENUMNAME(Wither),
    ENUMNAME(Bat),
    ENUMNAME(Witch),
    ENUMNAME(Endermite),
    ENUMNAME(Guardian),

    ENUMNAME(Pig),
    ENUMNAME(Sheep),
    ENUMNAME(Cow),
    ENUMNAME(Chicken),
    ENUMNAME(Squid),
    ENUMNAME(Wolf),
    ENUMNAME(Mooshroom),
    ENUMNAME(Snowman),
    ENUMNAME(Ocelot),
    ENUMNAME(IronGolem),
    ENUMNAME(Horse),
    ENUMNAME(Rabbit),
    ENUMNAME(Villager),
};

const char * get_entity_name(char *buf, EntityType type) {
    if (type < 0 || type >= MaxEntityType ) {
        sprintf(buf, "%s", "IllegalEntityType");
    }
    else if ( ENTITY_NAMES[type] ) {
        sprintf(buf, "%s", ENTITY_NAMES[type]);
    }
    else {
        sprintf(buf, "%s", "UnknownEntity");
    }
    return buf;
}

////////////////////////////////////////////////////////////////////////////////
// ANSI representation of blocks

const char * ANSI_BLOCK[] = {
    // 00
    "\x1b[0m ",                         // Air
    "\x1b[47m ",                        // Stone
    "\x1b[42;33m\xe2\x96\x91",          // Grass
    "\x1b[43;30m\xe2\x96\x93",          // Dirt
    "\x1b[47;30m\xe2\x96\x99",          // Cobblestone
    "\x1b[43;30;22m\xe2\x96\xa4",       // Planks
    "\x1b[40;32m\xf0\x9f\x8c\xb1",      // Sapling
    "\x1b[47;30m\xe2\x96\x93",          // Bedrock
    "\x1b[44;37m-",                     // Flowing Water
    "\x1b[44;37m ",                     // Water
    "\x1b[41;33;22m\xe2\x96\xa4",       // Flowing Lava
    "\x1b[41;33;22m\xe2\x96\x91",       // Lava
    "\x1b[43;37m\xe2\x96\x91",          // Sand
    "\x1b[47;30m\xe2\x96\x92",          // Gravel
    "\x1b[47;33m\xe2\x97\x86",          // Gold Ore
    "\x1b[47;35m\xe2\x97\x86",          // Iron Ore

    // 10
    "\x1b[47;30m\xe2\x97\x86",          // Iron Ore
    "\x1b[43;30m\xe2\x96\xa2",          // Wood
    "\x1b[40;32m\xe2\x96\x92",          // Leaves
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[47;34m\xe2\x97\x86",          // Iron Ore
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[40;32m\xe2\x96\x92",          // Leaves

    // 20
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 30
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[47;36m\xe2\x97\x86",          // Iron Ore
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[40;33m\xe1\xba\x9b",          // Wheat
    "\x1b[43;30m\xe2\x90\xa5",          // Farmland
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 40
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[47;31m\xe2\x97\x86",          // Redstone Ore
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 50
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[40;32m\xe1\xba\x9b",          // Sugar Cane
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    "\x1b[31;40m#",                     // Netherrack
    ANSI_UNKNOWN,
    "\x1b[33;47m#",                     // Glowstone
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 60
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 70
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // 80
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    ANSI_UNKNOWN,
    "\x1b[40;32m\xe1\xba\x9b",          // Carrots
    "\x1b[40;32m\xe1\xba\x9b",          // Potatoes
    ANSI_UNKNOWN,

    // 90
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // A0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // B0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // C0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // D0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // E0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,

    // F0
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
    ANSI_UNKNOWN,
};
