/*
 * src/runtime/rt_api.h
 *
 * The runtime layer is the only place the rest of the loader is allowed
 * to touch OS APIs. By isolating it behind an `wr_rt_ops` vtable we
 * can swap baseline (Win32) for indirect-syscalls (Hell's Hall) at
 * runtime without touching loader code.
 */

#ifndef WRAITH_RT_API_H
#define WRAITH_RT_API_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wr_rt_ops {
  const char *name;

  /* Resolve a foreign module by ASCII name (e.g. "kernel32.dll").
  * + swaps this for PEB-walk + API hashing. */
  wraith_status_t (*load_library)(struct wr_ctx *ctx,
  const char *name,
  wraith_foreign_module_t *out);

  /* Resolve a procedure within a foreign module. */
  wraith_status_t (*get_proc)(struct wr_ctx *ctx,
  wraith_foreign_module_t module,
  const char *name,
  void **out_proc);

  /* Release a foreign module (only for ones we loaded). */
  void  (*free_library)(struct wr_ctx *ctx,
  wraith_foreign_module_t module);

  /* Memory primitives. The baseline goes through Win32
  * VirtualAlloc/Protect/Free; the ntapi vtable routes through the
  * Hell's Hall engine (sc_engine). Semantics match the Nt* APIs:
  *  - addr is in/out (caller may pass a preferred base)
  *  - size is in/out (the kernel rounds up)
  *  - protect uses Win32 PAGE_* constants
  *  - return is 0 on success, non-zero NTSTATUS otherwise */
  int (*nt_alloc)(void **addr, size_t *size,
  unsigned alloc_type, unsigned protect);
  int (*nt_protect)(void *addr, size_t size,
  unsigned new_protect, unsigned *old_protect);
  int (*nt_free)(void *addr, size_t size, unsigned free_type);
  void (*nt_flush_icache)(void *addr, size_t size);
};

/* Default (Win32 baseline) vtable. */
extern const struct wr_rt_ops wr_rt_ops_baseline;

/* Hash-based vtable using PEB walk + DJB2 export resolver.
 * Compiled out when WRAITH_USE_API_HASHING is OFF. */
#if WRAITH_USE_API_HASHING
extern const struct wr_rt_ops wr_rt_ops_ntapi;
#endif

/* Selects the runtime vtable based on options.flags. always
 * returns the baseline; later phases wire in NTAPI / syscall variants. */
const struct wr_rt_ops *wr_rt_resolve(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_API_H */
