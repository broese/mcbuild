#if MEMORY_DEBUG
#include <mcheck.h>
#endif

#include <SDL/SDL.h> // main SDL header
#include <signal.h>

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

#define RS_OK    0
#define RS_EOF   1
#define RS_ERR   2

const char * REGMSG = "\x33\x32\x0d\x0b\x01\x27\x14\x1d";

// read a MCStream (.mcs) file and return only the messages
// 0x33 (Chunk Data), 0x0B (Player Position) and 0x0D (Player Position and Look)
// returns 1 on success, 0 on end of stream or error
// TODO: modify this function to be able to read from simultaneously written file (like tail -f)
int read_stream(FILE *mcs, uint8_t **buf, ssize_t *len, mcsh *header) {
    ssize_t rb;
    char hb[sizeof(mcsh)+1];

    clearerr(mcs);

    while(1) {
        // save current position in case of error
        off_t cpos = ftell(mcs);
        //printf("Offset: %d\n",cpos);

        // read header + message ID
        rb = fread(hb, 1, sizeof(hb), mcs);
        if (rb != sizeof(hb)) { // could not read the entire header, possibly EOF
            //printf("*header* Offset: %d, feof=%d, ferror=%d\n",ftell(mcs),feof(mcs),ferror(mcs));
            fseek(mcs, cpos, SEEK_SET);
            return ferror(mcs) ? RS_ERR : RS_EOF;
        }

        uint8_t *p = hb;
        header->is_client = read_int(p);
        header->sec       = read_int(p);
        header->usec      = read_int(p);
        header->length    = read_int(p);

        uint8_t type = read_char(p);
        int i;
        for(i=0; REGMSG[i] && type != REGMSG[i]; i++);
        if (!REGMSG[i]) {
            // this is not the message type we are interested in, so we skip
            // to the start of the next message. Note that this might set the
            // position past the file end, and lead to an FEOF on the next cycle
            //printf("Skipping %d -> %d\n", cpos, cpos + sizeof(mcsh) + header->length);
            fseek(mcs, cpos + sizeof(mcsh) + header->length, SEEK_SET);
            continue;
        }

        usleep(1000);

        // extend buffer if necessary
        if (*len < header->length)
            ARRAY_EXTEND(*buf, *len, header->length);

        // since we read the message type byte, put it back into buffer
        **buf = type;

        // read in the message data
        rb = fread(*buf+1, 1, header->length-1, mcs);
        if ( rb != header->length-1) {
            printf("*message* Offset: %d, feof=%d, ferror=%d\n",ftell(mcs),feof(mcs),ferror(mcs));
            fseek(mcs, cpos, SEEK_SET);
            return ferror(mcs) ? RS_ERR : RS_EOF;
        }

        return RS_OK;
    }
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
    //printf("Chunk %4d,%4d\n",X,Z);
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

#define SCR_WD  480
#define SCR_HG  480
#define CSZ      16
#define SSZ       1
#define SCR_CWD  ((SCR_WD)/SSZ)
#define SCR_CHG  ((SCR_HG)/SSZ)

int signal_caught;

void signal_handler(int signum) {
    printf("Caught signal %d, stopping main loop\n",signum);
    signal_caught = 1;
}

int32_t PID = -1; // Player's entity ID
int attached;
int32_t VID = -1; // attached vehicle's entity ID

typedef struct {
    int  eid;
    char name[20];
    int  x,y,z;
} player;

player players[256];
int nump;

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

    if (nump > 0) {
        int y,i;
        for (y=32; y<64; y++) {
            uint32_t *ptr = &scr[y*pitch+32];
            for (i=0; i<32; i++)
                *ptr++ = 0xff0000;
        }
        printf("***** WARNING *****\n");
        for (i=0; i<nump; i++) {
            printf(" %s\n",players[i].name);
        }
    }

    //SDL_UnlockSurface(screen);
    SDL_Flip(screen); 
}

static inline int read_string(char **p, char *s, char *limit) {
    if (limit-*p < 2) return 1;
    uint16_t slen = read_short(*p);
    if (limit-*p < slen*2) return 1;

    int i;
    for(i=0; i<slen; i++) {
        (*p)++;
        s[i] = read_char(*p);
    }
    s[i] = 0;
    return 0;
}

int main(int ac, char ** av) {
#if MEMORY_DEBUG
    mtrace();
#endif

    signal_caught = 0;
    struct sigaction sa;
    CLEAR(sa);
    sa.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sa, NULL))
        LH_ERROR(1,"Failed to set sigaction\n");

    if (!sdl_init(SCR_WD, SCR_HG, 0)) return 1;

    // allocate the chunk map
    init_map();

    ssize_t sz;
    FILE * mcs = open_file_r(av[1], &sz);
    if (!mcs) return 1;

    BUFFER(buf,len);
    mcsh h;

    CLEAR(players);
    nump = 0;

    while(!signal_caught) {
        int res = read_stream(mcs, &buf, &len, &h);

        if (res == RS_ERR) {
            printf("Encountered error: %d %s\n", errno, strerror(errno));
            break;
        }

        if (res == RS_EOF) {
            printf("EOF, waiting\n");
            sleep(1);
            continue;
        }

        uint8_t *p = buf;
        char     type = read_char(p);

        switch (type) {
            case 0x33: {
                // Chunk Data - loaded or unloaded chunks
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
                // Chunk Allocation (pre-1.3 protocol) - unloaded chunks
                int32_t  X     = read_int(p);
                int32_t  Z     = read_int(p);
                char     alloc = read_char(p);
                if (!alloc)
                    update_map(X,Z,0);
        
                break;
            }
            case 0x0D:
            case 0x0B: {
                // Player Position (and Look)
                if (!attached) {
                    // ignore while we are attached (i.e. to a boat)
                    double x = read_double(p);
                    double y = read_double(p);
                    double s = read_double(p);
                    double z = read_double(p);

                    xpos = (int)x;
                    zpos = (int)z;
                }
                break;
            }
            case 0x01: {
                // Login Request - get the player's EID
                // necessary to handle attach
                PID = read_int(p);
                break;
            }
            case 0x27: {
                // Attach Entity - attaching or detaching from vehicles
                int eid = read_int(p);
                int vid = read_int(p);
                if (eid == PID) {
                    attached = (vid!=-1);
                    VID = vid;
                }
                break;
            }
            case 0x14: {
                // Spawn Named Entity
                player *pl = players+nump;

                pl->eid = read_int(p);
                read_string((char **)&p, pl->name, p+1000); //FIXME
                pl->x = read_int(p); pl->x/=32;
                pl->y = read_int(p); pl->y/=32;
                pl->z = read_int(p); pl->z/=32;

                nump++;

                printf("NAMED ENTITY %s %d,%d,%d\n",pl->name,pl->x,pl->y,pl->z);

                break;                
            }
            case 0x1d: {
                // Destroy Entity
                unsigned char count = read_char(p);
                while(count > 0) {
                    int eid = read_int(p);
                    int i;
                    for(i=0; i<nump; i++) {
                        if (players[i].eid == eid) {
                            printf("DELETE NAMED ENTITY %s\n",players[i].name);
                            ARRAY_DELETE(players, nump,i);
                            break;
                        }
                    }
                    count --;
                }
                break;
            }
        }
        redraw_map();
    }

    printf("Saving map ....\n");

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
