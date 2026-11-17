/* SPDX-License-Identifier: MIT */
#include <nitro.h>
#include <simulator/g3_handler.h>
#include <ntr_host.h>
#include <time.h>
#include <atomic>
#include <cstdlib>
#include <simulator/sim.h>
extern "C" TPData s_tpData;
extern void G2Native_RenderFrame();

static std::atomic<HG_ThreadID> frame_thread{0};
static uint64_t next_vblank;
static bool dispatching_vblank;
static uint64_t monotonic_nanoseconds()
{
    timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) std::abort();
    return uint64_t(now.tv_sec) * 1000000000 + uint64_t(now.tv_nsec);
}
static void dispatch_vblank()
{
    dispatching_vblank = true;
    OSIntrMode previous_mask = OS_DisableInterrupts();
    next_vblank = monotonic_nanoseconds() + 1000000000 / 60;
    u16 previous_count = s_reg_GX_VCOUNT;
    u16 previous_status = s_reg_GX_DISPSTAT;
    s_reg_GX_VCOUNT = 192;
    s_reg_GX_DISPSTAT = (s_reg_GX_DISPSTAT & ~2u) | 1;
    if ((s_reg_GX_DISPSTAT & (1 << 3)) && (OS_GetIrqMask() & OS_IE_V_BLANK)
        && (reg_OS_IME & 1)) OS_IRQTable[0]();
    s_HW_INTR_CHECK_BUF |= 1;
    ++*reinterpret_cast<volatile u32 *>(HW_VBLANK_COUNT_BUF);
    s_reg_GX_VCOUNT = previous_count;
    s_reg_GX_DISPSTAT = (s_reg_GX_DISPSTAT & ~3u) | (previous_status & 3u);
    OS_RestoreInterrupts(previous_mask);
    dispatching_vblank = false;
}

/* Cooperative IRQ delivery at SDK critical-section exits during long loads.
 * Only the registered frame thread may run callbacks; no worker is introduced. */
extern "C" void SIM_PollVBlank()
{
    if (frame_thread.load(std::memory_order_acquire) != HG_GetThreadID(nullptr)) return;
    if (dispatching_vblank || OS_GetCpsrIrq() || !next_vblank) return;
    if (monotonic_nanoseconds() >= next_vblank) dispatch_vblank();
}

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
    HG_ThreadID unset = 0;
    frame_thread.compare_exchange_strong(unset, HG_GetThreadID(nullptr));
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
    dispatch_vblank();
    return arg;
}
extern "C" void SIM_PostRenderVBlank()
{
    s_reg_GX_VCOUNT=0;
    s_reg_GX_DISPSTAT&=~1u;
}
