/*
 * src/mapping/map_strategy.h
 *
 * Vtable interface implemented by every mapping strategy (private_rwx,
 * phantom_hollow, module_stomping, mockingjay).
 *
 * The loader only ever calls through this vtable; the concrete strategy
 * is selected by `map_dispatch.c` based on wraith_load_options.map_strategy.
 */

#ifndef WRAITH_MAP_STRATEGY_H
#define WRAITH_MAP_STRATEGY_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"
#include "wraith/wraith_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct wr_map_ops {
  const char *name;

  /* Reserve a contiguous virtual range of `size` bytes. The strategy
  * decides whether the range is MEM_PRIVATE, MEM_IMAGE, or overlaid
  * onto an existing module. *out_base receives the chosen base. */
  wraith_status_t (*reserve)(struct wr_ctx *ctx, size_t size, void **out_base);

  /* Make `size` bytes at `addr` writable so the loader can copy section
  * data into them. The actual final protection is applied by `protect`
  * after copy + relocations + imports. */
  wraith_status_t (*commit)(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot);

  /* Apply a final protection. Strategies that go through NtProtect must
  * never request RWX (the helper enforces this). */
  wraith_status_t (*protect)(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot);

  /* Release everything reserved/committed. Called from wraith_free_library
  * or on rollback after a pipeline failure. */
  wraith_status_t (*release)(struct wr_ctx *ctx);

  /* Strategy-specific cleanup of any auxiliary state (host handles,
  * stomp backups, etc.). May be NULL when there's nothing extra. */
  void  (*destroy)(struct wr_ctx *ctx);
};

/* Concrete strategies. Each lives in its own .c file. */
extern const struct wr_map_ops wr_map_ops_private_rwx;
#if WRAITH_USE_PHANTOM_HOLLOWING
extern const struct wr_map_ops wr_map_ops_phantom;
#endif
#if WRAITH_USE_MODULE_STOMPING
extern const struct wr_map_ops wr_map_ops_stomping;
#endif
/* hunt MEM_IMAGE+RWX regions in pre-existing modules. */
extern const struct wr_map_ops wr_map_ops_mockingjay;

/* Pick the appropriate vtable for the requested strategy. Returns NULL
 * if the strategy was compiled out (e.g. PHANTOM with WRAITH_USE_PHANTOM_HOLLOWING=OFF). */
const struct wr_map_ops *wr_map_resolve(wraith_map_strategy_t id);

#if WRAITH_USE_PHANTOM_HOLLOWING
/* Force phantom_is_blocked to report "blocked" for all subsequent
 * wr_map_resolve calls in this process. Used by the pipeline when an
 * actual ph_reserve attempt fails at runtime, so a later wraith_load_library
 * call is silently downgraded without retrying the doomed phantom path. */
void wr_phantom_mark_blocked(void);
#endif

/* Helpers shared across strategies. */

/* Convert wraith_prot_t to a Win32 PAGE_* constant, returning 0 if the value
 * is invalid. The helper rejects RWX combinations when WRAITH_RW_TO_RX_HYGIENE
 * is on, returning 0 in that case so callers can WRAITH_E_MAP_RWX_LEAK. */
unsigned wr_prot_to_win32(wraith_prot_t prot);

/* Convert a section's IMAGE_SCN_MEM_* characteristics to wraith_prot_t. */
wraith_prot_t wr_prot_from_section_chars(uint32_t characteristics);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_MAP_STRATEGY_H */
