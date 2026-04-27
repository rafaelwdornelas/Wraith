/*
 * wraith.h - amalgamated single-file build of Wraith 1.0.0
 *               profile: default
 *               generated: 2026-04-27
 *
 * This is a self-contained drop-in build of the Wraith stealth PE
 * loader. It is functionally equivalent to compiling the canonical
 * CMake source tree with -DWRAITH_PROFILE=default, but condensed
 * into two files (.c + .h) for fast integration.
 *
 * To regenerate: from the project root, run
 *     python3 tools/amalgamate.py default
 *
 * License: MIT - see the LICENSE file shipped beside this header.
 */

#ifndef WRAITH_AMALGAMATED_H
#define WRAITH_AMALGAMATED_H

#ifdef __cplusplus
extern "C" {
#endif


/* ==========================================================================
 * public surface (concatenated headers)
 * ========================================================================== */


/* ==========================================================================
 * include/wraith/wraith_types.h
 * ========================================================================== */

/*
 * include/wraith/wraith_types.h
 *
 * Opaque handles, primitive types, and enums for the Wraith public
 * API. This header is C99-clean and intentionally avoids dragging in
 * <windows.h> at the public surface - consumers that only need to hold a
 * handle and call wraith_load_library should not be forced to include the SDK.
 *
 * Anything that needs Windows types lives in wr_compat.h or wr_resource.h.
 */

#ifndef WRAITH_TYPES_H
#define WRAITH_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Opaque handles.
 *
 * wraith_handle_t is a context produced by wraith_load_library and consumed
 * by every WRAITH_* call. It is intentionally a typed opaque pointer (not
 * void*) so the compiler refuses to mix it with HMODULE / HANDLE / void*
 * by accident.
 *
 * wraith_resource_t is an opaque handle into the loaded image's `.rsrc`
 * directory.
 * wraith_foreign_module_t is a handle to a *dependency* module (resolved
 * during import processing).
 * ------------------------------------------------------------------------- */
typedef struct wr_ctx  *wraith_handle_t;
typedef struct wr_resource  *wraith_resource_t;
typedef struct wr_foreign  *wraith_foreign_module_t;

/* -------------------------------------------------------------------------
 * Memory protection flags as understood by mapping vtables.
 *
 * These are an internal, stable subset of PAGE_* / MEM_* values - by going
 * through this enum the loader avoids leaking <windows.h> macros into the
 * public surface and also enforces RW->RX hygiene at the type level: there
 * is no WRAITH_PROT_RWX value, only the legitimate combinations.
 * ------------------------------------------------------------------------- */
typedef enum wr_prot {
  WRAITH_PROT_NOACCESS  = 0x00,
  WRAITH_PROT_R  = 0x01,  /* read-only data  */
  WRAITH_PROT_RW  = 0x02,  /* writable data  */
  WRAITH_PROT_RX  = 0x04,  /* executable code  */
  WRAITH_PROT_WC  = 0x08,  /* writecopy (CoW)  */
  WRAITH_PROT_RWC  = 0x10,  /* read + writecopy  */
  WRAITH_PROT_RXC  = 0x20,  /* RX + writecopy  */
  WRAITH_PROT_NOCACHE  = 0x100, /* OR'd with one of above  */
  WRAITH_PROT_GUARD  = 0x200  /* OR'd with one of above  */
} wraith_prot_t;

/* -------------------------------------------------------------------------
 * Image type produced by the loader.
 * ------------------------------------------------------------------------- */
typedef enum wr_image_type {
  WRAITH_IMAGE_UNKNOWN = 0,
  WRAITH_IMAGE_DLL  = 1,
  WRAITH_IMAGE_EXE  = 2
} wraith_image_type_t;

/* -------------------------------------------------------------------------
 * Mapping strategy. See doc/TECHNIQUES.md for an in-depth comparison.
 *
 *  PRIVATE_RW_RX  - default. NtAllocateVirtualMemory + RW->RX flip.
 *  Stable, but produces an unbacked MEM_PRIVATE region.
 *  PHANTOM_HOLLOW  - NtCreateSection(SEC_IMAGE) backed by a legitimate
 *  Microsoft DLL. The region presents as MEM_IMAGE.
 *  MODULE_STOMPING  - overlay payload onto an already-loaded legitimate
 *  module. Lab-only - destroys the host module's .text.
 *  MOCKINGJAY  - hunt for pre-existing MEM_IMAGE+RWX regions in the
 *  process and reuse them. Zero new allocations.
 * ------------------------------------------------------------------------- */
typedef enum wr_map_strategy {
  WRAITH_MAP_PRIVATE_RW_RX  = 0,
  WRAITH_MAP_PHANTOM_HOLLOW  = 1,
  WRAITH_MAP_MODULE_STOMPING  = 2,
  WRAITH_MAP_MOCKINGJAY  = 3
} wraith_map_strategy_t;

/* -------------------------------------------------------------------------
 * Sleep obfuscation algorithm (only consulted when WRAITH_F_SLEEP_OBFUSCATION
 * is set in wraith_load_options.flags).
 *
 *  XOR  - simple XOR-on-thread baseline (didactic)
 *  EKKO  - timer queue + CONTEXT-driven encrypt/wait/decrypt (default)
 *  FOLIAGE - NtContinue-based, single-shot
 *  CRONOS  - APC chain + waitable timer; RIP stays kernel-side during idle
 * ------------------------------------------------------------------------- */
typedef enum wr_sleep_algo {
  WRAITH_SLEEP_XOR  = 0,
  WRAITH_SLEEP_EKKO  = 1,
  WRAITH_SLEEP_FOLIAGE = 2,
  WRAITH_SLEEP_CRONOS  = 3
} wraith_sleep_algo_t;

/* -------------------------------------------------------------------------
 * Bitwise feature flags for wraith_load_options.flags. Independent from the
 * compile-time WRAITH_USE_* macros - if a feature was compiled-out at build
 * time, requesting it at runtime returns WRAITH_E_FEATURE_DISABLED.
 * ------------------------------------------------------------------------- */
typedef uint64_t wraith_flags_t;

#define WRAITH_F_NONE  ((wraith_flags_t)0)
/* Classic stealth */
#define WRAITH_F_API_HASHING  ((wraith_flags_t)1ULL <<  0)
#define WRAITH_F_PEB_WALK  ((wraith_flags_t)1ULL <<  1)
#define WRAITH_F_INDIRECT_SYSCALLS  ((wraith_flags_t)1ULL <<  2)
#define WRAITH_F_PEB_LINKAGE  ((wraith_flags_t)1ULL <<  3)
#define WRAITH_F_SLEEP_OBFUSCATION  ((wraith_flags_t)1ULL <<  4)
#define WRAITH_F_UNHOOK_NTDLL  ((wraith_flags_t)1ULL <<  5)
#define WRAITH_F_ETW_PATCH  ((wraith_flags_t)1ULL <<  6)
#define WRAITH_F_AMSI_PATCH  ((wraith_flags_t)1ULL <<  7)
/* Bleeding-edge tier */
#define WRAITH_F_STACK_SPOOF  ((wraith_flags_t)1ULL <<  8)
#define WRAITH_F_HWBP_HOOKS  ((wraith_flags_t)1ULL <<  9)
#define WRAITH_F_PRIVATE_NTDLL  ((wraith_flags_t)1ULL << 10)
#define WRAITH_F_THREADLESS_EXEC  ((wraith_flags_t)1ULL << 11)
#define WRAITH_F_PAGE_GUARD_ENCRYPT  ((wraith_flags_t)1ULL << 12)
#define WRAITH_F_HEAP_MASQUERADE  ((wraith_flags_t)1ULL << 13)
#define WRAITH_F_ANTI_DEBUG_SPOOF  ((wraith_flags_t)1ULL << 14)
#define WRAITH_F_HOST_IAT_REDIRECT  ((wraith_flags_t)1ULL << 15)
/* Reliability toggles (rarely cleared) */
#define WRAITH_F_REGISTER_SEH_X64  ((wraith_flags_t)1ULL << 32)
#define WRAITH_F_FORWARDED_EXPORTS  ((wraith_flags_t)1ULL << 33)
#define WRAITH_F_DELAY_LOAD_IMPORTS  ((wraith_flags_t)1ULL << 34)
#define WRAITH_F_BOUND_IMPORTS  ((wraith_flags_t)1ULL << 35)
#define WRAITH_F_TLS_FULL_LIFECYCLE  ((wraith_flags_t)1ULL << 36)

/* Convenience presets (not stored - resolved by the loader at WRAITH_Load). */
#define WRAITH_F_RELIABILITY_ALL  (WRAITH_F_REGISTER_SEH_X64 | \
  WRAITH_F_FORWARDED_EXPORTS | \
  WRAITH_F_DELAY_LOAD_IMPORTS | \
  WRAITH_F_BOUND_IMPORTS | \
  WRAITH_F_TLS_FULL_LIFECYCLE)

