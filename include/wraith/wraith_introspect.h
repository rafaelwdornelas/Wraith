/*
 * include/wraith/wraith_introspect.h
 *
 * Optional debug/introspection surface. Useful for tooling
 * (e.g. tools/ioc_audit.py) and tests that need to peek into the loaded
 * image without re-parsing the PE.
 *
 * Compiled out when WRAITH_DEBUG_LOG is OFF and WRAITH_BUILD_TESTS is OFF.
 */

#ifndef WRAITH_INTROSPECT_H
#define WRAITH_INTROSPECT_H

#include "wraith_status.h"
#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wr_section_info {
  char  name[9];  /* IMAGE_SIZEOF_SHORT_NAME + NUL */
  void  *base;  /* virtual address inside loaded image */
  size_t  size;  /* page-aligned size */
  uint32_t  characteristics;  /* IMAGE_SCN_* bits */
  wraith_prot_t  final_prot;  /* protection after FinalizeSections */
} wr_section_info;

typedef struct wr_export_info {
  const char *name;  /* may be NULL for ordinal-only exports */
  uint16_t  ordinal;
  void  *address;
  int  is_forwarded;  /* 1 if export is "DLL!Func" string */
  const char *forward_target;  /* valid when is_forwarded == 1 */
} wr_export_info;

typedef struct wr_import_info {
  const char *dll_name;
  const char *symbol_name;  /* NULL for ordinal-by-number imports */
  uint16_t  ordinal;
  void  *resolved_address;
} wr_import_info;

/* -------------------------------------------------------------------------
 * Iteration. The callback returns 0 to continue, non-zero to stop.
 * ------------------------------------------------------------------------- */

typedef int (*wr_section_visitor)(const wr_section_info *info, void *userdata);
typedef int (*wr_export_visitor)(const wr_export_info *info, void *userdata);
typedef int (*wr_import_visitor)(const wr_import_info *info, void *userdata);

wraith_status_t wraith_for_each_section(wraith_handle_t h, wr_section_visitor cb, void *userdata);
wraith_status_t wraith_for_each_export(wraith_handle_t  h, wr_export_visitor  cb, void *userdata);
wraith_status_t wraith_for_each_import(wraith_handle_t  h, wr_import_visitor  cb, void *userdata);

/* -------------------------------------------------------------------------
 * Lookup the loaded image base address (for callers that want to compute
 * RVAs themselves, e.g. tests that grep for RWX regions).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_get_image_base(wraith_handle_t h, void **out_base, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_INTROSPECT_H */
