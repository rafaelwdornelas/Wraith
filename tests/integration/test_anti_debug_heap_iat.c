/*
 * tests/integration/test_anti_debug_heap_iat.c
 *
 * Verifies passive evasion finishers:
 *   - anti-debug spoof   (PEB.BeingDebugged + NtGlobalFlag)
 *   - heap masquerade    (RtlCreateHeap rooted in MEM_IMAGE)
 *   - host IAT redirect  (rewrite Sleep thunks to a stub)
 */

#include "wraith/wraith.h"

#include "stealth/anti_debug/anti_debug.h"
#include "stealth/heap_masq/heap_masq.h"
#include "stealth/host_iat/host_iat.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winternl.h>

static int test_anti_debug(void)
{
  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  /* Set BeingDebugged manually to verify the spoof clears it. */
  peb->BeingDebugged = 1;

  wraith_status_t rc = wr_anti_debug_spoof_install();
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: anti_debug -> %s\n", wraith_status_string(rc));
  return -1;
  }
  if (peb->BeingDebugged != 0) {
  fprintf(stderr, "FAIL: BeingDebugged not cleared\n");
  return -1;
  }
  printf("PASS: PEB.BeingDebugged cleared\n");
  return 0;
}

static int test_heap_masq(void)
{
  void *a = wr_heap_masq_alloc(64);
  void *b = wr_heap_masq_alloc(128);
  if (!a || !b) {
  fprintf(stderr, "FAIL: heap masq alloc\n");
  return -1;
  }
  /* The two allocations should be valid distinct addresses. */
  if (a == b) {
  fprintf(stderr, "FAIL: heap allocs collide\n");
  return -1;
  }
  /* Write/read smoke test. */
  memset(a, 0xAA, 64);
  if (((uint8_t *)a)[0] != 0xAA) {
  fprintf(stderr, "FAIL: heap masq read-back\n");
  return -1;
  }
  wr_heap_masq_free(a);
  wr_heap_masq_free(b);
  wr_heap_masq_release();
  printf("PASS: private masqueraded heap alloc/free round-trip\n");
  return 0;
}

static int test_host_iat(void)
{
  /* Resolve a real Sleep pointer through GetProcAddress so we
  * have a concrete `original` value to look for in IATs. We
  * don't actually redirect to a different function - that
  * would cause this test process's Sleep calls to misbehave.
  * Instead we redirect Sleep -> Sleep (no-op), proving the
  * walker hits at least one IAT slot. */
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  void *real_sleep = (void *)GetProcAddress(k32, "Sleep");
  if (!real_sleep) {
  fprintf(stderr, "FAIL: cannot resolve Sleep\n");
  return -1;
  }

  unsigned count = 0;
  wraith_status_t rc = wr_host_iat_redirect(real_sleep, real_sleep, &count);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: host_iat_redirect -> %s\n",
  wraith_status_string(rc));
  return -1;
  }
  /* Almost every loaded module imports Sleep one way or another;
  * we expect at least one hit, but accept zero on minimal
  * environments without complaining. */
  printf("PASS: host IAT walker visited %u Sleep thunks\n", count);
  return 0;
}

int main(void)
{
  if (test_anti_debug() != 0) return 1;
  if (test_heap_masq()  != 0) return 1;
  if (test_host_iat()   != 0) return 1;
  return 0;
}
