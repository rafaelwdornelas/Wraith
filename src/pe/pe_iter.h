/*
 * src/pe/pe_iter.h
 *
 * Iterators over a validated wr_pe_view. Pure functions that yield
 * pointers into the source buffer - no allocation. Used by the loader's
 * section/reloc/import phases and by tests/introspection.
 */

#ifndef WRAITH_PE_ITER_H
#define WRAITH_PE_ITER_H

#include "wraith/wraith_status.h"
#include "pe/pe_validate.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Section iterator -------------------------------------------------------- */

typedef struct wr_pe_section_iter {
  const wr_pe_view  *view;
  uint16_t  index;
} wr_pe_section_iter;

void wr_pe_section_iter_init(wr_pe_section_iter *it, const wr_pe_view *view);

/* Returns next section header, or NULL when exhausted. */
const wr_pe_section_header *wr_pe_section_iter_next(wr_pe_section_iter *it);

/* Relocation block iterator ---------------------------------------------- */

typedef struct wr_pe_reloc_iter {
  const wr_pe_view  *view;
  const wr_pe_base_relocation  *current;  /* points into raw buffer */
  const wr_pe_base_relocation  *end;
} wr_pe_reloc_iter;

wraith_status_t wr_pe_reloc_iter_init(wr_pe_reloc_iter *it, const wr_pe_view *view);
const wr_pe_base_relocation *wr_pe_reloc_iter_next(wr_pe_reloc_iter *it);

/* Helpers for relocation entries within a block -------------------------- */

static inline const uint16_t *
wr_pe_reloc_entries(const wr_pe_base_relocation *block)
{
  return (const uint16_t *)((const uint8_t *)block + 8 /* sizeof(IMAGE_BASE_RELOCATION) */);
}

static inline uint32_t
wr_pe_reloc_entry_count(const wr_pe_base_relocation *block)
{
  if (block->SizeOfBlock < 8) {
  return 0;
  }
  return (block->SizeOfBlock - 8) / sizeof(uint16_t);
}

static inline uint16_t
wr_pe_reloc_entry_type(uint16_t entry)
{
  return (uint16_t)((entry >> 12) & 0xf);
}

static inline uint16_t
wr_pe_reloc_entry_offset(uint16_t entry)
{
  return (uint16_t)(entry & 0x0fff);
}

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_ITER_H */
