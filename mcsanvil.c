#include "lh_buffers.h"
#include "lh_debug.h"
#include "lh_compress.h"
#include "lh_files.h"
#include "lh_image.h"

#include "nbt.h"
#include "anvil.h"


#define MC12 0
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

        // extend buffer if necessary
        if (*len < header->length)
            ARRAY_EXTEND(*buf, *len, header->length);

        // read in the message data
        if (fread(*buf, 1, header->length, mcs) != header->length) return 0;
        //printf("%02x %6d\n", **buf, header->length);
        //hexdump(*buf, header->length);
        return 1;
    } while (**buf != 0x33 && **buf != 0x38 && **buf != 0x17);
    return 1;
}

// find a free block of suitable size in the region file by analyzing its chunk directory
// either we find a block inside the file, or if not we'll append it at the end
int find_chunk_space(uint32_t *cdir, int size, int maxsize) {
    ALLOCB(map,maxsize);
    int i, j;
    uint8_t *dp = (uint8_t *)(cdir);
    for(i=0; i<1024; i++) {
        uint32_t c  = read_int(dp);
        int csize   = c&0xff;
        int coff    = c>>8;
        for(j=0; j<csize; j++) map[coff+j] = 1;
    }

    int maxpos = maxsize-size;

    for(i=1; i<maxpos; ) {
        int pass = 1;
        for(j=0; j<size; j++)
            if (map[i+j])
                break;

        if (j==size) {
            free(map);
            return i;
        }
        while(!map[i] && i<maxpos) i++; // skip zeros
        while(map[i] && i<maxpos) i++;  // skip ones
        // so after that i is positioned at the start of the next free window
    }

    // no suitable window found - return next block after the end of file
    free(map);
    return maxsize;
}

int store_chunk(int X, int Z, uint8_t *data, ssize_t len) {
    // region and region-local coordinates of this chunk
    int32_t  rX   = X>>5;
    int32_t  rZ   = Z>>5;
    int32_t  lX   = X&0x1f;
    int32_t  lZ   = Z&0x1f;
    int      idx  = lX + (lZ<<5);

    // buffer for the column directory
    uint32_t cdir[1024];
    CLEAR(cdir);

    char regname[1024];
    sprintf(regname, "region/r.%d.%d.mca", rX, rZ);

    // Try opening it for update and see if the region file already exists
    off_t rsize = -1;
    FILE * rf = open_file_u(regname, &rsize);

    if (!rf) {
        // probably the file does not exist yet. Try creating it instead
        rf = open_file_w(regname);
        if (!rf) LH_ERROR(0, "Failed to create the region file %s",regname);
        rsize = 4096;
        
        // note that the column directory is automatically initialized
		// as empty as it was zeroed
    }
    else if (rsize < 4096) {
        // file exists, but has defective directory
        rsize = 4096;
    }
    else {
        // file exists and is opened for update
        if (read_from(rf, 0, (uint8_t *)cdir, 4096)) return 0;
    }

    // position and size of the old chunk if one exists
    uint8_t *dp = (uint8_t *)(&cdir[idx]);
    uint32_t c  = parse_int(dp);
    int csize   = c&0xff; // size of the old chunk in 4k blocks
    int coff    = c>>8; // offset of the chunk in the file, in 4k blocks
    
    if (csize > 0) {
        // delete the old chunk
        ALLOCB(empty, csize*4096);
        write_to(rf, coff*4096, empty, csize*4096); // erase data
        place_int(dp, 0); // erase directory entry
        free(empty);
    }
	
	// size of the new chunk, in 4k blocks
	int ncsize  = GRANSIZE(len, 4096) / 4096;

    // size of the file, 4k blocks
    int maxsize = GRANSIZE(rsize, 4096) / 4096;
	
	// if csize>=ncsize, we can just reuse the offset
	if (csize < ncsize)
		// if not - we need to find a large enough unused window in the file
		coff = find_chunk_space(cdir, ncsize, maxsize);
		
    //printf("Storing chunk (len=%d) at position %d (%016x)\n",ncsize,coff,coff*4096);
	write_to(rf, coff*4096, data, ncsize*4096);

	// update chunk directory
	c = (ncsize&0xff)+((coff<<8)&0xffffff00);
	place_int(dp, c);
    //hexdump(cdir, 4096);
	write_to(rf, 0, (uint8_t *)cdir, 4096); // save chunk directory

    fclose(rf);
	
	return 1;
}

