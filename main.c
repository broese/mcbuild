#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "anvil.h"
#include "nbt.h"
#include "lh_image.h"

const char * program_name;
int o_level = 256;
int o_help = 0;
const char *o_path = NULL;

// parses argv/argc, 
// sets config variables :
//   o_blocksize, o_digest
int parse_args(int ac, char ** av) {
    int opt,error=0,i;

    program_name = av[0];

    // parse options
    while ( (opt = getopt(ac,av,"l:h")) != -1 ) {
        //printf("opt=%d(%c) optarg=%s optind=%d opterr=%d optopt=%d\n",opt,(char)opt,optarg,optind,opterr,optopt);

        switch (opt) {
            case 'l' : {
                if (sscanf(optarg, "%d", &o_level) != 1) {
                    fprintf(stderr,"%s : cannot parse level %s\n",program_name,optarg);
                    error++;
                }
                break;
            }
            case 'h' : {
                o_help = 1;
                break;
            }
            case '?' : {
                error++;
                break;
            }
        }
    }

    // fetch paths
    o_path = av[optind];
    if ( o_path == NULL ) {
        fprintf(stderr,"%s: missing parameter : file/directory path\n", program_name);
        error++;
    }
    return error;
}

void print_usage() {
    fprintf(stderr,
            "Mine Mapper, Minecraft mapping tool\n\n"

            "Usage: %s [options] path\n\n"


            "Options:\n"
            "  -l <num>          cut-off level\n"
            "  -h                print this help message and quit\n"
            ,program_name);
}

static int BIOMES[] = {
    0x000080, //Ocean
    0x00ff00, //Plains
    0xffff40, //Desert
    0x808040, //Extreme Hills
    0x00c000, //Forest
    0x008000, //Taiga
    0x40c000, //Swampland
    0x4040ff, //River
    0x800000, //Hell
    0x8080ff, //Sky
    0xc0c0ff, //Frozen Ocean
    0xe0e0ff, //Frozen River
    0xf0fff0, //Ice Plains
    0xf0f0ff, //Ice Mountains
    0xff8080, //Mushroom Island
    0xffc080, //Mushroom Island Shore
    0xffff80, //Beach
    0xffff00, //Desert Hills
    0x40c040, //Forest Hills
    0x308030, //Taiga
    0xff00ff, //Extreme Hills Edge
    0x408000, //Jungle
    0x60a020, //Jungle Hills
};

static int BLOCKS[256] = {
    0x000000,
    0x808080,
    0x00c000,
    0x404000,
    0x707070,
    0x808020,
    0x40c020,
    0x101010,
    0x0000c0,
    0x0000c0,
    0xc08020,
    0xc08020,
    0xc0c020,
    0xc0a0b0,
    0xffff00,
    0xff0000,
};

void draw_biomes(lhimage *img, nbte *level) {
    nbte *xPos = nbt_ce(level,"xPos");
    nbte *zPos = nbt_ce(level,"zPos");
    nbte *biomes = nbt_ce(level,"Biomes");
    int X=xPos->v.i+32,Z=zPos->v.i+32;
    unsigned char *b = biomes->v.ba;

    int x,y;
    for(x=0; x<16; x++) {
        for(y=0; y<16; y++) {
            int spos = x+X*16+(y+Z*16)*img->width;
            int bpos = x+y*16;
            int color = (b[bpos]<=22) ? BIOMES[b[bpos]] : 0x00ffff;
            img->data[spos] = color;
        }
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

            if (b[i]==8 || b[i]==9) img->data[spos] |= (vvv<<16);
            if (b[i]==10 || b[i]==11) img->data[spos] |= (vvv<<8);
            if (b[i]==4) img->data[spos] |= vvv;
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
                for(z=0; z<4096; z+=256) {
                    int bpos = z+(y<<4)+x;
                    if (b[bpos]!=0)
                        img->data[spos] = BLOCKS[b[bpos]];
                }
            }
        }
    }
#endif


}

void draw_region(lhimage *img, const char *path) {
    mcregion *region = load_region(path);
    
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
#if 0
        draw_biomes(img, level);
#else
        draw_topmap(img, level);
#endif

        nbt_free(comp);
        free(comp);

        free(data);
    }

    free_region(region);
}

int main(int ac, char **av) {

    lhimage * img = allocate_image(512,512);

    int i=1;
    while(av[i]) {
        draw_region(img, av[i]);
        i++;
    }


    export_png_file(img, "biomes.png");

    

    return 0;
}

