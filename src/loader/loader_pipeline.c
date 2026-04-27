/*
 * src/loader/loader_pipeline.c
 *
 * Top-level orchestrator. The 17-step pipeline from doc/ARCHITECTURE.md
 * is implemented here as a linear sequence of phase calls; on any failure
 * we run wr_pipeline_unwind to reverse what's been done so far.
 */

#include "core/wr_context_internal.h"
#include "loader/loader_pipeline.h"
#include "mapping/map_strategy.h"
#include "wraith/wraith_options.h"
#include "pe/pe_image_metrics.h"
#include "pe/pe_validate.h"
#include "runtime/rt_api.h"

#if WRAITH_USE_PEB_LINKAGE
#include "stealth/peb_link/peb_link.h"
#endif

#include <stdlib.h>
#include <string.h>

#define WRAITH_PIPE_STEP(ctx, id, name, expr) do {  \
  wraith_status_t _rc = (expr);  \
  wr_trace((ctx), (id), (name), _rc);  \
  if (_rc != WRAITH_OK) {  \
  return _rc;  \
  }  \
  } while (0)

void wr_trace(struct wr_ctx *ctx, int phase_id, const char *phase_name,
  wraith_status_t status)
{
  if (ctx && ctx->trace) {
  ctx->trace(phase_id, phase_name, status, ctx->trace_userdata);
  }
}

wraith_status_t wr_pipeline_run(const void *buffer, size_t size,
  struct wr_ctx *ctx)
{
  if (!buffer || size == 0 || !ctx) {
  return WRAITH_E_NULL_ARG;
  }

  /* [1] Validate PE -------------------------------------------------- */
  wr_pe_view view;
  WRAITH_PIPE_STEP(ctx, 1, "validate",
  wr_pe_validate(buffer, size, &view));

  /* [2] Image metrics ----------------------------------------------- */
  wr_pe_image_metrics metrics;
  WRAITH_PIPE_STEP(ctx, 2, "metrics",
  wr_pe_compute_metrics(&view, ctx->page_size, &metrics));

  ctx->image_size = metrics.aligned_image_size;
  ctx->image_type = view.is_dll ? WRAITH_IMAGE_DLL : WRAITH_IMAGE_EXE;

  /* [3] Runtime + [4] mapping strategy resolution ------------------- */
  ctx->rt_ops  = wr_rt_resolve(ctx);
  ctx->map_ops = wr_map_resolve(ctx->map_strategy);
  if (!ctx->map_ops) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_OPTIONS,
  "mapping strategy %d unavailable in this build",
  (int)ctx->map_strategy);
  }
  wr_trace(ctx, 3, "runtime", WRAITH_OK);
  wr_trace(ctx, 4, "map_resolve", WRAITH_OK);

  /* [5] Reserve image ----------------------------------------------- */
  void *base = NULL;
  wraith_status_t reserve_rc =
  ctx->map_ops->reserve(ctx, metrics.aligned_image_size, &base);

#if WRAITH_USE_PHANTOM_HOLLOWING
  /* Runtime auto-degradation: when the user asked for PHANTOM_HOLLOW
   * but the actual reserve failed (kernel veto, EDR hook synthesizing
   * an error, mitigation we didn't probe for), silently retry with
   * private_rwx. The pre-flight probe in wr_map_resolve catches most
   * Chrome / Edge cases up front; this is the safety net for the rest. */
  if (reserve_rc != WRAITH_OK
      && ctx->map_strategy == WRAITH_MAP_PHANTOM_HOLLOW
      && ctx->map_ops != &wr_map_ops_private_rwx) {
  wr_phantom_mark_blocked();
  if (ctx->map_ops->destroy) {
  ctx->map_ops->destroy(ctx);
  }
  ctx->map_ops = &wr_map_ops_private_rwx;
  base = NULL;
  reserve_rc = ctx->map_ops->reserve(ctx,
  metrics.aligned_image_size, &base);
  }
