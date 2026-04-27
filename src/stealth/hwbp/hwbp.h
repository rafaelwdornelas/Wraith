/*
 * src/stealth/hwbp/hwbp.h
 *
 * Hardware-breakpoint hook manager. Public API in wr_stealth.h:
 *  wraith_hwbp_install(target, replacement, dr_index)
 *  wraith_hwbp_remove(dr_index)
 *
 * Mechanism:
 *  - Set a `LEN=1, RWE=execute` debug breakpoint at `target` via
 *  `Wow64GetThreadContext` / `SetThreadContext` on the current
 *  thread (DR0..DR3, picked automatically when dr_index < 0).
 *  - Install a process-global Vectored Exception Handler that
 *  intercepts EXCEPTION_SINGLE_STEP, finds which slot fired by
 *  comparing `Rip` to the cached target, and rewrites
 *  `ContextRecord->Rip` to `replacement` before returning
 *  EXCEPTION_CONTINUE_EXECUTION.
 *
 * Zero memory modification: the target function's bytes never
 * change. Survives `.text` integrity checks against the on-disk
 * hash. The trade-off is per-thread state (DRs are thread-local)
 * and a process-global VEH (one instance covers any number of
 * installed slots).
 */

#ifndef WRAITH_HWBP_H
#define WRAITH_HWBP_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* No internal-only entry points yet beyond the public API. */

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HWBP_H */
