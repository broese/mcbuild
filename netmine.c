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

#define SERVERIP "83.233.237.64"
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

#if 0
static uint8_t * read_string(const uint8_t **p, int len) {
    if (len < 2) return NULL;
    uint16_t slen = read_short(*p);
    if (len < slen*2+2) return NULL;
    ALLOCB(s,slen+1);

    int i;
    for(i=0; i<slen; i++) {
        (*p)++;
        s[i] = read_char(*p);
    }
    s[i] = 0;
    return s;
}

/* Observed login process on 2b2t.net. Does not match description in the wiki
00000035  01 00 00 00 1d 00 0a 00  44 00 6f 00 72 00 71 00 ........ D.o.r.q.
00000045  75 00 65 00 6d 00 61 00  64 00 61 00 00 00 00 00 u.e.m.a. d.a.....
00000055  00 00 00 00 00 00 00 00                          ........ 
    00000023  01 00 40 15 14 00 00 00  07 00 64 00 65 00 66 00 ..@..... ..d.e.f.
    00000033  61 00 75 00 6c 00 74 00  00 00 00 00 00 00 00 03 a.u.l.t. ........
    00000043  00 3c 06 00 00 00 00 00  00 00 42 00 00 00 00 ca .<...... ..B.....
    00000053  00 00 00 00 04 00 00 00  00 f6 ed b4 e2          ........ .....
*/
#define RSTR s = read_string(&p,limit-p); if (!s) { atlimit=1; break; }
#endif

