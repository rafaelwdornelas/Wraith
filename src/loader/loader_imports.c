/*
 * src/loader/loader_imports.c
 *
 * Walk IMAGE_DIRECTORY_ENTRY_IMPORT, resolve each dependency through the
 * runtime vtable, and patch the IAT in place.
 *
 * Scope of this file:
 *   - Normal imports (by name + by ordinal)
 *   - Forwarded exports inside imported DLLs are followed implicitly
 *     by GetProcAddress / the resolver path
 *
 * Bound and delay imports live in `loader_imports_bound.c` and
 * `loader_imports_delay.c` respectively.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "loader/loader_pipeline.h"
#include "runtime/rt_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <winnt.h>

#define IMAGE_ORDINAL_FLAG64_LOCAL 0x8000000000000000ULL

static int snap_by_ordinal(uint64_t thunk)
{
  return (thunk & IMAGE_ORDINAL_FLAG64_LOCAL) != 0;
}

static uint16_t ordinal_of(uint64_t thunk)
{
  return (uint16_t)(thunk & 0xffff);
}

wraith_status_t wr_load_imports_resolve(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base) ||
      !wr_looks_like_valid_base(ctx->headers)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "imports: invalid image_base=%p / headers=%p",
  (void *)ctx->image_base, ctx->headers);
  }

  PIMAGE_DATA_DIRECTORY dir = NULL;
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir->Size == 0) {
  return WRAITH_OK;  /* no imports - perfectly legal */
  }

  PIMAGE_IMPORT_DESCRIPTOR desc =
  (PIMAGE_IMPORT_DESCRIPTOR)(ctx->image_base + dir->VirtualAddress);

  const struct wr_rt_ops *rt = ctx->rt_ops ? ctx->rt_ops
  : wr_rt_resolve(ctx);
  ctx->rt_ops = rt;

  while (desc->Name != 0) {
  const char *dll_name = (const char *)(ctx->image_base + desc->Name);

  wraith_foreign_module_t foreign = NULL;
  wraith_status_t rc = rt->load_library(ctx, dll_name, &foreign);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (!wr_looks_like_valid_base(foreign)) {
  /* The runtime claimed success but handed back an unusable
  * handle. Refuse rather than feed it to get_proc, which would
  * deref a near-NULL pointer reading the export table. */
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "imports: load_library(\"%s\") returned invalid base %p",
  dll_name, (void *)foreign);
  }

  /* Track for cleanup in wraith_free_library. */
  size_t newcap = ctx->imported_count + 1;
  wraith_foreign_module_t *grown =
  (wraith_foreign_module_t *)realloc(ctx->imported_modules,
  newcap * sizeof(*grown));
  bool *owned_grown =
  (bool *)realloc(ctx->imported_owned, newcap * sizeof(*owned_grown));
  if (!grown || !owned_grown) {
  free(grown);
  free(owned_grown);
  rt->free_library(ctx, foreign);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "imports: realloc tracking");
  }
  ctx->imported_modules  = grown;
  ctx->imported_owned  = owned_grown;
  ctx->imported_modules[ctx->imported_count] = foreign;
  ctx->imported_owned[ctx->imported_count]  = true;
  ctx->imported_count = (uint32_t)newcap;

  uint64_t *thunk_ref;
  void  **func_ref;

  if (desc->OriginalFirstThunk) {
  thunk_ref = (uint64_t *)(ctx->image_base + desc->OriginalFirstThunk);
  func_ref  = (void **)(ctx->image_base + desc->FirstThunk);
  } else {
  /* No hint table - thunks live in IAT directly. */
  thunk_ref = (uint64_t *)(ctx->image_base + desc->FirstThunk);
  func_ref  = (void **)(ctx->image_base + desc->FirstThunk);
  }

  while (*thunk_ref) {
  void *resolved = NULL;
  if (snap_by_ordinal(*thunk_ref)) {
  /* GetProcAddress accepts (LPCSTR)MAKEINTRESOURCE(ord) - we
  * forward this idiom through the vtable by passing the
  * ordinal in the lower 16 bits and letting the rt know. */
  const char *as_ord =
  (const char *)(uintptr_t)ordinal_of(*thunk_ref);
  rc = rt->get_proc(ctx, foreign, as_ord, &resolved);
  } else {
  PIMAGE_IMPORT_BY_NAME by_name =
  (PIMAGE_IMPORT_BY_NAME)(ctx->image_base + *thunk_ref);
  rc = rt->get_proc(ctx, foreign, (const char *)by_name->Name,
  &resolved);
  }
  if (rc != WRAITH_OK || !resolved) {
  return rc != WRAITH_OK ? rc
  : wr_ctx_fail(ctx,
  WRAITH_E_IMP_PROC_NOT_FOUND,
  "import resolution NULL");
  }
  *func_ref = resolved;
  ++thunk_ref;
  ++func_ref;
  }
  ++desc;
  }

  return WRAITH_OK;
}
