#if (defined(SDK_ARM9) || defined(SDK_PORT))

#include <nitro.h>
#if defined(SDK_PORT) && defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
#include <ntr_host.h>
#endif

#include "../include/card_common.h"
#include "../include/card_spi.h"

void CARDi_OnFifoRecv (PXIFifoTag tag, uPtr data, BOOL err)
{
#pragma unused(data)
	if ((tag == PXI_FIFO_TAG_FS) && err) {
		CARDiCommon *const p = &cardi_common;

		SDK_ASSERT(data == CARD_REQ_ACK);
		p->flag &= ~CARD_STAT_REQ;
		OS_WakeupThreadDirect(p->cur_th);
	}
}

void CARDi_TaskThread (void *arg)
{
	CARDiCommon *const p = &cardi_common;
	(void)arg;

	for (;;) {

		OSIntrMode bak_psr = OS_DisableInterrupts();
		while ((p->flag & CARD_STAT_TASK) == 0) {
			OS_SleepThread(NULL);
		}
		(void)OS_RestoreInterrupts(bak_psr);
		(*p->task_func)(p);
	}
}

BOOL CARDi_Request (CARDiCommon *p, int req_type, int retry_count)
{
#if defined(SDK_PORT) && defined(SDK_AARCH64) && defined(SDK_BUILD_MACOS)
    /* The original worker, cache, cancellation and callback lifecycle remains
     * above this device boundary. Native addresses never enter a 32-bit FIFO. */
    CARDiCommandArg *c = p->cmd;
    int ok = 0;
    (void)retry_count;
    switch (req_type) {
    case CARD_REQ_IDENTIFY:
        ok = c->spec.total_size && c->spec.total_size == HG_BackupSize();
        break;
    case CARD_REQ_READ_BACKUP:
        if (c->src > 0xffffffffu) break;
        ok = HG_BackupRead((u32)c->src, (void *)c->dst, c->len);
        break;
    case CARD_REQ_WRITE_BACKUP:
    case CARD_REQ_PROGRAM_BACKUP:
        if (c->dst > 0xffffffffu) break;
        ok = HG_BackupWrite((u32)c->dst, (const void *)c->src, c->len,
                            req_type == CARD_REQ_PROGRAM_BACKUP);
        break;
    case CARD_REQ_VERIFY_BACKUP:
        if (c->dst > 0xffffffffu) break;
        ok = HG_BackupVerify((u32)c->dst, (const void *)c->src, c->len);
        break;
    case CARD_REQ_ERASE_SECTOR_BACKUP:
    case CARD_REQ_ERASE_SUBSECTOR_BACKUP:
        if (c->dst > 0xffffffffu) break;
        ok = HG_BackupErase((u32)c->dst, c->len);
        break;
    case CARD_REQ_ERASE_CHIP_BACKUP:
        ok = HG_BackupErase(0, HG_BackupSize());
        break;
    default:
        c->result = CARD_RESULT_UNSUPPORTED;
        return FALSE;
    }
    c->result = ok ? CARD_RESULT_SUCCESS : CARD_RESULT_ERROR;
    return ok;
#else

	if ((p->flag & CARD_STAT_INIT_CMD) == 0) {
		p->flag |= CARD_STAT_INIT_CMD;
        #ifndef SDK_PORT
		while (!PXI_IsCallbackReady(PXI_FIFO_TAG_FS, PXI_PROC_ARM7)) {
			OS_SpinWait(100);
		}
        #endif

		(void)CARDi_Request(p, CARD_REQ_INIT, 1);
	}

	DC_FlushRange(p->cmd, sizeof(*p->cmd));
	DC_WaitWriteBufferEmpty();

	do {
		p->command = req_type;
		p->flag |= CARD_STAT_REQ;
		CARDi_SendPxi((u32)req_type);

		switch (req_type) {
		case CARD_REQ_INIT:
			CARDi_SendPxi((u32)p->cmd);
			break;
		}
		{
			OSIntrMode bak_psr = OS_DisableInterrupts();
            #ifndef SDK_PORT
			while ((p->flag & CARD_STAT_REQ) != 0) {
				OS_SleepThread(NULL);
			}
            #endif
			(void)OS_RestoreInterrupts(bak_psr);
		}
		DC_InvalidateRange(p->cmd, sizeof(*p->cmd));

	} while ((p->cmd->result == CARD_RESULT_TIMEOUT) && (--retry_count > 0));

	return (p->cmd->result == CARD_RESULT_SUCCESS);
#endif
}

#endif
