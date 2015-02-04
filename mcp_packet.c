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



#define Wx(n,fun)   lh_write_ ## fun ## _be(w, tpkt->n)

#define Wchar(n)    Wx(n,char)
#define Wshort(n)   Wx(n,short)
#define Wint(n)     Wx(n,int)
#define Wlong(n)    Wx(n,long)
#define Wfloat(n)   Wx(n,float)
#define Wdouble(n)  Wx(n,double)
//#define Wstr(n)     w=write_string(w, tpkt->n)
#define Wvarint(n)  lh_write_varint(w, tpkt->n)
//#define Pslot(n)    p=read_slot(p,tpkt->n)
#define Wdata(n,l)  memmove(w,tpkt->n,l); w+=l

////////////////////////////////////////////////////////////////////////////////

typedef struct {
    void    (*decode_method)(MCPacket *);
    ssize_t (*encode_method)(MCPacket *, uint8_t *buf);
    void    (*free_method)(MCPacket *);
    void    (*dump_method)(MCPacket *);
    const char * dump_name;
} packet_methods;

#define DECODE_BEGIN(name,version)                                  \
    void decode_##name##version(MCPacket *pkt) {                    \
        name##_pkt * tpkt = &pkt->_##name;                          \
        assert(pkt->raw);                                           \
        uint8_t *p = pkt->raw;                                      \
        pkt->ver = PROTO##version;

#define DECODE_END                              \
    }

#define ENCODE_BEGIN(name,version)                                  \
    ssize_t encode_##name##version(MCPacket *pkt, uint8_t *buf) {   \
        name##_pkt * tpkt = &pkt->_##name;                          \
        uint8_t *w = buf;

#define ENCODE_END                              \
        return w-buf;                           \
    }

#define SUPPORT_DE(name,version)                \
    [PID(name)] = {                             \
        decode_##name##version,                 \
        encode_##name##version,                 \
        NULL,                                   \
        NULL,                                   \
        #name                                   \
    }


////////////////////////////////////////////////////////////////////////////////

DECODE_BEGIN(SP_SetCompression,_1_8_1) {
    Pvarint(threshold);
} DECODE_END;

ENCODE_BEGIN(SP_SetCompression,_1_8_1) {
    Wvarint(threshold);
} ENCODE_END;

const static packet_methods SUPPORT_1_8_1[2][MAXPACKETTYPES] = {
    {
        SUPPORT_DE(SP_SetCompression,_1_8_1),
    },
    {
    },
};

////////////////////////////////////////////////////////////////////////////////

#define SUPPORT SUPPORT_1_8_1

MCPacket * decode_packet(int is_client, uint8_t *data, ssize_t len) {

    uint8_t * p = data;
    Rvarint(type);              // type field

    lh_create_obj(MCPacket, pkt);

    // fill in basic data
    pkt->type = type;
    pkt->cl   = is_client;
    pkt->mode = STATE_PLAY;
    pkt->ver  = PROTO_NONE;

    // make a raw data copy
    pkt->rawlen = data+len-p;
    pkt->raw = malloc(pkt->rawlen);
    memmove(pkt->raw, p, pkt->rawlen);

    // decode packet if supported
    if (SUPPORT[pkt->cl][pkt->type].decode_method) {
        SUPPORT[pkt->cl][pkt->type].decode_method(pkt);
    }

    return pkt;
}

//FIXME: for now we assume static buffer allocation and sufficient buffer size
//FIXME: we should convert this to lh_buf_t or a resizeable buffer later
ssize_t encode_packet(MCPacket *pkt, uint8_t *buf) {
    uint8_t * p = buf;

    // write packet type
    lh_write_varint(p, pkt->type);
    ssize_t ll = p-buf;

    if (!pkt->modified && pkt->raw) {
        memmove(p, pkt->raw, pkt->rawlen);
        return ll+pkt->rawlen;
    }
    else if ( SUPPORT[pkt->cl][pkt->type].encode_method ) {
        return ll+SUPPORT[pkt->cl][pkt->type].encode_method(pkt, p);
    }
    else {
        assert(0);
    }
}

////////////////////////////////////////////////////////////////////////////////

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

    printf("%c %c %2x ",pkt->cl?'C':'S',states[pkt->mode],pkt->type);
    if (SUPPORT[pkt->cl][pkt->type].dump_method) {
        printf("%-24s    ",SUPPORT[pkt->cl][pkt->type].dump_name);
        SUPPORT[pkt->cl][pkt->type].dump_method(pkt);
    }
    else if (pkt->raw) {
        printf("%-24s    len=%6zd, raw=%s","Raw",pkt->rawlen,limhex(pkt->raw,pkt->rawlen,64));
    }
    else {
        printf("(unknown)");
    }

    printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
 
void free_packet(MCPacket *pkt) {
    lh_free(pkt->raw);

    if (SUPPORT[pkt->cl][pkt->type].free_method)
        SUPPORT[pkt->cl][pkt->type].free_method(pkt);

    free(pkt);
}

void queue_packet (MCPacket *pkt, MCPacketQueue *q) {
    *lh_arr_new(GAR(q->queue)) = pkt;
}
