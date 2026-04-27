/*
 * src/exports/export_lookup.c
 *
 * Name + ordinal export resolution. Binary-search over the sorted
 * export-name array; returns rich wraith_status_t error codes.
 *
 * Forwarded exports (export RVA points back into the export-table
 * range, encoding "DLL.Func") are followed by `export_forward.c`.
 */

#include "exports/export_lookup.h"
#include "exports/export_forward.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

static int compare_entries(const void *a, const void *b)
{
  const wr_export_entry *ea = (const wr_export_entry *)a;
  const wr_export_entry *eb = (const wr_export_entry *)b;
  return strcmp(ea->name, eb->name);
}

static int find_entry(const void *key, const void *elem)
{
  const char *name = *(const char *const *)key;
  const wr_export_entry *e = (const wr_export_entry *)elem;
  return strcmp(name, e->name);
}

static wraith_status_t build_name_table(struct wr_ctx *ctx,
  PIMAGE_EXPORT_DIRECTORY exports)
{
  if (ctx->export_table) {
  return WRAITH_OK;
  }
  if (exports->NumberOfNames == 0) {
  return WRAITH_OK;
  }

  wr_export_entry *table =
  (wr_export_entry *)calloc(exports->NumberOfNames,
  sizeof(wr_export_entry));
  if (!table) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "export table alloc");
  }

  DWORD *name_rvas = (DWORD *)(ctx->image_base + exports->AddressOfNames);
  WORD  *ord_rvas  = (WORD  *)(ctx->image_base + exports->AddressOfNameOrdinals);
  for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
  table[i].name  = (const char *)(ctx->image_base + name_rvas[i]);
  table[i].ordinal = ord_rvas[i];
  }
  qsort(table, exports->NumberOfNames, sizeof(wr_export_entry),
  compare_entries);

  ctx->export_table = table;
  ctx->export_count = exports->NumberOfNames;
  return WRAITH_OK;
}

wraith_status_t wr_export_resolve(struct wr_ctx *ctx, const char *name_or_ord,
  void **out_proc)
{
  if (!ctx || !out_proc) {
  return WRAITH_E_NULL_ARG;
  }
  *out_proc = NULL;

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir->Size == 0) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_NO_TABLE,
  "image has no export table");
  }

  PIMAGE_EXPORT_DIRECTORY exports =
  (PIMAGE_EXPORT_DIRECTORY)(ctx->image_base + dir->VirtualAddress);
  if (exports->NumberOfFunctions == 0) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_NOT_FOUND,
  "image exports zero functions");
  }

  DWORD idx = 0;

  /* HIWORD == 0 -> caller passed an ordinal cast to (const char *). */
  if (((uintptr_t)name_or_ord >> 16) == 0) {
  WORD ord = (WORD)(uintptr_t)name_or_ord;
  if (ord < exports->Base) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_BAD_ORDINAL,
  "ordinal %u below Base %lu",
  (unsigned)ord, (unsigned long)exports->Base);
  }
  idx = ord - exports->Base;
  } else {
  wraith_status_t rc = build_name_table(ctx, exports);
  if (rc != WRAITH_OK) {
  return rc;
  }
  const char *needle = name_or_ord;
  const wr_export_entry *found =
  (const wr_export_entry *)bsearch(&needle, ctx->export_table,
  ctx->export_count,
  sizeof(wr_export_entry),
  find_entry);
  if (!found) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_NOT_FOUND,
  "export \"%s\" not found", needle);
  }
  idx = found->ordinal;
  }

  if (idx >= exports->NumberOfFunctions) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_BAD_ORDINAL,
  "ordinal index %lu out of range",
  (unsigned long)idx);
  }

  DWORD *funcs = (DWORD *)(ctx->image_base + exports->AddressOfFunctions);
  DWORD func_rva = funcs[idx];
  if (func_rva == 0) {
  return wr_ctx_fail(ctx, WRAITH_E_EXP_NOT_FOUND,
  "export at ordinal index %lu is NULL", (unsigned long)idx);
  }

  void *candidate = (void *)(ctx->image_base + func_rva);

#if WRAITH_FORWARDED_EXPORTS
  /* Forwarder detection: a forwarded export's RVA points back inside
  * the export directory itself ("kernel32.Sleep"). Resolve through
  * the runtime vtable and return the concrete address. */
  if (wr_export_is_forwarder(ctx, candidate)) {
  const char *forward_str = (const char *)candidate;
  void *resolved = NULL;
  wraith_status_t rc = wr_export_resolve_forwarder(ctx, forward_str, &resolved);
  if (rc != WRAITH_OK) {
  return rc;
  }
  *out_proc = resolved;
  return WRAITH_OK;
  }
#endif

  *out_proc = candidate;
  return WRAITH_OK;
}
