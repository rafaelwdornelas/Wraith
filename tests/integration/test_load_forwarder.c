/*
 * tests/integration/test_load_forwarder.c
 *
 * Verifies forwarder resolution.
 *
 * Loads `forwarder_dll.dll` (a fixture whose `GetForwardedProcessId`
 * export is a forwarder to `kernel32!GetCurrentProcessId()`), resolves the
 * forwarded export via wraith_get_proc_address, calls it, and checks the
 * returned PID matches GetCurrentProcessId() in the host process.
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef DWORD (WINAPI *getpid_fn)(void);

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
  if (n <= 0) {
  fclose(fp);
  fprintf(stderr, "FAIL: empty file \"%s\"\n", path);
  return -1;
  }
  void *buf = malloc((size_t)n);
  if (!buf) {
  fclose(fp);
  return -1;
  }
  size_t r = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  if (r != (size_t)n) {
  free(buf);
  fprintf(stderr, "FAIL: short read %zu/%ld\n", r, n);
  return -1;
  }
  *out_buf  = buf;
  *out_size = (size_t)n;
  return 0;
}

int main(int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "forwarder_dll.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  return 1;
  }

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, NULL, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr, "FAIL: wraith_load_library(forwarder_dll) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  void *proc = NULL;
  rc = wraith_get_proc_address(h, "GetForwardedProcessId", &proc);
  if (rc != WRAITH_OK || !proc) {
  fprintf(stderr,
  "FAIL: wraith_get_proc_address(GetForwardedProcessId) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  DWORD via_forward = ((getpid_fn)proc)();
  DWORD via_native  = GetCurrentProcessId();

  wraith_free_library(h);
  free(bytes);

  if (via_forward != via_native) {
  fprintf(stderr,
  "FAIL: forwarder result %lu != native %lu\n",
  (unsigned long)via_forward, (unsigned long)via_native);
  return 1;
  }

  printf("PASS: forwarder GetForwardedProcessId == kernel32!GetCurrentProcessId() == %lu\n",
  (unsigned long)via_forward);
  return 0;
}
