/*
 * src/pe/pe_iter.c
 *
 * Implementation of section + base-relocation iterators. Both are zero-
 * allocation and bounds-checked at every step.
 */

#include "pe/pe_iter.h"

#include <stdint.h>

void wr_pe_section_iter_init(wr_pe_section_iter *it, const wr_pe_view *view)
{
  if (!it) {
  return;
  }
  it->view  = view;
  it->index = 0;
}

const wr_pe_section_header *wr_pe_section_iter_next(wr_pe_section_iter *it)
{
  if (!it || !it->view) {
  return NULL;
  }
  if (it->index >= it->view->section_count) {
  return NULL;
  }
  return &it->view->sections[it->index++];
}

wraith_status_t wr_pe_reloc_iter_init(wr_pe_reloc_iter *it, const wr_pe_view *view)
{
  if (!it || !view) {
  return WRAITH_E_NULL_ARG;
  }

  it->view  = view;
  it->current = NULL;
  it->end  = NULL;

  uint32_t reloc_rva  = 0;
  uint32_t reloc_size = 0;
  wraith_status_t rc = wr_pe_get_data_directory(view, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (reloc_size == 0) {
  /* No relocs - leave current/end NULL so iter_next returns NULL. */
  return WRAITH_OK;
  }

  /* The reloc data is referenced by RVA, but during validation we still
  * have the file (raw) layout. We need to find the raw offset that the
  * RVA maps to via the section table. */
  const uint8_t *raw = NULL;
  for (uint16_t i = 0; i < view->section_count; ++i) {
  const wr_pe_section_header *s = &view->sections[i];
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : view->nt->OptionalHeader.SectionAlignment;
  if (reloc_rva >= s->VirtualAddress && reloc_rva < s->VirtualAddress + vsize) {
  uint32_t delta = reloc_rva - s->VirtualAddress;
  if (delta + reloc_size > s->SizeOfRawData) {
  return WRAITH_E_PE_TRUNCATED;
  }
  raw = view->buffer + s->PointerToRawData + delta;
  break;
  }
  }

  if (!raw) {
  /* Some images store reloc directly in headers (rare). Fall back
  * to interpreting the RVA as a buffer offset. */
  if ((size_t)reloc_rva + reloc_size > view->buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  raw = view->buffer + reloc_rva;
  }

  it->current = (const wr_pe_base_relocation *)raw;
  it->end  = (const wr_pe_base_relocation *)(raw + reloc_size);
  return WRAITH_OK;
}

const wr_pe_base_relocation *wr_pe_reloc_iter_next(wr_pe_reloc_iter *it)
{
  if (!it || !it->current || it->current >= it->end) {
  return NULL;
  }
  const wr_pe_base_relocation *block = it->current;

  /* Sanity: a block with SizeOfBlock < header size is corrupt; bail
  * to NULL to terminate iteration. */
  if (block->SizeOfBlock < 8 || block->VirtualAddress == 0) {
  it->current = it->end;
  return NULL;
  }
  /* Advance. Block sizes are in bytes; bounds-check before stepping. */
  const uint8_t *next = (const uint8_t *)block + block->SizeOfBlock;
  if (next > (const uint8_t *)it->end) {
  it->current = it->end;
  } else {
  it->current = (const wr_pe_base_relocation *)next;
  }
  return block;
}
