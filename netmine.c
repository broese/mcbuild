#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <pcap.h>
#include <netinet/in.h>

#include "lh_debug.h"
#include "lh_buffers.h"
#include "lh_files.h"

#include "anvil.h"
#include "nbt.h"

////////////////////////////////////////////////////////////////////////////////

//#define SERVERIP "83.233.237.64"
#define SERVERIP "10.0.0.1"
#define PCAPFILTER "host " SERVERIP

#define ET_IP4 0x800

typedef struct {
    unsigned char dst[6];
    unsigned char src[6];
    unsigned short et;
} h_eth;

#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */

#define IPP_ICMP 0x01
#define IPP_IGMP 0x02
#define IPP_IP4  0x04
#define IPP_TCP  0x06
#define IPP_UDP  0x11
#define IPP_IP6  0x29
#define IPP_ESP  0x32
#define IPP_AH   0x33

typedef struct {
    unsigned char  vhl;
    unsigned char  tos;
    unsigned short len;
    unsigned short id;
    unsigned short off;
    unsigned char  ttl;
	unsigned char  proto;
	unsigned short cs;
    struct in_addr src,dst;
} h_ip4;

typedef struct {
    unsigned short sport,dport;

    unsigned long  seq, ack;
    
    unsigned char th_offx2;	/* data offset, rsvd */
    #define TH_OFF(th) ((((th)->th_offx2 & 0xf0) >> 4)<<2)

	unsigned char flags;
	#define TH_FIN 0x01
	#define TH_SYN 0x02
	#define TH_RST 0x04
	#define TH_PUSH 0x08
	#define TH_ACK 0x10
	#define TH_URG 0x20
	#define TH_ECE 0x40
	#define TH_CWR 0x80
	#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    unsigned short win;
    unsigned short cs;
    unsigned short urg;
} h_tcp;

////////////////////////////////////////////////////////////////////////////////

#define MCP_KeepAlive           0x00
#define MCP_LoginRequest        0x01
#define MCP_Handshake           0x02
#define MCP_ChatMessage         0x03
#define MCP_TimeUpdate          0x04
#define MCP_EntityEquipment     0x05
#define MCP_SpawnPosition       0x06
#define MCP_UseEntity           0x07
#define MCP_UpdateHealth        0x08
#define MCP_Respawn             0x09
#define MCP_Player              0x0a
#define MCP_PlayerPosition      0x0b
#define MCP_PlayerLook          0x0c
#define MCP_PlayerPositionLook  0x0d
#define MCP_PlayerDigging       0x0e
#define MCP_PlayerBlockPlace    0x0f
#define MCP_HeldItemChange      0x10
#define MCP_UseBed              0x11
#define MCP_Animation           0x12
#define MCP_EntityAction        0x13
#define MCP_SpawnNamedEntity    0x14
#define MCP_SpawnDroppedItem    0x15
#define MCP_CollectItem         0x16
#define MCP_SpawnObject         0x17
#define MCP_SpawnMob            0x18
#define MCP_SpawnPainting       0x19
#define MCP_SpawnExperienceOrb  0x1a
#define MCP_EntityVelocity      0x1c
#define MCP_DestroyEntity       0x1d
#define MCP_Entity              0x1e
#define MCP_EntityRelativeMove  0x1f
#define MCP_EntityLook          0x20
#define MCP_EntityLookRelmove   0x21
#define MCP_EntityTeleport      0x22
#define MCP_EntityHeadLook      0x23
#define MCP_EntityStatus        0x26
#define MCP_AttachEntity        0x27
#define MCP_EntityMetadata      0x28
#define MCP_EntityEffect        0x29
#define MCP_RemoveEntityEffect  0x2a
#define MCP_SetExperience       0x2b
#define MCP_ChunkAllocation     0x32
#define MCP_ChunkData           0x33
#define MCP_MultiBlockChange    0x34
#define MCP_BlockChange         0x35
#define MCP_BlockAction         0x36
#define MCP_BlockBreakAnimation 0x37
#define MCP_MapChunkBulk        0x38
#define MCP_Explosion           0x3c
#define MCP_SoundParticleEffect 0x3d
#define MCP_NamedSoundEffect    0x3e
#define MCP_ChangeGameState     0x46
#define MCP_Thunderbolt         0x47
#define MCP_OpenWindow          0x64
#define MCP_CloseWindow         0x65
#define MCP_ClickWindow         0x66
#define MCP_SetSlot             0x67
#define MCP_SetWindowItems      0x68
#define MCP_UpdateWindowProp    0x69
#define MCP_ConfirmTransaction  0x6a
#define MCP_CreativeInventory   0x6b
#define MCP_EnchanItem          0x6c
#define MCP_UpdateSign          0x82
#define MCP_ItemData            0x83
#define MCP_UpdateTileEntity    0x84
#define MCP_IncrementStatistic  0xc8
#define MCP_PlayerListItem      0xc9
#define MCP_PlayerAbilities     0xca
#define MCP_TabComplete         0xcb
#define MCP_LocaleAndViewdist   0xcc
#define MCP_ClientStatuses      0xcd
#define MCP_PluginMessage       0xfa
#define MCP_EncryptionKeyResp   0xfc
#define MCP_EncryptionKeyReq    0xfd
#define MCP_ServerListPing      0xfe
#define MCP_Disconnect          0xff

