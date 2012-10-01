#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <libtasn1.h>

#include "lh_buffers.h"
#include "lh_files.h"

uint8_t decode_tlv(uint8_t *data, ssize_t size, uint8_t **value, ssize_t *vsize) {
    uint8_t *p = data;
    uint8_t type = read_char(p);

    // read lenght octets
    uint16_t length = 0;

    // try the short form
    uint8_t oct1 = read_char(p);
    if (oct1 & 0x80) {
        // this is the long form
        switch(oct1&0x7f) {
        case 1: length = (uint8_t)read_char(p); break;
        case 2: length = read_short(p); break;
        default: printf("invalid length %d in DER encoding\n",oct1&0x7f); exit(1);
        }
    }
    else 
        length = oct1;

    *value = p;
    *vsize = length;

    return type;
}

int main(int ac, char **av) {
    ssize_t size;
    uint8_t * der = read_file("public_key",&size);
    uint8_t *p = der;

    uint8_t *seq_payload;
    ssize_t seq_len;

    hexdump(der, size);
    uint8_t type = decode_tlv(der, size, &seq_payload, &seq_len);
    printf("Type=%02x length=%d\n",type,seq_len);
    hexdump(seq_payload, seq_len);

    return 1;
}
