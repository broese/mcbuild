//TODO: make a resource module and import these variables via header file
#include "anvil.h"
#include "nbt.h"
#include "lh_image.h"

extern int BIOMES[256];
extern int BLOCKS[256];

//FIXME: for now, limitation due to integer calculations - CWD must be equal CHG and divisible by 4
#define CWD 4
#define CHG CWD

#define CPOS_X(x,y,z) ((CWD/2)*((x)-(y)))
#define CPOS_Y(x,y,z) ((CHG/4)*((x)+(y)-2*(z)))
#define ISIZE_WD(x,y,z) (((x)+(y))*(CWD/2))
#define ISIZE_HG(x,y,z) (((x)+(y)+2*(z))*(CHG/4))
#define ORIGIN_X(X,Y,Z) CPOS_X(X,0,0)
#define ORIGIN_Y(X,Y,Z) CPOS_Y(X,0,0)


typedef struct {
    lhimage *img;
    int offX, offZ;
} drawstate;

drawstate * create_drawing();
void draw_biomes(drawstate *ds, int X, int Y, uint8_t *biomes);
void draw_topmap(drawstate *ds, int X, int Y, uint8_t **cubes);
//void draw_isometric(lhimage *img, mcregion * region, int ox, int oy);
