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
#include "ids.h"
//#include "nbt.h"

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
    for(c=0; bitmask; bitmask>>=1)
        c += (bitmask&1);
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

    //printf("import_chunk %d,%d, pbm=%04x, abm=%04x, skylight=%d biomes=%d len=%zd\n",
    //       X,Z,pbm,abm,skylight,biomes,length);

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

void switch_dimension(uint8_t dim) {
    printf("Switching to %02x\n",dim);

    if (gs.opt.prune_chunks) {
        // no need to save anything, just delete current data
        if (P(gs.chunk)) free(P(gs.chunk));
    }
    else {
        // save the current chunk data to their respective variable
        switch (gs.current_dimension) {
            case DIM_OVERWORLD:
                C(gs.chunko) = C(gs.chunk);
                P(gs.chunko) = P(gs.chunk);
                break;
            case DIM_NETHER:
                C(gs.chunkn) = C(gs.chunk);
                P(gs.chunkn) = P(gs.chunk);
                break;
            case DIM_END:
                C(gs.chunke) = C(gs.chunk);
                P(gs.chunke) = P(gs.chunk);
            break;
        }
    }

    // restore data to the current variables
    switch (dim) {
        case DIM_OVERWORLD:
            C(gs.chunk) = C(gs.chunko);
            P(gs.chunk) = P(gs.chunko);
            break;
        case DIM_NETHER:
            C(gs.chunk) = C(gs.chunkn);
            P(gs.chunk) = P(gs.chunkn);
            break;
        case DIM_END:
            C(gs.chunk) = C(gs.chunke);
            P(gs.chunk) = P(gs.chunke);
            break;
    }
    
    gs.current_dimension = dim;
}

int get_chunks_dim(int *Xl, int *Xh, int *Zl, int *Zh) {
    int i;

    *Xl = *Xh = P(gs.chunk)[0].X;
    *Zl = *Zh = P(gs.chunk)[0].Z;

    for(i=0; i<C(gs.chunk); i++) {
        chunkid *c = P(gs.chunk)+i;
        if (c->X < *Xl) *Xl=c->X;
        if (c->X > *Xh) *Xh=c->X;
        if (c->Z < *Zl) *Zl=c->Z;
        if (c->Z > *Zh) *Zh=c->Z;
    }

    return C(gs.chunk);
}

uint8_t * export_cuboid(int Xl, int Xh, int Zl, int Zh, int yl, int yh) {
    int Xsz = (Xh-Xl+1);   // x-size of cuboid in chunks
    int Zsz = (Zh-Zl+1);   // z-size of cuboid in chunks
    int ysz = (yh-yl+1);   // y-size of cuboid in blocks
    int lsz = Xsz*Zsz*256; // size of a single layer
    ssize_t sz = ysz*lsz;  // total size of the cuboid in bytes

    //printf("Cuboid %d*%d*%d = %zd bytes\n",Xsz,Zsz,ysz,sz);
    lh_create_buf(data, sz);
    memset(data, 0xff, sz);

    int i,j,k;
    for(i=0; i<C(gs.chunk); i++) {
        chunkid *c = P(gs.chunk)+i;

        if (c->X<Xl || c->X>Xh || c->Z<Zl || c->Z>Zh)
            continue;

        // offset of this chunk's data where it will be placed in the cuboid
        int xoff = (c->X-Xl)*16;
        int zoff = (c->Z-Zl)*16;

        int boff = xoff + zoff*Xsz*16; // byte offset of the chunk's start in the cuboid buffer

        for(j=0; j<ysz; j++) {
            int yoff = (j+yl)*256; // byte offset of y slice in the chunk's block data
            int doff = boff + Xsz*Zsz*256*j;
            for(k=0; k<16; k++) {
                memcpy(data+doff, c->c->blocks+yoff, 16);
                yoff += 16;
                doff += Xsz*16;
            }            
        }        
    }

    return data;    
}

