/*
 * src/runtime/rt_resolver.c
 *
 * Hash-based export resolver. Walks the export directory of a loaded
 * module (`module_base` points at the IMAGE_DOS_HEADER), hashes each
 * exported name, and returns the address whose hash matches.
 *
 * Forwarder handling: an export entry is treated as a forwarder
 * ("DLL.Func" or "DLL.#NNN" string instead of code) when ANY of:
 *   1. its RVA falls inside the export directory range (the
 *      canonical Microsoft layout);
 *   2. its RVA falls in a PE section that lacks IMAGE_SCN_MEM_EXECUTE
 *      (catches api-set forwarders on Win11 24H2 ntdll whose strings
 *      live in .rdata outside the export dir);
 *   3. its live page protection (via VirtualQuery) lacks any EXECUTE
 *      bit - section header can disagree with the runtime VM mapping
 *      (per-page loader resets, third-party hooks, api-set proxy
 *      tables that re-map a stub page R-only).
 *
 * Any one criterion flips us into forwarder-parsing mode. Returning
 * a non-executable pointer would DEP-fault the caller on its first
 * indirect call (ExceptionCode 0xC0000005, ExceptionAddress ==
 * FaultAddress) - notoriously hard to attribute to the resolver
 * because the crash happens well after the resolver returns. The
 * runtime check guarantees the resolver either follows the chain
 * to a real executable target or returns an error - never a NX
 * pointer. We chase the chain by:
 *   - Resolving the target DLL via PEB.Ldr first (zero side effects).
 *   - Falling back to LoadLibraryA (hash-resolved from kernel32) when
 *     the target DLL is not in PEB.Ldr - this is the common case for
 *     api-set forwarders ("api-ms-win-core-*.dll"), which the Windows
 *     loader resolves via the schema mechanism rather than as real
 *     PE modules.
 *   - Following ordinal forwarders ("DLL.#NNN") via the ordinal table.
 *
 * Cycles bounded by WRAITH_RESOLVER_MAX_FOLLOW (8); real-world chains
 * never exceed 2-3 in practice.
 */

#include "runtime/rt_resolver.h"
#include "core/wr_ptr_check.h"
#include "runtime/rt_pebwalk.h"
#include "stealth/hashing/hash_djb2.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WRAITH_RESOLVER_MAX_FOLLOW 8

/* ------------------------------------------------------------------------ */
/*  LoadLibraryA fallback (for forwarders into not-yet-loaded modules)      */
/* ------------------------------------------------------------------------ */

typedef HMODULE (WINAPI *wr_load_lib_a_fn)(LPCSTR);
typedef FARPROC (WINAPI *wr_get_proc_addr_fn)(HMODULE, LPCSTR);

static wr_load_lib_a_fn  g_resolver_LoadLibraryA  = NULL;
static wr_get_proc_addr_fn g_resolver_GetProcAddr = NULL;

/* DJB2 hashes of "kernel32.dll" and "LoadLibraryA". Computed at compile
 * time and verified against the pre-existing constants in
 * rt_api_ntapi.c so any drift is caught by the CI build. The
 * GetProcAddress hash is computed at runtime in bootstrap_get_proc_addr -
 * the schema-aware fallback runs once per program lifetime so the
 * runtime hash cost is negligible. */
#define WR_RESOLVER_H_kernel32_dll   0x7040ee75u
#define WR_RESOLVER_H_LoadLibraryA   0x0666395bu

/* Forward decl - the lookup recurses, and so does the bootstrap path. */
static wraith_status_t lookup_inner(void *module_base, uint32_t name_hash,
                                    int depth, void **out_proc);
static wraith_status_t lookup_ordinal_inner(void *module_base, uint16_t ordinal,
                                             int depth, void **out_proc);

