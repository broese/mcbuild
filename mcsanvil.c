#include "lh_buffers.h"
#include "lh_debug.h"
#include "lh_compress.h"
#include "lh_files.h"

#include "nbt.h"
#include "anvil.h"

#define MC12 1
#define MC13 0
#define MC14 0

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

        printf("%d %d.%06d %d %zd\n",
               header->is_client, header->sec, header->usec,
               header->length, *len);

        // extend buffer if necessary
        if (*len < header->length)
            ARRAY_EXTEND(*buf, *len, header->length);

        // read in the message data
        if (fread(*buf, 1, header->length, mcs) != header->length) return 0;
        printf("%02x %6d %zd\n", **buf, header->length, *len);

    } while (**buf != 0x33);
    return 1;
}

int store_chunk(int X, int Z, uint16_t pbm, uint8_t * cdata, ssize_t clen) {
    // decompress chunk data
    ssize_t len;
    hexdump(cdata, clen);
    uint8_t *buf = zlib_decode(cdata, clen, &len);
    if (!buf) LH_ERROR(0, "zlib decode failed\n");

    // determine the number of cubes in the chunk
    uint32_t Y,n=0;
    for (Y=0;Y<16;Y++)
        if (pbm & (1 << Y))
            n++;

    // region and region-local coordinates of this chunk
    int32_t  rX   = X>>5;
    int32_t  rZ   = Z>>5;
    int32_t  lX   = X&0x1f;
    int32_t  lZ   = Z&0x1f;
    
    printf("CHUNK: %+5d,%+5d (%+3d+%2d,%+3d+%2d) %04x %d cubes\n",
           X,Z,rX,lX,rZ,lZ,pbm,n);
    
    // prepare pointers
    uint8_t *blocks = buf;
    uint8_t *meta   = blocks+n*4096;
    uint8_t *blight = meta  +n*2048;
    uint8_t *slight = blight+n*2048;
    uint8_t *addbl  = slight+n*2048;
    uint8_t *biome  = addbl +n*2048;

    // generate NBT structure

    nbte * Chunk = nbt_make_comp("");
    nbte * Level = nbt_make_comp("Level");

    // Cubes
    nbte * Sections = nbt_make_list("Sections");
    for(Y=0; Y<n; Y++) {
        nbte *Data       = nbt_make_ba("Data",meta+Y*2048,2048);
        nbte *SkyLight   = nbt_make_ba("SkyLight",slight+Y*2048,2048);
        nbte *BlockLight = nbt_make_ba("BlockLight",blight+Y*2048,2048);
        nbte *Ycoord     = nbt_make_byte("Y",(int8_t)Y);
        nbte *Blocks     = nbt_make_ba("Blocks",blocks+Y*4096,4096);

        nbte *cube = nbt_make_comp(NULL);
        nbt_add(cube, Data);
        nbt_add(cube, SkyLight);
        nbt_add(cube, BlockLight);
        nbt_add(cube, Ycoord);
        nbt_add(cube, Blocks);

        nbt_add(Sections, cube);
    }
    nbt_add(Level, Sections);    

    // Other data
    nbt_add(Level, nbt_make_list("Entities"));
    nbt_add(Level, nbt_make_ba("Biomes", biome, 256));
    nbt_add(Level, nbt_make_long("LastUpdate", 0LL));
    nbt_add(Level, nbt_make_int("xPos", lX));
    nbt_add(Level, nbt_make_int("zPos", lZ));
    nbt_add(Level, nbt_make_list("TileEntities"));
    nbt_add(Level, nbt_make_byte("TerrainPopulated",1));
    uint8_t hmap[256];
    memset(hmap, n*16, 256); //FIXME: fake HeightMap
    nbt_add(Level, nbt_make_ba("HeightMap", hmap, 256));

    nbt_add(Chunk,Level);

    // Encode the NBT data
    uint8_t nbuf[1048576];
    uint8_t *end = nbt_write(nbuf, NULL, Chunk, 1);
    nbt_free(Chunk);
    free(buf);
    
    // compress it
    ssize_t nclen;
    uint8_t *ncbuf = zlib_encode(nbuf, end-nbuf, &nclen);



    // Open the region file and store the newly generated chunk there

  
}

int main(int ac, char ** av) {

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
#if MC12
            read_int(p);
#endif
            if (!pbm) continue;

            store_chunk(X, Z, pbm, p, size);
        }

        i++;
    }

#if 0
    mcregion * r = load_region(REG);
    ssize_t clen;
    uint8_t * comp = get_compound_data(r, 0, &clen);
    //hexdump(comp, clen);
#endif
    
}
