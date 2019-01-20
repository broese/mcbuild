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
    Entity              = 256-1,
    Living              = 256-2,
    Ageable             = 256-3,
    Hanging             = 256-9,
    Insentinent         = 256-10,
    Ambient             = 256-11,
    Creature            = 256-12,
    Animal              = 256-13,
    Golem               = 256-14,
    Flying              = 256-15,
    Monster             = 256-17,
    TameableAnimal      = 256-18,
    AbstractFireball    = 256-19,
    AbstractFish        = 256-20,
    AbstractHorse       = 256-21,
    AbstractIllager     = 256-22,
    AbstractSkeleton    = 256-23,
    Arrow               = 256-24,
    ChestedHorse        = 256-25,
    MinecartContainer   = 256-26,
    SpellcasterIllager  = 256-27,
    Throwable           = 256-28,
    WaterMob            = 256-29,
    Weather             = 256-30,

    //// Special
    ExperienceOrb       = 22,  // (SP_SpawnExperienceOrb)
    Painting            = 49,  // (SP_SpawnPainting)
    LightningBolt       = 91,  // (SP_SpawnGlobalEntity)
    Player              = 92,  // (SP_SpawnPlayer)

    //// Mobs (SP_SpawnMob)
    Bat                           = 3,
    Blaze                         = 4,
    CaveSpider                    = 6,
    Chicken                       = 7,
    Cod                           = 8,  // added
    Cow                           = 9,
    Creeper                       = 10,
    Donkey                        = 11,
    Dolphin                       = 12, // added
    Drowned                       = 14, // added
    ElderGuardian                 = 15,
    Enderdragon                   = 17,
    Enderman                      = 18,
    Endermite                     = 19,
    EvocationIllager              = 21,
    Ghast                         = 26,
    Giant                         = 27,
    Guardian                      = 28,
    Horse                         = 29,
    Husk                          = 30,
    IllusionIllager               = 31,
    Llama                         = 36,
    MagmaCube                     = 38,
    Mule                          = 46,
    Mooshroom                     = 47,
    Ocelot                        = 48,
    Parrot                        = 50,
    Pig                           = 51,
    Pufferfish                    = 52, // added
    ZombiePigman                  = 53,
    PolarBear                     = 54,
    Rabbit                        = 56,
    Salmon                        = 57, // added
    Sheep                         = 58,
    Shulker                       = 59,
    Silverfish                    = 61,
    Skeleton                      = 62,
    SkeletonHorse                 = 63,
    Slime                         = 64,
    Snowman                       = 66,
    Spider                        = 69,
    Squid                         = 70,
    Stray                         = 71,
    TropicalFish                  = 72, // added
    Turtle                        = 73, // added
    Vex                           = 78,
    Villager                      = 79,
    IronGolem                     = 80,
    VindicationIllager            = 81,
    Witch                         = 82,
    Wither                        = 83,
    WitherSkeleton                = 84,
    Wolf                          = 86,
    Zombie                        = 87,
    ZombieHorse                   = 88,
    ZombieVillager                = 89,
    Phantom                       = 90, // added

    //// Objects (SP_SpawnObject)
    Boat                = 1 +256,
    Item                = 2 +256,
    AreaEffectCloud     = 3 +256,

    Minecart            = 10+256,
    ChestMinecart       = 11+256, // deprecated since 1.6
    MinecartFurnace     = 12+256, // deprecated since 1.6
    MinecartCommandBlock= 13+256, // deprecated

    ActivatedTNT        = 50+256,
    EnderCrystal        = 51+256,

    TippedArrow         = 60+256,
    Snowball            = 61+256,
    Egg                 = 62+256,
    Fireball            = 63+256,
    FireCharge          = 64+256,
    ThrownEnderpearl    = 65+256,
    WitherSkull         = 66+256,
    ShulkerBullet       = 67+256,
    LlamaSpit           = 68+256, // moved from mob to object

    FallingBlock        = 70+256,
    ItemFrame           = 71+256,
    EyeOfEnder          = 72+256,
    Potion              = 73+256,
    FallingDragonEgg    = 74+256, // deprecated
    ThrownExpBottle     = 75+256,
    Fireworks           = 76+256,
    LeashKnot           = 77+256,
    ArmorStand          = 78+256,
    EvocationFangs      = 79+256, // moved from mob to object

    FishingFloat        = 90+256,
    SpectralArrow       = 91+256,
    DragonFireball      = 93+256,
    Trident             = 94+256, // added

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