typedef struct {
    int    type;
    char * name;
    int    metaadd;
} mobmeta;

mobmeta MOBS[] = {
    { 50, "Creeper",      2 },
    { 51, "Skeleton",     0 },
    { 52, "Spider",       1 },
    { 53, "Giant Zombie", 0 },
    { 54, "Zombie",       0 },
    { 55, "Slime",        1 },
    { 56, "Ghast",        1 },
    { 57, "Zombie Pigman",0 },
    { 58, "Enderman",     2 },
    { 59, "Cave Spider",  1 },
    { 60, "Silverfish",   0 },
    { 61, "Blaze",        1 },
    { 62, "Magma Cube",   1 },
    { 63, "Ender Dragon", 2 },
    { 90, "Pig",          1 },
    { 91, "Sheep",        1 },
    { 92, "Cow",          0 },
    { 93, "Chicken",      0 },
    { 94, "Squid",        0 },
    { 95, "Wolf",         7 },
    { 96, "Mooshoom",     0 },
    { 97, "Snowman",      0 },
    { 98, "Ocelot",       7 },
    { 99, "Iron Golem",   1 },
    { 120, "Villager",    4 },
    { -1, "",             0 }
};

// List of enchantable items
// Ref: http://wiki.vg/wiki/index.php?title=Slot_Data&oldid=2152
short TOOLS[] = {
    256,257,258,        // iron tools
    259,                // flint and steel
    261,                // bow
    267,                // iron sword
    268,269,270,271,    // wooden tools
    272,273,274,275,    // stone tools
    276,277,278,279,    // diamond tools
    283,284,285,286,    // golden tools
    290,291,292,293,294,// hoes
    298,299,300,301,    // leather equipment
    302,303,304,305,    // chainmain equipment
    306,307,308,309,    // iron equipment
    310,311,312,313,    // diamond equipment
    314,315,316,317,    // golden equipment
    346,                // fishing rod
    359,                // shears
    -1,
};

