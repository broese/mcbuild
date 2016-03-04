/*
 Authors:
 Copyright 2012-2016 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

/*
  entity : entities and entity metadata handling
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define LH_DECLARE_SHORT_NAMES 1
#include <lh_buffers.h>
#include <lh_bytes.h>

#include "entity.h"

// Entities

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
// Entity Metadata

const char * METATYPES[] = {
    [META_BYTE]     = "byte",
    [META_VARINT]   = "varint",
    [META_FLOAT]    = "float",
    [META_STRING]   = "string",
    [META_CHAT]     = "chat",
    [META_SLOT]     = "slot",
    [META_BOOL]     = "bool",
    [META_VEC3]     = "vector3f",
    [META_POS]      = "position",
    [META_OPTPOS]   = "optional_position",
    [META_DIR]      = "direction",
    [META_OPTUUID]  = "optional_uuid",
    [META_BID]      = "block_id",
    [META_NONE]     = "-"
};

metadata * clone_metadata(metadata *meta) {
    if (!meta) return NULL;
    lh_create_num(metadata, newmeta, 32);
    memmove(newmeta, meta, 32*sizeof(metadata));
    int i;
    for(i=0; i<32; i++)
        if (newmeta[i].type == META_SLOT)
            clone_slot(&meta[i].slot, &newmeta[i].slot);
    return newmeta;
}

void free_metadata(metadata *meta) {
    if (!meta) return;
    int i;
    for(i=0; i<32; i++)
        if (meta[i].type == META_SLOT)
            clear_slot(&meta[i].slot);
    free(meta);
}

uint8_t * read_metadata(uint8_t *p, metadata **meta) {
    assert(meta);
    assert(!*meta);
    ssize_t mc = 0;

    // allocate a whole set of 32 values
    lh_alloc_num(*meta, 32);
    metadata *m = *meta;

    int i;
    int bool;

    // mark all entries as not present - we use the same 0xff value
    // that Mojang uses as terminator
    for(i=0; i<32; i++) m[i].type = META_NONE;

    while (1) {
        uint8_t key = read_char(p);
        if (key == 0xff) break; // terminator
        assert(key < 32);

        metadata *mm = &m[key];
        mm->key = key;
        mm->type = read_char(p);

        switch (mm->type) {
            case META_BYTE:     mm->b = read_char(p);    break;
            case META_VARINT:   mm->i = read_varint(p);  break;
            case META_FLOAT:    mm->f = read_float(p);   break;
            case META_STRING:   p = read_string(p,mm->str); break;
            case META_CHAT:     p = read_string(p,mm->chat); break; //VERIFY
            case META_SLOT:     p = read_slot(p,&mm->slot); break;
            case META_BOOL:     mm->bool = read_char(p);  break; //VERIFY
            case META_VEC3:     mm->fx=read_float(p);
                                mm->fy=read_float(p);
                                mm->fz=read_float(p); break;
            case META_POS:      mm->pos.p = read_long(p); break;
            case META_OPTPOS:   bool = read_char(p); //VERIFY
                                if (bool) {
                                    mm->pos.p = read_long(p);
                                }
                                else {
                                    mm->pos.p = (uint64_t)-1;
                                }
                                break;
            case META_DIR:      mm->dir = read_varint(p);  break;
            case META_OPTUUID:  bool = read_char(p); //VERIFY
                                if (bool) {
                                    memmove(mm->uuid,p,sizeof(uuid_t));
                                    p+=sizeof(uuid_t);
                                }
                                break;
            case META_BID:      mm->block = read_char(p); break; // note- block ID only, no meta
        }
    }

    return p;
}

void dump_metadata(metadata *meta, EntityType et) {
    if (!meta) return;

    int i;
    for (i=0; i<32; i++) {
        metadata *mm = meta+i;
        if (mm->type==META_NONE) continue;

        printf("\n    ");

        const char * name = NULL;
        EntityType ett = et;
        while ((!name) && (ett!=IllegalEntityType)) {
            name = METANAME[ett][mm->key];
            ett = ENTITY_HIERARCHY[ett];
        }

        printf("%2d %-24s [%-6s] = ", mm->key, name?name:"Unknown",METATYPES[mm->type]);
        switch (mm->type) {
            case META_BYTE:     printf("%d",  mm->b);   break;
            case META_VARINT:   printf("%d",  mm->i);   break;
            case META_FLOAT:    printf("%.1f",mm->f);   break;
            case META_STRING:
            case META_CHAT:     printf("\"%s\"", mm->str); break;
            case META_SLOT:     dump_slot(&mm->slot); break;
            case META_BOOL:     printf("%s", mm->bool?"true":"false"); break; //VERIFY
            case META_VEC3:     printf("%.1f,%.1f,%.1f", mm->fx, mm->fy, mm->fz); break;
            case META_POS:
            case META_OPTPOS:   printf("%d,%d,%d", mm->pos.x, mm->pos.y, mm->pos.z); break;
            case META_DIR:      printf("%d",mm->dir);  break;
            case META_OPTUUID:  hexprint(mm->uuid, sizeof(uuid_t));
            case META_BID:      printf("%2x (%d)", mm->block, mm->block); //TODO: print material name
            default:            printf("<unknown type>"); break;
        }
    }
}

