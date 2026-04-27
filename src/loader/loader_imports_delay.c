/*
 * src/loader/loader_imports_delay.c
 *
 * Eagerly resolve IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT entries.
 *
 * The Microsoft delay-load mechanism normally defers resolution until
 * the first call site invokes __delayLoadHelper2. We instead resolve
 * everything up-front and patch the IAT, which:
 *  - keeps the loader self-contained (no helper stub injection)
 *  - guarantees deterministic state for subsequent stealth phases
 *  (stack spoofing relies on knowing all call targets)
 *
 * Layout (PE32+ V2 with bit 0 of grAttrs set - the only modern variant):
 *
 *  typedef struct ImgDelayDescr {
 *  DWORD grAttrs;  // bit 0 set => RVAs (V2). bit 0 clear => VAs (V1).
 *  DWORD rvaDLLName;
 *  DWORD rvaHmod;  // storage for HMODULE - we write our handle
 *  DWORD rvaIAT;  // patched in place
 *  DWORD rvaINT;  // hint/name table read for resolution
 *  DWORD rvaBoundIAT;  // ignored
 *  DWORD rvaUnloadIAT;  // ignored
 *  DWORD dwTimeStamp;  // 0 if not bound
 *  } ImgDelayDescr;
 */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"
#include "runtime/rt_api.h"

#include <stdlib.h>
#include <windows.h>

#define IMAGE_ORDINAL_FLAG64_LOCAL 0x8000000000000000ULL

#pragma pack(push, 1)
typedef struct wr_delay_descr {
  DWORD grAttrs;
  DWORD rvaDLLName;
  DWORD rvaHmod;
  DWORD rvaIAT;
  DWORD rvaINT;
  DWORD rvaBoundIAT;
  DWORD rvaUnloadIAT;
  DWORD dwTimeStamp;
} wr_delay_descr;
#pragma pack(pop)

#define WRAITH_DELAY_ATTR_RVA  0x00000001u

wraith_status_t wr_load_imports_delay(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
  if (dir->Size == 0) {
  return WRAITH_OK;  /* no delay imports */
  }

  const struct wr_rt_ops *rt = ctx->rt_ops ? ctx->rt_ops
  : wr_rt_resolve(ctx);
  ctx->rt_ops = rt;

  wr_delay_descr *desc =
  (wr_delay_descr *)(ctx->image_base + dir->VirtualAddress);

  while (desc->rvaDLLName != 0) {
  if (!(desc->grAttrs & WRAITH_DELAY_ATTR_RVA)) {
  /* V1 (absolute VAs) - very rare, predates Win2000. We refuse
  * rather than risk patching wrong addresses. */
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DELAY_BAD_DESCR,
  "V1 delay descriptor (VA layout) unsupported");
  }

  const char *dll_name = (const char *)(ctx->image_base + desc->rvaDLLName);

  wraith_foreign_module_t host = NULL;
  wraith_status_t rc = rt->load_library(ctx, dll_name, &host);
  if (rc != WRAITH_OK) {
  return rc;
  }

  /* Track for cleanup. */
  size_t newcap = ctx->imported_count + 1;
  wraith_foreign_module_t *grown =
  (wraith_foreign_module_t *)realloc(ctx->imported_modules,
  newcap * sizeof(*grown));
  bool *owned_grown =
  (bool *)realloc(ctx->imported_owned, newcap * sizeof(*owned_grown));
  if (!grown || !owned_grown) {
  free(grown);
  free(owned_grown);
  rt->free_library(ctx, host);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "delay tracking realloc");
  }
  ctx->imported_modules  = grown;
  ctx->imported_owned  = owned_grown;
  ctx->imported_modules[ctx->imported_count] = host;
  ctx->imported_owned[ctx->imported_count]  = true;
  ctx->imported_count  = (uint32_t)newcap;

  /* Persist HMODULE in the slot the linker reserved. Some
  * __delayLoadHelper2 codegens dereference it. */
  if (desc->rvaHmod) {
  void **hmod_slot = (void **)(ctx->image_base + desc->rvaHmod);
  *hmod_slot = host;
  }

  /* Walk INT, write resolved addresses into IAT. */
  uint64_t *int_thunk =
  (uint64_t *)(ctx->image_base + desc->rvaINT);
  void  **iat_slot  =
  (void  **)(ctx->image_base + desc->rvaIAT);

  while (*int_thunk) {
  void *resolved = NULL;
  if (*int_thunk & IMAGE_ORDINAL_FLAG64_LOCAL) {
  uint16_t ord = (uint16_t)(*int_thunk & 0xffff);
  rc = rt->get_proc(ctx, host,
  (const char *)(uintptr_t)ord, &resolved);
  } else {
  PIMAGE_IMPORT_BY_NAME by_name =
  (PIMAGE_IMPORT_BY_NAME)(ctx->image_base + *int_thunk);
  rc = rt->get_proc(ctx, host, (const char *)by_name->Name,
  &resolved);
  }
  if (rc != WRAITH_OK || !resolved) {
  return rc != WRAITH_OK
  ? rc
  : wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "delay-import resolution NULL");
  }
  *iat_slot = resolved;
  ++int_thunk;
  ++iat_slot;
  }

  ++desc;
  }

  return WRAITH_OK;
}