static inline int find_chunk(int X, int Z) {
    int i;
    for(i=0; i<C(gs.chunk); i++)
        if (P(gs.chunk)[i].X == X && P(gs.chunk)[i].Z == Z)
            return i;
    return -1;
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

int import_packet(uint8_t *ptr, ssize_t size, int is_client) {
    uint8_t *p = ptr;

    uint32_t type = lh_read_varint(p);
    uint32_t stype = 0x03000000|(is_client<<28)|type;

    switch (stype) {
        case CP_PlayerPosition: { // PlayerPosition
            Rdouble(x);
            Rdouble(feety);
            Rdouble(heady);
            Rdouble(z);
            Rchar(ground);

            gs.own.x = (int)(x*32);
            gs.own.y = (int)(nearbyint(feety)*32);
            gs.own.z = (int)(z*32);
            gs.own.dx = x;
            gs.own.dz = z;
            gs.own.dheady = heady;
            gs.own.dfeety = feety;
            gs.own.ground = ground;

            //printf("Player position: %d,%d,%d heady=%.2f feety=%.2f\n",
            //       gs.own.x/32,gs.own.y/32,gs.own.z/32,heady,feety);
            break;
        }
        case CP_PlayerLook: {
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);

            gs.own.yaw = yaw;
            gs.own.pitch = pitch;
            gs.own.ground = ground;
            break;
        }
        case CP_PlayerPositionLook: {
            Rdouble(x);
            Rdouble(feety);
            Rdouble(heady);
            Rdouble(z);
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);

            gs.own.x = (int)(x*32);
            gs.own.y = (int)(nearbyint(feety)*32);
            gs.own.z = (int)(z*32);
            gs.own.dx = x;
            gs.own.dz = z;
            gs.own.dheady = heady;
            gs.own.dfeety = feety;
            gs.own.ground = ground;

            gs.own.yaw = yaw;
            gs.own.pitch = pitch;

            //printf("Player position: %d,%d,%d\n",
            //       gs.own.x/32,gs.own.y/32,gs.own.z/32);
            break;
        }

        case SP_JoinGame: {
            Rint(pid);
            Rchar(gamemode);
            Rchar(dim);
            Rchar(diff);
            Rchar(maxpl);
            Rstr(leveltype);

            gs.own.id = pid;
            printf("Join Game pid=%d gamemode=%d dim=%d diff=%d maxpl=%d level=%s\n",
                   pid,gamemode,dim,diff,maxpl,leveltype);
            switch_dimension(dim);
            
            break;
        }

        case SP_Respawn: {
            Rint(dim);
            Rchar(diff);
            Rchar(maxpl);
            Rstr(leveltype);

            switch_dimension(dim);
            break;
        }

        case SP_PlayerPositionLook: {
            Rdouble(x);
            Rdouble(y);
            Rdouble(z);
            Rfloat(yaw);
            Rfloat(pitch);
            Rchar(ground);
            
            gs.own.x = (int)(x*32);
            gs.own.y = (int)(nearbyint(y-1.62)*32);
            gs.own.z = (int)(z*32);
            gs.own.dx = x;
            gs.own.dz = z;
            gs.own.dheady = y;
            gs.own.dfeety = y-1.62;

            gs.own.yaw = yaw;
            gs.own.pitch = pitch;
            gs.own.ground = ground;

            //printf("Player position: %d,%d,%d y from server=%.2f\n",
            //       gs.own.x/32,gs.own.y/32,gs.own.z/32,y);

            break;
        }

        case SP_SpawnPlayer: {
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

            //TODO: mark players hostile/neutral/friendly depending on the faglist
            break;
        }

        case SP_SpawnObject: {
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

        case SP_SpawnMob: {
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

            e->mtype = mtype;
            // mark all monster mobs hostile except pigmen (too dangerous) and bats (too cute)
            if (mtype >= 50 && mtype <90 && mtype!=57 && mtype!=65)
                e->hostile = 1;
            if (mtype == 50)
                e->hostile = 2; // mark creepers extra hostile to make them prio targets

            break;
        }

        case SP_SpawnPainting: {
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

        case SP_SpawnExperienceOrb: {
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

        case SP_DestroyEntities: {
            Rchar(count);
            int i,cnt=(unsigned int)count;
            for(i=0; i<cnt; i++) {
                Rint(eid);
                int idx = find_entity(eid);
                if (idx < 0) continue;

                lh_arr_delete(GAR(gs.entity),idx);
            }
            break;
        }

        case SP_EntityRelMove:
        case SP_EntityLookRelMove: {
            Rint(eid);
            Rchar(dx);
            Rchar(dy);
            Rchar(dz);

            int idx = find_entity(eid);
            if (idx<0) break;

            entity *e =P(gs.entity)+idx;
            e->x += (char)dx;
            e->y += (char)dy;
            e->z += (char)dz;
            break;
        }

        case SP_EntityTeleport: {
            Rint(eid);
            Rint(x);
            Rint(y);
            Rint(z);
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


        case SP_ChunkData: {
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

        case SP_MapChunkBulk: {
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
            
        case SP_MultiBlockChange: {
            Rint(X);
            Rint(Z);
            Rshort(nrec);
            Rint(dsize);

            int idx = find_chunk(X,Z);
            if (idx<0) {
                //printf("Cannot find chunk %d:%d\n",X,Z);
                break;
            }
            chunk *c = P(gs.chunk)[idx].c;

            int i;
            for(i=0; i<nrec; i++) {
                Rint(rec);
                int xr   = (rec>>28)&0x0f;
                int zr   = (rec>>24)&0x0f;
                int y    = (rec>>16)&0xff;
                int bid  = (rec>>4)&0xff;
                int meta = rec&0xf;

                idx = y*256+zr*16+xr;
                uint8_t *m = c->meta+idx/2;

                if (xr&1) {
                    *m &= 0x0f;
                    *m |= (meta<<4);
                }
                else {
                    *m &= 0xf0;
                    *m |= (meta&0x0f);
                }
            }
            break;
        }

        case SP_BlockChange: {
            Rint(x);
            Rchar(y);
            Rint(z);
            Rvarint(bid);
            Rchar(bmeta);
            //printf("BlockChange %d,%d,%d  id=%02x meta=%02x\n",x,y,z,bid,bmeta);

            bcoord bc = {x,z,y};
            ccoord cc = b2c(bc);

            int i = find_chunk(cc.X, cc.Z);
            if (i>=0) {
                P(gs.chunk)[i].c->blocks[cc.i] = bid;
                uint8_t * m = P(gs.chunk)[i].c->meta + cc.i/2;
                if (cc.i&1) {
                    *m &= 0x0f;
                    *m |= (bmeta<<4);
                }
                else {
                    *m &= 0xf0;
                    *m |= (bmeta&0x0f);
                }
            }
            else {
                //printf("Cannot find chunk %d,%d\n",cc.X,cc.Z);
            }

            break;
        }

        case SP_UpdateBlockEntity: {
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

        case SP_SetSlot: {
            Rchar(wid);
            Rshort(sid);
            Rshort(iid);

            // TODO: support other windows
            if (wid != 0) break;
            window * w = &gs.inventory;

            slot * s = &w->slots[sid];
            s->id = iid;

            if (iid != 0xffff) {
                Rchar(count);
                Rshort(dmg);
                s->count = count;
                s->dmg = dmg;

                //TODO: import aux data
                Rshort(dlen);
            }
            else {
                s->count=0;
                s->dmg=0;
            }
            break;
        }

        case SP_HeldItemChange: {
            Rchar(sid);
            gs.held = sid;
            break;
        }

        case CP_HeldItemChange: {
            Rshort(sid);
            gs.held = sid;
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
            gs.P(spawner)[i].type == SPAWNER_TYPE_UNKNOWN  ||
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
    printf(" Coords            Chunk  / Off   Ty Nearest\n");
    for(i=0; i<C(s); i++) {
        spawner *s = P(s)+i;
        bcoord bc = c2b(s->co);
        printf(" %5d,%2d,%5d  %4d:%4d/%5d  %d  %.1f\n",bc.x,bc.y,bc.z,
               s->co.X,s->co.Z,s->co.i,s->type, s->nearest);
    }
}

#define SQ(x) ((x)*(x))

int entity_in_range(entity * e, float range) {
    int sdist = SQ(gs.own.x-e->x)+SQ((gs.own.y+32)-e->y)+SQ(gs.own.z-e->z);
    sdist >>= 10;
    return ((float)sdist < SQ(range));
}

int get_entities_in_range(int *dst, int max, float range,
    int (*filt_pred)(entity *), int (*sort_pred)(entity *, entity *)) {

    int i,j;

    for(i=0,j=0; j<max && i<C(gs.entity); i++) {
        entity *e = P(gs.entity)+i;
        if (entity_in_range(e,range))
            if (filt_pred && filt_pred(e))
                dst[j++] = i;
    }

    //TODO: sorting

    return j;
}
