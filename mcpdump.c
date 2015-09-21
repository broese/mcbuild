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

#include "mcp_gamestate.h"
#include "mcp_game.h"
#include "mcp_ids.h"
#include "mcp_packet.h"

#define STATE_IDLE     0
#define STATE_STATUS   1
#define STATE_LOGIN    2
#define STATE_PLAY     3

static char * states = "ISLP";

#define VIEWDIST (158<<5)
#define THUNDERDIST (160000<<5)

void track_remote_sounds(int32_t x, int32_t z, int32_t y, struct timeval tv) {
    fixp dx = x - gs.own.x;
    fixp dz = z - gs.own.z;
    int32_t sqdist = dx*dx+dz*dz;

    // thunder within view distance - ignore local thunder
    if (sqdist < VIEWDIST*VIEWDIST) return;

    float scale = (float)THUNDERDIST / sqrtf((float)sqdist);
    fixp rx = (fixp)((float)dx*scale)+gs.own.x;
    fixp rz = (fixp)((float)dz*scale)+gs.own.z;

    // process output with ./mcpdump | egrep '^thunder' | sed 's/thunder: //' > output.csv
    printf("thunder: %ld,%d,%d,%d,%d,%d\n",tv.tv_sec,gs.own.x>>5,gs.own.z>>5,rx>>5,rz>>5,y);
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
    nbt_t * sType = nbt_hget(ube->nbt, "EntityId");
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
    int ts[4096][3], mts=0;

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

#define MAXPLEN (4*1024*1024)

void mcpd_packet(MCPacket *pkt) {
    switch (pkt->pid) {
        case SP_UpdateBlockEntity: {
            track_spawners(&pkt->_SP_UpdateBlockEntity);
            break;
        }

#if 1
        case SP_SoundEffect: {
            SP_SoundEffect_pkt *tpkt = (SP_SoundEffect_pkt *)&pkt->_SP_SoundEffect;
            if (!strcmp(tpkt->name,"ambient.weather.thunder")) {
                fixp tx = tpkt->x*4;
                fixp tz = tpkt->z*4;
                track_remote_sounds(tx, tz, tpkt->y/8, pkt->ts);
            }
            break;
        }
#endif

#if 0
        case SP_ChunkData: {
            SP_ChunkData_pkt *tpkt = (SP_ChunkData_pkt *)&pkt->_SP_ChunkData;
            if (tpkt->chunk.mask) {
                lh_save("pkt_original",pkt->raw, pkt->rawlen);
                uint8_t buf[MAXPLEN];
                ssize_t sz = encode_packet(pkt, buf);
                lh_save("pkt_encoded",buf+1, sz-1); //skip the packet ID that encode_packet adds
                pkt->modified = 1;
                sz = encode_packet(pkt, buf);
                lh_save("pkt_reencoded",buf+1, sz-1);
                exit(0);
            }
            break;
        }
#endif

#if 0
        case SP_MapChunkBulk: {
            SP_MapChunkBulk_pkt *tpkt = (SP_MapChunkBulk_pkt *)&pkt->_SP_MapChunkBulk;
            lh_save("pkt_original",pkt->raw, pkt->rawlen);
            uint8_t buf[MAXPLEN];
            ssize_t sz = encode_packet(pkt, buf);
            lh_save("pkt_encoded",buf+1, sz-1); //skip the packet ID that encode_packet adds
            pkt->modified = 1;
            sz = encode_packet(pkt, buf);
            lh_save("pkt_reencoded",buf+1, sz-1);
            exit(0);
            break;
        }
#endif

#if 0
        case SP_MultiBlockChange: {
            SP_MultiBlockChange_pkt *tpkt = (SP_MultiBlockChange_pkt *)&pkt->_SP_MultiBlockChange;
            lh_save("pkt_original",pkt->raw, pkt->rawlen);
            uint8_t buf[MAXPLEN];
            ssize_t sz = encode_packet(pkt, buf);
            lh_save("pkt_encoded",buf+1, sz-1); //skip the packet ID that encode_packet adds
            pkt->modified = 1;
            sz = encode_packet(pkt, buf);
            lh_save("pkt_reencoded",buf+1, sz-1);
            exit(0);
            break;
        }
#endif
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
            //dump_packet(pkt);
            //gs_packet(pkt);
            mcpd_packet(pkt);
            free_packet(pkt);
        }

        uint8_t *ptr = p;
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

            case SP_SetCompression:
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

// print a single chunk slice (16x16x1 blocks) on the screen using ANSI_COLORS
static void print_slice(bid_t * data,int Xs, int Zs) {
    int x,z;
    for(z=0; z<16*Zs; z++) {
        printf("%s%3d ",ANSI_CLEAR,z);
        bid_t * row = data+z*(Xs*16);
        for(x=0; x<Xs*16; x++)
            printf("%s",(row[x].bid<256) ?
                   ANSI_BLOCK[row[x].bid] : ANSI_ILLBLOCK );
        printf("%s\n",ANSI_CLEAR);
    }
}

void extract_cuboid(int X, int Z, int y) {
    int Xs=5,Zs=5;
    bid_t * map = export_cuboid(X,Xs,Z,Zs,y,1,NULL);
    //hexdump((char *)map,512);
    printf("Slice y=%d\n",y);
    print_slice(map,Xs,Zs);
    free(map);
}

////////////////////////////////////////////////////////////////////////////////

int main(int ac, char **av) {

#if 0
    int bid  = atoi(av[1]);
    int meta = atoi(av[2]);
    int rot  = atoi(av[3]);

    bid_t b  = BLOCKTYPE(bid,meta);
    bid_t bb = rotate_meta(b,rot);

    printf("rot=%d, %d:%d => %d:%d\n",
           rot, b.bid, b.meta, bb.bid, bb.meta);
#endif

#if 1
    if (!av[1]) LH_ERROR(-1, "Usage: %s <file.mcs>", av[0]);

    int i;
    for(i=1; av[i]; i++) {
        uint8_t *data;
        ssize_t size = lh_load_alloc(av[i], &data);
        if (size < 0) {
            fprintf(stderr,"Failed to open %s : %s",av[i],strerror(errno));
            //break;
        }
        else {
            gs_reset();
            gs_setopt(GSOP_PRUNE_CHUNKS, 0);
            gs_setopt(GSOP_SEARCH_SPAWNERS, 1);

            parse_mcp(data, size);
            //dump_inventory();
            //dump_entities();

            free(data);
            gs_destroy();
        }
    }

#if 0
    for(i=0; i<C(spawners); i++) {
        spawner_t *s = P(spawners)+i;
        printf("%c %5d,%2d,%5d\n",s->type==SPAWNER_ZOMBIE?'Z':'S',
               s->loc.x,s->loc.y,s->loc.z);
    }
#endif

    find_spawners();
#endif
}