static inline int istool(int id) {
    int i;
    for(i=0; TOOLS[i]>=0; i++)
        if (TOOLS[i] == id)
            return 1;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

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


#define Rlim(l) if (limit-p < l) { atlimit=1; break; }

#define Rx(n,type,fun) type n; Rlim(sizeof(type)) else { n = fun(p); }

#define Rchar(n)  Rx(n,char,read_char)
#define Rshort(n) Rx(n,short,read_short)
#define Rint(n)   Rx(n,int,read_int)
#define Rlong(n)  Rx(n,long long,read_long);
#define Rfloat(n) Rx(n,float,read_float)
#define Rdouble(n) Rx(n,double,read_double)
#define Rstr(n)   char n[4096]; if (read_string((char **)&p,n,(char *)limit)) { atlimit=1; break; }
#define Rskip(n)  Rlim(n) else { p+=n; }

typedef struct {
    short id;
    char  count;
    short damage;
    char  meta[256];
} slot_t;

#define Rslot(n)                                            \
    slot_t n = {0,0,0};                                     \
    Rshort(id); n.id = id;                                  \
    if (id != -1) {                                         \
        Rchar(count); n.count = count;                      \
        Rshort(damage); n.damage = damage;                  \
        if (istool(id)) {                                   \
            Rshort(metalen);                                \
            if (metalen != -1)                              \
                Rskip(metalen);                             \
        }                                                   \
    }
//TODO: extract metadata into buffer

#define Rmobmeta(n)                                                     \
    while (1) {                                                         \
        Rchar(type);                                                    \
        if (type == 0x7f) break;                                        \
        type = (type>>5)&0x07;                                          \
        switch (type) {                                                 \
            case 0: Rskip(1); break;                                    \
            case 1: Rskip(2); break;                                    \
            case 2: Rskip(4); break;                                    \
            case 3: Rskip(4); break;                                    \
            case 4: { Rstr(s); break; }                                 \
            case 5: { Rslot(s); break; }                                \
            case 6: Rskip(12); break;                                   \
            default: printf("Unsupported type %d\n",type); exit(1);     \
        }                                                               \
    }

////////////////////////////////////////////////////////////////////////////////

#include <lh_compress.h>

void process_chunk(uint8_t *cdata, ssize_t clen, uint16_t bitmask, int X, int Z) {
    printf("Decoding chunk (%08p,%d)\n",cdata,clen);

    ssize_t len;
    unsigned char * data = zlib_decode(cdata, clen, &len);
    if (!data) { printf("zlib_decode failed!\n"); exit(1); }

    uint8_t *ptr = data;

    char fname[256];
    sprintf(fname, "chunks/chunk_%04x_%04x.dat",(unsigned short)X,(unsigned short)Z);
    FILE *fd = open_file_w(fname);

    int Y,n=0;
    for (Y=0;Y<16;Y++)
        if (bitmask & (1 << Y))
            n++;

    write_file_f(fd, (unsigned char *)&n, sizeof(n));
    write_file_f(fd, (unsigned char *)&X, sizeof(X));
    write_file_f(fd, (unsigned char *)&Z, sizeof(Z));
    write_file_f(fd, data, n*4096);
    fclose(fd);

#if 0
    int Y,i;
    for (Y=0;Y<16;Y++) {
        //If the bitmask indicates this chunk has been sent...
        if (bitmask & (1 << Y)) {
            for(i=0; i<4096; i++) {
                if (ptr[i] == 22) {
                    //Byte arrays
                    int x = X*16 + (i&0x0F);
                    int y = Y*16 + (i>>8);
                    int z = Z*16 + ((i&0xF0)>>4);
                    printf("*** Sugar Cane : %d,%d,%d ***\n",x,y,z);
                }
            }
            ptr+=4096;
        }
    }
#endif

    free(data);
}

uint32_t process_streamz(uint8_t * data, uint32_t len, int is_client) {
    return len;
}

uint32_t process_stream(uint8_t * data, uint32_t len, int is_client) {
    //if (is_client) return len;

    int consumed = 0;
    uint8_t * limit = data+len;
    int atlimit = 0;

    char *s;

    // consumed stays constant within the switch and is only advanced
    // if the handler had success. Otherwise the stream pointer is left
    // on the beginning of the last unprocessed item and can be 
    // tried again on the next run (when next packets arrive)

    while(1) {
        uint8_t * p = data+consumed;
        if (p>=limit) break;

        uint8_t mtype = read_char(p);
        printf("%c %02x ",is_client?'C':'S',mtype);
        switch(mtype) {

        case MCP_KeepAlive: { // 00
            Rint(id);
            printf("Keepalive: id=%08x\n",id);
            break;
        }
       
        case MCP_LoginRequest: { // 01
            Rint(uid);
            if (is_client) {
                Rstr(s);
                Rskip(13);
                printf("Login Request (Client) %d %s\n",uid,s);
            }
            else {
                Rskip(2);
                Rstr(s);
                Rskip(38);
                printf("Login Request (Server) %d %s\n",uid,s);
            }
            break;
        }

        case MCP_Handshake: { // 02
            Rstr(s);
            printf("Handshake: %s\n",s);
            break;
        }

        case MCP_ChatMessage: { // 03
            Rstr(s);
            printf("Chat Message: %s\n",s);
            break;
        }

        case MCP_TimeUpdate: { // 04
            Rlong(tm);
            printf("Time Update: %lld (%dd %dh)\n",tm,(int)(tm/24000L),(int)(tm%24000)/1000);
            break;
        }

        case MCP_EntityEquipment: { //05
            Rint(eid);
            Rshort(sid);
            Rshort(iid);
            Rshort(damage);
            printf("Entity Equipment: EID=%d slot=%d item=%d dmg=%d\n",eid,sid,iid,damage);
            break;
        }

        case MCP_UseEntity: { //07
            Rint(user);
            Rint(target);
            Rchar(button);
            printf("Use Item: user=%d target=%d %c\n",user,target,button?'L':'R');
            break;
        }

        case MCP_UpdateHealth: { //08
            Rshort(health);
            Rshort(food);
            Rfloat(saturation);
            printf("UpdateHealth: HP=%d FP=%d SP=%.1f\n",health,food,saturation);
            break;
        }

#if 0
        case MCP_Respawn: { // 09
            Rint(dimension);
            Rchar(difficulty);
            Rchar(creative);
            Rshort(height);
            Rstr(leveltype);
            printf("Respawn: dim=%d diff=%d creative=%d height=%d level=%s\n",
                   dimension,difficulty,creative,height,leveltype);
            break;
        }
#endif

        case MCP_Player: { //0A
            Rchar(ground);
            printf("Player: ground=%d\n",ground);
            break;
        }

        case MCP_PlayerPosition: { //0B
            Rdouble(x);
            Rdouble(y);
            Rdouble(stance);
            Rdouble(z);
            Rchar(ground);
            printf("Player Position: coord=%.1lf,%.1lf,%.1lf stance=%.1lf ground=%d\n",
                   x,y,z,stance,ground);
            break;
        }

        case MCP_PlayerLook: { //0C
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);
            printf("Player Look: rot=%.1f,%.1f ground=%d\n",yaw,pitch,ground);
            break;
        }

        case MCP_PlayerPositionLook: { //0D
            Rdouble(x);
            Rdouble(y);
            Rdouble(stance);
            Rdouble(z);
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);
            printf("Player Position & Look: coord=%.1f,%.1f,%.1f stance=%.1f rot=%.1f,%.1f ground=%d\n",
                   x,y,z,stance,yaw,pitch,ground);
            break;
        }

        case MCP_PlayerDigging: {//0e
            Rchar(status);
            Rint(x);
            Rchar(y);
            Rint(z);
            Rchar(face);
            printf("Player Digging: coord=%d,%d,%d face=%d\n",x,(unsigned int)y,z,face);
            break;
        }

        case MCP_PlayerBlockPlace: { //0f
            Rint(x);
            Rchar(y);
            Rint(z);
            Rchar(dir);
            Rslot(s);
            
            printf("Player Block Placement: coord=%d,%d,%d item:",x,(unsigned int)y,z);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_HeldItemChange: {//10
            Rshort(sid);
            printf("Held Item Change: slot=%d\n",sid);
            break;
        }

        case MCP_UseBed: {//11
            Rint(eid);
            Rchar(unk);
            Rint(x);
            Rchar(y);
            Rint(z);
            printf("Use Bed: EID=%d coord=%d,%d,%d\n",eid,x,(unsigned int)y,z);
            break;
        }

        case MCP_Animation: {//12
            Rint(eid);
            Rchar(aid);
            printf("Animation: EID=%d anim=%d\n",eid,aid);
            break;
        }

        case MCP_EntityAction: { //13
            Rint(eid);
            Rchar(action);
            printf("Entity Action: eid=%d action=%d\n",eid,action);
            break;
        }

        case MCP_SpawnNamedEntity: {//14
            Rint(eid);
            Rstr(pname);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rshort(item);
            printf("Spawn Named Entity: EID=%d %s coords=%d,%d,%d rot=%d,%d item=%d\n",
                   eid,pname,x/32,y/16,z/32,yaw,pitch,item);
            break;
        }

        case MCP_SpawnDroppedItem: { //15
            Rint(eid);
            Rshort(iid);
            Rchar(count);
            Rshort(dmg);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(rot);
            Rchar(pitch);
            Rchar(roll);
            printf("Spawn Dropped Item: EID=%d IID=%d x%d %d,%d,%d\n",eid,iid,count,x/32,y/16,z/32);
            break;
        }

        case MCP_CollectItem: { //16
            Rint(ieid);
            Rint(ceid);
            printf("Collect Item: item=%d collector=%d\n",ieid,ceid);
            break;
        }

        case MCP_SpawnObject: {//17
            Rint(eid);
            Rchar(type);
            Rint(x);
            Rint(y);
            Rint(z);
            Rint(eeid);
            if (eeid == 0) {
                printf("Spawn Object: eid=%d type=%d coord=%d,%d,%d\n",eid,type,x/32,y/16,z/32);
            }
            else {
                Rshort(vx);
                Rshort(vy);
                Rshort(vz);
                printf("Spawn Object: eid=%d type=%d coord=%d,%d,%d Fireball eeid=%d speed=%d,%d,%d\n",eid,type,x/32,y/16,z/32,eeid,vx,vy,vz);
            }
            break;
        }

        case MCP_SpawnMob: { //18
            Rint(eid);
            Rchar(type);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rchar(hyaw);

            int i;
            for(i=0; MOBS[i].type>0 && MOBS[i].type!=type; i++);
            if (MOBS[i].type<0) {
                printf("Unknown mob type %d\n",type);
                exit(1);
            }

            Rmobmeta(m);
            printf("Spawn Mob: EID=%d type=%d %s %d,%d,%d\n",eid,type,MOBS[i].name,x/32,y/16,z/32);
            break;
        }

        case MCP_SpawnPainting: { //19
            Rint(eid);
            Rstr(title);
            Rint(x);
            Rint(y);
            Rint(z);
            Rint(dir);
            printf("Spawn Painting: EID=%d %s %d,%d,%d %d\n",eid,title,x,y,z,dir);
            break;
        }

        case MCP_SpawnExperienceOrb: { //1A
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rshort(count);
            printf("Spawn Experience Orb: EID=%d coord=%d,%d,%d count=%d\n",eid,x,y,z,count);
            break;
        }

        case MCP_EntityVelocity: { //1C
            Rint(eid);
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);
            printf("Entity Velocity: EID=%d %d,%d,%d\n",eid,vx,vy,vz);
            break;
        }

        case MCP_DestroyEntity: { //1D
            Rint(eid);
            printf("Destroy Entity: EID=%d\n",eid);
            break;
        }

        case MCP_EntityRelativeMove: { //1F
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);
            printf("Entity Relative Move: EID=%d move=%d,%d,%d\n",eid,dx,dy,dz);
            break;
        }

        case MCP_EntityLook: { //20
            Rint(eid);
            Rchar(yaw);
            Rchar(pitch);
            printf("Entity Look: EID=%d, yaw=%d pitch=%d\n",eid,yaw,pitch);
            break;
        }

        case MCP_EntityLookRelmove: { //21
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);
            Rchar(yaw);
            Rchar(pitch);
            printf("Entity Look and Relative Move: EID=%d move=%d,%d,%d look=%d,%d\n",eid,dx,dy,dz,yaw,pitch);
            break;
        }

        case MCP_EntityTeleport: { //22
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            printf("Entity Teleport: EID=%d move=%d,%d,%d look=%d,%d\n",eid,x/32,y/16,z/32,yaw,pitch);
            break;
        }

        case MCP_EntityHeadLook: { //23
            Rint(eid);
            Rchar(yaw);
            printf("Entity Head Look: EID=%d %d\n",eid,yaw);
            break;
        }

        case MCP_EntityStatus: { //26
            Rint(eid);
            Rchar(status);
            printf("Entity Status: EID=%d status=%d\n",eid,status);
            break;
        }

        case MCP_AttachEntity: { //27
            Rint(eid);
            Rint(vid);
            printf("Attach Entity: EID=%d vehicle=%d\n",eid,vid);
            break;
        }

        case MCP_EntityMetadata: { //28
            Rint(eid);
            Rmobmeta(m);
            printf("Entity Metadata: EID=%d\n",eid);
            break;
        }

        case MCP_SetExperience: { //2B
            Rfloat(expbar);
            Rshort(level);
            Rshort(exp);
            printf("Set Experience: EXP=%d LVL=%d bar=%.1f\n",exp,level,expbar);
            break;
        }

        case MCP_ChunkAllocation: { //32
            Rint(X);
            Rint(Y);
            Rchar(alloc);
            printf("Allocate Chunk: %d,%d (%d,%d) => %s\n",X,Y,X*16,Y*16,alloc?"allocate":"release");
            break;
        }

        case MCP_ChunkData: { //33
            Rint(X);
            Rint(Z);
            Rchar(cont);
            Rshort(primary_bitmap);
            Rshort(add_bitmap);
            Rint(compsize);
            Rint(unused);
            Rskip(compsize);
            process_chunk(p-compsize,compsize,primary_bitmap,X,Z);
            printf("Chunk Data: %d,%d cont=%d PBM=%04x ABM=%04x compsize=%d\n",X,Z,cont,primary_bitmap,add_bitmap,compsize);
            break;
        }

        case MCP_MultiBlockChange: { //34
            Rint(X);
            Rint(Z);
            Rshort(count);
            Rint(dsize);
            Rskip(dsize);
            printf("Multi Block Change: %d,%d count=%d (%d bytes)\n",X,Z,count,dsize);
            break;
        }

        case MCP_BlockChange: { //35
            Rint(x);
            Rchar(y);
            Rint(z);
            Rchar(bid);
            Rchar(metadata);
            printf("Block Change: %d,%d,%d block=%d meta=%d\n",x,(unsigned int)y,z,bid,metadata);
            break;
        }

        case MCP_BlockAction: { //36
            Rint(x);
            Rshort(y);
            Rint(z);
            Rchar(b1);
            Rchar(b2);
            printf("Block Action: %d,%d,%d %d %d\n",x,y,z,b1,b2);
            break;
        }

        case MCP_Explosion: { //3C
            Rdouble(x);
            Rdouble(y);
            Rdouble(z);
            Rfloat(radius);
            Rint(count);
            Rskip(count*3);
            printf("Explosion: %f,%f,%f r=%f count=%d\n",x,y,z,radius,count);
            break;
        }

        case MCP_SoundParticleEffect: { //3D
            Rint(effect);
            Rint(x);
            Rchar(y);
            Rint(z);
            Rint(data);
            printf("Sound/Particle Effect: %d,%d,%d effect=%d,%d\n",x,(unsigned int)y,z,effect,data);
            break;
        }

        case MCP_ChangeGameState: { //46
            Rchar(reason);
            Rchar(mode);
            printf("Change Game State: reason=%d mode=%d\n",reason, mode);
            break;
        }

        case MCP_Thunderbolt: { //47
            Rint(eid);
            Rchar(unk);
            Rint(x);
            Rint(y);
            Rint(z);
            printf("Thunderbolt EID=%d coord=%d,%d,%d\n",eid,x,y,z);
            break;
        }

        case MCP_OpenWindow: {//64
            Rchar(wid);
            Rchar(invtype);
            Rstr(title);
            Rchar(nslots);
            printf("Open Window: wid=%d invtype=%d nslots=%d Title=%s\n",wid,invtype,nslots,title);
            break;
        }

        case MCP_CloseWindow: {//65
            Rchar(wid);
            printf("Close Window: wid=%d\n",wid);
            break;
        }

        case MCP_ClickWindow: {//66
            Rchar(wid);
            Rshort(sid);
            Rchar(rclick);
            Rshort(action);
            Rchar(shift);
            Rslot(s);

            printf("Click Window: wid=%d slot=%d action=[%c %c %d]",wid,sid,rclick?'R':'L',shift?'S':'-',action);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_SetSlot: { //67
            Rchar(wid);
            Rshort(sid);
            Rslot(s);

            printf("Set Slot: wid=%d slot=%d item=",wid,sid);
            if (s.id == -1)
                printf("-");
            else {
                printf("%3d x%2d",s.id,s.count);
                if (s.damage != 0)
                    printf(" dmg=%d",s.damage);
            }
            printf("\n");
            break;
        }

        case MCP_SetWindowItems: { //68
            Rchar(wid);
            Rshort(nslots);
            printf("Set Window Items: wid=%d nslots=%d\n",wid,nslots);
            int slot;
            for(slot=0; slot<nslots; slot++) {
                Rslot(s);
                printf("    Slot %2d :",slot);
                if (s.id == -1)
                    printf("-");
                else {
                    printf("%3d x%2d",s.id,s.count);
                    if (s.damage != 0)
                        printf(" dmg=%d",s.damage);
                }
                printf("\n");
            }
            break;
        }

        case MCP_UpdateWindowProp: { //69
            Rchar(wid);
            Rshort(pid);
            Rshort(pvalue);
            printf("Update Window Property: wid=%d prop %d => %d\n",wid,pid,pvalue);
            break;
        }

        case MCP_ConfirmTransaction: { //6A
            Rchar(wid);
            Rshort(action);
            Rchar(accepted);
            printf("Confirm Transaction: wid=%d action=%d accepted=%d\n",wid,action,accepted);
            break;
        }
        
        case MCP_UpdateSign: { //82
            Rint(x);
            Rshort(y);
            Rint(z);
            Rstr(line1);
            Rstr(line2);
            Rstr(line3);
            Rstr(line4);
            printf("Update Sign: %d,%d,%d\n",x,y,z);
            printf("%s\n%s\n%s\n%s\n",line1,line2,line3,line4);
            break;
        }

        case MCP_UpdateTileEntity: { //84
            Rint(x);
            Rshort(y);
            Rint(z);
            Rchar(action);
            Rint(c1);
            Rint(c2);
            Rint(c3);
            printf("Update Tile Entry: %d,%d,%d action=%d custom=%d,%d,%d\n",x,y,z,action,c1,c2,c3);
            break;
        }

        case MCP_IncrementStatistic: { //c8
            Rint(stid);
            Rchar(amount);
            printf("Increment Statistic: id=%d by %d\n",stid,amount);
            break;
        }

        case MCP_PlayerListItem: { //C9
            Rstr(s);
            Rchar(online);
            Rshort(ping);
            printf("Player List Item: %-24s ping: %4dms online:%d\n",s,ping,online);
            break;
        }

#if 0
        case MCP_EncryptionKeyReq: { //FD
            Rstr(sid);
            Rshort(len);
            Rskip(len);
            Rshort(tlen);
            Rskip(tlen);
            printf("Encryption Key Request: %d %d\n",len,tlen);
            break;
        }
#endif

        case MCP_ServerListPing: { //FE
            printf("Server List Ping\n");
            break;
        }

        case MCP_Disconnect: { //FF
            Rstr(s);
            printf("Disconnect: %s\n",s);
            break;
        }

        default:
            printf("Unknown message %02x\n",mtype);
            hexdump(data+consumed, len-consumed);
            exit(1);
        }       
        if (!atlimit)
            consumed = p-data;
        else
            break;
    }

    return consumed;
}

