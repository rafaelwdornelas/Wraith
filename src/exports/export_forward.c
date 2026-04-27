/*
 * src/exports/export_forward.c
 *
 * Implementation of the forwarder resolver.
 *
 * Forwarder string syntax (per the PE spec):
 *  "<DLL>.<Func>"  - resolve Func by name in DLL
 *  "<DLL>.#<digits>"  - resolve by ordinal in DLL
 *
 * The DLL filename in the string is short (no .dll extension). We append
 * ".dll" before passing it to the runtime so LoadLibraryA / PEB-walk
 * resolvers see a canonical name.
 */

#include "exports/export_forward.h"
#include "runtime/rt_api.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WRAITH_FORWARD_MAX_DEPTH  16

bool wr_export_is_forwarder(struct wr_ctx *ctx, void *proc)
{
  if (!ctx || !proc || !ctx->headers) {
  return false;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir->Size == 0) {
  return false;
  }
  uintptr_t lo = (uintptr_t)(ctx->image_base + dir->VirtualAddress);
  uintptr_t hi = lo + dir->Size;
  uintptr_t p  = (uintptr_t)proc;
  return p >= lo && p < hi;
}

wraith_status_t wr_export_resolve_forwarder(struct wr_ctx *ctx,
  const char *forward_str,
  void **out_proc)
{
  if (!ctx || !forward_str || !out_proc) {
  return WRAITH_E_NULL_ARG;
  }
  *out_proc = NULL;

  /* Split "DLL.Func" on the first '.'. The DLL portion never contains
  * a dot itself in practice (Microsoft uses "kernel32", "ntdll",
  * etc.) so we keep this simple. */
  const char *dot = strchr(forward_str, '.');
  if (!dot || dot == forward_str || dot[1] == '\0') {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_FORWARDER_LOOP,
  "malformed forwarder \"%s\"", forward_str);
  }

  size_t dll_len = (size_t)(dot - forward_str);
  if (dll_len > 64) {
  /* Unusually long DLL name - reject rather than overflow stack. */
  return wr_ctx_fail(ctx, WRAITH_E_IMP_FORWARDER_LOOP,
  "forwarder DLL name too long");
  }
  char dll_buf[80];
  memcpy(dll_buf, forward_str, dll_len);
  /* Append .dll if not already present. */
  static const char ext[] = ".dll";
  memcpy(dll_buf + dll_len, ext, sizeof(ext));

  const char *func_part = dot + 1;

  const struct wr_rt_ops *rt = ctx->rt_ops ? ctx->rt_ops
  : wr_rt_resolve(ctx);
  ctx->rt_ops = rt;

  wraith_foreign_module_t host = NULL;
  wraith_status_t rc = rt->load_library(ctx, dll_buf, &host);
  if (rc != WRAITH_OK) {
  return rc;
  }

  /* "#NNN" -> ordinal lookup. */
  void *resolved = NULL;
  if (func_part[0] == '#') {
  unsigned long ord = strtoul(func_part + 1, NULL, 10);
  rc = rt->get_proc(ctx, host, (const char *)(uintptr_t)ord, &resolved);
  } else {
  rc = rt->get_proc(ctx, host, func_part, &resolved);
  }
  if (rc != WRAITH_OK) {
  rt->free_library(ctx, host);
  return rc;
  }

  /* Track the forwarder host so wraith_free_library decrements its refcount. */
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
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "forwarder tracking realloc");
  }
  ctx->imported_modules  = grown;
  ctx->imported_owned  = owned_grown;
  ctx->imported_modules[ctx->imported_count] = host;
  ctx->imported_owned[ctx->imported_count]  = true;
  ctx->imported_count  = (uint32_t)newcap;

  *out_proc = resolved;
  return WRAITH_OK;
}
