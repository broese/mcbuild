#pragma once

#include <stdint.h>

/*

Common tag format:

size  name      comment
1     type      tag type as specified in the table below
--------------------------------------------------------------------------------
2     strlen    length of the name string      These two fields are available in
N     name      name of the tag                all tags except Tag_End
--------------------------------------------------------------------------------
M     payload   size and format depends on tag type



Tags

0 	TAG_End 	0 	This tag serves no purpose but to signify the end of an open TAG_Compound. In most libraries, this type is abstracted away and never seen. TAG_End is not named.
1 	TAG_Byte 	1 	A single signed byte
2 	TAG_Short 	2 	A single signed short
3 	TAG_Int 	4 	A single signed integer
4 	TAG_Long 	8 	A single signed long (typically long long in C/C++)
5 	TAG_Float 	4 	A single IEEE-754 single-precision floating point number
6 	TAG_Double 	8 	A single IEEE-754 double-precision floating point number
7 	TAG_Byte_Array 	... 	A length-prefixed array of signed bytes. The prefix is a signed integer (thus 4 bytes)
8 	TAG_String 	... 	A length-prefixed UTF-8 string. The prefix is an unsigned short (thus 2 bytes)
9 	TAG_List 	... 	A list of nameless tags, all of the same type. The list is prefixed with the Type ID of the items it contains (thus 1 byte), and the length of the list as a signed integer (a further 4 bytes).
10 	TAG_Compound 	... 	Effectively a list of a named tags
11 	TAG_Int_Array 	... 	A length-prefixed array of signed integers. The prefix is a signed integer (thus 4 bytes) and indicates the number of 4 byte integers. 

*/

#define NBT_TAG_END         0
#define NBT_TAG_BYTE        1
#define NBT_TAG_SHORT       2
#define NBT_TAG_INT         3
#define NBT_TAG_LONG        4
#define NBT_TAG_FLOAT       5
#define NBT_TAG_DOUBLE      6
#define NBT_TAG_BYTE_ARRAY  7
#define NBT_TAG_STRING      8
#define NBT_TAG_LIST        9
#define NBT_TAG_COMPOUND   10
#define NBT_TAG_INT_ARRAY  11

//typedef struct nbtl nbtl;
typedef struct nbte nbte;

#if 0
struct nbtl {
    int   n;
    nbte *e;  
};
#endif

struct nbte {
    int      type;      // type of element
    char    *name;      // element name - allocated, must be freed!
    int      count;     // number of elements in the parsed object:
                        // 1 for all elemental types
                        // number of elements in the array for ba, ia
                        // string length for str
                        // number of elements in the compound or list

    union {
        int8_t  b;      // NBT_TAG_BYTE
        int16_t s;      // NBT_TAG_SHORT
        int32_t i;      // NBT_TAG_INT
        int64_t l;      // NBT_TAG_LONG
        float   f;      // NBT_TAG_FLOAT
        double  d;      // NBT_TAG_DOUBLE

                        // all following pointers are allocated and must be freed on destruction!
        int8_t *ba;     // NBT_TAG_BYTE_ARRAY
        int32_t*ia;     // NBT_TAG_INT_ARRAY

        char   *str;    // NBT_TAG_STRING

        nbte   *list;   // NBT_TAG_LIST
        nbte   *comp;   // NBT_TAG_COMPOUND
    } v;
};

nbte *nbt_parse(unsigned char **ptr);
void nbt_dump(nbte *elem, int indent);

nbte * nbt_ce(nbte *elem, const char *name);
nbte * nbt_le(nbte *elem, int index);

nbte * nbt_make_byte   (const char *name, int8_t  b);
nbte * nbt_make_short  (const char *name, int16_t s);
nbte * nbt_make_int    (const char *name, int32_t i);
nbte * nbt_make_long   (const char *name, int64_t l);
nbte * nbt_make_float  (const char *name, float   f);
nbte * nbt_make_double (const char *name, double  d);

nbte * nbt_make_ba     (const char *name, int8_t  *ba, int num);
nbte * nbt_make_ia     (const char *name, int32_t *ia, int num);
nbte * nbt_make_str    (const char *name, char    *str);

nbte * nbt_make_list   (const char *name);
nbte * nbt_make_comp   (const char *name);

int nbt_add(nbte *list, nbte *el);
uint8_t * nbt_write(uint8_t *p, uint8_t *limit, nbte *el, int with_header);
