#ifndef HEARTGOLD_NTR_HOST_H
#define HEARTGOLD_NTR_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Host services used by libntr's portable C implementation, not an SDL ABI. */
typedef struct HG_Thread HG_Thread;
typedef struct HG_Mutex HG_Mutex;
typedef struct HG_Semaphore HG_Semaphore;
typedef uint64_t HG_TimerID;
typedef uint64_t HG_ThreadID;
/* Renderer/audio objects are opaque: their backends are not implemented here. */
typedef struct HG_Window HG_Window;
typedef struct HG_AudioStream HG_AudioStream;

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
