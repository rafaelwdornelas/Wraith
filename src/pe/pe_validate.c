/*
 * src/pe/pe_validate.c
 *
 * Bounds-checked PE validator. Every pointer dereference is gated by an
 * explicit size check; no integer arithmetic on offsets is performed
 * without an overflow guard. Designed to be fed arbitrary fuzz inputs.
 */

#include "pe/pe_validate.h"

#include <stdint.h>
#include <string.h>

/* Returns 1 iff a + b would overflow uint32_t. */
static int u32_add_overflows(uint32_t a, uint32_t b)
{
  return b > (uint32_t)0xffffffffu - a;
}

/* Returns 1 iff a + b would exceed `limit`. */
static int range_exceeds(size_t a, size_t b, size_t limit)
{
  if (b > limit) {
  return 1;
  }
  return a > limit - b;
}

static int is_power_of_two(uint32_t v)
{
  return v != 0 && (v & (v - 1)) == 0;
}

wraith_status_t wr_pe_validate(const void *buffer, size_t buffer_size,
  wr_pe_view *out)
{
  if (!buffer || !out) {
  return WRAITH_E_NULL_ARG;
  }
  memset(out, 0, sizeof(*out));

  if (buffer_size < sizeof(wr_pe_dos_header)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const uint8_t *bytes = (const uint8_t *)buffer;
  const wr_pe_dos_header *dos = (const wr_pe_dos_header *)bytes;

  if (dos->e_magic != WRAITH_PE_DOS_SIGNATURE) {
  return WRAITH_E_PE_BAD_DOS_MAGIC;
  }

  /* e_lfanew is 32-bit unsigned. Reject negative-as-unsigned values that
  * would overflow when we add sizeof(NT headers). */
  uint32_t nt_off = (uint32_t)dos->e_lfanew;
  if (nt_off < sizeof(wr_pe_dos_header)) {
  /* NT headers must follow the DOS header */
  return WRAITH_E_PE_OVERFLOW;
  }
  if (range_exceeds(nt_off, sizeof(wr_pe_nt_headers64), buffer_size)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const wr_pe_nt_headers64 *nt =
  (const wr_pe_nt_headers64 *)(bytes + nt_off);
  if (nt->Signature != WRAITH_PE_NT_SIGNATURE) {
  return WRAITH_E_PE_BAD_NT_MAGIC;
  }

  if (nt->FileHeader.Machine != WRAITH_PE_MACHINE_AMD64) {
  return WRAITH_E_PE_WRONG_MACHINE;
  }
  if (nt->OptionalHeader.Magic != WRAITH_PE_OPT_MAGIC_PE32PLUS) {
  return WRAITH_E_PE_BAD_OPT_MAGIC;
  }

  /* Section/file alignment must be powers of two and SectionAlignment
  * must be >= FileAlignment per the PE spec. */
  if (!is_power_of_two(nt->OptionalHeader.SectionAlignment) ||
  !is_power_of_two(nt->OptionalHeader.FileAlignment) ||
  nt->OptionalHeader.SectionAlignment < nt->OptionalHeader.FileAlignment) {
  return WRAITH_E_PE_BAD_ALIGNMENT;
  }

  /* SizeOfOptionalHeader from the file header must match what we expect
  * (NumberOfRvaAndSizes affects this; check at least the prefix is
  * accounted for). */
  if (nt->FileHeader.SizeOfOptionalHeader < sizeof(wr_pe_optional_header64)
  - sizeof(wr_pe_data_directory)
  * WRAITH_PE_DIR_COUNT) {
  return WRAITH_E_PE_BAD_OPT_MAGIC;
  }

  /* Locate the section table - it lives immediately after the optional
  * header (plus FileHeader). */
  uint32_t sec_off = nt_off
  + (uint32_t)offsetof(wr_pe_nt_headers64, OptionalHeader)
  + nt->FileHeader.SizeOfOptionalHeader;

  uint16_t sec_count = nt->FileHeader.NumberOfSections;
  if (sec_count == 0) {
  return WRAITH_E_PE_BAD_SECTION;
  }

  /* sec_off + sec_count * sizeof(section_header) must fit. */
  size_t sec_table_bytes = (size_t)sec_count * sizeof(wr_pe_section_header);
  if (range_exceeds(sec_off, sec_table_bytes, buffer_size)) {
  return WRAITH_E_PE_TRUNCATED;
  }

  const wr_pe_section_header *sections =
  (const wr_pe_section_header *)(bytes + sec_off);

  /* Sanity-walk every section header. We don't verify raw data here -
  * that's the loader's job during section copy. */
  uint32_t last_section_end = 0;
  for (uint16_t i = 0; i < sec_count; ++i) {
  const wr_pe_section_header *s = &sections[i];

  /* SizeOfRawData + PointerToRawData must fit in buffer (when raw
  * data is present). Bounds-check against integer overflow. */
  if (s->SizeOfRawData > 0) {
  if (u32_add_overflows(s->PointerToRawData, s->SizeOfRawData)) {
  return WRAITH_E_PE_OVERFLOW;
  }
  uint32_t end = s->PointerToRawData + s->SizeOfRawData;
  if ((size_t)end > buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  }

  /* VirtualAddress + max(VirtualSize, SizeOfRawData) bounded by
  * SizeOfImage. Use VirtualSize when nonzero, else SectionAlignment. */
  uint32_t vsize = s->VirtualSize;
  if (vsize == 0) {
  vsize = nt->OptionalHeader.SectionAlignment;
  }
  if (u32_add_overflows(s->VirtualAddress, vsize)) {
  return WRAITH_E_PE_OVERFLOW;
  }
  uint32_t vend = s->VirtualAddress + vsize;
  if (vend > last_section_end) {
  last_section_end = vend;
  }
  }

  if (last_section_end > nt->OptionalHeader.SizeOfImage) {
  /* Sections extend past SizeOfImage - reject. */
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  /* SizeOfHeaders must encompass the headers we just walked. */
  if (nt->OptionalHeader.SizeOfHeaders > buffer_size) {
  return WRAITH_E_PE_TRUNCATED;
  }
  if (nt->OptionalHeader.SizeOfHeaders < sec_off + sec_table_bytes) {
  return WRAITH_E_PE_SIZE_MISMATCH;
  }

  out->buffer  = bytes;
  out->buffer_size  = buffer_size;
  out->dos  = dos;
  out->nt  = nt;
  out->sections  = sections;
  out->section_count = sec_count;
  out->is_dll  = (nt->FileHeader.Characteristics & WRAITH_PE_FILE_DLL) != 0;
  out->is_executable = (nt->FileHeader.Characteristics & WRAITH_PE_FILE_EXECUTABLE) != 0;
  return WRAITH_OK;
}

wraith_status_t wr_pe_get_data_directory(const wr_pe_view *view,
  unsigned index,
  uint32_t *out_rva,
  uint32_t *out_size)
{
  if (!view || !view->nt || !out_rva || !out_size) {
  return WRAITH_E_NULL_ARG;
  }
  if (index >= WRAITH_PE_DIR_COUNT) {
  return WRAITH_E_NULL_ARG;
  }
  if (index >= view->nt->OptionalHeader.NumberOfRvaAndSizes) {
  *out_rva  = 0;
  *out_size = 0;
  return WRAITH_OK;
  }
  *out_rva  = view->nt->OptionalHeader.DataDirectory[index].VirtualAddress;
  *out_size = view->nt->OptionalHeader.DataDirectory[index].Size;
  return WRAITH_OK;
}
