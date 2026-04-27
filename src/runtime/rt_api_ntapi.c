/*
 * src/runtime/rt_api_ntapi.c
 *
 * Alternative `wr_rt_ops` implementation that uses the PEB walker +
 * hash-based export resolver instead of LoadLibraryA / GetProcAddress.
 *
 * Activated when `WRAITH_F_API_HASHING` is set on the load options. This
 * vtable:
 *  1. Tries PEB.Ldr first (zero-API path, no hooks lit up).
 *  2. Falls back to LoadLibraryA - resolved itself via hash - when the
 *  requested DLL is not yet loaded.
 *
 * will wrap step 2's LoadLibraryA into an indirect syscall
 * sequence; for now we stay pure userland.
 *
 * Compiled out when WRAITH_USE_API_HASHING is OFF.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "runtime/rt_api.h"
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#if WRAITH_USE_API_HASHING

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#if __has_include("wr_api_hashes.h")
#  include "wr_api_hashes.h"
#else
/* Fallback constants when hashgen.py wasn't run; computed by hand from
 * the reference implementation. Keeps the build green even without a
 * Python interpreter on the build host. */
#  define WRAITH_H_kernel32_dll  0x7040ee75u
#  define WRAITH_H_LoadLibraryA  0x0666395bu
#  define WRAITH_H_FreeLibrary  0x7bcd0e7cu
#endif

typedef HMODULE (WINAPI *load_lib_a_fn)(LPCSTR);
typedef BOOL  (WINAPI *free_lib_fn)(HMODULE);

static load_lib_a_fn g_LoadLibraryA  = NULL;
static free_lib_fn  g_FreeLibrary  = NULL;

static wraith_status_t bootstrap_kernel32(void)
{
  if (g_LoadLibraryA && g_FreeLibrary) {
  return WRAITH_OK;
  }
  void *k32 = NULL;
  wraith_status_t rc = wr_pebwalk_find_module(WRAITH_H_kernel32_dll, &k32);
  if (rc != WRAITH_OK || !wr_looks_like_valid_base(k32)) {
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }
  void *p = NULL;
  rc = wr_resolver_lookup(k32, WRAITH_H_LoadLibraryA, &p);
  if (rc != WRAITH_OK || !wr_looks_like_valid_base(p)) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_LoadLibraryA = (load_lib_a_fn)p;
  rc = wr_resolver_lookup(k32, WRAITH_H_FreeLibrary, &p);
  if (rc != WRAITH_OK || !wr_looks_like_valid_base(p)) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  g_FreeLibrary = (free_lib_fn)p;
  return WRAITH_OK;
}

static wraith_status_t rtn_load_library(struct wr_ctx *ctx, const char *name,
  wraith_foreign_module_t *out)
{
  if (!ctx || !name || !out) {
  return WRAITH_E_NULL_ARG;
  }

  /* User-supplied callback always wins. */
  if (ctx->user_loadlib) {
  wraith_foreign_module_t m = ctx->user_loadlib(name, ctx->user_data);
  if (!wr_looks_like_valid_base(m)) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "user_loadlib(\"%s\") returned invalid base %p",
  name, (void *)m);
  }
  *out = m;
  return WRAITH_OK;
  }

  /* PEB.Ldr first - module already loaded? Validate the base before
  * accepting; placeholder entries can have DllBase = NULL or small. */
  void *base = NULL;
  if (wr_pebwalk_find_module(wr_djb2_a(name), &base) == WRAITH_OK
      && wr_looks_like_valid_base(base)) {
  *out = (wraith_foreign_module_t)base;
  return WRAITH_OK;
  }

  /* Not loaded yet - call LoadLibraryA, resolved through the same
  * hash mechanism so no plaintext "LoadLibraryA" appears in our IAT. */
  wraith_status_t rc = bootstrap_kernel32();
  if (rc != WRAITH_OK) {
  return wr_ctx_fail(ctx, rc,
  "rt_api_ntapi: bootstrap kernel32 failed");
  }

  HMODULE m = g_LoadLibraryA(name);
  if (!wr_looks_like_valid_base(m)) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "LoadLibraryA(\"%s\") returned invalid base %p",
  name, (void *)m);
  }
  *out = (wraith_foreign_module_t)m;
  return WRAITH_OK;
}

