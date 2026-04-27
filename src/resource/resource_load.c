/*
 * src/resource/resource_load.c
 *
 * wraith_sizeof_resource + wraith_load_resource_data. Trivial readers off the
 * resource entry produced by wraith_find_resource.
 */

#include "wraith/wraith_resource.h"
#include "resource/resource_internal.h"

#include <windows.h>

wraith_status_t wraith_sizeof_resource(wraith_handle_t h, wraith_resource_t r,
  size_t *out_size)
{
  if (!out_size) {
  return WRAITH_E_NULL_ARG;
  }
  *out_size = 0;
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  PIMAGE_RESOURCE_DATA_ENTRY entry = (PIMAGE_RESOURCE_DATA_ENTRY)r;
  if (!entry) {
  return WRAITH_E_RES_NOT_FOUND;
  }
  *out_size = entry->Size;
  return WRAITH_OK;
}

wraith_status_t wraith_load_resource_data(wraith_handle_t h, wraith_resource_t r,
  const void **out_data)
{
  if (!out_data) {
  return WRAITH_E_NULL_ARG;
  }
  *out_data = NULL;
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  PIMAGE_RESOURCE_DATA_ENTRY entry = (PIMAGE_RESOURCE_DATA_ENTRY)r;
  if (!entry) {
  return WRAITH_E_RES_NOT_FOUND;
  }
  *out_data = ctx->image_base + entry->OffsetToData;
  return WRAITH_OK;
}
