/*
 * src/stealth/etw/etw_patch.h
 *
 * Hot-patch ntdll!EtwEventWrite so it returns ERROR_SUCCESS without
 * forwarding the event into the ETW dispatcher. Silences userland
 * ETW telemetry (the channel most EDR products consume in user
 * mode); does NOT silence ETW-Ti, which is dispatched in kernel
 * space and therefore unaffected by user-mode bytes.
 */

#ifndef WRAITH_ETW_PATCH_H
#define WRAITH_ETW_PATCH_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. Safe to call multiple times. Returns WRAITH_OK if the
 * patch was already in place. */
wraith_status_t wr_etw_patch_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_ETW_PATCH_H */
