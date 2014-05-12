#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define LH_DECLARE_SHORT_NAMES 1

#include <lh_debug.h>
#include <lh_bytes.h>
#include <lh_files.h>
#include <lh_buffers.h>
#include <lh_compress.h>

#include "gamestate.h"
#include "nbt.h"

////////////////////////////////////////////////////////////////////////////////

gamestate gs;

static int gs_used = 0;

////////////////////////////////////////////////////////////////////////////////

int reset_gamestate() {
    int i;

    if (gs_used) {
        // delete cached chunks
        for(i=0; i<gs.C(chunk); i++)
            if (gs.P(chunk)[i].c)
                free(gs.P(chunk)[i].c);
        free(gs.P(chunk));
    }
    
    CLEAR(gs);
    gs_used = 1;

    return 0;
}

int set_option(int optid, int value) {
    switch (optid) {
    case GSOP_PRUNE_CHUNKS:
        gs.opt.prune_chunks = value;
        break;
    case GSOP_SEARCH_SPAWNERS:
        gs.opt.search_spawners = value;
        break;
    case GSOP_TRACK_ENTITIES:
        gs.opt.track_entities = value;
        break;

    default:
        LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }

    return 0;
}

int get_option(int optid) {
    switch (optid) {
    case GSOP_PRUNE_CHUNKS:
        return gs.opt.prune_chunks;
    case GSOP_SEARCH_SPAWNERS:
        return gs.opt.search_spawners;
    case GSOP_TRACK_ENTITIES:
        return gs.opt.track_entities;

    default:
        LH_ERROR(-1,"Unknown option ID %d\n", optid);
    }
}

////////////////////////////////////////////////////////////////////////////////

static uint8_t * read_string(uint8_t *p, char *s) {
    uint32_t len = lh_read_varint(p);
    memmove(s, p, len);
    s[len] = 0;
    return p+len;
}

#define Rx(n,type,fun) type n = lh_read_ ## fun ## _be(p);

#define Rchar(n)  Rx(n,uint8_t,char)
#define Rshort(n) Rx(n,uint16_t,short)
#define Rint(n)   Rx(n,uint32_t,int)
#define Rlong(n)  Rx(n,uint64_t,long);
#define Rfloat(n) Rx(n,float,float)
#define Rdouble(n) Rx(n,double,double)
#define Rstr(n)   char n[4096]; p=read_string(p,n)
#define Rskip(n)  p+=n;
#define Rvarint(n) uint32_t n = lh_read_varint(p);

////////////////////////////////////////////////////////////////////////////////

static inline int count_bits(uint32_t bitmask) {
    int i,c=0;
    while(bitmask) {
        c++;
        bitmask>>=1;
    }
    return c;
}

int get_chunk_idx(int X, int Z) {
    int i;
    for(i=0; i<gs.C(chunk); i++)
        if (gs.P(chunk)[i].X==X && gs.P(chunk)[i].Z==Z)
            return i;
    return -1;
}

int import_chunk(int X, int Z, uint16_t pbm, uint16_t abm, int skylight, int biomes, uint8_t *data, ssize_t length) {
    int idx = get_chunk_idx(X,Z);
    chunkid * cid;
    if (idx<0) {
        cid = lh_arr_new_c(GAR(gs.chunk));
    }
    else {
        cid = gs.P(chunk)+idx;
    }

    cid->X = X;
    cid->Z = Z;
    lh_alloc_obj(cid->c);
    chunk *c = cid->c;

    c->X = X;
    c->Z = Z;

    uint8_t *p = data;
    uint16_t bm;
    int i;

    // blocks
    for(i=0,bm=pbm; bm; i++,bm>>=1) {
        if (bm&1) {
            memmove(c->blocks+i*4096,p,4096);
            p += 4096;
        }
    }

    // block metadata
    for(i=0,bm=pbm; bm; i++,bm>>=1) {
        if (bm&1) {
            memmove(c->meta+i*2048,p,2048);
            p += 2048;
        }
    }

    // block light
    for(i=0,bm=pbm; bm; i++,bm>>=1) {
        if (bm&1) {
            memmove(c->light+i*2048,p,2048);
            p += 2048;
        }
    }

    // block skylight
    if (skylight) {
        for(i=0,bm=pbm; bm; i++,bm>>=1) {
            if (bm&1) {
                memmove(c->skylight+i*2048,p,2048);
                p += 2048;
            }
        }
    }

    // add block data
    for(i=0,bm=abm; bm; i++,bm>>=1) {
        if (bm&1) {
            memmove(c->add+i*2048,p,2048);
            p += 2048;
        }
    }

    // biomes
    if (biomes)
        memmove(c->biome,p,256);


    // do various updates based on new chunk data
    if (gs.opt.search_spawners) {
        update_spawners(c);
    }

    return 0;
}

