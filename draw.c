#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include "lh_buffers.h"
#include "draw.h"

drawstate * create_drawing() {
    lhimage *img = allocate_image(512,512);

    uint32_t size = img->width*img->height;
    int i;
    for (i=0; i<size; i++) img->data[i] = 0x00ffffff;

    ALLOC(drawstate,state);
    state->img = img;
    return state;
}

// ensures that there's space allocated for a chunk X,Z on the drawstate
void update_drawstate(drawstate *ds, int X, int Z) {
    // determine the borders of the current drawstate
    int Xmin = -ds->offX;              // this is the chunk X coord at the western border
    int Zmin = -ds->offZ;              // ... at the northern border
    int Xmax = Xmin+ds->img->width/16; // this is the first chunk X coord that would be past the eastern border
    int Zmax = Zmin+ds->img->height/16; // ... past the southern border

    // check if our chunk is within the borders
    int needresize = 0;
    if (X<Xmin)  { Xmin=X; needresize=1; }
    if (Z<Zmin)  { Zmin=Z; needresize=1; }
    if (X>=Xmax) { Xmax=X+1; needresize=1; }
    if (Z>=Zmax) { Zmax=Z+1; needresize=1; }

    // resize image if needed
    if (needresize) {
        int newwidth  = (Xmax-Xmin)*16;
        int newheight = (Zmax-Zmin)*16;
        int newOffX   = -Xmin;
        int newOffZ   = -Zmin;
        printf("Resizing %dx%d -> %dx%d , %d,%d -> %d,%d, because of %d,%d\n",
               ds->img->width, ds->img->height, newwidth, newheight,
               ds->offX,ds->offZ,newOffX,newOffZ, X,Z);
               
               
        resize_image(ds->img, newwidth, newheight, 
                     (newOffX-ds->offX)*16, (newOffZ-ds->offZ)*16, 0x00ffffff);
        ds->offX = newOffX;
        ds->offZ = newOffZ;
    }
}

void draw_biomes(drawstate *ds, int X, int Z, uint8_t *biomes) {
    update_drawstate(ds, X, Z);

    int Xoff = (X+ds->offX)*16;
    int Zoff = (Z+ds->offZ)*16;

    int x,z;
    for(x=0; x<16; x++) {
        for(z=0; z<16; z++) {
            int spos = x+Xoff+(z+Zoff)*ds->img->width;
            int bpos = x+z*16;
            int color = (biomes[bpos]<=22) ? BIOMES[biomes[bpos]] : 0x00ffff;
            ds->img->data[spos] = color;
        }
    }    
}

void draw_topmap(drawstate *ds, int X, int Z, uint8_t **cubes) {
    update_drawstate(ds, X, Z);
    int Xoff = (X+ds->offX)*16;
    int Zoff = (Z+ds->offZ)*16;

    int x,y,z,l;
    for(l=0; l<16; l++) {
        uint8_t *b = cubes[l];
        if (!b) continue; // skip empty cubes

        for(x=0; x<16; x++) {
            for(y=0; y<16; y++) {
                int spos = x+Xoff+(y+Zoff)*ds->img->width;
                int vvv = 4*((l<<4)); if (vvv>255) vvv=255;
                //ds->img->data[spos] = 0;
                for(z=0; z<4096; z+=256) {
                    int bpos = z+(y<<4)+x;
                    if (b[bpos]!=0) {
#if 1
                        ds->img->data[spos] = BLOCKS[b[bpos]];
#endif

#if 0
                        if (b[bpos]==50 || b[bpos]==58 || b[bpos]==54 || b[bpos]==61) ds->img->data[spos] |= (vvv<<16);
                        //if (is_slime(X,Z)) ds->img->data[spos] |= (1<<8);
                        if (b[bpos]==52 || b[bpos]==48) ds->img->data[spos] |= (vvv<<8);
                        if (b[bpos]==8 || b[bpos]==9) ds->img->data[spos] |= vvv;
#endif

                    }
                }
            }
        }
    }
}

