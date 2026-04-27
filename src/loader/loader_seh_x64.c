/*
 * src/loader/loader_seh_x64.c
 *
 * x64 structured-exception-handling registration.
 *
 * Background: on Windows x64 the unwinder (RtlUnwindEx) walks per-module
 * tables of `RUNTIME_FUNCTION` entries to know how to unwind a frame.
 * For PE images these entries live in `IMAGE_DIRECTORY_ENTRY_EXCEPTION`
 * (the .pdata section). The OS loader registers them automatically; a
 * memory-loaded image must register them itself via `RtlAddFunctionTable`,
 * otherwise any `__try`/`__except` (or any C++ exception, or any access
 * violation that would normally be handled) inside the loaded code
 * crashes the process with EXCEPTION_NONCONTINUABLE.
 *
 * shipped a no-op stub here. turns it on by default
 * (gated by WRAITH_REGISTER_SEH_X64, ON in every profile).
 *
 * Cleanup: ctx->runtime_funcs / functbl_registered are honored by
 * wr_load_unregister_seh_x64, called from wr_pipeline_unwind.
 */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"

#include <windows.h>

wraith_status_t wr_load_register_seh_x64(struct wr_ctx *ctx)
{
#if WRAITH_REGISTER_SEH_X64
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

  /* Empty .pdata is legal: pure-asm or stub DLLs may emit nothing.
  * In that case there's nothing to register and SEH isn't needed -
  * silently succeed. */
  if (dir->Size == 0 || dir->VirtualAddress == 0) {
  return WRAITH_OK;
  }

  /* RUNTIME_FUNCTION entries are RVAs relative to the image base; they
  * stay valid for the lifetime of the loaded image. We point at the
  * in-image table directly - no copy needed. */
  PRUNTIME_FUNCTION table =
  (PRUNTIME_FUNCTION)(ctx->image_base + dir->VirtualAddress);
  DWORD count = dir->Size / (DWORD)sizeof(RUNTIME_FUNCTION);

  if (count == 0) {
  return WRAITH_OK;
  }

  if (!RtlAddFunctionTable(table, count, (DWORD64)(uintptr_t)ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_SEH_REGISTER_FAILED,
  "RtlAddFunctionTable(%lu entries) returned FALSE: 0x%08lx",
  (unsigned long)count,
  (unsigned long)GetLastError());
  }

  ctx->runtime_funcs  = table;
  ctx->runtime_funcs_count = count;
  ctx->functbl_registered  = 1;
  return WRAITH_OK;
#else
  (void)ctx;
  return WRAITH_OK;
#endif
}

void wr_load_unregister_seh_x64(struct wr_ctx *ctx)
{
#if WRAITH_REGISTER_SEH_X64
  if (!ctx || !ctx->functbl_registered || !ctx->runtime_funcs) {
  return;
  }
  /* Best-effort: if the table no longer exists (e.g. the user module
  * already unmapped under us) the call returns FALSE and we ignore
  * it - we're tearing down anyway. */
  RtlDeleteFunctionTable((PRUNTIME_FUNCTION)ctx->runtime_funcs);
  ctx->functbl_registered  = 0;
  ctx->runtime_funcs  = NULL;
  ctx->runtime_funcs_count = 0;
#else
  (void)ctx;
#endif
}
