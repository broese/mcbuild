#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <lh_image.h>

#define R_MIN   6
#define R_LOOPS 3
#define R_STEP  3
#define R_ANGLE (M_PI/256.0)

static inline int calc_spiral(int *yv, float y0, float dir) {
    // y = y0 + a/(2*PI)*R_STEP

    int c=0,x=0,y=lrintf(y0);
    yv[c++] = y;

    float dpr = (float)R_STEP*dir/(2*M_PI); // distance-per-radian

    do {
        x++;
        // middle dot (y stays same)
        float am = atanf((float)x/(float)y);    // angle to the dot
        float sm = y0+am*dpr;                   // spiral distance to the angle going through the dot
        float dm = fabsf(sqrtf(x*x+y*y)-sm);    // diff between the spiral and the dot
        
        // upper dot (y increases)
        float au = atanf((float)x/(float)(y+1));
        float su = y0+au*dpr;
        float du = fabsf(sqrtf(x*x+(y+1)*(y+1))-su);
        
        // bottom dot (y decreases)
        float ad = atanf((float)x/(float)(y-1));
        float sd = y0+ad*dpr;
        float dd = fabsf(sqrtf(x*x+(y-1)*(y-1))-sd);

        //printf("%d,%d  %.2f,%.2f,%.2f %.2f,%.2f,%.2f %.2f,%.2f,%.2f\n",
        //       y,x, am,sm,dm, au,su,du, ad,sd,dd); 
        
        if (du<dm && du<dd) y++;               // upper dot is the best choice
        else if (dd<dm && dd<du) y--;          // bottom dot is the best choice
        // otherwise we keep the middle dot

        yv[c++] = y;
    } while (y>x);

    return c;
}

static inline void draw_arc(lhimage *im, uint32_t color, int *yv, int count, int x0, int y0, int xdir, int ydir) {
    int i;

    for(i=0; i<count; i++) {
        int x = x0 + i*xdir;
        int y = y0 + yv[i]*ydir;
        IMGDOT(im, x, y) = color;
    }
}

static inline void draw_yarc(lhimage *im, uint32_t color, int *yv, int count, int x0, int y0, int xdir, int ydir) {
    int i;

    for(i=0; i<count; i++) {
        int x = x0 + yv[i]*xdir;
        int y = y0 + i*ydir;
        IMGDOT(im, x, y) = color;
    }
}

void makespiral(lhimage *im) {
    int i,j;
    IMGDOT(im,64,64) = 0xffff00;
    for(i=1; i<56; i++) {
        IMGDOT(im, 64-i, 64-i) = 0x808080;
        IMGDOT(im, 64-i, 64+i) = 0x808080;
        IMGDOT(im, 64+i, 64-i) = 0x808080;
        IMGDOT(im, 64+i, 64+i) = 0x808080;
    }

    for(i=0; i<R_LOOPS; i++) {
        float d0=R_MIN+R_STEP*i;      // distance on the N-ray
        float d4=R_MIN+R_STEP*(i+1);  // next loop
        float d2=(d0+d4)/2;           // S-ray
        float d1=(d0+d2)/2;           // E-ray
        float d3=(d2+d4)/2;           // W-ray

        int y[4096];
        int c;

        // North-CW
        c = calc_spiral(y, d0, 1);
        draw_arc(im, 0xff00ff, y, c, 64, 64, 1, -1);

        // North-CCW
        c = calc_spiral(y, d4, -1);
        draw_arc(im, 0xff00ff, y, c, 64, 64, -1, -1);

        // East-CW
        c = calc_spiral(y, d1, 1);
        draw_yarc(im, 0xff00ff, y, c, 64, 64, 1, 1);

        // East-CCW
        c = calc_spiral(y, d1, -1);
        draw_yarc(im, 0xff00ff, y, c, 64, 64, 1, -1);

        // South-CW
        c = calc_spiral(y, d2, 1);
        draw_arc(im, 0xff00ff, y, c, 64, 64, -1, 1);

        // South-CCW
        c = calc_spiral(y, d2, -1);
        draw_arc(im, 0xff00ff, y, c, 64, 64, 1, 1);

        // West-CW
        c = calc_spiral(y, d3, 1);
        draw_yarc(im, 0xff00ff, y, c, 64, 64, -1, -1);

        // West-CCW
        c = calc_spiral(y, d3, -1);
        draw_yarc(im, 0xff00ff, y, c, 64, 64, -1, 1);
    }
    
}

int main(int ac, char **av) {
    lhimage * im = allocate_image(128, 128, -1);
    assert(im);

    makespiral(im);

    export_png_file(im, "spiral.png");
    destroy_image(im);
}
