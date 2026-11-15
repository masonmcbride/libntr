/* SPDX-License-Identifier: MIT */
#include <nitro.h>
#include <simulator/g3_handler.h>
#include <ntr_host.h>
#include <time.h>
#include <simulator/sim.h>
extern "C" TPData s_tpData;
extern void G2Native_RenderFrame();

/* SDK_PORT dispatch is synchronous: commands execute on the SDK thread and
 * this frame boundary replaces the desktop render/vcount semaphore handshake.
 * Native presentation and input are enabled by the desktop process entry. */
extern "C" void SIM_PreRenderVBlank()
{
    s_reg_GX_VCOUNT=192;
    s_reg_GX_DISPSTAT|=1;
}
extern "C" void *SIM_Render(void *arg)
{
    if(HG_RuntimeActive()) {
        uint16_t x=0,y=0; int touch=0;
        uint16_t keys=HG_RuntimePollInput(&x,&y,&touch);
        s_reg_PAD_KEYINPUT=(~keys)&0x3ff;
        *(volatile u16 *)HW_BUTTON_XY_BUF=(~keys)&0x2c00;
        s_tpData.x=x; s_tpData.y=y; s_tpData.touch=touch; s_tpData.validity=0;
    }
    G3Native_RenderFrame();
    if(HG_RuntimeActive()) G2Native_RenderFrame();
    /* Deliver original registered callbacks. No guest scheduler is replaced. */
    if(s_reg_GX_DISPSTAT & (1<<3)) OS_IRQTable[0]();
    s_HW_INTR_CHECK_BUF|=1;
    auto *counter=reinterpret_cast<volatile u32 *>(HW_VBLANK_COUNT_BUF);
    ++*counter;
    return arg;
}
extern "C" void SIM_PostRenderVBlank()
{
    s_reg_GX_VCOUNT=0;
    s_reg_GX_DISPSTAT&=~1u;
}
