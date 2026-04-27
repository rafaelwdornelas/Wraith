/*
 * src/stealth/host_iat/host_iat.c
 *
 * Walk PEB.Ldr modules; for each module, parse
 * IMAGE_DIRECTORY_ENTRY_IMPORT, walk every IMPORT_DESCRIPTOR's
 * FirstThunk array, and replace any thunk whose function pointer
 * equals `original` with `replacement`. Both bound and unbound
 * import tables are visited.
 *
 * Caveats:
 *  - Demote read-only IAT pages to RW for the patch and back to
 *  R afterward, respecting RW->RX hygiene at the type level.
 *  - Doesn't patch our own module's IAT (avoid corrupting the
 *  loader's own callers); the comparison `module == self` is
 *  made via the loaded base of `host_iat.c`.
 */

#include "stealth/host_iat/host_iat.h"

#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <winternl.h>

typedef struct wr_peb_ldr_data2 {
  ULONG  Length;
  BOOLEAN  Initialized;
  PVOID  SsHandle;
  LIST_ENTRY  InLoadOrderModuleList;
  LIST_ENTRY  InMemoryOrderModuleList;
  LIST_ENTRY  InInitializationOrderModuleList;
} wr_peb_ldr_data2;

typedef struct wr_ldr_entry2 {
  LIST_ENTRY  InLoadOrderLinks;
  LIST_ENTRY  InMemoryOrderLinks;
  LIST_ENTRY  InInitializationOrderLinks;
  PVOID  DllBase;
  PVOID  EntryPoint;
  ULONG  SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
} wr_ldr_entry2;

#define ENTRY_FROM_INMEM(p) \
  ((wr_ldr_entry2 *)((uint8_t *)(p) - offsetof(wr_ldr_entry2, InMemoryOrderLinks)))

static unsigned patch_module(void *base, void *original, void *replacement)
{
  if (!base) return 0;
  PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
  PIMAGE_NT_HEADERS64 nt =
  (PIMAGE_NT_HEADERS64)((uint8_t *)base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (dir->Size == 0) return 0;

  unsigned patched = 0;
  PIMAGE_IMPORT_DESCRIPTOR desc =
  (PIMAGE_IMPORT_DESCRIPTOR)((uint8_t *)base + dir->VirtualAddress);

  while (desc->Name != 0) {
  void **iat = (void **)((uint8_t *)base + desc->FirstThunk);
  while (*iat) {
  if (*iat == original) {
  DWORD old = 0;
  if (VirtualProtect(iat, sizeof(*iat),
  PAGE_READWRITE, &old)) {
  *iat = replacement;
  DWORD ignore = 0;
  VirtualProtect(iat, sizeof(*iat), old, &ignore);
  ++patched;
  }
  }
  ++iat;
  }
  ++desc;
  }
  return patched;
}

wraith_status_t wr_host_iat_redirect(void *original, void *replacement,
  unsigned *out_count)
{
#if WRAITH_USE_HOST_IAT_REDIRECT
  if (!original || !replacement) {
  return WRAITH_E_NULL_ARG;
  }
  if (out_count) *out_count = 0;

  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb || !peb->Ldr) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  wr_peb_ldr_data2 *ldr = (wr_peb_ldr_data2 *)peb->Ldr;
  PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
  PLIST_ENTRY cur  = head->Flink;

  /* Find the module that owns this very function so we don't
  * patch our own IAT (creates an infinite loop if the redirect
  * target also imports `original`). */
  HMODULE self = NULL;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
  | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
  (LPCWSTR)wr_host_iat_redirect, &self);

  unsigned total = 0;
  while (cur && cur != head) {
  wr_ldr_entry2 *e = ENTRY_FROM_INMEM(cur);
  cur = cur->Flink;
  if (e->DllBase && e->DllBase != (PVOID)self) {
  total += patch_module(e->DllBase, original, replacement);
  }
  }
  if (out_count) *out_count = total;
  return WRAITH_OK;
#else
  (void)original; (void)replacement;
  if (out_count) *out_count = 0;
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
