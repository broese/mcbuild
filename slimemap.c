#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "lh_image.h"


/*
seed: 1517717513523274421

Random rnd = new Random(seed + 
                        (long) (xPosition * xPosition * 0x4c1906) + 
                        (long) (xPosition * 0x5ac0db) + 
                        (long) (zPosition * zPosition) * 0x4307a7L + 
                        (long) (zPosition * 0x5f24f) ^ 0x3ad8025f);
return rnd.nextInt(10) == 0;

public Random(long seed) { setSeed(seed); }

synchronized public void setSeed(long seed) {
       this.seed = (seed ^ 0x5DEECE66DL) & ((1L << 48) - 1);
       haveNextNextGaussian = false;
 }

synchronized protected int next(int bits) {
       seed = (seed * 0x5DEECE66DL + 0xBL) & ((1L << 48) - 1);
       return (int)(seed >>> (48 - bits));
 }

public int nextInt() {  return next(32); }

public int nextInt(int n) {
     if (n<=0)
		throw new IllegalArgumentException("n must be positive");

     if ((n & -n) == n)  // i.e., n is a power of 2
         return (int)((n * (long)next(31)) >> 31);

     int bits, val;
     do {
         bits = next(31);
         val = bits % n;
     } while(bits - val + (n-1) < 0);
     return val;
 }

*/

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

int main(int ac, char **av) {
    lhimage * img = allocate_image(512,512);

    int X,Y;
    for(X=0; X<32; X++) {
        for(Y=0; Y<32; Y++) {
            if (is_slime(X,Y))
                draw_block(img,X,Y);
        }
    }

    export_png_file(img, "slimes.png");

    return 0;
}