#define WRAITH_F_PARANOID_FULL  (WRAITH_F_API_HASHING | \
  WRAITH_F_PEB_WALK | \
  WRAITH_F_INDIRECT_SYSCALLS | \
  WRAITH_F_PEB_LINKAGE | \
  WRAITH_F_SLEEP_OBFUSCATION | \
  WRAITH_F_ETW_PATCH | \
  WRAITH_F_AMSI_PATCH | \
  WRAITH_F_STACK_SPOOF | \
  WRAITH_F_HWBP_HOOKS | \
  WRAITH_F_PRIVATE_NTDLL | \
  WRAITH_F_THREADLESS_EXEC | \
  WRAITH_F_PAGE_GUARD_ENCRYPT | \
  WRAITH_F_HEAP_MASQUERADE | \
  WRAITH_F_ANTI_DEBUG_SPOOF | \
  WRAITH_F_RELIABILITY_ALL)

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* WRAITH_TYPES_H */


/* ==========================================================================
 * include/wraith/wraith_status.h
 * ========================================================================== */

/*
 * include/wraith/wraith_status.h
 *
 * Rich error codes returned by every WRAITH_* entry point.
 *
 * Each error code maps deterministically to a category. Consumers can
 * switch on the category to decide retry vs. propagate without
 * enumerating every leaf code.
 */

