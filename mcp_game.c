#include <math.h>
#include <time.h>
#include <unistd.h>

#define LH_DECLARE_SHORT_NAMES 1

#include "lh_bytes.h"

#include "mcp_game.h"
#include "blocks_ansi.h"

// Various options
struct {
    int autokill;
    
    int grinding;
    int maxlevel;
    
    int holeradar;
    
    int build;
} opt;

// used for detecting player position changes
int hr_last_x;
int hr_last_y;
int hr_last_z;
int hr_last_yaw;

////////////////////////////////////////////////////////////////////////////////

uint64_t gettimestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t ts = (uint64_t)tv.tv_sec*1000000+(uint64_t)tv.tv_usec;
}

#define MIN(a,b) ((a<b)?(a):(b))
#define MAX(a,b) ((a>b)?(a):(b))
#define SGN(x) (((x)>=0)?1:-1)
#define SQ(x) ((x)*(x))

////////////////////////////////////////////////////////////////////////////////

void write_packet(uint8_t *ptr, ssize_t len, lh_buf_t *buf) {
    uint8_t hbuf[16]; CLEAR(hbuf);
    ssize_t ll = lh_place_varint(hbuf,len) - hbuf;

    ssize_t widx = buf->C(data);
    
    lh_arr_add(GAR4(buf->data),(len+ll));

    memmove(P(buf->data)+widx, hbuf, ll);
    memmove(P(buf->data)+widx+ll, ptr, len);
}

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
// Autokill

#define MAX_ENTITIES     4096
#define MAX_ATTACK       1
#define MIN_ENTITY_DELAY 250000  // minimum interval between hitting the same entity (us)
#define MIN_ATTACK_DELAY  50000  // minimum interval between attacking any entity

int is_hostile_entity(entity *e) {
    return e->hostile > 0;
}

