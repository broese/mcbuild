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

0 	TAG_End         0  This tag serves no purpose but to signify the end of an
                       open TAG_Compound. In most libraries, this type is
                       abstracted away and never seen. TAG_End is not named.
1 	TAG_Byte        1  A single signed byte
2 	TAG_Short       2  A single signed short
3 	TAG_Int         4  A single signed integer
4 	TAG_Long        8  A single signed long (typically long long in C/C++)
5 	TAG_Float       4  A single IEEE-754 single-precision floating point number
6 	TAG_Double      8  A single IEEE-754 double-precision floating point number
7 	TAG_Byte_Array ... A length-prefixed array of signed bytes. The prefix is
                       a signed integer (thus 4 bytes)
8 	TAG_String     ... A length-prefixed UTF-8 string. The prefix is an
                       unsigned short (thus 2 bytes)
9 	TAG_List       ... A list of nameless tags, all of the same type. The list
                       is prefixed with the Type ID of the items it contains
                       (thus 1 byte), and the length of the list as a signed
                       integer (a further 4 bytes).
10 	TAG_Compound   ... Effectively a list of a named tags
11 	TAG_Int_Array  ... A length-prefixed array of signed integers. The prefix
                       is a signed integer (thus 4 bytes) and indicates the
                       number of 4 byte integers.

*/

#define NBT_END         0
#define NBT_BYTE        1
#define NBT_SHORT       2
#define NBT_INT         3
#define NBT_LONG        4
#define NBT_FLOAT       5
#define NBT_DOUBLE      6
#define NBT_BYTE_ARRAY  7
#define NBT_STRING      8
#define NBT_LIST        9
#define NBT_COMPOUND   10
#define NBT_INT_ARRAY  11

//typedef struct nbt_t nbt_t;

typedef struct nbt_t {
    int      type;      // type of element
    char    *name;      // element name - allocated, must be freed!
    int      count;     // number of elements in the parsed object:
                        // 1 for all elemental types
                        // number of elements in the array for ba, ia
                        // string length for str (the allocated buffer for the string has a 1 byte more for the terminator)
                        // number of elements in the compound or list

    union {
        int8_t  b;      // NBT_BYTE
        int16_t s;      // NBT_SHORT
        int32_t i;      // NBT_INT
        int64_t l;      // NBT_LONG
        float   f;      // NBT_FLOAT
        double  d;      // NBT_DOUBLE

                        // all following pointers are allocated and must be freed on destruction!
        int8_t *ba;     // NBT_BYTE_ARRAY
        int32_t*ia;     // NBT_INT_ARRAY

        char   *st;     // NBT_STRING

        struct nbt_t   **li;    // NBT_LIST
        struct nbt_t   **co;    // NBT_COMPOUND
    };
} nbt_t;

nbt_t * nbt_parse(uint8_t **p);
void    nbt_write(uint8_t **w, nbt_t *nbt);
nbt_t * nbt_clone(nbt_t *nbt);
void    nbt_dump(nbt_t *nbt);
void    nbt_free(nbt_t *nbt);
