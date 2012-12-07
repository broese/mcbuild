#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "anvil.h"
#include "nbt.h"
#include "draw.h"
#include "lh_image.h"
#include "lh_buffers.h"
#include "lh_files.h"

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

int BIOMES[] = {
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

int BLOCKS[256] = {
    0x000000, 0x808080, 0x00c000, 0x404000,    0x707070, 0x808020, 0x40c020, 0x101010, // 0
    0x0000c0, 0x0000c0, 0xc08020, 0xc08020,    0xc0c020, 0xc0a0b0, 0xffff00, 0xff0000,
    0x00ffff, 0x00ffff, 0x00a000, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 1
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 2
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 3
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 

    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 4
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0xe0e0e0, 0xc0c0ff, 
    0xe0e0e0, 0x006000, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 5
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 6
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 7
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 

    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 8
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // 9
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // A
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // B
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 

    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // C
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // D
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // E
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, // F
    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff,    0x00ffff, 0x00ffff, 0x00ffff, 0x00ffff, 
};

////////////////////////////////////////////////////////////////////////////////
// Slime chunk computation

#if 0
#define MAPSEED 1517717513523274421LL

long long Random_new(long long seed) {
    long long r = (seed ^ 0x5deece66dLL) & ((1LL<<48)-1);
    return r;
}

int Random_next(long long *seed, int bits) {
    *seed = (*seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
    return (int)(*seed >> (48 - bits));
}

int Random_nextInt(long long *seed, int n) {
    if ((n & -n) == n)  // i.e., n is a power of 2
        return (int)((n * (long long)Random_next(seed,31)) >> 31);

    int bits, val;
    do {
        bits = Random_next(seed,31);
        val = bits % n;
    } while(bits - val + (n-1) < 0);
    return val;
}

int is_slime(int X, int Y) {
    long long r = Random_new(
        MAPSEED +
        (long long) (X*X*0x4c1906) + 
        (long long) (X*0x5ac0db) + 
        (long long) (Y*Y) * 0x4307a7LL + 
        (long long) (Y*0x5f24f) ^ 0x3ad8025f
    );
        
    return Random_nextInt(&r,10) == 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////

void draw_region(drawstate *ds, const char *path) {
    mcregion *region = load_region(path);
    
    int idx;
    for (idx=0; idx<1024; idx++) {
        ssize_t len;
        unsigned char * data = get_compound_data(region,idx,&len);
        if (!data) continue; //{ printf("Skipping column %d\n",idx); continue; }
        //printf("Parsing column %d (%d bytes)\n",idx,len);

        unsigned char * ptr = data;
        nbte *comp = nbt_parse(&ptr);

        nbte *level = nbt_ce(comp,"Level");
        nbte *xPos  = nbt_ce(level,"xPos");
        nbte *zPos  = nbt_ce(level,"zPos");
        int X=xPos->v.i, Z=zPos->v.i;

#if 0
        nbte *entities = nbt_ce(level,"Entities");
        if (entities) {
            printf("\n\n\n[%03d,%03d]\n",X,Z);
            nbt_dump(entities, 0);
        }

#endif        


#if 0
        nbte *biomes = nbt_ce(level,"Biomes");
        draw_biomes(ds, X, Z, biomes->v.ba);
#endif

#if 1
        nbte *sect = nbt_ce(level,"Sections");
        uint8_t *cubes[16]; CLEAR(cubes);

        int i;
        for(i=0; i<16; i++) {
            nbte *c = nbt_le(sect, i);
            if (!c) continue;
            nbte *Y = nbt_ce(c, "Y");
            int y = Y->v.i;
            nbte *B = nbt_ce(c, "Blocks");
            cubes[y] = B->v.ba;
        }

        draw_topmap(ds, X, Z, cubes);
#endif

        nbt_free(comp);

        free(data);
    }

    free_region(region);
}

int main(int ac, char **av) {

    drawstate *ds = create_drawing();

    char **arg = av+1;
    while (*arg) {

#if 1
        // topmap from the anvil region files
        draw_region(ds, *arg);
#endif

#if 0
        // topmap from the netmine chunk files
        ssize_t size;
        uint8_t *buf = read_file(*arg,&size);
        if (buf) {
            uint8_t *p = buf;
            int n = read_int_le(p);
            int X = read_int_le(p);
            int Z = read_int_le(p);
            printf("Opened chunk %s (%d,%d) %d cubes\n",*arg,X,Z,n);

            uint8_t *cubes[16]; CLEAR(cubes);
            int l;
            for (l=0; l<n; l++) {
                cubes[l] = p+4096*l;
            }

            draw_topmap(ds, X, Z, cubes);
        }
        free(buf);
#endif

#if 0
        // isometric map from 1 anvil region
        lhimage * img = allocate_image(ISIZE_WD(512,512,256)+64,ISIZE_HG(512,512,256)+64);
        mcregion *region = load_region(*arg);

        printf("Origin : %d %d\n", ORIGIN_X(512,512,256)+32, ORIGIN_Y(512,512,256)+32);

        draw_isometric (img, region, ORIGIN_X(512,512,256)+32, ORIGIN_Y(512,512,256)+32);
        
        export_png_file(img, "iso.png");
        return 0;
#endif

        arg++;
    }

    draw_coordmesh(ds, 500);

    export_png_file(ds->img, "map.png");
    printf("Drawstate offset is X:%d Z:%d\n",ds->offX,ds->offZ); 

    return 0;
}

