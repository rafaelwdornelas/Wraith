/*
 * src/mapping/map_dispatch.c
 *
 * Strategy selection and shared helpers. Keeping this in one place means
 * the loader pipeline doesn't carry #ifdefs over which strategies are
 * compiled in.
 */

#include "mapping/map_strategy.h"
#include "pe/pe_constants.h"

#include <windows.h>

const struct wr_map_ops *wr_map_resolve(wraith_map_strategy_t id)
{
  switch (id) {
  case WRAITH_MAP_PRIVATE_RW_RX:
  return &wr_map_ops_private_rwx;

#if WRAITH_USE_PHANTOM_HOLLOWING
  case WRAITH_MAP_PHANTOM_HOLLOW:
  return &wr_map_ops_phantom;
#endif

#if WRAITH_USE_MODULE_STOMPING
  case WRAITH_MAP_MODULE_STOMPING:
  return &wr_map_ops_stomping;
#endif

  case WRAITH_MAP_MOCKINGJAY:
  return &wr_map_ops_mockingjay;

  default:
  return NULL;
  }
}

unsigned wr_prot_to_win32(wraith_prot_t prot)
{
  unsigned base = 0;
  unsigned modifier = 0;

  if (prot & WRAITH_PROT_NOCACHE) {
  modifier |= PAGE_NOCACHE;
  }
  if (prot & WRAITH_PROT_GUARD) {
  modifier |= PAGE_GUARD;
  }

  switch (prot & 0xff) {
  case WRAITH_PROT_NOACCESS: base = PAGE_NOACCESS;  break;
  case WRAITH_PROT_R:  base = PAGE_READONLY;  break;
  case WRAITH_PROT_RW:  base = PAGE_READWRITE;  break;
  case WRAITH_PROT_RX:  base = PAGE_EXECUTE_READ;  break;
  case WRAITH_PROT_WC:  base = PAGE_WRITECOPY;  break;
  case WRAITH_PROT_RWC:  base = PAGE_WRITECOPY;  break; /* same flag in Win32 */
  case WRAITH_PROT_RXC:  base = PAGE_EXECUTE_WRITECOPY; break;
  default:
  /* Reject RWX combinations explicitly when RW_TO_RX_HYGIENE is on.
  * Currently we never even synthesize the bit, but be defensive. */
  return 0;
  }

  return base | modifier;
}

wraith_prot_t wr_prot_from_section_chars(uint32_t c)
{
  int execute = (c & WRAITH_PE_SCN_MEM_EXECUTE) != 0;
  int read  = (c & WRAITH_PE_SCN_MEM_READ)  != 0;
  int write  = (c & WRAITH_PE_SCN_MEM_WRITE)  != 0;

  /* RW_TO_RX hygiene: there is no RWX state. The loader must commit
  * sections RW, then flip RX after relocations + imports. The "final"
  * protection used here is what `protect` will apply once writes
  * are no longer needed. */
  wraith_prot_t p = WRAITH_PROT_NOACCESS;
  if (execute && read && !write) {
  p = WRAITH_PROT_RX;
  } else if (execute && read && write) {
  /* Forbidden in v2. Caller is expected to split this into a
  * post-write VirtualProtect to RX-only. We return RX so the
  * loader strips the write bit. */
  p = WRAITH_PROT_RX;
  } else if (!execute && read && write) {
  p = WRAITH_PROT_RW;
  } else if (!execute && read && !write) {
  p = WRAITH_PROT_R;
  } else if (execute && !read && !write) {
  p = WRAITH_PROT_RX;  /* read implied for execute on x64 */
  }

  if (c & WRAITH_PE_SCN_MEM_NOT_CACHED) {
  p |= WRAITH_PROT_NOCACHE;
  }
  return p;
}