void draw_coordmesh(drawstate *ds, int sz) {
    int x,z;

    // coordinates of the origin point on the drawing, in pixels
    int mx=16*ds->offX, mz=16*ds->offZ;

    // determine X axis extents
    int mx_min=mx;
    while(mx_min>=0) mx_min-=sz;
    while(mx_min<0)  mx_min+=sz;

    for(x=mx_min; x<ds->img->width; x+=sz) {
        for(z=0; z<ds->img->height; z++) {
            int spos = x+z*ds->img->width;
            ds->img->data[spos] ^= 0xffffff;
        }
    }

    // determine Z axis extents
    int mz_min=mz;
    while(mz_min>=0) mz_min-=sz;
    while(mz_min<0)  mz_min+=sz;

    for(z=mz_min; z<ds->img->height; z+=sz) {
        int spos = z*ds->img->width;
        for(x=0; x<ds->img->width; x++) {
            ds->img->data[spos+x] ^= 0xffffff;
        }
    }

}





#if 0
void draw_block(lhimage *img, int X, int Y) {
    int offset = Y*16*img->width+X*16;
    int i,j;
    for(i=0; i<16; i++) {
        for(j=0; j<16; j++) {
            img->data[offset+j] = 0x00008000;
        }
        offset += img->width;
    }
}

void draw_topmap(lhimage *img, nbte *level) {
    //nbt_dump(level, 0); exit(1);

    nbte *xPos = nbt_ce(level,"xPos");
    nbte *zPos = nbt_ce(level,"zPos");
    nbte *sect = nbt_ce(level,"Sections");

    // extract block data
    unsigned char *cubes[16];
    memset(cubes, 0, sizeof(cubes));

    int i;
    for(i=0; i<16; i++) {
        nbte *c = nbt_le(sect, i);
        if (!c) continue;
        nbte *Y = nbt_ce(c, "Y");
        int y = Y->v.i;
        nbte *B = nbt_ce(c, "Blocks");
        cubes[y] = B->v.ba;
    }

#if 0
    printf("%2d %2d ",xPos->v.i,zPos->v.i);
    for(i=0; i<16; i++)
        if (cubes[i]) printf("%x",i);
    printf("\n");
#endif

#if 0
    int X=xPos->v.i,Z=zPos->v.i;
    int x,y,l,z;
    for(l=0; l<16; l++) {
        unsigned char *b = cubes[l];
        if (!b) break; // skip empty cubes
        for(i=0; i<4096; i++) {
            int spos = X*16+(i&0x0f)+(((i&0xf0)>>4)+Z*16)*img->width;
            int vvv = 4*((l<<4)+(i>>8));
            if (vvv>255) vvv=255;

            if (b[i]==50 || b[i]==58 || b[i]==54 || b[i]==61) img->data[spos] |= (vvv<<16);
            if (is_slime(X,Z)) img->data[spos] |= (1<<8);
            if (b[i]==52 || b[i]==48) img->data[spos] |= (vvv<<8);
            if (b[i]==8 || b[i]==9) img->data[spos] |= vvv;
            printf("HERE\n"); exit(1);
        }
    }
#endif


#if 1
    // draw
    int X=xPos->v.i,Z=zPos->v.i;
    int x,y,l,z;
    for(l=0; l<16; l++) {
        unsigned char *b = cubes[l];
        if (!b) break; // skip empty cubes

        for(x=0; x<16; x++) {
            for(y=0; y<16; y++) {
                int spos = x+X*16+(y+Z*16)*img->width;
                for(z=4096-256; z>=0; z-=256) {
                    int bpos = z+(y<<4)+x;
                    if (b[bpos]!=0) {
                        img->data[spos] = BLOCKS[b[bpos]];
                        break;
                    }
                }
            }
        }
    }
#endif


}

#if 0
//FIXME: for now, limitation due to integer calculations - CWD must be equal CHG and divisible by 4
#define CWD 4
#define CHG CWD

#define CPOS_X(x,y,z) ((CWD/2)*((x)-(y)))
#define CPOS_Y(x,y,z) ((CHG/4)*((x)+(y)-2*(z)))
#define ISIZE_WD(x,y,z) (((x)+(y))*(CWD/2))
#define ISIZE_HG(x,y,z) (((x)+(y)+2*(z))*(CHG/4))
#define ORIGIN_X(X,Y,Z) CPOS_X(X,0,0)
#define ORIGIN_Y(X,Y,Z) CPOS_Y(X,0,0)
#endif

