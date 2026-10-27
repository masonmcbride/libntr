#ifndef NITRO_H_
#define NITRO_H_

// Data-only asset tools need fixed-width types, not host runtime services.
#if defined(SDK_DATA_ONLY)
#include <nitro/types.h>
#else

#if SDK_VERSION_MAJOR == 4
#ifdef SDK_X86
#define ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifdef SDK_AARCH64
#define ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifdef SDK_BUILD_NX
#include "switch.h"
#endif

#ifdef SDK_ASM
    #include <nitro/spec.h>
    #include <nitro/types.h>
    #include <nitro/memorymap.h>
    #include <nitro/hw/common/armArch.h>
#else
    #ifdef __MWERKS__
        #ifdef __cplusplus
            #ifndef __embedded_cplusplus
                #define __embedded_cplusplus    0
            #endif

            #ifndef __VEC__
                #define __VEC__    0
            #endif
        #endif

        #if !defined(_MSL_USE_INLINE) && __option(dont_inline)
            #define _MSL_USE_INLINE    0
        #endif
    #endif

    #include <nitro/spec.h>
    #include <nitro/misc.h>
    #include <nitro/init/crt0.h>

    #include <nitro/types.h>
    #include <nitro/memorymap.h>
    #include <nitro/hw/common/armArch.h>
    #include <nitro/hw/common/lcd.h>

    #include <nitro/os.h>
    #include <nitro/mi.h>
    #include <nitro/pxi.h>
    #include <nitro/snd.h>
    #include <nitro/pad.h>
    #include <nitro/spi.h>
    #include <nitro/card.h>
    #include <nitro/fs.h>
    #include <nitro/rtc.h>
    #include <nitro/gx.h>
    /* HeartGold's original WM implementation owns its API declarations. */
    #ifndef HG_USE_ORIGINAL_WM
    #include <nitro/wm.h>
    #endif
    #include <nitro/wvr.h>
    #include <nitro/ctrdg.h>
    #include <nitro/math.h>
    #include <nitro/utest.h>
    #include <nitro/std.h>

    #ifdef SDK_ARM9
        #include <nitro/fx/fx.h>
        #include <nitro/fx/fx_const.h>
        #include <nitro/fx/fx_trig.h>
        #include <nitro/fx/fx_cp.h>
        #include <nitro/fx/fx_vec.h>
        #include <nitro/fx/fx_mtx.h>
        #include <nitro/cp.h>
        #include <nitro/ext.h>
        #ifndef HG_USE_ORIGINAL_WM
        #include <nitro/mb.h>
        #endif
        #include <nitro/wbt.h>
        #include <nitro/wfs.h>
        #include <nitro/env.h>
        #include <nitro/vib.h>
    #endif
    #ifdef SDK_X86
        #include <nitro/fx/fx.h>
        #include <nitro/fx/fx_const.h>
        #include <nitro/fx/fx_trig.h>
        #include <nitro/fx/fx_cp.h>
        #include <nitro/fx/fx_vec.h>
        #include <nitro/fx/fx_mtx.h>
        #include <nitro/cp.h>
        #include <nitro/ext.h>
        #ifndef HG_USE_ORIGINAL_WM
        #include <nitro/mb.h>
        #endif
        #include <nitro/wbt.h>
        #include <nitro/wfs.h>
        #include <nitro/env.h>
        #include <nitro/vib.h>
    #endif
    #ifdef SDK_AARCH64
        #include <nitro/fx/fx.h>
        #include <nitro/fx/fx_const.h>
        #include <nitro/fx/fx_trig.h>
        #include <nitro/fx/fx_cp.h>
        #include <nitro/fx/fx_vec.h>
        #include <nitro/fx/fx_mtx.h>
        #include <nitro/cp.h>
        #include <nitro/ext.h>
        #ifndef HG_USE_ORIGINAL_WM
        #include <nitro/mb.h>
        #endif
        #include <nitro/wbt.h>
        #include <nitro/wfs.h>
        #include <nitro/env.h>
        #include <nitro/vib.h>
    #endif
#endif
#elif SDK_VERSION_MAJOR == 5
#include <twl_hybrid.h>
#endif

#endif // SDK_DATA_ONLY

#endif
