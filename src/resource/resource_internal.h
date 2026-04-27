/*
 * src/resource/resource_internal.h
 *
 * Shared helpers for the resource walker.
 */

#ifndef WRAITH_RESOURCE_INTERNAL_H
#define WRAITH_RESOURCE_INTERNAL_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve a resource entry by walking the three-level tree. Returns a
 * pointer to a valid IMAGE_RESOURCE_DATA_ENTRY (cast to void*) or NULL. */
PIMAGE_RESOURCE_DATA_ENTRY wr_resource_find_entry(struct wr_ctx *ctx,
  const void *name,
  const void *type,
  uint16_t language);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_RESOURCE_INTERNAL_H */
