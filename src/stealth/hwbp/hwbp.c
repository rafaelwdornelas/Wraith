/*
 * src/stealth/hwbp/hwbp.c
 *
 * Implementation. The DR setup is per-thread (DR0..DR3 are part of
 * the thread context); the VEH is process-global and dispatches by
 * matching `Rip` against the per-thread slot table.
 *
 * DR7 layout we touch:
 *
 *  bit  0: L0  local enable for DR0
 *  bit  2: L1  ... DR1
 *  bit  4: L2  ... DR2
 *  bit  6: L3  ... DR3
 *
 *  bits 16-17: RWE0  00 = execute, 01 = write, 11 = read/write
 *  bits 18-19: LEN0  00 = 1 byte, 01 = 2, 10 = 8, 11 = 4
 *  bits 20-23: RWE1 / LEN1
 *  bits 24-27: RWE2 / LEN2
 *  bits 28-31: RWE3 / LEN3
 *
 * For an instruction breakpoint we want RWE=00 (execute) and LEN=00
 * (1 byte), with the corresponding L<i> bit set.
 */

#include "wraith/wraith_stealth.h"
#include "stealth/hwbp/hwbp.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#if defined(__GNUC__) || defined(__clang__)
#  define WRAITH_TLS __thread
#elif defined(_MSC_VER)
#  define WRAITH_TLS __declspec(thread)
#else
#  define WRAITH_TLS
#endif

typedef struct hwbp_slot {
  void *target;
  void *replacement;
  int  active;
} hwbp_slot;

static WRAITH_TLS hwbp_slot g_slots[4];
static PVOID  g_veh = NULL;
static volatile LONG  g_veh_lock = 0;

static LONG NTAPI wr_hwbp_handler(PEXCEPTION_POINTERS info)
{
  if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) {
  return EXCEPTION_CONTINUE_SEARCH;
  }
  DWORD64 rip = info->ContextRecord->Rip;
  for (int i = 0; i < 4; ++i) {
  if (g_slots[i].active && (DWORD64)g_slots[i].target == rip) {
  info->ContextRecord->Rip = (DWORD64)g_slots[i].replacement;
  /* Clear the resume-flag-on-trap so single-step doesn't
  * loop. The CPU sets RF in EFlags when it raises a code
  * BP exception so the next iret resumes without
  * re-triggering, but some loaders strip that. Set it
  * defensively. */
  info->ContextRecord->EFlags |= 0x10000;  /* RF */
  return EXCEPTION_CONTINUE_EXECUTION;
  }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

static wraith_status_t ensure_veh_installed(void)
{
  if (g_veh) {
  return WRAITH_OK;
  }
  /* Tiny spinlock; install is rare. */
  while (InterlockedCompareExchange(&g_veh_lock, 1, 0) != 0) { ; }
  if (!g_veh) {
  g_veh = AddVectoredExceptionHandler(1u, wr_hwbp_handler);
  }
  InterlockedExchange(&g_veh_lock, 0);
  return g_veh ? WRAITH_OK : WRAITH_E_STEALTH_INSTALL;
}

wraith_status_t wraith_hwbp_install(void *target, void *replacement, int dr_index)
{
#if WRAITH_USE_HWBP_HOOKS
  if (!target || !replacement) {
  return WRAITH_E_NULL_ARG;
  }
  if (dr_index < -1 || dr_index > 3) {
  return WRAITH_E_INVALID_OPTIONS;
  }
  /* Auto-pick the first inactive slot. */
  if (dr_index < 0) {
  for (int i = 0; i < 4; ++i) {
  if (!g_slots[i].active) { dr_index = i; break; }
  }
  if (dr_index < 0) {
  return WRAITH_E_STEALTH_INCOMPATIBLE;  /* all 4 slots taken */
  }
  }
  if (g_slots[dr_index].active) {
  return WRAITH_E_STEALTH_INCOMPATIBLE;
  }

  wraith_status_t rc = ensure_veh_installed();
  if (rc != WRAITH_OK) {
  return rc;
  }

  HANDLE  h = GetCurrentThread();
  CONTEXT ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (!GetThreadContext(h, &ctx)) {
  return WRAITH_E_STEALTH_INSTALL;
  }

  DWORD64 addr = (DWORD64)target;
  switch (dr_index) {
  case 0: ctx.Dr0 = addr; break;
  case 1: ctx.Dr1 = addr; break;
  case 2: ctx.Dr2 = addr; break;
  case 3: ctx.Dr3 = addr; break;
  }

  /* Set L<i>=1, clear LEN<i>=00 (1 byte) and RWE<i>=00 (execute). */
  DWORD64 dr7 = ctx.Dr7;
  dr7 |=  (1ULL << (dr_index * 2));
  dr7 &= ~(0xFULL << (16 + dr_index * 4));
  ctx.Dr7 = dr7;

  if (!SetThreadContext(h, &ctx)) {
  return WRAITH_E_STEALTH_INSTALL;
  }

  g_slots[dr_index].target  = target;
  g_slots[dr_index].replacement = replacement;
  g_slots[dr_index].active  = 1;
  return WRAITH_OK;
#else
  (void)target; (void)replacement; (void)dr_index;
  return WRAITH_E_FEATURE_DISABLED;
#endif
}

wraith_status_t wraith_hwbp_remove(int dr_index)
{
#if WRAITH_USE_HWBP_HOOKS
  if (dr_index < 0 || dr_index > 3) {
  return WRAITH_E_INVALID_OPTIONS;
  }
  if (!g_slots[dr_index].active) {
  return WRAITH_OK;  /* nothing to do */
  }

  HANDLE  h = GetCurrentThread();
  CONTEXT ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
  if (!GetThreadContext(h, &ctx)) {
  return WRAITH_E_STEALTH_INSTALL;
  }

  DWORD64 dr7 = ctx.Dr7;
  dr7 &= ~(1ULL << (dr_index * 2));  /* clear L<i> */
  dr7 &= ~(0xFULL << (16 + dr_index * 4));  /* clear LEN/RWE for tidiness */
  ctx.Dr7 = dr7;

  switch (dr_index) {
  case 0: ctx.Dr0 = 0; break;
  case 1: ctx.Dr1 = 0; break;
  case 2: ctx.Dr2 = 0; break;
  case 3: ctx.Dr3 = 0; break;
  }

  if (!SetThreadContext(h, &ctx)) {
  return WRAITH_E_STEALTH_INSTALL;
  }

  g_slots[dr_index].active  = 0;
  g_slots[dr_index].target  = NULL;
  g_slots[dr_index].replacement = NULL;
  return WRAITH_OK;
#else
  (void)dr_index;
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
