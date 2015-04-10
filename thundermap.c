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
                img[x][z] = 1.0;
        init=1;
    }
    else {
        // merge layer onto image by multiplication
        for(z=0; z<HG; z++)
            for(x=0; x<WD; x++)
                img[x][z] += layer[x][z];
    }

    // re-initialze layer
    for(z=0; z<HG; z++)
        for(x=0; x<WD; x++)
            layer[x][z] = 1.0;
}

void draw_line(int32_t px, int32_t pz, int32_t rx, int32_t rz) {
    //printf("%d,%d -> %d,%d\n",px,pz,rx,rz);

    int32_t dx = rx-px;
    int32_t dz = rz-pz;

    int32_t *m,*n,ms,ns,mv,nv,q;
    if (abs(dx)>=abs(dz)) {
        m = &px;
        n = &pz;
        ms = (dx>0)?1:-1;
        ns = (dz>0)?1:-1;
        mv = abs(dx);
        nv = abs(dz);
    }
    else {
        m = &pz;
        n = &px;
        ms = (dz>0)?1:-1;
        ns = (dx>0)?1:-1;
        mv = abs(dz);
        nv = abs(dx);
    }
    q = mv/2;

    // note - we don't set the first dot, to avoid concentrations at the observation points
    //TODO: maybe skip a certain amount of the first dots?
    while (px!=rx && pz!=rz) {
        *m += ms; // always walk in the major direction
        q-=nv;
        if (q<=0) {
            q = mv;
            *n += ns; // sometimes walk in the minor direction
        }

        if ((px+OFFX)>=WD || (px+OFFX)<0) break;
        if ((pz+OFFZ)>=HG || (pz+OFFZ)<0) break;

        layer[px+OFFX][pz+OFFZ] += 1.0;
    }
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
        if (sscanf(buf, "%ld,%d,%d,%d,%d", &ts, &px, &pz, &rx, &rz)!=5) {
            printf("Warning, cannot parse line %d\n",line);
            continue;
        }

        if (ts>last_ts+SEPINT) flush_layer();
        last_ts = ts;

        draw_line(px>>9,pz>>9,rx>>9,rz>>9);
    }
    flush_layer();

    int x,z;

    // calculate maximum - for the color scaling
    float maxval = 0.0;
    for(z=0; z<HG; z++)
        for(x=0; x<WD; x++)
            if (img[x][z] > maxval)
                maxval = img[x][z];

    // convert to image
    lhimage *image = allocate_image(WD,HG,-1);
    for(z=0; z<HG; z++)
        for(x=0; x<WD; x++) {
            unsigned char level = (unsigned char)(255*img[x][z]/maxval);
            IMGDOT(image,x,z) = level;
        }
    //TODO: draw axis and gridlines

    // export to PNG
    export_png_file(image, "tmap.png");
}
