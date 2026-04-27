/*
 * src/exports/export_forward.h
 *
 * Forwarder export resolution. When an export's RVA points back into the
 * export directory itself, the data at that RVA is an ASCII string of the
 * form "DLL.Func" (or "DLL.#ord") - the canonical PE forwarder layout.
 *
 * The resolver loads the target DLL via the runtime vtable and resolves
 * the name (or ordinal) inside it. Loops are detected and reported.
 */

#ifndef WRAITH_EXPORT_FORWARD_H
#define WRAITH_EXPORT_FORWARD_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* True if the proc address `proc` is actually a pointer to a forwarder
 * string inside the export directory. */
bool wr_export_is_forwarder(struct wr_ctx *ctx, void *proc);

/* Resolve a forwarder string ("kernel32.Sleep" / "ntdll.#0x42") to a
 * concrete function address by loading the target DLL through the
 * runtime vtable. Loop depth is capped at 16. */
wraith_status_t wr_export_resolve_forwarder(struct wr_ctx *ctx,
  const char *forward_str,
  void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_EXPORT_FORWARD_H */
