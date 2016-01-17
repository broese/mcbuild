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

#include <lh_bytes.h>
#include <lh_debug.h>

int main(int ac, char ** av) {
    if (!av[1]) exit(1);

    uint8_t buf[16];
    ssize_t len = hex_import(av[1], buf, sizeof(buf));

    //hexdump(buf,len);
    int32_t v = lh_parse_varint(buf);
    printf("%x %d\n",v,v);
}