void autokill(lh_buf_t *server) {
    uint64_t ts = gettimestamp();
    if ((ts-gs.last_attack)<MIN_ATTACK_DELAY) return;

    // calculate list of hostile entities in range
    int hent[MAX_ENTITIES];

    //TODO: sort entities by how dangerous and how close they are
    int nhent = get_entities_in_range(hent,MAX_ENTITIES,5.0,is_hostile_entity,NULL);

    //if (nhent > 0) printf("%s : got %d entities to kill\n",__func__,nhent);
    
    //TODO: select primary weapon for priority targets

    //TODO: turn to target?

    int i,h;
    for(i=0,h=0; h<MAX_ATTACK && i<nhent; i++) {
        entity *e = gs.P(entity)+hent[i];
        if ((ts-e->lasthit) < MIN_ATTACK_DELAY)
            continue;
        
        uint8_t pkt[4096], *p;
        
        //printf("%lld : Attack entity %d\n", ts, e->id);
        
        // attack entity
        p = pkt;
        write_varint(p,0x02); // Use Entity
        write_int(p, e->id);
        write_char(p, 0x01);
        write_packet(pkt, p-pkt, server);
        
        // wave arm
        p = pkt;
        write_varint(p,0x0a); // Animation
        write_int(p,gs.own.id);
        write_char(p,0x01);
        write_packet(pkt, p-pkt, server);
        
        e->lasthit = ts;
        gs.last_attack = ts;
        h++;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Autobuild

#define MIN_BUILD_DELAY  200000
#define REACH_RANGE 5

typedef struct {
    int x,y,z;          // block coordinates
    int v;              // type of placement, only applicable to slabs: 'u'=upper, 'd'=bottom, 'm'=default
    int id;             // block type ID
    uint8_t dir;        // 
    uint8_t cx,cy,cz;   // cursor x,y,z
} bblock;

struct {
    int active;                 // building is in progress
    uint64_t last_placement;    // timestamp when the most recent block was placed
    lh_arr_declare(bblock,all);     // all blocks to be built - init by chat command
    lh_arr_declare(bblock,inreach); // all blocks in reach - updated by player position change
    int last_x, last_y, last_z;
} build;

static inline int sqdist(int x, int y, int z, int x2, int y2, int z2) {
    return SQ(x-x2)+SQ(y-y2)+SQ(z-z2);
}

static inline int is_solid(int type) {
    return !(type==0 ||                  // air
             type==0x08 || type==0x09 || // water
             type==0x0a || type==0x0b || // lava
             type==0x1f ||               // tallgrass  
             type==0x06 ||               // saplings
             type==0x33                  // fire
             );
}

void autobuild(lh_buf_t *server) {
    uint64_t ts = gettimestamp();
    if ((ts-build.last_placement)<MIN_BUILD_DELAY) return;

    if (C(build.inreach)<=0) return;

    int idx = random()%C(build.inreach);

    bblock *bb = P(build.inreach)+idx;

    printf("Placing at %d,%d,%d\n",bb->x,bb->y,bb->z);
        
    uint8_t pkt[4096], *p;
        
    // place block
    p = pkt;
    write_varint(p,0x08); // PlayerBlockPlacement

    switch (bb->dir) {
        case 0: //UP
            write_int(p,bb->x);         
            write_char(p,(char)bb->y+1);
            write_int(p,bb->z);
            break;
        case 1: //DOWN
            write_int(p,bb->x);         
            write_char(p,(char)bb->y-1);
            write_int(p,bb->z);
            break;
        case 2: //SOUTH
            write_int(p,bb->x);         
            write_char(p,(char)bb->y);
            write_int(p,bb->z+1);
            break;
        case 3: //NORTH
            write_int(p,bb->x);         
            write_char(p,(char)bb->y);
            write_int(p,bb->z-1);
            break;
        case 4: //EAST
            write_int(p,bb->x+1);         
            write_char(p,(char)bb->y);
            write_int(p,bb->z);
            break;
        case 5: //WEST
            write_int(p,bb->x-1);         
            write_char(p,(char)bb->y);
            write_int(p,bb->z);
            break;
    }
    
    write_char(p,bb->dir);      // direction

    // TODO: take slot data from the appropriate slot

#if 0
    write_short(p,87);          // netherrack
    write_char(p,1);            // count
    write_short(p,0);           // damage
#endif
    write_short(p,0xffff);      // no data

    int cx = bb->cx; //if (cx==8) cx+=((random()%7)-4);
    int cy = bb->cy; //if (cy==8) cy+=((random()%7)-4);
    int cz = bb->cz; //if (cz==8) cz+=((random()%7)-4);
    
    write_char(p,cx);       // cursor position
    write_char(p,cy);       
    write_char(p,cz);        
    write_packet(pkt, p-pkt, server);
    
    // wave arm
    p = pkt;
    write_varint(p,0x0a); // Animation
    write_int(p,gs.own.id);
    write_char(p,0x01);
    write_packet(pkt, p-pkt, server);
    
    build.last_placement = ts;
}

void clear_autobuild() {
    lh_free(GAR(build.all));
    lh_free(GAR(build.inreach));
    lh_clear_obj(build);
}

void build_request(char **words, lh_buf_t *client) {
    char reply[32768];
    reply[0]=0;

    if (!words[1]) {
        sprintf(reply, "Usage: build <type> [ parameters ... ] or build cancel");
    }
    else if (!strcmp(words[1], "cancel")) {
        clear_autobuild();
        sprintf(reply, "Current build canceled");
        opt.build = 0;
    }
    else if (!strcmp(words[1], "floor")) {
        int xsize,zsize;
        if (!words[2] || !words[3] || 
            sscanf(words[2],"%d",&xsize)!=1 || 
            sscanf(words[3],"%d",&zsize)!=1 ) {
            sprintf(reply, "Usage: build floor <xsize> <zsize> [ypos] [u|d]");
        }
        else {
            int x = gs.own.x>>5;
            int y = (gs.own.y>>5)-1; // coords of the block just below your feet
            int z = gs.own.z>>5;

            if (words[4]) sscanf(words[4],"%d",&y);
            
            char slab = 'm';
            if (words[5]) {
                if (sscanf(words[5],"%c",&slab)!=1) {
                    sprintf(reply, "Usage: build floor <xsize> <zsize> [ypos] [u|d]");
                    chat_message(reply, client, NULL);
                    return;
                }
                if (slab != 'u' && slab != 'd') {
                    sprintf(reply, "Usage: build floor <xsize> <zsize> [ypos] [u|d]");
                    chat_message(reply, client, NULL);
                    return;
                }
            }   

            clear_autobuild(); // cancel any current build

            int i,j;
            int dx = SGN(xsize);
            int dz = SGN(zsize);
            int n=0;

            for(i=0; i<abs(xsize); i++) {
                for(j=0; j<abs(zsize); j++) {
                    bblock * bb = lh_arr_new(GAR(build.all));
                    bb->x = x+i*dx;
                    bb->y = y;
                    bb->z = z+j*dz;
                    bb->v = slab;
                    //TODO: block ID
                    //printf("Scheduled block at %d,%d,%d\n",bb->x,bb->y,bb->z);
                    n++;
                }
            }
            sprintf(reply,"Scheduled %d blocks to build",n);
            opt.build = 1;
        }        
    }

    if (reply[0])
        chat_message(reply, client, NULL);
}

void build_process(lh_buf_t *client) {
    lh_arr_free(GAR(build.inreach));

    // get a cuboid in range
    int x = gs.own.x>>5;
    int y = (gs.own.y>>5)+1;
    int z = gs.own.z>>5;

    int Xl = (x>>4)-1;
    int Xh = Xl+2;
    int xoff = Xl*16;

    int Zl = (z>>4)-1;
    int Zh = Zl+2;
    int zoff = Zl*16;

    int yl = MAX(y-REACH_RANGE-1,1);
    int yh = MIN(y+REACH_RANGE+1,255);

    uint8_t *data = export_cuboid(Xl,Xh,Zl,Zh,yl,yh);
    
    // detect which blocks are OK to place in

    int i,n=0;
    for(i=0; i<C(build.all); i++) {
        bblock * bb = P(build.all)+i;

        if (bb->x == x && bb->z == z && (bb->y == y || bb->y == y-1))
            continue; // don't try to build at our own position

        if (sqdist(x,y,z,bb->x,bb->y,bb->z) > SQ(REACH_RANGE))
            continue; // this block is too far away

        int cx = bb->x-xoff;
        int cz = bb->z-zoff;

        uint8_t *ydata = data+48*48*(bb->y-yl);
        uint8_t *b = ydata+cx+cz*48;

        // is the block type suitable to build in ?
        if (!is_solid(*b)) {
            bb->dir = 255;

            // find direction from which the block can be built
            if (is_solid(b[48*48]) && bb->v!='d')         {  // U
                bb->dir = 0; bb->cx=8;  bb->cy=0;  bb->cz=8;
            }
            else if (is_solid(b[-(48*48)]) && bb->v!='u') {  // D
                bb->dir = 1; bb->cx=8;  bb->cy=16; bb->cz=8;
            }
            else if (is_solid(b[48]))       { bb->dir = 2; bb->cx=8;  bb->cy=8;  bb->cz=0;  } // S
            else if (is_solid(b[-48]))      { bb->dir = 3; bb->cx=8;  bb->cy=8;  bb->cz=16; } // N
            else if (is_solid(b[1]))        { bb->dir = 4; bb->cx=0;  bb->cy=8;  bb->cz=8;  } // E
            else if (is_solid(b[-1]))       { bb->dir = 5; bb->cx=16; bb->cy=8;  bb->cz=8;  } // W
            else { continue; }
            
            // adjust the block placement for the slabs
            if ( bb->cy == 8) {
                if (bb->v == 'u') bb->cy = 12;
                if (bb->v == 'd') bb->cy = 4;
            }


            // schedule it for handle_async
            bblock * ir = lh_arr_new(GAR(build.inreach));
            *ir = *bb;
            n++;
        }
    }

#if 0
    char reply[32768];
    sprintf(reply,"Scheduled %d blocks for handle_async",n);
    chat_message(reply,client,NULL);
#endif

    build.active = (n>0);

    free(data);
}

////////////////////////////////////////////////////////////////////////////////

int handle_async(lh_buf_t *client, lh_buf_t *server) {
    if (opt.autokill) autokill(server);
    autobuild(server);    
    return 0;
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

void salt_request(lh_buf_t *server, int x, int y, int z) {
    uint8_t pkt[4096], *p;

    // place block
    p = pkt;
    write_varint(p,0x08); // PlayerBlockPlacement
    write_int(p,x);
    write_char(p,y);
    write_int(p,z);
    write_char(p,4);
    write_short(p,0xffff); //TODO: proper slot data
    write_char(p,1);       
    write_char(p,7);       
    write_char(p,4);        
    write_packet(pkt, p-pkt, server);

    // wave arm
    p = pkt;
    write_varint(p,0x0a); // Animation
    write_int(p,gs.own.id);
    write_char(p,0x01);
    write_packet(pkt, p-pkt, server);
}

////////////////////////////////////////////////////////////////////////////////

int process_message(const char *msg, lh_buf_t *forw, lh_buf_t *retour) {
    if (msg[0] != '#') return 0;

    // tokenize
    char *words[256];
    CLEAR(words);
    int w=0;

    char wbuf[4096];
    strncpy(wbuf, msg+1, sizeof(wbuf));
    char *wsave;

    char *wstr = wbuf;
    do {
        words[w++] = strtok_r(wstr, " ", &wsave);
        wstr = NULL;
    } while(words[w-1]);
    w--;

    if (w==0) return 0;

    uint8_t reply[32768];
    reply[0] = 0;

    if (!strcmp(words[0],"test")) {
        sprintf(reply,"Chat test response");
    }
    else if (!strcmp(words[0],"entities")) {
        sprintf(reply,"Tracking %zd entities",gs.C(entity));
    }
    else if (!strcmp(words[0],"autokill")) {
        opt.autokill = !opt.autokill;
        sprintf(reply,"Autokill is %s",opt.autokill?"enabled":"disabled");
    }
    else if (!strcmp(words[0],"grind")) {
        int maxlevel=30;
        int start = 1;

        if (words[1]) {
            if (!strcmp(words[1],"stop")) {
                opt.grinding = 0;
                opt.autokill = 0;
                sprintf(reply,"Grinding is stopped");
                start = 0;
            }
            else if (sscanf(words[1],"%d",&maxlevel)!=1) {
                maxlevel = 30;
            }
        }

        if (start) {
            opt.grinding = 1;
            opt.autokill = 1;
            opt.maxlevel = maxlevel;
            sprintf(reply,"Grinding to level %d",opt.maxlevel);
        }
    }
    else if (!strcmp(words[0],"ft")) {
        int ll = 119;
        int lh = 122;
        int error = 0;
        if (words[1]) {
            if (sscanf(words[1],"%d",&ll)!=1)
                error++;

            if (words[2]) {
                if (sscanf(words[2],"%d",&lh)!=1)
                    error++;
            }
        }

        if (ll<0 || lh<0 || lh<ll || lh>((gs.current_dimension==DIM_NETHER)?126:254))
            error++;

        if (error)
            sprintf(reply, "incorrect level range specified");
        else
            find_tunnels(retour,ll,lh);
    }
    else if (!strcmp(words[0],"map")) {
        int error=0;
        int y = gs.own.y>>5;

        if (words[1]) {
            if (sscanf(words[1],"%d",&y)!=1)
                error++;
        }

        if (error) {
            sprintf(reply, "incorrect level specified");
        }
        else {
            int xp = gs.own.x>>5;
            int zp = gs.own.z>>5;
            int X = gs.own.x>>9;
            int Z = gs.own.z>>9;
            uint8_t *map = export_cuboid(X-5,X+5,Z-5,Z+5,y,y);

            int xoff = (X-5)*16;
            int zoff = (Z-5)*16;

            int x,z;
            printf("MAP x=%d:%d z=%d:%d y=%d\n",(X-5)*16,(X+6)*16-1,(Z-5)*16,(Z+6)*16-1,y);
            for(z=(5*16); z<(7*16); z++) {
                uint8_t * p = map+z*(11*16)+5*16;
                printf("%s%6d  ",ANSI_CLEAR,z);
                for(x=(5*16); x<(7*16); x++,p++) {
                    if (x+xoff==xp && z+zoff==zp)
                        printf("%s",ANSI_PLAYER);
                    else
                        printf("%s",ANSI_BLOCK[*p]);
                }
                printf("%s\n",ANSI_CLEAR);
            }
            free(map);
        }
    }
    else if (!strcmp(words[0],"build")) {
        build_request(words, retour);
    }
    else if (!strcmp(words[0],"salt")) {
        int x,y,z;
        if (!words[1] || !words[2] || !words[3] || 
            sscanf(words[1],"%d",&x)!=1 || 
            sscanf(words[2],"%d",&y)!=1 || 
            sscanf(words[3],"%d",&z)!=1 ) {
            sprintf(reply, "Usage: #salt x y z");
        }
        salt_request(retour,x,y,z);
        
    }
    else if (!strcmp(words[0],"holeradar")) {
        opt.holeradar = !opt.holeradar;
        sprintf(reply,"Hole radar is %s",opt.holeradar?"enabled":"disabled");
    }

    if (reply[0])
        chat_message(reply, retour, NULL);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////

static uint8_t * read_string(uint8_t *p, char *s) {
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

void process_packet(int is_client, uint8_t *ptr, ssize_t len,
                    lh_buf_t *forw, lh_buf_t *retour, int *state) {

    // one nice advantage - we can be sure that we have all data in the buffer,
    // so there's no need for limit checking with the new protocol

    uint8_t *p = ptr;
    uint32_t type = lh_read_varint(p);
    uint32_t stype = ((*state<<24)|(is_client<<28)|(type&0xffffff));

    char *states = "ISLP";

#if 0
    printf("%c %c type=%02x, len=%zd\n", is_client?'C':'S',
           states[*state],type,len);
    hexdump(ptr, len);
#endif

    uint8_t output[65536];
    uint8_t *w = output;

    if (*state == STATE_PLAY)
        import_packet(ptr, len, is_client);

    switch (stype) {
        ////////////////////////////////////////////////////////////////////////
        // Idle state

        case CI_Handshake: {
            Rvarint(protocolVer);
            Rstr(serverAddr);
            Rshort(serverPort);
            Rvarint(nextState);
            *state = nextState;
            printf("C %-30s protocol=%d server=%s:%d nextState=%d\n",
                   "Handshake",protocolVer,serverAddr,serverPort,nextState);
            write_packet(ptr, len, forw);
            break;
        }

        ////////////////////////////////////////////////////////////////////////
        // Login

        case CL_EncryptionResponse:
            process_encryption_response(p, forw);
            break;

        case SL_EncryptionRequest:
            process_encryption_request(p, forw);
            break;

        ////////////////////////////////////////////////////////////////////////
        // Play

        case SP_SpawnPlayer: {
            Rvarint(eid);
            Rstr(uuid);
            Rstr(name);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rshort(item);
            //TODO: metadata

            char msg[32768];
            sprintf(msg, "Player %s at %d,%d,%d",name,x>>5,y>>5,z>>5);
            chat_message(msg, forw, "blue");
            write_packet(ptr, len, forw);
            break;
        }

        case SP_SoundEffect: {
            Rstr(name);
            Rint(x);
            Rint(y);
            Rint(z);
            Rfloat(volume);
            Rchar(pitch);
            if (!strcmp(name,"ambient.weather.thunder")) {
                printf("**** THUNDER ****\n"
                       "coords=%d,%d,%d vol=%.4f pitch=%d\n",
                       x/8,y/8,z/8,volume,pitch);
                drop_connection();
                //close(mitm.ms);
            }
            if (strcmp(name,"mob.sheep.say") && strcmp(name,"mob.sheep.step"))
                write_packet(ptr, len, forw);
            break;
        }

        case SP_Effect: {
            Rint(efid);
            Rint(x);
            Rchar(y);
            Rint(z);
            Rint(data);
            Rchar(disrv);
            if (efid == 1013) {
                printf("**** Wither Spawn ****  efid=%d data=%d bcoord=%d:%d:%d %s\n",
                       efid,data,x,y,z,disrv?"disable relative volume":"");
            }
            write_packet(ptr, len, forw);
            break;
        }

        case SP_SetExperience: {
            Rfloat(bar);
            Rshort(level);
            Rshort(exp);

            if (opt.grinding && level >= opt.maxlevel) {
                opt.grinding = 0;
                opt.autokill = 0;

                char msg[4096];
                sprintf(msg, "Grinding finished at level %d",level);
                chat_message(msg, forw, "green");                
            }
            write_packet(ptr, len, forw);
            break;
        }

        case SP_PlayerPositionLook:
        case CP_PlayerPositionLook:
        case CP_PlayerPosition:
        case CP_PlayerLook:
        case SP_MultiBlockChange:
        case SP_BlockChange: {
            if (opt.holeradar) {
                int x = gs.own.x>>5;
                int y = gs.own.y>>5;
                int z = gs.own.z>>5;
                int yaw = (int)gs.own.yaw/90;

                if (x!= hr_last_x || y!= hr_last_y || 
                    z!= hr_last_z || yaw!= hr_last_yaw ) {
                    hr_last_x = x;
                    hr_last_y = y;
                    hr_last_z = z;
                    hr_last_yaw = yaw;
                    hole_radar(is_client?retour:forw);
                }
            }
            if (opt.build) {
                build_process(is_client?retour:forw);
            }

            write_packet(ptr, len, forw);
            break;
        }

        case CP_ChatMessage: {
            Rstr(msg);
            if (msg[0] == '#') {
                if (process_message(msg, forw, retour))
                    break;
            }

            // if it was a normal chat message, just forward it
            write_packet(ptr, len, forw);
            break;
        }

        ////////////////////////////////////////////////////////////////////////
        default: {
            // by default, just forward the packet as is
            write_packet(ptr, len, forw);
        }
    }

}
