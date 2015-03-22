#include "mcp_ids.h"

#include <stdlib.h>
#include <stdio.h>

#define MNAMES_WOOD     { "Oak", "Spruce", "Birch", "Jungle", "Acacia", "Dark Oak" }
#define MNAMES_OLDWOOD  { "Oak", "Spruce", "Birch", "Jungle" }
#define MNAMES_NEWWOOD  { "Acacia", "Dark Oak", NULL, NULL }
#define MNAMES_COLOR    { "White", "Orange", "Magenta", "LBlue", "Yellow", "Lime", "Pink", "Grey", \
                          "LGrey", "Cyan", "Purple", "Blue", "Brown", "Green", "Red", "Black" }

const item_id ITEMS[] = {
    ////////////////////////////////////////////////////////////////////////
    // Blocks

    [0x00] = { "Air" },
    [0x01] = { "Stone", I_MTYPE,
               { NULL, "Granite", "P. Granite", "Diorite", "P. Diorite",
                 "Andesite", "P. Andesite" } },
    [0x02] = { "Grass" },
    [0x03] = { "Dirt", I_MTYPE,
               { NULL, "Coarse", "Podzol" } },
    [0x04] = { "Cobblestone" },
    [0x05] = { "Woodplanks", I_MTYPE,
               MNAMES_WOOD },
    [0x06] = { "Sapling", I_MTYPE|I_STATE,                      // S: grow indication
               MNAMES_WOOD },
    [0x07] = { "Bedrock" },
    [0x08] = { "Waterflow", I_STATE },                          // S: level/drop
    [0x09] = { "Water", I_STATE },                              // S: level
    [0x0a] = { "Lavaflow", I_STATE },                           // S: level/drop
    [0x0b] = { "Lava", I_STATE },                               // S: level
    [0x0c] = { "Sand", I_MTYPE,
               { NULL, "Red" } },
    [0x0d] = { "Gravel" },
    [0x0e] = { "Gold ore" },
    [0x0f] = { "Iron ore" },

    [0x10] = { "Coal ore" },
    [0x11] = { "Wood", I_MTYPE|I_MPOS, MNAMES_OLDWOOD },        // P: dir
    [0x12] = { "Leaves", I_MTYPE|I_STATE, MNAMES_OLDWOOD },     // S: decay
    [0x13] = { "Sponge", I_MTYPE,
               { NULL, "Wet" }, },
    [0x14] = { "Glass" },
    [0x15] = { "Lapis ore" },
    [0x16] = { "Lapis block" },
    [0x17] = { "Dispenser", I_MPOS|I_STATE },                   // P: dir, S: active
    [0x18] = { "Sandstone", I_MTYPE,
               { NULL, "Chiseled", "Smooth"} },
    [0x19] = { "Noteblock" },
    [0x1a] = { "Bed", I_MPOS|I_STATE },                         // P: dir, S: occupied
    [0x1b] = { "Powered Rail", I_MPOS|I_STATE },                // P: dir, S: active
    [0x1c] = { "Detector Rail", I_MPOS|I_STATE },               // P: dir, S: active
    [0x1d] = { "Sticky Piston", I_MPOS|I_STATE },               // P: dir, S: extended
    [0x1e] = { "Cobweb", },
    [0x1f] = { "Tallgrass", I_MTYPE,
               { "Shrub", NULL, "Fern", }, },

    [0x20] = { "Deadbush" },
    [0x21] = { "Piston", I_MPOS|I_STATE },                      // P: dir, S: extended
    [0x22] = { "Piston Head", I_MPOS|I_STATE },                 // P: dir, S: extended
    [0x23] = { "Wool", I_MTYPE, MNAMES_COLOR },
    [0x24] = { "Pushed block", },
    [0x25] = { "Dandelion", },
    [0x26] = { "Flower", I_MTYPE,
               { "Poppy", "Blue orchid", "Allium", "Azure Bluet", 
                 "Red Tulip", "Orange Tulip", "White Tulip", "Pink Tulip",
                 "Oxeye Daisy", }, },
    [0x27] = { "Brown Mushroom" },
    [0x28] = { "Red Mushroom" },
    [0x29] = { "Gold Block" },
    [0x2a] = { "Iron Block" },
    [0x2b] = { "Double Slab", I_MTYPE,
               { "Stone", "Sandstone", "Wood", "Cobble",
                 "Brick", "Stone Brick", "Netherbrick", "Quartz",
                 "Smooth Stone", "Smooth Sandstone",
                 [15] = "Tile Quartz", }, },
    [0x2c] = { "Stone Slab", I_MTYPE|I_MPOS|I_SLAB,             // P: U/D
               { NULL, "Sandstone", "Stonewood", "Cobble",
                 "Brick", "Stone Brick", "Netherbrick", "Quartz" } },
    [0x2d] = { "Brick" },
    [0x2e] = { "TNT" },
    [0x2f] = { "Bookshelf" },

    [0x30] = { "Mossy Stone" },
    [0x31] = { "Obsidian" },
    [0x32] = { "Torch", I_MPOS },                               // P: dir
    [0x33] = { "Fire", I_STATE },                               // S: age
    [0x34] = { "Spawner" },
    [0x35] = { "Wooden Stairs", I_MPOS|I_STAIR },               // P: dir
    [0x36] = { "Chest", I_MPOS },                               // P: dir
    [0x37] = { "Redstone Wire", I_STATE },                      // S: power level
    [0x38] = { "Diamond Ore" },
    [0x39] = { "Diamond Block" },
    [0x3a] = { "Workbench" },
    [0x3b] = { "Wheat", I_STATE },                              // S: level
    [0x3c] = { "Farmland", I_STATE },                           // S: wetness
    [0x3d] = { "Furnace", I_MPOS },                             // P: dir
    [0x3e] = { "Lit Furnace", I_MPOS },                         // P: dir
    [0x3f] = { "Standing Sign", I_MPOS },                       // P: dir

    [0x40] = { "Wooden Door", I_MPOS|I_STATE },                 // P: dir, S: open/close
    [0x41] = { "Ladder", I_MPOS },                              // P: dir
    [0x42] = { "Rail", I_MPOS },                                // P: dir
    [0x43] = { "Cobblestone Stairs", I_MPOS|I_STAIR },          // P: dir
    [0x44] = { "Wall Sign", I_MPOS },                           // P: dir
    [0x45] = { "Lever", I_MPOS|I_STATE },                       // P: dir, S: up/down
    [0x46] = { "Stone Pressure Plate", I_STATE },               // S: pressed
    [0x47] = { "Iron Door", I_MPOS|I_STATE },                   // P: dir, S: open/close
    [0x48] = { "Wooden Pressure Plate", I_STATE },              // S: pressed
    [0x49] = { "Redstone Ore" },
    [0x4a] = { "Glowing Redstone Ore" },
    [0x4b] = { "Redstone Torch (off)", I_MPOS },                // P: dir
    [0x4c] = { "Redstone Torch (on)", I_MPOS },                 // P: dir
    [0x4d] = { "Stone Button", I_MPOS|I_STATE },                // P: dir, S: pressed
    [0x4e] = { "Snow Layer", I_STATE },                         // P: level
    [0x4f] = { "Ice" },

    [0x50] = { "Snow" },
    [0x51] = { "Cactus", I_STATE },                             // S: grow
    [0x52] = { "Clay Block" },
    [0x53] = { "Sugar Cane", I_STATE },                         // S: grow
    [0x54] = { "Jukebox", I_STATE },                            // S: has a disc
    [0x55] = { "Fence" },
    [0x56] = { "Pumpkin", I_MPOS },                             // P: dir
    [0x57] = { "Netherrack" },
    [0x58] = { "Soul Sand" },
    [0x59] = { "Glowstone Block" },
    [0x5a] = { "Portal", I_MPOS },
    [0x5b] = { "Jack-o-Lantern", I_MPOS },
    [0x5c] = { "Cake", I_STATE },                               // S: level
    [0x5d] = { "Repeater (off)", I_MPOS|I_STATE },              // P: dir, S: delay setting
    [0x5e] = { "Repeater (on)", I_MPOS|I_STATE },               // P: dir, S: delay setting
    [0x5f] = { "Stained Glass", I_MTYPE, MNAMES_COLOR },

    [0x60] = { "Trapdoor", I_MPOS|I_STATE },                    // P: dir,u/d, S: open/close
    [0x61] = { "Silverfish Block", I_MTYPE,
               { "Stone", "Cobblestone", "Stonebrick", "Mossy Stonebrick",
                 "Cracked Stonebrick", "Chiseled Stonebrick" }, },
    [0x62] = { "Stonebrick", I_MTYPE,
               { NULL, "Mossy", "Cracked", "Chiseled" }, },
    [0x63] = { "Brown Mushroom Block", I_MPOS },                // P: part
    [0x64] = { "Red Mushroom Block", I_MPOS },                  // P: part
    [0x65] = { "Iron Bars" },
    [0x66] = { "Glass Pane" },
    [0x67] = { "Melon Block" },
    [0x68] = { "Pumpkin Stem", I_STATE },                       // S: level
    [0x69] = { "Melon Stem", I_STATE },                         // S: level
    [0x6a] = { "Vine", I_MPOS },                                // P: dir
    [0x6b] = { "Fence Gate", I_MPOS|I_STATE },                  // P: dir, S: open/close
    [0x6c] = { "Brick Stairs", I_MPOS|I_STAIR },                // P: dir
    [0x6d] = { "Stone Brick Stairs", I_MPOS|I_STAIR },          // P: dir
    [0x6e] = { "Mycelium" },
    [0x6f] = { "Lilypad", I_MPOS },

    [0x70] = { "Nether Brick" },
    [0x71] = { "Nether Brick Fence" },
    [0x72] = { "Nether Brick Stairs", I_MPOS|I_STAIR },         // P: dir
    [0x73] = { "Nether Wart", I_STATE },                        // S: level
    [0x74] = { "Enchantment Table" },
    [0x75] = { "Brewing Stand", I_STATE },                      // S: bottles
    [0x76] = { "Cauldron", I_STATE },                           // S: level
    [0x77] = { "End Portal", I_MPOS },
    [0x78] = { "End Portal Block", I_MPOS },                    // P: dir
    [0x79] = { "End Stone" },
    [0x7a] = { "Dragon Egg" },
    [0x7b] = { "Redstone Lamp" },
    [0x7c] = { "Redstone Lamp (lit)" },
    [0x7d] = { "Wooden Double Slab", I_MTYPE, MNAMES_WOOD },
    [0x7e] = { "Wooden Slab", I_MTYPE|I_MPOS|I_SLAB, MNAMES_WOOD }, // P: up/down
    [0x7f] = { "Cocoa", I_STATE },                              // S: level

    [0x80] = { "Sandstone Stairs", I_MPOS|I_STAIR },            // P: dir
    [0x81] = { "Emerald Ore" },
    [0x82] = { "Ender Chest", I_MPOS },                         // P: dir
    [0x83] = { "Tripwire Hook", I_MPOS|I_STATE },               // P: dir, S: connected,active
    [0x84] = { "Tripwire", I_STATE },                           // S: active/suspended/attached/armed
    [0x85] = { "Emerald Block" },
    [0x86] = { "Spruce Stairs", I_MPOS|I_STAIR },               // P: dir
    [0x87] = { "Birch Stairs", I_MPOS|I_STAIR },                // P: dir
    [0x88] = { "Jungle Stairs", I_MPOS|I_STAIR },               // P: dir
    [0x89] = { "Command Block" },
    [0x8a] = { "Beacon" },
    [0x8b] = { "Cobblestone Wall", I_MTYPE,
               { NULL, "Mossy" } },
    [0x8c] = { "Flower Pot" },
    [0x8d] = { "Carrot Plant", I_STATE },                       // S: level
    [0x8e] = { "Potato Plant", I_STATE },                       // S: level
    [0x8f] = { "Wooden Button", I_MPOS|I_STATE },               // P: dir, S: pressed

    [0x90] = { "Skull", I_MPOS }, //FIXME: different block/item metas
    [0x91] = { "Anvil", I_MPOS }, //FIXME: different block/item metas
    [0x92] = { "Trapped Chest", I_MPOS },                       // P: dir
    [0x93] = { "Pressure Plate (light)", I_STATE },             // S: pressed
    [0x94] = { "Pressure Plate (heavy)", I_STATE },             // S: pressed
    [0x95] = { "Comparator (off)", I_MPOS|I_STATE },            // P: dir, S: mode/power
    [0x96] = { "Comparator (on)", I_MPOS|I_STATE },             // P: dir, S: mode/power
    [0x97] = { "Daylight Sensor" },
    [0x98] = { "Redstone Block" },
    [0x99] = { "Quartz Ore" },
    [0x9a] = { "Hopper", I_MPOS|I_STATE },                      // P: dir, S: active
    [0x9b] = { "Quartz Block", I_MTYPE|I_MPOS,                  // P: dir (pillar only)
               { NULL, "Chiseled", "Pillar" } },
    [0x9c] = { "Quartz Stairs", I_MPOS|I_STAIR },               // P: dir
    [0x9d] = { "Activator Rail", I_MPOS|I_STATE },              // P: dir, S: active
    [0x9e] = { "Dropper", I_MPOS|I_STATE },                     // P: dir, S: active
    [0x9f] = { "Stained Clay", I_MTYPE, MNAMES_COLOR },

    [0xa0] = { "Stained Glass Pane", I_MTYPE, MNAMES_COLOR },
    [0xa1] = { "Leaves2", I_MTYPE|I_STATE, MNAMES_NEWWOOD },    // S: decay
    [0xa2] = { "Wood2", I_MTYPE|I_MPOS, MNAMES_NEWWOOD },       // P: dir
    [0xa3] = { "Acacia Stairs", I_MPOS|I_STAIR },               // P: dir
    [0xa4] = { "Dark Oak Stairs", I_MPOS|I_STAIR },             // P: dir
    [0xa5] = { "Slime Block" },
    [0xa6] = { "Barrier" },
    [0xa7] = { "Iron Trapdoor", I_MPOS|I_STATE },               // P: dir,u/d, S: open/close
    [0xa8] = { "Prismarine", I_MTYPE,
               { NULL, "Brick", "Dark" }, },
    [0xa9] = { "Sea Lantern" },
    [0xaa] = { "Hay Bale", I_MPOS },                            // P: dir
    [0xab] = { "Carpet", I_MTYPE, MNAMES_COLOR },
    [0xac] = { "Hardened Clay" },
    [0xad] = { "Coal Block" },
    [0xae] = { "Packed Ice" },
    [0xaf] = { "Large Flower", I_MTYPE|I_STATE,                 // S: top/bottom
               { "Sunflower", "Lilac", "DblTallgrass", "Large Fern",
                 "Rose Bush", "Peony", NULL, NULL }, },

    [0xb0] = { "Standing Banner", I_MPOS },                     // P: dir
    [0xb1] = { "Wall Banner", I_MPOS },                         // P: dir
    [0xb2] = { "Inv. Daylight Sensor" },
    [0xb3] = { "Red Sandstone", I_MTYPE,
               { NULL, "Chiseled", "Smooth"} },
    [0xb4] = { "Red Sandstone Stairs", I_MPOS|I_STAIR },        // P: dir
    [0xb5] = { "Red Sandstone D-Slab", I_MTYPE,
               { [8]="Chiseled" }, },
    [0xb6] = { "Red Sandstone Slab", I_MPOS|I_SLAB },           // P: up/down
    [0xb7] = { "Spruce Fence Gate", I_MPOS|I_STATE },           // P: dir, S: open/close
    [0xb8] = { "Birch Fence Gate", I_MPOS|I_STATE },            // P: dir, S: open/close
    [0xb9] = { "Jungle Fence Gate", I_MPOS|I_STATE },           // P: dir, S: open/close
    [0xba] = { "Dark Oak Fence Gate", I_MPOS|I_STATE },         // P: dir, S: open/close
    [0xbb] = { "Acacia Fence Gate", I_MPOS|I_STATE },           // P: dir, S: open/close
    [0xbc] = { "Spruce Fence" },
    [0xbd] = { "Birch Fence" },
    [0xbe] = { "Jungle Fence" },
    [0xbf] = { "Dark Oak Fence" },

    [0xc0] = { "Acacia Fence" },
    [0xc1] = { "Spruce Door", I_MPOS|I_STATE },                 // P: dir, S: open/close
    [0xc2] = { "Birch Door", I_MPOS|I_STATE },                  // P: dir, S: open/close
    [0xc3] = { "Jungle Door", I_MPOS|I_STATE },                 // P: dir, S: open/close
    [0xc4] = { "Dark Oak Door", I_MPOS|I_STATE },               // P: dir, S: open/close
    [0xc5] = { "Acacia Door", I_MPOS|I_STATE },                 // P: dir, S: open/close

    ////////////////////////////////////////////////////////////////////////
    // Items

    [0x100] = { "Iron Shovel", I_ITEM|I_NSTACK },
    [0x101] = { "Iron Pickaxe", I_ITEM|I_NSTACK },
    [0x102] = { "Iron Axe", I_ITEM|I_NSTACK },
    [0x103] = { "Flint and Steel", I_ITEM|I_NSTACK },
    [0x104] = { "Apple", I_ITEM },
    [0x105] = { "Bow", I_ITEM|I_NSTACK },
    [0x106] = { "Arrow", I_ITEM },
    [0x107] = { "Coal", I_ITEM|I_MTYPE,
                { NULL, "Charcoal" }, },
    [0x108] = { "Diamond", I_ITEM },
    [0x109] = { "Iron Ingot", I_ITEM },
    [0x10a] = { "Gold Ingot", I_ITEM },
    [0x10b] = { "Iron Sword", I_ITEM },
    [0x10c] = { "Wooden Sword", I_ITEM|I_NSTACK },
    [0x10d] = { "Wooden Shovel", I_ITEM|I_NSTACK },
    [0x10e] = { "Wooden Pickaxe", I_ITEM|I_NSTACK },
    [0x10f] = { "Wooden Axe", I_ITEM|I_NSTACK },

    [0x110] = { "Stone Sword", I_ITEM|I_NSTACK },
    [0x111] = { "Stone Shovel", I_ITEM|I_NSTACK },
    [0x112] = { "Stone Pickaxe", I_ITEM|I_NSTACK },
    [0x113] = { "Stone Axe", I_ITEM|I_NSTACK },
    [0x114] = { "Diamond Sword", I_ITEM|I_NSTACK },
    [0x115] = { "Diamond Shovel", I_ITEM|I_NSTACK },
    [0x116] = { "Diamond Pickaxe", I_ITEM|I_NSTACK },
    [0x117] = { "Diamond Axe", I_ITEM|I_NSTACK },
    [0x118] = { "Stick", I_ITEM },
    [0x119] = { "Bowl", I_ITEM },
    [0x11a] = { "Mushroom Stew", I_ITEM|I_NSTACK },
    [0x11b] = { "Golden Sword", I_ITEM },
    [0x11c] = { "Golden Shovel", I_ITEM },
    [0x11d] = { "Golden Pickaxe", I_ITEM },
    [0x11e] = { "Golden Axe", I_ITEM },
    [0x11f] = { "String", I_ITEM },

    [0x120] = { "Feather", I_ITEM },
    [0x121] = { "Gunpowder", I_ITEM },
    [0x122] = { "Wooden Hoe", I_ITEM|I_NSTACK },
    [0x123] = { "Stone Hoe", I_ITEM|I_NSTACK },
    [0x124] = { "Iron Hoe", I_ITEM|I_NSTACK },
    [0x125] = { "Diamond Hoe", I_ITEM|I_NSTACK },
    [0x126] = { "Golden Hoe", I_ITEM|I_NSTACK },
    [0x127] = { "Wheat Seeds", I_ITEM },
    [0x128] = { "Wheat", I_ITEM },
    [0x129] = { "Bread", I_ITEM },
    [0x12a] = { "Leather Helmet", I_ITEM|I_NSTACK },
    [0x12b] = { "Leather Chestplate", I_ITEM|I_NSTACK },
    [0x12c] = { "Leather Leggings", I_ITEM|I_NSTACK },
    [0x12d] = { "Leather Boots", I_ITEM|I_NSTACK },
    [0x12e] = { "Chainmail Helmet", I_ITEM|I_NSTACK },
    [0x12f] = { "Chainmail Chestplate", I_ITEM|I_NSTACK },

    [0x130] = { "Chainmail Leggings", I_ITEM|I_NSTACK },
    [0x131] = { "Chainmail Boots", I_ITEM|I_NSTACK },
    [0x132] = { "Iron Helmet", I_ITEM|I_NSTACK },
    [0x133] = { "Iron Chestplate", I_ITEM|I_NSTACK },
    [0x134] = { "Iron Leggings", I_ITEM|I_NSTACK },
    [0x135] = { "Iron Boots", I_ITEM|I_NSTACK },
    [0x136] = { "Diamond Helmet", I_ITEM|I_NSTACK },
    [0x137] = { "Diamond Chestplate", I_ITEM|I_NSTACK },
    [0x138] = { "Diamond Leggings", I_ITEM|I_NSTACK },
    [0x139] = { "Diamond Boots", I_ITEM|I_NSTACK },
    [0x13a] = { "Golden Helmet", I_ITEM|I_NSTACK },
    [0x13b] = { "Golden Chestplate", I_ITEM|I_NSTACK },
    [0x13c] = { "Golden Leggings", I_ITEM|I_NSTACK },
    [0x13d] = { "Golden Boots", I_ITEM|I_NSTACK },
    [0x13e] = { "Flint", I_ITEM },
    [0x13f] = { "Porkchop", I_ITEM },

    [0x140] = { "Cooked Porkchop", I_ITEM },
    [0x141] = { "Painting", I_ITEM },
    [0x142] = { "Golden Apple", I_ITEM,
                { NULL, "Enchanted" }, },
    [0x143] = { "Sign", I_ITEM|I_S16 },
    [0x144] = { "Wooden Door", I_ITEM|I_NSTACK },
    [0x145] = { "Bucket", I_ITEM },
    [0x146] = { "Water Bucket", I_ITEM|I_NSTACK },
    [0x147] = { "Lava Bucket", I_ITEM|I_NSTACK },
    [0x148] = { "Minecart", I_ITEM|I_NSTACK },
    [0x149] = { "Saddle", I_ITEM|I_NSTACK },
    [0x14a] = { "Iron Door", I_ITEM|I_NSTACK },
    [0x14b] = { "Redstone", I_ITEM },
    [0x14c] = { "Snowball", I_ITEM|I_S16 },
    [0x14d] = { "Boat", I_ITEM|I_NSTACK },
    [0x14e] = { "Leather", I_ITEM },
    [0x14f] = { "Milk Bucket", I_ITEM|I_NSTACK },

    [0x150] = { "Brick", I_ITEM },
    [0x151] = { "Clay Ball", I_ITEM|I_S16 },
    [0x152] = { "Reeds", I_ITEM },
    [0x153] = { "Paper", I_ITEM },
    [0x154] = { "Book", I_ITEM },
    [0x155] = { "Slime Ball", I_ITEM },
    [0x156] = { "Chest Minecart", I_ITEM|I_NSTACK },
    [0x157] = { "Furnace Minecart", I_ITEM|I_NSTACK },
    [0x158] = { "Egg", I_ITEM|I_S16 },
    [0x159] = { "Compass", I_ITEM|I_NSTACK },
    [0x15a] = { "Fishing Rod", I_ITEM|I_NSTACK },
    [0x15b] = { "Clock", I_ITEM|I_NSTACK },
    [0x15c] = { "Glowstone Dust", I_ITEM },
    [0x15d] = { "Raw Fish", I_ITEM,
                { "Nondescript", "Salmon", "Clownfish", "Pufferfish" }, },
    [0x15e] = { "Cooked Fish", I_ITEM,
                { "Nondescript", "Salmon", "Clownfish", "Pufferfish" }, },
    [0x15f] = { "Dye", I_ITEM|I_MTYPE,
                { "Ink Sac", "Rose Red", "Cactus Green", "Cocoa Beans",
                  "Lapis Lazuli", "Purple", "Cyan", "L.Gray",
                  "Gray", "Pink", "Lime", "Dandelion Yellow",
                  "L.Blue", "Magenta", "Orange", "Bone Meal" }, },

    [0x160] = { "Bone", I_ITEM },
    [0x161] = { "Sugar", I_ITEM },
    [0x162] = { "Cake", I_ITEM },
    [0x163] = { "Bed", I_ITEM },
    [0x164] = { "Repeater", I_ITEM },
    [0x165] = { "Cookie", I_ITEM },
    [0x166] = { "Map", I_ITEM },
    [0x167] = { "Shears", I_ITEM },
    [0x168] = { "Melon", I_ITEM },
    [0x169] = { "Pumpkin Seeds", I_ITEM },
    [0x16a] = { "Melon Seeds", I_ITEM },
    [0x16b] = { "Raw Beef", I_ITEM },
    [0x16c] = { "Steak", I_ITEM },
    [0x16d] = { "Raw Chicken", I_ITEM },
    [0x16e] = { "Cooked Chicken", I_ITEM },
    [0x16f] = { "Rotten Flesh", I_ITEM },

    [0x170] = { "Ender Pearl", I_ITEM },
    [0x171] = { "Blaze Rod", I_ITEM },
    [0x172] = { "Ghast Tear", I_ITEM },
    [0x173] = { "Gold Nugget", I_ITEM },
    [0x174] = { "Nether Wart", I_ITEM },
    [0x175] = { "Potion", I_ITEM, }, //TODO: potion names
    [0x176] = { "Glass Bottle", I_ITEM },
    [0x177] = { "Spider Eye", I_ITEM },
    [0x178] = { "Fermented Eye", I_ITEM },
    [0x179] = { "Blaze Powder", I_ITEM },
    [0x17a] = { "Magma Cream", I_ITEM },
    [0x17b] = { "Brewing Stand", I_ITEM },
    [0x17c] = { "Cauldron", I_ITEM },
    [0x17d] = { "Ender Eye", I_ITEM },
    [0x17e] = { "Speckled Melon", I_ITEM },
    [0x17f] = { "Spawn Egg", I_ITEM },

    [0x180] = { "Exp Potion", I_ITEM },
    [0x181] = { "Fire Charge", I_ITEM },
    [0x182] = { "Book and Quill", I_ITEM },
    [0x183] = { "Written Book", I_ITEM },
    [0x184] = { "Emerald", I_ITEM },
    [0x185] = { "Item Frame", I_ITEM },
    [0x186] = { "Flower Pot", I_ITEM },
    [0x187] = { "Carrot", I_ITEM },
    [0x188] = { "Potato", I_ITEM },
    [0x189] = { "Baked Potato", I_ITEM },
    [0x18a] = { "Poisonous Potato", I_ITEM },
    [0x18b] = { "Empty Map", I_ITEM },
    [0x18c] = { "Golden Carrot", I_ITEM },
    [0x18d] = { "Mob Head", I_ITEM },
    [0x18e] = { "Carrot on a Stick", I_ITEM },
    [0x18f] = { "Nether Star", I_ITEM },

    [0x190] = { "Pumpkin Pie", I_ITEM },
    [0x191] = { "Firework Rocket", I_ITEM },
    [0x192] = { "Firework Charge", I_ITEM },
    [0x193] = { "Enchanted Book", I_ITEM },
    [0x194] = { "Comparator", I_ITEM },
    [0x195] = { "Netherbrick", I_ITEM },
    [0x196] = { "Quartz", I_ITEM },
    [0x197] = { "TNT Minecart", I_ITEM },
    [0x198] = { "Hopper Minecart", I_ITEM },
    [0x199] = { "Prismarine Shard", I_ITEM },
    [0x19a] = { "Prismarine Crystal", I_ITEM },
    [0x19b] = { "Raw Rabbit", I_ITEM },
    [0x19c] = { "Cooked Rabbit", I_ITEM },
    [0x19d] = { "Rabbit Stew", I_ITEM },
    [0x19e] = { "Rabbit Foot", I_ITEM },
    [0x19f] = { "Rabbit Hide", I_ITEM },

    [0x1a0] = { "Armor Stand", I_ITEM },
    [0x1a1] = { "Iron Horse Armor", I_ITEM },
    [0x1a2] = { "Golden Horse Armor", I_ITEM },
    [0x1a3] = { "Diamond Horse Armor", I_ITEM },
    [0x1a4] = { "Lead", I_ITEM },
    [0x1a5] = { "Name Tag", I_ITEM },
    [0x1a6] = { "Cmd Block Minecart", I_ITEM },
    [0x1a7] = { "Mutton", I_ITEM },
    [0x1a8] = { "Cooked Mutton", I_ITEM },
    [0x1a9] = { "Banner", I_ITEM },
    [0x1ab] = { "Spruce Door", I_ITEM },
    [0x1ac] = { "Birch Door", I_ITEM },
    [0x1ad] = { "Jungle Door", I_ITEM },
    [0x1ae] = { "Acacia Door", I_ITEM },
    [0x1af] = { "Dark Oak Door", I_ITEM },

    [0x8d0] = { "Record 13", I_ITEM },
    [0x8d1] = { "Record Cat", I_ITEM },
    [0x8d2] = { "Record Blocks", I_ITEM },
    [0x8d3] = { "Record Chirp", I_ITEM },
    [0x8d4] = { "Record Far", I_ITEM },
    [0x8d5] = { "Record Mall", I_ITEM },
    [0x8d6] = { "Record Mellohi", I_ITEM },
    [0x8d7] = { "Record Stal", I_ITEM },
    [0x8d8] = { "Record Strad", I_ITEM },
    [0x8d9] = { "Record Ward", I_ITEM },
    [0x8da] = { "Record 11", I_ITEM },
    [0x8db] = { "Record Wait", I_ITEM },


    [0x8ff] = { NULL }, //Terminator
};