ssize_t create_nbt_chunk(int X, int Z, uint16_t pbm, uint8_t * cdata, ssize_t clen, uint8_t *ncbuf, ssize_t ncsize) {
    // decompress chunk data
    uint8_t buf[262144];
    CLEAR(buf);
    ssize_t len = zlib_decode_to(cdata, clen, buf, sizeof(buf));
    if (!len<0) LH_ERROR(-1, "zlib decode failed\n");

    // determine the number of cubes in the chunk
    uint32_t Y,n=0;
    for (Y=0;Y<16;Y++)
        if (pbm & (1 << Y))
            n++;

    // region-local coordinates of this chunk
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
    uint8_t *biome  = slight+n*2048;

    // block search
#if 1
    printf("Chunk %3d,%3d\n",X,Z);
    int Cy,b;
    for(Cy=0; Cy<n; Cy++) {
        uint8_t *bl = blocks+Cy*4096;
        for(b=0; b<4096; b++) {
            int xx = b&0x0f;
            int zz = (b&0xf0)>>4;
            int yy = Cy*16+(b>>8);
            if (bl[b] == 110) printf("Mycelium  : %5d,%5d (%3d)\n",X*32+xx,Z*32+zz,yy);
            if (bl[b] == 130) printf("Enderchest: %5d,%5d (%3d)\n",X*32+xx,Z*32+zz,yy);
            if (bl[b] == 120) printf("EndportalF: %5d,%5d (%3d)\n",X*32+xx,Z*32+zz,yy);
        }        
    }
    return 0;
#endif

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

    // Other data
    nbt_add(Level, nbt_make_list("Entities"));
    nbt_add(Level, nbt_make_ba("Biomes", biome, 256));
    nbt_add(Level, nbt_make_long("LastUpdate", 0x1234LL));
    nbt_add(Level, nbt_make_int("xPos", X));
    nbt_add(Level, nbt_make_int("zPos", Z));
    nbt_add(Level, nbt_make_list("TileEntities"));
    nbt_add(Level, nbt_make_byte("TerrainPopulated",1));

    // generate heightmap
    uint32_t hmap[256];
    memset(hmap, 0, 1024);
    int y,i;
    for(i=0; i<256; i++) {
        uint8_t *bp = blocks+i;
        for(y=0; y<n*16; y++) {
            if (*bp) hmap[i] = y;
            bp+=256;
        }
    }
    nbt_add(Level, nbt_make_ia("HeightMap", hmap, 256));

    nbt_add(Level, Sections);    
    nbt_add(Chunk,Level);

    // Encode the NBT data
    uint8_t nbuf[1048576];
    uint8_t *end = nbt_write(nbuf, NULL, Chunk, 1);
    nbt_free(Chunk);
    
    // compress the NBT data
    CLEARN(ncbuf,ncsize);
    ssize_t encsize = zlib_encode_to(nbuf, end-nbuf, ncbuf+5, ncsize-5);
    if (encsize<0) return -1;

    // add the 5-byte header
    uint8_t *wp = ncbuf;
    write_int(wp, encsize);
    write_char(wp, 2);
	return encsize+5;
}

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

#include <time.h>

#define CLR(a,b,c,d,e,f) (((a)<<23)|((b)<<22)|((c)<<15)|((d)<<14)|((e)<<7)|((f)<<6))
#define CCC(a,b,c) CLR(a,a,b,b,c,c)

