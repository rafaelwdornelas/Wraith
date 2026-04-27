/*
 * src/loader/loader_entry.c
 *
 * Invoke DllMain for DLL images, or stash the entry point address for
 * EXE images so the consumer can call wraith_call_entry_point later.
 */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"

#include <windows.h>

typedef BOOL (WINAPI *DllEntryProc)(HINSTANCE, DWORD, LPVOID);

wraith_status_t wr_load_run_entry(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  DWORD aoep = nt->OptionalHeader.AddressOfEntryPoint;

  if (aoep == 0) {
  ctx->dll_entry  = NULL;
  ctx->exe_entry  = NULL;
  return WRAITH_OK;
  }

  void *entry = ctx->image_base + aoep;

  if (ctx->image_type == WRAITH_IMAGE_DLL) {
  DllEntryProc dll = (DllEntryProc)(LPVOID)entry;
  ctx->dll_entry = (void *)dll;
  BOOL ok = dll((HINSTANCE)ctx->image_base, DLL_PROCESS_ATTACH, NULL);
  if (!ok) {
  return wr_ctx_fail(ctx, WRAITH_E_RT_DLLMAIN_FAILED,
  "DllMain returned FALSE");
  }
  ctx->initialized = 1;
  } else {
  ctx->exe_entry = entry;
  }
  return WRAITH_OK;
}

void wr_load_run_entry_detach(struct wr_ctx *ctx)
{
  if (!ctx || !ctx->initialized || !ctx->dll_entry) {
  return;
  }
  DllEntryProc dll = (DllEntryProc)ctx->dll_entry;
  dll((HINSTANCE)ctx->image_base, DLL_PROCESS_DETACH, NULL);
}