const char * get_item_name(char *buf, slot_t *s) {
    if (s->item<0) {
        sprintf(buf, "-");
        return buf;
    }

    if (ITEMS[s->item].name) {
        int pos = sprintf(buf, "%s", ITEMS[s->item].name);
        if ((ITEMS[s->item].flags&I_MTYPE) && ITEMS[s->item].mname[s->damage])
            sprintf(buf+pos, " (%s)",ITEMS[s->item].mname[s->damage]);
    }
    else {
        printf(buf, "???");
    }
    return buf;
}

const char * get_bid_name(char *buf, bid_t b) {
    slot_t s = { .count=1, .item = b.bid, .damage = b.meta };
    return get_item_name(buf, &s);
}

bid_t get_base_material(bid_t mat) {
    // only accept existing block types
    assert(mat.bid <0x100);
    const item_id * it = &ITEMS[mat.bid];
    assert(it->name);

    // if the block has no I_MTYPE subtypes, so base meta is 0
    if (!(it->flags&I_MTYPE)) { mat.meta = 0; return mat; }

    // block meta is used for I_MTYPE but not used for position/state => base meta as is
    if (!(it->flags&(I_MPOS|I_STATE))) return mat;

    // everything else needs to be determined individually
    switch (mat.bid) {
        case 0x06: // Sapling
        case 0x2c: // Stone slab
        case 0x7e: // Wooden slab
        case 0xaf: // Large Flower
            mat.meta &= 7;
            break;

        case 0x11: // Wood
        case 0x12: // Leaves
        case 0xa1: // Leaves2
        case 0xa2: // Wood2
            mat.meta &= 3;
            break;

        case 0x9b:
            if (mat.meta > 2) mat.meta = 2;
            break;

        default:
            assert(0);
    }
    return mat;
}

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
    [Entity]          = IllegalEntityType,
    [LivingEntity]    = Entity,
    [Ageable]         = LivingEntity,
    [ArmorStand]      = LivingEntity,
    [Human]           = LivingEntity,
    [Horse]           = Ageable,
    [Bat]             = LivingEntity,
    [Tameable]        = Tameable,
    [Ocelot]          = Tameable,
    [Wolf]            = Tameable,
    [Pig]             = Ageable,
    [Rabbit]          = Ageable,
    [Sheep]           = Ageable,
    [Villager]        = Ageable,
    [Enderman]        = LivingEntity,
    [Zombie]          = LivingEntity,
    [ZombiePigman]    = Zombie,
    [Blaze]           = LivingEntity,
    [Spider]          = LivingEntity,
    [CaveSpider]      = Spider,
    [Creeper]         = LivingEntity,
    [Ghast]           = LivingEntity,
    [Slime]           = LivingEntity,
    [MagmaCube]       = Slime,
    [Skeleton]        = LivingEntity,
    [Witch]           = LivingEntity,
    [IronGolem]       = LivingEntity,
    [Wither]          = LivingEntity,
    [Boat]            = Entity,
    [Minecart]        = Entity,
    [FurnaceMinecart] = Minecart,
    [Item]            = Entity,
    [Arrow]           = Entity,
    [Firework]        = Entity,
    [ItemFrame]       = Entity,
    [EnderCrystal]    = Entity,
};

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
