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

// generic tag parser
// buf     : pointer to the tag start
// name    : buffer to copy the name to. The name will be zero-terminated. Allocate 64k to ensure it fits
// payload : RETURN where the offset to payload is stored (i.e. offset past the name, if any)
char nbt_parsetag(unsigned char **ptr, char *name);
