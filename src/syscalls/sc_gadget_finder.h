/*
 * src/syscalls/sc_gadget_finder.h
 */

#ifndef WRAITH_SC_GADGET_FINDER_H
#define WRAITH_SC_GADGET_FINDER_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Locate a `syscall; ret` (0f 05 c3) gadget inside ntdll's .text.
 * Returns S_OK on success and writes the address to *out_gadget.
 * Returns WRAITH_E_SC_NO_GADGET when none is found (e.g. under wine64,
 * which doesn't ship that exact byte sequence in user-mode ntdll). */
wraith_status_t wr_sc_find_syscall_gadget(void *ntdll_base, void **out_gadget);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_GADGET_FINDER_H */
