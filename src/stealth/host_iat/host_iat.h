/*
 * src/stealth/host_iat/host_iat.h
 *
 * patch the host process's IAT entries so calls to a named
 * import auto-route through a replacement function. Used to make
 * `Sleep` calls inside an arbitrary already-loaded module
 * transparently invoke `wraith_sleep` instead.
 */

#ifndef WRAITH_HOST_IAT_H
#define WRAITH_HOST_IAT_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Walk every loaded module's IMAGE_DIRECTORY_ENTRY_IMPORT and
 * replace any thunk pointing at `original` with `replacement`.
 * Returns the number of patched thunks via *out_count. */
wraith_status_t wr_host_iat_redirect(void *original, void *replacement,
  unsigned *out_count);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HOST_IAT_H */
