/*
 * src/loader/loader_pipeline.h
 *
 * Internal contract between the orchestrator (`loader_pipeline.c`) and
 * each phase module. Every phase function receives the current `wr_ctx`
 * (already validated) and the parsed PE view. They mutate ctx; on error
 * they update last_status / err_context via wr_ctx_fail and return.
 */

#ifndef WRAITH_LOADER_PIPELINE_H
#define WRAITH_LOADER_PIPELINE_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"
#include "pe/pe_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* section copy + initial RW commit. */
wraith_status_t wr_load_sections_copy(struct wr_ctx *ctx,
  const wr_pe_view *src);

/* x64 base relocations (DIR64). */
wraith_status_t wr_load_relocs_apply(struct wr_ctx *ctx,
  const wr_pe_view *src);

/* a: bound imports (currently a documented no-op - see
 * loader_imports_bound.c for the policy rationale). */
wraith_status_t wr_load_imports_bound_check(struct wr_ctx *ctx);

/* b: normal import descriptor walk. */
wraith_status_t wr_load_imports_resolve(struct wr_ctx *ctx,
  const wr_pe_view *src);

/* c: delay-load imports (eagerly resolved, IAT patched in place). */
wraith_status_t wr_load_imports_delay(struct wr_ctx *ctx);

/* final per-section VirtualProtect (RW -> RX/R/RW), strict
 * RW->RX hygiene enforced by mapping vtable. */
wraith_status_t wr_load_finalize_sections(struct wr_ctx *ctx,
  const wr_pe_view *src);

/* x64 SEH registration via RtlAddFunctionTable. Counterpart
 * is invoked from wr_pipeline_unwind. */
wraith_status_t wr_load_register_seh_x64(struct wr_ctx *ctx);
void  wr_load_unregister_seh_x64(struct wr_ctx *ctx);

/* TLS callbacks (DLL_PROCESS_ATTACH). */
wraith_status_t wr_load_run_tls_attach(struct wr_ctx *ctx,
  const wr_pe_view *src);
/* Counterpart used by wraith_free_library. */
void  wr_load_run_tls_detach(struct wr_ctx *ctx);

/* DllMain (DLL) or save EXE entry point. */
wraith_status_t wr_load_run_entry(struct wr_ctx *ctx);
void  wr_load_run_entry_detach(struct wr_ctx *ctx);

/* Top-level orchestrator. Implemented by loader_pipeline.c. */
wraith_status_t wr_pipeline_run(const void *buffer, size_t size,
  struct wr_ctx *ctx);

/* Free everything the pipeline allocated/registered (idempotent). */
void wr_pipeline_unwind(struct wr_ctx *ctx);

/* Convenience: emit a trace event when ctx->trace is non-NULL. */
void wr_trace(struct wr_ctx *ctx, int phase_id, const char *phase_name,
  wraith_status_t status);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_LOADER_PIPELINE_H */