static wraith_status_t rtn_get_proc(struct wr_ctx *ctx, wraith_foreign_module_t m,
  const char *name, void **out_proc)
{
  if (!ctx || !name || !out_proc) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(m)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "rtn_get_proc: invalid module base %p", (void *)m);
  }
  if (ctx->user_getproc) {
  void *p = ctx->user_getproc(m, name, ctx->user_data);
  if (!p) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "user_getproc NULL");
  }
  *out_proc = p;
  return WRAITH_OK;
  }

  /* If `name` is actually an ordinal cast to (const char*), the upper
  * bits are zero (MAKEINTRESOURCE idiom). Route through the resolver's
  * ordinal entry point - it validates PE headers, follows forwarders
  * (including the "DLL.#NNN" form), and bound-checks the ordinal. */
  int by_ordinal = (((uintptr_t)name >> 16) == 0);
  wraith_status_t rc;
  if (by_ordinal) {
  rc = wr_resolver_lookup_ordinal(m, (uint16_t)(uintptr_t)name, out_proc);
  } else {
  rc = wr_resolver_lookup_a(m, name, out_proc);
  }
  if (rc != WRAITH_OK) {
  return rc;
  }

  /* Diagnostic post-condition: the resolved pointer must land on a page
   * the OS currently considers executable. The resolver's static checks
   * (forward_in_export_dir + rva_in_executable_section) are necessary
   * but not sufficient - they trust the section header's Characteristics
   * bit, which can disagree with the live VM mapping (e.g., api-set
   * forwarders whose strings live in a section header-marked executable
   * but page-actually NX, or third-party hooks that flipped a page to
   * RW for patching).
   *
   * VirtualQuery walks NtQueryVirtualMemory, which sees real VM state.
   * When the page isn't executable we refuse the resolution and capture
   * the exact import name + module + faulting address into err_context;
   * the caller (loader_imports.c) returns that error up to the user, who
   * can read it via wraith_last_error() instead of post-mortem-debugging
   * a 0xC0000005 with no symbol context. */
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T qb = VirtualQuery(*out_proc, &mbi, sizeof(mbi));
  if (qb >= sizeof(mbi)) {
  const DWORD exec_mask = PAGE_EXECUTE | PAGE_EXECUTE_READ
                        | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  if ((mbi.Protect & exec_mask) == 0) {
  void *bad = *out_proc;
  *out_proc = NULL;
  if (by_ordinal) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "resolver returned non-exec page %p for ord #%u in module %p (Protect=0x%lx, State=0x%lx, Type=0x%lx)",
  bad, (unsigned)(uintptr_t)name, (void *)m,
  (unsigned long)mbi.Protect,
  (unsigned long)mbi.State,
  (unsigned long)mbi.Type);
  }
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "resolver returned non-exec page %p for \"%s\" in module %p (Protect=0x%lx, State=0x%lx, Type=0x%lx)",
  bad, name, (void *)m,
  (unsigned long)mbi.Protect,
  (unsigned long)mbi.State,
  (unsigned long)mbi.Type);
  }
  }
  return WRAITH_OK;
}

static void rtn_free_library(struct wr_ctx *ctx, wraith_foreign_module_t m)
{
  if (!m) {
  return;
  }
  if (ctx && ctx->user_freelib) {
  ctx->user_freelib(m, ctx->user_data);
  return;
  }
  if (bootstrap_kernel32() == WRAITH_OK && g_FreeLibrary) {
  g_FreeLibrary((HMODULE)m);
  }
}

/* -------------------------------------------------------------------------
 * Memory primitives (Hell's Hall path).
 * ------------------------------------------------------------------------- */

#include "syscalls/sc_engine.h"

static int rtn_nt_alloc(void **addr, size_t *size,
  unsigned alloc_type, unsigned protect)
{
  SIZE_T sz = (SIZE_T)*size;
  NTSTATUS s = wr_sc_call_NtAllocateVirtualMemory(
  (HANDLE)-1, addr, 0, &sz, (ULONG)alloc_type, (ULONG)protect);
  *size = (size_t)sz;
  return (int)s;
}

static int rtn_nt_protect(void *addr, size_t size,
  unsigned new_protect, unsigned *old_protect)
{
  PVOID  p  = addr;
  SIZE_T sz = (SIZE_T)size;
  ULONG  old = 0;
  NTSTATUS s = wr_sc_call_NtProtectVirtualMemory(
  (HANDLE)-1, &p, &sz, (ULONG)new_protect, &old);
  if (old_protect) {
  *old_protect = (unsigned)old;
  }
  return (int)s;
}

static int rtn_nt_free(void *addr, size_t size, unsigned free_type)
{
  PVOID  p  = addr;
  SIZE_T sz = (SIZE_T)size;
  NTSTATUS s = wr_sc_call_NtFreeVirtualMemory(
  (HANDLE)-1, &p, &sz, (ULONG)free_type);
  return (int)s;
}

static void rtn_nt_flush_icache(void *addr, size_t size)
{
  (void)wr_sc_call_NtFlushInstructionCache((HANDLE)-1, addr, (SIZE_T)size);
}

const struct wr_rt_ops wr_rt_ops_ntapi = {
  .name  = "ntapi-hashed",
  .load_library  = rtn_load_library,
  .get_proc  = rtn_get_proc,
  .free_library  = rtn_free_library,
  .nt_alloc  = rtn_nt_alloc,
  .nt_protect  = rtn_nt_protect,
  .nt_free  = rtn_nt_free,
  .nt_flush_icache = rtn_nt_flush_icache,
};

#endif  /* WRAITH_USE_API_HASHING */