#ifndef WRAITH_STATUS_H
#define WRAITH_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t wraith_status_t;

/* Top-level categories - the upper byte of wraith_status_t encodes the category. */
#define WRAITH_CAT(s)  (((s) >> 24) & 0xff)

#define WRAITH_CAT_OK  0x00
#define WRAITH_CAT_INVALID_ARG  0x01
#define WRAITH_CAT_PE_FORMAT  0x02
#define WRAITH_CAT_RESOURCE  0x03
#define WRAITH_CAT_MAPPING  0x04
#define WRAITH_CAT_RELOCATIONS  0x05
#define WRAITH_CAT_IMPORTS  0x06
#define WRAITH_CAT_EXPORTS  0x07
#define WRAITH_CAT_TLS  0x08
#define WRAITH_CAT_SEH  0x09
#define WRAITH_CAT_RUNTIME  0x0a
#define WRAITH_CAT_SYSCALL  0x0b
#define WRAITH_CAT_STEALTH  0x0c
#define WRAITH_CAT_FEATURE  0x0d
#define WRAITH_CAT_INTERNAL  0x0e

#define WRAITH_MAKE_STATUS(cat, sub)  (((wraith_status_t)(cat) << 24) | (wraith_status_t)(sub))

/* ============================ WRAITH_CAT_OK ============================ */
#define WRAITH_OK  WRAITH_MAKE_STATUS(WRAITH_CAT_OK,  0x00)

