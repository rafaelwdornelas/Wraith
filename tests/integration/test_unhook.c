/*
 * tests/integration/test_unhook.c
 *
 * Verifies userland-hook removal.
 *
 *  1. Locate `ntdll!NtClose` via PEB walk + export resolver.
 *  2. Read the function's first 16 bytes (the canonical prologue).
 *  3. Patch them with a fake hook (0x90 NOP sled - innocuous, won't
 *  itself cause a crash if accidentally executed).
 *  4. Confirm the patch landed.
 *  5. Call wraith_unhook_ntdll().
 *  6. Verify the original 16 bytes are restored.
 *
 * Wine note: wine 9.0 ships its own ntdll.dll on disk that matches
 * the loaded copy bit-for-bit (apart from runtime relocations). The
 * 16-byte chunk granularity catches the fake hook without overwriting
 * unrelated drift.
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Public PEB walker exposed via internal headers - tests link the
 * library statically and have access. */
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#define PROBE_BYTES 16

int main(void)
{
  void *ntdll = NULL;
  if (wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll) != WRAITH_OK
  || !ntdll) {
  fprintf(stderr, "FAIL: PEB walk could not locate ntdll.dll\n");
  return 1;
  }

  void *NtClose = NULL;
  if (wr_resolver_lookup_a(ntdll, "NtClose", &NtClose) != WRAITH_OK
  || !NtClose) {
  fprintf(stderr, "FAIL: cannot resolve NtClose\n");
  return 1;
  }

  /* Snapshot the original prologue. */
  uint8_t orig[PROBE_BYTES];
  memcpy(orig, NtClose, PROBE_BYTES);

  /* Sanity: the prologue must not already be all 0x90. */
  int all_nops = 1;
  for (int i = 0; i < PROBE_BYTES; ++i) {
  if (orig[i] != 0x90) { all_nops = 0; break; }
  }
  if (all_nops) {
  fprintf(stderr, "FAIL: NtClose already 0x90's pre-test (unexpected)\n");
  return 1;
  }
  printf("PASS: NtClose original prologue captured (%02x %02x %02x ...)\n",
  orig[0], orig[1], orig[2]);

  /* Install the fake hook. */
  DWORD old_prot = 0;
  if (!VirtualProtect(NtClose, PROBE_BYTES, PAGE_READWRITE, &old_prot)) {
  fprintf(stderr, "FAIL: VirtualProtect RW pre-hook errno=0x%lx\n",
  (unsigned long)GetLastError());
  return 1;
  }
  memset(NtClose, 0x90, PROBE_BYTES);
  DWORD ignore = 0;
  VirtualProtect(NtClose, PROBE_BYTES, old_prot, &ignore);
  FlushInstructionCache(GetCurrentProcess(), NtClose, PROBE_BYTES);

  /* Confirm the hook landed. */
  if (memcmp(NtClose, orig, PROBE_BYTES) == 0) {
  fprintf(stderr,
  "FAIL: VirtualProtect/memset didn't actually patch\n");
  return 1;
  }
  printf("PASS: fake 0x90 hook installed\n");

  /* Unhook. */
  wraith_status_t rc = wraith_unhook_ntdll();
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_unhook_ntdll() -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  return 1;
  }

  /* Confirm the original bytes are back. */
  if (memcmp(NtClose, orig, PROBE_BYTES) != 0) {
  fprintf(stderr,
  "FAIL: NtClose bytes after unhook differ from original "
  "(got %02x %02x %02x ...)\n",
  ((uint8_t *)NtClose)[0],
  ((uint8_t *)NtClose)[1],
  ((uint8_t *)NtClose)[2]);
  return 1;
  }
  printf("PASS: NtClose prologue restored from disk copy\n");
  return 0;
}
