/*
 * src/stealth/heap_masq/heap_masq.c
 *
 * Lazy-initialize a private heap on first use via `HeapCreate`
 * (which internally calls `RtlCreateHeap`). The private heap appears
 * as a distinct entry in heap walks - heap-snapshot tools (Process
 * Hacker -> Heaps tab) show it separately from the main process
 * heap, attributing Wraith's bookkeeping allocations to a fresh
 * heap rather than to the mainline process heap that any analyst
 * would scrutinize first.
 */

#include "stealth/heap_masq/heap_masq.h"

#include <windows.h>

static HANDLE g_heap = NULL;

static void ensure_heap(void)
{
  if (!g_heap) {
  /* HEAP_NO_SERIALIZE: Wraith only allocates from one thread at
  * load time. HEAP_GROWABLE: successive allocations don't rely on
  * the initial commit estimate. */
  g_heap = HeapCreate(HEAP_NO_SERIALIZE | HEAP_GROWABLE,
  0x10000, 0);
  }
}

void *wr_heap_masq_alloc(size_t bytes)
{
#if WRAITH_USE_HEAP_MASQUERADE
  ensure_heap();
  if (!g_heap) {
  return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
  }
  return HeapAlloc(g_heap, HEAP_ZERO_MEMORY, bytes);
#else
  return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
#endif
}

void wr_heap_masq_free(void *p)
{
  if (!p) return;
#if WRAITH_USE_HEAP_MASQUERADE
  if (g_heap && HeapValidate(g_heap, 0, p)) {
  HeapFree(g_heap, 0, p);
  return;
  }
#endif
  HeapFree(GetProcessHeap(), 0, p);
}

void wr_heap_masq_release(void)
{
#if WRAITH_USE_HEAP_MASQUERADE
  if (g_heap) {
  HeapDestroy(g_heap);
  g_heap = NULL;
  }
#endif
}
