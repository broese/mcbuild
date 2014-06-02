#include "mcp_game.h"

#include <math.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_bytes.h"

////////////////////////////////////////////////////////////////////////////////

void chat_message(const char *str, lh_buf_t *buf, const char *color) {
    uint8_t jreply[32768];
    ssize_t jlen = sprintf(jreply,
                           "{"
                           "\"text\":\"[MCP] %s\","
                           "\"color\":\"%s\""
                           "}",str,color?color:"red");
    uint8_t wbuf[65536];
    uint8_t *wp = wbuf;
    write_varint(wp, 0x02);
    write_varint(wp, jlen);
    memcpy(wp,jreply,jlen);
    write_packet(wbuf, (wp-wbuf)+jlen, buf);
}

int cprintf(lh_buf_t *buf, const char *color, const char *fmt, ...) {
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Hole Radar

#define HR_DISTC 2
#define HR_DISTB (HR_DISTC<<4)

void hole_radar(lh_buf_t *client) {
    int x = gs.own.x>>5;
    int y = (gs.own.y>>5)-1;
    int z = gs.own.z>>5;

    int lx=-(int)(65536*sin(gs.own.yaw/180*M_PI));
    int lz=(int)(65536*cos(gs.own.yaw/180*M_PI));

    int X=x>>4;
    int Z=z>>4;

    uint8_t * data;
    int off,sh;
    if (abs(lx) > abs(lz)) {
        lz=0;
        // looking into east or west direction
        if (lx<0) {
            //to west
            lx=-1;
            data = export_cuboid(X-HR_DISTC,X,Z,Z,y,y);
            off = (x&0x0f)+HR_DISTB+(z&0x0f)*(HR_DISTB+16);
        }
        else {
            // east
            lx=1;
            data = export_cuboid(X,X+HR_DISTC,Z,Z,y,y);
            off = (x&0x0f)+(z&0x0f)*(HR_DISTB+16);
        }
        sh=lx;
    }
    else {
        lx=0;
        // looking into north or south direction
        if (lz<0) {
            //to north
            lz=-1;
            data = export_cuboid(X,X,Z-HR_DISTC,Z,y,y);
            off = (x&0x0f)+((z&0x0f)+HR_DISTB)*16;
        }
        else {
            // south
            lz=1;
            data = export_cuboid(X,X,Z,Z+HR_DISTC,y,y);
            off = (x&0x0f)+(z&0x0f)*16;
        }
        sh = 16*lz;
    }

    int i;
    for(i=1; i<=HR_DISTB; i++) {
        off+=sh;
        if (data[off]==0) {
            char reply[32768];
            sprintf(reply, "*** HOLE *** : %d,%d d=%d",x+lx*i,z+lz*i,i);
            chat_message(reply, client, NULL);
            break;
        }
    }

    free(data);    
}

////////////////////////////////////////////////////////////////////////////////
// Find Tunnels

#define TUNNEL_TRESHOLD 0.4

void find_tunnels(lh_buf_t *answer, int l, int h) {
    int i;

    int Xl,Xh,Zl,Zh;
    int nc = get_chunks_dim(&Xl,&Xh,&Zl,&Zh);
    uint8_t *data = export_cuboid(Xl,Xh,Zl,Zh,l,h);

    int xsz = (Xh-Xl+1)*16;
    int zsz = (Zh-Zl+1)*16;
    int ysz = (h-l+1);

    char reply[32768];
    //sprintf(reply,"got %d chunks (%d,%d) to (%d,%d)",nc,Xl,Zl,Xh,Zh);
    //chat_message(reply, answer, "blue");

    // search for tunnels

    for(i=l; i<=h; i++) {
        uint8_t * slice = data+(i-l)*xsz*zsz;

        int x,z;
        for(x=0; x<xsz; x++) {
            int nair=0, nfilled=0;
            uint8_t * p=slice+x;
            for(z=0; z<zsz; z++) {
                if (*p!=0xff) {
                    if (*p)
                        nfilled++;
                    else
                        nair++;
                }
                p += xsz;
            }

            //printf("level=%d x=%d : %d/%d\n",i,x+Xl*16,nair,nfilled);
            if ((float)(nair) > (float)(nair+nfilled)*TUNNEL_TRESHOLD) {
                sprintf(reply,"Tunnel NS at level=%d x=%d  (%d/%d)",i,x+Xl*16,nair,nfilled);
                chat_message(reply, answer, "yellow");
            }
        }
    }

    for(i=l; i<=h; i++) {
        uint8_t * slice = data+(i-l)*xsz*zsz;
        int x,z;

        for(z=0; z<zsz; z++) {
            int nair=0, nfilled=0;
            uint8_t * p=slice+z*xsz;

            for(x=0; x<xsz; x++) {
                if (*p!=0xff) {
                    if (*p)
                        nfilled++;
                    else
                        nair++;
                }
                p++;
            }

            //printf("level=%d x=%d : %d/%d\n",i,x+Xl*16,nair,nfilled);
            if ((float)(nair) > (float)(nair+nfilled)*TUNNEL_TRESHOLD) {
                sprintf(reply,"Tunnel WE at level=%d z=%d  (%d/%d)",i,z+Zl*16,nair,nfilled);
                chat_message(reply, answer, "yellow");
            }
        }
    }

    free(data);
}


