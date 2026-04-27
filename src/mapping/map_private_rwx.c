/*
 * src/mapping/map_private_rwx.c
 *
 * Default mapping strategy. Despite the legacy "PRIVATE_RWX" name, the
 * region transitions are strictly:
 *
 *  reserved (RW only) -> committed RW per section -> protected RX/R/RW
 *
 * No PAGE_EXECUTE_READWRITE is ever requested.
 *
 * indirection: every memory primitive routes through
 * `ctx->rt_ops->nt_*`. With the baseline rt_ops vtable that's a thin
 * wrapper around Win32 (VirtualAlloc, VirtualProtect, ...). With the
 * `ntapi-hashed` vtable it goes through the Hell's Hall syscall engine,
 * making the same code path stealth-capable without a strategy rewrite.
 */

#include "core/wr_context_internal.h"
#include "mapping/map_strategy.h"
#include "pe/pe_constants.h"
#include "runtime/rt_api.h"

#include <stdlib.h>
#include <windows.h>

typedef struct map_state {
  void  *base;
  size_t size;
} map_state;

static wraith_status_t pr_reserve(struct wr_ctx *ctx, size_t size, void **out_base)
{
  if (!ctx || !out_base || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  /* Try preferred ImageBase first (lets us skip relocations when the
  * preferred range is free). Then any address. */
  void *base = NULL;
  if (ctx->headers) {
  const wr_pe_nt_headers64 *nt = (const wr_pe_nt_headers64 *)ctx->headers;
  base = (void *)(uintptr_t)nt->OptionalHeader.ImageBase;
  }

  size_t request = size;
  int rc = ctx->rt_ops->nt_alloc(&base, &request,
  MEM_RESERVE | MEM_COMMIT,
  PAGE_READWRITE);
  if (rc != 0 || !base) {
  base = NULL;
  request = size;
  rc = ctx->rt_ops->nt_alloc(&base, &request,
  MEM_RESERVE | MEM_COMMIT,
  PAGE_READWRITE);
  }
  if (rc != 0 || !base) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RESERVE_FAILED,
  "rt_ops->nt_alloc(size=%zu) -> 0x%x",
  size, (unsigned)rc);
  }

  map_state *st = (map_state *)calloc(1, sizeof(map_state));
  if (!st) {
  size_t zero = 0;
  ctx->rt_ops->nt_free(base, zero, MEM_RELEASE);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "alloc map_state");
  }
  st->base = base;
  st->size = size;
  ctx->map_state = st;

  *out_base = base;
  return WRAITH_OK;
}

static wraith_status_t pr_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  /* The reserve already committed the whole image with PAGE_READWRITE.
  * Re-committing a sub-range is a no-op but lets the strategy
  * normalize sub-page edges if a future change requires it. */
  unsigned prot = wr_prot_to_win32(initial_prot ? initial_prot : WRAITH_PROT_RW);
  if (!prot) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "rejected RWX request in commit");
  }
  void  *p  = addr;
  size_t sz = size;
  int rc = ctx->rt_ops->nt_alloc(&p, &sz, MEM_COMMIT, prot);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_COMMIT_FAILED,
  "rt_ops->nt_alloc(MEM_COMMIT) -> 0x%x",
  (unsigned)rc);
  }
  return WRAITH_OK;
}

static wraith_status_t pr_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "rejected RWX request in protect");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "rt_ops->nt_protect(0x%p,%zu,0x%x) -> 0x%x",
  addr, size, w32, (unsigned)rc);
  }
  /* Flush ICache after any RX flip - cheap and avoids stale fetches
  * after relocations / IAT writes flipped the page from RW to RX. */
  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t pr_release(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  map_state *st = (map_state *)ctx->map_state;
  if (st && st->base && ctx->rt_ops) {
  size_t zero = 0;
  ctx->rt_ops->nt_free(st->base, zero, MEM_RELEASE);
  st->base = NULL;
  st->size = 0;
  }
  return WRAITH_OK;
}

static void pr_destroy(struct wr_ctx *ctx)
{
  if (!ctx) {
  return;
  }
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_private_rwx = {
  .name  = "private_rwx",
  .reserve = pr_reserve,
  .commit  = pr_commit,
  .protect = pr_protect,
  .release = pr_release,
  .destroy = pr_destroy,
};
