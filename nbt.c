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
            char type = read_char(*ptr); // type of elemets in the list
            elem->count = read_int(*ptr);
            ALLOCNE(elem->v.list, elem->count);

            int i;
            for(i=0; i<elem->count; i++) {
                nbte * subelem = ALLOCE(elem->v.list[i]);
                // the elements we put into the object are not
                // completely defined in the file - i.e. there's no name
                // so we recreate the header and use NULL for name
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

                // extend the list and add the new element at the end
                ARRAY_ADD(elem->v.comp,elem->count,1);
                elem->v.list[elem->count-1] = subelem;
            }
            break;
        }
    }
}

nbte *nbt_parse(unsigned char **ptr) {
    char type = read_char(*ptr);

    if (type == NBT_TAG_END) return NULL;

    if (type < NBT_TAG_END || type > NBT_TAG_INT_ARRAY)
        LH_ERROR(NULL,"Unknown tag type %d at %p\n",type,*ptr-1);

    ALLOC(nbte,elem);
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
            printf("Long %016lx",elem->v.l);
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
                nbt_dump(elem->v.list[i],indent+1);
            printf("\n");
            for(i=0; i<indent; i++) printf("  ");
            printf("]");
            break;
        }
        case NBT_TAG_COMPOUND: {
            printf("Compound %d entries {\n",elem->count);
            for(i=0; i<elem->count; i++)
                nbt_dump(elem->v.comp[i],indent+1);
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
                nbt_free(elem->v.list[i]);
            free(elem->v.list);
            break;
    }
    free(elem);
}

// retrieve a subelement from a compound elem, by its name
nbte * nbt_ce(nbte *elem, const char *name) {
    if (!elem) LH_ERROR(NULL,"nbt_ce on the NULL");
    if (elem->type != NBT_TAG_COMPOUND) LH_ERROR(NULL,"nbt_ce on a non-compound object");

    int i;
    for(i=0; i<elem->count; i++) {
        nbte *se = elem->v.comp[i];
        if (se->name && !strcmp(se->name, name))
            return se;
    }
    return NULL;
}

// retrieve a subelement from a list elem, by the index
nbte * nbt_le(nbte *elem, int index) {
    if (!elem) LH_ERROR(NULL,"nbt_le on the NULL");
    if (elem->type != NBT_TAG_LIST) LH_ERROR(NULL,"nbt_le on a non-list object");
    if (index <0 || index >= elem->count) return NULL; //LH_ERROR(NULL,"nbt_le - index %d out of bounds", index);
    // LH_ERROR commented out because it was producing too many error messages in minemap

    return elem->v.comp[index];
}


////////////////////////////////////////////////////////////////////////////////

inline
nbte * nbt_make(const char *name, int type) {
    ALLOC(nbte,el);
    el->type = type;
    el->name = name ? strdup(name) : NULL;
    el->count = 1;
    return el;
}

nbte * nbt_make_byte   (const char *name, int8_t  b) {
    nbte *el = nbt_make(name, NBT_TAG_BYTE);
    el->v.b = b;
    return el;
}

nbte * nbt_make_short  (const char *name, int16_t s) {
    nbte *el = nbt_make(name, NBT_TAG_SHORT);
    el->v.s = s;
    return el;
}

nbte * nbt_make_int    (const char *name, int32_t i) {
    nbte *el = nbt_make(name, NBT_TAG_INT);
    el->v.i = i;
    return el;
}

nbte * nbt_make_long   (const char *name, int64_t l) {
    nbte *el = nbt_make(name, NBT_TAG_LONG);
    el->v.l = l;
    return el;
}

nbte * nbt_make_float  (const char *name, float   f) {
    nbte *el = nbt_make(name, NBT_TAG_FLOAT);
    el->v.f = f;
    return el;
}

nbte * nbt_make_double (const char *name, double  d) {
    nbte *el = nbt_make(name, NBT_TAG_DOUBLE);
    el->v.d = d;
    return el;
}

nbte * nbt_make_ba     (const char *name, int8_t *ba, int num) {
    nbte *el = nbt_make(name, NBT_TAG_BYTE_ARRAY);
    el->count = num;
    ALLOCBE(el->v.ba, num);
    memcpy(el->v.ba, ba, num);
    return el;
}

nbte * nbt_make_ia     (const char *name, int32_t *ia, int num) {
    nbte *el = nbt_make(name, NBT_TAG_INT_ARRAY);
    el->count = num;
    ALLOCBE(el->v.ia, num);
    memcpy(el->v.ia, ia, num*sizeof(int32_t));
    return el;
}

nbte * nbt_make_str    (const char *name, char    *str) {
    nbte *el = nbt_make(name, NBT_TAG_STRING);
    el->count = strlen(str);
    el->v.str   = strdup(str);
    return el;
}

nbte * nbt_make_list   (const char *name) {
    nbte *el = nbt_make(name, NBT_TAG_LIST);
    el->count = 0;
    el->v.list = NULL;
}

nbte * nbt_make_comp   (const char *name) {
    nbte *el = nbt_make(name, NBT_TAG_COMPOUND);
    el->count = 0;
    el->v.comp = NULL;
}

int nbt_add(nbte *list, nbte *el) {
    if (list->type != NBT_TAG_LIST && list->type != NBT_TAG_COMPOUND)
        LH_ERROR(-1, "nbt_add : must be of type list or compound");

    ARRAY_ADD(list->v.list, list->count, 1);
    list->v.list[list->count-1] = el;
    return list->count-1;
}

////////////////////////////////////////////////////////////////////////////////

//FIXME: check for limit
uint8_t * nbt_write(uint8_t *p, uint8_t *limit, nbte *el, int with_header) {
    if (with_header) {
        write_char(p, el->type);
        if (el->name) {
            write_short(p, strlen(el->name));
            p += sprintf(p, "%s", el->name);
        }
        else {
            write_short(p, 0);
        }
    }

    switch (el->type) {
    case NBT_TAG_BYTE:
        write_char(p, el->v.b);
        break;
    case NBT_TAG_SHORT:
        write_short(p, el->v.s);
        break;
    case NBT_TAG_INT:
        write_int(p, el->v.i);
        break;
    case NBT_TAG_LONG:
        write_long(p, el->v.l);
        break;
    case NBT_TAG_FLOAT:
        write_float(p, el->v.f);
        break;
    case NBT_TAG_DOUBLE:
        write_double(p, el->v.d);
        break;
    case NBT_TAG_BYTE_ARRAY:
        write_int(p, el->count);
        memcpy(p, el->v.ba, el->count);
        p += el->count;
        break;
    case NBT_TAG_INT_ARRAY: {
        write_int(p, el->count);
        int i;
        for (i=0; i<el->count; i++) {
            write_int(p, el->v.ia[i]);
        }
        break;
    }
    case NBT_TAG_STRING:
        write_short(p, el->count);
        memcpy(p, el->v.str, el->count);
        p += el->count;
        break;
    case NBT_TAG_COMPOUND: {
        int i;
        for(i=0; i<el->count; i++)
            p = nbt_write(p, limit, el->v.comp[i], 1);
        write_char(p,0);
        break;
    }
    case NBT_TAG_LIST: {
        int i;
        char type = NBT_TAG_BYTE;
        if (el->count > 0)
            type = el->v.list[0]->type;

        write_char(p, type);
        write_int(p, el->count);

        for(i=0; i<el->count; i++)
            p = nbt_write(p, limit, el->v.list[i], 0);
        break;
    }
        
    }
    return p;
}
