/*
 Authors:
 Copyright 2012-2015 by Eduard Broese <ed.broese@gmx.de>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version
 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <lh_debug.h>
#include <lh_buffers.h>
#include <lh_bytes.h>
#include <lh_files.h>

#include "nbt.h"

////////////////////////////////////////////////////////////////////////////////
// dumping

static void nbt_dump_ind(nbt_t *nbt, int indent) {
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

// print NBT contents - entry point into recursive indentation
void nbt_dump(nbt_t *nbt) {
    if (!nbt) return;
    nbt_dump_ind(nbt, 0);
}

////////////////////////////////////////////////////////////////////////////////
// serialization

// parse the payload of a known NBT token type
// we need this separation of functions to be able to parse
// the "headless" tokens in the arrays
static nbt_t * nbt_parse_type(uint8_t **p, uint8_t type, int named) {
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
            while( (ctype=lh_read_char(*p)) ) {
                lh_resize(nbt->co, nbt->count+1);
                nbt->co[nbt->count] = nbt_parse_type(p, ctype, 1);
                nbt->count++;
            }
            break;
        }
    }

    return nbt;
}

// parse NBT object from serialized data
nbt_t * nbt_parse(uint8_t **p) {
    uint8_t type = lh_read_char(*p);
    if (type == NBT_END) return NULL;
    return nbt_parse_type(p, type, 1);
}

// serialize NBT object to a buffer
//FIXME: this function assumes the output buffer has sufficient size
//(typically, it will be the MAXPLEN (4MiB) buffer in mcproxy used for packet encoding)
void nbt_write(uint8_t **w, nbt_t *nbt) {
    if (!nbt) {
        lh_write_char(*w, NBT_END); // write terminating token
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

////////////////////////////////////////////////////////////////////////////////
// management

// properly deallocate NBT objects
void nbt_free(nbt_t *nbt) {
    if (!nbt) return;

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

// deep-copy clone of NBT objects
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


////////////////////////////////////////////////////////////////////////////////
// element access

// access an element in the compound by name
nbt_t * nbt_hget(nbt_t *nbt, const char *name) {
    if (!nbt) return NULL;
    if (nbt->type != NBT_COMPOUND) return NULL;

    int i;
    for(i=0; i<nbt->count; i++) {
        const char *elname = nbt->co[i]->name;
        if (elname && !strcmp(elname, name))
            return nbt->co[i];
    }
    return NULL;
}

// access an element in an array by index
nbt_t * nbt_aget(nbt_t *nbt, int idx) {
    if (!nbt) return NULL;
    if (nbt->type != NBT_LIST || idx>=nbt->count) return NULL;
    return nbt->li[idx];
}

////////////////////////////////////////////////////////////////////////////////
// construction

// create a new NBT object of given type and name
// name can be set to NULL to create an unnamed object
// values are specified in the variable argument list depending on type:
// all basic types : value
// byte/int array  : type *, count
//   - the data will be copied
// list            : count, obj0, obj1, obj2, ...
//   - first object defines the list type
//   - named objects will retain their name
// compoiund       : count, obj0, obj1, obj2, ...
//   - element names are taken from inserted objects
nbt_t * nbt_new(int type, const char *name, ...) {
    assert(type>NBT_END && type<=NBT_INT_ARRAY);

    lh_create_obj(nbt_t, nbt);
    nbt->type = type;
    nbt->name = name ? strdup(name) : NULL;
    nbt->count = 1;

    va_list ap;
    va_start(ap, name);

    switch (type) {
        case NBT_BYTE:
            nbt->b = va_arg(ap, int32_t);
            break;
        case NBT_SHORT:
            nbt->s = va_arg(ap, int32_t);
            break;
        case NBT_INT:
            nbt->i = va_arg(ap, int32_t);
            break;
        case NBT_LONG:
            nbt->l = va_arg(ap, int64_t);
            break;
        case NBT_FLOAT:
            nbt->f = va_arg(ap, double);
            break;
        case NBT_DOUBLE:
            nbt->d = va_arg(ap, double);
            break;
        case NBT_BYTE_ARRAY: {
            int8_t *values = va_arg(ap, int8_t *);
            nbt->count = va_arg(ap, int);
            lh_alloc_buf(nbt->ba, nbt->count);
            memmove(nbt->ba, values, nbt->count);
            break;
        }
        case NBT_STRING:
            nbt->st = strdup(va_arg(ap, const char *));
            nbt->count = strlen(nbt->st);
            break;
        case NBT_LIST: {
            nbt->count = va_arg(ap, int);
            lh_alloc_num(nbt->li, nbt->count);
            int i;
            for(i=0; i<nbt->count; i++)
                nbt->li[i] = va_arg(ap, nbt_t *);
            break;
        }
        case NBT_COMPOUND: {
            nbt->count = va_arg(ap, int);
            lh_alloc_num(nbt->co, nbt->count);
            int i;
            for(i=0; i<nbt->count; i++)
                nbt->co[i] = va_arg(ap, nbt_t *);
            break;
        }
        case NBT_INT_ARRAY: {
            int32_t *values = va_arg(ap, int32_t *);
            nbt->count = va_arg(ap, int);
            lh_alloc_num(nbt->ia, nbt->count);
            memmove(nbt->ia, values, nbt->count*sizeof(*values));
            break;
        }
    }

    va_end(ap);
    return nbt;
}

////////////////////////////////////////////////////////////////////////////////
// test routines

#if TEST

int main(int ac, char **av) {
#if 0
    if (!av[1]) {
        printf("Usage: %s <file.nbt>\n",av[0]);
        exit(1);
    }

    uint8_t * buf;
    ssize_t sz = lh_load_alloc(av[1], &buf);
    //hexdump(buf, sz);

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
    //hexdump(wbuf, szw);

    nbt_t *nested = nbt_hget(nbt, "nested compound test");
    nbt_dump(nested);

    nbt_free(nbt);
    nbt_free(newnbt);

    lh_free(buf);
#endif

    nbt_t * b = nbt_new(NBT_BYTE,   "MyByte",   12);
    nbt_t * s = nbt_new(NBT_SHORT,  "MyShort",  1234);
    nbt_t * i = nbt_new(NBT_INT,    "MyInt",    12345678);
    nbt_t * l = nbt_new(NBT_LONG,   "MyLong",   123456789123456789LL);
    nbt_t * f = nbt_new(NBT_FLOAT,  "MyFloat",  1.234);
    nbt_t * d = nbt_new(NBT_DOUBLE, "MyDouble", 1.234567890123456789);
    nbt_t * ba = nbt_new(NBT_BYTE_ARRAY, "MyByteArray", "ABCDEFGH", 8);
    nbt_t * st = nbt_new(NBT_STRING, "MyString", "This is a test string!");
    nbt_t * ia = nbt_new(NBT_INT_ARRAY, "MyIntArray",
                         (int[]){ 1, 1, 2, 3, 5, 8, 13, 21, 34, 55 }, 10);
    nbt_t * li = nbt_new(NBT_LIST,  "MyList", 5,
                         nbt_new(NBT_INT, NULL, 1),
                         nbt_new(NBT_INT, NULL, 2),
                         nbt_new(NBT_INT, NULL, 3),
                         nbt_new(NBT_INT, NULL, 4),
                         nbt_new(NBT_INT, NULL, 5));
    nbt_t * co = nbt_new(NBT_COMPOUND, "MyCompound", 10,
                         b, s, i, l, f, d, ba, st, ia, li);

    nbt_dump(b);
    nbt_dump(s);
    nbt_dump(i);
    nbt_dump(l);
    nbt_dump(f);
    nbt_dump(d);
    nbt_dump(ba);
    nbt_dump(st);
    nbt_dump(ia);
    nbt_dump(li);
    nbt_dump(co);

#if 0
    nbt_free(b);
    nbt_free(s);
    nbt_free(i);
    nbt_free(l);
    nbt_free(f);
    nbt_free(d);
    nbt_free(ba);
    nbt_free(st);
    nbt_free(ia);
    nbt_free(li);
#endif
    nbt_free(co);
}

#endif
