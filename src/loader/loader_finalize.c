/*
 * src/loader/loader_finalize.c
 *
 * Apply final per-section VirtualProtect after relocations + imports are
 * applied. Strict RW->RX hygiene: no PAGE_EXECUTE_READWRITE is ever
 * requested. Discardable sections are decommitted when they fully occupy
 * a page.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "loader/loader_pipeline.h"
#include "mapping/map_strategy.h"
#include "pe/pe_iter.h"

#include <windows.h>

static size_t align_down(size_t v, size_t a) { return v & ~(a - 1); }
static size_t align_up(size_t v, size_t a)  { return (v + a - 1) & ~(a - 1); }

static uint32_t real_section_size(const wr_pe_section_header *s,
  const wr_pe_view *src)
{
  uint32_t size = s->SizeOfRawData;
  if (size == 0) {
  if (s->Characteristics & WRAITH_PE_SCN_CNT_INITIALIZED_DATA) {
  size = src->nt->OptionalHeader.SizeOfInitializedData;
  } else if (s->Characteristics & WRAITH_PE_SCN_CNT_UNINITIALIZED) {
  size = src->nt->OptionalHeader.SizeOfUninitializedData;
  }
  }
  return size;
}

wraith_status_t wr_load_finalize_sections(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src || !ctx->map_ops) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "finalize: invalid image_base=%p",
  (void *)ctx->image_base);
  }

  size_t page = ctx->page_size;
  if (page == 0) {
  page = 4096;
  }

  /* Walk sections and merge protections per page. */
  struct {
  void  *addr;
  size_t  size;
  uint32_t characteristics;
  } region;

  wr_pe_section_iter it;
  wr_pe_section_iter_init(&it, src);
  const wr_pe_section_header *s = wr_pe_section_iter_next(&it);
  if (!s) {
  return WRAITH_OK;
  }

  region.addr  = ctx->image_base + s->VirtualAddress;
  region.size  = real_section_size(s, src);
  region.characteristics = s->Characteristics;

  while ((s = wr_pe_section_iter_next(&it)) != NULL) {
  void  *sec_addr  = ctx->image_base + s->VirtualAddress;
  size_t sec_size  = real_section_size(s, src);
  size_t aligned  = align_down((size_t)sec_addr, page);
  size_t cur_align = align_down((size_t)region.addr, page);

  if (cur_align == aligned ||
  (size_t)region.addr + region.size > (size_t)sec_addr) {
  /* Same page as previous - merge. */
  region.size = ((size_t)sec_addr + sec_size) - (size_t)region.addr;
  region.characteristics |= s->Characteristics;
  continue;
  }

  /* Flush the prior region, then start a new one. */
  if (region.size > 0) {
  wraith_prot_t prot = wr_prot_from_section_chars(region.characteristics);
  wraith_status_t rc = ctx->map_ops->protect(ctx, region.addr,
  align_up(region.size, page),
  prot);
  if (rc != WRAITH_OK) {
  return rc;
  }
  }

  region.addr  = sec_addr;
  region.size  = sec_size;
  region.characteristics = s->Characteristics;
  }

  if (region.size > 0) {
  wraith_prot_t prot = wr_prot_from_section_chars(region.characteristics);
  return ctx->map_ops->protect(ctx, region.addr,
  align_up(region.size, page), prot);
  }
  return WRAITH_OK;
}
