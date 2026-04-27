/*
 * src/loader/loader_api.c
 *
 * Public v2 entry points: wraith_load_library, wraith_get_proc_address,
 * wraith_free_library, wraith_call_entry_point. These are thin glue between the
 * caller and the internal pipeline / lookup helpers.
 */

#include "core/wr_context_internal.h"
#include "exports/export_lookup.h"
#include "loader/loader_pipeline.h"
#include "wraith/wraith_loader.h"

#include <windows.h>

wraith_status_t wraith_load_library(const void *buffer, size_t size,
  const wraith_load_options *options,
  wraith_handle_t *out)
{
  if (!out) {
  return WRAITH_E_NULL_ARG;
  }
  *out = NULL;

  if (!buffer || size == 0) {
  return WRAITH_E_NULL_ARG;
  }

  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_create(options, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }

  rc = wr_pipeline_run(buffer, size, ctx);
  if (rc != WRAITH_OK) {
  wr_pipeline_unwind(ctx);
  wr_ctx_destroy(ctx);
  return rc;
  }

  *out = (wraith_handle_t)ctx;
  return WRAITH_OK;
}

wraith_status_t wraith_get_proc_address(wraith_handle_t h, const char *name,
  void **out_proc)
{
  if (!out_proc) {
  return WRAITH_E_NULL_ARG;
  }
  *out_proc = NULL;

  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  return wr_export_resolve(ctx, name, out_proc);
}

wraith_status_t wraith_call_entry_point(wraith_handle_t h, int *out_exit_code)
{
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (!ctx->exe_entry || ctx->image_type != WRAITH_IMAGE_EXE ||
  !ctx->is_relocated) {
  return WRAITH_E_INVALID_HANDLE;
  }

  typedef int (WINAPI *exe_entry_fn)(void);
  int code = ((exe_entry_fn)ctx->exe_entry)();
  if (out_exit_code) {
  *out_exit_code = code;
  }
  ctx->entry_called = 1;
  return WRAITH_OK;
}

wraith_status_t wraith_free_library(wraith_handle_t h)
{
  if (!h) {
  return WRAITH_OK;
  }
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  wr_pipeline_unwind(ctx);
  wr_ctx_destroy(ctx);
  return WRAITH_OK;
}

/* -------------------------------------------------------------------------
 * Introspection
 * ------------------------------------------------------------------------- */

wraith_status_t wraith_get_image_base(wraith_handle_t h, void **out_base, size_t *out_size)
{
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (out_base) *out_base = ctx->image_base;
  if (out_size) *out_size = ctx->image_size;
  return WRAITH_OK;
}
