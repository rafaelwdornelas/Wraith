/*
 * src/stealth/sleep/sleep.c
 *
 * Public wraith_sleep + dispatcher + section-protection reapply helper.
 *
 * wraith_sleep:
 *  - When WRAITH_F_SLEEP_OBFUSCATION is OFF in the load options, falls
 *  through to plain Sleep(ms) - the consumer just gets to sleep.
 *  - When ON, delegates to the algorithm selected at load time
 *  (ctx->sleep_algo). implements XOR; the EKKO/FOLIAGE/
 *  CRONOS aliases all route to the XOR path until swaps
 *  in their proper implementations.
 */

#include "core/wr_context_internal.h"
#include "mapping/map_strategy.h"
#include "wraith/wraith_stealth.h"
#include "wraith/wraith_types.h"
#include "pe/pe_constants.h"
#include "runtime/rt_api.h"
#include "stealth/sleep/sleep.h"

#include <windows.h>

wraith_status_t wraith_sleep(wraith_handle_t h, uint32_t duration_ms)
{
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) {
  return rc;
  }
#if WRAITH_USE_SLEEP_OBFUSCATION
  if (ctx->flags & WRAITH_F_SLEEP_OBFUSCATION) {
  return wr_sleep_obfuscate(ctx, duration_ms);
  }
#endif
  Sleep(duration_ms);
  return WRAITH_OK;
}

wraith_status_t wr_sleep_obfuscate(struct wr_ctx *ctx, uint32_t duration_ms)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  switch (ctx->sleep_algo) {
  case WRAITH_SLEEP_CRONOS:
  case WRAITH_SLEEP_FOLIAGE:
  /* Foliage's NtContinue-driven variant aliases to the
  * cronos timer-queue path - both keep the calling thread
  * parked in kernel during the encrypted window. */
  return wr_sleep_cronos_cycle(ctx, duration_ms);
  case WRAITH_SLEEP_EKKO:
  case WRAITH_SLEEP_XOR:
  default:
  return wr_sleep_xor_cycle(ctx, duration_ms);
  }
}

/* Walk our in-image section headers, derive each section's intended
 * wraith_prot_t from its Characteristics, and call rt_ops->nt_protect to
 * restore. This recreates the state FinalizeSections produced at
 * load time without needing the original PE source buffer. */
wraith_status_t wr_sleep_reapply_section_protections(struct wr_ctx *ctx)
{
  if (!ctx || !ctx->headers || !ctx->image_base || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_SECTION_HEADER s = IMAGE_FIRST_SECTION(nt);
  DWORD page = ctx->page_size ? ctx->page_size : 4096;

  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
  size_t vsize = s->Misc.VirtualSize ? s->Misc.VirtualSize
  : s->SizeOfRawData;
  if (vsize == 0) {
  continue;
  }
  /* Round up to page size so the protect call covers full pages. */
  size_t rounded = (vsize + page - 1) & ~(size_t)(page - 1);

  wraith_prot_t prot = wr_prot_from_section_chars(s->Characteristics);
  if (prot == WRAITH_PROT_NOACCESS) {
  continue;
  }
  unsigned w32 = wr_prot_to_win32(prot);
  if (!w32) {
  continue;
  }
  unsigned old = 0;
  (void)ctx->rt_ops->nt_protect(ctx->image_base + s->VirtualAddress,
  rounded, w32, &old);
  if (prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(ctx->image_base + s->VirtualAddress,
  rounded);
  }
  }
  return WRAITH_OK;
}
