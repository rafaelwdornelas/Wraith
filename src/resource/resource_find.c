/*
 * src/resource/resource_find.c
 *
 * Three-level resource directory walk: type -> name -> language.
 * Returns wraith_status_t at the public API edge.
 */

#include "wraith/wraith_resource.h"
#include "resource/resource_internal.h"

#include <string.h>
#include <wchar.h>
#include <windows.h>

static PIMAGE_RESOURCE_DIRECTORY_ENTRY
search_entry(void *root, PIMAGE_RESOURCE_DIRECTORY dir, const void *key)
{
  PIMAGE_RESOURCE_DIRECTORY_ENTRY entries =
  (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(dir + 1);

  if (((uintptr_t)key >> 16) == 0) {
  WORD wkey = (WORD)(uintptr_t)key;
  DWORD start = dir->NumberOfNamedEntries;
  DWORD end  = start + dir->NumberOfIdEntries;
  while (end > start) {
  DWORD mid = (start + end) >> 1;
  WORD entry_key = (WORD)entries[mid].Name;
  if (wkey < entry_key) {
  end = (end != mid ? mid : mid - 1);
  } else if (wkey > entry_key) {
  start = (start != mid ? mid : mid + 1);
  } else {
  return &entries[mid];
  }
  }
  return NULL;
  }

  /* Named entry. Resource names are stored as wide strings; we accept
  * input as ASCII or wide and convert if needed. The vast majority of
  * real-world consumers pass MAKEINTRESOURCE(id) (handled above), so
  * this slow path is acceptable. */
  const char *as_ansi = (const char *)key;
  size_t needle_chars = 0;
  wchar_t needle_buf[2048];
  {
  size_t n = mbstowcs(needle_buf, as_ansi,
  sizeof(needle_buf) / sizeof(needle_buf[0]) - 1);
  if (n == (size_t)-1) {
  return NULL;
  }
  needle_buf[n] = 0;
  needle_chars  = n;
  }

  DWORD start = 0;
  DWORD end  = dir->NumberOfNamedEntries;
  while (end > start) {
  DWORD mid = (start + end) >> 1;
  PIMAGE_RESOURCE_DIR_STRING_U s =
  (PIMAGE_RESOURCE_DIR_STRING_U)
  ((uint8_t *)root + (entries[mid].Name & 0x7fffffff));
  int cmp = _wcsnicmp(needle_buf, s->NameString, s->Length);
  if (cmp == 0) {
  if (needle_chars > s->Length) cmp =  1;
  else if (needle_chars < s->Length) cmp = -1;
  }
  if (cmp < 0) {
  end  = (mid != end ? mid : mid - 1);
  } else if (cmp > 0) {
  start = (mid != start ? mid : mid + 1);
  } else {
  return &entries[mid];
  }
  }
  return NULL;
}

PIMAGE_RESOURCE_DATA_ENTRY wr_resource_find_entry(struct wr_ctx *ctx,
  const void *name,
  const void *type,
  uint16_t language)
{
  if (!ctx || !ctx->headers) {
  return NULL;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
  if (dir->Size == 0) {
  return NULL;
  }

  if (language == WRAITH_LANG_DEFAULT) {
  language = LANGIDFROMLCID(GetThreadLocale);
  }

  PIMAGE_RESOURCE_DIRECTORY root =
  (PIMAGE_RESOURCE_DIRECTORY)(ctx->image_base + dir->VirtualAddress);

  PIMAGE_RESOURCE_DIRECTORY_ENTRY found_type =
  search_entry(root, root, type);
  if (!found_type) {
  return NULL;
  }
  PIMAGE_RESOURCE_DIRECTORY type_dir =
  (PIMAGE_RESOURCE_DIRECTORY)
  (ctx->image_base + dir->VirtualAddress +
  (found_type->OffsetToData & 0x7fffffff));

  PIMAGE_RESOURCE_DIRECTORY_ENTRY found_name =
  search_entry(root, type_dir, name);
  if (!found_name) {
  return NULL;
  }
  PIMAGE_RESOURCE_DIRECTORY name_dir =
  (PIMAGE_RESOURCE_DIRECTORY)
  (ctx->image_base + dir->VirtualAddress +
  (found_name->OffsetToData & 0x7fffffff));

  PIMAGE_RESOURCE_DIRECTORY_ENTRY found_lang =
  search_entry(root, name_dir,
  (const void *)(uintptr_t)language);
  if (!found_lang) {
  if (name_dir->NumberOfIdEntries == 0) {
  return NULL;
  }
  found_lang = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(name_dir + 1);
  }

  return (PIMAGE_RESOURCE_DATA_ENTRY)
  (ctx->image_base + dir->VirtualAddress +
  (found_lang->OffsetToData & 0x7fffffff));
}

wraith_status_t wraith_find_resource(wraith_handle_t h, const void *name, const void *type,
  uint16_t language, wraith_resource_t *out)
{
  if (!out) {
  return WRAITH_E_NULL_ARG;
  }
  *out = NULL;
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  PIMAGE_RESOURCE_DATA_ENTRY entry =
  wr_resource_find_entry(ctx, name, type, language);
  if (!entry) {
  return WRAITH_E_RES_NOT_FOUND;
  }
  *out = (wraith_resource_t)entry;
  return WRAITH_OK;
}
