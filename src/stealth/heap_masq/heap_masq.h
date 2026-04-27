/*
 * src/stealth/heap_masq/heap_masq.h
 *
 * Heap masquerade. Allocates a private Wraith-owned heap distinct
 * from the process default heap so heap walks don't correlate the
 * loader's bookkeeping allocations with the consumer process's
 * `GetProcessHeap()` activity during a load.
 *
 * Baseline: a private process heap created on first use.
 * A future variant could root the heap segment inside a legit
 * MEM_IMAGE region so a heap-walker attributes the allocs to the
 * host module - that requires private knowledge of the ntdll heap
 * manager internals and is intentionally out of scope here.
 */

#ifndef WRAITH_HEAP_MASQ_H
#define WRAITH_HEAP_MASQ_H

#include "wraith/wraith_status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  *wr_heap_masq_alloc(size_t bytes);
void  wr_heap_masq_free(void *p);
void  wr_heap_masq_release(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HEAP_MASQ_H */
