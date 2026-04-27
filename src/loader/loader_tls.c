/*
 * src/loader/loader_tls.c
 *
 * Walk IMAGE_DIRECTORY_ENTRY_TLS and run callbacks. only invokes
 * the ATTACH callbacks - DETACH/THREAD lifecycle lands in */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"

#include <windows.h>

wraith_status_t wr_load_run_tls_attach(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  (void)src;
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
  if (dir->VirtualAddress == 0) {
  return WRAITH_OK;  /* no TLS data */
  }

  PIMAGE_TLS_DIRECTORY64 tls =
  (PIMAGE_TLS_DIRECTORY64)(ctx->image_base + dir->VirtualAddress);

  PIMAGE_TLS_CALLBACK *cb = (PIMAGE_TLS_CALLBACK *)tls->AddressOfCallBacks;
  if (!cb) {
  return WRAITH_OK;
  }
  while (*cb) {
  (*cb)((LPVOID)ctx->image_base, DLL_PROCESS_ATTACH, NULL);
  ++cb;
  }
  ctx->tls_attach_ran = true;
  return WRAITH_OK;
}

void wr_load_run_tls_detach(struct wr_ctx *ctx)
{
  /* Only fire DETACH when ATTACH actually ran. The pipeline calls this
  * from wr_pipeline_unwind on every failure path - including failures
  * before phase 14 (tls_attach) ever ran. Without this guard, a failure
  * at phase 9 (imports) would invoke the payload's TLS callbacks on a
  * .text that finalize never flipped to RX, faulting with
  * ExceptionAddress == FaultAddress at the callback's RVA. */
  if (!ctx || !ctx->headers || !ctx->tls_attach_ran) {
  return;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
  if (dir->VirtualAddress == 0) {
  return;
  }
  PIMAGE_TLS_DIRECTORY64 tls =
  (PIMAGE_TLS_DIRECTORY64)(ctx->image_base + dir->VirtualAddress);
  PIMAGE_TLS_CALLBACK *cb = (PIMAGE_TLS_CALLBACK *)tls->AddressOfCallBacks;
  if (!cb) {
  return;
  }
  while (*cb) {
  (*cb)((LPVOID)ctx->image_base, DLL_PROCESS_DETACH, NULL);
  ++cb;
  }
}
