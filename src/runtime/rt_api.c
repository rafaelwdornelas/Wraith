/*
 * src/runtime/rt_api.c
 *
 * Runtime selector. Today this is a one-liner - it returns the
 * baseline. + extends it to:
 *  - prefer the Hell's Hall vtable when WRAITH_F_INDIRECT_SYSCALLS is set
 *  - fall back to baseline if SSN resolution fails
 */

#include "core/wr_context_internal.h"
#include "wraith/wraith_types.h"
#include "runtime/rt_api.h"

#if WRAITH_USE_STACK_SPOOF
extern void wr_sc_engine_set_thread_spoof(int enabled);
#endif

const struct wr_rt_ops *wr_rt_resolve(struct wr_ctx *ctx)
{
#if WRAITH_USE_STACK_SPOOF
  /* Tell the syscall engine whether this load wants stack-spoofed
  * trampolines. The toggle is per-thread; loads on different
  * threads can request different policies. */
  if (ctx) {
  wr_sc_engine_set_thread_spoof(
  (ctx->flags & WRAITH_F_STACK_SPOOF) ? 1 : 0);
  }
#endif

#if WRAITH_USE_API_HASHING
  if (ctx && (ctx->flags & WRAITH_F_API_HASHING)) {
  return &wr_rt_ops_ntapi;
  }
#endif
  (void)ctx;
  return &wr_rt_ops_baseline;
}