static wraith_status_t bootstrap_loadlibrary(void)
{
    if (g_resolver_LoadLibraryA) {
        return WRAITH_OK;
    }
    void *k32 = NULL;
    wraith_status_t rc =
        wr_pebwalk_find_module(WR_RESOLVER_H_kernel32_dll, &k32);
    if (rc != WRAITH_OK) {
        return rc;
    }
    void *p = NULL;
    rc = lookup_inner(k32, WR_RESOLVER_H_LoadLibraryA, 0, &p);
    if (rc != WRAITH_OK) {
        return rc;
    }
    g_resolver_LoadLibraryA = (wr_load_lib_a_fn)p;
    return WRAITH_OK;
}

/* Bootstrap GetProcAddress for the self-referencing api-set fallback path.
 * kernel32!GetProcAddress is a forwarder to KERNELBASE!GetProcAddress on
 * modern Win11 - that recursion bottoms out in 2 levels (KERNELBASE has
 * the real implementation as a non-forwarder export), so it never triggers
 * the self-loop path itself. */
static wraith_status_t bootstrap_get_proc_addr(void)
{
    if (g_resolver_GetProcAddr) {
        return WRAITH_OK;
    }
    void *k32 = NULL;
    wraith_status_t rc =
        wr_pebwalk_find_module(WR_RESOLVER_H_kernel32_dll, &k32);
    if (rc != WRAITH_OK) {
        return rc;
    }
    void *p = NULL;
    rc = lookup_inner(k32, wr_djb2_a("GetProcAddress"), 0, &p);
    if (rc != WRAITH_OK) {
        return rc;
    }
    g_resolver_GetProcAddr = (wr_get_proc_addr_fn)p;
    return WRAITH_OK;
}

/* Schema-aware fallback for self-referencing api-set forwarders.
 *
 * Background: on Win11, several kernel32 exports forward to api-set
 * pseudo-DLLs (e.g. "api-ms-win-core-processthreads-l1-1-0.dll"). The
 * Windows loader resolves api-sets via the per-process ApiSetMap in the
 * PEB; without that schema knowledge, our LoadLibraryA fallback may
 * return the SAME host module we started from (the api-set host on this
 * Windows build is kernel32 itself, not the implementation DLL). Recursing
 * through lookup_inner then re-finds the same forwarder string and loops
 * until WRAITH_RESOLVER_MAX_FOLLOW kicks us out with FORWARDER_LOOP.
 *
 * GetProcAddress, by contrast, calls into LdrpResolveForwardForGetProcAddress
 * which uses the schema and returns the real implementation. We pay a
 * small IOC cost here (one extra call into a well-known kernel32 export)
 * but only on this narrow self-loop path; non-looping forwarders still
 * resolve via the hash path. */
static wraith_status_t schema_aware_lookup(void *module_base, const char *func_name,
                                           uint16_t ordinal, int by_ordinal,
                                           void **out_proc)
{
    wraith_status_t rc = bootstrap_get_proc_addr();
    if (rc != WRAITH_OK) {
        return rc;
    }
    FARPROC fp = NULL;
    if (by_ordinal) {
        fp = g_resolver_GetProcAddr((HMODULE)module_base,
                                    (LPCSTR)(uintptr_t)ordinal);
    } else {
        fp = g_resolver_GetProcAddr((HMODULE)module_base, func_name);
    }
    if (!fp) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }
    *out_proc = (void *)fp;
    return WRAITH_OK;
}

static wraith_status_t resolve_dependency(const char *dll_name, void **out_base)
{
    if (!out_base || !dll_name) {
        return WRAITH_E_NULL_ARG;
    }
    *out_base = NULL;

    /* Try PEB.Ldr first - zero-API path. Treat OK-with-NULL as failure
     * (defensive: rt_pebwalk_find_module is supposed to filter those,
     * but we don't trust it on cold paths). */
    if (wr_pebwalk_find_module(wr_djb2_a(dll_name), out_base) == WRAITH_OK
        && *out_base != NULL) {
        return WRAITH_OK;
    }
    *out_base = NULL;

    /* Fall back to LoadLibraryA. The Windows loader resolves api-set
     * schema entries through this path, returning the actual host DLL
     * base. */
    wraith_status_t rc = bootstrap_loadlibrary();
    if (rc != WRAITH_OK) {
        return rc;
    }
    HMODULE m = g_resolver_LoadLibraryA(dll_name);
    if (!m) {
        return WRAITH_E_RT_PEB_WALK_FAILED;
    }
    *out_base = (void *)m;
    return WRAITH_OK;
}


