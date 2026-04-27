/*
 * src/stealth/private_ntdll/private_ntdll.h
 *
 * Maps a private second copy of ntdll.dll into the process via
 * NtCreateSection(SEC_IMAGE) + NtMapViewOfSection. The second copy
 * is fresh from disk, so its `Nt*` thunks contain the canonical
 * `mov r10,rcx; mov eax,SSN; ...` prologues even when the OS-loaded
 * ntdll has inline hooks installed by an EDR product.
 *
 * ships this as a standalone primitive; the SSN resolver
 * (sc_ssn_resolver.c) and the runtime layer can opt into resolving
 * symbols against the private base instead of the OS-loaded one.
 */

#ifndef WRAITH_PRIVATE_NTDLL_H
#define WRAITH_PRIVATE_NTDLL_H

#include "wraith/wraith_status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. On success a private mapping of ntdll exists for the
 * lifetime of the process (or until wr_private_ntdll_release). */
wraith_status_t wr_private_ntdll_init(void);

/* Returns the private ntdll base, or NULL if init has not been
 * called or failed. */
void *wr_private_ntdll_get_base(void);

/* Returns the size of the private mapping in bytes (0 when unset). */
size_t wr_private_ntdll_get_size(void);

/* Unmap + close the private mapping. Idempotent. */
void wr_private_ntdll_release(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PRIVATE_NTDLL_H */
