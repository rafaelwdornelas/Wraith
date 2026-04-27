/*
 * src/pe/pe_validate.h
 *
 * Header bounds validation. The validator is the FIRST thing the loader
 * runs - everything downstream (section copy, relocations, imports) trusts
 * that the buffer's headers are well-formed.
 *
 * Design notes:
 *  - All checks are pure (no allocation, no syscalls). Runs identically
 *  in unit tests on Linux and inside a Windows process.
 *  - Returns rich wraith_status_t for every failure mode so fuzzers can
 *  classify crash inputs.
 *  - Robust against integer overflow (e_lfanew + sizeof(NT_HEADERS),
 *  section.PointerToRawData + section.SizeOfRawData, etc).
 */

#ifndef WRAITH_PE_VALIDATE_H
#define WRAITH_PE_VALIDATE_H

#include "wraith/wraith_status.h"
#include "pe/pe_constants.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Output of a successful validation. The pointers alias into the caller's
 * buffer - no copies are made. Lifetime equals the buffer's lifetime. */
typedef struct wr_pe_view {
  const uint8_t  *buffer;
  size_t  buffer_size;
  const wr_pe_dos_header  *dos;
  const wr_pe_nt_headers64  *nt;
  const wr_pe_section_header  *sections;  /* array of NumberOfSections */
  uint16_t  section_count;
  int  is_dll;
  int  is_executable;
} wr_pe_view;

/* Validate `buffer` and produce an `wr_pe_view`. The view is only valid
 * while `buffer` is alive. */
wraith_status_t wr_pe_validate(const void *buffer, size_t buffer_size,
  wr_pe_view *out);

/* Convenience: get a single data directory entry (returns ABSENT-like via
 * out_size==0 when the directory is empty). */
wraith_status_t wr_pe_get_data_directory(const wr_pe_view *view,
  unsigned index,
  uint32_t *out_rva,
  uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_VALIDATE_H */
