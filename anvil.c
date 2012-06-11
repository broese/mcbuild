#include "anvil.h"
#include "nbt.h"

#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef HAS_ENDIAN
#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>
#define be32toh(x) ntohl(x)
#else
#include <endian.h>
#endif

#include "lh_debug.h"
#include "lh_files.h"
#include "lh_buffers.h"
#include "lh_compress.h"


mcregion * load_region(const char * path) {
    // open file
    ssize_t size;
    FILE *fd = open_file_r(path, &size);
    if (!fd) LH_ERROR(NULL,"load_region : failed to open file %s", path);
    printf("Opened %s (%lu KiB, %lu pages)\n",path,size/1024,size/4096);

    // allocate region
    ALLOC(mcregion,region);
    region->fd = fd;
    region->path = path;
    region->size = size/4096;

    // load column (compound) table
    int i;
    uint32_t table[4096];
    if (read_from(fd, 0, (unsigned char *)table, sizeof(table))<0)
        return NULL;

    // allocate column table
    ALLOCNE(mccolumn, region->columns, 1024);

    int gen = 0; // number of generated columns

    for(i=0; i<1024; i++) {
        uint32_t c = be32toh(table[i]);      // column info from the column table
        mccolumn * col = region->columns+i;
        col->sectors = c&0xff;               // number of 4k sectors/pages of the column data
        col->offset = (off_t)(c>>8) * 4096;  // starting offset of the column data in the MCA file


        if (col->sectors > 0) {              // this index is non-empty
            struct {
                uint32_t size;
                char     compr;
            } ch;
            
            read_from(fd, col->offset, (unsigned char *)&ch, sizeof(ch));
            col->size = be32toh(ch.size);    // exact size of the column data in bytes
            col->compr = ch.compr;           // compression type

            if (col->size>0) gen++;          // is size>0, column data is already generated
            //printf("%4d %08x %2d %08x %10d %d\n",i,c,col->sectors,col->offset,col->size,col->compr);
        }

    }
    printf("Loaded column table, %d columns generated (%.2f%%)\n",
           gen, (float)gen/10.24);

    return region;
}

void free_region(mcregion *region) {
    if (!region) return;

    if (region->columns)
    fclose(region->fd);
    free(region);
}

unsigned char * get_compound_data(mcregion *region, int x, int y, ssize_t *len) {
    mccolumn * col = &region->columns[x+y*16];

    unsigned char *cdata = read_froma(region->fd, col->offset+5, col->size);
    if (!cdata) return NULL;

    unsigned char * data = zlib_decode(cdata, col->size, len);
    free(cdata);
    return data;
}

////////////////////////////////////////////////////////////////////////////////

void parse_compound(unsigned char *data, ssize_t len) {
    //hexdump(data,len);
    unsigned char *rptr = data;
    char name[65536];
    int i;

    int depth = 0;
    do {
        char type = nbt_parsetag(rptr, name, &rptr);
        for(i=0; i<depth; i++) printf("  ");

        ssize_t plen = 0;
        //printf("%2d (%s)\n",type,name);
        switch (type) {
            case NBT_TAG_END:
                printf("End\n");
                depth--;
                break;
            case NBT_TAG_BYTE: {
                int8_t val = nbt_read_byte(rptr);
                printf("Byte (%s) %d\n",name,val);
                rptr++;
                break;
            }
            case NBT_TAG_SHORT: {
                int16_t val = nbt_read_short(rptr);
                printf("Short (%s) %d\n",name,val);
                rptr+=2;
                break;
            }
            case NBT_TAG_INT: {
                int32_t val = nbt_read_int(rptr);
                printf("Int (%s) %d\n",name,val);
                rptr+=4;
                break;
            }
            case NBT_TAG_LONG: {
                int64_t val = nbt_read_long(rptr);
                printf("Long (%s) %lld\n",name,val);
                rptr+=8;
                break;
            }
            case NBT_TAG_FLOAT: {
                float val = nbt_read_float(rptr);
                printf("Float (%s) %f\n",name,val);
                rptr+=4;
                break;
            }
            case NBT_TAG_DOUBLE: {
                double val = nbt_read_double(rptr);
                printf("Double (%s) %f\n",name,val);
                rptr+=8;
                break;
            }
            case NBT_TAG_BYTE_ARRAY: {
                int32_t len = nbt_read_int(rptr);
                rptr+=4+len;
                printf("Byte Array (%s) %d bytes\n",name,len);
                break;
            }
            case NBT_TAG_STRING: {
                uint16_t len = nbt_read_short(rptr);
                rptr+=2+len;
                printf("String (%s) %d bytes\n",name,len);
                break;
            }
            case NBT_TAG_LIST: {
                char stype = nbt_read_byte(rptr);
                rptr++;
                int32_t len = nbt_read_int(rptr);
                rptr+=4;
                printf("List (%s) of %d, len=%d\n",name,stype,len);
                break;
            }
            case NBT_TAG_COMPOUND: {
                printf("Compound (%s)\n",name);
                depth++;
                break;
            }
            case NBT_TAG_INT_ARRAY: {
                int32_t len = nbt_read_int(rptr);
                rptr += 4+len*4;
                printf("Int Array (%s) %d ints\n",name,len);
                break;
            }
        }

    } while(rptr-data < len);
}
