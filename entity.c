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

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

const char * METATYPES[] = {
    [META_BYTE]       = "byte",
    [META_SHORT]      = "short",
    [META_INT]        = "int",
    [META_FLOAT]      = "float",
    [META_STRING]     = "string",
    [META_SLOT]       = "slot",
    [META_COORD]      = "coord",
    [META_ROT]        = "rot",
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

    // mark all entries as not present - we use the same 0x7f value
    // that Mojang uses as terminator
    for(i=0; i<32; i++) m[i].h = 0x7f;

    while (1) {
        uint8_t kv = read_char(p);
        if (kv == 0x7f) break; // terminator

        uint8_t k = kv&31;
        uint8_t type = (kv>>5)&7;
        metadata *mm = &m[k];
        mm->h = kv;

        switch (mm->type) {
            case META_BYTE:   mm->b = read_char(p);    break;
            case META_SHORT:  mm->s = read_short(p);   break;
            case META_INT:    mm->i = read_int(p);     break;
            case META_FLOAT:  mm->f = read_float(p);   break;
            case META_STRING: p = read_string(p,mm->str); break;
            case META_SLOT:   p = read_slot(p,&mm->slot); break;
            case META_COORD:
                mm->x = read_int(p);
                mm->y = read_int(p);
                mm->z = read_int(p);
                break;
            case META_ROT:
                mm->pitch = read_float(p);
                mm->yaw   = read_float(p);
                mm->roll  = read_float(p);
                break;
        }
    }

    return p;
}

uint8_t * write_metadata(uint8_t *w, metadata *meta) {
    assert(meta);

    int i,j;
    char bool;
    for (i=0; i<32; i++) {
        metadata *mm = meta+i;
        if (mm->h==0x7f) continue;

        lh_write_char(w, mm->h);
        switch (mm->type) {
            case META_BYTE:     write_char(w, mm->b);    break;
            case META_SHORT:    write_short(w, mm->s);   break;
            case META_INT:      write_int(w, mm->i);     break;
            case META_FLOAT:    write_float(w, mm->f);   break;
            case META_STRING:   w = write_string(w, mm->str); break;
            case META_SLOT:     w = write_slot(w, &mm->slot); break;
            case META_COORD:
                write_int(w, mm->x);
                write_int(w, mm->y);
                write_int(w, mm->z);
                break;
            case META_ROT:
                write_float(w, mm->pitch);
                write_float(w, mm->yaw);
                write_float(w, mm->roll);
                break;
        }
    }
    lh_write_char(w, 0x7f);

    return w;
}

