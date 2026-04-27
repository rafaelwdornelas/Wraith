/*
 * tests/integration/test_load_hashed.c
 *
 * Verifies hash-based API resolution.
 *
 *  1. PEB walk equivalence: wr_pebwalk_find_module_a("kernel32.dll")
 *  returns the same base as GetModuleHandleW(L"kernel32.dll").
 *
 *  2. Export resolver: wr_resolver_lookup_a(kernel32, "GetCurrentProcessId")
 *  returns the same pointer as GetProcAddress.
 *
 *  3. End-to-end hash-on-load: wraith_load_library with WRAITH_F_API_HASHING
 *  flag loads payload.dll and calls addNumbers - all dependency
 *  resolution went through PEB walk + DJB2.
 */

#include "wraith/wraith.h"

/* Internal headers - exposed to the integration test only via this
 * tests/integration/ directory's include path. */
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

static int read_file_bytes(const char *path, void **out_buf, size_t *out_size)
{
  FILE *fp = fopen(path, "rb");
  if (!fp) { return -1; }
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (n <= 0) { fclose(fp); return -1; }
  void *buf = malloc((size_t)n);
  if (!buf) { fclose(fp); return -1; }
  size_t r = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  if (r != (size_t)n) { free(buf); return -1; }
  *out_buf  = buf;
  *out_size = (size_t)n;
  return 0;
}

int main(int argc, char **argv)
{
  /* (1) PEB walk == GetModuleHandle ----------------------------------- */
  void *via_pebwalk = NULL;
  wraith_status_t rc = wr_pebwalk_find_module_a("kernel32.dll", &via_pebwalk);
  if (rc != WRAITH_OK || !via_pebwalk) {
  fprintf(stderr, "FAIL: pebwalk(kernel32.dll) -> %s\n",
  wraith_status_string(rc));
  return 1;
  }
  HMODULE via_native = GetModuleHandleW(L"kernel32.dll");
  if ((void *)via_native != via_pebwalk) {
  fprintf(stderr,
  "FAIL: pebwalk %p != GetModuleHandle %p\n",
  via_pebwalk, (void *)via_native);
  return 1;
  }
  printf("PASS: PEB walk(kernel32.dll) == GetModuleHandleW (%p)\n",
  via_pebwalk);

  /* (2) Export resolver == GetProcAddress ---------------------------- */
  void *via_resolver = NULL;
  rc = wr_resolver_lookup_a(via_pebwalk, "GetCurrentProcessId",
  &via_resolver);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: resolver_lookup -> %s\n",
  wraith_status_string(rc));
  return 1;
  }
  void *via_gpa = (void *)GetProcAddress(via_native, "GetCurrentProcessId");
  if (via_resolver != via_gpa) {
  fprintf(stderr,
  "FAIL: resolver %p != GetProcAddress %p\n",
  via_resolver, via_gpa);
  return 1;
  }
  printf("PASS: resolver(GetCurrentProcessId()) == GetProcAddress (%p)\n",
  via_resolver);

  /* (3) End-to-end hashed load --------------------------------------- */
  const char *path = (argc > 1) ? argv[1] : "payload.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  fprintf(stderr, "FAIL: cannot read %s\n", path);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_PRIVATE_RW_RX;
  opt.flags  = WRAITH_F_API_HASHING | WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(WRAITH_F_API_HASHING) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  void *proc = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &proc);
  if (rc != WRAITH_OK || !proc) {
  fprintf(stderr, "FAIL: wraith_get_proc_address -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)proc)(11, 22);
  wraith_free_library(h);
  free(bytes);

  if (sum != 33) {
  fprintf(stderr, "FAIL: addNumbers(11,22) = %d (expected 33)\n", sum);
  return 1;
  }
  printf("PASS: hashed-resolution load + addNumbers(11,22)=33\n");
  return 0;
}
