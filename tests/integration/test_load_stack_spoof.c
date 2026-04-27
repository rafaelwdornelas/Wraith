/*
 * tests/integration/test_load_stack_spoof.c
 *
 * Verifies : single-frame stack spoofing around the
 * Hell's Hall syscall path.
 *
 *  1. wraith_stackspoof_probe() returns either WRAITH_OK (engine in
 *  HELLS_HALL mode with a ret gadget located) or
 *  WRAITH_E_STEALTH_INCOMPATIBLE (FALLBACK mode, e.g. wine64).
 *
 *  2. wraith_load_library with WRAITH_F_STACK_SPOOF + WRAITH_F_API_HASHING +
 *  WRAITH_F_INDIRECT_SYSCALLS works end-to-end. Under wine the
 *  engine routes to FALLBACK (the spoof asm stubs aren't
 *  executed); under real Windows the asm stubs run but the
 *  ABI guarantees the call returns to the right caller. Either
 *  way, addNumbers must resolve and return the right answer.
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include <stdio.h>
#include <stdlib.h>
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
  /* (1) Probe - report whichever mode the engine settled into. */
  wraith_status_t p = wraith_stackspoof_probe();
  if (p == WRAITH_OK) {
  printf("PASS: probe ok - HELLS_HALL with ret gadget\n");
  } else if (p == WRAITH_E_STEALTH_INCOMPATIBLE) {
  printf("PASS: probe reports incompatible (FALLBACK mode - "
  "wine or hooked ntdll); spoof inert this run\n");
  } else {
  fprintf(stderr, "FAIL: probe -> %s\n", wraith_status_string(p));
  return 1;
  }

  /* (2) Functional regression with the flag on. */
  const char *path = (argc > 1) ? argv[1] : "payload.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  fprintf(stderr, "FAIL: cannot read %s\n", path);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_PRIVATE_RW_RX;
  opt.flags  = WRAITH_F_API_HASHING
  | WRAITH_F_INDIRECT_SYSCALLS
  | WRAITH_F_STACK_SPOOF
  | WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(STACK_SPOOF) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  void *fn = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &fn);
  if (rc != WRAITH_OK || !fn) {
  fprintf(stderr, "FAIL: wraith_get_proc_address -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)fn)(7, 35);
  wraith_free_library(h);
  free(bytes);

  if (sum != 42) {
  fprintf(stderr, "FAIL: addNumbers(7,35) = %d (expected 42)\n", sum);
  return 1;
  }
  printf("PASS: spoofed-flag load + addNumbers(7,35)=42\n");
  return 0;
}
