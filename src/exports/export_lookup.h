/*
 * src/exports/export_lookup.h
 *
 * Binary-search export lookup. The first call lazily builds a sorted
 * (name, ordinal) table on the heap; subsequent calls bsearch into it.
 */

#ifndef WRAITH_EXPORT_LOOKUP_H
#define WRAITH_EXPORT_LOOKUP_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_export_entry {
  const char *name;
  uint16_t  ordinal;
} wr_export_entry;

/* Resolve an export by name (`name` is a real string) or by ordinal
 * (pass (const char *)(uintptr_t)ordinal, like Win32). */
wraith_status_t wr_export_resolve(struct wr_ctx *ctx, const char *name_or_ord,
  void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_EXPORT_LOOKUP_H */
