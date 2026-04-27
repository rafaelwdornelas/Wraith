/*
 * tests/integration/test_load_seh.c
 *
 * Verifies deliverables:
 *
 *  1. RtlAddFunctionTable was called for the loaded image.
 *  Proof:  after wraith_load_library, RtlLookupFunctionEntry(rip, ...)
 *  for an address inside the loaded DLL returns non-NULL.
 *
 *  2. DllMain receives DLL_PROCESS_ATTACH at load and
 *  DLL_PROCESS_DETACH at wraith_free_library.
 *  Proof:  the fixture's DllMain stores each reason value into a
 *  process-global atom of the form
 *  "wr_seh_dll_last_reason=<n>". After load, "1" must be
 *  present (DLL_PROCESS_ATTACH); after free, "0" must be
 *  present too (DLL_PROCESS_DETACH).
 *
 *  3. RtlDeleteFunctionTable was called on free.
 *  Proof:  after wraith_free_library, RtlLookupFunctionEntry on an
 *  address that *used to* be inside the loaded image returns
 *  NULL again. (Note: the address is unmapped at this point
 *  so we look up the saved RIP value before free.)
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef int (*seh_target_fn)(int, int);

static int read_file_bytes(const char *path, void **out_buf, size_t *out_size)
{
  FILE *fp = fopen(path, "rb");
  if (!fp) {
  fprintf(stderr, "FAIL: fopen(\"%s\") errno=%d\n", path, errno);
  return -1;
  }
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

static int atom_exists(DWORD reason)
{
  wchar_t name[64];
  wsprintfW(name, L"wr_seh_dll_last_reason=%u", reason);
  ATOM a = GlobalFindAtomW(name);
  return a != 0;
}

int main(int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "seh_dll.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  return 1;
  }

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, NULL, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr, "FAIL: wraith_load_library -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  /* (2a) DLL_PROCESS_ATTACH must have been seen. */
  if (!atom_exists(DLL_PROCESS_ATTACH)) {
  fprintf(stderr,
  "FAIL: no DLL_PROCESS_ATTACH atom after load\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  /* (1) Find a function inside the loaded image and probe SEH lookup. */
  void *proc = NULL;
  rc = wraith_get_proc_address(h, "seh_target", &proc);
  if (rc != WRAITH_OK || !proc) {
  fprintf(stderr, "FAIL: wraith_get_proc_address(seh_target) -> %s\n",
  wraith_status_string(rc));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  DWORD64 image_base = 0;
  PRUNTIME_FUNCTION rf =
  RtlLookupFunctionEntry((DWORD64)(uintptr_t)proc, &image_base, NULL);
  if (!rf) {
  fprintf(stderr,
  "FAIL: RtlLookupFunctionEntry returned NULL for proc=%p\n",
  proc);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  /* Sanity: the call also exercises the prologue/epilogue path. */
  int sanity = ((seh_target_fn)proc)(3, 4);
  if (sanity != 12) {
  fprintf(stderr, "FAIL: seh_target(3,4) = %d (expected 12)\n", sanity);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  /* Save the RIP for the post-free lookup. */
  DWORD64 saved_rip = (DWORD64)(uintptr_t)proc;

  rc = wraith_free_library(h);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_free_library -> %s\n", wraith_status_string(rc));
  free(bytes);
  return 1;
  }
  free(bytes);

  /* (2b) DLL_PROCESS_DETACH must have been seen. */
  if (!atom_exists(DLL_PROCESS_DETACH)) {
  fprintf(stderr, "FAIL: no DLL_PROCESS_DETACH atom after free\n");
  return 1;
  }

  /* (3) After RtlDeleteFunctionTable + memory release, the saved RIP
  * is no longer in any registered table. We cannot dereference it,
  * but the lookup is purely tabular and safe to call. */
  DWORD64 base2 = 0;
  PRUNTIME_FUNCTION rf2 =
  RtlLookupFunctionEntry(saved_rip, &base2, NULL);
  if (rf2 != NULL) {
  fprintf(stderr,
  "FAIL: RtlLookupFunctionEntry still finds entry after free\n");
  return 1;
  }

  printf("PASS: SEH x64 registered (RtlLookupFunctionEntry hit)\n");
  printf("PASS: DllMain saw both ATTACH and DETACH\n");
  printf("PASS: RtlDeleteFunctionTable removed the entry on free\n");
  return 0;
}
