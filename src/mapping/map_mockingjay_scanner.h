/*
 * src/mapping/map_mockingjay_scanner.h
 *
 * Walk PEB.Ldr modules and find a contiguous `MEM_IMAGE + RWX`
 * region big enough for a payload. Some signed DLLs (msys-2.0,
 * specific MSI helper DLLs, certain SDK shims) ship with their
 * `.text` section marked `PAGE_EXECUTE_READWRITE` rather than
 * `PAGE_EXECUTE_READ`; reusing those skirts the "new RWX page"
 * IOC entirely.
 */

#ifndef WRAITH_MAP_MOCKINGJAY_SCANNER_H
#define WRAITH_MAP_MOCKINGJAY_SCANNER_H

#include "wraith/wraith_status.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_mockingjay_find_region(size_t needed_bytes,
  void **out_base,
  size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_MOCKINGJAY_SCANNER_H */
