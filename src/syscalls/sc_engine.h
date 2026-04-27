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

#include "wraith/wraith_status.h"
#include "syscalls/sc_table.h"

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
