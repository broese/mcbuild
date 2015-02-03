#include <string.h>
#include <assert.h>

#define LH_DECLARE_SHORT_NAMES 1
#include <lh_buffers.h>
#include <lh_bytes.h>

#include "mcp_packet.h"
#include "mcp_ids.h"

////////////////////////////////////////////////////////////////////////////////

static uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

#if 0
static uint8_t * read_slot(uint8_t *p, slot_t *s) {
    lh_clear_ptr(s);

    s->id     = lh_read_short_be(p);
    if (s->id != 0xffff) {
        s->count  = lh_read_char(p);
        s->damage = lh_read_short_be(p);
        s->dlen   = lh_read_short_be(p);
        if (s->dlen!=0 && s->dlen!=0xffff) {
            memcpy(s->data, p, s->dlen);
            p += s->dlen;
        }
    }
    else {
        s->count = 0;
        s->damage= 0;
        s->dlen  = 0;
    }
    return p;
}
#endif
    

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p)

#define Rchar(n)    Rx(n,uint8_t,char)
#define Rshort(n)   Rx(n,uint16_t,short)
#define Rint(n)     Rx(n,uint32_t,int)
#define Rlong(n)    Rx(n,uint64_t,long)
#define Rfloat(n)   Rx(n,float,float)
#define Rdouble(n)  Rx(n,double,double)
#define Rstr(n)     char n[4096]; p=read_string(p,n)
#define Rskip(n)    p+=n;
#define Rvarint(n)  uint32_t n = lh_read_varint(p)
//#define Rslot(n)    slot_t n; p=read_slot(p,&n)



#define Px(n,fun)   tpkt->n = lh_read_ ## fun ## _be(p)

#define Pchar(n)    Px(n,char)
#define Pshort(n)   Px(n,short)
#define Pint(n)     Px(n,int)
#define Plong(n)    Px(n,long)
#define Pfloat(n)   Px(n,float)
#define Pdouble(n)  Px(n,double)
#define Pstr(n)     p=read_string(p,tpkt->n)
#define Pvarint(n)  tpkt->n = lh_read_varint(p)
//#define Pslot(n)    p=read_slot(p,tpkt->n)
#define Pdata(n,l)  memmove(tpkt->n,p,l); p+=l



////////////////////////////////////////////////////////////////////////////////

const static char *SUPPORT_1_8[2] = {
    "........+......."
    "................"
    "................"
    "................"
    "......+..."
    ,
    "................"
    ".........."
};

MCPacket * decode_packet_1_8(MCPacket *pkt, uint8_t *data, ssize_t len) {
    uint8_t *p = data;

    // decode packet
    pkt->protocol=PROTO_1_8_1;
    switch(pkt->type) {

        case SP_SetCompression: {
            SetCompression *tpkt = &pkt->p_SetCompression;
            Pvarint(threshold);
            break;
        }

        case SP_PlayerPositionLook: {
            PlayerPositionLook *tpkt = &pkt->p_PlayerPositionLook;
            Pdouble(x);
            Pdouble(y);
            Pdouble(z);
            Pfloat(yaw);
            Pfloat(pitch);
            Pchar(flags);
            break;
        }

        default:
            //TODO: pass the packet to the next lower version decoder
            assert(0); // lowest version decoder should bail out with assert
                       // either declare the packet unsupported or write a decoding routine
    }

    return pkt;
}

ssize_t encode_packet_1_8(MCPacket *pkt, uint8_t *buf) {
    uint8_t *p = buf;

    lh_write_varint(p, PID(pkt->type));

    // encode packet
    switch (pkt->type) {

        case SP_SetCompression: {
            SetCompression *tpkt = &pkt->p_SetCompression;
            write_varint(p,tpkt->threshold);
            break;
        }        

        case SP_PlayerPositionLook: {
            PlayerPositionLook *tpkt = &pkt->p_PlayerPositionLook;
            write_double(p,tpkt->x);
            write_double(p,tpkt->y);
            write_double(p,tpkt->z);
            write_float(p,tpkt->yaw);
            write_float(p,tpkt->pitch);
            write_char(p,tpkt->flags);
            break;
        }

        default:
            //TODO: pass the packet to the next lower version encoder
            assert(0);
    }

    return 0;
}

void free_packet_1_8(MCPacket *pkt) {
    switch (pkt->type) {

        default:
            //TODO: pass the packet to the next lower version encoder
            assert(0);
    }
}

