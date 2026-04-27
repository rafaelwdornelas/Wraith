/*
 * src/runtime/rt_pebwalk.h
 *
 * Walks PEB.Ldr.InMemoryOrderModuleList to locate a loaded module by
 * its (case-insensitive) DJB2 base-name hash. Used as the GetModuleHandle
 * replacement when WRAITH_F_API_HASHING is requested.
 */

#ifndef WRAITH_RT_PEBWALK_H
#define WRAITH_RT_PEBWALK_H

#include "wraith/wraith_status.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns base address of the module whose BaseDllName hashes to
 * `name_hash`. Returns WRAITH_E_RT_PEB_WALK_FAILED if not found.
 *
 * The function never enters loader-lock and never calls into ntdll's
 * loader APIs - it just dereferences the PEB.Ldr lists. */
wraith_status_t wr_pebwalk_find_module(uint32_t name_hash, void **out_base);

/* Convenience: ASCII variant - hashes the name internally. */
wraith_status_t wr_pebwalk_find_module_a(const char *name, void **out_base);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_PEBWALK_H */