/* ============================ INVALID_ARG ============================ */
#define WRAITH_E_NULL_ARG  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x01)
#define WRAITH_E_INVALID_HANDLE  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x02)
#define WRAITH_E_INVALID_OPTIONS  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x03)
#define WRAITH_E_BUFFER_TOO_SMALL  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x04)

/* =============================== PE_FORMAT =========================== */
#define WRAITH_E_PE_TRUNCATED  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x01)
#define WRAITH_E_PE_BAD_DOS_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x02)
#define WRAITH_E_PE_BAD_NT_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x03)
#define WRAITH_E_PE_WRONG_MACHINE  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x04)
#define WRAITH_E_PE_BAD_OPT_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x05)
#define WRAITH_E_PE_BAD_ALIGNMENT  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x06)
#define WRAITH_E_PE_BAD_SECTION  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x07)
#define WRAITH_E_PE_SIZE_MISMATCH  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x08)
#define WRAITH_E_PE_OVERFLOW  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x09)

/* =============================== RESOURCE ============================ */
#define WRAITH_E_RES_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x01)
#define WRAITH_E_RES_TYPE_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x02)
#define WRAITH_E_RES_NAME_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x03)
#define WRAITH_E_RES_LANG_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x04)

/* =============================== MAPPING ============================== */
#define WRAITH_E_MAP_RESERVE_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x01)
#define WRAITH_E_MAP_COMMIT_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x02)
#define WRAITH_E_MAP_PROTECT_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x03)
#define WRAITH_E_MAP_NO_HOST_DLL  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x04)
#define WRAITH_E_MAP_HOST_TOO_SMALL  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x05)
#define WRAITH_E_MAP_RWX_LEAK  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x06)

/* ============================= RELOCATIONS ============================ */
#define WRAITH_E_RELOC_NOT_RELOCATABLE  WRAITH_MAKE_STATUS(WRAITH_CAT_RELOCATIONS, 0x01)
#define WRAITH_E_RELOC_BAD_TYPE  WRAITH_MAKE_STATUS(WRAITH_CAT_RELOCATIONS, 0x02)

/* =============================== IMPORTS ============================== */
#define WRAITH_E_IMP_DLL_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x01)
#define WRAITH_E_IMP_PROC_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x02)
#define WRAITH_E_IMP_FORWARDER_LOOP  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x03)
#define WRAITH_E_IMP_DELAY_BAD_DESCR  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x04)

/* =============================== EXPORTS ============================== */
#define WRAITH_E_EXP_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x01)
#define WRAITH_E_EXP_BAD_ORDINAL  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x02)
#define WRAITH_E_EXP_NO_TABLE  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x03)

/* ================================ TLS ================================= */
#define WRAITH_E_TLS_CALLBACK_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_TLS,  0x01)

/* ================================ SEH ================================= */
#define WRAITH_E_SEH_NO_PDATA  WRAITH_MAKE_STATUS(WRAITH_CAT_SEH,  0x01)
#define WRAITH_E_SEH_REGISTER_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_SEH,  0x02)

/* =============================== RUNTIME ============================== */
#define WRAITH_E_RT_PEB_WALK_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x01)
#define WRAITH_E_RT_API_NOT_RESOLVED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x02)
#define WRAITH_E_RT_DLLMAIN_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x03)

/* =============================== SYSCALL ============================== */
#define WRAITH_E_SC_SSN_NOT_RESOLVED  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x01)
#define WRAITH_E_SC_NO_GADGET  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x02)
#define WRAITH_E_SC_INVOKE_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x03)

/* =============================== STEALTH ============================== */
#define WRAITH_E_STEALTH_INSTALL  WRAITH_MAKE_STATUS(WRAITH_CAT_STEALTH,  0x01)
#define WRAITH_E_STEALTH_INCOMPATIBLE  WRAITH_MAKE_STATUS(WRAITH_CAT_STEALTH,  0x02)

