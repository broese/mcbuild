#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>

#include <lh_image.h>

typedef struct {
    uint8_t  r,g,b;
    uint16_t bid;
    uint8_t  meta;
    int      support;
} mapmat_t;

mapmat_t COLORS[] = {
    { 127, 178,  56, 0x02,  0, 0 },  // Grass
    { 247, 233, 163, 0x2c,  1, 2 },  // Sandstone slab
    //{ 167, 167, 167, 0x64,  0, 0 },  // Mushroom block
    { 255,   0,   0, 0x98,  0, 0 },  // Redstone block
    { 160, 160, 255, 0xae,  0, 0 },  // Packed ice
    { 167, 167, 167, 0x94,  0, 1 },  // Heavy (iron) pressure plate
    {   0, 124,   0, 0x12,  0, 0 },  // Oak leaves
    { 255, 255, 255, 0xab,  0, 1 },  // White carpet
    { 164, 168, 184, 0x52,  0, 0 },  // Clay block
    { 183, 106,  47, 0x7e,  3, 0 },  // Jungle wooden slab
    { 112, 112, 112, 0x2c,  3, 2 },  // Cobblestone slab
    {  64,  64, 255, 0x09,  0, 1 },  // Water
    { 104,  83,  50, 0x7e,  0, 2 },  // Oak wooden slab
    { 255, 252, 245, 0x01,  3, 0 },  // Diorite
    { 216, 127,  51, 0xab,  1, 1 },  // Orange carpet
    { 178,  76, 216, 0xab,  2, 1 },  // Magenta carpet
    { 102, 153, 216, 0xab,  3, 1 },  // L-blue carpet
    { 229, 229,  51, 0xab,  4, 1 },  // Yellow carpet
    { 127, 204,  25, 0xab,  5, 1 },  // Lime carpet
    { 242, 127, 165, 0xab,  6, 1 },  // Pink carpet
    {  76,  76,  76, 0xab,  7, 1 },  // Gray carpet
    { 153, 153, 153, 0xab,  8, 1 },  // L-gray carpet
    {  76, 127, 153, 0xab,  9, 1 },  // Cyan carpet
    { 127,  63, 178, 0xab, 10, 1 },  // Purple carpet
    {  51,  76, 178, 0xab, 11, 1 },  // Blue carpet
    { 102,  76,  51, 0xab, 12, 1 },  // Brown carpet
    { 102, 127,  51, 0xab, 13, 1 },  // Green carpet
    { 153,  51,  51, 0xab, 14, 1 },  // Red carpet
    {  25,  25,  25, 0xab, 15, 1 },  // Black carpet
    { 250, 238,  77, 0x93,  0, 1 },  // Light (gold) pressure plate
    {  92, 219, 213, 0xa8,  1, 0 },  // Prismarine Bricks
    {  74, 128, 255, 0x16,  0, 0 },  // Lapis block
    {   0, 217,  58, 0x85,  0, 0 },  // Emerald block
    {  21,  20,  31, 0x7e,  1, 2 },  // Spruce wooden slab
    { 112,   2,   0, 0x57,  0, 0 },  // Netherrack
};

float color_diff(uint8_t r, uint8_t g, uint8_t b, int idx, int subidx) {
    mapmat_t cd = COLORS[idx];

    float f;
    switch(subidx) {
        case 0: f=180.0/255.0; break;
        case 1: f=220.0/255.0; break;
        case 2: f=1.0; break;
        case 3: f=135.0/255.0; break;
    }
    float dr = (float)COLORS[idx].r*f-r;
    float dg = (float)COLORS[idx].g*f-g;
    float db = (float)COLORS[idx].b*f-b;
    return sqrtf(dr*dr+dg*dg+db*db);
}

int pick_nearest_color(uint8_t r, uint8_t g, uint8_t b, int *level, int flat) {
    int i;

    float diff = 1000;
    int idx = -1; *level=0;

    for(i=0; COLORS[i].bid; i++) {
        float d_level, d_low, d_high;
        d_level = color_diff(r,g,b,i,1);
        if (!flat) {
            d_low   = color_diff(r,g,b,i,0);
            d_high  = color_diff(r,g,b,i,2);
        }

        if (d_level<diff) { idx=i; *level=0; diff=d_level; }
        if (!flat) {
            if (d_low<diff)   { idx=i; *level=-1; diff=d_low;   }
            if (d_high<diff)  { idx=i; *level=1; diff=d_high;  }
        }
    }

    return idx;
}

void process_column(lhimage *img, int col, int flat, FILE *out) {
    int i;
    int clevel = 0, level, prev=0, prevsupport=0;

    for(i=0; i<img->width; i++) {
        uint32_t color = IMGDOT(img, col, i);
        int idx = pick_nearest_color((color>>16)&0xff, (color>>8)&0xff, color&0xff, &level, flat);
        int needsupport = COLORS[idx].support;

        if ((level<0 && prev>0) || (level>0 && prev<0)) {
            clevel=0;
        }
        else {
            clevel += level;
        }
        fprintf(out,"%d,%d,%d,%d,%d\n", col, clevel, i-(img->height-1), COLORS[idx].bid, COLORS[idx].meta);

        // depending on block placement we may need to add some supports
        switch (level) {
            case 0:
                if (needsupport==1) {
                    // support for ourselves
                    fprintf(out,"%d,%d,%d,%d,%d\n", col, clevel-1, i-(img->height-1), 3, 0);
                    // support for the previous block
                    if (!prevsupport)
                        fprintf(out,"%d,%d,%d,%d,%d\n", col, prev-1, i-(img->height-1)-1, 3, 0);
                    prevsupport = 1;
                }
                break;
            case -1: // current block is lower than the previous one
                // in any case we need a support for the previous one, so we can connect
                if (!prevsupport)
                    fprintf(out,"%d,%d,%d,%d,%d\n", col, prev-1, i-(img->height-1)-1, 3, 0);
                if (needsupport==1) {
                    // support for ourselves
                    fprintf(out,"%d,%d,%d,%d,%d\n", col, clevel-1, i-(img->height-1), 3, 0);
                    // second support for the previous block - so we can connect our support to it
                    fprintf(out,"%d,%d,%d,%d,%d\n", col, prev-2, i-(img->height-1)-1, 3, 0);
                    prevsupport = 1;
                }
                break;
            case 1: // current block is higher than the previous one
                // support for ourselves in any case
                fprintf(out,"%d,%d,%d,%d,%d\n", col, clevel-1, i-(img->height-1), 3, 0);
                prevsupport = 1;
                break;
        }
        prev = clevel;
    }
}

int main(int ac, char **av) {
    if (!av[1] || !av[2]) {
        printf("Usage: %s <image.png> <output.csv>\n", av[0]);
        exit(1);
    }

    lhimage *img = import_png_file(av[1]);
    if (!img) {
        printf("Failed to load PNG image from %s\n", av[1]);
        exit(2);
    }

    int col;
    FILE *out = fopen(av[2], "w");
    if (!out) {
        printf("Failed to open file %s for writing: %s\n", av[2], strerror(errno));
        exit(2);
    }
    fprintf(out, "x,y,z,bid,meta\n");

    for(col=0; col<img->width; col++)
        process_column(img, col, 1, out);

    fclose(out);
}
