#if MEMORY_DEBUG
#include <mcheck.h>
#endif

#include <SDL/SDL.h> // main SDL header

#include "lh_buffers.h"
#include "lh_debug.h"
#include "lh_files.h"
#include "lh_compress.h"

typedef struct {
    int32_t is_client;
    int32_t sec;
    int32_t usec;
    int32_t length;
} mcsh;

// read a MCStream (.mcs) file and return only the messages
// 0x33 (Chunk Data), 0x0B (Player Position) and 0x0D (Player Position and Look)
// returns 1 on success, 0 on end of stream or error
// TODO: modify this function to be able to read from simultaneously written file (like tail -f)
int read_stream(FILE *mcs, uint8_t **buf, ssize_t *len, mcsh *header) {
    do {
        // read header
        char hb[sizeof(mcsh)];
        if (fread(hb, sizeof(mcsh), 1, mcs) != 1) return 0;
        //hexdump(hb, sizeof(hb));
        uint8_t *p = hb;
        header->is_client = read_int(p);
        header->sec       = read_int(p);
        header->usec      = read_int(p);
        header->length    = read_int(p);

        // extend buffer if necessary
        if (*len < header->length)
            ARRAY_EXTEND(*buf, *len, header->length);

        // read in the message data
        if (fread(*buf, 1, header->length, mcs) != header->length) return 0;
        //printf("%02x %6d\n", **buf, header->length);
        //hexdump(*buf, header->length);

    } while (**buf != 0x33 && **buf != 0x32 && **buf != 0x0D && **buf != 0x0B);
    return 1;
}

uint8_t map[2048*2048];
#define SSPATH "crstate.dat"

int xpos, zpos;

void init_map() {
    //FIXME: for now, it's fixed size, covering a sufficiently large area
    //(32x32km, in other words 16km or 1024 chunks or 32 regions in each direction
    //from the spawn, giving us 4M chunks in total - just a nifty 4MiB of RAM
    
    CLEAR(map);

    //load previous state
    ssize_t sz;
    uint8_t *saved = read_file(SSPATH, &sz);
    if (saved) {
        // the file is there
        ssize_t osz = zlib_decode_to(saved, sz, map, sizeof(map));
        free(saved);
        if (osz != sizeof(map)) {
            printf("Failed to uncompress saved map\n");
            CLEAR(map);
        }
    }

    xpos = zpos = 0;
}

void update_map(int X, int Z, uint16_t pbm) {
    printf("Chunk %4d,%4d\n",X,Z);
    int off = (X+1024)+(Z+1024)*2048;
    if (pbm == 0) // this chunk update was a "chunk release", as PBM is set to 0
        map[off] = 1; // set to "visited"
    else
        map[off] = 2; // set to "visiting"
}

static SDL_Surface *screen = NULL;

int sdl_init(int screen_width, int screen_height, int fullscreen) {
    // init SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
        return 0;
    }
    atexit(SDL_Quit);

    // set video mode
    int flags = SDL_DOUBLEBUF | SDL_HWSURFACE;
    if (fullscreen) flags |= SDL_FULLSCREEN;

    screen = SDL_SetVideoMode(screen_width, screen_height, 32, flags);
    if (!screen) {
        fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
        return 0;
    }

    SDL_EnableKeyRepeat(100, 20);

    printf("SDL init successful\n");
    return 1;
}

#define SCR_WD 1280
#define SCR_HG  800
#define CSZ      16
#define SSZ       1
#define SCR_CWD  ((SCR_WD)/SSZ)
#define SCR_CHG  ((SCR_HG)/SSZ)

void redraw_map() {
    static time_t lasttime = -1;
    time_t now = time(NULL);
    if (now == lasttime)
        return;
    lasttime = now;

    uint32_t *scr = screen->pixels;
    int pitch = screen->pitch/4;

    int X=xpos/CSZ, Z=zpos/CSZ;
    int Xmin = X-(SCR_CWD/2);
    int Zmin = Z-(SCR_CHG/2);
    
    //SDL_LockSurface(screen);
    int Xi,Zi;
    for(Xi=0; Xi<SCR_CWD; Xi++) {
        for(Zi=0; Zi<SCR_CHG; Zi++) {
            int MX = Xi+Xmin+1024;
            int MZ = Zi+Zmin+1024;
            uint8_t state = map[MX+MZ*2048];
            uint32_t color = 0;
            if (Xi==SCR_CWD/2 && Zi==SCR_CHG/2) {
                color = 0xffff00;
            }
            else if (state == 1) {
                color = 0x808080;
            }
            else if (state == 2) {
                color = 0xc0c0c0;
            }

            int i,j;
            uint32_t *ptr = &scr[Xi*SSZ + Zi*SSZ*pitch];
            for(j=0; j<SSZ; j++) {
                for(i=0; i<SSZ; i++) {
                    ptr[i] = color;
                }
                ptr += pitch;
            }
        }
    }
    //SDL_UnlockSurface(screen);
    SDL_Flip(screen); 
}

int main(int ac, char ** av) {
#if MEMORY_DEBUG
    mtrace();
#endif

    if (!sdl_init(SCR_WD, SCR_HG, 0)) return 1;

    // allocate the chunk map
    init_map();

    ssize_t sz;
    FILE * mcs = open_file_r(av[1], &sz);
    if (!mcs) return 1;

    BUFFER(buf,len);
    mcsh h;

    while(read_stream(mcs, &buf, &len, &h)) {
        uint8_t *p = buf;
        char     type = read_char(p);

        switch (type) {
            case 0x33: {
                // read the Chunk Data header
                int32_t  X    = read_int(p);
                int32_t  Z    = read_int(p);
                char     cont = read_char(p);
                uint16_t pbm  = read_short(p);
                uint16_t abm  = read_short(p);
                int32_t  size = read_int(p);

                update_map(X,Z,pbm);
        
                break;
            }
            case 0x32: {
                // read the Chunk Data header
                int32_t  X     = read_int(p);
                int32_t  Z     = read_int(p);
                char     alloc = read_char(p);
                if (!alloc)
                    update_map(X,Z,0);
        
                break;
            }
            case 0x0D:
            case 0x0B: {
                double x = read_double(p);
                double y = read_double(p);
                double s = read_double(p);
                double z = read_double(p);

                xpos = (int)x;
                zpos = (int)z;
                break;
            }
        }
        redraw_map();
        usleep(50);

        //TODO: make the loop terminate on Ctrl+C
    }

    // clear all "visiting" areas
    int i;
    for(i=0; i<sizeof(map); i++)
        if (map[i] == 2)
            map[i] = 1;

    //save the map
    ssize_t osz;
    uint8_t * encoded = zlib_encode(map, sizeof(map), &osz);
    write_file(SSPATH, encoded, osz);

    free(buf);
    fclose(mcs);

#if MEMORY_DEBUG
    muntrace();
#endif
    
}
