/* SPDX-License-Identifier: MIT */
#include <nitro.h>
#include <simulator/g3_handler.h>
#include <ntr_host.h>
#include <time.h>

/* SDK_PORT dispatch is synchronous: commands execute on the SDK thread and
 * this frame boundary replaces the desktop render/vcount semaphore handshake.
 * It renders the 3D layer offscreen. 2D compositing/presentation is not provided. */
extern "C" void SIM_PreRenderVBlank()
{
    s_reg_GX_VCOUNT=192;
    s_reg_GX_DISPSTAT|=1;
}
extern "C" void *SIM_Render(void *arg)
{
    G3Native_RenderFrame();
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
