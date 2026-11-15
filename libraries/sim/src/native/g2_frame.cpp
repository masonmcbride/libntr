/* SPDX-License-Identifier: MIT
 * Desktop-free presentation of sim_g2.cpp's original decoded layers. Layer
 * order and blend operations follow sim_main.cpp / shaders/g2_fragment.glsl.
 * Buffers retain the simulator's bottom-up, two-screen layout until handoff. */
#include <nitro.h>
#include <simulator/sim.h>
#include "sim_g2.h"
#include <ntr_host.h>
#include <algorithm>
#include <cstring>

#define LAYER(name) u8 name[4*256*384]
LAYER(bgtex); LAYER(bg0tex); LAYER(bg1tex); LAYER(bg2tex); LAYER(bg3tex);
LAYER(bg0stex); LAYER(bg1stex); LAYER(bg2stex); LAYER(bg3stex);
LAYER(oam0Tex); LAYER(oam1Tex); LAYER(oam2Tex); LAYER(oam3Tex);
LAYER(oam0sTex); LAYER(oam1sTex); LAYER(oam2sTex); LAYER(oam3sTex);
LAYER(objWindowTex); LAYER(objWindowsTex);
u8 s_SIM_DBG_BGenable[4]={1,1,1,1}, s_SIM_DBG_BGSenable[4]={1,1,1,1};
u8 s_SIM_DBG_OAMenable=1,s_SIM_DBG_OAMSenable=1;
u8 s_mosaicTable[16][256];
u32 OBJLine[2][256]; u8 OBJWindow[2][256];

void SIM_u16ToRGB(u16 in,u8 *r,u8 *g,u8 *b)
{
    *r=((in&31)*527+23)>>6;
    *g=(((in>>5)&31)*527+23)>>6;
    *b=(((in>>10)&31)*527+23)>>6;
}
namespace {
u8 *bg[2][4]={{bg0tex,bg1tex,bg2tex,bg3tex},{bg0stex,bg1stex,bg2stex,bg3stex}};
u8 *obj[2][4]={{oam0Tex,oam1Tex,oam2Tex,oam3Tex},{oam0sTex,oam1sTex,oam2sTex,oam3sTex}};
u8 output[256*384*4],g3[256*192*4];
void engine(bool sub)
{
    u32 display=sub?s_reg_GXS_DB_DISPCNT:s_reg_GX_DISPCNT;
    u16 control[4];
    if(sub) {control[0]=s_reg_G2S_DB_BG0CNT;control[1]=s_reg_G2S_DB_BG1CNT;control[2]=s_reg_G2S_DB_BG2CNT;control[3]=s_reg_G2S_DB_BG3CNT;}
    else {control[0]=s_reg_G2_BG0CNT;control[1]=s_reg_G2_BG1CNT;control[2]=s_reg_G2_BG2CNT;control[3]=s_reg_G2_BG3CNT;}
    int order[4]={0,1,2,3};
    std::sort(order,order+4,[&](int a,int b){return (control[a]&3)!=(control[b]&3)?(control[a]&3)>(control[b]&3):a>b;});
    u8 *window=sub?objWindowsTex:objWindowTex;
    std::memset(window,0,256*384*4);
    for(int n=0;n<4;++n) {
        std::memset(bg[sub][n],0,256*384*4);
        std::memset(obj[sub][n],0,256*384*4);
        if(display&(1<<(8+n)))G2SIM_DrawBG(n,display&7,(display>>3)&1,sub);
    }
    for(int y=0;y<192;++y)for(int priority=3;priority>=0;--priority) {
        std::memset(OBJLine,0,sizeof OBJLine);std::memset(OBJWindow,0,sizeof OBJWindow);
        G2SIM_DrawOBJ(y,sub,priority<<10);
        for(int x=0;x<256;++x) {
            unsigned index=OBJLine[sub][x]&255;
            if(index) {
                u16 color=reinterpret_cast<u16 *>(sub?s_HW_DB_OBJ_PLTT:s_HW_OBJ_PLTT)[index];
                u8 r,g,b;SIM_u16ToRGB(color,&r,&g,&b);
                G2SIM_PlotPixel(obj[sub][priority],r,g,b,x,y,sub);
            }
            if(OBJWindow[sub][x])G2SIM_PlotPixel(window,255,255,255,x,y,sub);
        }
    }
    bool top=(s_reg_GX_POWCNT>>15)?!sub:sub;
    u16 backdrop=*reinterpret_cast<u16 *>(sub?s_HW_DB_BG_PLTT:s_HW_BG_PLTT);
    u8 back[3];SIM_u16ToRGB(backdrop,back,back+1,back+2);
    u16 blend=sub?s_reg_G2S_DB_BLDCNT:s_reg_G2_BLDCNT;
    u16 alpha=sub?s_reg_G2S_DB_BLDALPHA:s_reg_G2_BLDALPHA;
    u16 bldy=sub?s_reg_G2S_DB_BLDY:s_reg_G2_BLDY;
    u16 bright=sub?s_reg_GXS_DB_MASTER_BRIGHT:s_reg_GX_MASTER_BRIGHT;
    u16 winout=sub?s_reg_G2S_DB_WINOUT:s_reg_G2_WINOUT;
    for(int y=0;y<192;++y)for(int x=0;x<256;++x) {
        size_t offset=((383-y-(top?0:192))*256+x)*4;
        u8 *dst=output+((y+(top?0:192))*256+x)*4;
        float c[3]={(float)back[0],(float)back[1],(float)back[2]};int current=5,priority=3;
        auto sprite=[&](int p) {const u8 *s=obj[sub][p]+offset;if(s[3]){for(int k=0;k<3;++k)c[k]=s[k];current=4;}};
        if(((display>>16)&3)==0 || (display&128)) {c[0]=c[1]=c[2]=255;}
        else {
            for(int n:order) {
                while(priority>(control[n]&3))sprite(priority--);
                if(display&(1<<15)) {int shift=window[offset+3]?8:0;if(!(winout&(1<<(n+shift))))continue;}
                const u8 *s=bg[sub][n]+offset;
                if(!sub && n==0 && (display&8) && (display&256))s=g3+(y*256+x)*4;
                if(!s[3])continue;
                int mode=(blend>>6)&3;
                for(int k=0;k<3;++k) {
                    float a=std::min(16,(int)(alpha&31))/16.f,b=std::min(16,(int)((alpha>>8)&31))/16.f;
                    if(!sub && n==0 && (display&8)){a=s[3]/255.f;b=1-a;}
                    float f=std::min(16,(int)(bldy&31))/16.f;
                    if(mode==1 && (blend&(1<<n)) && (blend&(1<<(8+current))))c[k]=std::min(255.f,s[k]*a+c[k]*b);
                    else if(mode==2 && (blend&(1<<n)))c[k]=s[k]+(255-s[k])*f;
                    else if(mode==3 && (blend&(1<<n)))c[k]=s[k]*(1-f);
                    else c[k]=s[k];
                }
                current=n;
            }
            while(priority>=0)sprite(priority--);
        }
        float f=std::min(16,(int)(bright&31))/16.f;
        for(int k=0;k<3;++k) {
            if((bright>>14)==1)c[k]+=(255-c[k])*f;
            else if((bright>>14)==2)c[k]*=1-f;
            dst[k]=(u8)std::clamp(c[k],0.f,255.f);
        }
        dst[3]=255;
    }
}
}
void G2Native_RenderFrame()
{
    if(HG_G3SubmittedFrames())HG_G3Readback(g3,256*4);
    engine(false);engine(true);HG_RuntimePresent(output);
}
