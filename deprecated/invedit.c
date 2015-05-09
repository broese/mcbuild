#include "nbt.h"

#include <lh_debug.h>
#include <lh_buffers.h>
#include <lh_files.h>
#include <lh_compress.h>

int main(int ac, char **av) {
    ssize_t csz;
    uint8_t * cdata = read_file("level.dat",&csz);

    ssize_t sz;
    uint8_t * data = gzip_decode(cdata, csz, &sz);
    hexdump(data, sz);

    nbte * level = nbt_parse(&data);
    nbte * Data  = nbt_ce(level,"Data");
    nbte * Player = nbt_ce(Data,"Player");
    nbte * XpLevel = nbt_ce(Player,"XpLevel");

    XpLevel->v.i = 600;

    nbt_dump(level, 0);


    ////////////////////////////////////////////////////////////////////////////

    uint8_t nbuf[1048576];
    uint8_t * end = nbt_write(nbuf, NULL, level, 1);

    ssize_t nsz;
    uint8_t * ncdata = gzip_encode(nbuf, end-nbuf, &nsz);
    write_file("newlevel.dat", ncdata, nsz);

    
}