typedef struct {
    int    type;
    char * name;
    int    metalen;
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

////////////////////////////////////////////////////////////////////////////////

#define Rest(l) (limit-p >= (l))

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



#define Rx(n,type,fun) type n; if Rest(sizeof(type)) { n = fun(p); } else { atlimit=1; break; }

#define Rchar(n)  Rx(n,char,read_char)
#define Rshort(n) Rx(n,short,read_short)
#define Rint(n)   Rx(n,int,read_int)
#define Rlong(n)  Rx(n,long long,read_long);
#define Rfloat(n) Rx(n,float,read_float)
#define Rdouble(n) Rx(n,double,read_double)
#define Rstr(n)   char n[4096]; if (read_string((char **)&p,n,(char *)limit)) { atlimit=1; break; }
#define Rskip(n)  if (limit-p < n) { atlimit=1; break; } else { p+=n; }

////////////////////////////////////////////////////////////////////////////////

uint32_t process_stream(uint8_t * data, uint32_t len, int is_client) {
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
                printf("Login Request (Client) %d %s\n",uid,s);
                Rskip(13);
            }
            else {
                p +=2;
                Rstr(s);
                printf("Login Request (Server) %d %s\n",uid,s);
                Rskip(38);
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

        case MCP_PlayerPositionLook: { //0D
            Rdouble(x);
            Rdouble(y);
            Rdouble(stance);
            Rdouble(z);
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);
            printf("Player Position & Look: coord=%.1lf,%.1lf,%.1lf stance=%.1lf rot=%.1f,%.1f ground=%d\n",
                   x,y,z,stance,yaw,pitch,ground);
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

        case MCP_EntityVelocity: { //1C
            Rint(eid);
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);
            printf("Entity Velocity: EID=%d %d,%d,%d\n",eid,vx,vy,vz);
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
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);

            int i;
            for(i=0; MOBS[i].type>0 && MOBS[i].type!=type; i++);
            if (MOBS[i].type<0) {
                printf("Unknown mob type %d\n",type);
                exit(1);
            }

            while(p<limit && *p!=127) p++;
            if (p>=limit) {atlimit=1; break;}
            printf("Spawn Mob: EID=%d type=%d %s %d,%d,%d\n",eid,type,MOBS[i].name,x/32,y/16,z/32);
            p++;
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

        case MCP_EntityLook: { //20
            Rint(eid);
            Rchar(yaw);
            Rchar(pitch);
            printf("Entity Look: EID=%d, yaw=%d pitch=%d\n",eid,yaw,pitch);
            break;
        }

        case MCP_EntityHeadLook: { //23
            Rint(eid);
            Rchar(yaw);
            printf("Entity Head Look: EID=%d %d\n",eid,yaw);
            break;
        }

        case MCP_SetExperience: { //2B
            Rfloat(expbar);
            Rshort(level);
            Rshort(exp);
            printf("Set Experience EXP=%d LVL=%d bar=%.1f\n",exp,level,expbar);
            break;
        }

        case MCP_ChunkAllocation: { //32
            Rint(X);
            Rint(Y);
            Rchar(alloc);
            printf("Allocate Chunk %d,%d (%d,%d) => %s\n",X,Y,X*16,Y*16,alloc?"allocate":"release");
            break;
        }

        case MCP_ChunkData: { //33
            printf("in 33\n");
            Rint(X);
            Rint(Z);
            Rchar(cont);
            Rshort(primary_bitmap);
            Rshort(add_bitmap);
            Rint(compsize);
            Rint(unused);
            printf("preparing to skip %d bytes, only %d available\n",compsize,limit-p);
            Rskip(compsize);
            printf("Chunk Data: %d,%d cont=%d PBM=%04x ABM=%04x compsize=%d\n",X,Z,cont,primary_bitmap,add_bitmap,compsize);
            break;
        }

        case MCP_SetSlot: { //67
            Rchar(wid);
            Rshort(slot);
            Rshort(iid);
            if (iid == -1) {
                printf("SetSlot: wid=%d slot=%d -\n",wid,slot);
            }
            else {
                Rchar(icount);
                Rshort(damage);
                Rshort(notchshit);

                if (notchshit != -1) p-=2;// roll back two bytes if no notchshit present
                //FIXME: Enchantments?

                printf("SetSlot: wid=%d slot=%d %3d x%2d (%04x)\n",wid,slot,iid,icount,(unsigned short)notchshit);
                if (damage > 0) {
                    if (iid < 256) {
                        printf(" metadata=%d\n",damage);
                        Rskip(damage);
                    }
                    else {
                        printf(" dmg=%d",damage);
                    }
                }
                printf("\n");
            }
            break;
        }

        case MCP_SetWindowItems: { //68
            Rchar(wid);
            Rshort(count);
            printf("Set Window Items: wid=%d count=%d\n",wid,count);
            int slot;
            count+=12; // GOD DAMN IT, NOTCH, Y U DO THIS
            for(slot=0; slot<count; slot++) { 
                Rshort(iid);
                if (iid == -1) {
                    printf(" Slot %2d : -\n",slot);
                    continue;
                }
                else {
                    Rchar(icount);
                    Rshort(damage);
                    printf(" Slot %2d : %3d x%2d",slot,iid,icount);
                    if (damage > 0) {
                        if (iid < 256) {
                            printf(" metadata=%d\n",damage);
                            Rskip(damage);
                        }
                        else {
                            printf(" dmg=%d",damage);
                        }
                    }
                    printf("\n");
                }                
            }
            printf("-------------------\n");
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
            printf("UpdateTileEntry: %d,%d,%d action=%d custom=%d,%d,%d\n",x,y,z,action,c1,c2,c3);
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
            //printf("in FF\n");
            //hexdump(data+consumed, len-consumed);
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
    uint32_t slen[2] = { 0, 0};
    uint16_t cport = 0xffff; // port FFFF == no connection tracked

    #define BUFGRAN 65536

    while(pkt = pcap_next(p, &h)) {
        pn++;
        //printf("Packet length %d\n",h.len);
        h_eth * eth = (h_eth *)pkt;
        //printf("%02x:%02x:%02x:%02x:%02x:%02x -> %02x:%02x:%02x:%02x:%02x:%02x\n",
        //       eth->src[0],eth->src[1],eth->src[2],eth->src[3],eth->src[4],eth->src[5],
        //       eth->dst[0],eth->dst[1],eth->dst[2],eth->dst[3],eth->dst[4],eth->dst[5]);
        if (ntohs(eth->et) != ET_IP4)
            LH_ERROR(1,"Incorrect ethertype %04x\n",ntohs(eth->et));

        h_ip4 * ip = (h_ip4 *)(pkt+sizeof(h_eth));
        if (ip->proto != IPP_TCP)
            LH_ERROR(1,"Incorrect IP type %04x\n",ip->proto);
        //char src[32],dst[32];
        //sprintf(src,"%s",inet_ntoa(ip->src));
        //sprintf(dst,"%s",inet_ntoa(ip->dst));
        //printf("%s -> %s\n",src,dst);

        h_tcp * tcp = (h_tcp *)(pkt+sizeof(h_eth)+sizeof(h_ip4));
        //printf("%s:%d -> %s:%d\n",src,ntohs(tcp->sport),dst,ntohs(tcp->dport));

        if (ntohs(ip->off) & IP_MF)
            LH_ERROR(1,"Fragmented IP packet %d",pn);

        uint8_t * pdata = pkt+sizeof(h_eth)+sizeof(h_ip4)+TH_OFF(tcp);
        int plen = ntohs(ip->len)-sizeof(h_ip4)-TH_OFF(tcp);
        int is_client = (ntohl(ip->src.s_addr) == 0x0a000002);

        //printf("%7d %c %4d\n",pn,is_client?'C':' ',plen);

        if ((tcp->flags & TH_SYN) && !(tcp->flags & TH_ACK)) {
            if (cport != 0xffff) LH_ERROR(1, "Multiple parallel connections are not supported");
            cport = ntohs(tcp->sport);
            //printf("Resetting sdata\n");
            if (sdata[0]) { free(sdata[0]); } sdata[0] = NULL; slen[0]=0;
            if (sdata[1]) { free(sdata[1]); } sdata[1] = NULL; slen[1]=0;
        }
        if (tcp->flags & TH_FIN) {
            cport = 0xffff;
            //printf("Close connection\n");
        }

        if (plen > 0) {
            if ( (is_client && cport != ntohs(tcp->sport)) ||
                 (!is_client && cport != ntohs(tcp->dport)) )
                LH_ERROR(1, "Incorrect port");

            int oldlen = slen[is_client];
            BUFFER_ADDG(sdata[is_client], slen[is_client], plen, BUFGRAN);
            printf("%6d %c %08x %7d\n", pn, is_client?'C':' ', sdata[is_client], slen[is_client]);
            memcpy(sdata[is_client]+oldlen,pdata,plen);

            int consumed = process_stream(sdata[is_client], slen[is_client], is_client);
            if (consumed > 0) {
                memcpy(sdata[is_client], sdata[is_client]+consumed, slen[is_client]-consumed);
                BUFFER_EXTENDG(sdata[is_client], slen[is_client], slen[is_client]-consumed, BUFGRAN);
            }
            
        }

        //printf("%d\n",pn);

        //if (!--cnt) break;
    }

    

    return 0;
}
