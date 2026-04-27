/*
 * src/mapping/map_mockingjay_scanner.c
 *
 * Implementation. Walks PEB.Ldr.InMemoryOrderModuleList directly
 * (avoids `EnumProcessModules` so we don't introduce a psapi
 * dependency in the runtime path). For each module we walk
 * `VirtualQuery` from base to base+SizeOfImage, looking for a
 * contiguous span where every probed sub-region has
 * `Type == MEM_IMAGE` and `Protect & PAGE_EXECUTE_READWRITE`.
 *
 * Wine note: wine 9.0's standard module set generally does NOT
 * ship `.text` as `PAGE_EXECUTE_READWRITE`; the scanner will
 * usually return WRAITH_E_MAP_NO_HOST_DLL on a stock wine prefix.
 * Real Windows hosts where the technique applies (msys2 binaries,
 * some MSI installers) do have such regions. The integration test
 * skips gracefully when no candidate is present.
 */

#include "mapping/map_mockingjay_scanner.h"

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <winternl.h>

/* MinGW's <winternl.h> declares only InMemoryOrderModuleList; we
 * mirror the layout so we can reach the link list head and the
 * LDR_DATA_TABLE_ENTRY fields beyond it. Stable Win10 1809..Win11. */
typedef struct mj_peb_ldr_data {
  ULONG  Length;
  BOOLEAN  Initialized;
  PVOID  SsHandle;
  LIST_ENTRY  InLoadOrderModuleList;  /* +0x10 */
  LIST_ENTRY  InMemoryOrderModuleList;  /* +0x20 */
  LIST_ENTRY  InInitializationOrderModuleList;  /* +0x30 */
} mj_peb_ldr_data;

typedef struct mj_ldr_entry {
  LIST_ENTRY  InLoadOrderLinks;
  LIST_ENTRY  InMemoryOrderLinks;
  LIST_ENTRY  InInitializationOrderLinks;
  PVOID  DllBase;
  PVOID  EntryPoint;
  ULONG  SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
} mj_ldr_entry;

#define MJ_FROM_INMEM(p) \
  ((mj_ldr_entry *)((uint8_t *)(p) - offsetof(mj_ldr_entry, InMemoryOrderLinks)))

static int region_is_rwx_image(const MEMORY_BASIC_INFORMATION *mbi)
{
  if (mbi->State != MEM_COMMIT)  return 0;
  if (mbi->Type  != MEM_IMAGE)  return 0;
  /* Match if the page has ALL of read+write+execute set. */
  DWORD p = mbi->Protect & 0xFF;
  return (p == PAGE_EXECUTE_READWRITE) ||
  (p == PAGE_EXECUTE_WRITECOPY);
}

wraith_status_t wr_mockingjay_find_region(size_t needed_bytes,
  void **out_base,
  size_t *out_size)
{
  if (!out_base) {
  return WRAITH_E_NULL_ARG;
  }
  *out_base = NULL;
  if (out_size) *out_size = 0;
  if (needed_bytes == 0) {
  return WRAITH_E_INVALID_OPTIONS;
  }

  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb || !peb->Ldr) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  mj_peb_ldr_data *ldr = (mj_peb_ldr_data *)peb->Ldr;
  PLIST_ENTRY head = &ldr->InMemoryOrderModuleList;
  PLIST_ENTRY cur  = head->Flink;

  while (cur && cur != head) {
  mj_ldr_entry *e = MJ_FROM_INMEM(cur);
  cur = cur->Flink;
  if (!e->DllBase || e->SizeOfImage == 0) continue;

  uint8_t *mod_base = (uint8_t *)e->DllBase;
  uint8_t *mod_end  = mod_base + e->SizeOfImage;

  /* Walk every distinct VA range inside the module. */
  uint8_t *p = mod_base;
  while (p < mod_end) {
  MEMORY_BASIC_INFORMATION mbi = {0};
  if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
  break;
  }
  uint8_t *region_end = (uint8_t *)mbi.BaseAddress + mbi.RegionSize;
  if (region_is_rwx_image(&mbi) && mbi.RegionSize >= needed_bytes) {
  *out_base = mbi.BaseAddress;
  if (out_size) *out_size = (size_t)mbi.RegionSize;
  return WRAITH_OK;
  }
  if (region_end <= p) {
  break;  /* defensive */
  }
  p = region_end;
  }
  }

  return WRAITH_E_MAP_NO_HOST_DLL;
}
