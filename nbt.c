#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt.h"
#include "lh_debug.h"

char nbt_parsetag(unsigned char *buf, char *name, unsigned char **payload) {
    char type = (char) *buf++;
    if (type < NBT_TAG_END || type > NBT_TAG_INT_ARRAY)
        LH_ERROR(-1,"Unknown tag type %d at %p\n",type,buf-1);

    if (type != NBT_TAG_END) {
        // read the name
        short len = (*buf++)<<8;
        len += *buf++;

        while(len--) *name++ = *buf++;
    }
    *name++ = 0;

    *payload = buf;
    return type;
}

int8_t nbt_read_byte(unsigned char *buf) {
    return (int8_t)*buf;
}

int16_t nbt_read_short(unsigned char *buf) {
    int16_t val = *buf++;
    val<<=8; val += *buf++;
    return val;
}

int32_t nbt_read_int(unsigned char *buf) {
    int32_t val = *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    return val;
}

int64_t nbt_read_long(unsigned char *buf) {
    int64_t val = *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    val<<=8; val += *buf++;
    return val;
}

float nbt_read_float(unsigned char *buf) {
    float val;
    memcpy(&val,buf,sizeof(val));
    return val;
}

float nbt_read_double(unsigned char *buf) {
    double val;
    memcpy(&val,buf,sizeof(val));
    return val;
}