/* =============================== FEATURE ============================== */
#define WRAITH_E_FEATURE_DISABLED  WRAITH_MAKE_STATUS(WRAITH_CAT_FEATURE,  0x01)

/* =============================== INTERNAL ============================= */
#define WRAITH_E_OOM  WRAITH_MAKE_STATUS(WRAITH_CAT_INTERNAL,  0x01)
#define WRAITH_E_UNEXPECTED  WRAITH_MAKE_STATUS(WRAITH_CAT_INTERNAL,  0x02)

/* ----------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Returns a static, NUL-terminated ASCII string for the given status. */
const char *wraith_status_string(wraith_status_t s);

/* Returns a human-readable label for the category byte. */
const char *wraith_category_string(int category);

#define WRAITH_SUCCESS(s)  ((s) == WRAITH_OK)
#define WRAITH_FAILED(s)  ((s) != WRAITH_OK)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_STATUS_H */


/* ==========================================================================
 * include/wraith/wraith_options.h
 * ========================================================================== */

/*
 * include/wraith/wraith_options.h
 *
 * wraith_load_options - per-load configuration. Passed to wraith_load_library.
 * Designated-initializer friendly: callers write only the fields they
 * care about; defaults are documented field-by-field.
 */

#ifndef WRAITH_OPTIONS_H
#define WRAITH_OPTIONS_H


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


/* ==========================================================================
 * include/wraith/wraith_introspect.h
 * ========================================================================== */

/*
 * include/wraith/wraith_introspect.h
 *
 * Optional debug/introspection surface. Useful for tooling
 * (e.g. tools/ioc_audit.py) and tests that need to peek into the loaded
 * image without re-parsing the PE.
 *
 * Compiled out when WRAITH_DEBUG_LOG is OFF and WRAITH_BUILD_TESTS is OFF.
 */

#ifndef WRAITH_INTROSPECT_H
#define WRAITH_INTROSPECT_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_section_info {
  char  name[9];  /* IMAGE_SIZEOF_SHORT_NAME + NUL */
  void  *base;  /* virtual address inside loaded image */
  size_t  size;  /* page-aligned size */
  uint32_t  characteristics;  /* IMAGE_SCN_* bits */
  wraith_prot_t  final_prot;  /* protection after FinalizeSections */
} wr_section_info;

typedef struct wr_export_info {
  const char *name;  /* may be NULL for ordinal-only exports */
  uint16_t  ordinal;
  void  *address;
  int  is_forwarded;  /* 1 if export is "DLL!Func" string */
  const char *forward_target;  /* valid when is_forwarded == 1 */
} wr_export_info;

typedef struct wr_import_info {
  const char *dll_name;
  const char *symbol_name;  /* NULL for ordinal-by-number imports */
  uint16_t  ordinal;
  void  *resolved_address;
} wr_import_info;

/* -------------------------------------------------------------------------
 * Iteration. The callback returns 0 to continue, non-zero to stop.
 * ------------------------------------------------------------------------- */

typedef int (*wr_section_visitor)(const wr_section_info *info, void *userdata);
typedef int (*wr_export_visitor)(const wr_export_info *info, void *userdata);
typedef int (*wr_import_visitor)(const wr_import_info *info, void *userdata);

wraith_status_t wraith_for_each_section(wraith_handle_t h, wr_section_visitor cb, void *userdata);
wraith_status_t wraith_for_each_export(wraith_handle_t  h, wr_export_visitor  cb, void *userdata);
wraith_status_t wraith_for_each_import(wraith_handle_t  h, wr_import_visitor  cb, void *userdata);

/* -------------------------------------------------------------------------
 * Lookup the loaded image base address (for callers that want to compute
 * RVAs themselves, e.g. tests that grep for RWX regions).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_get_image_base(wraith_handle_t h, void **out_base, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_INTROSPECT_H */