/* ------------------------------------------------------------------------ */
/*  Image bounds helpers                                                    */
/* ------------------------------------------------------------------------ */

static int forward_in_export_dir(uint8_t *base,
                                  PIMAGE_DATA_DIRECTORY dir, void *p)
{
    if (dir->Size == 0) {
        return 0;
    }
    uintptr_t lo = (uintptr_t)(base + dir->VirtualAddress);
    uintptr_t hi = lo + dir->Size;
    uintptr_t v  = (uintptr_t)p;
    return v >= lo && v < hi;
}

/* Returns 1 if `rva` falls inside a PE section that has the
 * IMAGE_SCN_MEM_EXECUTE characteristic set; 0 otherwise.
 *
 * Used as a second forwarder-detection criterion: a few forwarder
 * strings (notably some api-set entries on Win11 24H2 ntdll) live
 * in .rdata outside the export-directory range. When
 * forward_in_export_dir() says "no" but the candidate RVA actually
 * points into a non-executable section, the bytes there are an
 * ASCII "DLL.Func" string, not real x64 code. Returning the literal
 * pointer would crash the caller on its first indirect call with
 * ExceptionCode == 0xC0000005 and ExceptionAddress == FaultAddress
 * pointing at the .rdata page (DEP/NX fault).
 *
 * Returns 0 (defensive "not executable") in failure modes:
 *   - PE headers don't parse
 *   - The RVA falls outside every section (likely garbage)
 *   - The matching section lacks IMAGE_SCN_MEM_EXECUTE
 */
static int rva_in_executable_section(uint8_t *base, DWORD rva)
{
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    PIMAGE_NT_HEADERS64 nt =
        (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    PIMAGE_SECTION_HEADER sec = (PIMAGE_SECTION_HEADER)(
        (uint8_t *)&nt->OptionalHeader
        + nt->FileHeader.SizeOfOptionalHeader);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        DWORD lo = sec->VirtualAddress;
        DWORD vsz = sec->Misc.VirtualSize ? sec->Misc.VirtualSize
                                           : sec->SizeOfRawData;
        DWORD hi = lo + vsz;
        if (rva >= lo && rva < hi) {
            return (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)
                       ? 1 : 0;
        }
    }
    return 0;
}

/* Returns 1 iff `p` lives on a page the OS *currently* treats as
 * executable. Used as a third forwarder-detection criterion to catch
 * the case where the section header's IMAGE_SCN_MEM_EXECUTE bit
 * disagrees with the live VM mapping - e.g. an api-set forwarder whose
 * string is in a section the loader marked R-only at runtime, or a
 * page that a third-party hook flipped to RW for patching. The static
 * section walk would say "executable", but the indirect call would
 * still DEP-fault.
 *
 * Returns 0 in any failure mode (VirtualQuery error, MEM_FREE, no
 * EXECUTE bit). The resolver treats 0 as "looks like a forwarder
 * string" and falls into the chase path, which either follows the
 * forwarder successfully or returns a clean error - never a NX
 * pointer that crashes the caller. */
static int page_is_executable(const void *p)
{
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T qb = VirtualQuery(p, &mbi, sizeof(mbi));
    if (qb < sizeof(mbi)) {
        return 0;
    }
    if (mbi.State != MEM_COMMIT) {
        return 0;
    }
    const DWORD exec_mask = PAGE_EXECUTE | PAGE_EXECUTE_READ
                          | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & exec_mask) ? 1 : 0;
}

/* Bounded strchr: scans at most `cap` bytes starting at `s` for `c`,
 * stopping at NUL. Returns NULL if not found within bounds. Used for
 * forwarder strings where the buffer is bounded by the export dir
 * size and we can't trust the data to be NUL-terminated. */
