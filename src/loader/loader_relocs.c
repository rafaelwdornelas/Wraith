/*
 * src/loader/loader_relocs.c
 *
 * Apply x64 base relocations (IMAGE_REL_BASED_DIR64). x86 HIGHLOW is
 * intentionally rejected - v2 is x64-only. ARM64 (THUMB_MOV32) lands in
 * v2.1 if scope permits.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "loader/loader_pipeline.h"
#include "pe/pe_constants.h"
#include "pe/pe_iter.h"

wraith_status_t wr_load_relocs_apply(struct wr_ctx *ctx,
  const wr_pe_view *src)
{
  if (!ctx || !src) {
  return WRAITH_E_NULL_ARG;
  }
  if (!wr_looks_like_valid_base(ctx->image_base) ||
      !wr_looks_like_valid_base(ctx->headers)) {
  return wr_ctx_fail(ctx, WRAITH_E_INVALID_HANDLE,
  "relocs: invalid image_base=%p / headers=%p",
  (void *)ctx->image_base, ctx->headers);
  }

  const wr_pe_nt_headers64 *nt =
  (const wr_pe_nt_headers64 *)ctx->headers;

  int64_t delta = (int64_t)((uintptr_t)ctx->image_base) -
  (int64_t)(src->nt->OptionalHeader.ImageBase);

  if (delta == 0) {
  ctx->is_relocated = 1;
  return WRAITH_OK;
  }

  /* Honor IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE only at the validation
  * layer; here we just check the directory exists. */
  uint32_t reloc_rva  = 0;
  uint32_t reloc_size = 0;
  (void)wr_pe_get_data_directory(src, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  if (reloc_size == 0) {
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_NOT_RELOCATABLE,
  "image not relocatable but base differs by %lld",
  (long long)delta);
  }
  (void)nt;  /* not strictly needed - we walk the in-image reloc dir */

  /* The reloc table in the *loaded image* lives at image_base + reloc_rva. */
  const uint8_t *block_base = ctx->image_base + reloc_rva;
  const uint8_t *block_end  = block_base + reloc_size;
  const wr_pe_base_relocation *block =
  (const wr_pe_base_relocation *)block_base;

  while ((const uint8_t *)block < block_end) {
  if (block->VirtualAddress == 0 || block->SizeOfBlock < 8) {
  break;
  }
  const uint16_t *entries = wr_pe_reloc_entries(block);
  uint32_t count = wr_pe_reloc_entry_count(block);

  uint8_t *patch_base = ctx->image_base + block->VirtualAddress;

  for (uint32_t i = 0; i < count; ++i) {
  uint16_t e = entries[i];
  uint16_t type  = wr_pe_reloc_entry_type(e);
  uint16_t offset = wr_pe_reloc_entry_offset(e);

  switch (type) {
  case WRAITH_PE_REL_ABSOLUTE:
  /* No-op padding entry. */
  break;
  case WRAITH_PE_REL_DIR64: {
  uint64_t *target = (uint64_t *)(patch_base + offset);
  *target = (uint64_t)((int64_t)*target + delta);
  break;
  }
  case WRAITH_PE_REL_HIGHLOW:
  /* x86 only - reject. */
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_BAD_TYPE,
  "x86 HIGHLOW relocation in x64 image");
  default:
  return wr_ctx_fail(ctx, WRAITH_E_RELOC_BAD_TYPE,
  "unsupported reloc type %u", type);
  }
  }

  block = (const wr_pe_base_relocation *)
  ((const uint8_t *)block + block->SizeOfBlock);
  }

  ctx->is_relocated = 1;
  return WRAITH_OK;
}
