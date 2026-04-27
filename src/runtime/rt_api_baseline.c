/*
 * src/runtime/rt_api_baseline.c
 *
 * Win32 baseline implementation of the runtime vtable. This is the
 * "obvious" path - it goes through LoadLibraryA / GetProcAddress and
 * therefore lights up every userland hook an EDR has installed. That's
 * intentional: the baseline exists so the upgrade path to indirect
 * syscalls is a vtable swap, not a loader rewrite.
 */

#include "core/wr_context_internal.h"
#include "runtime/rt_api.h"

#include <windows.h>

static wraith_status_t rt_load_library(struct wr_ctx *ctx,
  const char *name,
  wraith_foreign_module_t *out)
{
  if (!ctx || !name || !out) {
  return WRAITH_E_NULL_ARG;
  }

  /* User-provided callback wins, so consumers can intercept
  * dependency loads (e.g. to apply their own caching). */
  if (ctx->user_loadlib) {
  wraith_foreign_module_t m = ctx->user_loadlib(name, ctx->user_data);
  if (!m) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "user_loadlib(\"%s\") returned NULL", name);
  }
  *out = m;
  return WRAITH_OK;
  }

  HMODULE h = LoadLibraryA(name);
  if (!h) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_DLL_NOT_FOUND,
  "LoadLibraryA(\"%s\") failed: 0x%08lx",
  name, (unsigned long)GetLastError());
  }
  *out = (wraith_foreign_module_t)h;
  return WRAITH_OK;
}

static wraith_status_t rt_get_proc(struct wr_ctx *ctx,
  wraith_foreign_module_t module,
  const char *name,
  void **out_proc)
{
  if (!ctx || !module || !name || !out_proc) {
  return WRAITH_E_NULL_ARG;
  }

  if (ctx->user_getproc) {
  void *p = ctx->user_getproc(module, name, ctx->user_data);
  if (!p) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "user_getproc(\"%s\") returned NULL", name);
  }
  *out_proc = p;
  return WRAITH_OK;
  }

  FARPROC p = GetProcAddress((HMODULE)module, name);
  if (!p) {
  return wr_ctx_fail(ctx, WRAITH_E_IMP_PROC_NOT_FOUND,
  "GetProcAddress(\"%s\") failed: 0x%08lx",
  name, (unsigned long)GetLastError());
  }
  *out_proc = (void *)p;
  return WRAITH_OK;
}

static void rt_free_library(struct wr_ctx *ctx,
  wraith_foreign_module_t module)
{
  if (!module) {
  return;
  }
  if (ctx && ctx->user_freelib) {
  ctx->user_freelib(module, ctx->user_data);
  return;
  }
  FreeLibrary((HMODULE)module);
}

/* -------------------------------------------------------------------------
 * Memory primitives (Win32 path).
 * ------------------------------------------------------------------------- */

static int rt_nt_alloc(void **addr, size_t *size,
  unsigned alloc_type, unsigned protect)
{
  LPVOID p = VirtualAlloc(*addr, *size, alloc_type, protect);
  if (!p) {
  return (int)GetLastError();
  }
  *addr = p;
  return 0;
}

static int rt_nt_protect(void *addr, size_t size,
  unsigned new_protect, unsigned *old_protect)
{
  DWORD old = 0;
  if (!VirtualProtect(addr, size, new_protect, &old)) {
  return (int)GetLastError();
  }
  if (old_protect) {
  *old_protect = (unsigned)old;
  }
  return 0;
}

static int rt_nt_free(void *addr, size_t size, unsigned free_type)
{
  if (!VirtualFree(addr, size, free_type)) {
  return (int)GetLastError();
  }
  return 0;
}

static void rt_nt_flush_icache(void *addr, size_t size)
{
  FlushInstructionCache(GetCurrentProcess(), addr, size);
}

const struct wr_rt_ops wr_rt_ops_baseline = {
  .name  = "baseline-win32",
  .load_library  = rt_load_library,
  .get_proc  = rt_get_proc,
  .free_library  = rt_free_library,
  .nt_alloc  = rt_nt_alloc,
  .nt_protect  = rt_nt_protect,
  .nt_free  = rt_nt_free,
  .nt_flush_icache = rt_nt_flush_icache,
};
