/*
 * tests/integration/test_hwbp.c
 *
 * Verifies : hardware-breakpoint hooks via DR0–DR3
 * + VEH redirect.
 *
 *  1. Define two no-args functions returning distinct sentinels.
 *  Call the target through a pointer-via-volatile to defeat
 *  compiler inlining.
 *  2. Install wraith_hwbp_install(target, replacement, -1) (auto-pick
 *  DR slot).
 *  3. Call the target. With the BP active, the VEH redirects
 *  RIP to the replacement, which returns 0x222.
 *  4. wraith_hwbp_remove on the slot.
 *  5. Call again. Expect 0x111 (BP gone, original path runs).
 *
 * Wine note: wine 9.0 supports DR get/set via SetThreadContext but
 * single-step exception delivery for code BPs is sometimes flaky.
 * The test prints SKIP if wraith_hwbp_install reports
 * WRAITH_E_STEALTH_INSTALL (couldn't program DRs). Otherwise asserts
 * the redirect happened.
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

__attribute__((noinline))
static int target_fn(void)
{
  return 0x111;
}

__attribute__((noinline))
static int replacement_fn(void)
{
  return 0x222;
}

int main(void)
{
  /* Defeat inlining at the call site by routing through a
  * volatile function pointer. */
  volatile int (*pfn)(void) = target_fn;

  /* (1) Sanity */
  int pre = pfn();
  if (pre != 0x111) {
  fprintf(stderr, "FAIL: pre-install target_fn = 0x%x\n", pre);
  return 1;
  }
  printf("PASS: target_fn pre-install = 0x111\n");

  /* (2) Install BP */
  wraith_status_t rc = wraith_hwbp_install((void *)target_fn,
  (void *)replacement_fn, -1);
  if (rc == WRAITH_E_STEALTH_INSTALL) {
  printf("SKIP: SetThreadContext for DR registers unsupported "
  "in this environment\n");
  return 0;
  }
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_hwbp_install -> %s\n",
  wraith_status_string(rc));
  return 1;
  }
  printf("PASS: HWBP installed (auto-picked DR slot)\n");

  /* (3) Call target - VEH should redirect */
  int hit = pfn();
  if (hit != 0x222) {
  fprintf(stderr,
  "FAIL: target_fn with BP returned 0x%x (expected 0x222)\n",
  hit);
  /* Best-effort cleanup before exiting. */
  for (int i = 0; i < 4; ++i) wraith_hwbp_remove(i);
  return 1;
  }
  printf("PASS: BP redirected target_fn -> replacement_fn (0x222)\n");

  /* (4) Remove BP - we don't know which slot the auto-picker
  * grabbed, so try all four. RemoveSlot for an inactive slot is
  * documented to return S_OK. */
  for (int i = 0; i < 4; ++i) {
  wraith_hwbp_remove(i);
  }

  /* (5) Call target again - original path */
  int post = pfn();
  if (post != 0x111) {
  fprintf(stderr,
  "FAIL: post-remove target_fn = 0x%x (expected 0x111)\n",
  post);
  return 1;
  }
  printf("PASS: post-remove target_fn = 0x111\n");
  return 0;
}
