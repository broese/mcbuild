#include <string.h>
#include <assert.h>

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

#define Px(n,fun)   pkt->n = lh_read_ ## fun ## _be(p)

#define Pchar(n)    Px(n,char)
#define Pshort(n)   Px(n,short)
#define Pint(n)     Px(n,int)
#define Plong(n)    Px(n,long)
#define Pfloat(n)   Px(n,float)
#define Pdouble(n)  Px(n,double,double)
#define Pstr(n)     p=read_string(p,pkt->n)
#define Pvarint(n)  pkt->n = lh_read_varint(p)
//#define Pslot(n)    p=read_slot(p,pkt->n)
#define Pdata(n,l)  memmove(pkt->n,p,l); p+=l

////////////////////////////////////////////////////////////////////////////////

const char *SUPPORT_1_8[2] = {
    "................"
    "................"
    "................"
    "................"
    ".........."
    ,
    "................"
    ".........."
};

MCPacket * decode_packet_1_8(int is_client, MCPacket *pkt, uint8_t *data, ssize_t len) {
    uint8_t * p = data;
    uint8_t * lim = data+len;

    Rvarint(type);
    pkt->type = ((STATE_PLAY<<24)|(is_client<<28)|(type&0xffffff));
    pkt->protocol = PROTO_1_8_1;

    if (SUPPORT_1_8[is_client][type]!='*') {
        // decode of this packet type is not supported by the current
        // protocol version implementation, neither direct or deferred
        // decode this as a generic UnknownPacket

        // check the length of data - should not exceed MAXPLEN
        assert(lim-p < sizeof(pkt->p_UnknownPacket.data));

        // store the remaining payload data
        pkt->p_UnknownPacket.length = lim-p;
        memmove(pkt->p_UnknownPacket.data, p, pkt->p_UnknownPacket.length);

        // reset the protocol ID, since we didn't decode the packet
        pkt->protocol = PROTO_NONE;

        return pkt;
    }

    // decode packet
    switch(pkt->type) {


        default:
            //TODO: pass the packet to the next lower version decoder
            assert(0); // lowest version decoder should bail out with assert
                       // either declare the packet unsupported or write a decoding routine
    }

    return pkt;
}

ssize_t    encode_packet_1_8(MCPacket *pkt, uint8_t *buf) {
    uint8_t *p = buf;

    int type = pkt->type&0xffffff;

    if (pkt->protocol == PROTO_NONE) {
        // this is a generic UnknownPacket
        lh_write_varint(p, type);
        memmove(p, pkt->p_UnknownPacket.data, pkt->p_UnknownPacket.length);
        return p-buf+pkt->p_UnknownPacket.length;
    }

    // encode packet
    switch (pkt->type) {

        default:
            //TODO: pass the packet to the next lower version encoder
            assert(0);
    }


    return 0;
}

////////////////////////////////////////////////////////////////////////////////

MCPacket * decode_packet(int is_client, uint8_t *data, ssize_t len) {
    lh_create_obj(MCPacket, pkt);
    MCPacket *res = decode_packet_1_8(is_client,pkt,data,len);
    if (!res) free(pkt);
    return res;
}

ssize_t    encode_packet(MCPacket *pkt, uint8_t *buf) {
    return encode_packet_1_8(pkt,buf);
}
