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

    ElderGuardian       = 4,
    WitherSkeleton      = 5,
    Stray               = 6,

    Potion              = 7,
    FallingBlock        = 8,
    Hanging             = 9,
    Insentinent         = 10,
    Ambient             = 11,
    Creature            = 12,
    Animal              = 13,
    Golem               = 14,
    Flying              = 15,

    Husk                = 23,
    ZombieVillager      = 27,
    SkeletonHorse       = 28,
    ZombieHorse         = 29,
    Donkey              = 31,
    Mule                = 32,
    EvocationFangs      = 33,
    EvocationIllager    = 34,
    Vex                 = 35,
    VindicationIllager  = 36,
    IllusionIllager     = 37,

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
    Shulker             = 69,

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
    Llama               = 103,
    LlamaSpit           = 104,
    Parrot              = 105,

    Villager            = 120,

    // inofficial entity types so we can track exp orbs and paintings
    TameableAnimal      = 244,
    Item                = 245,
    Fireworks           = 246,

    Painting            = 254,
    ExperienceOrb       = 255,

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
    ShulkerBullet       = 67+256,

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
    SpectralArrow       = 91+256,
    TippedArrow         = 92+256,
    DragonFireball      = 93+256,

    //// Terminating ID
    MaxEntityType       = 512,
} EntityType;

extern const char * METANAME[][32];
extern const EntityType ENTITY_HIERARCHY[];
extern const char * ENTITY_NAMES[];
const char * get_entity_name(char *buf, EntityType type);

////////////////////////////////////////////////////////////////////////////////

#define META_BYTE       0
#define META_VARINT     1
#define META_FLOAT      2
#define META_STRING     3
#define META_CHAT       4
#define META_OPTCHAT    5
#define META_SLOT       6
#define META_BOOL       7
#define META_VEC3       8
#define META_POS        9
#define META_OPTPOS     10
#define META_DIR        11
#define META_OPTUUID    12
#define META_BID        13
#define META_NBT        14
#define META_PARTICLE   15
#define META_NONE       255

// single metadata key-value pair
typedef struct {
    uint8_t         key;
    uint32_t        type;
    union {
        uint8_t     b;
        int32_t     i;
        float       f;
        char        *str;
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
        uint32_t    block;
        nbt_t*      nbt;
        //FIXME: particle metadata is ignored and discarded at the moment
    };
} metadata;

extern const char * METATYPES[];

metadata * clone_metadata(metadata *meta);
metadata * update_metadata(metadata *meta, metadata *upd);
void free_metadata(metadata *meta);
uint8_t * read_metadata(uint8_t *p, metadata **meta);
uint8_t * write_metadata(uint8_t *w, metadata *meta);
void dump_metadata(metadata *meta, EntityType et);

