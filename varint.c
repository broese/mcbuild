#include <stdio.h>
#include <stdlib.h>

#include <lh_bytes.h>
#include <lh_debug.h>

int main(int ac, char ** av) {
    if (!av[1]) exit(1);

    uint8_t buf[16];
    ssize_t len = hex_import(av[1], buf, sizeof(buf));

    //hexdump(buf,len);
    int32_t v = lh_parse_varint(buf);
    printf("%x\n",v);
}