/* ==========================================================================
 * include/wraith/wraith_loader.h
 * ========================================================================== */

/*
 * include/wraith/wraith_loader.h
 *
 * Primary entry points. All return wraith_status_t; the produced handle (if
 * any) is written to an out-parameter. Pointers are typed and never aliased
 * with HMODULE / HANDLE / void* in the public surface.
 */

#ifndef WRAITH_LOADER_H
#define WRAITH_LOADER_H



#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Load a DLL or EXE from an in-memory PE buffer.
 *
 * `buffer`  : pointer to the start of the PE image (DOS header).
 * `size`  : length of `buffer` in bytes.
 * `options` : may be NULL - equivalent to passing a zero-initialized struct
 *  (no stealth features active).
 * `out`  : on WRAITH_OK, receives a non-NULL wraith_handle_t. On failure,
 *  *out is set to NULL.
 *
 * On error the error code categorizes which pipeline phase failed; callers
 * can look at wraith_last_error() for a free-form description.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_library(const void *buffer, size_t size,
  const wraith_load_options *options,
  wraith_handle_t *out);

/* -------------------------------------------------------------------------
 * Resolve an export by name or ordinal.
 *
 * Pass the ordinal value cast to (const char*) to look up by ordinal.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_get_proc_address(wraith_handle_t h, const char *name,
  void **out_proc);

/* -------------------------------------------------------------------------
 * Run an EXE entry point. Returns WRAITH_E_INVALID_HANDLE for DLL handles or
 * if the image had no entry point.
 *
 * The loaded EXE owns the process from this point. `out_exit_code` is
 * written only if the loaded EXE returns normally (rare for full EXEs).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_call_entry_point(wraith_handle_t h, int *out_exit_code);

/* -------------------------------------------------------------------------
 * Release the loaded image. After this call the handle is invalid.
 *
 * For DLL handles: invokes DLL_PROCESS_DETACH, runs TLS DETACH callbacks
 * (when WRAITH_F_TLS_FULL_LIFECYCLE was set), unwinds RtlAddFunctionTable,
 * removes PEB.Ldr entry (if WRAITH_F_PEB_LINKAGE), restores the host's .text
 * (if MODULE_STOMPING), and finally releases all tracked allocations.
 *
 * Calling this with a NULL handle is a no-op and returns WRAITH_OK.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_free_library(wraith_handle_t h);

/* -------------------------------------------------------------------------
 * Diagnostic helpers - always available, even when WRAITH_DEBUG_LOG is off.
 * ------------------------------------------------------------------------- */

/* Returns the most recent free-form error description for the current
 * thread. The string is owned by the loader and is overwritten by the next
 * WRAITH_* call on the same thread. May be empty. */
const char *wraith_last_error(void);

/* Returns the version string ("Wraith <profile> <semver>"). */
const char *wraith_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_LOADER_H */


/* ==========================================================================
 * include/wraith/wraith_resource.h
 * ========================================================================== */

/*
 * include/wraith/wraith_resource.h
 *
 * Resource (.rsrc) walking API. Returns wraith_status_t for every entry
 * point (no NULL-handle convention).
 *
 * This header includes <windows.h> indirectly via LPCTSTR, because the
 * resource API has to interop with RT_*, MAKEINTRESOURCE, etc.
 * Consumers that want to avoid <windows.h> can stick to byte-level
 * wraith_find_resource + wraith_load_resource_data.
 */

#ifndef WRAITH_RESOURCE_H
#define WRAITH_RESOURCE_H


