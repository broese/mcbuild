#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <openssl/rand.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_arr.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_compress.h>
#include <lh_image.h>

#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_ids.h"
#include "mcp_packet.h"

#define STATE_IDLE     0
#define STATE_STATUS   1
#define STATE_LOGIN    2
#define STATE_PLAY     3

////////////////////////////////////////////////////////////////////////////////

int o_help                      = 0;
int o_block_id                  = -1;
int o_block_meta                = -1;
int o_spawner_mult              = 0;
int o_spawner_single            = 0;
int o_track_inventory           = 0;
int o_track_thunder             = 0;
int o_extract_maps              = 0;
int o_dump_packets              = 0;
int o_dimension                 = 0;

void print_usage() {
    printf("Usage:\n"
           "mcpdump [options] file.mcs...\n"
           "  -h                        : print this help\n"
           //"  -b id[:meta]              : search for blocks by block ID and optionally meta value\n"
           "  -s                        : search for multiple-spawner locations\n"
           "  -S                        : search for single spawner locations\n"
           "  -i                        : track inventory transactions and dump inventory\n"
           "  -t                        : track thunder sounds\n"
           "  -m                        : extract in-game maps\n"
           "  -d                        : dump packets\n"
           "  -D dimension              : specify dimension (0:overworld, -1:nether, 1:end)\n"
    );
}

int parse_args(int ac, char **av) {
    int opt,error=0;

    while ( (opt=getopt(ac,av,"b:D:sSihmdt")) != -1 ) {
        switch (opt) {
            case 'h':
                o_help = 1;
                break;
            case 's':
                o_spawner_single = 1;
                break;
            case 'S':
                o_spawner_mult = 1;
                break;
            case 'i':
                o_track_inventory = 1;
                break;
            case 't':
                o_track_thunder = 1;
                break;
            case 'm':
                o_extract_maps = 1;
                break;
            case 'd':
                o_dump_packets = 1;
                break;
            case 'b': {
                int bid,meta;
                if (sscanf(optarg, "%d:%d", &bid, &meta)==2) {
                    o_block_id = bid;
                    o_block_meta = meta;
                }
                else if (sscanf(optarg, "%d", &bid)==1) {
                    o_block_id = bid;
                    o_block_meta = -1;
                }
                else {
                    printf("-b : you must specify an argument as block_id[:meta] - use decimal numbers\n");
                    error++;
                }
                break;
            }
            case 'D': {
                if (sscanf(optarg, "%d", &o_dimension)!=1 || o_dimension<-1 || o_dimension>1) {
                    printf("-D : incorrect dimension specified, must be 0 for Overworld, -1 for nether, 1 for end\n");
                    error++;
                }
            }
            case '?': {
                printf("Unknown option -%c", opt);
                error++;
                break;
            }
        }
    }

    if (!av[optind]) error++;

    return error==0;
}

////////////////////////////////////////////////////////////////////////////////

#define VIEWDIST (158<<5)
#define THUNDERDIST (160000<<5)

void track_remote_sounds(int32_t x, int32_t z, int32_t y, struct timeval tv) {
#if 0
    fixp dx = x - gs.own.x;
    fixp dz = z - gs.own.z;
    int32_t sqdist = dx*dx+dz*dz;

    // thunder within view distance - ignore local thunder
    if (sqdist < VIEWDIST*VIEWDIST) return;

    float scale = (float)THUNDERDIST / sqrtf((float)sqdist);
    fixp rx = (fixp)((float)dx*scale)+gs.own.x;
    fixp rz = (fixp)((float)dz*scale)+gs.own.z;

    // process output with ./mcpdump | egrep '^thunder' | sed 's/thunder: //' > output.csv
    printf("thunder: %ld,%d,%.1f,%.1f,%.1f,%d\n",tv.tv_sec,gs.own.x,gs.own.z,rx,rz,y);
#endif
}

////////////////////////////////////////////////////////////////////////////////

#define SPAWNER_OTHER      0
#define SPAWNER_SKELETON   1
#define SPAWNER_ZOMBIE     2

#define MAXDIST 31.0
#define SQ(x) ((x)*(x))

