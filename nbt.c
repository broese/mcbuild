#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lh_debug.h>
#include <lh_buffers.h>
#include <lh_bytes.h>
#include <lh_files.h>

#include "nbt.h"

void nbt_dump_ind(nbt_t *nbt, int indent) {
    char ind[4096];
    memset(ind, ' ', indent);
    ind[indent] = 0;
    printf("%s",ind);

    int i;

    const char *name = nbt->name ? nbt->name : "<anon>";

    switch (nbt->type) {
        case NBT_BYTE:
            printf("Byte '%s'=%d\n",name,nbt->b);
            break;

        case NBT_SHORT:
            printf("Short '%s'=%d\n",name,nbt->s);
            break;

        case NBT_INT:
            printf("Int '%s'=%d\n",name,nbt->i);
            break;

        case NBT_LONG:
            printf("Long '%s'=%jd\n",name,nbt->l);
            break;

        case NBT_FLOAT:
            printf("Float '%s'=%f\n",name,nbt->f);
            break;

        case NBT_DOUBLE:
            printf("Double '%s'=%f\n",name,nbt->d);
            break;

        case NBT_BYTE_ARRAY:
            printf("Byte Array '%s'[%d] = { ",name,nbt->count);
            for(i=0; i<nbt->count; i++)
                printf("%s%d",i?", ":"",nbt->ba[i]);
            printf(" }\n");
            break;

        case NBT_INT_ARRAY:
            printf("Int Array '%s'[%d] = { ",name,nbt->count);
            for(i=0; i<nbt->count; i++)
                printf("%s%d",i?", ":"",nbt->ia[i]);
            printf(" }\n");
            break;

        case NBT_STRING:
            printf("String '%s'=\"%s\"\n",name,nbt->st);
            break;

        case NBT_LIST:
            printf("List '%s' = [\n",name);
            for(i=0; i<nbt->count; i++)
                nbt_dump_ind(nbt->li[i], indent+2);
            printf("%s]\n",ind);
            break;

        case NBT_COMPOUND:
            printf("Compound '%s' = {\n",name);
            for(i=0; i<nbt->count; i++)
                nbt_dump_ind(nbt->co[i], indent+2);
            printf("%s}\n",ind);
            break;
    }
}

void nbt_dump(nbt_t *nbt) {
    nbt_dump_ind(nbt, 0);
}

nbt_t * nbt_parse_type(uint8_t **p, uint8_t type, int named) {
    assert(type);
    int i;

    // allocate NBT object
    lh_create_obj(nbt_t,nbt);
    nbt->type = type;
    nbt->count = 1;

    // read object's name
    if (named) {
        int16_t slen = lh_read_short_be(*p);
        lh_alloc_buf(nbt->name, slen+1);
        memmove(nbt->name, *p, slen);
        *p += slen;
    }

    // read the payload
    switch (type) {
        case NBT_BYTE:
            nbt->b = lh_read_char(*p);
            break;

        case NBT_SHORT:
            nbt->s = lh_read_short_be(*p);
            break;

        case NBT_INT:
            nbt->i = lh_read_int_be(*p);
            break;

        case NBT_LONG:
            nbt->l = lh_read_long_be(*p);
            break;

        case NBT_FLOAT:
            nbt->f = lh_read_float_be(*p);
            break;

        case NBT_DOUBLE:
            nbt->d = lh_read_double_be(*p);
            break;

        case NBT_BYTE_ARRAY:
            nbt->count = lh_read_int_be(*p);
            lh_alloc_num(nbt->ba, nbt->count);
            memmove(nbt->ba, *p, nbt->count);
            *p += nbt->count;
            break;

        case NBT_INT_ARRAY:
            nbt->count = lh_read_int_be(*p);
            lh_alloc_num(nbt->ia, nbt->count);
            for(i=0; i<nbt->count; i++)
                nbt->ia[i] = lh_read_int_be(*p);
            break;

        case NBT_STRING:
            nbt->count = (uint16_t)lh_read_short_be(*p);
            lh_alloc_buf(nbt->st, nbt->count+1);
            memmove(nbt->st, *p, nbt->count);
            *p += nbt->count;
            break;

        case NBT_LIST: {
            uint8_t ltype = lh_read_char(*p);
            nbt->count = lh_read_int_be(*p);
            lh_alloc_num(nbt->li, nbt->count);

            for(i=0; i<nbt->count; i++)
                nbt->li[i] = nbt_parse_type(p, ltype, 0);

            break;
        }

        case NBT_COMPOUND: {
            nbt->count = 0;
            uint8_t ctype;
            while(ctype=lh_read_char(*p)) {
                lh_resize(nbt->co, nbt->count+1);
                nbt->co[nbt->count] = nbt_parse_type(p, ctype, 1);
                nbt->count++;
            }
            break;
        }
    }

    return nbt;
}

nbt_t * nbt_parse(uint8_t **p) {
    uint8_t type = lh_read_char(*p);
    if (type == NBT_END) return NULL;
    return nbt_parse_type(p, type, 1);
}

