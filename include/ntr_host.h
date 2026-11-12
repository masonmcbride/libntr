#ifndef HEARTGOLD_NTR_HOST_H
#define HEARTGOLD_NTR_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int HG_CartridgeInitialize(const char *path, void *header);
void HG_CartridgeTransfer(uint32_t offset, void *dst, uint32_t length);
void HG_CartridgeShutdown(void);
uint32_t HG_CartridgeIdentity(void);

/* Host services used by libntr's portable C implementation, not an SDL ABI. */
typedef struct HG_Thread HG_Thread;
typedef struct HG_Mutex HG_Mutex;
typedef struct HG_Semaphore HG_Semaphore;
typedef uint64_t HG_TimerID;
typedef uint64_t HG_ThreadID;
/* Renderer/audio objects are opaque: their backends are not implemented here. */
typedef struct HG_Window HG_Window;
typedef struct HG_AudioStream HG_AudioStream;

/* Native overlay lifecycle services; implemented by the host. */
typedef struct HG_OverlayImage HG_OverlayImage;
HG_OverlayImage *HG_GetOverlayImage(uint32_t id);
int HG_LoadOverlayImage(HG_OverlayImage *image);
int HG_StartOverlayImage(HG_OverlayImage *image);
int HG_OverlayCanUnload(const HG_OverlayImage *image);
int HG_OverlayOwnsAddress(const HG_OverlayImage *image, const void *address);
int HG_UnloadOverlayImage(HG_OverlayImage *image);
HG_Thread *HG_CreateThread(int (*entry)(void *), const char *name, void *arg);
HG_ThreadID HG_GetThreadID(HG_Thread *thread);
void HG_WaitThread(HG_Thread *thread, int *status);
HG_Mutex *HG_CreateMutex(void);
int HG_LockMutex(HG_Mutex *mutex);
int HG_UnlockMutex(HG_Mutex *mutex);
int HG_TryLockMutex(HG_Mutex *mutex);
void HG_DestroyMutex(HG_Mutex *mutex);
HG_Semaphore *HG_CreateSemaphore(uint32_t value);
int HG_SemPost(HG_Semaphore *semaphore);
int HG_SemWait(HG_Semaphore *semaphore);
int HG_SemWaitTimeout(HG_Semaphore *semaphore, uint32_t milliseconds);
void HG_DestroySemaphore(HG_Semaphore *semaphore);
uint64_t HG_GetTicks64(void);
void HG_Delay(uint32_t milliseconds);
HG_TimerID HG_AddTimer(uint32_t interval, uint32_t (*callback)(uint32_t, void *), void *arg);
int HG_RemoveTimer(HG_TimerID timer);
int HG_ShowMessage(const char *title, const char *message);

#ifdef __cplusplus
}
#endif

#endif

/* Native fixed-point geometry handoff. The host retains no caller pointers.
 * One submission is a whole frame; no per-polygon GPU submissions/readbacks. */
#ifndef NTR_HOST_G3_H
#define NTR_HOST_G3_H
#include <stddef.h>
#include <simulator/g3_native_math.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t first, count, polygon, texture, palette, viewport, control, wbuffer;
} HG_G3Draw;
typedef struct {
    uint32_t clear_color, clear_depth, banks[7], toon[32];
    uint32_t alpha_ref, fog_color, fog_offset, fog[32];
} HG_G3Registers;
int HG_G3Submit(const G3NativeVertex *vertices, size_t vertex_count,
                const HG_G3Draw *draws, size_t draw_count,
                const uint8_t *vram, size_t vram_size,
                const HG_G3Registers *registers);
/* Explicit diagnostic boundary only; waits for the last frame. Not called by
 * handlers or VBlank. Returns 0 on absence or GPU execution failure. */
int HG_G3Readback(uint8_t *rgba, size_t row_stride);
uint64_t HG_G3SubmittedFrames(void);
#ifdef __cplusplus
}
#endif
#endif
