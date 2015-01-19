#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <lh_arr.h>
#include <lh_files.h>
#include <lh_image.h>
#include <lh_debug.h>

#include "ids.h"

typedef struct {
    uint64_t ts;        // last timestamp we attempted to place this block
    int x,y,z;          // block coordinates
    int bid;            // block ID to place
    int meta;           // meta-value for the subtype
    uint32_t place[6];  // block placement methods in 6 available directions
                        // bits 31..28 : enabled
                        // bits 27..24 : possible (in reach)
                        // bits 23..16 : cx
                        // bits 15..8  : cy
                        // bits  7..0  : cz
} bblock;

lh_arr_declare_i(bblock, blocks);

typedef struct {
    uint32_t color;     // RGB color
    uint8_t  bid;       // block ID
    int      meta;      // block metatype
} material;

material mat[] = {
    { 0xDDDDDD, 35, 0 },
  	{ 0xDB7D3E, 35, 1 },
 	{ 0xB350BC, 35, 2 },
 	{ 0x6B8AC9, 35, 3 },
 	{ 0xB1A627, 35, 4 },
 	{ 0x41AE38, 35, 5 },
 	{ 0xD08499, 35, 6 },
 	{ 0x404040, 35, 7 },
 	{ 0x9AA1A1, 35, 8 },
 	{ 0x2E6E89, 35, 9 },
 	{ 0x7E3DB5, 35, 10 },
 	{ 0x2E388D, 35, 11 },
 	{ 0x4F321F, 35, 12 },
 	{ 0x35461B, 35, 13 },
 	{ 0x963430, 35, 14 },
 	{ 0x191616, 35, 15 },
    { 0x000000, 0, 0 },
};    

#define SCAFF_NETHER 0

#if SCAFF_NETHER
#define SCAFF_BID  0x57 // Netherrack
#define SCAFF_META 0x00
#else
#define SCAFF_BID  0x03 // Dirt
#define SCAFF_META 0x00
#endif

#define DIR_UP      0
#define DIR_DOWN    1
#define DIR_SOUTH   2
#define DIR_NORTH   3
#define DIR_EAST    4
#define DIR_WEST    5

char * get_item_name(char *buf, int id, int dmg) {
    if (id == 0xffff) {
        sprintf(buf, "-");
        return buf;
    }

    if (ITEMS[id].name) {
        int pos = sprintf(buf, "%s", ITEMS[id].name);
        if ((ITEMS[id].flags&I_MTYPE) && ITEMS[id].mname[dmg])
            sprintf(buf+pos, " (%s)",ITEMS[id].mname[dmg]);
        //printf("id=%d dmg=%d name=%s\n",id,dmg,buf);
        return buf;
    }
    return NULL;
}

int find_mat(uint32_t color) {
    color &= 0x00ffffff;

    int bi=0, bdiff=0x10000;

    int i;
    for (i=0; mat[i].bid; i++) {
        uint32_t mc = mat[i].color;
        int rd = ((color>>16)&0xff)-((mc>>16)&0xff);
        int gd = ((color>>8)&0xff) -((mc>>8)&0xff);
        int bd = (color&0xff)      -(mc&0xff);
        int diff = rd*rd+gd*gd+bd*bd;
        if (diff < bdiff) { bi = i; bdiff = diff; }
    }

    return bi;
}

#define NSTACKS(x) (((x)>>6)+(((x)&0x3f)>0))

void add_block(int x, int y, int z, int bid, int meta, int *req) {
    bblock * bb = lh_arr_new(GAR(blocks));

    bb->ts = 0;
    bb->x = x;
    bb->y = y;
    bb->z = z;

    bb->bid = bid;
    bb->meta = meta;

    bb->place[DIR_UP]    = 0x10080008;
    bb->place[DIR_DOWN]  = 0x10081008;
    bb->place[DIR_SOUTH] = 0x10080800;
    bb->place[DIR_NORTH] = 0x10080810;
    bb->place[DIR_EAST]  = 0x10000808;
    bb->place[DIR_WEST]  = 0x10100808;

    int bm = (meta<<8)+bid;
    req[bm]++;
}

int add_picture(lhimage *img) {
    // this array is used to track the total amount of required material
    lh_create_num(int, req, 4096);

    //TODO: reduce picture to cut out all transparent borders

    int x,y;
    // building the picture plane
    for (y=0; y<img->height; y++) {
        int yy=img->height-y-1;
        uint32_t *row = img->data+yy*img->stride;

        for (x=0; x<img->width; x++) {
            uint32_t pixel = row[x];
            if (pixel&0xff000000) continue; // skip transparent pixels

            int mi = find_mat(pixel);
            add_block(0,y,x,mat[mi].bid,mat[mi].meta,req);
        }
    }

    // building scaffolding
    for (y=3; y<img->height; y+=4) {
        for (x=0; x<img->width; x++) {
            add_block(-2,y,x,SCAFF_BID,SCAFF_META,req);
        }

        // staircase
        add_block(-3,y,0,SCAFF_BID,SCAFF_META,req);
        add_block(-3,y+1,1,SCAFF_BID,SCAFF_META,req);
        add_block(-3,y+2,2,SCAFF_BID,SCAFF_META,req);
        add_block(-3,y+3,3,SCAFF_BID,SCAFF_META,req);
        add_block(-3,y+4,4,SCAFF_BID,SCAFF_META,req);

        // supports for the staircase
        add_block(-4,y,1,SCAFF_BID,SCAFF_META,req);
        add_block(-4,y+1,1,SCAFF_BID,SCAFF_META,req);
        add_block(-4,y+1,2,SCAFF_BID,SCAFF_META,req);
        add_block(-4,y+2,2,SCAFF_BID,SCAFF_META,req);
        add_block(-4,y+2,3,SCAFF_BID,SCAFF_META,req);
        add_block(-4,y+3,3,SCAFF_BID,SCAFF_META,req);
    }


    int i;
    for (i=0; i<4096; i++) {
        if (req[i] > 0) {
            char buf[4096];
            printf("%5d (%3d) %s\n",req[i],NSTACKS(req[i]),get_item_name(buf, i&0xff, (i>>8)));
        }
    }
}

int export_brec(const char *fname) {
    ssize_t wlen = lh_save(fname, (uint8_t *)P(blocks), C(blocks)*sizeof(bblock));
    return (wlen>=0)?1:0;
}

int main(int ac, char **av) {
    const char * iname = av[1];
    const char * oname = av[2];
    if (!iname || !oname) LH_ERROR(1, "Usage: %s <input.png> <output.brec>\n",av[0]);

    lhimage *img = import_png_file(iname);
    if (!img) LH_ERROR(1, "Failed to open %s as PNG\n",iname);

    add_picture(img);

    return export_brec(oname);
}
