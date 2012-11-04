#include "lh_buffers.h"
#include "lh_debug.h"
#include "lh_compress.h"
#include "lh_files.h"

#include "nbt.h"
#include "anvil.h"

#define REG "r.0.0.mca"

typedef struct {
    int32_t is_client;
    int32_t sec;
    int32_t usec;
    int32_t length;
} mcsh;

// read a MCStream (.mcs) file and return only the 0x33 messages (chunks)
// returns 1 on success, 0 on end of stream or error
int read_stream(FILE *mcs, uint8_t **buf, ssize_t *len, mcsh *header) {
    do {
        // read header
        char hb[sizeof(mcsh)];
        if (fread(hb, sizeof(mcsh), 1, mcs) != 1) return 0;
        //hexdump(hb, sizeof(hb));
        uint8_t *p = hb;
        header->is_client = read_int(p);
        header->sec       = read_int(p);
        header->usec      = read_int(p);
        header->length    = read_int(p);

        // extend buffer if necessary
        if (*len < header->length)
            ARRAY_EXTEND(*buf, *len, header->length);

        // read in the message data
        if (fread(*buf, 1, header->length, mcs) != header->length) return 0;
        //printf("%02x %6d\n", **buf, header->length);

    } while (**buf != 0x33);

    return 1;
}

store_chunk(int X, int Z, uint16_t pbm, uint8_t * cdata, ssize_t clen) {
    // decompress chunk data
    ssize_t len;
    uint8_t *buf = zlib_decode(cdata, clen, &len);

    // determine the number of cubes in the chunk
    uint32_t Y,n=0;
    for (Y=0;Y<16;Y++)
        if (pbm & (1 << Y))
            n++;

    // prepare pointers
    uint8_t *blocks = buf;
    uint8_t *meta   = blocks+n*4096;
    uint8_t *blight = meta  +n*2048;
    uint8_t *slight = blight+n*2048;
    uint8_t *addbl  = slight+n*2048;
    uint8_t *biome  = addbl +n*2048;

    
    
    
}

int main(int ac, char ** av) {

#if 0
    int i=1;
    while(av[i]) {
        ssize_t sz;
        FILE * mcs = open_file_r(av[i], &sz);
        if (!mcs) continue;

        BUFFER(buf,len);
        mcsh h;

        while(read_stream(mcs, &buf, &len, &h)) {
            uint8_t *p = buf;
            p++;
            int32_t  X    = read_int(p);
            int32_t  Z    = read_int(p);
            char     cont = read_char(p);
            uint16_t pbm  = read_short(p);
            uint16_t abm  = read_short(p);
            int32_t  size = read_int(p);

            if (pbm == 0x0000) continue;

            printf("%+5d,%+5d %6d %04x\n",X,Z,size,pbm);
        }

        i++;
    }
#endif

#if 0
    mcregion * r = load_region(REG);
    ssize_t clen;
    uint8_t * comp = get_compound_data(r, 0, &clen);
    //hexdump(comp, clen);
#endif

    char *str = NULL;
    char *p = strdup(str);
    printf("%p\n",p);
    
}