int prune_chunk(int X, int Z) {
    int i = get_chunk_idx(X,Z);
    if (i<0) return -1;

    if (gs.P(chunk)[i].c) free(gs.P(chunk)[i].c);
    lh_arr_delete(GAR(gs.chunk),i);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

int cmp_cc(ccoord a, ccoord b) {
    if (a.Z < b.Z) return -1;
    if (a.Z > b.Z) return  1;
    if (a.X < b.X) return -1;
    if (a.X > b.X) return  1;
    if (a.i < b.i) return -1;
    if (a.i > b.i) return  1;
    return 0;
}

#define SQ(x) ((x)*(x))

static inline float dist_cc(ccoord a, ccoord b) {
    bcoord A = c2b(a);
    bcoord B = c2b(b);
    return sqrt(SQ(A.x-B.x)+SQ(A.z-B.z)+SQ(A.y-B.y));
}

int update_spawners(chunk *c) {
    int i,j;
    //printf("Searching for spawners in chunk %d,%d\n",c->X,c->Z);
    for(i=0; i<65536; i++) {
        if (c->blocks[i] == 0x34) {
            spawner *s;
            ccoord co = { c->X, c->Z, i };
            int cmp = -1;

            for(j=0; j<gs.C(spawner); j++) {
                s = gs.P(spawner)+j;
                cmp = cmp_cc(s->co,co);
                if (cmp >= 0) break;
            }
            if (cmp==0) continue; // this spawner was already in the list, skip it

            // create a new slot in the list at the sorted position
            if (j >= gs.C(spawner))
                s = lh_arr_new_c(GAR(gs.spawner));
            else
                s = lh_arr_insert_c(GAR(gs.spawner), j);

            s->co = co;
            s->type = -1;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

static inline int find_entity(int eid) {
    int i;
    for(i=0; i<C(gs.entity); i++)
        if (P(gs.entity)[i].id == eid)
            return i;
    return -1;
}

////////////////////////////////////////////////////////////////////////////////

int import_packet(uint8_t *ptr, ssize_t size) {
    uint8_t *p = ptr;

    uint32_t type = lh_read_varint(p);
    switch (type) {
        ///// Entity data

        case 0x0c: { // SpawnPlayer
            Rvarint(eid);
            Rstr(uuid);
            Rstr(name);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(yaw);
            Rchar(pitch);
            Rshort(item);
            //TODO: metadata

            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = eid;
            e->x = x;
            e->y = y;
            e->z = z;
            e->type = ENTITY_PLAYER;
            sprintf(e->name, "%s", name);
            break;
        }

        case 0x0e: { // SpawnObject
            Rvarint(eid);
            Rchar(mtype);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(pitch);
            Rchar(yaw);
            //TODO: data

            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = eid;
            e->x = x;
            e->y = y;
            e->z = z;
            e->type = ENTITY_OBJECT;
            break;
        }

        case 0x0f: { // SpawnMob
            Rvarint(eid);
            Rchar(mtype);
            Rint(x);
            Rint(y);
            Rint(z);
            Rchar(pitch);
            Rchar(hpitch);
            Rchar(yaw);
            Rshort(vx);
            Rshort(vy);
            Rshort(vz);
            //TODO: metadata

            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = eid;
            e->x = x;
            e->y = y;
            e->z = z;
            e->type = ENTITY_MOB;
            break;
        }

        case 0x10: { // SpawnPainting
            Rvarint(eid);
            Rstr(name);
            Rint(x);
            Rint(y);
            Rint(z);
            Rint(dir);

            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = eid;
            e->x = x;
            e->y = y;
            e->z = z;
            e->type = ENTITY_OTHER;
            sprintf(e->name, "%s", name);
            break;
        }

        case 0x11: { // SpawnExperienceOrb
            Rvarint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
            Rshort(exp);

            entity *e = lh_arr_new_c(GAR(gs.entity));
            e->id = eid;
            e->x = x;
            e->y = y;
            e->z = z;
            e->type = ENTITY_OTHER;
            break;
        }

        case 0x13: { // DestroyEntities
            Rchar(count);
            int i,cnt=(unsigned int)count;
            for(i=0; i<cnt; i++) {
                Rint(eid);
                int idx = find_entity(eid);
                if (idx < 0) continue;

                lh_arr_delete_c(GAR(gs.entity),idx);
            }
            break;
        }

        case 0x15:   // EntityRelMove
        case 0x17: { // EntityLookRelMove
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);

            int idx = find_entity(eid);
            if (idx<0) break;

            entity *e =P(gs.entity)+idx;
            e->x += (int)dx;
            e->y += (int)dy;
            e->z += (int)dz;
            break;
        }

        case 0x18: { // EntityTeleport
            Rint(eid);
            Rchar(x);
            Rchar(y);
            Rchar(z);
            Rchar(yaw);
            Rchar(pitch);

            int idx = find_entity(eid);
            if (idx<0) break;

            entity *e =P(gs.entity)+idx;
            e->x = x;
            e->y = y;
            e->z = z;
            break;
        }


        ///// Chunk data
        case 0x21: {
            Rint(X);
            Rint(Z);
            Rchar(cont);
            Rshort(pbm);
            Rshort(abm);
            Rint(dsize);

            if (pbm) {
                ssize_t clen=0;
                uint8_t *chunk = lh_zlib_decode(p,dsize,&clen);
                //printf("ChunkData %d:%d pbm=%04x abm=%04x len=%zd\n",X,Z,pbm,abm,clen);
                import_chunk(X,Z,pbm,abm,1,cont,chunk,clen);
            }
            else if (gs.opt.prune_chunks) {
                //printf("ChunkData %d:%d PRUNE\n",X,Z);
                prune_chunk(X,Z);
            }
            
            break;
        }

        case 0x26: {
            Rshort(nchunks);
            Rint(dsize);
            Rchar(skylight);

            // compressed chunk data
            ssize_t clen=0;
            uint8_t *chunks = lh_zlib_decode(p,dsize,&clen);
            //printf("MapChunkBulk skylight=%d nchunks=%d clen=%zd\n",skylight,nchunks,clen);
            Rskip(dsize);

            // metadata - chunk descriptions
            int i;
            uint8_t *cptr = chunks;
            for(i=0; i<nchunks; i++) {
                Rint(X);
                Rint(Z);
                Rshort(pbm);
                Rshort(abm);

                if (!pbm) {
                    fprintf(stderr, "Warning: chunk %d:%d with pbm=0\n",X,Z);
                    return 0;
                }

                // warning: no support for add-blocks
                if (abm) LH_ERROR(-1, "Add bitmap is non-zero for chunk %d:%d\n",X,Z);

                // note: biome array is always present in MapChunkBulk
                ssize_t length = count_bits(pbm)*(4096 + 2048 + 2048 + (skylight?2048:0))+
                                 count_bits(abm)*2048 + 256;

                //printf("  %d:%d pbm=%04x abm=%04x len=%zd\n",X,Z,pbm,abm,length);
                
                import_chunk(X,Z,pbm,abm,skylight,1,cptr,length);
                cptr += length;
            }
            free(chunks);
            
            break;
        }

        case 0x23: {
            Rint(x);
            Rchar(y);
            Rint(z);
            Rvarint(bid);
            Rchar(bmeta);
            //printf("BlockChange %d,%d,%d  id=%02x meta=%02x\n",x,y,z,bid,bmeta);
            break;
        }

        case 0x35: {
            Rint(x);
            Rshort(y);
            Rint(z);
            Rchar(action);
            Rshort(dlen);
            //printf("UpdateBlockEntity %d,%d,%d action=%d dlen=%d\n",x,y,z,action,dlen);
            if (dlen > 0) {
                ssize_t clen;
                uint8_t *edata = lh_gzip_decode(p,dlen,&clen);

                int j;
                bcoord bc = { .x=x, .y=y, .z=z };
                ccoord cc = b2c(bc);
                for(j=0; j<gs.C(spawner); j++) {
                    spawner *s = gs.P(spawner)+j;
                    if (s->co.X == cc.X && s->co.Z == cc.Z && s->co.i == cc.i) {
                        p=edata+162;
                        Rstr(ename);
                        if      (!strcmp(ename,"Zombie"))       { s->type = SPAWNER_TYPE_ZOMBIE; }
                        else if (!strcmp(ename,"Skeleton"))     { s->type = SPAWNER_TYPE_SKELETON; }
                        else if (!strcmp(ename,"Spider"))       { s->type = SPAWNER_TYPE_SPIDER; }
                        else if (!strcmp(ename,"CaveSpider"))   { s->type = SPAWNER_TYPE_CAVESPIDER; }
                        else                                    { s->type = SPAWNER_TYPE_UNKNOWN; }
                        break;
                    }
                }

                free(edata);
            }
            break;
        }
    }    
}

////////////////////////////////////////////////////////////////////////////////

int search_spawners() {
    int i,j;
    
    lh_arr_declare_i(spawner, s);

    // create a filtered list of spawners - only include skeletons
    for(i=0; i<gs.C(spawner); i++)
        if (
            //gs.P(spawner)[i].type == SPAWNER_TYPE_ZOMBIE   ||
            //gs.P(spawner)[i].type == SPAWNER_TYPE_UNKNOWN  ||
            gs.P(spawner)[i].type == SPAWNER_TYPE_SKELETON
            )
            *lh_arr_new(GAR(s)) = gs.P(spawner)[i];
    
    for(i=0; i<C(s); i++)
        P(s)[i].nearest = 1000;

    // calculate distances to the nearest spawner for every spawner
    for(i=0; i<C(s); i++)
        for(j=0; j<C(s); j++)
            if (i!=j) {
                float dist = dist_cc(P(s)[i].co,P(s)[j].co);
                if (dist < P(s)[i].nearest)
                    P(s)[i].nearest = dist;
            }

    // eliminate spawners that have no close neighbors
    for(i=0; i<C(s); ) {
        if (P(s)[i].nearest > 31) {
            lh_arr_delete(GAR(s),i);
        }
        else {
            i++;
        }
    }
                
    printf("Dumping %zd spawners\n",C(s));
    for(i=0; i<C(s); i++) {
        spawner *s = P(s)+i;
        bcoord bc = c2b(s->co);
        printf(" %d,%d,%d  %d:%d/%5d  %d  %.1f\n",bc.x,bc.y,bc.z,
               s->co.X,s->co.Z,s->co.i,s->type, s->nearest);
    }
}