#endif

  WRAITH_PIPE_STEP(ctx, 5, "reserve", reserve_rc);
  ctx->image_base = (uint8_t *)base;

  /* [7] Copy headers + sections ------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 7, "copy_sections",
  wr_load_sections_copy(ctx, &view));

  /* [8] Base relocations -------------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 8, "relocs",
  wr_load_relocs_apply(ctx, &view));

  /* [9a] Bound imports (policy: skip) ------------------------------- */
#if WRAITH_BOUND_IMPORTS
  WRAITH_PIPE_STEP(ctx, 9, "bound_imports",
  wr_load_imports_bound_check(ctx));
#endif

  /* [9b] Normal imports --------------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 9, "imports",
  wr_load_imports_resolve(ctx, &view));

  /* [9c] Delay-load imports ----------------------------------------- */
#if WRAITH_DELAY_LOAD_IMPORTS
  WRAITH_PIPE_STEP(ctx, 9, "delay_imports",
  wr_load_imports_delay(ctx));
#endif

  /* [10] Finalize sections (RW -> RX/R/RW) -------------------------- */
  WRAITH_PIPE_STEP(ctx, 10, "finalize",
  wr_load_finalize_sections(ctx, &view));

  /* [12] x64 SEH registration --------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 12, "seh_x64",
  wr_load_register_seh_x64(ctx));

  /* [14] TLS callbacks --------------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 14, "tls_attach",
  wr_load_run_tls_attach(ctx, &view));

  /* [15] DllMain / EXE entry --------------------------------------- */
  WRAITH_PIPE_STEP(ctx, 15, "entry",
  wr_load_run_entry(ctx));

  /* [13] PEB.Ldr linkage runs *after* DllMain/init. Inserting before
  * init breaks Rust / C++ runtimes that walk PEB.Ldr during their own
  * startup; deferring lets the payload finish initializing on the
  * unmodified list and still be visible to subsequent enumerators
  * (EnumProcessModulesEx, GetModuleHandleW, dump tools). */
#if WRAITH_USE_PEB_LINKAGE
  WRAITH_PIPE_STEP(ctx, 13, "peb_link",
  wr_peb_link_install(ctx));
#endif

  return WRAITH_OK;
}

void wr_pipeline_unwind(struct wr_ctx *ctx)
{
  if (!ctx) {
  return;
  }

  /* Symmetric with the deferred install order: install runs after
  * DllMain ATTACH, so unlink runs before DllMain DETACH. This way
  * the payload's runtime, if it walks PEB.Ldr during its own
  * shutdown, sees the same list it saw at init. */
#if WRAITH_USE_PEB_LINKAGE
  wr_peb_link_remove(ctx);
#endif

  wr_load_run_entry_detach(ctx);
  wr_load_run_tls_detach(ctx);

  /* Unregister SEH unwind tables *before* releasing memory; the
  * function table pointer must still be valid for RtlDeleteFunctionTable. */
  wr_load_unregister_seh_x64(ctx);

  /* Free imported dependencies. */
  if (ctx->imported_modules) {
  for (uint32_t i = 0; i < ctx->imported_count; ++i) {
  if (ctx->imported_owned && ctx->imported_owned[i] &&
  ctx->imported_modules[i]) {
  if (ctx->rt_ops && ctx->rt_ops->free_library) {
  ctx->rt_ops->free_library(ctx, ctx->imported_modules[i]);
  }
  }
  }
  free(ctx->imported_modules);
  free(ctx->imported_owned);
  ctx->imported_modules = NULL;
  ctx->imported_owned  = NULL;
  ctx->imported_count  = 0;
  }

  /* Release mapped image. */
  if (ctx->map_ops) {
  ctx->map_ops->release(ctx);
  if (ctx->map_ops->destroy) {
  ctx->map_ops->destroy(ctx);
  }
  }
  ctx->image_base = NULL;
  ctx->image_size = 0;

  /* Cached export table. */
  free(ctx->export_table);
  ctx->export_table = NULL;
}
