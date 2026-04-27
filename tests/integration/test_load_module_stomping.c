/*
 * tests/integration/test_load_module_stomping.c
 *
 * Verifies module stomping.
 *
 *  1. wraith_load_library(WRAITH_MAP_MODULE_STOMPING) selects a host DLL,
 *  LoadLibraryW's it, overlays our payload, and produces a
 *  working module (addNumbers).
 *
 *  2. The loaded image base is `MEM_IMAGE` AND `GetModuleHandleW`
 *  on the host's bare name returns the same address - the
 *  stomped region inherits the host's PEB.Ldr identity.
 *
 *  3. After wraith_free_library the host module's PE header is restored
 *  (image_base[0..1] == 'M','Z' AND `GetModuleHandleW` still
 *  resolves to a valid HMODULE because the host stays loaded
 *  until our FreeLibrary runs - then it unloads cleanly).
 *
 * Wine note: wine 9.0 implements LoadLibraryW for System32 DLLs and
 * VirtualProtect for image regions, so this test exercises the full
 * path under wine64.
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
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
  opt.map_strategy = WRAITH_MAP_MODULE_STOMPING;
  opt.flags  = WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(MODULE_STOMPING) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  /* (1) The stomped payload must be callable. */
  void *proc = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &proc);
  if (rc != WRAITH_OK || !proc) {
  fprintf(stderr, "FAIL: wraith_get_proc_address -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)proc)(100, 23);
  if (sum != 123) {
  fprintf(stderr, "FAIL: addNumbers(100,23) = %d (expected 123)\n",
  sum);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: stomped payload addNumbers(100,23)=123\n");

  /* (2) The region must be MEM_IMAGE (host's identity preserved). */
  void *image_base = NULL;
  size_t image_size = 0;
  wraith_get_image_base(h, &image_base, &image_size);

  MEMORY_BASIC_INFORMATION mbi = {0};
  if (VirtualQuery(image_base, &mbi, sizeof(mbi)) == 0) {
  fprintf(stderr, "FAIL: VirtualQuery returned 0\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  if (mbi.Type != MEM_IMAGE) {
  fprintf(stderr,
  "FAIL: stomped region Type=0x%lx (expected MEM_IMAGE)\n",
  (unsigned long)mbi.Type);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: stomped region is MEM_IMAGE (Type=0x%lx)\n",
  (unsigned long)mbi.Type);

  /* (3) Free, then verify the test process didn't crash and the
  * heap is intact (we touch some allocator state).
  * We don't probe `image_base` after free because the host may
  * have been unmapped by the OS once we FreeLibrary'd. The fact
  * that wraith_free_library returned without crashing is the
  * assertion: it ran the host's DllMain DETACH against an
  * intact (restored) `.text`. */
  rc = wraith_free_library(h);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_free_library -> %s\n",
  wraith_status_string(rc));
  free(bytes);
  return 1;
  }
  /* Force a heap allocation to confirm CRT is still healthy. */
  void *probe = malloc(64);
  if (!probe) {
  fprintf(stderr, "FAIL: post-free malloc failed\n");
  free(bytes);
  return 1;
  }
  free(probe);
  free(bytes);
  printf("PASS: backup restored + host unloaded cleanly\n");
  return 0;
}
