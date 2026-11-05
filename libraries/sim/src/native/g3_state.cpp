/* SPDX-License-Identifier: MIT
 * Native geometry state. The desktop g3_handler.cpp remains a separate target.
 * Arithmetic is deliberately confined to g3_math.c's replaceable batch API. */
#include <simulator/g3_handler.h>
#include <simulator/g3_native_math.h>
#include <ntr_host.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

GXMtxMode s_curMtxMode=GX_MTXMODE_PROJECTION;
GXBegin s_primType=GX_BEGIN_TRIANGLES;
G3SIM_FxVtx_t s_g3PolygonVerts[4];
u8 s_G3numInPoly, s_G3drawPoly;
u32 s_G3triStripTriNum, s_G3quadStripTriNum;
G3SIM_TexImageParam_t s_texImageParam;
u32 s_texPlttBase;
GLfloat s_g3NextTexCoordS, s_g3NextTexCoordT;
u8 s_g3CurColor[3];
G3SIM_PolygonAttr_t s_curPolygonAttr;

namespace {
G3NativeMatrix matrices[4], stacks[4][32], clip;
unsigned position_sp, projection_sp;
uint32_t pending_attr, polygon_attr, texture, palette, viewport=0xBFFF0000;
uint32_t swap_flags, active_flags;
int32_t tex_base[2], tex_current[2];
G3NativeLighting lighting;
bool swap_pending, begun, force_span;
std::vector<G3NativeVertex> vertices;
std::vector<HG_G3Draw> draws;
std::vector<G3NativeVertex> pending_vertices;
std::vector<HG_G3Draw> pending_draws;
const G3NativeMath *math=&G3Native_Scalar;
constexpr size_t max_vertices=6144, max_polygons=2048;
size_t polygon_count,ram_vertices,pending_ram_vertices,pending_polygons;
bool previous_shared;

[[noreturn]] void fail(const char *message)
{
    std::fprintf(stderr,"libntr native G3: %s\n",message); std::abort();
}
G3NativeMatrix &current() { return matrices[(unsigned)s_curMtxMode]; }
bool position_mode() { return s_curMtxMode==GX_MTXMODE_POSITION || s_curMtxMode==GX_MTXMODE_POSITION_VECTOR; }
void sync()
{
    math->multiply(&clip,&matrices[1],&matrices[0],1);
    /* These register globals are distinct C objects, not an array. */
#define CLIP(n,r,c) s_reg_G3X_CLIPMTX_RESULT_##n=(u32)clip.m[r][c]
    CLIP(0,0,0); CLIP(1,0,1); CLIP(2,0,2); CLIP(3,0,3);
    CLIP(4,1,0); CLIP(5,1,1); CLIP(6,1,2); CLIP(7,1,3);
    CLIP(8,2,0); CLIP(9,2,1); CLIP(10,2,2); CLIP(11,2,3);
    CLIP(12,3,0); CLIP(13,3,1); CLIP(14,3,2); CLIP(15,3,3);
#undef CLIP
#define VEC(n,r,c) s_reg_G3X_VECMTX_RESULT_##n=(u32)matrices[2].m[r][c]
    VEC(0,0,0); VEC(1,0,1); VEC(2,0,2); VEC(3,1,0); VEC(4,1,1);
    VEC(5,1,2); VEC(6,2,0); VEC(7,2,1); VEC(8,2,2);
#undef VEC
    s_reg_G3X_GXSTAT=(s_reg_G3X_GXSTAT & ~0x3F00u)|((position_sp&31)<<8)|((projection_sp&1)<<13);
}
void load(const G3NativeMatrix &m)
{
    current()=m;
    if(s_curMtxMode==GX_MTXMODE_POSITION_VECTOR) matrices[1]=m;
    sync();
}
void mult(const G3NativeMatrix &m, bool scale=false)
{
    if(s_curMtxMode==GX_MTXMODE_POSITION_VECTOR) {
        math->multiply(&matrices[1],&m,&matrices[1],1);
        if(!scale)math->multiply(&matrices[2],&m,&matrices[2],1);
    } else math->multiply(&current(),&m,&current(),1);
    sync();
}
G3NativeVector unpack(u32 data)
{
    G3NativeVector v={};
    for(unsigned i=0;i<3;++i) {
        int x=(data>>(10*i))&1023;
        v.v[i]=(x>=512?x-1024:x)*8;
    }
    return v;
}
void color5(u32 data,uint8_t *out)
{ for(unsigned c=0;c<3;++c)out[c]=(data>>(5*c))&31; }
void tex_transform(const G3NativeVector &input, unsigned shift)
{
    G3NativeVector transformed;
    math->transform(&transformed,&input,1,&matrices[3]);
    for(unsigned c=0;c<2;++c) tex_current[c]=tex_base[c]+(transformed.v[c]>>shift);
}
void error_stack() { s_reg_G3X_GXSTAT|=1u<<15; }
void store(unsigned index)
{
    if(position_mode()) {
        if(index>=31)error_stack();
        stacks[1][index&31]=matrices[1]; stacks[2][index&31]=matrices[2];
    } else stacks[(unsigned)s_curMtxMode][0]=current();
}
void restore(unsigned index)
{
    if(position_mode()) {
        if(index>=31)error_stack();
        matrices[1]=stacks[1][index&31]; matrices[2]=stacks[2][index&31];
    } else current()=stacks[(unsigned)s_curMtxMode][0];
    sync();
}
}
extern "C" void G3Native_Reset()
{
    std::memset(matrices,0,sizeof(matrices)); std::memset(stacks,0,sizeof(stacks));
    std::memset(&lighting,0,sizeof(lighting));
    position_sp=projection_sp=0; pending_attr=polygon_attr=texture=palette=0;
    viewport=0xBFFF0000; swap_pending=begun=false; active_flags=swap_flags=0;
    tex_base[0]=tex_base[1]=tex_current[0]=tex_current[1]=0;
    s_curMtxMode=GX_MTXMODE_PROJECTION;
    s_G3numInPoly=s_G3drawPoly=0; s_G3triStripTriNum=s_G3quadStripTriNum=0;
    std::memset(s_g3CurColor,0,sizeof(s_g3CurColor));
    std::memset(&s_curPolygonAttr,0,sizeof(s_curPolygonAttr));
    std::memset(&s_texImageParam,0,sizeof(s_texImageParam));
    vertices.clear(); draws.clear(); vertices.reserve(max_vertices*8); draws.reserve(max_polygons);
    pending_vertices.reserve(max_vertices*8); pending_draws.reserve(max_polygons);
    polygon_count=ram_vertices=pending_ram_vertices=pending_polygons=0; previous_shared=false; force_span=false; pending_vertices.clear(); pending_draws.clear(); sync();
}
void G3SIM_MtxMode(GXMtxMode mode) { s_curMtxMode=(GXMtxMode)((unsigned)mode&3); }
void G3SIM_Identity() { G3NativeMatrix m; G3Native_Identity(&m); load(m); }
void G3SIM_MtxLoad44(MtxFx44 *m) { G3NativeMatrix n; std::memcpy(&n,m,sizeof(n)); load(n); }
void G3SIM_MtxLoad43(MtxFx43 *m)
{
    G3NativeMatrix n; G3Native_Identity(&n);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<3;++c)n.m[r][c]=m->m[r][c];
    load(n);
}
void G3SIM_MtxMult44(MtxFx44 *m) { G3NativeMatrix n; std::memcpy(&n,m,sizeof(n)); mult(n); }
void G3SIM_MtxMult43(MtxFx43 *m)
{
    G3NativeMatrix n; G3Native_Identity(&n);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<3;++c)n.m[r][c]=m->m[r][c];
    mult(n);
}
void G3SIM_MtxMult33(MtxFx33 *m)
{
    G3NativeMatrix n; G3Native_Identity(&n);
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)n.m[r][c]=m->m[r][c];
    mult(n);
}
void G3SIM_MtxScale(fx32 *s)
{ G3NativeMatrix n; G3Native_Identity(&n); for(unsigned i=0;i<3;++i)n.m[i][i]=s[i]; mult(n,true); }
void G3SIM_MtxTranslate(fx32 *s)
{ G3NativeMatrix n; G3Native_Identity(&n); for(unsigned i=0;i<3;++i)n.m[3][i]=s[i]; mult(n); }
void G3SIM_MtxStore(u8 i) { store(i&31); }
void G3SIM_MtxRestore(u8 i) { restore(i&31); }
void G3SIM_MtxPush()
{
    if(position_mode()) { store(position_sp); position_sp=(position_sp+1)&63; if(position_sp>=31)error_stack(); }
    else if(s_curMtxMode==GX_MTXMODE_PROJECTION) { if(projection_sp)error_stack(); store(0); projection_sp^=1; }
    else store(0);
    sync();
}
void G3SIM_MtxPop(u8 n)
{
    if(position_mode()) {
        int offset=n&63; if(offset&32)offset-=64;
        position_sp=(position_sp-offset)&63; restore(position_sp);
    } else if(s_curMtxMode==GX_MTXMODE_PROJECTION) { if(!projection_sp)error_stack(); projection_sp^=1; restore(0); }
    else restore(0);
    sync();
}
void G3SIM_Color(u16 c)
{ for(unsigned i=0;i<3;++i)s_g3CurColor[i]=(c>>(i*5))&31; }
void G3SIM_DiffAmb(u32 d)
{ color5(d,lighting.diffuse); color5(d>>16,lighting.ambient); if(d&0x8000)G3SIM_Color(d); }
void G3SIM_SpecEmi(u32 d)
{ color5(d,lighting.specular); color5(d>>16,lighting.emission); lighting.use_shininess=(d>>15)&1; }
void G3SIM_LightColor(u32 d) { color5(d,lighting.color[d>>30]); }
void G3SIM_LightVector(u32 d)
{
    unsigned l=d>>30; G3NativeVector v=unpack(d),o;
    math->transform(&o,&v,1,&matrices[2]);
    for(unsigned c=0;c<3;++c) {
        int value=(o.v[c]>>3)&1023; if(value&512)value-=1024;
        lighting.direction[l][c]=value;
        lighting.half[l][c]=(value-(c==2?512:0))>>1;
    }
}
void G3SIM_Normal(u32 d)
{
    G3NativeVector v=unpack(d),o;
    if(s_texImageParam.texCordTransform==2)tex_transform(v,8);
    math->transform(&o,&v,1,&matrices[2]);
    for(unsigned c=0;c<3;++c)o.v[c]=(int16_t)o.v[c]>>3;
    lighting.enabled=polygon_attr&15;
    math->light(&s_g3CurColor,&o,1,&lighting);
}
void G3Native_Shininess(const u32 *data) { std::memcpy(lighting.shininess,data,128); }
void G3SIM_PolygonAttr(u32 d) { pending_attr=d; }
void G3Native_Viewport(u32 d) { viewport=d; }
void G3SIM_Begin(GXBegin type)
{
    previous_shared=false; begun=true; s_primType=(GXBegin)((unsigned)type&3); polygon_attr=pending_attr;
    s_G3numInPoly=s_G3drawPoly=0; s_G3triStripTriNum=s_G3quadStripTriNum=0;
    s_curPolygonAttr.lightFlag=polygon_attr&15;
    s_curPolygonAttr.cullMode=(polygon_attr>>6)&3;
    s_curPolygonAttr.polygonMode=(polygon_attr>>4)&3;
    s_curPolygonAttr.alphaInt=(polygon_attr>>16)&31;
    s_curPolygonAttr.alpha=s_curPolygonAttr.alphaInt/31.0f;
}
void G3Native_End() { begun=false; }
void G3SIM_TexImageParam(u32 d)
{
    texture=d; s_texImageParam.textureOffset=(d&65535)*8;
    s_texImageParam.repeatS=(d>>16)&1; s_texImageParam.repeatT=(d>>17)&1;
    s_texImageParam.flipS=(d>>18)&1; s_texImageParam.flipT=(d>>19)&1;
    s_texImageParam.textureSSize=8<<((d>>20)&7); s_texImageParam.textureTSize=8<<((d>>23)&7);
    s_texImageParam.textureFormat=(d>>26)&7; s_texImageParam.color0=(d>>29)&1;
    s_texImageParam.texCordTransform=d>>30;
}
void G3SIM_TexPlttBase(u32 d) { palette=d&8191; s_texPlttBase=palette<<(s_texImageParam.textureFormat==2?3:4); }
void G3SIM_TexCoord(u32 d)
{
    tex_base[0]=(int16_t)d; tex_base[1]=(int16_t)(d>>16);
    if(s_texImageParam.texCordTransform==1) {
        G3NativeVector v={{tex_base[0]*256,tex_base[1]*256,256,256}},o;
        math->transform(&o,&v,1,&matrices[3]);
        for(unsigned c=0;c<2;++c)tex_current[c]=o.v[c]>>8;
    } else std::memcpy(tex_current,tex_base,sizeof(tex_base));
    s_g3NextTexCoordS=tex_current[0]/16.0f; s_g3NextTexCoordT=tex_current[1]/16.0f;
}
void G3SIM_Vtx(s16 x,s16 y,s16 z)
{
    if(!begun)fail("vertex outside BEGIN/END");
    G3NativeVector v={{x,y,z,4096}},o;
    if(s_texImageParam.texCordTransform==3)tex_transform(v,8);
    math->transform(&o,&v,1,&clip);
    G3SIM_FxVtx_t &dest=s_g3PolygonVerts[s_G3numInPoly];
    std::memcpy(dest.nums,o.v,sizeof(o));
    std::memcpy(dest.native_texcoord,tex_current,sizeof(tex_current));
    for(unsigned c=0;c<3;++c)dest.native_color[c]=s_g3CurColor[c];
    dest.r=s_g3CurColor[0]/31.0f; dest.g=s_g3CurColor[1]/31.0f; dest.b=s_g3CurColor[2]/31.0f;
    dest.a=((polygon_attr>>16)&31)/31.0f;
    ++s_G3numInPoly;
    unsigned needed=(s_primType==GX_BEGIN_QUADS||s_primType==GX_BEGIN_QUAD_STRIP)?4:3;
    if(s_G3numInPoly==needed) {
        if(s_primType==GX_BEGIN_TRIANGLE_STRIP||s_primType==GX_BEGIN_QUAD_STRIP) {
            s_G3numInPoly=2; s_G3drawPoly=1;
            if(needed==3)++s_G3triStripTriNum; else ++s_G3quadStripTriNum;
        } else s_G3numInPoly=0;
    }
}
void G3SIM_SubmitPolygon(G3SIM_FxVtx_t *v,int count)
{
    if(count<3||count>4)fail("invalid polygon size");
    G3NativeVertex input[4]={},clipped[12];
    for(int i=0;i<count;++i) {
        std::memcpy(input[i].position,v[i].nums,16);
        std::memcpy(input[i].texcoord,v[i].native_texcoord,8);
        std::memcpy(input[i].color,v[i].native_color,12);
        input[i].color[3]=(polygon_attr>>16)&31;
    }
    int n=math->clip(clipped,12,input,count,(polygon_attr>>12)&1);
    if(n<0)fail("clip capacity exceeded"); if(n<3){previous_shared=false;return;}
    /* Homogeneous determinant avoids a floating-point perspective divide. */
    auto &a=clipped[0]; auto &b=clipped[1]; auto &c=clipped[2];
    __int128 area=(__int128)a.position[0]*(__int128(b.position[1])*c.position[3]-__int128(c.position[1])*b.position[3])
        -(__int128)a.position[1]*(__int128(b.position[0])*c.position[3]-__int128(c.position[0])*b.position[3])
        +(__int128)a.position[3]*(__int128(b.position[0])*c.position[1]-__int128(c.position[0])*b.position[1]);
    if(!(polygon_attr&(area>0?0x80u:0x40u))){previous_shared=false;return;}
    bool unchanged=n==count && std::memcmp(input,clipped,count*sizeof(*input))==0;
    bool strip=s_primType==GX_BEGIN_TRIANGLE_STRIP||s_primType==GX_BEGIN_QUAD_STRIP;
    size_t used=(strip&&unchanged&&previous_shared)?count-2:n;
    if(polygon_count>=max_polygons||ram_vertices+used>max_vertices||vertices.size()+size_t(n-2)*3>max_vertices*8) {
        s_reg_G3X_DISP3DCNT|=1<<13; previous_shared=false; return;
    }
    bool wire=((polygon_attr>>16)&31)==0;
    HG_G3Draw draw={(u32)vertices.size(),(u32)(wire?n+1:(n-2)*3),polygon_attr,texture,palette,viewport,(u32)s_reg_G3X_DISP3DCNT|(force_span?0x80000000u:0),active_flags>>1};
    if(wire) { for(int i=0;i<n;++i)vertices.push_back(clipped[i]); vertices.push_back(clipped[0]); }
    else for(int i=1;i<n-1;++i) { vertices.push_back(clipped[0]); vertices.push_back(clipped[i]); vertices.push_back(clipped[i+1]); }
    ++polygon_count; ram_vertices+=used; previous_shared=strip&&unchanged;
    draws.push_back(draw); force_span=false;
}
int G3SIM_ClipPolygon(fx32 v[][4],int n,int start,BOOL attribs)
{
    if(start||attribs)fail("unsupported legacy clipping arguments");
    G3NativeVertex in[4]={},out[12];
    if(n<3||n>4)return 0;
    for(int i=0;i<n;++i)std::memcpy(in[i].position,v[i],16);
    int used=math->clip(out,12,in,n,1);
    for(int i=0;i<used;++i)std::memcpy(v[i],out[i].position,16);
    return used;
}
void G3Native_PositionTest(s16 x,s16 y,s16 z)
{
    G3NativeVector v={{x,y,z,4096}},o; math->transform(&o,&v,1,&clip);
    s_reg_G3X_POS_RESULT_X=o.v[0]; s_reg_G3X_POS_RESULT_Y=o.v[1];
    s_reg_G3X_POS_RESULT_Z=o.v[2]; s_reg_G3X_POS_RESULT_W=o.v[3];
}
void G3Native_VectorTest(u32 d)
{
    auto v=unpack(d); G3NativeVector o; math->transform(&o,&v,1,&matrices[2]);
    s_reg_G3X_VEC_RESULT_X=o.v[0]; s_reg_G3X_VEC_RESULT_Y=o.v[1]; s_reg_G3X_VEC_RESULT_Z=o.v[2];
}
void G3SIM_BoxTest(s16 x,s16 y,s16 z,s16 w,s16 h,s16 d)
{
    G3NativeVector in[8],out[8];
    for(unsigned i=0;i<8;++i)in[i]={{(s16)(x+((i&1)?w:0)),(s16)(y+((i&2)?h:0)),(s16)(z+((i&4)?d:0)),4096}};
    math->transform(out,in,8,&clip);
    static const unsigned faces[6][4]={{0,1,3,2},{4,5,7,6},{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6}};
    bool visible=false;
    for(auto &face:faces) {
        G3NativeVertex v[4]={},o[12];
        for(unsigned i=0;i<4;++i)std::memcpy(v[i].position,out[face[i]].v,16);
        if(math->clip(o,12,v,4,1)>0)visible=true;
    }
    s_reg_G3X_GXSTAT=(s_reg_G3X_GXSTAT&~2u)|(visible?2:0);
}
void G3Native_Swap(u32 data)
{
    if(swap_pending)fail("second SWAP before VBlank");
    pending_vertices.swap(vertices); pending_draws.swap(draws);
    pending_ram_vertices=ram_vertices; pending_polygons=polygon_count;
    vertices.clear(); draws.clear(); polygon_count=ram_vertices=0; previous_shared=false; force_span=false;
    swap_flags=active_flags; active_flags=data&3; swap_pending=true;
}
void G3SIM_SwapBuffers(u8 mode) { G3Native_Swap((mode&1)<<1); }
u8 G3SIM_SwapBuffersCalledThisFrame() { return swap_pending; }
void G3SIM_ResetSwapBuffersCall() { swap_pending=false; }
void G3Native_RenderFrame()
{
    if(!swap_pending)return;
    HG_G3Registers regs={}; regs.clear_color=s_reg_G3X_CLEAR_COLOR; regs.clear_depth=s_reg_G3X_CLEAR_DEPTH;
    regs.banks[0]=s_reg_GX_VRAMCNT_A; regs.banks[1]=s_reg_GX_VRAMCNT_B;
    regs.banks[2]=s_reg_GX_VRAMCNT_C; regs.banks[3]=s_reg_GX_VRAMCNT_D;
    regs.banks[4]=s_reg_GX_VRAMCNT_E; regs.banks[5]=s_reg_GX_VRAMCNT_F; regs.banks[6]=s_reg_GX_VRAMCNT_G;
    /* MMIO bulk-copy tables have contiguous native backing. */
#define TOON(n) regs.toon[n*2]=s_reg_G3X_TOON_TABLE_##n&65535; regs.toon[n*2+1]=s_reg_G3X_TOON_TABLE_##n>>16
    TOON(0); TOON(1); TOON(2); TOON(3); TOON(4); TOON(5); TOON(6); TOON(7);
    TOON(8); TOON(9); TOON(10); TOON(11); TOON(12); TOON(13); TOON(14); TOON(15);
#undef TOON
    regs.alpha_ref=s_reg_G3X_ALPHA_TEST_REF;
    regs.fog_color=s_reg_G3X_FOG_COLOR; regs.fog_offset=s_reg_G3X_FOG_OFFSET;
    for(unsigned i=0;i<32;++i)regs.fog[i]=((const volatile u8 *)s_g3NativeTables)[i];
    struct Ordered { HG_G3Draw draw; int min_y,max_y; bool translucent; };
    static std::vector<Ordered> ordered;
    ordered.clear(); ordered.reserve(max_polygons);
    for(const auto &d:pending_draws) {
        Ordered item={d,INT32_MAX,INT32_MIN,false};
        unsigned alpha=(d.polygon>>16)&31,format=(d.texture>>26)&7;
        item.translucent=(alpha>0&&alpha<31)||format==1||format==6;
        for(size_t i=d.first;i<d.first+d.count;++i) {
            const auto &v=pending_vertices[i]; if(!v.position[3])continue;
            int y=(int)((int64_t(v.position[1])+v.position[3])*192/(int64_t(v.position[3])*2));
            item.min_y=std::min(item.min_y,192-y); item.max_y=std::max(item.max_y,192-y);
        }
        ordered.push_back(item);
    }
    std::stable_sort(ordered.begin(),ordered.end(),[](const Ordered &a,const Ordered &b) {
        if(a.translucent!=b.translucent)return !a.translucent;
        if((swap_flags&1)&&a.translucent)return false;
        if(a.max_y!=b.max_y)return a.max_y<b.max_y;
        return a.min_y<b.min_y;
    });
    pending_draws.clear();
    for(auto &item:ordered) {
        HG_G3Draw d=item.draw; bool boundary=d.control&0x80000000u; d.control&=~0x80000000u;
        if(!boundary&&!pending_draws.empty()&&((d.polygon>>16)&31)) {
            HG_G3Draw &prev=pending_draws.back();
            if(prev.first+prev.count==d.first && !std::memcmp(&prev.polygon,&d.polygon,6*sizeof(u32))) {prev.count+=d.count;continue;}
        }
        pending_draws.push_back(d);
    }
    s_reg_G3X_LISTRAM_COUNT=(u16)pending_polygons;
    s_reg_G3X_VTXRAM_COUNT=(u16)pending_ram_vertices;
    if(!HG_G3Submit(pending_vertices.data(),pending_vertices.size(),pending_draws.data(),pending_draws.size(),s_HW_LCDC_VRAM,sizeof(s_HW_LCDC_VRAM),&regs))fail("Metal frame submission failed");
    pending_vertices.clear(); pending_draws.clear(); swap_pending=false;
}

extern "C" void G3SIM_FlushArray() { force_span=true; }