static const char *bounded_strchr(const char *s, char c, size_t cap)
{
    for (size_t i = 0; i < cap; ++i) {
        if (s[i] == '\0') return NULL;
        if (s[i] == c) return s + i;
    }
    return NULL;
}

static size_t bounded_strlen(const char *s, size_t cap)
{
    for (size_t i = 0; i < cap; ++i) {
        if (s[i] == '\0') return i;
    }
    return cap;
}

/* ------------------------------------------------------------------------ */
/*  Core lookup - by hash                                                   */
/* ------------------------------------------------------------------------ */

static wraith_status_t lookup_inner(void *module_base, uint32_t name_hash,
                                    int depth, void **out_proc)
{
    if (depth >= WRAITH_RESOLVER_MAX_FOLLOW) {
        return WRAITH_E_IMP_FORWARDER_LOOP;
    }
    if (!out_proc) {
        return WRAITH_E_NULL_ARG;
    }
    *out_proc = NULL;
    if (!wr_looks_like_valid_base(module_base)) {
        return WRAITH_E_NULL_ARG;
    }

    uint8_t *base = (uint8_t *)module_base;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    DWORD image_size = nt->OptionalHeader.SizeOfImage;
    PIMAGE_DATA_DIRECTORY dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->Size == 0 || dir->VirtualAddress == 0) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }

    PIMAGE_EXPORT_DIRECTORY exp =
        (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);
    if (exp->NumberOfNames == 0) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }

    DWORD *name_rvas = (DWORD *)(base + exp->AddressOfNames);
    WORD  *ord_rvas  = (WORD  *)(base + exp->AddressOfNameOrdinals);
    DWORD *func_rvas = (DWORD *)(base + exp->AddressOfFunctions);

    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        DWORD name_rva = name_rvas[i];
        if (name_rva == 0 || name_rva >= image_size) {
            /* Defensive: malformed export table. Skip, don't deref. */
            continue;
        }
        const char *name = (const char *)(base + name_rva);
        if (wr_djb2_a(name) != name_hash) {
            continue;
        }
        WORD ordinal = ord_rvas[i];
        if (ordinal >= exp->NumberOfFunctions) {
            return WRAITH_E_EXP_BAD_ORDINAL;
        }
        DWORD frva = func_rvas[ordinal];
        if (frva == 0 || frva >= image_size) {
            return WRAITH_E_RT_API_NOT_RESOLVED;
        }

        void *candidate = base + frva;

        /* Forwarder detection. Three-criteria:
         *   (1) candidate RVA falls within the export directory range
         *       (the canonical Microsoft layout - "DLL.Func" strings
         *       live alongside the export tables).
         *   (2) candidate RVA falls in a non-executable section. Some
         *       api-set forwarders on Win11 24H2 ntdll place their
         *       strings in .rdata outside the export-dir range; the
         *       RVA is still valid, but treating it as a function
         *       pointer would DEP-fault on the first indirect call.
         *   (3) candidate's live page protection lacks any EXECUTE
         *       bit. The static section walk in (2) trusts the on-disk
         *       Characteristics flag, which can disagree with the
         *       runtime VM mapping (loader-applied per-section reset,
         *       third-party hooks flipping pages to RW, api-set proxy
         *       tables, etc.). VirtualQuery sees real state.
         *
         * Any condition flips us into forwarder-parsing mode. The
         * later criteria short-circuit when an earlier one matches
         * (cheap path stays cheap on canonical Microsoft layout). */
        int is_forwarder = forward_in_export_dir(base, dir, candidate)
                           || !rva_in_executable_section(base, frva)
                           || !page_is_executable(candidate);

        if (is_forwarder) {
            const char *fwd = (const char *)candidate;
            /* Forwarder strings normally live inside the export dir;
             * bound the search by whichever cap is tighter so a
             * malformed entry can't walk past the section. */
            uintptr_t hi = (uintptr_t)(base + dir->VirtualAddress + dir->Size);
            if ((uintptr_t)fwd >= hi) {
                /* Out-of-export-dir forwarder (criterion 2 path). Fall
                 * back to image-size as the upper bound; bounded_strchr
                 * will stop at the first NUL anyway. */
                hi = (uintptr_t)(base + image_size);
            }
            size_t cap = (size_t)(hi - (uintptr_t)fwd);
            const char *dot = bounded_strchr(fwd, '.', cap);
            if (!dot || dot == fwd || dot[1] == '\0') {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }

            /* Build "<dll>.dll" for module lookup. */
            size_t dll_part = (size_t)(dot - fwd);
            if (dll_part > 64) {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }
            char fname[80];
            memcpy(fname, fwd, dll_part);
            memcpy(fname + dll_part, ".dll", 5);  /* incl. NUL */

            /* Resolve target DLL: PEB.Ldr first, LoadLibraryA fallback
             * (handles api-set schema entries). */
            void *dep = NULL;
            wraith_status_t rc = resolve_dependency(fname, &dep);
            if (rc != WRAITH_OK) {
                return rc;
            }

            /* Self-referencing api-set: LoadLibraryA returned the same
             * module we started from (e.g. kernel32 hosts an api-set whose
             * host is kernel32 again). Recursing would re-find the same
             * forwarder string and exhaust depth. Fall back to OS
             * GetProcAddress, which uses the api-set schema. */
            if (dep == module_base) {
                if (dot[1] == '#') {
                    const char *p = dot + 2;
                    size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                    uint32_t ord_val = 0;
                    for (size_t k = 0; k < plen; ++k) {
                        if (p[k] < '0' || p[k] > '9' || ord_val > 0xFFFFu) {
                            return WRAITH_E_IMP_FORWARDER_LOOP;
                        }
                        ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                    }
                    return schema_aware_lookup(dep, NULL, (uint16_t)ord_val,
                                                1, out_proc);
                }
                return schema_aware_lookup(dep, dot + 1, 0, 0, out_proc);
            }

            if (dot[1] == '#') {
                /* Ordinal forwarder: "DLL.#NNN". Parse the integer
                 * (decimal, bounded) and recurse via ordinal. */
                const char *p = dot + 2;
                size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                if (plen == 0 || plen > 6) {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
                uint32_t ord_val = 0;
                for (size_t k = 0; k < plen; ++k) {
                    if (p[k] < '0' || p[k] > '9') {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                    ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                    if (ord_val > 0xFFFFu) {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                }
                return lookup_ordinal_inner(dep, (uint16_t)ord_val,
                                             depth + 1, out_proc);
            }

            uint32_t func_hash = wr_djb2_a(dot + 1);
            return lookup_inner(dep, func_hash, depth + 1, out_proc);
        }

        *out_proc = candidate;
        return WRAITH_OK;
    }

    return WRAITH_E_RT_API_NOT_RESOLVED;
}

/* ------------------------------------------------------------------------ */
/*  Core lookup - by ordinal                                                */
/* ------------------------------------------------------------------------ */

static wraith_status_t lookup_ordinal_inner(void *module_base, uint16_t ordinal,
                                             int depth, void **out_proc)
{
    if (depth >= WRAITH_RESOLVER_MAX_FOLLOW) {
        return WRAITH_E_IMP_FORWARDER_LOOP;
    }
    if (!out_proc) {
        return WRAITH_E_NULL_ARG;
    }
    *out_proc = NULL;
    if (!wr_looks_like_valid_base(module_base)) {
        return WRAITH_E_NULL_ARG;
    }

    uint8_t *base = (uint8_t *)module_base;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    DWORD image_size = nt->OptionalHeader.SizeOfImage;
    PIMAGE_DATA_DIRECTORY dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir->Size == 0) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }
    PIMAGE_EXPORT_DIRECTORY exp =
        (PIMAGE_EXPORT_DIRECTORY)(base + dir->VirtualAddress);

    if (ordinal < exp->Base) {
        return WRAITH_E_EXP_BAD_ORDINAL;
    }
    DWORD idx = (DWORD)(ordinal - exp->Base);
    if (idx >= exp->NumberOfFunctions) {
        return WRAITH_E_EXP_BAD_ORDINAL;
    }
    DWORD *funcs = (DWORD *)(base + exp->AddressOfFunctions);
    DWORD frva = funcs[idx];
    if (frva == 0 || frva >= image_size) {
        return WRAITH_E_IMP_PROC_NOT_FOUND;
    }

    void *candidate = base + frva;

    /* Forwarder detection identical to the by-name path. See
     * lookup_inner() for the full three-criteria rationale (export-dir
     * range, static section EXECUTE bit, live page protection). */
    int is_forwarder = forward_in_export_dir(base, dir, candidate)
                       || !rva_in_executable_section(base, frva)
                       || !page_is_executable(candidate);

    if (is_forwarder) {
        const char *fwd = (const char *)candidate;
        uintptr_t hi = (uintptr_t)(base + dir->VirtualAddress + dir->Size);
        if ((uintptr_t)fwd >= hi) {
            /* Out-of-export-dir forwarder; widen the search cap to the
             * end of the image. bounded_strchr stops at the first NUL. */
            hi = (uintptr_t)(base + image_size);
        }
        size_t cap = (size_t)(hi - (uintptr_t)fwd);
        const char *dot = bounded_strchr(fwd, '.', cap);
        if (!dot || dot == fwd || dot[1] == '\0') {
            return WRAITH_E_IMP_FORWARDER_LOOP;
        }
        size_t dll_part = (size_t)(dot - fwd);
        if (dll_part > 64) {
            return WRAITH_E_IMP_FORWARDER_LOOP;
        }
        char fname[80];
        memcpy(fname, fwd, dll_part);
        memcpy(fname + dll_part, ".dll", 5);

        void *dep = NULL;
        wraith_status_t rc = resolve_dependency(fname, &dep);
        if (rc != WRAITH_OK) {
            return rc;
        }

        /* Self-referencing api-set self-host (see lookup_inner for rationale). */
        if (dep == module_base) {
            if (dot[1] == '#') {
                const char *p = dot + 2;
                size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
                uint32_t ord_val = 0;
                for (size_t k = 0; k < plen; ++k) {
                    if (p[k] < '0' || p[k] > '9' || ord_val > 0xFFFFu) {
                        return WRAITH_E_IMP_FORWARDER_LOOP;
                    }
                    ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                }
                return schema_aware_lookup(dep, NULL, (uint16_t)ord_val,
                                            1, out_proc);
            }
            return schema_aware_lookup(dep, dot + 1, 0, 0, out_proc);
        }

        if (dot[1] == '#') {
            const char *p = dot + 2;
            size_t plen = bounded_strlen(p, cap - (size_t)(p - fwd));
            if (plen == 0 || plen > 6) {
                return WRAITH_E_IMP_FORWARDER_LOOP;
            }
            uint32_t ord_val = 0;
            for (size_t k = 0; k < plen; ++k) {
                if (p[k] < '0' || p[k] > '9') {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
                ord_val = ord_val * 10 + (uint32_t)(p[k] - '0');
                if (ord_val > 0xFFFFu) {
                    return WRAITH_E_IMP_FORWARDER_LOOP;
                }
            }
            return lookup_ordinal_inner(dep, (uint16_t)ord_val,
                                         depth + 1, out_proc);
        }

        uint32_t func_hash = wr_djb2_a(dot + 1);
        return lookup_inner(dep, func_hash, depth + 1, out_proc);
    }

    *out_proc = candidate;
    return WRAITH_OK;
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

wraith_status_t wr_resolver_lookup(void *module_base, uint32_t name_hash,
                                    void **out_proc)
{
    return lookup_inner(module_base, name_hash, 0, out_proc);
}

wraith_status_t wr_resolver_lookup_a(void *module_base, const char *name,
                                      void **out_proc)
{
    if (!name) {
        return WRAITH_E_NULL_ARG;
    }
    return lookup_inner(module_base, wr_djb2_a(name), 0, out_proc);
}

wraith_status_t wr_resolver_lookup_ordinal(void *module_base, uint16_t ordinal,
                                            void **out_proc)
{
    return lookup_ordinal_inner(module_base, ordinal, 0, out_proc);
}
