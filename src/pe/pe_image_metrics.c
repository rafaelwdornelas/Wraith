/*
 * src/pe/pe_image_metrics.c
 *
 * Computes the image-level metrics derived from a validated PE view.
 * Pure - no allocation, no syscalls.
 */

#include "pe/pe_image_metrics.h"

#include <string.h>

static size_t align_up(size_t v, size_t alignment)
{
  if (alignment == 0) {
  return v;
  }
  return (v + alignment - 1) & ~(alignment - 1);
}

wraith_status_t wr_pe_compute_metrics(const wr_pe_view *view,
  uint32_t page_size,
  wr_pe_image_metrics *out)
{
  if (!view || !out || page_size == 0) {
  return WRAITH_E_NULL_ARG;
  }
  memset(out, 0, sizeof(*out));

  out->section_alignment = view->nt->OptionalHeader.SectionAlignment;
  out->file_alignment  = view->nt->OptionalHeader.FileAlignment;
  out->preferred_base  = view->nt->OptionalHeader.ImageBase;
  out->headers_size  = view->nt->OptionalHeader.SizeOfHeaders;

  /* Walk sections to find the highest VA in use. The validator already
  * checked that all sections fit within SizeOfImage. */
  size_t last_end = 0;
  for (uint16_t i = 0; i < view->section_count; ++i) {
  const wr_pe_section_header *s = &view->sections[i];
  uint32_t vsize = s->VirtualSize ? s->VirtualSize
  : view->nt->OptionalHeader.SectionAlignment;
  size_t end = (size_t)s->VirtualAddress + (size_t)vsize;
  if (end > last_end) {
  last_end = end;
  }
  }

  out->last_section_end  = align_up(last_end, page_size);
  out->aligned_image_size  = align_up(view->nt->OptionalHeader.SizeOfImage, page_size);

  if (out->aligned_image_size != align_up(out->last_section_end, page_size)) {
  /* If aligned image size disagrees with the last section end, the
  * headers are bogus and the loader cannot reserve memory safely. */
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  uint32_t reloc_rva = 0, reloc_size = 0;
  (void)wr_pe_get_data_directory(view, WRAITH_PE_DIR_BASERELOC,
  &reloc_rva, &reloc_size);
  out->has_relocations = (reloc_size > 0);

  return WRAITH_OK;
}