#define MAX(a,b) (((a)>(b))?(a):(b))


int main(int ac, char ** av) {

    // open PCAP

    const char *pname = av[1];
    if (!pname) LH_ERROR(1, "you must specify file name");

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t * p = pcap_open_offline(pname, errbuf);
    if (!p) LH_ERROR(1, "Failed to open pcap %s : %s",pname,errbuf);

    printf("Opened %s successfully\n",pname);


    // configure filter

    struct bpf_program bpf;

    if (pcap_compile(p, &bpf, PCAPFILTER, 1, 0xffffff00)<0) 
        LH_ERROR(1, "Failed to compile filter %s", PCAPFILTER);

    if (pcap_setfilter(p, &bpf)<0)
        LH_ERROR(1, "Failed to set filter %s - %s", PCAPFILTER, pcap_geterr(p));

    printf("Set filter %s successfully\n",PCAPFILTER);


    // start reading packets
    struct pcap_pkthdr h;
    unsigned char *pkt;

    int cnt = 100;
    int pn=0;

    // monitored connection
    uint8_t *sdata[2] = { NULL, NULL };
    int32_t slen[2] = { 0, 0}; // current length of data in the buffers, does not necessarily equal acknowledged data
    int32_t seqn[2] = { 0, 0}; // sequence number of the first byte in the buffer
    int32_t ackd[2] = { 0, 0}; // last acknowledged seqn for a stream
    uint16_t cport = 0xffff; // port FFFF == no connection tracked

    #define BUFGRAN 65536

    while(pkt = pcap_next(p, &h)) {
        pn++;

        h_eth * eth = (h_eth *)pkt;
        if (ntohs(eth->et) != ET_IP4)
            LH_ERROR(1,"Incorrect ethertype %04x\n",ntohs(eth->et));

        h_ip4 * ip = (h_ip4 *)(pkt+sizeof(h_eth));
        if (ip->proto != IPP_TCP)
            LH_ERROR(1,"Incorrect IP type %04x\n",ip->proto);

        h_tcp * tcp = (h_tcp *)(pkt+sizeof(h_eth)+sizeof(h_ip4));
        //printf("%s:%d -> %s:%d\n",src,ntohs(tcp->sport),dst,ntohs(tcp->dport));

        if (ntohs(ip->off) & IP_MF)
            LH_ERROR(1,"Fragmented IP packet %d",pn);

        uint8_t * pdata = pkt+sizeof(h_eth)+sizeof(h_ip4)+TH_OFF(tcp);
        int plen = ntohs(ip->len)-sizeof(h_ip4)-TH_OFF(tcp);
        int is_client = (ntohl(ip->src.s_addr) == 0x0a000002);

        if (tcp->flags & TH_SYN) {
            if (!(tcp->flags & TH_ACK)) {
                // first packet from the client - SYN

                if (cport != 0xffff) LH_ERROR(1, "Multiple parallel connections are not supported");
                cport = ntohs(tcp->sport);

                //printf("Resetting sdata\n");
                if (sdata[1]) { free(sdata[1]); } sdata[1] = NULL; slen[1]=0; seqn[1]=ntohl(tcp->seq)+1; ackd[1]=seqn[1];
            }
            else {
                // first packet from the server - SYN ACK
                if (sdata[0]) { free(sdata[0]); } sdata[0] = NULL; slen[0]=0; seqn[0]=ntohl(tcp->seq)+1; ackd[0]=seqn[0];
            }
            continue;
        }
        if (tcp->flags & TH_FIN) {
            cport = 0xffff;
            //printf("Close connection\n");
            continue;
            //FIXME: do correct connection close
        }
        if (tcp->flags & TH_ACK) {
            ackd[1-is_client] = ntohl(tcp->ack);
        }

        if (plen > 0) {
            if ( (is_client && cport != ntohs(tcp->sport)) ||
                 (!is_client && cport != ntohs(tcp->dport)) )
                LH_ERROR(1, "Incorrect port");

            int offset = ntohl(tcp->seq)-seqn[is_client];
            int maxlen = offset+plen;

            if (maxlen > slen[is_client]) {
                ARRAY_EXTENDG(sdata[is_client], slen[is_client], maxlen, BUFGRAN);
            }
            //printf("%6d %c %08x %7d seq=%u %u ack=%u (%u)\n", pn, is_client?'C':'S', sdata[is_client], slen[is_client], 
            //       seqn[is_client], ntohl(tcp->seq), ackd[is_client], ackd[is_client]-seqn[is_client]);
            //printf("Inserting %d bytes (%u) at offset %d. maxlen=%d\n",plen,ntohl(tcp->seq),offset,maxlen);

            if (offset >= 0) {
                memcpy(sdata[is_client]+offset,pdata,plen);
            }
            else if (maxlen > 0) {
                memcpy(sdata[is_client],pdata-offset,plen+offset);
            }

            //printf("Giving %d data, %d in the buffer\n",ackd[is_client]-seqn[is_client],slen[is_client]);
            int consumed = process_stream(sdata[is_client], ackd[is_client]-seqn[is_client], is_client);
            if (consumed > 0) {
                memcpy(sdata[is_client], sdata[is_client]+consumed, slen[is_client]-consumed);
                ARRAY_EXTENDG(sdata[is_client], slen[is_client], slen[is_client]-consumed, BUFGRAN);
                seqn[is_client]+=consumed;
            }            
        }
   }

    

    return 0;
}
