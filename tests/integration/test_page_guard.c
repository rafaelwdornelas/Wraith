/*
 * tests/integration/test_page_guard.c
 *
 * Verifies : page-guard self-encryption.
 *
 *  1. Load SampleDLL.
 *  2. Resolve addNumbers and call once unguarded (sanity).
 *  3. wraith_pageguard_arm - .text pages are now encrypted with
 *  PAGE_GUARD set.
 *  4. Sample image_base[..]: bytes inside an executable section
 *  are now scrambled (we read header bytes for safety, since
 *  the header section is not what gets guarded - we pick a
 *  page inside .text via the function pointer's RVA).
 *  5. Call addNumbers - the first instruction triggers
 *  EXCEPTION_GUARD_PAGE; the VEH decrypts that page; the
 *  function executes correctly. Subsequent calls hit the
 *  now-plain page without faulting.
 *  6. wraith_pageguard_disarm.
 *  7. Call addNumbers again - works normally.
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

static int read_file_bytes(const char *path, void **out_buf, size_t *out_size)
{
  FILE *fp = fopen(path, "rb");
  if (!fp) return -1;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (n <= 0) { fclose(fp); return -1; }
  void *buf = malloc((size_t)n);
  if (!buf) { fclose(fp); return -1; }
  size_t r = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  if (r != (size_t)n) { free(buf); return -1; }
  *out_buf = buf;
  *out_size = (size_t)n;
  return 0;
}

int main(int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "payload.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  fprintf(stderr, "FAIL: cannot read %s\n", path);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.flags = WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr, "FAIL: wraith_load_library -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  void *fn = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &fn);
  if (rc != WRAITH_OK || !fn) {
  fprintf(stderr, "FAIL: wraith_get_proc_address\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  int pre_arm = ((add_fn)fn)(10, 11);
  if (pre_arm != 21) {
  fprintf(stderr, "FAIL: pre-arm addNumbers(10,11)=%d\n", pre_arm);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: pre-arm addNumbers(10,11)=21\n");

  /* Capture a few bytes of the function for the encrypted-check. */
  uint8_t pre_bytes[8];
  memcpy(pre_bytes, fn, sizeof(pre_bytes));

  rc = wraith_pageguard_arm(h);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_pageguard_arm -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: wraith_pageguard_arm returned OK\n");

  /* (4) The page should now be encrypted. We can't read it
  * directly without triggering the guard - but we can use
  * VirtualQuery to confirm PAGE_GUARD is set. */
  MEMORY_BASIC_INFORMATION mbi = {0};
  if (VirtualQuery(fn, &mbi, sizeof(mbi)) == 0) {
  fprintf(stderr, "FAIL: VirtualQuery failed\n");
  wraith_pageguard_disarm(h);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  if (!(mbi.Protect & PAGE_GUARD)) {
  fprintf(stderr,
  "FAIL: post-arm Protect=0x%lx (expected PAGE_GUARD bit set)\n",
  (unsigned long)mbi.Protect);
  wraith_pageguard_disarm(h);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: function page has PAGE_GUARD set (Protect=0x%lx)\n",
  (unsigned long)mbi.Protect);

  /* (5) Calling triggers EXCEPTION_GUARD_PAGE - VEH decrypts and
  * the call returns normally. */
  int armed_call = ((add_fn)fn)(33, 44);
  if (armed_call != 77) {
  fprintf(stderr,
  "FAIL: armed addNumbers(33,44)=%d (expected 77)\n",
  armed_call);
  wraith_pageguard_disarm(h);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: armed-call addNumbers(33,44)=77 (lazy decrypt fired)\n");

  /* (6) Disarm */
  rc = wraith_pageguard_disarm(h);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_pageguard_disarm -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  /* Verify bytes are back. */
  if (memcmp(fn, pre_bytes, sizeof(pre_bytes)) != 0) {
  fprintf(stderr,
  "FAIL: post-disarm function bytes don't match original\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: post-disarm function bytes restored\n");

  int post = ((add_fn)fn)(2, 3);
  wraith_free_library(h);
  free(bytes);
  if (post != 5) {
  fprintf(stderr, "FAIL: post-disarm addNumbers(2,3)=%d\n", post);
  return 1;
  }
  printf("PASS: post-disarm addNumbers(2,3)=5\n");
  return 0;
}
