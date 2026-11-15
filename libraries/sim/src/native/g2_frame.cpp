/* SPDX-License-Identifier: MIT
 * Compose sim_g2.cpp's original decoded layers. Buffers retain the simulator's
 * bottom-up two-screen layout until handoff. Scanline registers and original
 * HBlank callbacks supply window boundaries; Nintendo effects are unchanged. */
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
u8 s_SIM_DBG_BGenable[4]={1,1,1,1},s_SIM_DBG_BGSenable[4]={1,1,1,1};
u8 s_SIM_DBG_OAMenable=1,s_SIM_DBG_OAMSenable=1;
u8 s_mosaicTable[16][256];
u32 OBJLine[2][256];u8 OBJWindow[2][256];

void SIM_u16ToRGB(u16 in,u8 *r,u8 *g,u8 *b)
{
    *r=((in&31)*527+23)>>6;*g=(((in>>5)&31)*527+23)>>6;
    *b=(((in>>10)&31)*527+23)>>6;
}
namespace {
u8 *bg[2][4]={{bg0tex,bg1tex,bg2tex,bg3tex},{bg0stex,bg1stex,bg2stex,bg3stex}};
u8 *obj[2][4]={{oam0Tex,oam1Tex,oam2Tex,oam3Tex},{oam0sTex,oam1sTex,oam2sTex,oam3sTex}};
u8 output[256*384*4],g3[256*192*4];
bool range(unsigned p,unsigned bounds)
{
    unsigned first=bounds>>8,last=bounds&255;
    return first<=last ? p>=first && p<last : p>=first || p<last;
}
void row(bool sub,int y)
{
    u32 display=sub?s_reg_GXS_DB_DISPCNT:s_reg_GX_DISPCNT;
    u16 control[4];
    if(sub){control[0]=s_reg_G2S_DB_BG0CNT;control[1]=s_reg_G2S_DB_BG1CNT;control[2]=s_reg_G2S_DB_BG2CNT;control[3]=s_reg_G2S_DB_BG3CNT;}
    else{control[0]=s_reg_G2_BG0CNT;control[1]=s_reg_G2_BG1CNT;control[2]=s_reg_G2_BG2CNT;control[3]=s_reg_G2_BG3CNT;}
    int order[4]={0,1,2,3};
    std::sort(order,order+4,[&](int a,int b){return (control[a]&3)!=(control[b]&3)?(control[a]&3)>(control[b]&3):a>b;});
    u8 *window=sub?objWindowsTex:objWindowTex;
    for(int n=0;n<4;++n)if(display&(1<<(8+n)))G2SIM_DrawBGLine(n,display&7,(display>>3)&1,sub,y);
    for(int priority=3;priority>=0;--priority){
        std::memset(OBJLine,0,sizeof OBJLine);std::memset(OBJWindow,0,sizeof OBJWindow);
        G2SIM_DrawOBJ(y,sub,priority<<10);
        for(int x=0;x<256;++x){
            unsigned index=OBJLine[sub][x]&255;
            if(index){u16 c=reinterpret_cast<u16 *>(sub?s_HW_DB_OBJ_PLTT:s_HW_OBJ_PLTT)[index];
                u8 r,g,b;SIM_u16ToRGB(c,&r,&g,&b);G2SIM_PlotPixel(obj[sub][priority],r,g,b,x,y,sub);}
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
    u16 winin=sub?s_reg_G2S_DB_WININ:s_reg_G2_WININ;
    u16 h0=sub?s_reg_G2S_DB_WIN0H:s_reg_G2_WIN0H,v0=sub?s_reg_G2S_DB_WIN0V:s_reg_G2_WIN0V;
    u16 h1=sub?s_reg_G2S_DB_WIN1H:s_reg_G2_WIN1H,v1=sub?s_reg_G2S_DB_WIN1V:s_reg_G2_WIN1V;
    for(int x=0;x<256;++x){
        size_t offset=((383-y-(top?0:192))*256+x)*4;
        u8 *dst=output+((y+(top?0:192))*256+x)*4;
        unsigned mask=63;
        if(display&0xe000){
            if((display&0x2000)&&range(x,h0)&&range(y,v0))mask=winin&63;
            else if((display&0x4000)&&range(x,h1)&&range(y,v1))mask=(winin>>8)&63;
            else mask=(winout>>(((display&0x8000)&&window[offset+3])?8:0))&63;
        }
        float c[3]={(float)back[0],(float)back[1],(float)back[2]};int current=5,priority=3;
        auto sprite=[&](int p){const u8 *s=obj[sub][p]+offset;if((mask&16)&&s[3]){for(int k=0;k<3;++k)c[k]=s[k];current=4;}};
        if(((display>>16)&3)==0||(display&128)){c[0]=c[1]=c[2]=255;}
        else{
            for(int n:order){
                while(priority>(control[n]&3))sprite(priority--);
                if(!(mask&(1<<n)))continue;
                const u8 *s=bg[sub][n]+offset;
                if(!sub&&n==0&&(display&8)&&(display&256))s=g3+(y*256+x)*4;
                if(!s[3])continue;
                int mode=(mask&32)?(blend>>6)&3:0;
                for(int k=0;k<3;++k){
                    float a=std::min(16,(int)(alpha&31))/16.f,b=std::min(16,(int)((alpha>>8)&31))/16.f;
                    if(!sub&&n==0&&(display&8)){a=s[3]/255.f;b=1-a;}
                    float f=std::min(16,(int)(bldy&31))/16.f;
                    if(mode==1&&(blend&(1<<n))&&(blend&(1<<(8+current))))c[k]=std::min(255.f,s[k]*a+c[k]*b);
                    else if(mode==2&&(blend&(1<<n)))c[k]=s[k]+(255-s[k])*f;
                    else if(mode==3&&(blend&(1<<n)))c[k]=s[k]*(1-f);
                    else c[k]=s[k];
                }
                current=n;
            }
            while(priority>=0)sprite(priority--);
        }
        float f=std::min(16,(int)(bright&31))/16.f;
        for(int k=0;k<3;++k){if((bright>>14)==1)c[k]+=(255-c[k])*f;else if((bright>>14)==2)c[k]*=1-f;dst[k]=(u8)std::clamp(c[k],0.f,255.f);}
        dst[3]=255;
    }
}
}
const u8 *G2Native_ComposeFrame()
{
    for(int i=0;i<16;++i)for(int x=0;x<256;++x)s_mosaicTable[i][x]=x/(i+1)*(i+1);
    for(int sub=0;sub<2;++sub)for(int n=0;n<4;++n){std::memset(bg[sub][n],0,256*384*4);std::memset(obj[sub][n],0,256*384*4);}
    std::memset(objWindowTex,0,sizeof objWindowTex);std::memset(objWindowsTex,0,sizeof objWindowsTex);
    if(HG_G3SubmittedFrames())HG_G3Readback(g3,256*4);
    for(int y=0;y<192;++y){
        s_reg_GX_VCOUNT=y;s_reg_GX_DISPSTAT&=~3u;
        row(false,y);row(true,y);
        s_reg_GX_DISPSTAT|=2;
        if((s_reg_GX_DISPSTAT&16)&&(OS_GetIrqMask()&OS_IE_H_BLANK))OS_IRQTable[1]();
    }
    s_reg_GX_VCOUNT=192;s_reg_GX_DISPSTAT=(s_reg_GX_DISPSTAT&~2u)|1;
    return output;
}
void G2Native_RenderFrame(){HG_RuntimePresent(G2Native_ComposeFrame());}
