/*
 * tests/integration/test_load_phantom.c
 *
 * Verifies phantom DLL hollowing.
 *
 *  1. wraith_load_library with WRAITH_MAP_PHANTOM_HOLLOW selects a host DLL,
 *  maps it via NtCreateSection(SEC_IMAGE), overlays our payload,
 *  and produces a working module (addNumbers returns the right
 *  value).
 *
 *  2. VirtualQuery against the loaded image's base reports
 *  Type == MEM_IMAGE (not MEM_PRIVATE) - the IOC differentiator
 *  that pe-sieve / Moneta key off.
 *
 *  3. wraith_free_library unmaps the view + closes the section without
 *  leaking handles or memory; a second VirtualQuery on the same
 *  RIP yields MEM_FREE.
 *
 * Wine note: wine64 implements NtCreateSection(SEC_IMAGE) for system
 * DLLs in System32, so this test is expected to pass under wine when
 * one of the curated host candidates is present in the prefix.
 */

#include "wraith/wraith.h"

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

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_PHANTOM_HOLLOW;
  opt.flags  = WRAITH_F_API_HASHING
  | WRAITH_F_INDIRECT_SYSCALLS
  | WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(PHANTOM) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  /* (1) Functional check ----------------------------------------------- */
  void *proc = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &proc);
  if (rc != WRAITH_OK || !proc) {
  fprintf(stderr, "FAIL: wraith_get_proc_address -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)proc)(13, 17);
  if (sum != 30) {
  fprintf(stderr, "FAIL: addNumbers(13,17) = %d (expected 30)\n", sum);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: phantom-hollowed payload addNumbers(13,17)=30\n");

  /* (2) Region classification ----------------------------------------- */
  MEMORY_BASIC_INFORMATION mbi = {0};
  SIZE_T q = VirtualQuery(proc, &mbi, sizeof(mbi));
  if (q == 0) {
  fprintf(stderr, "FAIL: VirtualQuery returned 0\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  /* Save the RIP for the post-free check. */
  void *saved_rip = proc;

  /* MEM_IMAGE (0x1000000) is what we want; MEM_PRIVATE (0x20000)
  * means we accidentally allocated unbacked. */
  if (mbi.Type != MEM_IMAGE) {
  fprintf(stderr,
  "FAIL: VirtualQuery.Type=0x%lx (expected MEM_IMAGE 0x%lx)\n",
  (unsigned long)mbi.Type, (unsigned long)MEM_IMAGE);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: VirtualQuery reports MEM_IMAGE backing (Type=0x%lx)\n",
  (unsigned long)mbi.Type);

  /* (3) Cleanup ------------------------------------------------------- */
  rc = wraith_free_library(h);
  free(bytes);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_free_library -> %s\n",
  wraith_status_string(rc));
  return 1;
  }

  MEMORY_BASIC_INFORMATION mbi2 = {0};
  if (VirtualQuery(saved_rip, &mbi2, sizeof(mbi2)) == 0) {
  fprintf(stderr, "FAIL: post-free VirtualQuery returned 0\n");
  return 1;
  }
  if (mbi2.State != MEM_FREE) {
  fprintf(stderr,
  "FAIL: post-free State=0x%lx (expected MEM_FREE 0x%lx)\n",
  (unsigned long)mbi2.State, (unsigned long)MEM_FREE);
  return 1;
  }
  printf("PASS: section unmapped on free (State=MEM_FREE)\n");
  return 0;
}
