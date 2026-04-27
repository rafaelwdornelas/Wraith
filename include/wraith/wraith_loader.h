/*
 * include/wraith/wraith_loader.h
 *
 * Primary entry points. All return wraith_status_t; the produced handle (if
 * any) is written to an out-parameter. Pointers are typed and never aliased
 * with HMODULE / HANDLE / void* in the public surface.
 */

#ifndef WRAITH_LOADER_H
#define WRAITH_LOADER_H

#include "wraith_options.h"
#include "wraith_status.h"
#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Load a DLL or EXE from an in-memory PE buffer.
 *
 * `buffer`  : pointer to the start of the PE image (DOS header).
 * `size`  : length of `buffer` in bytes.
 * `options` : may be NULL - equivalent to passing a zero-initialized struct
 *  (no stealth features active).
 * `out`  : on WRAITH_OK, receives a non-NULL wraith_handle_t. On failure,
 *  *out is set to NULL.
 *
 * On error the error code categorizes which pipeline phase failed; callers
 * can look at wraith_last_error() for a free-form description.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_load_library(const void *buffer, size_t size,
  const wraith_load_options *options,
  wraith_handle_t *out);

/* -------------------------------------------------------------------------
 * Resolve an export by name or ordinal.
 *
 * Pass the ordinal value cast to (const char*) to look up by ordinal.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_get_proc_address(wraith_handle_t h, const char *name,
  void **out_proc);

/* -------------------------------------------------------------------------
 * Run an EXE entry point. Returns WRAITH_E_INVALID_HANDLE for DLL handles or
 * if the image had no entry point.
 *
 * The loaded EXE owns the process from this point. `out_exit_code` is
 * written only if the loaded EXE returns normally (rare for full EXEs).
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_call_entry_point(wraith_handle_t h, int *out_exit_code);

/* -------------------------------------------------------------------------
 * Release the loaded image. After this call the handle is invalid.
 *
 * For DLL handles: invokes DLL_PROCESS_DETACH, runs TLS DETACH callbacks
 * (when WRAITH_F_TLS_FULL_LIFECYCLE was set), unwinds RtlAddFunctionTable,
 * removes PEB.Ldr entry (if WRAITH_F_PEB_LINKAGE), restores the host's .text
 * (if MODULE_STOMPING), and finally releases all tracked allocations.
 *
 * Calling this with a NULL handle is a no-op and returns WRAITH_OK.
 * ------------------------------------------------------------------------- */
wraith_status_t wraith_free_library(wraith_handle_t h);

/* -------------------------------------------------------------------------
 * Diagnostic helpers - always available, even when WRAITH_DEBUG_LOG is off.
 * ------------------------------------------------------------------------- */

/* Returns the most recent free-form error description for the current
 * thread. The string is owned by the loader and is overwritten by the next
 * WRAITH_* call on the same thread. May be empty. */
const char *wraith_last_error(void);

/* Returns the version string ("Wraith <profile> <semver>"). */
const char *wraith_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_LOADER_H */