void draw_hex4(lhimage *img, int x, int y, uint32_t c) {
    int i;
    uint32_t * lt;

    lt = &IMGDOT(img, x-2, y-1);
    for(i=0; i<5; i++) lt[i] = c;
    lt += img->width;
    for(i=0; i<5; i++) lt[i] = c;
    lt += img->width;
    for(i=0; i<5; i++) lt[i] = c;

    IMGDOT(img, x, y-2) = IMGDOT(img, x, y+2) = c;
}

void draw_topmap_iso(lhimage *img, nbte *level, int ox, int oy) {
    //nbt_dump(level, 0); exit(1);

    nbte *xPos = nbt_ce(level,"xPos");
    nbte *zPos = nbt_ce(level,"zPos");
    nbte *sect = nbt_ce(level,"Sections");

    // extract block data
    unsigned char *cubes[16];
    memset(cubes, 0, sizeof(cubes));

    int i;
    for(i=0; i<16; i++) {
        nbte *c = nbt_le(sect, i);
        if (!c) continue;
        nbte *Y = nbt_ce(c, "Y");
        int y = Y->v.i;
        nbte *B = nbt_ce(c, "Blocks");
        cubes[y] = B->v.ba;
    }

    // draw
    int X=(xPos->v.i)&0x1f,Z=(zPos->v.i)&0x1f;
    int x,y,t,l,z;
    for(l=0; l<16; l++) {
        unsigned char *b = cubes[l];
        if (!b) break; // skip empty cubes

        for(i=0; i<4096; i++) {
            int x=i&0x0f;
            int y=(i>>4)&0x0f;
            int z=i>>8;

            int pos_x = CPOS_X(x+X*16,y+Z*16,z+l*16) + ox;
            int pos_y = CPOS_Y(x+X*16,y+Z*16,z+l*16) + oy;
            
            int c;
            if (!b[i]) continue;

#if 0
            switch (b[i]) {
                case 18: c=0x40c020; break;
                case 17: c=0x808000; break;
                case 2:  c=0x008000; break;
                case 12: c=0xc0c020; break;
                case 8:
                case 9:  c=0x000080; break;
                default: c=0xc0c0c0; break;
            }
#endif
            draw_hex4(img, pos_x, pos_y, BLOCKS[b[i]]);
        }
    }
}

void draw_isometric(lhimage *img, mcregion * region, int ox, int oy) {
    printf("Image size : %d %d\n",img->width,img->height);

    int idx;
    for (idx=0; idx<1024; idx++) {
        ssize_t len;
        unsigned char * data = get_compound_data(region,idx,&len);
        if (!data) continue; //{ printf("Skipping column %d\n",idx); continue; }
        //printf("Parsing column %d (%d bytes)\n",idx,len);

        unsigned char * ptr = data;
        nbte *comp = nbt_parse(&ptr);
        //printf("parsed %d bytes out of %d\n",ptr-data,len);

        nbte *level = nbt_ce(comp,"Level");

        draw_topmap_iso(img, level, ox, oy);

        nbt_free(comp);
        free(comp);

        free(data);
    }


#if 0
    int x,y,z;
    for(z=0; z<16; z++) {
        for(y=0; y<32; y++) {
            for(x=0; x<32; x++) {
                int pos_x = CPOS_X(x*16,y*16,z*16) + ox;
                int pos_y = CPOS_Y(x*16,y*16,z*16) + oy;
                if (pos_x < 0 || pos_x >= img->width) continue; // printf("Incorrect X=%d for (%d,%d,%d)\n",pos_x,x,y,z);
                if (pos_y < 0 || pos_y >= img->height) continue; // printf("Incorrect Y=%d for (%d,%d,%d)\n",pos_y,x,y,z);
                //IMGDOT(img, pos_x, pos_y) = 0xff0000;
                draw_hex4(img, pos_x, pos_y, 0xff0000);
            }
        }
    }
#endif

}

#endif
