/*
 * src/runtime/rt_pebwalk.c
 *
 * Implementation of the PEB walker. Notes:
 *
 *  - On x64, the PEB is reachable via TEB.ProcessEnvironmentBlock at
 *  gs:[0x60]. We use the documented `NtCurrentTeb()` intrinsic so
 *  the code compiles cleanly on MinGW + MSVC + Clang-cl.
 *
 *  - `LDR_DATA_TABLE_ENTRY`'s public layout in <winternl.h> stops at
 *  `BaseDllName`; that's all we need. To stay forward-compatible we
 *  define our own minimal struct rather than depend on toolchain
 *  header layout.
 *
 *  - We walk InMemoryOrderModuleList rather than InLoadOrderModuleList
 *  because the head LIST_ENTRY pointer offset within the entry is
 *  stable across decades of Windows versions for InMemoryOrder, and
 *  because the OS loader uses this list internally.
 */

#include "runtime/rt_pebwalk.h"
#include "stealth/hashing/hash_djb2.h"

#include <windows.h>
#include <winternl.h>

/* Minimal LDR_DATA_TABLE_ENTRY - we only access fields up to BaseDllName.
 * Layout is stable Win10 1809 .. Win11 24H2. */
typedef struct wr_ldr_entry {
  LIST_ENTRY  InLoadOrderLinks;  /* +0x00 */
  LIST_ENTRY  InMemoryOrderLinks;  /* +0x10 */
  LIST_ENTRY  InInitializationOrderLinks; /* +0x20 */
  PVOID  DllBase;  /* +0x30 */
  PVOID  EntryPoint;  /* +0x38 */
  ULONG  SizeOfImage;  /* +0x40 */
  UNICODE_STRING FullDllName;  /* +0x48 */
  UNICODE_STRING BaseDllName;  /* +0x58 */
} wr_ldr_entry;

#define WRAITH_LDR_ENTRY_FROM_INMEMORY(p) \
  ((wr_ldr_entry *)((uint8_t *)(p) - offsetof(wr_ldr_entry, InMemoryOrderLinks)))

wraith_status_t wr_pebwalk_find_module(uint32_t name_hash, void **out_base)
{
  if (!out_base) {
  return WRAITH_E_NULL_ARG;
  }
  *out_base = NULL;

  PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
  if (!peb || !peb->Ldr) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }

  PLIST_ENTRY head =
  (PLIST_ENTRY)&((PPEB_LDR_DATA)peb->Ldr)->InMemoryOrderModuleList;
  PLIST_ENTRY cur = head->Flink;

  while (cur && cur != head) {
  wr_ldr_entry *e = WRAITH_LDR_ENTRY_FROM_INMEMORY(cur);
  if (e->BaseDllName.Buffer && e->BaseDllName.Length > 0) {
  size_t chars = e->BaseDllName.Length / sizeof(wchar_t);
  uint32_t h = wr_djb2_w_n(e->BaseDllName.Buffer, chars);
  if (h == name_hash) {
  /* Guard against placeholder / partially-initialised entries
  * where the BaseDllName matched but the image isn't actually
  * mapped. Anything below the lowest user-mode page (0x10000)
  * is either NULL or a bogus value the kernel never produces -
  * reporting OK with such a base crashes downstream consumers
  * that deref it expecting a valid PE image. */
  if (!e->DllBase || (uintptr_t)e->DllBase < 0x10000) {
  cur = cur->Flink;
  continue;
  }
  *out_base = e->DllBase;
  return WRAITH_OK;
  }
  }
  cur = cur->Flink;
  }
  return WRAITH_E_RT_PEB_WALK_FAILED;
}

wraith_status_t wr_pebwalk_find_module_a(const char *name, void **out_base)
{
  if (!name) {
  return WRAITH_E_NULL_ARG;
  }
  return wr_pebwalk_find_module(wr_djb2_a(name), out_base);
}
