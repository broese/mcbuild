#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt.h"
#include "lh_debug.h"
#include "lh_buffers.h"

char nbt_parsetag(unsigned char **ptr, char *name) {
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

