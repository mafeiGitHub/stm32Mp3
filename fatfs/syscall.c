#include <stdlib.h>
#include "ff.h"

#if _FS_REENTRANT
	int ff_cre_syncobj (BYTE vol, _SYNC_t *sobj) {
		osSemaphoreDef (SEM);
		*sobj = osSemaphoreCreate (osSemaphore(SEM), 1);
		return *sobj != NULL;
		}

	int ff_del_syncobj (_SYNC_t sobj) {
		osSemaphoreDelete (sobj);
		return 1;
		}

	int ff_req_grant (_SYNC_t sobj) {
		int ret = 0;
		if (osSemaphoreWait(sobj, _FS_TIMEOUT) == osOK)
			ret = 1;
		return ret;
		}

	void ff_rel_grant (_SYNC_t sobj) {
		osSemaphoreRelease(sobj);
		}
#endif

void* ff_memalloc (UINT msize) {
	return pvPortMalloc (msize);
	}

void ff_memfree (void* mblock) {
	vPortFree (mblock);
	}