int main(int ac, char ** av) {
    int i=1;
    while(av[i]) {
        off_t sz;
        FILE * mcs = open_file_r(av[i], &sz);
        if (!mcs) continue;

        BUFFER(buf,len);
        mcsh h;

#if 0
        while(read_stream(mcs, &buf, &len, &h)) {
            uint8_t *p = buf;
            p++;
#if 0
            char str[1024];
            read_string((char **)&p, str, p+1000); //FIXME
            char date[1024];
            strftime(date, sizeof(date), "%F %T", localtime((time_t *)&h.sec));
            printf("%s %-20s\n",date,str);
#endif

#if 0
            int32_t  eid  = read_int(p);
            char name[1024];
            read_string((char **)&p, name, p+1000); //FIXME


            int32_t  X    = read_int(p);
            int32_t  Y    = read_int(p);
            int32_t  Z    = read_int(p);

            char date[1024];
            strftime(date, sizeof(date), "%F %T", localtime((time_t *)&h.sec));
            printf("%s %-20s %5d,%5d %3d\n",date,name,X/32,Z/32,Y/32);
#endif

#if 0
            if (*buf == 0x33) {
                // ChunkData
                int32_t  X    = read_int(p);
                int32_t  Z    = read_int(p);
                char     cont = read_char(p);
                uint16_t pbm  = read_short(p);
                uint16_t abm  = read_short(p);
                int32_t  size = read_int(p);
#if MC12
                read_int(p);
#endif
                if (!pbm) continue; // skip empty updates

                uint8_t ncbuf[512288];

                ssize_t ccsize = create_nbt_chunk(X,Z,pbm,p, size, ncbuf, sizeof(ncbuf));
                //store_chunk(X, Z, ncbuf, ccsize);
            }
            else if (*buf == 0x38) {
                // MapChunkBulk
                int16_t count = read_short(p);
                int32_t dlen = read_int(p);
                char    skylight = read_char(p);

                uint8_t *data = p;
                p += dlen;
                ssize_t ulen;
                uint8_t *udata = zlib_decode(data, dlen, &ulen);
                
                for(i=0; i<count; i++) {
                    int32_t  X    = read_int(p);
                    int32_t  Z    = read_int(p);
                    uint16_t pbm  = read_short(p);
                    uint16_t abm  = read_short(p);
                    //printf("Chunk %d,%d\n",X,Z);

                    uint32_t Y,n=0;
                    for (Y=0;Y<16;Y++)
                        if (pbm & (1 << Y))
                            n++;

                    // block search
#if 1
                    int Cy,b;
                    for(Cy=0; Cy<n; Cy++) {
                        uint8_t *bl = udata+Cy*4096;
                        for(b=0; b<4096; b++) {
                            int xx = b&0x0f;
                            int zz = (b&0xf0)>>4;
                            int yy = Cy*16+(b>>8);
                            if (bl[b] == 110) printf("Mycelium  : %5d,%5d (%3d)\n",X*16+xx,Z*16+zz,yy);
                            if (bl[b] == 130) printf("Enderchest: %5d,%5d (%3d)\n",X*16+xx,Z*16+zz,yy);
                            if (bl[b] == 120) printf("EndportalF: %5d,%5d (%3d)\n",X*16+xx,Z*16+zz,yy);
                        }        
                    }
#endif

                    // advance udata pointer to the next chunk
                    udata += (4096+2048+2048+2048*skylight)*n+256;
                }
            }
#endif

            if (*buf == 0x17) {
                // SpawnVehicle
                int32_t eid = read_int(p);
                char    type = read_char(p);
                int32_t  x    = read_int(p);
                int32_t  y    = read_int(p);
                int32_t  z    = read_int(p);
                printf("Object: type=%2d %5d,%5d (%3d)\n",type,x/32,y/32,z/32);
            }




        }
#endif

        lhimage * img = allocate_image(128,128);

        int32_t colors[16] = { 0, 8368696, 16247203, 10987431,
                               16711680, 10526975, 10987431, 31744,
                               16777215, 10791096, 12020271, 7368816,
                               4210943, 6837042, 0, 0};

        while(read_stream(mcs, &buf, &len, &h)) {
            if (buf[0] == 0x83) {
                //printf("%c %4d.%06d %5d     ",h.is_client?'C':'S',h.sec,h.usec,h.length);
                //hexprint(buf+8, h.length-8); //(h.length <= 40)?h.length-8:40);

                uint8_t *p= buf+1;
                uint16_t itype = read_short(p);
                uint16_t dmg   = read_short(p);
                uint16_t len   = read_short(p);
                uint8_t  mtype = read_char(p);
                if (mtype == 0) {
                    //hexprint(buf, 138);
                    uint8_t x = *p++;
                    uint8_t y = *p++;
                    while(y<128) {
                        char u = *p++;
                        
                        int var6 = colors[u>>2];
                        unsigned char var7 = u&3;
                        unsigned char var8 = 220;
                        if (var7 == 2) var8 = 255;
                        if (var7 == 0) var8 = 180;

                        int r = (var6 >> 16 & 255) * var8 / 255;
                        int g = (var6 >> 8 & 255) * var8 / 255;
                        int b = (var6 & 255) * var8 / 255;

                        uint32_t color = (r<<16)|(g<<8)|b;
                        IMGDOT(img,x,y) = color;
                        y++;
                    }
                }
            }
        }

        export_png_file(img,"mymap.png");


        free(buf);
        fclose(mcs);

        i++;
    }
}
