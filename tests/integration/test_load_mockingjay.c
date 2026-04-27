/*
 * tests/integration/test_load_mockingjay.c
 *
 * Verifies : Mockingjay mapping.
 *
 * The technique requires a `MEM_IMAGE+RWX` region somewhere in
 * the process - typically a `.text` section of a signed DLL that
 * shipped writable. Stock wine64 prefixes don't usually have such
 * a region, so we accept WRAITH_E_MAP_NO_HOST_DLL as a SKIP. When a
 * candidate IS found, we assert the load succeeds and addNumbers
 * works.
 */

#include "wraith/wraith.h"

#include "mapping/map_mockingjay_scanner.h"

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
  const char *path = (argc > 1) ? argv[1] : "payload.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  fprintf(stderr, "FAIL: cannot read %s\n", path);
  return 1;
  }

  /* Probe the scanner directly first so we can SKIP cleanly. */
  void *probe = NULL;
  size_t probe_sz = 0;
  wraith_status_t pr = wr_mockingjay_find_region(size, &probe, &probe_sz);
  if (pr == WRAITH_E_MAP_NO_HOST_DLL) {
  printf("SKIP: no MEM_IMAGE+RWX region in process for "
  "Mockingjay (typical on wine64; real-world hosts "
  "with msys-2.0 / select MSI helpers do qualify)\n");
  free(bytes);
  return 0;
  }
  if (pr != WRAITH_OK) {
  fprintf(stderr, "FAIL: scanner -> %s\n", wraith_status_string(pr));
  free(bytes);
  return 1;
  }
  printf("PASS: scanner found %zu-byte MEM_IMAGE+RWX region at %p\n",
  probe_sz, probe);

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_MOCKINGJAY;
  opt.flags  = WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(MOCKINGJAY) -> %s (%s)\n",
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
  int sum = ((add_fn)fn)(60, 6);
  wraith_free_library(h);
  free(bytes);

  if (sum != 66) {
  fprintf(stderr, "FAIL: addNumbers(60,6) = %d (expected 66)\n", sum);
  return 1;
  }
  printf("PASS: mockingjay-overlaid payload addNumbers(60,6)=66\n");
  return 0;
}
