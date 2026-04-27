/*
 * src/pe/pe_image_metrics.h
 *
 * Image-level metrics computed once after validation: aligned image size,
 * last section end, header size, etc. The loader uses these to make a
 * single allocation decision before sections are copied.
 */

#ifndef WRAITH_PE_IMAGE_METRICS_H
#define WRAITH_PE_IMAGE_METRICS_H

#include "wraith/wraith_status.h"
#include "pe/pe_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_pe_image_metrics {
  size_t  aligned_image_size;  /* SizeOfImage rounded to page_size */
  size_t  last_section_end;  /* highest VA + size, page-aligned */
  size_t  headers_size;  /* SizeOfHeaders */
  uint32_t section_alignment;
  uint32_t file_alignment;
  uint64_t preferred_base;  /* OptionalHeader.ImageBase */
  int  has_relocations;  /* 1 iff base reloc directory is non-empty */
} wr_pe_image_metrics;

wraith_status_t wr_pe_compute_metrics(const wr_pe_view *view,
  uint32_t page_size,
  wr_pe_image_metrics *out);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_IMAGE_METRICS_H */
