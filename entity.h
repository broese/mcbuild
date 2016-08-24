
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
    Living              = 1,
    Ageable             = 2,
    Player              = 3,
    TameableAnimal      = 4,
    Item                = 5,
    Fireworks           = 6,
    Potion              = 7,
    FallingBlock        = 8,
    Hanging             = 9,
    Insentinent         = 10,
    Ambient             = 11,
    Creature            = 12,
    Animal              = 13,
    Golem               = 14,
    Flying              = 15,
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
    PolarBear           = 102,

    Villager            = 120,

    //// Objects
    Boat                = 1 +256,
    ItemStack           = 2 +256,
    AreaEffectCloud     = 3 +256,

    Minecart            = 10+256,
    ChestMinecart       = 11+256, // deprecated since 1.6
    MinecartFurnace     = 12+256, // deprecated since 1.6
    MinecartCommandBlock= 13+256,

    ActivatedTNT        = 50+256,
    EnderCrystal        = 51+256,

    Arrow               = 60+256,
    Snowball            = 61+256,
    Egg                 = 62+256,
    Fireball            = 63+256,
    FireCharge          = 64+256,
    ThrownEnderpearl    = 65+256,
    WitherSkull         = 66+256,

    FallingObjects      = 70+256,
    ItemFrame           = 71+256,
    EyeOfEnder          = 72+256,
    ThrownPotion        = 73+256,
    FallingDragonEgg    = 74+256,
    ThrownExpBottle     = 75+256,
    FireworkRocket      = 76+256,
    LeashKnot           = 77+256,
    ArmorStand          = 78+256,

    FishingFloat        = 90+256,

    //// Terminating ID
    MaxEntityType       = 512,
} EntityType;

extern const char * METANAME[][32];
extern const EntityType ENTITY_HIERARCHY[];
extern const char * ENTITY_NAMES[];
const char * get_entity_name(char *buf, EntityType type);

////////////////////////////////////////////////////////////////////////////////
// Metadata format

#define META_BYTE    0
#define META_SHORT   1
#define META_INT     2
#define META_FLOAT   3
#define META_STRING  4
#define META_SLOT    5
#define META_COORD   6
#define META_ROT     7

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
        char    str[MCP_MAXSTR];
        slot_t  slot;
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

extern const char * METATYPES[];

metadata * clone_metadata(metadata *meta);
void free_metadata(metadata *meta);
uint8_t * read_metadata(uint8_t *p, metadata **meta);
uint8_t * write_metadata(uint8_t *w, metadata *meta);
void dump_metadata(metadata *meta, EntityType et);
