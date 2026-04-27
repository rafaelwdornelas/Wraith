/*
 * src/stealth/anti_debug/anti_debug.h
 *
 * passive anti-debug masking. Zeroes the PEB flags that
 * generic anti-debug checks consult. Does NOT block a connected
 * debugger; it just hides the signals that reveal one.
 */

#ifndef WRAITH_ANTI_DEBUG_H
#define WRAITH_ANTI_DEBUG_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. Returns S_OK in all reasonable configurations. */
wraith_status_t wr_anti_debug_spoof_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_ANTI_DEBUG_H */
