#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include <lh_files.h>
#include <lh_arr.h>
#include <lh_image.h>

#define SEPINT 300
#define WD 1024
#define HG 1024
#define OFFX (WD/2)
#define OFFZ (HG/2)

float img[WD][HG], layer[WD][HG];
int init=0;

void flush_layer() {
    int x,z;

    if (!init) {
        // init the image for the first time
        for(z=0; z<HG; z++)
            for(x=0; x<WD; x++)
                img[z][x] = 1.0;
        init=1;
    }
    else {
        // merge layer onto image by multiplication
        for(z=0; z<HG; z++)
            for(x=0; x<WD; x++)
                img[z][x] *= layer[x][z];
    }

    // re-initialze layer
    for(z=0; z<HG; z++)
        for(x=0; x<WD; x++)
            layer[z][x] = 1.0;
}

int main(int ac, char **av) {
    if (!av[1]) {
        printf("Usage: %s <thunder.csv>\n",av[0]);
        return 1;
    }

    FILE * fh = fopen(av[1],"r");
    if (!fh) {
        printf("Failed to open %s : %s\n",av[1],strerror(errno));
        return 1;
    }

    int line = 0;
    time_t last_ts = 0;

    while (!feof(fh)) {
        char buf[256];
        if (!fgets(buf, sizeof(buf), fh)) break;
        line++;

        time_t ts;
        int32_t px,pz,rx,rz;
        if (sscanf(buf, "%d,%d,%d,%d,%d", &ts, &px, &pz, &rx, &rz)!=5) {
            printf("Warning, cannot parse line %d\n",line);
            continue;
        }

        if (ts>last_ts+SEPINT) flush_layer();
        last_ts = ts;

        //TODO: draw line on layer
    }
    flush_layer();

    //TODO: convert to image
    //TODO: draw axis and gridlines
    //TODO: export image
}
