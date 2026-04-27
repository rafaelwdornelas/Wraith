/*
 * tests/integration/test_load_peb_linkage.c
 *
 * Verifies PEB.Ldr linkage with masquerade.
 *
 *  1. After wraith_load_library with WRAITH_F_PEB_LINKAGE + masquerade name,
 *  EnumProcessModulesEx lists a module whose base name equals the
 *  masquerade.
 *
 *  2. After wraith_free_library the same enumeration NO LONGER lists
 *  that name (proves unlink ran without leaving a dangling entry).
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <psapi.h>

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

static int find_module_by_name(const wchar_t *needle, HMODULE *out)
{
  HMODULE mods[256] = {0};
  DWORD  needed  = 0;
  if (!EnumProcessModulesEx(GetCurrentProcess(), mods, sizeof(mods),
  &needed, LIST_MODULES_ALL)) {
  fprintf(stderr, "FAIL: EnumProcessModulesEx errno=0x%lx\n",
  (unsigned long)GetLastError());
  return -1;
  }
  DWORD count = needed / sizeof(HMODULE);
  if (count > sizeof(mods) / sizeof(mods[0])) {
  count = sizeof(mods) / sizeof(mods[0]);
  }
  for (DWORD i = 0; i < count; ++i) {
  wchar_t name[256] = {0};
  if (GetModuleBaseNameW(GetCurrentProcess(), mods[i],
  name, sizeof(name) / sizeof(name[0])) > 0) {
  if (_wcsicmp(name, needle) == 0) {
  if (out) *out = mods[i];
  return 1;
  }
  }
  }
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

  /* Sanity: the masquerade name must NOT be present pre-load. */
  const wchar_t *masquerade = L"winnet_phase8.dll";
  if (find_module_by_name(masquerade, NULL)) {
  fprintf(stderr,
  "FAIL: masquerade name already loaded before wraith_load_library\n");
  free(bytes);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_PRIVATE_RW_RX;
  opt.flags  = WRAITH_F_PEB_LINKAGE | WRAITH_F_RELIABILITY_ALL;
  opt.masquerade  = masquerade;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr,
  "FAIL: wraith_load_library(PEB_LINKAGE) -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  /* (1) The masquerade name must now appear in EnumProcessModulesEx. */
  HMODULE found = NULL;
  int hit = find_module_by_name(masquerade, &found);
  if (hit != 1) {
  fprintf(stderr, "FAIL: masquerade not enumerated after load\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: EnumProcessModulesEx lists \"%ls\" at base %p\n",
  masquerade, (void *)found);

  /* Sanity: the function still works while linked. */
  void *proc = NULL;
  rc = wraith_get_proc_address(h, "addNumbers", &proc);
  if (rc != WRAITH_OK || !proc || ((add_fn)proc)(8, 9) != 17) {
  fprintf(stderr,
  "FAIL: addNumbers via masqueraded module broken\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: masqueraded module is callable (addNumbers(8,9)=17)\n");

  rc = wraith_free_library(h);
  free(bytes);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_free_library -> %s\n",
  wraith_status_string(rc));
  return 1;
  }

  /* (2) Post-free: must NOT be in the enumeration anymore. */
  if (find_module_by_name(masquerade, NULL)) {
  fprintf(stderr,
  "FAIL: masquerade still enumerated after wraith_free_library\n");
  return 1;
  }
  printf("PASS: \"%ls\" removed from PEB.Ldr after free\n", masquerade);
  return 0;
}
