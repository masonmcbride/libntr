/* SPDX-License-Identifier: MIT
 * All callers hold the sound mutex. Entries live exactly as long as the bank
 * or wave archive and are released by the original disposal operations. */
#include "snd_native_storage.h"
#include <stdlib.h>
typedef struct BankState {const SNDBankData *key;SNDWaveArcLink links[4];struct BankState *next;} BankState;
typedef struct ArcState {const SNDWaveArc *key;SNDWaveArcLink *top;const SNDWaveData **waves;u8 *assigned;struct ArcState *next;} ArcState;
static BankState *banks;static ArcState *arcs;
static void *allocate(size_t size){void *p=calloc(1,size);if(!p)abort();return p;}
SNDWaveArcLink *SNDi_NativeBankLinks(const SNDBankData *bank)
{
    for(BankState *s=banks;s;s=s->next)if(s->key==bank)return s->links;
    BankState *s=allocate(sizeof *s);s->key=bank;s->next=banks;banks=s;return s->links;
}
static ArcState *arcState(const SNDWaveArc *arc)
{
    for(ArcState *s=arcs;s;s=s->next)if(s->key==arc)return s;
    ArcState *s=allocate(sizeof *s);s->key=arc;s->next=arcs;arcs=s;return s;
}
SNDWaveArcLink **SNDi_NativeArcTop(const SNDWaveArc *arc){return &arcState(arc)->top;}
void SNDi_NativeDestroyBank(const SNDBankData *bank)
{
    for(BankState **p=&banks;*p;p=&(*p)->next)if((*p)->key==bank){BankState *s=*p;*p=s->next;free(s);return;}
}
void SNDi_NativeDestroyArc(const SNDWaveArc *arc)
{
    for(ArcState **p=&arcs;*p;p=&(*p)->next)if((*p)->key==arc){ArcState *s=*p;*p=s->next;free(s->waves);free(s->assigned);free(s);return;}
}
void SNDi_NativeSetWave(const SNDWaveArc *arc,int index,const SNDWaveData *wave)
{
    if(index<0 || (u32)index>=arc->waveCount)abort();
    ArcState *s=arcState(arc);
    if(!s->waves){s->waves=allocate((size_t)arc->waveCount*sizeof *s->waves);s->assigned=allocate(arc->waveCount);}
    s->waves[index]=wave;s->assigned[index]=1;
}
BOOL SNDi_NativeGetWave(const SNDWaveArc *arc,int index,const SNDWaveData **wave)
{
    if(index<0 || (u32)index>=arc->waveCount)abort();
    for(ArcState *s=arcs;s;s=s->next)if(s->key==arc && s->assigned && s->assigned[index]){*wave=s->waves[index];return TRUE;}
    return FALSE;
}
