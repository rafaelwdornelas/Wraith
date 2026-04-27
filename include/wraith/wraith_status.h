/*
 * include/wraith/wraith_status.h
 *
 * Rich error codes returned by every WRAITH_* entry point.
 *
 * Each error code maps deterministically to a category. Consumers can
 * switch on the category to decide retry vs. propagate without
 * enumerating every leaf code.
 */

#ifndef WRAITH_STATUS_H
#define WRAITH_STATUS_H

#include "wraith_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t wraith_status_t;

/* Top-level categories - the upper byte of wraith_status_t encodes the category. */
#define WRAITH_CAT(s)  (((s) >> 24) & 0xff)

#define WRAITH_CAT_OK  0x00
#define WRAITH_CAT_INVALID_ARG  0x01
#define WRAITH_CAT_PE_FORMAT  0x02
#define WRAITH_CAT_RESOURCE  0x03
#define WRAITH_CAT_MAPPING  0x04
#define WRAITH_CAT_RELOCATIONS  0x05
#define WRAITH_CAT_IMPORTS  0x06
#define WRAITH_CAT_EXPORTS  0x07
#define WRAITH_CAT_TLS  0x08
#define WRAITH_CAT_SEH  0x09
#define WRAITH_CAT_RUNTIME  0x0a
#define WRAITH_CAT_SYSCALL  0x0b
#define WRAITH_CAT_STEALTH  0x0c
#define WRAITH_CAT_FEATURE  0x0d
#define WRAITH_CAT_INTERNAL  0x0e

#define WRAITH_MAKE_STATUS(cat, sub)  (((wraith_status_t)(cat) << 24) | (wraith_status_t)(sub))

/* ============================ WRAITH_CAT_OK ============================ */
#define WRAITH_OK  WRAITH_MAKE_STATUS(WRAITH_CAT_OK,  0x00)

/* ============================ INVALID_ARG ============================ */
#define WRAITH_E_NULL_ARG  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x01)
#define WRAITH_E_INVALID_HANDLE  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x02)
#define WRAITH_E_INVALID_OPTIONS  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x03)
#define WRAITH_E_BUFFER_TOO_SMALL  WRAITH_MAKE_STATUS(WRAITH_CAT_INVALID_ARG, 0x04)

/* =============================== PE_FORMAT =========================== */
#define WRAITH_E_PE_TRUNCATED  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x01)
#define WRAITH_E_PE_BAD_DOS_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x02)
#define WRAITH_E_PE_BAD_NT_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x03)
#define WRAITH_E_PE_WRONG_MACHINE  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x04)
#define WRAITH_E_PE_BAD_OPT_MAGIC  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x05)
#define WRAITH_E_PE_BAD_ALIGNMENT  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x06)
#define WRAITH_E_PE_BAD_SECTION  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x07)
#define WRAITH_E_PE_SIZE_MISMATCH  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x08)
#define WRAITH_E_PE_OVERFLOW  WRAITH_MAKE_STATUS(WRAITH_CAT_PE_FORMAT,  0x09)

/* =============================== RESOURCE ============================ */
#define WRAITH_E_RES_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x01)
#define WRAITH_E_RES_TYPE_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x02)
#define WRAITH_E_RES_NAME_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x03)
#define WRAITH_E_RES_LANG_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_RESOURCE,  0x04)

/* =============================== MAPPING ============================== */
#define WRAITH_E_MAP_RESERVE_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x01)
#define WRAITH_E_MAP_COMMIT_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x02)
#define WRAITH_E_MAP_PROTECT_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x03)
#define WRAITH_E_MAP_NO_HOST_DLL  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x04)
#define WRAITH_E_MAP_HOST_TOO_SMALL  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x05)
#define WRAITH_E_MAP_RWX_LEAK  WRAITH_MAKE_STATUS(WRAITH_CAT_MAPPING,  0x06)

/* ============================= RELOCATIONS ============================ */
#define WRAITH_E_RELOC_NOT_RELOCATABLE  WRAITH_MAKE_STATUS(WRAITH_CAT_RELOCATIONS, 0x01)
#define WRAITH_E_RELOC_BAD_TYPE  WRAITH_MAKE_STATUS(WRAITH_CAT_RELOCATIONS, 0x02)

/* =============================== IMPORTS ============================== */
#define WRAITH_E_IMP_DLL_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x01)
#define WRAITH_E_IMP_PROC_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x02)
#define WRAITH_E_IMP_FORWARDER_LOOP  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x03)
#define WRAITH_E_IMP_DELAY_BAD_DESCR  WRAITH_MAKE_STATUS(WRAITH_CAT_IMPORTS,  0x04)

/* =============================== EXPORTS ============================== */
#define WRAITH_E_EXP_NOT_FOUND  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x01)
#define WRAITH_E_EXP_BAD_ORDINAL  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x02)
#define WRAITH_E_EXP_NO_TABLE  WRAITH_MAKE_STATUS(WRAITH_CAT_EXPORTS,  0x03)

/* ================================ TLS ================================= */
#define WRAITH_E_TLS_CALLBACK_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_TLS,  0x01)

/* ================================ SEH ================================= */
#define WRAITH_E_SEH_NO_PDATA  WRAITH_MAKE_STATUS(WRAITH_CAT_SEH,  0x01)
#define WRAITH_E_SEH_REGISTER_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_SEH,  0x02)

/* =============================== RUNTIME ============================== */
#define WRAITH_E_RT_PEB_WALK_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x01)
#define WRAITH_E_RT_API_NOT_RESOLVED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x02)
#define WRAITH_E_RT_DLLMAIN_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_RUNTIME,  0x03)

/* =============================== SYSCALL ============================== */
#define WRAITH_E_SC_SSN_NOT_RESOLVED  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x01)
#define WRAITH_E_SC_NO_GADGET  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x02)
#define WRAITH_E_SC_INVOKE_FAILED  WRAITH_MAKE_STATUS(WRAITH_CAT_SYSCALL,  0x03)

/* =============================== STEALTH ============================== */
#define WRAITH_E_STEALTH_INSTALL  WRAITH_MAKE_STATUS(WRAITH_CAT_STEALTH,  0x01)
#define WRAITH_E_STEALTH_INCOMPATIBLE  WRAITH_MAKE_STATUS(WRAITH_CAT_STEALTH,  0x02)

/* =============================== FEATURE ============================== */
#define WRAITH_E_FEATURE_DISABLED  WRAITH_MAKE_STATUS(WRAITH_CAT_FEATURE,  0x01)

/* =============================== INTERNAL ============================= */
#define WRAITH_E_OOM  WRAITH_MAKE_STATUS(WRAITH_CAT_INTERNAL,  0x01)
#define WRAITH_E_UNEXPECTED  WRAITH_MAKE_STATUS(WRAITH_CAT_INTERNAL,  0x02)

/* ----------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

/* Returns a static, NUL-terminated ASCII string for the given status. */
const char *wraith_status_string(wraith_status_t s);

/* Returns a human-readable label for the category byte. */
const char *wraith_category_string(int category);

#define WRAITH_SUCCESS(s)  ((s) == WRAITH_OK)
#define WRAITH_FAILED(s)  ((s) != WRAITH_OK)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_STATUS_H */
