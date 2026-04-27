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
