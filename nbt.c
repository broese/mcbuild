#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt.h"
#include "lh_debug.h"
#include "lh_buffers.h"

static char nbt_parsetag(unsigned char **ptr, char *name) {
    char type = read_char(*ptr);
    if (type < NBT_TAG_END || type > NBT_TAG_INT_ARRAY)
        LH_ERROR(-1,"Unknown tag type %d at %p\n",type,*ptr-1);

    if (type != NBT_TAG_END) {
        // read the name
        //hexdump(*ptr, 64);
        short len = read_short(*ptr);
        while(len--) *name++ = *(*ptr)++;
    }
    *name++ = 0;
    return type;
}

static void nbt_parse_noheader(unsigned char **ptr, nbte *elem) {
    // fecth payload
    switch (elem->type) {

        // elemental types
        case NBT_TAG_BYTE:
            elem->count = 1;
            elem->v.b = read_char(*ptr);
            break;
        case NBT_TAG_SHORT:
            elem->count = 1;
            elem->v.s = read_short(*ptr);
            break;
        case NBT_TAG_INT:
            elem->count = 1;
            elem->v.i = read_int(*ptr);
            break;
        case NBT_TAG_LONG:
            elem->count = 1;
            elem->v.l = read_long(*ptr);
            break;
        case NBT_TAG_FLOAT:
            elem->count = 1;
            elem->v.f = read_float(*ptr);
            break;
        case NBT_TAG_DOUBLE:
            elem->count = 1;
            elem->v.d = read_double(*ptr);
            break;

        // arrays
        case NBT_TAG_BYTE_ARRAY: {
            elem->count = read_int(*ptr);
            ALLOCBE(elem->v.ba, elem->count);
            memcpy(elem->v.ba, *ptr, elem->count);
            (*ptr)+=elem->count;
            break;
        }
        case NBT_TAG_INT_ARRAY: {
            elem->count = read_int(*ptr);
            ALLOCNE(elem->v.ia, elem->count);
            int i;
            for(i=0; i<elem->count; i++) {
                elem->v.ia[i] = read_int(*ptr);
            }
            break;
        }
        case NBT_TAG_STRING: {
            elem->count = read_short(*ptr);
            ALLOCBE(elem->v.str, elem->count+1);
            memcpy(elem->v.str, *ptr, elem->count);
            elem->v.str[elem->count] = 0;
            (*ptr)+=elem->count;
            break;
        }

        // lists
        case NBT_TAG_LIST: {
            char type = read_char(*ptr);
            elem->count = read_int(*ptr);
            ALLOCNE(elem->v.list, elem->count);

            int i;
            for(i=0; i<elem->count; i++) {
                nbte * subelem = &elem->v.list[i];
                subelem->type = type;
                subelem->name = NULL;
                nbt_parse_noheader(ptr, subelem);
            }

            break;
        }
        case NBT_TAG_COMPOUND: {
            ARRAY_ALLOC(elem->v.comp,elem->count,0);
            while (1) {
                nbte *subelem = nbt_parse(ptr);
                if (!subelem) break; // Tag_End found - stop parsing the compound

                ARRAY_ADD(elem->v.comp,elem->count,1);
                nbte *newelem = &elem->v.list[elem->count-1];

                memcpy(newelem, subelem, sizeof(nbte));
                free(subelem);
            }
            break;
        }
    }
}

nbte *nbt_parse(unsigned char **ptr) {
    char type = read_char(*ptr);

    if (type == NBT_TAG_END) return NULL;

    ALLOC(nbte,elem);

    if (type < NBT_TAG_END || type > NBT_TAG_INT_ARRAY)
        LH_ERROR(NULL,"Unknown tag type %d at %p\n",type,*ptr-1);

    elem->type = type;

    // fetch name
    int nlen = read_short(*ptr);
    ALLOCBE(elem->name,nlen+1);
    memcpy(elem->name, *ptr, nlen);
    elem->name[nlen] = 0;
    (*ptr)+=nlen;

    nbt_parse_noheader(ptr, elem);

    return elem;
}

void nbt_dump(nbte *elem, int indent) {
    int i;
    for(i=0; i<indent; i++) printf("  ");
    printf("%-10s => ",elem->name?elem->name:"?");

    
    switch(elem->type) {
        // elemental types
        case NBT_TAG_BYTE:
            printf("Byte %02x",elem->v.b);
            break;
        case NBT_TAG_SHORT:
            printf("Short %04x",elem->v.s);
            break;
        case NBT_TAG_INT:
            printf("Int %08x",elem->v.i);
            break;
        case NBT_TAG_LONG:
            printf("Long %016llx",elem->v.l);
            break;
        case NBT_TAG_FLOAT:
            printf("Float %f",elem->v.f);
            break;
        case NBT_TAG_DOUBLE:
            printf("Byte %f",elem->v.d);
            break;

        // arrays
        case NBT_TAG_BYTE_ARRAY: {
            printf("Byte Array [%d]",elem->count);
            break;
        }
        case NBT_TAG_INT_ARRAY: {
            printf("Int Array [%d]",elem->count);
            break;
        }
        case NBT_TAG_STRING: {
            printf("String \"%s\"",elem->v.str);
            break;
        }

        // lists
        case NBT_TAG_LIST: {
            printf("List %d entries [\n",elem->count);
            for(i=0; i<elem->count; i++)
                nbt_dump(&elem->v.list[i],indent+1);
            printf("\n");
            for(i=0; i<indent; i++) printf("  ");
            printf("]");
            break;
        }
        case NBT_TAG_COMPOUND: {
            printf("Compound %d entries {\n",elem->count);
            for(i=0; i<elem->count; i++)
                nbt_dump(&elem->v.comp[i],indent+1);
            printf("\n");
            for(i=0; i<indent; i++) printf("  ");
            printf("}");
            break;
        }
    }
    printf("\n");
}

void nbt_free(nbte *elem) {
    int i;

    if (elem->name) free(elem->name);

    switch(elem->type) {
        case NBT_TAG_BYTE_ARRAY:
        case NBT_TAG_INT_ARRAY:
        case NBT_TAG_STRING:
            free(elem->v.ba);
            break;

        case NBT_TAG_LIST:
        case NBT_TAG_COMPOUND:
            for(i=0; i<elem->count; i++)
                nbt_free(&elem->v.list[i]);
            break;
    }
}

nbte * nbt_ce(nbte *elem, const char *name) {
    if (!elem) LH_ERROR(NULL,"nbt_ce on the NULL");
    if (elem->type != NBT_TAG_COMPOUND) LH_ERROR(NULL,"nbt_ce on a non-compound object");

    int i;
    for(i=0; i<elem->count; i++) {
        nbte *se = &elem->v.comp[i];
        if (se->name && !strcmp(se->name, name))
            return se;
    }
    return NULL;
}

nbte * nbt_le(nbte *elem, int index) {
    if (!elem) LH_ERROR(NULL,"nbt_le on the NULL");
    if (elem->type != NBT_TAG_LIST) LH_ERROR(NULL,"nbt_le on a non-list object");
    if (index <0 || index >= elem->count) return NULL;//LH_ERROR(NULL,"nbt_le - index %d out of bounds", index);

    return &elem->v.comp[index];
}
