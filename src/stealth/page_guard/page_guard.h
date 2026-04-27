/*
 * src/stealth/page_guard/page_guard.h
 *
 * Lazy per-page self-encryption via PAGE_GUARD + VEH.
 *
 * Mechanism:
 *  - "Arm" walks every executable page of the loaded image,
 *  XOR-encrypts it with a rolling key, then VirtualProtects it
 *  to `PAGE_EXECUTE_READ | PAGE_GUARD`.
 *  - When the CPU first executes (or just touches) a guarded
 *  page, it raises EXCEPTION_GUARD_PAGE_VIOLATION and clears
 *  the guard bit on that page.
 *  - A vectored exception handler catches the fault, identifies
 *  the page in our table, decrypts it in place, sets the
 *  final RX protection, and returns EXCEPTION_CONTINUE_EXECUTION
 *  so the original instruction reruns against the now-plain bytes.
 *  - "Disarm" decrypts any pages that are still encrypted and
 *  removes the VEH.
 *
 * Footprint while armed: only the page(s) currently executing are
 * decrypted; the rest stay encrypted on disk and in memory. Idle
 * processes that hold the loaded module but aren't actively
 * running its code present zero exposed image bytes to a memory
 * scanner.
 *
 * ships the basic single-module variant (one armed
 * module at a time). Multiple concurrent armed modules are
 * deferred.
 */

#ifndef WRAITH_PAGE_GUARD_H
#define WRAITH_PAGE_GUARD_H

#include "wraith/wraith_status.h"
#include "wraith/wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* No internal-only entry points yet beyond the public API
 * declared in wr_stealth.h: wraith_pageguard_arm / wraith_pageguard_disarm. */

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PAGE_GUARD_H */