typedef struct {
    pos_t loc;
    int   type;
} spawner_t;

lh_arr_declare_i(spawner_t, spawners);

static inline float pos_dist(pos_t a, pos_t b) {
    int32_t dx = a.x-b.x;
    int32_t dy = a.y-b.y;
    int32_t dz = a.z-b.z;
    return sqrtf((float)SQ(dx)+(float)SQ(dy)+(float)SQ(dz));
}

void track_spawners(SP_UpdateBlockEntity_pkt *ube) {
    if (ube->action != 1) return; // only process SpawnPotentials updates
    assert(ube->nbt);

    // determine the type of the spawner
    int type = SPAWNER_OTHER;
    nbt_t * sd = nbt_hget(ube->nbt, "SpawnData");
    assert(sd && sd->type==NBT_COMPOUND);
    nbt_t * sType = nbt_hget(sd, "id");
    assert(sType && sType->type==NBT_STRING);

    if (!strcmp(sType->st, "Zombie"))   type = SPAWNER_ZOMBIE;
    if (!strcmp(sType->st, "Skeleton")) type = SPAWNER_SKELETON;

    if (type == SPAWNER_OTHER) return;

    // check if this spawner was already recorded in the list
    int i;
    for (i=0; i<C(spawners); i++)
        if (P(spawners)[i].loc.p == ube->loc.p)
            return;

    // store the spawner in the list for later processing
    spawner_t *s = lh_arr_new(GAR(spawners));
    s->loc = ube->loc;
    s->type = type;
}

