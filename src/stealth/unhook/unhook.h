/*
 * src/stealth/unhook/unhook.h
 *
 * Userland-hook removal. Reads ntdll.dll from disk, compares each
 * 16-byte chunk of `.text` against the loaded copy, and patches the
 * loaded copy where they differ.
 *
 * For a stronger posture see `WRAITH_USE_PRIVATE_NTDLL`, which maps a
 * private second copy of ntdll and bypasses userland hooks by
 * construction rather than removing them in place.
 */

#ifndef WRAITH_UNHOOK_H
#define WRAITH_UNHOOK_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_unhook_ntdll_disk(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_UNHOOK_H */
