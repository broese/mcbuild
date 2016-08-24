/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>

#include <lh_buffers.h>
#include <lh_bytes.h>

#include "mcp_types.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////
// Coordinates

extent_t ps2extent(pivot_t pv, size3_t sz) {
    int32_t tx,ty,tz;

    // calculate the point opposite of the pivot based on specified size
    switch (pv.dir) {
        case DIR_NORTH: tx=pv.pos.x+(sz.x-1); tz=pv.pos.z-(sz.z-1); break;
        case DIR_SOUTH: tx=pv.pos.x-(sz.x-1); tz=pv.pos.z+(sz.z-1); break;
        case DIR_EAST:  tx=pv.pos.x+(sz.z-1); tz=pv.pos.z+(sz.x-1); break;
        case DIR_WEST:  tx=pv.pos.x-(sz.z-1); tz=pv.pos.z-(sz.x-1); break;
        default: assert(0); break;
    }
    ty = pv.pos.y+(sz.y-1);

    // calculate the extent
    extent_t ex;
    ex.min.x = MIN(pv.pos.x,tx);
    ex.min.y = MIN(pv.pos.y,ty);
    ex.min.z = MIN(pv.pos.z,tz);
    ex.max.x = MAX(pv.pos.x,tx);
    ex.max.y = MAX(pv.pos.y,ty);
    ex.max.z = MAX(pv.pos.z,tz);

    return ex;
}

void free_cuboid(cuboid_t c) {
    int y;
    for(y=0; y<c.sa.y; y++)
        if (c.data[y])
            free(c.data[y]);
}

////////////////////////////////////////////////////////////////////////////////
// String

/*
  MCP string format:
  varint length
  char[length] string, no terminator
*/

uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    assert(len<MCP_MAXSTR);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

uint8_t * write_string(uint8_t *w, const char *s) {
    uint32_t len = (uint32_t)strlen(s);
    lh_write_varint(w, len);
    memmove(w, s, len);
    w+=len;
    return w;
}

////////////////////////////////////////////////////////////////////////////////
// Helpers

char limhexbuf[4100];
const char * limhex(uint8_t *data, ssize_t len, ssize_t maxbyte) {
    //assert(len<(sizeof(limhexbuf)-4)/2);
    assert(maxbyte >= 4);

    int i;
    //TODO: implement aaaaaa....bbbbbb - type of printing
    if (len > maxbyte) len = maxbyte;
    for(i=0;i<len;i++)
        sprintf(limhexbuf+i*2,"%02x ",data[i]);
    return limhexbuf;
}

