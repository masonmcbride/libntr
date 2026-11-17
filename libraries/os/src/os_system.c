#include <nitro/os.h>
#include <nitro/code32.h>

#ifdef SDK_PORT
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
extern void SIM_PollVBlank(void);
static _Thread_local OSIntrMode native_interrupt_mask;
static OSIntrMode Native_RestoreInterruptMask(OSIntrMode state, OSIntrMode bits)
{
    OSIntrMode previous = native_interrupt_mask & bits;
    native_interrupt_mask = (native_interrupt_mask & ~bits) | (state & bits);
    if (!(native_interrupt_mask & HW_PSR_IRQ_DISABLE)) SIM_PollVBlank();
    return previous;
}
#endif
OSIntrMode OS_EnableInterrupts( void )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    return Native_RestoreInterruptMask(0, HW_PSR_IRQ_DISABLE);
#else
    return 0;
#endif
}

OSIntrMode OS_DisableInterrupts( void )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    OSIntrMode previous = native_interrupt_mask & HW_PSR_IRQ_DISABLE;
    native_interrupt_mask |= HW_PSR_IRQ_DISABLE;
    return previous;
#else
    return 0;
#endif
}

OSProcMode OS_GetProcMode( void )
{
    return 0;
}

OSIntrMode OS_DisableInterrupts_IrqAndFiq( void )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    OSIntrMode bits = HW_PSR_IRQ_DISABLE | HW_PSR_FIQ_DISABLE;
    OSIntrMode previous = native_interrupt_mask & bits;
    native_interrupt_mask |= bits;
    return previous;
#else
    return 0;
#endif
}

OSIntrMode OS_RestoreInterrupts( OSIntrMode state )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    return Native_RestoreInterruptMask(state, HW_PSR_IRQ_DISABLE);
#else
    return 0;
#endif
}

OSIntrMode OS_EnableInterrupts_IrqAndFiq( void )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    return Native_RestoreInterruptMask(0, HW_PSR_IRQ_DISABLE | HW_PSR_FIQ_DISABLE);
#else
    return 0;
#endif
}

OSIntrMode OS_RestoreInterrupts_IrqAndFiq( OSIntrMode state )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    return Native_RestoreInterruptMask(state, HW_PSR_IRQ_DISABLE | HW_PSR_FIQ_DISABLE);
#else
    return 0;
#endif
}

OSIntrMode_Irq OS_GetCpsrIrq( void )
{
#if defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    return (OSIntrMode_Irq)(native_interrupt_mask & HW_PSR_IRQ_DISABLE);
#else
    return 0;
#endif
}
#else
asm OSIntrMode OS_EnableInterrupts (void)
{
    mrs r0, cpsr
    bic r1, r0, #HW_PSR_IRQ_DISABLE
    msr cpsr_c, r1
    and r0, r0, #HW_PSR_IRQ_DISABLE
    bx lr
}

asm OSIntrMode OS_DisableInterrupts (void)
{
    mrs r0, cpsr
    orr r1, r0, #HW_PSR_IRQ_DISABLE
    msr cpsr_c, r1
    and r0, r0, #HW_PSR_IRQ_DISABLE
    bx lr
}

asm OSIntrMode OS_RestoreInterrupts (register OSIntrMode state)
{
    mrs r1, cpsr
    bic r2, r1, #HW_PSR_IRQ_DISABLE
    orr r2, r2, r0
    msr cpsr_c, r2
    and r0, r1, #HW_PSR_IRQ_DISABLE
    bx lr
}

asm OSIntrMode OS_EnableInterrupts_IrqAndFiq (void)
{
    mrs r0, cpsr
    bic r1, r0, #HW_PSR_IRQ_FIQ_DISABLE
    msr cpsr_c, r1
    and r0, r0, #HW_PSR_IRQ_FIQ_DISABLE
    bx lr
}

asm OSIntrMode OS_DisableInterrupts_IrqAndFiq (void)
{
    mrs r0, cpsr
    orr r1, r0, #HW_PSR_IRQ_FIQ_DISABLE
    msr cpsr_c, r1
    and r0, r0, #HW_PSR_IRQ_FIQ_DISABLE
    bx lr
}

asm OSIntrMode OS_RestoreInterrupts_IrqAndFiq (register OSIntrMode state)
{
    mrs r1, cpsr
    bic r2, r1, #HW_PSR_IRQ_FIQ_DISABLE
    orr r2, r2, r0
    msr cpsr_c, r2
    and r0, r1, #HW_PSR_IRQ_FIQ_DISABLE
    bx lr
}

asm OSIntrMode_Irq OS_GetCpsrIrq (void)
{
    mrs r0, cpsr
    and r0, r0, #HW_PSR_IRQ_DISABLE
    bx lr
}

asm OSProcMode OS_GetProcMode (void)
{
    mrs r0, cpsr
    and r0, r0, #HW_PSR_CPU_MODE_MASK
    bx lr
}

#include <nitro/codereset.h>
#endif

#ifdef SDK_ARM9
    #include <nitro/code32.h>
    asm void OS_SpinWait (u32 cycle)
    {
    _1: subs r0, r0, #4
        bcs _1
        bx lr
    }
    #include <nitro/codereset.h>
#else
    void OS_SpinWait (u32 cycle)
    {
        SVC_WaitByLoop((s32)cycle / 4);
    }
#endif

#if SDK_VERSION_MAJOR == 5
#ifdef SDK_ARM9
#include <nitro/code32.h>
asm void OS_SpinWaitCpuCycles(u32 cycle) {
_1:
  subs r0, r0,
      #4     // 3 cycle
      bcs _1 // 1 cycle
          bx lr
}
#include <nitro/codereset.h>
#else
void OS_SpinWaitCpuCycles(u32 cycle) {
#ifdef SDK_BUILD_ARM
  SVC_WaitByLoop((s32)cycle / 4);
#endif
}
#endif

#if defined(SDK_ARM9) || defined(SDK_PORT)
void OS_SpinWaitSysCycles(u32 cycle) {
#ifdef SDK_TWL
  cycle <<= (SCFG_GetCpuSpeed() == SCFG_CPU_SPEED_2X) ? 2 : 1;
#else
  cycle <<= 1;
#endif

  if (cycle > 16) {
    OS_SpinWaitCpuCycles(cycle - 16);
  }
}
#else
void OS_SpinWaitSysCycles(u32 cycle) { SVC_WaitByLoop((s32)cycle / 4); }
#endif
#endif

void OS_WaitInterrupt (BOOL clear, OSIrqMask irqFlags)
{
    OSIntrMode cpsrIrq = OS_DisableInterrupts();
    BOOL ime = OS_EnableIrq();

    if (clear) {
        (void)OS_ClearIrqCheckFlag(irqFlags);
    }

    while (!(OS_GetIrqCheckFlag() & irqFlags)) {
        #ifdef SDK_BUILD_ARM
        OS_Halt();
        #endif
        (void)OS_EnableInterrupts();
        (void)OS_DisableInterrupts();
    }

    (void)OS_ClearIrqCheckFlag(irqFlags);
    (void)OS_RestoreIrq(ime);
    (void)OS_RestoreInterrupts(cpsrIrq);
}

void OS_WaitVBlankIntr (void)
{
    SVC_WaitByLoop(1);
#if defined(SDK_ENABLE_ARM7_PRINT) && defined(SDK_ARM9)

    OS_PrintServer();
#endif
    OS_WaitIrq(TRUE, OS_IE_V_BLANK);
}
