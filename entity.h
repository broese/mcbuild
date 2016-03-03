/*
 Authors:
 Copyright 2012-2016 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#pragma once

/*
  entity : entities and entity metadata handling
*/

#include <stdint.h>

#include "mcp_types.h"
#include "slot.h"

////////////////////////////////////////////////////////////////////////////////

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

#define META_BYTE    0
#define META_VARINT  1
#define META_FLOAT   2
#define META_STRING  3
#define META_CHAT    4
#define META_SLOT    5
#define META_BOOL    6
#define META_VEC3    7
#define META_POS     8
#define META_OPTPOS  9
#define META_DIR     10
#define META_OPTUUID 11
#define META_BID     12
#define META_NONE    255

// single metadata key-value pair
typedef struct {
    uint8_t         key;
    uint8_t         type;
    union {
        uint8_t     b;
        int32_t     i;
        float       f;
        char        str[MCP_MAXSTR];
        char        chat[MCP_MAXSTR];
        slot_t      slot;
        uint8_t     bool;
        struct {
            float   fx;
            float   fy;
            float   fz;
        };
        pos_t       pos;    // OPTPOS with missing position is encoded as (int64_t)-1
        int32_t     dir;
        uuid_t      uuid;   // missing UUID is encoded in all-zeros
        bid_t       bid;
    };
} metadata;

extern const char * METATYPES[];

metadata * clone_metadata(metadata *meta);
void free_metadata(metadata *meta);
uint8_t * read_metadata(uint8_t *p, metadata **meta);
void dump_metadata(metadata *meta, EntityType et);