////////////////////////////////////////////////////////////////////////////////

#define SUPPORT SUPPORT_1_8

MCPacket * decode_packet(int is_client, uint8_t *data, ssize_t len) {

    uint8_t * p = data;
    Rvarint(type);              // type field

    lh_create_obj(MCPacket, pkt);

    pkt->type = ((STATE_PLAY<<24)|(is_client<<28)|(type&0xffffff));
    pkt->protocol = PROTO_NONE; // this will be later set by the function that
                                // decoded the packet, or not - then it will be
                                // an unknown packet

    ssize_t psize = data+len-p; // length of the data that follows the type field

    if (SUPPORT[is_client][type]=='*' || SUPPORT[is_client][type]=='+') {
        // we support decode (directly or differred), pass the rest of the data
        // to the decoder routine
        return decode_packet_1_8(pkt, p, psize);
    }
    else {
        // no support - decode as UnknownPacket
        lh_alloc_buf(pkt->p_UnknownPacket.data,psize);
        pkt->p_UnknownPacket.length = psize;
        memmove(pkt->p_UnknownPacket.data, p, psize);
        return pkt;
    }
}

//FIXME: for now we assume static buffer allocation and sufficient buffer size
//FIXME: we should convert this to lh_buf_t or a resizeable buffer later
ssize_t encode_packet(MCPacket *pkt, uint8_t *buf) {
    uint8_t * p = buf;
    
    if (pkt->protocol == PROTO_NONE) {
        // UnknownPacket
        lh_write_varint(p, PID(pkt->type));
        memmove(p,pkt->p_UnknownPacket.data,pkt->p_UnknownPacket.length);
        return p+pkt->p_UnknownPacket.length-buf;
    }
    else {
        return encode_packet_1_8(pkt,buf);
    }
}

char limhexbuf[4100];
static const char * limhex(uint8_t *data, ssize_t len, ssize_t maxbyte) {
    //assert(len<(sizeof(limhexbuf)-4)/2);
    assert(maxbyte >= 4);

    int i;
    //TODO: implement aaaaaa....bbbbbb - type of printing
    if (len > maxbyte) len = maxbyte;
    for(i=0;i<len;i++)
        sprintf(limhexbuf+i*2,"%02x ",data[i]);
    return limhexbuf;
}

void dump_packet(MCPacket *pkt) {
    char *states="ISLP";

    if (pkt->protocol == PROTO_NONE) {
#if 0
        printf("%c %c %2x ",PCLIENT(pkt->type)?'C':'S',states[PSTATE(pkt->type)],PID(pkt->type));
        printf("%-24s len=%6zd, data=%s\n","[UnknownPacket]",pkt->p_UnknownPacket.length,
               limhex(pkt->p_UnknownPacket.data,pkt->p_UnknownPacket.length,64));
#endif
        return;
    }

    printf("%c %c %2x ",PCLIENT(pkt->type)?'C':'S',states[PSTATE(pkt->type)],PID(pkt->type));
    switch (pkt->type) {

        case SP_SetCompression: {
            printf("%-24s threshold=%d\n","SetCompression",pkt->p_SetCompression.threshold);
            break;
        }

        case SP_PlayerPositionLook: {
            PlayerPositionLook *tpkt = &pkt->p_PlayerPositionLook;
            printf("%-24s coords=%.1f,%.1f,%.1f rot=%.1f,%.1f flags=%02x\n","PlayerPositionLook",
                   tpkt->x,tpkt->y,tpkt->z,tpkt->yaw,tpkt->pitch,tpkt->flags);
            break;
        }

        default:
            printf("%-24s\n","[UnsupportedType]");
    }
}

////////////////////////////////////////////////////////////////////////////////
 
void free_packet(MCPacket *pkt) {
    if (pkt->protocol == PROTO_NONE) {
        if (pkt->p_UnknownPacket.data)
            free(pkt->p_UnknownPacket.data);
        pkt->p_UnknownPacket.data = NULL;
    }
    else {
        if (SUPPORT[PCLIENT(pkt->type)][PID(pkt->type)]=='*')
            free_packet_1_8(pkt);
    }
    free(pkt);
}

void queue_packet (MCPacket *pkt, MCPacketQueue *q) {
    *lh_arr_new(GAR(q->queue)) = pkt;
}
