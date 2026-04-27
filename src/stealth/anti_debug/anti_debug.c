/*
 * src/stealth/anti_debug/anti_debug.c
 *
 * Zero out the PEB-resident anti-debug signals:
 *
 *  - PEB.BeingDebugged  (offset 0x02 on x64)
 *  - PEB.NtGlobalFlag  (offset 0xBC on x64)
 *
 * The fields are part of the documented PEB layout; we use the
 * MinGW <winternl.h> definition which exposes BeingDebugged but
 * not NtGlobalFlag, so we reach the latter by byte offset.
 *
 * The technique does not bypass an attached kernel debugger and
 * does not affect `NtQueryInformationProcess(ProcessDebugPort)` -
 * those are driven by kernel state and require a more invasive
 * approach (DKOM or a dedicated NtQueryInformationProcess hook,
 * deferred to a future tier).
 */

#include "stealth/anti_debug/anti_debug.h"

#include <stdint.h>
#include <windows.h>
#include <winternl.h>

#define WRAITH_PEB_NTGLOBALFLAG_OFFSET 0xBC

/* Heap-flag bits cleared from NtGlobalFlag - the canonical
 * "process is being debugged" markers in the PEB. */
#define WRAITH_FLG_HEAP_ENABLE_TAIL_CHECK  0x10
#define WRAITH_FLG_HEAP_ENABLE_FREE_CHECK  0x20
#define WRAITH_FLG_HEAP_VALIDATE_PARAMETERS 0x40

wraith_status_t wr_anti_debug_spoof_install(void)
{
#if WRAITH_USE_ANTI_DEBUG_SPOOF
  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  /* PEB.BeingDebugged is at +0x02 on every x64 layout we care
  * about; the public struct member is `BeingDebugged`. */
  peb->BeingDebugged = 0;

  uint32_t *ng = (uint32_t *)((uint8_t *)peb + WRAITH_PEB_NTGLOBALFLAG_OFFSET);
  *ng &= ~(uint32_t)(WRAITH_FLG_HEAP_ENABLE_TAIL_CHECK
  | WRAITH_FLG_HEAP_ENABLE_FREE_CHECK
  | WRAITH_FLG_HEAP_VALIDATE_PARAMETERS);

  return WRAITH_OK;
#else
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
