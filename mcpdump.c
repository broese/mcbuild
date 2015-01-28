#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_arr.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_compress.h>

#include "mcp_gamestate.h"
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
    while(hdr-data < (size-16) && max>0) {
        max--;
        uint8_t *p = hdr;

        Rint(is_client);
        Rint(sec);
        Rint(usec);
        Rint(len);

        printf("%d.%06d: len=%d\n",sec,usec,len);
        uint8_t *lim = p+len;
        if (lim > data+size) {printf("incomplete packet\n"); break;}

        if (compression) {
            // compression was enabled previously - we need to handle packets differently now
            Rvarint(usize); // size of the uncompressed data
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
        printf("%d.%06d: %c %c type=?? plen=%6zd    ",sec,usec,is_client?'C':'S',states[state],plen);
        hexprint(p, (plen>64)?64:plen);

        if (state == STATE_PLAY) {
            MCPacket *pkt = decode_packet(is_client, p, plen);

            printf("MCPacket @%p:\n",pkt);
            printf("  type =%08x\n",pkt->type);
            printf("  proto=%08x\n",pkt->protocol);
            printf("    data=%p, len=%zd\n",pkt->p_UnknownPacket.data,pkt->p_UnknownPacket.length);

            hexdump(pkt->p_UnknownPacket.data,pkt->p_UnknownPacket.length);
            printf("--------------------------------------------------------------------------------\n");
        }

        uint8_t *ptr = p;
        // pkt will point at the start of packet (specifically at the type field)
        // while p will move to the next field to be parsed.

        Rvarint(type);
        //printf("%d.%06d: %c type=%x len=%zd\n",sec,usec,is_client?'C':'S',type,len);

        uint32_t stype = ((state<<24)|(is_client<<28)|(type&0xffffff));

        
        //if (state == STATE_PLAY)
        //    import_packet(pkt, len, is_client);

        switch (stype) {
            case CI_Handshake: {
                Rvarint(protocolVer);
                Rstr(serverAddr);
                Rshort(serverPort);
                Rvarint(nextState);
                state = nextState;
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

#if 0
            // PLAY    
            case SP_SpawnPlayer: {
                Rint(eid);
                Rstr(uuid);
                char name[4096]; p=read_string(p,name);

                Rvarint(dcount);

                printf("%d.%06d: Spawn Player  eid=%d uuid=%s\n",sec,usec,eid,uuid);
                break;
            }
            case SP_Effect: {
                Rint(efid);
                Rint(x);
                Rchar(y);
                Rint(z);
                Rint(data);
                Rchar(disrv);
                if (efid == 1013 || efid == 1014)
                    printf("%d.%06d: Effect  efid=%d data=%d bcoord=%d:%d:%d %s\n",
                           sec,usec,efid,data,x,y,z,disrv?"disable relative volume":"");
                break;
            }
            case SP_SoundEffect: {
                Rstr(name);
                Rint(x);
                Rint(y);
                Rint(z);
                Rfloat(volume);
                Rchar(pitch);
                if (!strcmp(name,"ambient.weather.thunder"))
                    printf("%d.%06d: Sound Effect  name=%s bcoord=%d:%d:%d vol=%.3f pitch=%d\n",
                           sec,usec,name,(int)x/8,(int)y/8,(int)z/8,volume,pitch);
                break;
            }
            case SP_Maps: {
                Rvarint(damage);
                Rshort(length);
                printf("%d.%06d: Maps  dmg=%d length=%d\n",sec,usec,damage,length);
                break;
            }

            case SP_SetSlot: {
                Rchar(wid);
                // TODO: support other windows
                if (wid != 0) break;
                Rshort(sid);
                Rslot(s);
                printf("SetSlot wid=%d sid=%d iid=%d count=%d dmg=%d",
                       wid, sid, s.id, s.count, s.damage);
                if (s.dlen == 0 || s.dlen == 0xffff) {
                    printf(" no aux data\n");
                }
                else {
                    printf(" %d bytes of aux data\n", s.dlen);
                }
                break;
            }

            case SP_HeldItemChange: {
                Rchar(sid);
                gs.held = sid;
                printf("HeldItemChange (S) sid=%d\n",sid);
                break;
            }
                
            case CP_HeldItemChange: {
                Rshort(sid);
                gs.held = sid;
                printf("HeldItemChange (C) sid=%d\n",sid);
                break;
            }

            case CP_ChatMessage: {
                Rstr(msg);
                printf("ChatMessage: %s\n",msg);
                break;
            }

            case SP_WindowItems: {
                Rchar(wid);
                Rshort(nslots);

                int i;
                printf("WindowItems : %d slots\n",nslots);
                for(i=0; i<nslots; i++) {
                    Rslot(s);
                    printf("  %2d: iid=%-3d count=%-2d dmg=%-5d dlen=%d bytes\n", i, s.id, s.count, s.damage, s.dlen);
                    if (s.dlen!=0 && s.dlen!=0xffff) {
                        uint8_t buf[256*1024];
                        ssize_t olen = lh_gzip_decode_to(s.data, s.dlen, buf, sizeof(buf));
                        //if (olen > 0) hexdump(buf, 128);
                    }
                }
                break;
            }

            case SP_OpenWindow: {
                Rchar(wid);
                Rchar(invtype);
                Rstr(title);
                Rshort(nslots);
                Rchar(usetitle);
                printf("OpenWindow: wid=%d type=%d title=%s nslots=%d usetitle=%d\n",
                       wid, invtype, title, nslots, usetitle);
                break;
            }

            case SP_CloseWindow: {
                Rchar(wid);
                printf("CloseWindow (S): wid=%d\n", wid);
                break;
            }

            case CP_CloseWindow: {
                Rchar(wid);
                printf("CloseWindow (C): wid=%d\n", wid);
                break;
            }

            case SP_ConfirmTransaction: {
                Rchar(wid);
                Rshort(action);
                Rchar(accepted);
                printf("ConfirmTransaction: wid=%d action=%04x accepted=%d\n",wid,action,accepted);
                break;
            }

            case CP_ClickWindow: {
                Rchar(wid);
                Rshort(sid);
                Rchar(button);
                Rshort(action);
                Rchar(mode);
                Rslot(s);

                printf("ClickWindow: wid=%d action=%04x sid=%d mode=%d button=%d\n",wid, action, (short)sid, mode, button);
                break;
            }
#endif
        }
    
        hdr += 16+len; // advance header pointer to the next packet
    }
}

////////////////////////////////////////////////////////////////////////////////
// Parser for pre-1.6 protocol format

uint8_t * read_string_old(uint8_t *p, char *s) {
    uint16_t len = lh_read_short_be(p);
    int i,j=0;
    for(i=0; i<len; i++) {
        uint16_t c = lh_read_short_be(p);
        s[j++] = (uint8_t)c;
    }
    s[j++] = 0;
    return p;
}

#define Rstro(n) char n[4096]; p=read_string_old(p,n)

void parse_mcp_old(uint8_t *data, ssize_t size) {
    uint8_t *p = data;
    while(p-data < (size-16)) {
        uint8_t *pkts = p;
        Rint(is_client);
        Rint(sec);
        Rint(usec);
        Rint(len);
        //printf("%d.%06d: len=%d\n",sec,usec,len);
        if (p-data > (size-len)) {printf("incomplete packet\n"); break;}

        uint8_t *pkt = p;
        Rvarint(type);
        //printf("%d.%06d: %c  type=%x\n",sec,usec,is_client?'C':'S',type);

        if (!is_client) {
            switch (type) {
#if 0
                case 0x14: {
                    //hexdump(pkts, len+16); exit(0);

                    Rint(eid);
                    Rstro(name);
                    Rint(x);
                    Rint(y);
                    Rint(z);
                    Rchar(yaw);
                    Rchar(pitch);
                    Rshort(item);
                    //TODO: metadata
                    printf("%d.%06d: Spawn Player  eid=%d name=%s coords=%d,%d,%d\n",sec,usec,eid,name,(int)x>>5,(int)y>>5,(int)z>>5);
                    break;
                }
                case 0x28: {
                    Rint(efid);
                    Rint(x);
                    Rchar(y);
                    Rint(z);
                    Rint(data);
                    Rchar(disrv);
                    printf("%d.%06d: Effect  efid=%d data=%d bcoord=%d:%d:%d %s\n",
                           sec,usec,efid,data,x,y,z,disrv?"disable relative volume":"");
                    break;
                }
#endif
                case 0x3e: {
                    //hexdump(pkts, len+16); exit(0);

                    Rstro(name);
                    Rint(x);
                    Rint(y);
                    Rint(z);
                    Rfloat(volume);
                    Rchar(pitch);
                    if (!strcmp(name,"ambient.weather.thunder") && abs((int)x)>8000 && abs((int)z)>8000)
                        //printf("%d.%06d: Sound Effect  name=%s bcoord=%d:%d:%d vol=%.3f pitch=%d\n", sec,usec,name,(int)x/8,(int)y/8,(int)z/8,volume,pitch);
                        printf("%d,%d,%d\n",sec,(int)x/8,(int)z/8);
                    break;
                }
            }
        }
        
        p = pkt+len;
    }
}

////////////////////////////////////////////////////////////////////////////////

void search_blocks(uint8_t id) {
    int i,j;
    switch_dimension(DIM_END);
    printf("Current: %zd, Overworld: %zd, Nether: %zd, End: %zd\n",
           gs.C(chunk),gs.C(chunko),gs.C(chunkn),gs.C(chunke));

    printf("searching through %zd chunks\n",gs.C(chunko));

    for(i=0; i<gs.C(chunko); i++) {
        int X = P(gs.chunko)[i].X;
        int Z = P(gs.chunko)[i].Z;
        chunk *c = P(gs.chunko)[i].c;
        for(j=0; j<65536; j++) {
            if (c->blocks[j] == id) {
                printf("%d,%d\n",X,Z);
                break;
            }
        }
    }
}

int main(int ac, char **av) {
    reset_gamestate();
    set_option(GSOP_PRUNE_CHUNKS, 0);
    set_option(GSOP_SEARCH_SPAWNERS, 1);

    if (!av[1]) LH_ERROR(-1, "Usage: %s <file.mcs>", av[0]);

    int i;
    for(i=1; av[i]; i++) {
        uint8_t *data;
        ssize_t size = lh_load_alloc(av[i], &data);
        if (size < 0) {
            fprintf(stderr,"Failed to open %s : %s",av[i],strerror(errno));
        }
        else {
            parse_mcp(data, size);
            free(data);
        }
    }
    //search_spawners();
    //search_blocks(0xa2);
}
