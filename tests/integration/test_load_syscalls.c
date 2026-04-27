/*
 * tests/integration/test_load_syscalls.c
 *
 * Verifies Hell's Hall indirect syscalls.
 *
 *  1. wr_sc_engine_init returns S_OK and reports either
 *  HELLS_HALL or FALLBACK mode (FALLBACK is acceptable under
 *  wine64 where ntdll's user-mode prologue / gadget bytes don't
 *  match the Windows pattern).
 *
 *  2. wr_sc_call_NtAllocateVirtualMemory + NtProtectVirtualMemory +
 *  NtFreeVirtualMemory work end-to-end for a simple RW page.
 *
 *  3. End-to-end: wraith_load_library with WRAITH_F_INDIRECT_SYSCALLS |
 *  WRAITH_F_API_HASHING loads SampleDLL and addNumbers returns the
 *  expected sum.
 */

#include "wraith/wraith.h"
#include "syscalls/sc_engine.h"

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

static const char *mode_name(wr_sc_mode m)
{
  switch (m) {
  case WRAITH_SC_MODE_HELLS_HALL: return "HELLS_HALL";
  case WRAITH_SC_MODE_FALLBACK:  return "FALLBACK";
  default:  return "UNINIT";
  }
}

int main(int argc, char **argv)
{
  /* (1) Engine init ---------------------------------------------------- */
  wraith_status_t rc = wr_sc_engine_init();
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wr_sc_engine_init -> %s\n",
  wraith_status_string(rc));
  return 1;
  }
  wr_sc_mode mode = wr_sc_engine_mode();
  if (mode != WRAITH_SC_MODE_HELLS_HALL && mode != WRAITH_SC_MODE_FALLBACK) {
  fprintf(stderr, "FAIL: engine mode = %d\n", (int)mode);
  return 1;
  }
  printf("PASS: engine initialized in mode=%s\n", mode_name(mode));

  /* (2) Round-trip alloc/protect/free via the engine ------------------- */
  void  *page = NULL;
  SIZE_T  sz  = 0x1000;
  NTSTATUS s = wr_sc_call_NtAllocateVirtualMemory(
  (HANDLE)-1, &page, 0, &sz,
  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (s != 0 || !page) {
  fprintf(stderr,
  "FAIL: NtAllocateVirtualMemory NTSTATUS=0x%08x\n",
  (unsigned)s);
  return 1;
  }
  /* Touch it - any crash here would fail the test. */
  *(volatile uint32_t *)page = 0xDEADBEEF;
  if (*(volatile uint32_t *)page != 0xDEADBEEF) {
  fprintf(stderr, "FAIL: cannot read back what we wrote\n");
  return 1;
  }

  ULONG  oldp = 0;
  SIZE_T psz  = sz;
  s = wr_sc_call_NtProtectVirtualMemory(
  (HANDLE)-1, &page, &psz, PAGE_READONLY, &oldp);
  if (s != 0) {
  fprintf(stderr,
  "FAIL: NtProtectVirtualMemory NTSTATUS=0x%08x\n",
  (unsigned)s);
  return 1;
  }

  SIZE_T fsz = 0;
  s = wr_sc_call_NtFreeVirtualMemory((HANDLE)-1, &page, &fsz, MEM_RELEASE);
  if (s != 0) {
  fprintf(stderr,
  "FAIL: NtFreeVirtualMemory NTSTATUS=0x%08x\n",
  (unsigned)s);
  return 1;
  }
  printf("PASS: alloc/protect/free round-trip via engine\n");

  /* (3) End-to-end load with both flags on ----------------------------- */
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
  | WRAITH_F_RELIABILITY_ALL;

  wraith_handle_t h = NULL;
  rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(HASHING+INDIRECT_SYSCALLS) -> %s (%s)\n",
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
  int sum = ((add_fn)proc)(50, 51);
  wraith_free_library(h);
  free(bytes);

  if (sum != 101) {
  fprintf(stderr, "FAIL: addNumbers(50,51) = %d (expected 101)\n", sum);
  return 1;
  }
  printf("PASS: hashed+indirect-syscall load + addNumbers(50,51)=101\n");
  return 0;
}
