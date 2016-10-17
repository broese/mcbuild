#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_arr.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_dir.h>
#include <lh_compress.h>
#include <lh_image.h>

#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_ids.h"
#include "mcp_packet.h"
#include "anvil.h"

#define STATE_IDLE     0
#define STATE_STATUS   1
#define STATE_LOGIN    2
#define STATE_PLAY     3

#define INTSWAP(x,y) { int temp=(x); (x)=(y); (y)=temp; }

////////////////////////////////////////////////////////////////////////////////

// fake hud_bogus_map function to make mcpdump not dependent on hud.c
int  hud_bogus_map(slot_t *s) {
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int o_help                      = 0;
int o_block_id                  = -1;
int o_block_meta                = -1;
int o_spawner_mult              = 0;
int o_spawner_single            = 0;
int o_track_inventory           = 0;
int o_track_thunder             = 0;
int o_dump_players              = 0;
int o_extract_maps              = 0;
int o_dump_packets              = 0;
int o_dump_entities             = 0;
int o_dimension                 = 0;
gsworld * o_world               = NULL;
char *o_biomemap                = NULL;
char *o_heightmap               = NULL;
char *o_worlddir                = NULL;
int o_flatbedrock               = 0;
int o_reglimit                  = 0;
int o_xmin                      = -60000;
int o_zmin                      = -60000;
int o_xmax                      =  60000;
int o_zmax                      =  60000;

void print_usage() {
    printf("Usage:\n"
           "mcpdump [options] file.mcs...\n"
           "  -h                        : print this help\n"
           "  -b id[:meta]              : search for blocks by block ID and optionally meta value\n"
           "  -s                        : search for multiple-spawner locations\n"
           "  -S                        : search for single spawner locations\n"
           "  -i                        : track inventory transactions and dump inventory\n"
           "  -t                        : track thunder sounds\n"
           "  -p                        : dump player list\n"
           "  -e                        : dump tracked entities\n"
           "  -m                        : extract in-game maps\n"
           "  -B output.png             : extract biome maps\n"
           "  -H output.png             : extract height maps\n"
           "  -A worlddir               : extract world data to Anvil format - update or create region files in worlddir\n"
           "  -d                        : dump packets\n"
           "  -D dimension              : specify dimension (0:overworld, -1:nether, 1:end)\n"
           "  -L xmin,zmin,xmax,zmax    : limit the area from which chunks will be stored, in regions\n"
           "  -W                        : search for flat bedrock formations suitable for wither spawning\n"
    );
}

int parse_args(int ac, char **av) {
    int opt,error=0;

    while ( (opt=getopt(ac,av,"b:D:B:H:A:L:sSihmdtpWe")) != -1 ) {
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
            case 'p':
                o_dump_players = 1;
                break;
            case 'e':
                o_dump_entities = 1;
                break;
            case 'm':
                o_extract_maps = 1;
                break;
            case 'd':
                o_dump_packets = 1;
                break;
            case 'W':
                o_flatbedrock = 1;
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
                break;
            }
            case 'B': {
                o_biomemap = optarg;
                break;
            }
            case 'H': {
                o_heightmap = optarg;
                break;
            }
            case 'A': {
                o_worlddir = optarg;
                break;
            }
            case 'L': {
                if (sscanf(optarg,"%d,%d,%d,%d", &o_xmin, &o_zmin, &o_xmax, &o_zmax)!=4) {
                    printf("Failed to parse limit specification %s - must be in format xmin,zmin,xmax,zmax\n",optarg);
                    error++;
                }
                if (o_xmin > o_xmax) INTSWAP(o_xmin, o_xmax);
                if (o_zmin > o_zmax) INTSWAP(o_zmin, o_zmax);
                o_reglimit = 1;
                break;
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

void track_remote_sounds(double x, double z, double y, struct timeval tv) {
    double dx = x - gs.own.x;
    double dz = z - gs.own.z;
    //double dist = sqrt(dx*dx+dz*dz);

    //fixp rx = (fixp)((float)dx*scale)+gs.own.x;
    //fixp rz = (fixp)((float)dz*scale)+gs.own.z;

    // process output with ./mcpdump | egrep '^thunder' | sed 's/thunder: //' > output.csv
    printf("thunder: %ld,%.1f,%.1f,%.1f,%.1f,%.1f\n",tv.tv_sec,gs.own.x,gs.own.z,dx,dz,y);
}

////////////////////////////////////////////////////////////////////////////////

void dump_players() {
    int i;
    for(i=0; i<C(gs.players); i++) {
        printf("%3d: %-32s [ %s]", i, P(gs.players)[i].name,
               limhex(P(gs.players)[i].uuid, 16, 16));
        if (P(gs.players)[i].dispname)
            printf(" display=%s",P(gs.players)[i].dispname);
        printf("\n");
    }
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

void track_spawners(nbt_t *te) {
    // determine the type of the spawner
    int type = SPAWNER_OTHER;
    nbt_t * sd = nbt_hget(te, "SpawnData");
    assert(sd && sd->type==NBT_COMPOUND);
    nbt_t * sType = nbt_hget(sd, "id");
    assert(sType && sType->type==NBT_STRING);

    if (!strcmp(sType->st, "Zombie"))   type = SPAWNER_ZOMBIE;
    if (!strcmp(sType->st, "Skeleton")) type = SPAWNER_SKELETON;

    if (type == SPAWNER_OTHER) return;

    nbt_t *x = nbt_hget(te, "x"); assert(x); assert(x->type == NBT_INT);
    nbt_t *y = nbt_hget(te, "y"); assert(y); assert(y->type == NBT_INT);
    nbt_t *z = nbt_hget(te, "z"); assert(z); assert(z->type == NBT_INT);
    pos_t loc = POS(x->i,y->i,z->i);

    // check if this spawner was already recorded in the list
    int i;
    for (i=0; i<C(spawners); i++)
        if (P(spawners)[i].loc.p == loc.p)
            return;

    // store the spawner in the list for later processing
    spawner_t *s = lh_arr_new(GAR(spawners));
    s->loc = loc;
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

void extract_biome_map() {
    int32_t Xmin,Xmax,Zmin,Zmax;
    if (!get_stored_area(&gs.overworld, &Xmin, &Xmax, &Zmin, &Zmax)) {
        printf("No chunks\n");
        return;
    }

    lhimage * img = allocate_image((Xmax-Xmin+1)*16, (Zmax-Zmin+1)*16, -1);
    assert(img);

    int X,Z;
    for(X=Xmin; X<=Xmax; X++) {
        for(Z=Zmin; Z<=Zmax; Z++) {
            gschunk *c = find_chunk(&gs.overworld, X, Z, 0);
            if (!c) continue;

            int x,z;
            int xoff = (X-Xmin)*16, zoff = (Z-Zmin)*16;

            for(z=0; z<16; z++) {
                for(x=0; x<16; x++) {
                    uint8_t bid = c->biome[x+z*16];
                    uint32_t color = BIOMES[bid].name ? BIOMES[bid].color : 0xff00ff;
                    IMGDOT(img, x+xoff, z+zoff) = color;
                }
            }
        }
    }

    ssize_t sz = export_png_file(img, o_biomemap);
    printf("Exported %dx%d map to %s, size=%zd bytes\n",
           img->width, img->height, o_biomemap, sz);

    destroy_image(img);
}

////////////////////////////////////////////////////////////////////////////////

void extract_height_map() {
    int32_t Xmin,Xmax,Zmin,Zmax;
    if (!get_stored_area(o_world, &Xmin, &Xmax, &Zmin, &Zmax)) {
        printf("No chunks\n");
        return;
    }

    lhimage * img = allocate_image((Xmax-Xmin+1)*16, (Zmax-Zmin+1)*16, -1);
    assert(img);

    int X,Z;
    for(X=Xmin; X<=Xmax; X++) {
        for(Z=Zmin; Z<=Zmax; Z++) {
            gschunk *c = find_chunk(o_world, X, Z, 0);
            if (!c) continue;

            int x,z,h;
            int xoff = (X-Xmin)*16, zoff = (Z-Zmin)*16;

            for(z=0; z<16; z++) {
                for(x=0; x<16; x++) {
                    for(h=255; h>=0; h--) {
                        if (c->blocks[x+z*16+h*256].bid) {
                            uint32_t color = (h<<16)|(h<<8)|h;
                            IMGDOT(img, x+xoff, z+zoff) = color;
                            break;
                        }
                    }
                }
            }
        }
    }

    ssize_t sz = export_png_file(img, o_heightmap);
    printf("Exported %dx%d map to %s, size=%zd bytes\n",
           img->width, img->height, o_heightmap, sz);

    destroy_image(img);
}

////////////////////////////////////////////////////////////////////////////////

int extract_world_data() {
    //TODO: delegate directory creation to libhelper
    // determine the directory to save files to
    char dirname[PATH_MAX];
    switch(o_dimension) {
        case 0:
            sprintf(dirname, "%s/region", o_worlddir);
            break;
        case 1:
            sprintf(dirname, "%s/DIM1/region", o_worlddir);
            break;
        case -1:
            sprintf(dirname, "%s/DIM-1/region", o_worlddir);
            break;
    }

    // create directory
    printf("Creating directory %s\n", dirname);
    if (lh_create_dir(dirname, 0777)) {
        printf("Failed to create directory %s : %s\n", dirname, strerror(errno));
        return -1;
    }

    // extract regions
    int s,r,c,i;
    for(s=0; s<512*512; s++) {
        gssreg *sr = o_world->sreg[s];
        if (!sr) continue;

        for(r=0; r<256*256; r++) {
            gsregion *re = sr->region[r];
            if (!re) continue;

            int32_t RX = CC_X(s,r,0)>>5;
            int32_t RZ = CC_Z(s,r,0)>>5;
            //printf("s=%08x, r=%08x, RX=%08x, RZ=%08x\n",s,r,RX,RZ);

            char rpath[PATH_MAX];
            sprintf(rpath, "%s/r.%d.%d.mca", dirname, RX, RZ);

            // check if the file exists and load it
            // FIXME: right now we are just checking if the file can be loaded, catch other possible errors
            mca * reg = NULL;
            if (lh_path_isfile(rpath))
                reg = anvil_load(rpath);
            if (!reg) // if file does not exist or fails to load, create a new one
                reg = anvil_create();

            int nch = 0;
            for(c=0; c<REGCHUNKS; c++) {
                gschunk *ch = re->chunk[c];
                if (!ch) continue;

                int32_t X = CC_X(s,r,c);
                int32_t Z = CC_Z(s,r,c);

                update_chunk_containers(ch, X, Z);
                nbt_t * nbtch = anvil_chunk_create(ch, X, Z);
                anvil_insert_chunk(reg, X, Z, nbtch);
                nch++;
            }

            anvil_save(reg, rpath);
            printf("Added %4d chunks to %s\n", nch, rpath);
        }
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

#define MAXPLEN (4*1024*1024)

void mcpd_packet(MCPacket *pkt) {
    switch (pkt->pid) {
        case SP_UpdateBlockEntity: {
            SP_UpdateBlockEntity_pkt *ube = &pkt->_SP_UpdateBlockEntity;
            if (ube->action != 1) break; // only process SpawnPotentials updates
            assert(ube->nbt);
            track_spawners(ube->nbt);
            break;
        }

        case SP_ChunkData: {
            SP_ChunkData_pkt *cd = &pkt->_SP_ChunkData;
            if (!cd->te) break;
            assert(cd->te->type == NBT_LIST);

            int i;
            for(i=0; i<cd->te->count; i++) {
                nbt_t * te = nbt_aget(cd->te, i);
                nbt_t * id = nbt_hget(te, "id");
                if (!id || id->type != NBT_STRING) {
                    printf("Warning: tile entity id is missing or incorrect:\n");
                    nbt_dump(te);
                    continue;
                }
                if (!strcmp(id->st, "MobSpawner"))
                    track_spawners(te);
            }
            break;
        }

        case SP_SoundEffect: {
            SP_SoundEffect_pkt *tpkt = (SP_SoundEffect_pkt *)&pkt->_SP_SoundEffect;
            if (tpkt->id == 267) { // entity.lightning.thunder
                double tx = (double)tpkt->x/8;
                double tz = (double)tpkt->z/8;
                track_remote_sounds(tx, tz, tpkt->y/8, pkt->ts);
            }
            break;
        }

        case SP_Map: {
            if (!o_extract_maps) break;
            SP_Map_pkt *tpkt = (SP_Map_pkt *)&pkt->_SP_Map;
            if (tpkt->ncols == 128 && tpkt->nrows == 128) {
                if (!maps[tpkt->mapid])
                    lh_alloc_buf(maps[tpkt->mapid], 16384);
                memmove(maps[tpkt->mapid], tpkt->data, 16384);
            }
            break;
        }
    }
}

void parse_mcp(uint8_t *data, ssize_t size, char * name) {
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

        uint8_t *lim = p+len;
        if (lim > data+size) {printf("incomplete packet\n"); break;}

        if (compression) {
            // compression was enabled previously - we need to handle packets differently now
            int usize = lh_read_varint(p); // size of the uncompressed data
            if (usize > 0) {
                // this packet is compressed, unpack and move the decoding pointer to the decoded buffer
                arr_resize(GAR(udata), usize);
                ssize_t usize_ret = zlib_decode_to(p, data+size-p, AR(udata));
                if (usize_ret != usize) {
                    printf("Failed to decompress packet, expected %d bytes, zlib returned %zd. Skipping packet. Some decompressed data shown below:\n", usize, usize_ret);
                    hexdump(P(udata), 64);
                    hdr+=16+len;
                    continue;
                }
                p = P(udata);
                lim = p+usize;
            }
            // usize==0 means the packet is not compressed, so in effect we simply moved the
            // decoding pointer to the start of the actual packet data
        }

        ssize_t plen=lim-p;
        //char *states = "ISLP";
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
        uint32_t stype = ((state<<24)|(is_client<<28)|(type&0xffffff));

        switch (stype) {
            case CI_Handshake: {
                CI_Handshake_pkt tpkt;
                decode_handshake(&tpkt, p);
                state = tpkt.nextState;
                if (!set_protocol(tpkt.protocolVer, NULL)) {
                    printf("Unsupported protocol version %d\n", tpkt.protocolVer);
                    max = 0;
                }
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
    printf("Imported %s : %d packets, protocol %08x\n", name, numpackets, currentProtocol);
}

////////////////////////////////////////////////////////////////////////////////

void search_blocks(gsworld *w, int bid, int meta) {
    assert(w);

    int s,r,c,i;
    for(s=0; s<512*512; s++) {
        gssreg *sr = w->sreg[s];
        if (!sr) continue;

        for(r=0; r<256*256; r++) {
            gsregion *re = sr->region[r];
            if (!re) continue;

            for(c=0; c<32*32; c++) {
                gschunk *ch = re->chunk[c];
                if (!ch) continue;

                int32_t X = CC_X(s,r,c);
                int32_t Z = CC_Z(s,r,c);

                for(i=0; i<65536; i++) {
                    bid_t bl = ch->blocks[i];
                    if (bl.bid == bid && (meta<0 || bl.meta == meta) ) {
                        int32_t x = (X*16+(i&0xf));
                        int32_t z = (Z*16+((i>>4)&0xf));
                        int32_t y = i>>8;

                        printf("Block %3d:%2d at %5d,%5d,%3d\n",
                               bl.bid, bl.meta, x, z, y);
                    }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void search_flat_bedrock() {
    gsworld * oldworld = gs.world;
    gs.world = &gs.nether;
    gsworld *w = gs.world;

    int s,r,c,i;
    for(s=0; s<512*512; s++) {
        gssreg *sr = w->sreg[s];
        if (!sr) continue;

        for(r=0; r<256*256; r++) {
            gsregion *re = sr->region[r];
            if (!re) continue;

            for(c=0; c<32*32; c++) {
                gschunk *ch = re->chunk[c];
                if (!ch) continue;

                int32_t X = CC_X(s,r,c);
                int32_t Z = CC_Z(s,r,c);

                int x,y,z;
                for(y=123; y<125; y++) {
                    for(x=0; x<16; x++) {
                        for(z=0; z<16; z++) {
                            int32_t xx = X*16+x;
                            int32_t zz = Z*16+z;

                            bid_t blk[] = {
                                get_block_at(xx-1,zz-1,y),
                                get_block_at(xx-1,zz,y),
                                get_block_at(xx-1,zz+1,y),
                                get_block_at(xx,zz-1,y),
                                get_block_at(xx,zz,y),
                                get_block_at(xx,zz+1,y),
                                get_block_at(xx+1,zz-1,y),
                                get_block_at(xx+1,zz,y),
                                get_block_at(xx+1,zz+1,y),

                                get_block_at(xx,zz,y-1),
                                get_block_at(xx,zz,y-2),
                            };

                            if (blk[0].bid  == 7 &&
                                blk[1].bid  == 7 &&
                                blk[2].bid  == 7 &&
                                blk[3].bid  == 7 &&
                                blk[4].bid  == 7 &&
                                blk[5].bid  == 7 &&
                                blk[6].bid  == 7 &&
                                blk[7].bid  == 7 &&
                                blk[8].bid  == 7 &&
                                blk[9].bid  != 7 &&
                                blk[10].bid != 7)
                                printf("Flat Bedrock at %d,%d y=%d\n",xx,zz,y);
                        }
                    }
                }
            }
        }
    }

    gs.world = oldworld;
}

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

    if (o_reglimit) {
        gs_setopt(GSOP_REGION_LIMIT, 1);
        gs_setopt(GSOP_XMIN, o_xmin);
        gs_setopt(GSOP_ZMIN, o_zmin);
        gs_setopt(GSOP_XMAX, o_xmax);
        gs_setopt(GSOP_ZMAX, o_zmax);
    }

    int i;
    for(i=optind; av[i]; i++) {
        uint8_t *data;
        ssize_t size = lh_load_alloc(av[i], &data);
        if (size >= 0) {
            parse_mcp(data, size, av[i]);
            free(data);
        }
    }

    switch (o_dimension) {
        case 0:  o_world = &gs.overworld; break;
        case -1: o_world = &gs.nether; break;
        case 1:  o_world = &gs.end; break;
    }

    if (o_track_inventory)
        dump_inventory();
    //dump_entities();

    if (o_dump_players)
        dump_players();

    if (o_spawner_single) {
        for(i=0; i<C(spawners); i++) {
            spawner_t *s = P(spawners)+i;
            printf("SNG %c %5d,%2d,%5d\n",s->type==SPAWNER_ZOMBIE?'Z':'S',
                   s->loc.x,s->loc.y,s->loc.z);
        }
    }

    if (o_spawner_mult)
        find_spawners();

    if (o_block_id >=0)
        search_blocks(o_world, o_block_id, o_block_meta);

    if (o_extract_maps)
        extract_maps();

    if (o_biomemap)
        extract_biome_map();

    if (o_heightmap)
        extract_height_map();

    if (o_worlddir)
        extract_world_data();

    if (o_flatbedrock)
        search_flat_bedrock();

    if (o_dump_entities)
        dump_entities();

    gs_destroy();

    return 0;
}
