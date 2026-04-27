/*
 * src/mapping/map_phantom_host_picker.h
 *
 * Selects a Microsoft-signed DLL from System32 large enough to host a
 * payload of `payload_size` bytes when mapped via SEC_IMAGE.
 */

#ifndef WRAITH_MAP_PHANTOM_HOST_PICKER_H
#define WRAITH_MAP_PHANTOM_HOST_PICKER_H

#include "wraith/wraith_status.h"

#include <stddef.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Find a host DLL at least `payload_size` bytes long.
 *
 * `out_path` receives a NUL-terminated full Windows path. `cap` is the
 * buffer length in wchar_t units (recommended: 260).
 *
 * On success returns S_OK and writes the path. The caller may also
 * pass a non-NULL `preferred` to test a specific candidate first
 * (used by tests). */
wraith_status_t wr_phantom_pick_host(size_t payload_size,
  const wchar_t *preferred,
  wchar_t *out_path, size_t cap);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_PHANTOM_HOST_PICKER_H */
