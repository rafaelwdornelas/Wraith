/*
 * src/syscalls/sc_ssn_resolver.h
 */

#ifndef WRAITH_SC_SSN_RESOLVER_H
#define WRAITH_SC_SSN_RESOLVER_H

#include "wraith/wraith_status.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Try to resolve `name`'s SSN inside `ntdll_base` (already located via
 * PEB walk). On success writes the SSN to *out_ssn and returns S_OK.
 * On failure returns WRAITH_E_SC_SSN_NOT_RESOLVED.
 *
 * Resolution strategy (3-tier fallback chain):
 *  1. Hell's Hall: locate "Nt<name>" and match the canonical
 *  prologue pattern `4c 8b d1 b8 XX XX 00 00`. Extract SSN
 *  from bytes 4..5.
 *  2. Halo's Gate: if the prologue is JMP-hooked, scan +/- 32
 *  neighbour exports for an intact prologue and derive the
 *  target SSN by index distance (the kernel assigns SSNs in
 *  ascending RVA order).
 *  3. FreshyCalls : sort all "Nt"-prefixed exports by RVA
 *  and look up `name`'s position in the sorted list - that
 *  index IS the SSN. Survives the case where ALL Nt* are
 *  hooked (Halo's Gate fails) since it doesn't read prologue
 *  bytes at all. */
wraith_status_t wr_sc_resolve_ssn(void *ntdll_base, const char *name,
  uint32_t *out_ssn);

/* FreshyCalls-only entrypoint: build a sorted-by-RVA index of
 * "Nt"-prefixed exports and return `name`'s position in it.
 * Exposed publicly so tests can validate that FreshyCalls matches
 * the prologue-based result. */
wraith_status_t wr_sc_resolve_ssn_by_rva(void *ntdll_base, const char *name,
  uint32_t *out_ssn);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_SSN_RESOLVER_H */
