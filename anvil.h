#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    off_t   offset;
    int     sectors;
    ssize_t size;
    int     compr;
} mccolumn;

typedef struct {
    FILE * fd;
    const char * path;
    int size;
    mccolumn * columns;
} mcregion;

mcregion * load_region(const char * path);
void free_region(mcregion *region);
//int export_compound(mcregion *region, int x, int y, const char *oname);
unsigned char * get_compound_data(mcregion *region, int x, int y, ssize_t *len);
void parse_compound(unsigned char *data, ssize_t len);