void nbt_free(nbt_t *nbt) {
    int i;

    switch (nbt->type) {
        case NBT_BYTE_ARRAY: lh_free(nbt->ba); break;
        case NBT_INT_ARRAY:  lh_free(nbt->ia); break;
        case NBT_STRING:     lh_free(nbt->st); break;
        case NBT_LIST:
            for(i=0; i<nbt->count; i++)
                nbt_free(nbt->li[i]);
            lh_free(nbt->li);
            break;
        case NBT_COMPOUND:
            for(i=0; i<nbt->count; i++)
                nbt_free(nbt->co[i]);
            lh_free(nbt->co);
            break;
    }
    lh_free(nbt->name);
    lh_free(nbt);
}

nbt_t * nbt_clone(nbt_t *src) {
    if (!src) return NULL;

    // allocate NBT object
    lh_create_obj(nbt_t,dst);

    // copy static elements
    dst->type = src->type;
    dst->count = src->count;
    if (src->name) dst->name = strdup(src->name);

    int i;

    // copy data
    switch (src->type) {
        case NBT_BYTE:
            dst->b = src->b;
            break;

        case NBT_SHORT:
            dst->s = src->s;
            break;

        case NBT_INT:
            dst->i = src->i;
            break;

        case NBT_LONG:
            dst->l = src->l;
            break;

        case NBT_FLOAT:
            dst->f = src->f;
            break;

        case NBT_DOUBLE:
            dst->d = src->d;
            break;

        case NBT_BYTE_ARRAY:
            lh_alloc_num(dst->ba, dst->count);
            memmove(dst->ba, src->ba, dst->count);
            break;

        case NBT_INT_ARRAY:
            lh_alloc_num(dst->ia, dst->count);
            memmove(dst->ia, src->ia, dst->count*sizeof(*dst->ia));
            break;

        case NBT_STRING:
            dst->st = strdup(src->st);
            break;

        case NBT_LIST:
            lh_alloc_num(dst->li, dst->count);
            for(i=0; i<dst->count; i++)
                dst->li[i] = nbt_clone(src->li[i]);
            break;

        case NBT_COMPOUND:
            lh_alloc_num(dst->co, dst->count);
            for(i=0; i<dst->count; i++)
                dst->co[i] = nbt_clone(src->co[i]);
            break;
    }

    return dst;
}

void nbt_write(uint8_t **w, nbt_t *nbt) {
    if (!nbt) {
        lh_write_char(*w, 0);
        return;
    }

    // if the object is unnamed - don't write type or name
    if (nbt->name) {
        lh_write_char(*w, nbt->type);
        ssize_t nlen = strlen(nbt->name);
        lh_write_short_be(*w, nlen);
        memmove(*w, nbt->name, nlen);
        *w += nlen;
    }

    int i;
    switch (nbt->type) {
        case NBT_BYTE:
            lh_write_char(*w, nbt->b);
            break;

        case NBT_SHORT:
            lh_write_short_be(*w, nbt->s);
            break;

        case NBT_INT:
            lh_write_int_be(*w, nbt->i);
            break;

        case NBT_LONG:
            lh_write_long_be(*w, nbt->l);
            break;

        case NBT_FLOAT:
            lh_write_float_be(*w, nbt->f);
            break;

        case NBT_DOUBLE:
            lh_write_double_be(*w, nbt->d);
            break;

        case NBT_BYTE_ARRAY:
            lh_write_int_be(*w, nbt->count);
            memmove(*w, nbt->ba, nbt->count);
            *w += nbt->count;
            break;

        case NBT_INT_ARRAY:
            lh_write_int_be(*w, nbt->count);
            for(i=0; i<nbt->count; i++)
                lh_write_int_be(*w, nbt->ia[i]);
            break;

        case NBT_STRING:
            lh_write_short_be(*w, strlen(nbt->st));
            memmove(*w, nbt->st, nbt->count);
            *w += nbt->count;
            break;

        case NBT_LIST: {
            uint8_t ltype = nbt->li[0]->type;
            lh_write_char(*w, ltype);
            lh_write_int_be(*w, nbt->count);

            for(i=0; i<nbt->count; i++)
                nbt_write(w, nbt->li[i]);

            break;
        }

        case NBT_COMPOUND: {
            for(i=0; i<nbt->count; i++)
                nbt_write(w, nbt->li[i]);
            lh_write_char(*w, NBT_END);
            break;
        }
    }
}

#if TEST

int main(int ac, char **av) {
    if (!av[1]) {
        printf("Usage: %s <file.nbt>\n",av[0]);
        exit(1);
    }

    uint8_t * buf;
    ssize_t sz = lh_load_alloc(av[1], &buf);
    hexdump(buf, sz);

    uint8_t *p = buf;
    nbt_t * nbt = nbt_parse(&p);
    printf("Consumed %zd bytes\n",p-buf);
    nbt_dump(nbt);

    nbt_t * newnbt = nbt_clone(nbt);
    nbt_dump(newnbt);

    uint8_t wbuf[65536];
    uint8_t *w = wbuf;
    nbt_write(&w, newnbt);
    ssize_t szw = w-wbuf;
    hexdump(wbuf, szw);

    nbt_free(nbt);
    nbt_free(newnbt);

    lh_free(buf);
}

#endif
