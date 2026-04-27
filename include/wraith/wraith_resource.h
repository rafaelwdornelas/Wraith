/*
 * include/wraith/wraith_resource.h
 *
 * Resource (.rsrc) walking API. Returns wraith_status_t for every entry
 * point (no NULL-handle convention).
 *
 * This header includes <windows.h> indirectly via LPCTSTR, because the
 * resource API has to interop with RT_*, MAKEINTRESOURCE, etc.
 * Consumers that want to avoid <windows.h> can stick to byte-level
 * wraith_find_resource + wraith_load_resource_data.
 */

#ifndef WRAITH_RESOURCE_H
#define WRAITH_RESOURCE_H

#include "wraith_status.h"
#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Default language constant - 0 means "any language" / neutral. */
#define WRAITH_LANG_DEFAULT  ((uint16_t)0)

/* -------------------------------------------------------------------------
 * Find a resource entry. `name` and `type` accept either an ASCII string
 * or an integer cast to (const char*) - same idiom as Win32 RT_RCDATA etc.
 *
 * `language` may be WRAITH_LANG_DEFAULT to use the calling thread's locale.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_find_resource(wraith_handle_t h,
  const void *name, const void *type,
  uint16_t language,
  wraith_resource_t *out);

/* -------------------------------------------------------------------------
 * Get the size of a previously-found resource, in bytes.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_sizeof_resource(wraith_handle_t h, wraith_resource_t r,
  size_t *out_size);

/* -------------------------------------------------------------------------
 * Get a pointer to the resource bytes. The pointer aliases into the
 * loaded image; do NOT free or modify it.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_resource_data(wraith_handle_t h, wraith_resource_t r,
  const void **out_data);

/* -------------------------------------------------------------------------
 * Load a string from STRINGTABLE by ID. `out_buffer` is wchar_t (UTF-16
 * little-endian as per Windows convention). `buf_chars` is the buffer
 * size in wchar_t units, and `out_chars` (optional, may be NULL) receives
 * the actual length copied (excluding the terminator).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_string(wraith_handle_t h, uint32_t id,
  uint16_t language,
  wchar_t *out_buffer, size_t buf_chars,
  size_t *out_chars);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RESOURCE_H */
