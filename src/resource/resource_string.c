/*
 * src/resource/resource_string.c
 *
 * wraith_load_string - looks up an ID in RT_STRING and copies (up to
 * `buf_chars`) wide characters into `out_buffer`.
 */

#include "wraith/wraith_resource.h"
#include "resource/resource_internal.h"

#include <wchar.h>
#include <windows.h>

wraith_status_t wraith_load_string(wraith_handle_t h, uint32_t id,
  uint16_t language,
  wchar_t *out_buffer, size_t buf_chars,
  size_t *out_chars)
{
  if (out_chars) {
  *out_chars = 0;
  }
  if (buf_chars == 0 || !out_buffer) {
  return WRAITH_E_BUFFER_TOO_SMALL;
  }
  out_buffer[0] = 0;

  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }

  PIMAGE_RESOURCE_DATA_ENTRY block =
  wr_resource_find_entry(ctx, MAKEINTRESOURCEA((id >> 4) + 1),
  (const void *)(uintptr_t)6 /* RT_STRING */,
  language);
  if (!block) {
  return WRAITH_E_RES_NOT_FOUND;
  }

  PIMAGE_RESOURCE_DIR_STRING_U entry =
  (PIMAGE_RESOURCE_DIR_STRING_U)(ctx->image_base + block->OffsetToData);

  /* Skip to the entry within the bundle of 16. */
  uint32_t skip = id & 0x0f;
  while (skip--) {
  size_t step = (entry->Length + 1) * sizeof(wchar_t);
  entry = (PIMAGE_RESOURCE_DIR_STRING_U)((uint8_t *)entry + step);
  }
  if (entry->Length == 0) {
  return WRAITH_E_RES_NAME_NOT_FOUND;
  }

  size_t copy = entry->Length;
  if (copy >= buf_chars) {
  copy = buf_chars - 1;
  }
  memcpy(out_buffer, entry->NameString, copy * sizeof(wchar_t));
  out_buffer[copy] = 0;

  if (out_chars) {
  *out_chars = copy;
  }
  return WRAITH_OK;
}
