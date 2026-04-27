/*
 * src/syscalls/sc_ssn_resolver.c
 *
 * Hell's Hall SSN resolution + Halo's Gate fallback. The SSN is the
 * dword that ntdll's Nt-prologue moves into EAX before issuing the
 * `syscall` instruction.
 *
 * Canonical x64 prologue:
 *
 *  00:  4c 8b d1  mov  r10, rcx
 *  03:  b8 XX XX 00 00  mov  eax, SSN
 *  08:  f6 04 25 08 03 fe 7f 01  test byte [SharedUserData+0x308], 1
 *  10:  75 03  jne  +3
 *  12:  0f 05  syscall
 *  14:  c3  ret
 *
 * If the first 8 bytes (mov r10,rcx + mov eax,SSN) are intact, the
 * SSN is at offset 4..5. If they aren't (typical EDR inline hook
 * replaces them with `jmp <thunk>`), Halo's Gate scans nearby Nt*
 * exports for an intact one and derives the target SSN by counting
 * neighbours - SSNs are assigned in ascending RVA order at boot.
 */

#include "syscalls/sc_ssn_resolver.h"
#include "core/wr_ptr_check.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

static int prologue_intact(const uint8_t *p)
{
  /* mov r10, rcx ; mov eax, imm32 (we don't pin the immediate) */
  return p[0] == 0x4c && p[1] == 0x8b && p[2] == 0xd1 && p[3] == 0xb8;
}

static uint32_t prologue_ssn(const uint8_t *p)
{
  /* dword at offset 4 - low 16 bits hold the SSN, high 16 are zero. */
  return (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
  ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
}

/* Halo's Gate: scan +/- N neighbour Nt* exports for one whose prologue
 * IS intact, then return its SSN +/- distance. Distance is in
 * "consecutive Nt* exports", which on real Windows correlates with
 * ascending SSN numbering. */
#define WRAITH_HALOS_GATE_MAX_DISTANCE 32

static wraith_status_t halos_gate(void *ntdll_base, void *target_func,
  uint32_t *out_ssn)
{
  if (!wr_looks_like_valid_base(ntdll_base) ||
      !wr_looks_like_valid_base(target_func) || !out_ssn) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  uint8_t *base = (uint8_t *)ntdll_base;
  PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  DWORD image_size = nt->OptionalHeader.SizeOfImage;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir->Size == 0 || dir->VirtualAddress == 0) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  PIMAGE_EXPORT_DIRECTORY exp =
  (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);

  /* Locate target_func's index in the address-of-functions table. */
  DWORD *func_rvas = (DWORD *)(base + exp->AddressOfFunctions);
  DWORD target_rva = (DWORD)((uintptr_t)target_func - (uintptr_t)base);
  long target_idx = -1;
  for (DWORD i = 0; i < exp->NumberOfFunctions; ++i) {
  if (func_rvas[i] == target_rva) {
  target_idx = (long)i;
  break;
  }
  }
  if (target_idx < 0) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }

  for (long d = 1; d <= WRAITH_HALOS_GATE_MAX_DISTANCE; ++d) {
  for (int sign = -1; sign <= 1; sign += 2) {
  long idx = target_idx + sign * d;
  if (idx < 0 || (DWORD)idx >= exp->NumberOfFunctions) {
  continue;
  }
  /* Skip empty / out-of-image RVA slots before deref. Real
  * Windows ntdll has empty slots (forwarders that resolve to
  * a string in the export directory at runtime). */
  DWORD rva = func_rvas[idx];
  if (rva == 0 || rva >= image_size) {
  continue;
  }
  uint8_t *cand = base + rva;
  if (prologue_intact(cand)) {
  uint32_t ssn = prologue_ssn(cand);
  /* SSN delta: +1 per +1 in idx (ascending RVA == ascending SSN). */
  long delta = -sign * d;
  long out = (long)ssn + delta;
  if (out < 0 || out > 0xffff) {
  continue;
  }
  *out_ssn = (uint32_t)out;
  return WRAITH_OK;
  }
  }
  }
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
}

/* ------------------------------------------------------------------------
 * FreshyCalls : SSN-by-RVA sort.
 *
 * The kernel assigns syscall numbers in ntdll-build-order, which on
 * disk lines up with ascending RVA in the export table. Sorting all
 * "Nt"-prefixed exports by RVA and looking up the target's index
 * yields the SSN without ever reading prologue bytes - immune to
 * inline hooks.
 *
 * The sort is rebuilt per call (cheap: ntdll has ~470 Nt-exports;
 * qsort over 8-byte structs is sub-millisecond). A future refinement
 * could memoize.
 * ------------------------------------------------------------------------ */

typedef struct rva_entry {
  uint32_t  rva;
  const char *name;
} rva_entry;

static int compare_by_rva(const void *a, const void *b)
{
  uint32_t ra = ((const rva_entry *)a)->rva;
  uint32_t rb = ((const rva_entry *)b)->rva;
  if (ra < rb) return -1;
  if (ra > rb) return 1;
  return 0;
}

