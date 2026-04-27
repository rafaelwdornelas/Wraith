/*
 * src/stealth/amsi/amsi_patch.h
 *
 * Hot-patch amsi.dll!AmsiScanBuffer. Relevant only for processes
 * that load .NET / PowerShell / JScript runtimes that submit
 * buffers to AMSI before execution. Native PE loading does not
 * invoke AMSI.
 */

#ifndef WRAITH_AMSI_PATCH_H
#define WRAITH_AMSI_PATCH_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Loads amsi.dll via LoadLibraryW if it is not already present in
 * the process; that's why the function is gated behind explicit
 * WRAITH_F_AMSI_PATCH (forces the user to opt in to amsi.dll being
 * brought into the process). Idempotent. */
wraith_status_t wr_amsi_patch_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_AMSI_PATCH_H */
