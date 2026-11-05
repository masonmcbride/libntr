/* SPDX-License-Identifier: MIT */
#include <simulator/g3_native_math.h>
#include <string.h>

static int32_t narrow(uint64_t sum, unsigned shift)
{
    /* Only low 32 bits after shifting are retained, so logical shift has
     * identical low bits to arithmetic shift for shift <=32. */
    uint32_t bits = (uint32_t)(sum >> shift);
    int32_t result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}
static uint64_t product(int32_t a, int32_t b)
{
    return (uint64_t)((int64_t)a * b);
}
void G3Native_Identity(G3NativeMatrix *out)
{
    memset(out, 0, sizeof(*out));
    for (unsigned i=0; i<4; ++i) out->m[i][i] = 4096;
}
void G3Native_MultiplyScalar(G3NativeMatrix *out, const G3NativeMatrix *a,
                            const G3NativeMatrix *b, size_t count)
{
    for (size_t n=0; n<count; ++n) {
        G3NativeMatrix tmp;
        for (unsigned r=0; r<4; ++r)
            for (unsigned c=0; c<4; ++c) {
                uint64_t sum=0;
                for (unsigned k=0; k<4; ++k) sum += product(a[n].m[r][k],b[n].m[k][c]);
                tmp.m[r][c]=narrow(sum,12);
            }
        out[n]=tmp;
    }
}
void G3Native_TransformScalar(G3NativeVector *out, const G3NativeVector *in,
                             size_t count, const G3NativeMatrix *matrix)
{
    for (size_t n=0; n<count; ++n) {
        G3NativeVector tmp;
        for (unsigned c=0; c<4; ++c) {
            uint64_t sum=0;
            for (unsigned k=0; k<4; ++k) sum += product(in[n].v[k],matrix->m[k][c]);
            tmp.v[c]=narrow(sum,12);
        }
        out[n]=tmp;
    }
}
static int32_t dot9(const int32_t *a, const int32_t *b)
{
    uint64_t sum=0;
    for (unsigned k=0; k<3; ++k) sum += product(a[k],b[k]);
    return narrow(sum,9);
}
void G3Native_LightScalar(uint8_t (*out)[3], const G3NativeVector *normal,
                         size_t count, const G3NativeLighting *lights)
{
    for (size_t n=0; n<count; ++n) {
        int color[3];
        for (unsigned c=0;c<3;++c) color[c]=lights->emission[c];
        for (unsigned l=0;l<4;++l) if (lights->enabled & (1u<<l)) {
            int d=-dot9(normal[n].v, lights->direction[l]);
            int s=-dot9(normal[n].v, lights->half[l]);
            if(d<0)d=0; if(d>511)d=511;
            if(s<0)s=0; if(s>511)s=511;
            s=(s*s)>>9;
            if(lights->use_shininess) s=lights->shininess[s>>2]<<1;
            for(unsigned c=0;c<3;++c) {
                color[c] += (lights->specular[c]*lights->color[l][c]*s)>>14;
                color[c] += (lights->diffuse[c]*lights->color[l][c]*d)>>14;
                color[c] += (lights->ambient[c]*lights->color[l][c])>>5;
            }
        }
        for(unsigned c=0;c<3;++c) out[n][c]=(uint8_t)(color[c]>31?31:color[c]);
    }
}
static int64_t distance(const G3NativeVertex *v, unsigned axis, int sign)
{
    return (int64_t)v->position[3] - (int64_t)sign*v->position[axis];
}
static G3NativeVertex intersection(const G3NativeVertex *a, const G3NativeVertex *b,
                                   int64_t da, int64_t db, unsigned axis, int sign)
{
    G3NativeVertex v={0};
    /* Wide temporary avoids host overflow, division truncates toward zero.
     * Keep the inside endpoint as the interpolation origin. */
#define LERP(field, n) for(unsigned k=0;k<n;++k) v.field[k]=(int32_t)(a->field[k]+((__int128)b->field[k]-a->field[k])*da/(da-db))
    LERP(position,4); LERP(color,4); LERP(texcoord,2);
#undef LERP
    v.position[axis]=(int32_t)((int64_t)sign*v.position[3]);
    return v;
}
int G3Native_ClipScalar(G3NativeVertex *out, size_t capacity,
                      const G3NativeVertex *in, size_t count, int far_clip)
{
    if(!out || !in || count<3 || count>4 || capacity<10) return -1;
    G3NativeVertex a[12],b[12];
    memcpy(a,in,count*sizeof(*in));
    if(!far_clip) for(size_t i=0;i<count;++i) if(distance(&a[i],2,1)<0) return 0;
    /* DS clip order: Z, Y, X, positive then negative. */
    for(int axis=2;axis>=0;--axis) for(int sign=1;sign>=-1;sign-=2) {
        size_t used=0;
        for(size_t i=0;i<count;++i) {
            const G3NativeVertex *prev=&a[(i+count-1)%count], *cur=&a[i];
            int64_t dp=distance(prev,axis,sign),dc=distance(cur,axis,sign);
            if((dp<0)!=(dc<0)) {
                if(used>=12)return -1;
                b[used++]=dp>=0?intersection(prev,cur,dp,dc,axis,sign):intersection(cur,prev,dc,dp,axis,sign);
            }
            if(dc>=0) { if(used>=12)return -1; b[used++]=*cur; }
        }
        count=used; memcpy(a,b,count*sizeof(*a));
    }
    if(count>capacity)return -1;
    memcpy(out,a,count*sizeof(*out)); return (int)count;
}
const G3NativeMath G3Native_Scalar = {
    G3Native_MultiplyScalar, G3Native_TransformScalar,
    G3Native_LightScalar, G3Native_ClipScalar
};