wraith_status_t wr_sc_resolve_ssn_by_rva(void *ntdll_base, const char *name,
  uint32_t *out_ssn)
{
  if (!wr_looks_like_valid_base(ntdll_base) || !name || !out_ssn) {
  return WRAITH_E_NULL_ARG;
  }
  *out_ssn = 0;

  uint8_t *base = (uint8_t *)ntdll_base;
  PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  DWORD image_size = nt->OptionalHeader.SizeOfImage;
  PIMAGE_DATA_DIRECTORY dir =
  &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (dir->Size == 0 || dir->VirtualAddress == 0) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }
  PIMAGE_EXPORT_DIRECTORY exp =
  (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);
  if (exp->NumberOfNames == 0) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }

  DWORD *name_rvas = (DWORD *)(base + exp->AddressOfNames);
  WORD  *ord_rvas  = (WORD  *)(base + exp->AddressOfNameOrdinals);
  DWORD *func_rvas = (DWORD *)(base + exp->AddressOfFunctions);

  /* Filter: a real syscall export starts with "Nt" followed by an
  * UPPERCASE letter. The check `en[2] >= 'A' && en[2] <= 'Z'`
  * intentionally rejects "Ntdll*" exports (NtdllDefWindowProc_A/W,
  * NtdllDialogWndProc_A/W) - those are user-mode helpers with RVAs
  * inside ntdll but zero SSN; including them in the sorted list
  * shifts the index of every real syscall by their count, producing
  * off-by-N SSNs that dispatch to the wrong kernel function. */
  DWORD nt_count = 0;
  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
  DWORD nrva = name_rvas[i];
  if (nrva == 0 || nrva >= image_size) continue;
  const char *en = (const char *)(base + nrva);
  if (en[0] == 'N' && en[1] == 't' &&
      en[2] >= 'A' && en[2] <= 'Z') {
  ++nt_count;
  }
  }
  if (nt_count == 0) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }

  rva_entry *table = (rva_entry *)malloc(nt_count * sizeof(rva_entry));
  if (!table) {
  return WRAITH_E_OOM;
  }

  /* Second pass: collect RVAs + names. Same filter as above. */
  DWORD k = 0;
  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
  DWORD nrva = name_rvas[i];
  if (nrva == 0 || nrva >= image_size) continue;
  const char *en = (const char *)(base + nrva);
  if (en[0] == 'N' && en[1] == 't' &&
      en[2] >= 'A' && en[2] <= 'Z') {
  WORD ord = ord_rvas[i];
  if (ord < exp->NumberOfFunctions) {
  table[k].rva  = func_rvas[ord];
  table[k].name = en;
  ++k;
  }
  }
  }

  /* Sort by RVA - the index in this sorted table IS the SSN. */
  qsort(table, k, sizeof(rva_entry), compare_by_rva);

  /* Look up `name`. */
  wraith_status_t rc = WRAITH_E_SC_SSN_NOT_RESOLVED;
  for (DWORD i = 0; i < k; ++i) {
  if (strcmp(table[i].name, name) == 0) {
  *out_ssn = i;
  rc = WRAITH_OK;
  break;
  }
  }
  free(table);
  return rc;
}

wraith_status_t wr_sc_resolve_ssn(void *ntdll_base, const char *name,
  uint32_t *out_ssn)
{
  if (!wr_looks_like_valid_base(ntdll_base) || !name || !out_ssn) {
  return WRAITH_E_NULL_ARG;
  }
  *out_ssn = 0;

  /* Tier 1: FreshyCalls (RVA-sort, index == SSN).
  *
  * Promoted from tier 3 to tier 1 because it's the only method
  * provably immune to inline hooks: it never reads a single byte
  * of any Nt* prologue. Cost is one qsort over ~470 exports, which
  * happens once per init (8 calls amortised) - well under 1ms.
  *
  * Hell's Hall and Halo's Gate are kept as fallbacks for the
  * unlikely case where the export table has been tampered with
  * (e.g. names stripped). On a normal Win10/11 ntdll, FreshyCalls
  * always succeeds and the fallback chain never fires. */
  if (wr_sc_resolve_ssn_by_rva(ntdll_base, name, out_ssn) == WRAITH_OK) {
  return WRAITH_OK;
  }

  /* Locate the function so the fallback tiers can read its prologue. */
  void *func = NULL;
  wraith_status_t rc = wr_resolver_lookup_a(ntdll_base, name, &func);
  if (rc != WRAITH_OK || !wr_looks_like_valid_base(func)) {
  return WRAITH_E_SC_SSN_NOT_RESOLVED;
  }

  /* Tier 2 (fallback): prologue match (Hell's Hall). */
  const uint8_t *p = (const uint8_t *)func;
  if (prologue_intact(p)) {
  *out_ssn = prologue_ssn(p);
  return WRAITH_OK;
  }

  /* Tier 3 (last resort): Halo's Gate (neighbour walk). */
  return halos_gate(ntdll_base, func, out_ssn);
}
