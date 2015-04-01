#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
            dump_packet(pkt);
            gs_packet(pkt);
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
            //extract_cuboid(918,3048,64);

            free(data);
            gs_destroy();
        }
    }
}
