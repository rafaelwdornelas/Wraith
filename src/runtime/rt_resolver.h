/*
 * src/runtime/rt_resolver.h
 *
 * Walk a loaded module's IMAGE_EXPORT_DIRECTORY by hash, returning the
 * function address. The companion to rt_pebwalk: GetModuleHandle is
 * replaced by wr_pebwalk_find_module, GetProcAddress by
 * wr_resolver_lookup.
 *
 * Forwarder strings are resolved transparently - if the resolved RVA
 * points back into the export directory, the function follows the
 * "DLL.Func" reference via PEB walk and a recursive lookup.
 */

#ifndef WRAITH_RT_RESOLVER_H
#define WRAITH_RT_RESOLVER_H

#include "wraith/wraith_status.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve a function in `module_base` whose ASCII name hashes to
 * `name_hash`. Returns WRAITH_E_RT_API_NOT_RESOLVED on miss. */
wraith_status_t wr_resolver_lookup(void *module_base, uint32_t name_hash,
  void **out_proc);

/* Convenience: hashes the ASCII name internally. */
wraith_status_t wr_resolver_lookup_a(void *module_base, const char *name,
  void **out_proc);

/* Resolve by export ordinal. Follows forwarders identically to the
 * by-name path - including ordinal-form forwarders ("DLL.#NNN"). */
wraith_status_t wr_resolver_lookup_ordinal(void *module_base, uint16_t ordinal,
                                           void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_RESOLVER_H */