void dump_metadata(metadata *meta, EntityType et) {
    if (!meta) return;

    int i;
    for (i=0; i<32; i++) {
        metadata *mm = meta+i;
        if (mm->h==0x7f) continue;

        printf("\n    ");

        const char * name = NULL;
        EntityType ett = et;
        while ((!name) && (ett!=IllegalEntityType)) {
            name = METANAME[ett][mm->key];
            ett = ENTITY_HIERARCHY[ett];
        }

        printf("%2d %-24s [%-6s] = ", mm->key, name?name:"Unknown",METATYPES[mm->type]);
        switch (mm->type) {
            case META_BYTE:   printf("%d",  mm->b);   break;
            case META_SHORT:  printf("%d",  mm->s);   break;
            case META_INT:    printf("%d",  mm->i);   break;
            case META_FLOAT:  printf("%.1f",mm->f);   break;
            case META_STRING: printf("\"%s\"", mm->str); break;
            case META_SLOT:   dump_slot(&mm->slot); break;
            case META_COORD:
                printf("(%d,%d,%d)",mm->x,mm->y,mm->z); break;
            case META_ROT:
                printf("%.1f,%.1f,%.1f",mm->pitch,mm->yaw,mm->roll); break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Entity Metadata

const char * METANAME[][32] = {
    [Entity] = {
        [0]  = "Flags",
        [1]  = "Air",
        [2]  = "Name tag",
        [3]  = "Always show name tag",
        [4]  = "Is silent",
    },
    [Living] = {
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
    [Player] = {
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
    [TameableAnimal] = {
        [16] = "TameableAnimal flags",
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
    [MinecartFurnace] = {
        [16] = "Is powered",
    },
    [Item] = {
        [10] = "Item",
    },
    [Arrow] = {
        [16] = "Is critical",
    },
    [Fireworks] = {
        [8] = "Fireworks data",
    },
    [ItemFrame] = {
        [8] = "Framed item",
        [9] = "Rotation",
    },
};

const EntityType ENTITY_HIERARCHY[] = {
    //// Superclasses
    [Entity]          = IllegalEntityType,
    [Living]          = Entity,
    [Ageable]         = Living,
    [Player]          = Living,
    [TameableAnimal]  = Ageable,
    [Item]            = Entity,
    [Fireworks]       = Entity,
    [Mob]             = Living,
    [Monster]         = Living,

    //// Monsters
    [Creeper]         = Living,
    [Skeleton]        = Living,
    [Spider]          = Living,
    [GiantZombie]     = Living,
    [Zombie]          = Living,
    [Slime]           = Living,
    [Ghast]           = Living,
    [ZombiePigman]    = Zombie,
    [Enderman]        = Living,
    [CaveSpider]      = Spider,
    [Silverfish]      = Living,
    [Blaze]           = Living,
    [MagmaCube]       = Slime,
    [Enderdragon]     = Living,
    [Wither]          = Living,
    [Bat]             = Living,
    [Witch]           = Living,
    [Endermite]       = Living,
    [Guardian]        = Living,

    //// Mobs
    [Pig]             = Ageable,
    [Sheep]           = Ageable,
    [Cow]             = Ageable,
    [Chicken]         = Ageable,
    [Squid]           = Living,
    [Wolf]            = TameableAnimal,
    [Mooshroom]       = Ageable,
    [Snowman]         = Living,
    [Ocelot]          = TameableAnimal,
    [IronGolem]       = Living,
    [Horse]           = Ageable,
    [Rabbit]          = Ageable,
    [Villager]        = Ageable,

    //// Objects
    [Boat]            = Entity,
    [ItemStack]       = Entity,
    [Minecart]        = Entity,
    [ChestMinecart]   = Minecart,
    [MinecartFurnace] = Minecart,
    [EnderCrystal]    = Entity,
    [Arrow]           = Entity,
    [ItemFrame]       = Entity,
    [ArmorStand]      = Living,

    //// Generic
    [Painting]        = Entity,
    [ExperienceOrb]   = Entity,
};

#define ENUMNAME(name) [name] = #name

const char * ENTITY_NAMES[MaxEntityType] = {
    // Generic
    ENUMNAME(Entity),
    ENUMNAME(Painting),
    ENUMNAME(ExperienceOrb),

    // Hostile mobs
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

    // Passive mobs
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

    // Objects
    ENUMNAME(Boat),
    ENUMNAME(ItemStack),
    ENUMNAME(AreaEffectCloud),
    ENUMNAME(Minecart),
    ENUMNAME(ChestMinecart),
    ENUMNAME(MinecartFurnace),
    ENUMNAME(MinecartCommandBlock),
    ENUMNAME(ActivatedTNT),
    ENUMNAME(FallingObjects),
    ENUMNAME(ItemFrame),
    ENUMNAME(LeashKnot),
    ENUMNAME(ArmorStand),

    // Projectiles
    ENUMNAME(Arrow),
    ENUMNAME(Snowball),
    ENUMNAME(Egg),
    ENUMNAME(Fireball),
    ENUMNAME(FireCharge),
    ENUMNAME(ThrownEnderpearl),
    ENUMNAME(WitherSkull),
    ENUMNAME(EyeOfEnder),
    ENUMNAME(ThrownPotion),
    ENUMNAME(FallingDragonEgg),
    ENUMNAME(ThrownExpBottle),
    ENUMNAME(FireworkRocket),
    ENUMNAME(FishingFloat),
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