#ifdef __cplusplus
extern "C" {
#endif

/* Default language constant - 0 means "any language" / neutral. */
#define WRAITH_LANG_DEFAULT  ((uint16_t)0)

/* -------------------------------------------------------------------------
 * Find a resource entry. `name` and `type` accept either an ASCII string
 * or an integer cast to (const char*) - same idiom as Win32 RT_RCDATA etc.
 *
 * `language` may be WRAITH_LANG_DEFAULT to use the calling thread's locale.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_find_resource(wraith_handle_t h,
  const void *name, const void *type,
  uint16_t language,
  wraith_resource_t *out);

/* -------------------------------------------------------------------------
 * Get the size of a previously-found resource, in bytes.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_sizeof_resource(wraith_handle_t h, wraith_resource_t r,
  size_t *out_size);

/* -------------------------------------------------------------------------
 * Get a pointer to the resource bytes. The pointer aliases into the
 * loaded image; do NOT free or modify it.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_resource_data(wraith_handle_t h, wraith_resource_t r,
  const void **out_data);

/* -------------------------------------------------------------------------
 * Load a string from STRINGTABLE by ID. `out_buffer` is wchar_t (UTF-16
 * little-endian as per Windows convention). `buf_chars` is the buffer
 * size in wchar_t units, and `out_chars` (optional, may be NULL) receives
 * the actual length copied (excluding the terminator).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_string(wraith_handle_t h, uint32_t id,
  uint16_t language,
  wchar_t *out_buffer, size_t buf_chars,
  size_t *out_chars);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RESOURCE_H */


/* ==========================================================================
 * include/wraith/wraith_stealth.h
 * ========================================================================== */

/*
 * include/wraith/wraith_stealth.h
 *
 * Opt-in stealth-control surface. Each entry point is conditionally
 * compiled based on the corresponding WRAITH_USE_* macro - if a feature was
 * compiled out, calling the corresponding API returns
 * WRAITH_E_FEATURE_DISABLED.
 *
 * The intent is that consumers with `paranoid-full` builds can drive the
 * stealth state machine explicitly; the default profile keeps these
 * symbols out of the binary entirely.
 */

#ifndef WRAITH_STEALTH_H
#define WRAITH_STEALTH_H


#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Sleep obfuscation - request that the loaded image be encrypted in place
 * and the calling thread blocked for `duration_ms`. When WRAITH_F_THREADLESS_EXEC
 * is also set, the awakener runs from a hijacked thread-pool callback.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_sleep(wraith_handle_t h, uint32_t duration_ms);

/* -------------------------------------------------------------------------
 * Trigger a one-shot "encrypt now / decrypt on demand" cycle for the
 * Page-Guard tier . Useful when the consumer wants to mark idle
 * windows shorter than a regular sleep would justify.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_pageguard_arm(wraith_handle_t h);
wraith_status_t wraith_pageguard_disarm(wraith_handle_t h);

/* -------------------------------------------------------------------------
 * Hardware-breakpoint hooks . Install/remove a DR-based redirect from
 * `target_fn` to `replacement_fn` for the calling thread only.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_hwbp_install(void *target_fn, void *replacement_fn,
  int dr_index /* 0..3, -1 = auto */);
wraith_status_t wraith_hwbp_remove(int dr_index);

/* -------------------------------------------------------------------------
 * Stack spoofing - returns WRAITH_OK if the synthetic-frame engine
 * has a curated gadget table for the current OS build, otherwise
 * WRAITH_E_STEALTH_INCOMPATIBLE (caller should disable WRAITH_F_STACK_SPOOF).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_stackspoof_probe(void);

/* -------------------------------------------------------------------------
 * Userland unhooking - either via legacy disk-refresh (WRAITH_USE_UNHOOK_NTDLL)
 * or via Private ntdll (WRAITH_USE_PRIVATE_NTDLL). The flag set on
 * wraith_load_options picks which mechanism is used.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_unhook_ntdll(void);

/* -------------------------------------------------------------------------
 * ETW / AMSI patches. Idempotent. Not undone by wraith_free_library - they
 * affect the host process for its remaining lifetime.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_patch_etw(void);
wraith_status_t wraith_patch_amsi(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_STEALTH_H */


#ifdef __cplusplus
}
#endif

#endif /* WRAITH_AMALGAMATED_H */
