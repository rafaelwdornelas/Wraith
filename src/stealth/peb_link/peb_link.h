/*
 * src/stealth/peb_link/peb_link.h
 *
 * PEB.Ldr linkage. Fabricates an `LDR_DATA_TABLE_ENTRY` for the loaded
 * image and inserts it into the three lists `EnumProcessModulesEx`,
 * `GetModuleHandleW`, and the OS loader iterate over.
 *
 * After install, the loaded module appears in module-enumeration tools
 * (Process Hacker, Get-Process | Format-List Modules, x64dbg) under
 * the masquerade name supplied in wraith_load_options.
 */

#ifndef WRAITH_PEB_LINK_H
#define WRAITH_PEB_LINK_H

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

wraith_status_t wr_peb_link_install(struct wr_ctx *ctx);
void  wr_peb_link_remove(struct wr_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PEB_LINK_H */
