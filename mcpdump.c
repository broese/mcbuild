#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_files.h>

#include "gamestate.h"

uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p);

#define Rchar(n)  Rx(n,uint8_t,char)
#define Rshort(n) Rx(n,uint16_t,short)
#define Rint(n)   Rx(n,uint32_t,int)
#define Rlong(n)  Rx(n,uint64_t,long);
#define Rfloat(n) Rx(n,float,float)
#define Rdouble(n) Rx(n,double,double)
#define Rstr(n)   char n[4096]; p=read_string(p,n)
#define Rskip(n)  p+=n;
#define Rvarint(n) uint32_t n = lh_read_varint(p);



void parse_mcp(uint8_t *data, ssize_t size) {
    uint8_t *p = data;
    while(p-data < (size-16)) {
        Rint(is_client);
        Rint(sec);
        Rint(usec);
        Rint(len);
        //printf("%d.%06d: len=%d\n",sec,usec,len);
        if (p-data > (size-len)) {printf("incomplete packet\n"); break;}

        uint8_t *pkt = p;
        Rvarint(type);
        printf("%d.%06d: %c  type=%x\n",sec,usec,is_client?'C':'S',type);

        if (!is_client) {
            switch (type) {
                case 0x0a: {
                    Rint(eid);
                    Rint(X);
                    Rchar(Y);
                    Rint(Z);
                    //printf("%d.%06d: Use Bed  eid=%d bcoord=%d:%d:%d\n",sec,usec,eid,X,Y,Z);
                    break;
                }
                case 0x0c: {
                    Rint(eid);
                    Rstr(uuid);
                    char name[4096]; p=read_string(p,name);

                    Rvarint(dcount);

                    printf("%d.%06d: Spawn Player  eid=%d uuid=%s\n",sec,usec,eid,uuid);
                    break;
                }
                case 0x28: {
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
                case 0x29: {
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
                case 0x34: {
                    Rvarint(damage);
                    Rshort(length);
                    //printf("%d.%06d: Maps  dmg=%d length=%d\n",sec,usec,damage,length);
                    break;
                }
                case 0x21: // Chunk Data
                case 0x23: // Block Change
                case 0x26: // Map Chunk Bulk
                case 0x35: // Update Block Entity
                    import_packet(pkt, len);
                    break;
            }
        }
        
        p = pkt+len;
    }
}

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


void parse_mcp2(uint8_t *data, ssize_t size) {
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
    search_spawners();
}
