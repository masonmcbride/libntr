/* SPDX-License-Identifier: MIT
 * Runtime pointers are owned separately from serialized SBNK/SWAR words. */
#ifndef SND_NATIVE_STORAGE_H
#define SND_NATIVE_STORAGE_H
#include <nitro/snd/common/bank.h>
SNDWaveArcLink *SNDi_NativeBankLinks(const SNDBankData *bank);
SNDWaveArcLink **SNDi_NativeArcTop(const SNDWaveArc *arc);
void SNDi_NativeDestroyBank(const SNDBankData *bank);
void SNDi_NativeDestroyArc(const SNDWaveArc *arc);
void SNDi_NativeSetWave(const SNDWaveArc *arc, int index, const SNDWaveData *wave);
BOOL SNDi_NativeGetWave(const SNDWaveArc *arc, int index, const SNDWaveData **wave);
#endif
