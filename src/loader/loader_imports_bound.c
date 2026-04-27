/*
 * src/loader/loader_imports_bound.c
 *
 * Bound-import policy: ignore the timestamp/checksum payload, ALWAYS
 * resolve imports normally.
 *
 * Why: bound imports rely on the loader trusting that the dependency
 * DLL resides at the exact base address the linker assumed. Under ASLR
 * (mandatory since Win10) this assumption is essentially never true,
 * so honoring the bound IAT would yield invalid pointers. Modern
 * toolchains have stopped emitting them by default.
 *
 * This module exists as a documented no-op so that future scope
 * (e.g. rebasing-aware delay imports) has a natural home.
 */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"

#include <windows.h>

wraith_status_t wr_load_imports_bound_check(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT];
  /* If the linker emitted a bound table at all, leave a breadcrumb in
  * the trace channel but do not honor it. */
  if (dir->Size != 0) {
  wr_trace(ctx, 9, "bound_imports_skip", WRAITH_OK);
  }
  return WRAITH_OK;
}
