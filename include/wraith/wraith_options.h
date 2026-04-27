/*
 * include/wraith/wraith_options.h
 *
 * wraith_load_options - per-load configuration. Passed to wraith_load_library.
 * Designated-initializer friendly: callers write only the fields they
 * care about; defaults are documented field-by-field.
 */

#ifndef WRAITH_OPTIONS_H
#define WRAITH_OPTIONS_H

#include "wraith_status.h"
#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Custom callback signatures.
 *
 * void* / size_t / uint32_t are used at the public surface to avoid
 * pulling in <windows.h> here.
 * ------------------------------------------------------------------------- */
typedef void *(*wraith_alloc_fn)(void *address, size_t size,
  uint32_t alloc_type, uint32_t protect,
  void *userdata);
typedef int  (*wraith_free_fn)(void *address, size_t size,
  uint32_t free_type, void *userdata);
typedef wraith_foreign_module_t (*WRAITH_LoadLibFn)(const char *name, void *userdata);
typedef void *(*WRAITH_GetProcFn)(wraith_foreign_module_t m, const char *name,
  void *userdata);
typedef void  (*WRAITH_FreeLibFn)(wraith_foreign_module_t m, void *userdata);

/* -------------------------------------------------------------------------
 * Telemetry hook - called once at the end of each pipeline phase. Useful
 * for the curriculum-side tooling (ioc_audit.py reads logged events).
 *
 * `phase_id` matches the 17-step pipeline in doc/ARCHITECTURE.md
 * (1=validate, 2=alloc_ctx, ..., 17=return).
 * ------------------------------------------------------------------------- */
typedef void (*wraith_trace_fn)(int phase_id, const char *phase_name,
  wraith_status_t status, void *userdata);

/* -------------------------------------------------------------------------
 * wraith_load_options
 *
 * Defaults (zero-initialized struct = "behavior"):
 *  - map_strategy = WRAITH_MAP_PRIVATE_RW_RX
 *  - flags  = WRAITH_F_RELIABILITY_ALL  (compile-time gates apply)
 *  - sleep_algo  = WRAITH_SLEEP_EKKO
 *  - sleep_jitter_ms = 0 (no sleep obfuscation unless flag set)
 *  - masquerade  = NULL  (use original PE name when PEB linkage on)
 *  - host_dll  = NULL  (auto-pick from curated list when phantom on)
 *  - all callbacks = NULL (use built-in defaults)
 * ------------------------------------------------------------------------- */
typedef struct wr_load_options {
  /* --- core --- */
  wraith_map_strategy_t map_strategy;
  wraith_flags_t  flags;

  /* --- sleep obfuscation (consulted iff WRAITH_F_SLEEP_OBFUSCATION) --- */
  wraith_sleep_algo_t  sleep_algo;
  uint32_t  sleep_jitter_ms;  /* 0 = no jitter */

  /* --- PEB linkage (consulted iff WRAITH_F_PEB_LINKAGE) --- */
  const wchar_t  *masquerade;  /* e.g. L"winnet.dll"  */
  const wchar_t  *masquerade_path;  /* full Windows path; NULL = synthesize */

  /* --- phantom hollowing (consulted iff map_strategy == PHANTOM_HOLLOW) --- */
  const wchar_t  *host_dll;  /* full path; NULL = curated picker */

  /* --- module stomping (consulted iff map_strategy == MODULE_STOMPING) --- */
  const wchar_t  *stomp_target;  /* full path; NULL = picker */

  /* --- consumer-supplied callbacks (override built-in defaults) --- */
  wraith_alloc_fn  alloc;
  wraith_free_fn  freefn;
  WRAITH_LoadLibFn  loadlib;
  WRAITH_GetProcFn  getproc;
  WRAITH_FreeLibFn  freelib;
  void  *userdata;

  /* --- diagnostics --- */
  wraith_trace_fn  trace;  /* fired per-phase; NULL = silent */
  void  *trace_userdata;

  /* Reserved padding so future fields don't break ABI for early adopters. */
  uintptr_t  _reserved[8];
} wraith_load_options;

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_OPTIONS_H */