static void find_spawners() {
    int i,j,k;

    for(i=0; i<C(spawners); i++) {
        for(j=i+1; j<C(spawners); j++) {
            spawner_t *a = P(spawners)+i;
            spawner_t *b = P(spawners)+j;
            float dist = pos_dist(a->loc, b->loc);
            if (dist < MAXDIST) {
                printf("DBL  %c:%5d,%2d,%5d %c:%5d,%2d,%5d dist=%.1f\n",
                       (a->type==SPAWNER_ZOMBIE)?'Z':'S',
                       a->loc.x,a->loc.y,a->loc.z,
                       (b->type==SPAWNER_ZOMBIE)?'Z':'S',
                       b->loc.x,b->loc.y,b->loc.z,
                       dist);

                // try to find triple-spawners
                for(k=0; k<C(spawners); k++) {
                    if (k==i || k==j) continue;

                    spawner_t *c = P(spawners)+k;
                    if (pos_dist(a->loc, c->loc) < MAXDIST &&
                        pos_dist(b->loc, c->loc) < MAXDIST) {

                        pos_t center;
                        center.x = (a->loc.x+b->loc.x+c->loc.x)/3;
                        center.y = (a->loc.y+b->loc.y+c->loc.y)/3;
                        center.z = (a->loc.z+b->loc.z+c->loc.z)/3;
                        float da = pos_dist(a->loc, center);
                        float db = pos_dist(b->loc, center);
                        float dc = pos_dist(c->loc, center);

                        printf("TRP  %c:%5d,%2d,%5d %c:%5d,%2d,%5d %c:%5d,%2d,%5d dist=%.1f,%.1f,%.1f\n",
                               (a->type==SPAWNER_ZOMBIE)?'Z':'S',
                               a->loc.x,a->loc.y,a->loc.z,
                               (b->type==SPAWNER_ZOMBIE)?'Z':'S',
                               b->loc.x,b->loc.y,b->loc.z,
                               (c->type==SPAWNER_ZOMBIE)?'Z':'S',
                               c->loc.x,c->loc.y,c->loc.z,
                               da,db,dc);
                    }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

uint8_t * maps[65536];

uint8_t map_colors[256][3] = {
    {0, 0, 0},
    {127, 178, 56},
    {247, 233, 163},
    {167, 167, 167},
    {255, 0, 0},
    {160, 160, 255},
    {167, 167, 167},
    {0, 124, 0},
    {255, 255, 255},
    {164, 168, 184},
    {183, 106, 47},
    {112, 112, 112},
    {64, 64, 255},
    {104, 83, 50},
    {255, 252, 245},
    {216, 127, 51},
    {178, 76, 216},
    {102, 153, 216},
    {229, 229, 51},
    {127, 204, 25},
    {242, 127, 165},
    {76, 76, 76},
    {153, 153, 153},
    {76, 127, 153},
    {127, 63, 178},
    {51, 76, 178},
    {102, 76, 51},
    {102, 127, 51},
    {153, 51, 51},
    {25, 25, 25},
    {250, 238, 77},
    {92, 219, 213},
    {74, 128, 255},
    {0, 217, 58},
    {21, 20, 31},
    {112, 2, 0},
    {126, 84, 48},
};

void extract_maps() {
    int id;
    for (id=0; id<65536; id++) {
        if (!maps[id]) continue;
        printf("Extracting map #%d\n",id);
        lhimage * img = allocate_image(128, 128, -1);

        int x,z;
        for(z=0; z<128; z++) {
            uint8_t *row = maps[id]+z*128;
            for(x=0; x<128; x++) {
                float f;
                switch(row[x]&3) {
                    case 0: f=180.0/255.0; break;
                    case 1: f=220.0/255.0; break;
                    case 2: f=1.0; break;
                    case 3: f=135.0/255.0; break;
                }
                float r = (float)map_colors[row[x]>>2][0]*f;
                float g = (float)map_colors[row[x]>>2][1]*f;
                float b = (float)map_colors[row[x]>>2][2]*f;
                uint32_t color = (((uint32_t)r)<<16)|(((uint32_t)g)<<8)|((uint32_t)b);
                IMGDOT(img,x,z) = color;
            }
        }

        char fname[256];
        sprintf(fname, "map_%04d.png", id);
        ssize_t sz = export_png_file(img, fname);
        destroy_image(img);
    }
}

////////////////////////////////////////////////////////////////////////////////

#define MAXPLEN (4*1024*1024)

void mcpd_packet(MCPacket *pkt) {
    switch (pkt->pid) {
        case SP_UpdateBlockEntity: {
            track_spawners(&pkt->_SP_UpdateBlockEntity);
            break;
        }

        case SP_SoundEffect: {
            SP_SoundEffect_pkt *tpkt = (SP_SoundEffect_pkt *)&pkt->_SP_SoundEffect;
            if (!strcmp(tpkt->name,"ambient.weather.thunder")) {
                fixp tx = tpkt->x*4;
                fixp tz = tpkt->z*4;
                track_remote_sounds(tx, tz, tpkt->y/8, pkt->ts);
            }
            break;
        }

        case SP_Map: {
            if (!o_extract_maps) break;
            SP_Maps_pkt *tpkt = (SP_Maps_pkt *)&pkt->_SP_Maps;
            if (tpkt->ncols == 128 && tpkt->nrows == 128) {
                if (!maps[tpkt->mapid])
                    lh_alloc_buf(maps[tpkt->mapid], 16384);
                memmove(maps[tpkt->mapid], tpkt->data, 16384);
            }
            break;
        }
    }
}

void parse_mcp(uint8_t *data, ssize_t size) {
    uint8_t *hdr = data;
    int state = STATE_IDLE;

    int compression = 0; // compression disabled initially
    BUFI(udata);         // buffer for decompressed data

    int max=20;
    int numpackets = 0;
    while(hdr-data < (size-16) && max>0) {
        numpackets++;
        //max--;
        uint8_t *p = hdr;

        int is_client = read_int(p);
        int sec       = read_int(p);
        int usec      = read_int(p);
        int len       = read_int(p);

        //printf("%d.%06d: len=%d\n",sec,usec,len);
        uint8_t *lim = p+len;
        if (lim > data+size) {printf("incomplete packet\n"); break;}

        if (compression) {
            // compression was enabled previously - we need to handle packets differently now
            int usize = lh_read_varint(p); // size of the uncompressed data
            if (usize > 0) {
                // this packet is compressed, unpack and move the decoding pointer to the decoded buffer
                arr_resize(GAR(udata), usize);
                ssize_t usize_ret = zlib_decode_to(p, data+size-p, AR(udata));
                if (usize_ret < 0) { printf("failed to decompress packet\n"); break;}
                p = P(udata);
                lim = p+usize;
            }
            // usize==0 means the packet is not compressed, so in effect we simply moved the
            // decoding pointer to the start of the actual packet data
        }

        ssize_t plen=lim-p;
        //printf("%d.%06d: %c %c type=?? plen=%6zd    ",sec,usec,is_client?'C':'S',states[state],plen);
        //hexprint(p, (plen>64)?64:plen);

        if (state == STATE_PLAY) {
            MCPacket *pkt = decode_packet(is_client, p, plen);
            pkt->ts.tv_sec = sec;
            pkt->ts.tv_usec = usec;
            if (o_dump_packets) dump_packet(pkt);
            gs_packet(pkt);
            mcpd_packet(pkt);
            free_packet(pkt);
        }

        // pkt will point at the start of packet (specifically at the type field)
        // while p will move to the next field to be parsed.

        int type = lh_read_varint(p);
        //printf("%d.%06d: %c type=%x len=%zd\n",sec,usec,is_client?'C':'S',type,len);

        uint32_t stype = ((state<<24)|(is_client<<28)|(type&0xffffff));


        //if (state == STATE_PLAY)
        //    import_packet(pkt, len, is_client);

        switch (stype) {
            case CI_Handshake: {
                CI_Handshake_pkt tpkt;
                decode_handshake(&tpkt, p);
                state = tpkt.nextState;
                break;
            }
            case SL_LoginSuccess: {
                state = STATE_PLAY;
                break;
            }

            case SL_SetCompression: {
                compression = 1;
                break;
            }
        }

        hdr += 16+len; // advance header pointer to the next packet
    }

    lh_free(P(udata));
    printf("Total packets: %d\n",numpackets);
}

////////////////////////////////////////////////////////////////////////////////

#if 0
void search_blocks(int dim, int bid, int meta) {
    gsworld *w;
    switch (dim) {
        case 0:  w = &gs.overworld; break;
        case -1: w = &gs.nether; break;
        case 1:  w = &gs.end; break;
    }

    if (!w) return;

    int Xi,Zi,i;
    for(Zi=0; Zi<w->Zs; Zi++) {
        for(Xi=0; Xi<w->Xs; Xi++) {
            int32_t idx = Xi+Zi*w->Xs;
            gschunk *c = w->chunks[idx];

            if (c) {
                for(i=0; i<65536; i++) {
                    bid_t bl = c->blocks[i];
                    if (bl.bid == bid && (meta<0 || bl.meta == meta) ) {
                        int32_t x = ((Xi+w->Xo)*16+(i&0xf));
                        int32_t z = ((Zi+w->Zo)*16+((i>>4)&0xf));
                        int32_t y = i>>8;

                        printf("Block %3d:%2d at %5d,%5d,%3d\n",
                               bl.bid, bl.meta, x, z, y);
                    }
                }
            }
        }
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////

int main(int ac, char **av) {

    if (!parse_args(ac,av) || o_help) {
        print_usage();
        return !o_help;
    }

    gs_reset();
    gs_setopt(GSOP_PRUNE_CHUNKS, 0);

    if (o_spawner_single && o_spawner_mult)
        gs_setopt(GSOP_SEARCH_SPAWNERS, 1);

    if (o_track_inventory)
        gs_setopt(GSOP_TRACK_INVENTORY, 1);

    int i;
    for(i=optind; av[i]; i++) {
        uint8_t *data;
        ssize_t size = lh_load_alloc(av[i], &data);
        if (size >= 0) {
            parse_mcp(data, size);
            free(data);
        }
    }

    if (o_track_inventory)
        dump_inventory();
    //dump_entities();

    if (o_spawner_single) {
        for(i=0; i<C(spawners); i++) {
            spawner_t *s = P(spawners)+i;
            printf("SNG %c %5d,%2d,%5d\n",s->type==SPAWNER_ZOMBIE?'Z':'S',
                   s->loc.x,s->loc.y,s->loc.z);
        }
    }

    if (o_spawner_mult)
        find_spawners();

#if 0
    if (o_block_id >=0)
        search_blocks(o_dimension, o_block_id, o_block_meta);
#endif

    if (o_extract_maps)
        extract_maps();

    gs_destroy();

    return 0;
}
