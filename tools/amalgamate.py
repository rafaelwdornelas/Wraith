#!/usr/bin/env python3
"""tools/amalgamate.py - per-profile single-file Wraith distributions.

Reads the canonical CMake source layout under ``src/`` and ``include/``
and emits self-contained ``wraith.c`` + ``wraith.h`` pairs at
``dist/<profile>/`` for each build profile (``default``, ``minimal``,
``teaching``, ``paranoid-classic``, ``paranoid-full``).

Each pair is a true 2-file drop-in (modeled on the SQLite / fancycode
MemoryModule pattern) that compiles standalone with MinGW-w64::

    x86_64-w64-mingw32-gcc -c wraith.c -o wraith.o

The ``sc_trampoline_x64.S`` translation unit (used when
``WRAITH_USE_INDIRECT_SYSCALLS`` is on) is converted in-place to
``__attribute__((naked))`` C functions with GAS-flavored Intel inline
asm so the ``.S`` companion file is never needed.

Run::

    python3 tools/amalgamate.py            # all five profiles
    python3 tools/amalgamate.py default    # one profile only
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
import textwrap
from datetime import date
from pathlib import Path

# ---------------------------------------------------------------------------
# Project layout
# ---------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
INC = ROOT / "include" / "wraith"
DIST = ROOT / "dist"
LICENSE = ROOT / "LICENSE"

# Public headers, listed in dependency order. The aggregator wraith.h
# (which only re-includes the others) is intentionally excluded; the
# amalgamation produces its own wraith.h.
PUBLIC_HEADERS = [
    "wraith_types.h",
    "wraith_status.h",
    "wraith_options.h",
    "wraith_introspect.h",
    "wraith_loader.h",
    "wraith_resource.h",
    "wraith_stealth.h",
]

# Internal headers in topological order. Each header below only depends
# on public Wraith headers + headers listed above it. Header guards still
# de-duplicate any redundant pulls.
INTERNAL_HEADERS = [
    # Tier 0: roots (no project deps)
    "core/wr_ptr_check.h",
    "pe/pe_constants.h",
    "syscalls/sc_table.h",
    "stealth/hashing/hash_djb2.h",
    # Tier 1: headers that depend on roots or only on public headers
    "pe/pe_validate.h",                        # needs pe_constants.h
    "syscalls/sc_engine.h",                    # needs sc_table.h
    "syscalls/sc_gadget_finder.h",
    "syscalls/sc_ssn_resolver.h",
    "mapping/map_mockingjay_scanner.h",
    "mapping/map_phantom_host_picker.h",
    "runtime/rt_pebwalk.h",
    "runtime/rt_resolver.h",
    "stealth/amsi/amsi_patch.h",
    "stealth/anti_debug/anti_debug.h",
    "stealth/etw/etw_patch.h",
    "stealth/heap_masq/heap_masq.h",
    "stealth/host_iat/host_iat.h",
    "stealth/hwbp/hwbp.h",
    "stealth/page_guard/page_guard.h",
    "stealth/private_ntdll/private_ntdll.h",
    "stealth/threadless/threadless.h",
    "stealth/unhook/unhook.h",
    # Tier 2: depend on tier 1
    "pe/pe_iter.h",                            # needs pe_validate.h
    "pe/pe_image_metrics.h",                   # needs pe_validate.h
    "core/wr_context_internal.h",
    # Tier 3: depend on wr_context_internal.h
    "exports/export_lookup.h",
    "exports/export_forward.h",
    "loader/loader_pipeline.h",                # also needs pe_validate.h
    "mapping/map_strategy.h",
    "resource/resource_internal.h",
    "runtime/rt_api.h",
    "stealth/peb_link/peb_link.h",
    "stealth/sleep/sleep.h",
]

# Source TUs that are always compiled in.
ALWAYS_SOURCES = [
    "core/wr_status.c",
    "core/wr_context.c",
    "pe/pe_validate.c",
    "pe/pe_iter.c",
    "pe/pe_image_metrics.c",
    "mapping/map_dispatch.c",
    "mapping/map_private_rwx.c",
    "runtime/rt_api.c",
    "runtime/rt_api_baseline.c",
    "runtime/rt_pebwalk.c",
    "runtime/rt_resolver.c",
    "stealth/hashing/hash_djb2.c",
    "loader/loader_pipeline.c",
    "loader/loader_sections.c",
    "loader/loader_relocs.c",
    "loader/loader_imports.c",
    "loader/loader_imports_bound.c",
    "loader/loader_imports_delay.c",
    "loader/loader_finalize.c",
    "loader/loader_seh_x64.c",
    "loader/loader_tls.c",
    "loader/loader_entry.c",
    "loader/loader_api.c",
    "exports/export_lookup.c",
    "exports/export_forward.c",
    "resource/resource_find.c",
    "resource/resource_load.c",
    "resource/resource_string.c",
    # Mockingjay is always compiled - the scanner returns
    # WRAITH_E_MAP_NO_HOST_DLL when the host has no qualifying region.
    "mapping/map_mockingjay.c",
    "mapping/map_mockingjay_scanner.c",
]

# Conditional sources, keyed by feature flag (mirrors CMakeLists.txt).
FLAG_SOURCES = {
    "WRAITH_USE_API_HASHING":      ["runtime/rt_api_ntapi.c"],
    "WRAITH_USE_INDIRECT_SYSCALLS":[
        "syscalls/sc_engine.c",
        "syscalls/sc_ssn_resolver.c",
        "syscalls/sc_gadget_finder.c",
        # sc_trampoline_x64.S is handled separately (asm-to-naked-C).
    ],
    "WRAITH_USE_PHANTOM_HOLLOWING":[
        "mapping/map_phantom.c",
        "mapping/map_phantom_host_picker.c",
    ],
    "WRAITH_USE_MODULE_STOMPING":  ["mapping/map_stomping.c"],
    "WRAITH_USE_PEB_LINKAGE":      [
        "stealth/peb_link/peb_link.c",
        "stealth/peb_link/peb_link_masquerade.c",
    ],
    "WRAITH_USE_SLEEP_OBFUSCATION":[
        "stealth/sleep/sleep.c",
        "stealth/sleep/sleep_xor_baseline.c",
        "stealth/sleep/sleep_cronos.c",
    ],
    "WRAITH_USE_UNHOOK_NTDLL":     ["stealth/unhook/unhook.c"],
    "WRAITH_USE_ETW_PATCH":        ["stealth/etw/etw_patch.c"],
    "WRAITH_USE_AMSI_PATCH":       ["stealth/amsi/amsi_patch.c"],
    "WRAITH_USE_HWBP_HOOKS":       ["stealth/hwbp/hwbp.c"],
    "WRAITH_USE_PRIVATE_NTDLL":    ["stealth/private_ntdll/private_ntdll.c"],
    "WRAITH_USE_THREADLESS_EXEC":  ["stealth/threadless/threadless.c"],
    "WRAITH_USE_PAGE_GUARD_ENCRYPT":["stealth/page_guard/page_guard.c"],
    "WRAITH_USE_HEAP_MASQUERADE":  ["stealth/heap_masq/heap_masq.c"],
    "WRAITH_USE_ANTI_DEBUG_SPOOF": ["stealth/anti_debug/anti_debug.c"],
    "WRAITH_USE_HOST_IAT_REDIRECT":["stealth/host_iat/host_iat.c"],
}

# ---------------------------------------------------------------------------
# Profile -> compile-define mapping. Mirrors cmake/options.cmake exactly.
# ---------------------------------------------------------------------------

ALL_USE_FLAGS = [
    "WRAITH_USE_API_HASHING", "WRAITH_USE_PEB_WALK", "WRAITH_USE_INDIRECT_SYSCALLS",
    "WRAITH_USE_PEB_LINKAGE", "WRAITH_USE_PHANTOM_HOLLOWING", "WRAITH_USE_MODULE_STOMPING",
    "WRAITH_USE_SLEEP_OBFUSCATION", "WRAITH_USE_UNHOOK_NTDLL",
    "WRAITH_USE_ETW_PATCH", "WRAITH_USE_AMSI_PATCH",
    "WRAITH_USE_STACK_SPOOF", "WRAITH_USE_HWBP_HOOKS", "WRAITH_USE_PRIVATE_NTDLL",
    "WRAITH_USE_THREADLESS_EXEC", "WRAITH_USE_PAGE_GUARD_ENCRYPT",
    "WRAITH_USE_HEAP_MASQUERADE", "WRAITH_USE_ANTI_DEBUG_SPOOF",
    "WRAITH_USE_HOST_IAT_REDIRECT",
]
ALL_RELIABILITY_FLAGS = [
    "WRAITH_FORWARDED_EXPORTS", "WRAITH_DELAY_LOAD_IMPORTS", "WRAITH_BOUND_IMPORTS",
    "WRAITH_REGISTER_SEH_X64", "WRAITH_TLS_FULL_LIFECYCLE", "WRAITH_RW_TO_RX_HYGIENE",
]
ALL_DIAG_FLAGS = ["WRAITH_DEBUG_LOG", "WRAITH_TRACE_PIPELINE"]


def _profile(name: str, **kwargs) -> dict:
    """Build a profile dict starting from option defaults, applying overrides."""
    cfg = {
        # reliability ON by default
        **{f: 1 for f in ALL_RELIABILITY_FLAGS},
        # stealth OFF by default
        **{f: 0 for f in ALL_USE_FLAGS},
        # diagnostics OFF by default
        **{f: 0 for f in ALL_DIAG_FLAGS},
        # algorithm name selectors (default settings)
        "WRAITH_HASH_ALGO":   "djb2",
        "WRAITH_SLEEP_ALGO":  "ekko",
        "WRAITH_SC_RESOLVER": "hellshall",
        "WRAITH_MAP_DEFAULT": "private_rwx",
        "WRAITH_PROFILE_NAME": name,
    }
    cfg.update(kwargs)
    return cfg


PROFILES = {
    "default": _profile("default"),
    "minimal": _profile(
        "minimal",
        WRAITH_FORWARDED_EXPORTS=0,
        WRAITH_DELAY_LOAD_IMPORTS=0,
        WRAITH_BOUND_IMPORTS=0,
    ),
    "teaching": _profile(
        "teaching",
        **{f: 1 for f in ALL_USE_FLAGS},
        WRAITH_DEBUG_LOG=1,
        WRAITH_TRACE_PIPELINE=1,
    ),
    "paranoid-classic": _profile(
        "paranoid-classic",
        WRAITH_USE_API_HASHING=1,
        WRAITH_USE_PEB_WALK=1,
        WRAITH_USE_INDIRECT_SYSCALLS=1,
        WRAITH_USE_PEB_LINKAGE=1,
        WRAITH_USE_PHANTOM_HOLLOWING=1,
        WRAITH_USE_SLEEP_OBFUSCATION=1,
        WRAITH_USE_ETW_PATCH=1,
        WRAITH_USE_STACK_SPOOF=1,
        WRAITH_USE_PRIVATE_NTDLL=1,
        WRAITH_SLEEP_ALGO="ekko",
        WRAITH_SC_RESOLVER="freshycalls",
    ),
    "paranoid-full": _profile(
        "paranoid-full",
        WRAITH_USE_API_HASHING=1,
        WRAITH_USE_PEB_WALK=1,
        WRAITH_USE_INDIRECT_SYSCALLS=1,
        WRAITH_USE_PEB_LINKAGE=1,
        WRAITH_USE_PHANTOM_HOLLOWING=1,
        WRAITH_USE_SLEEP_OBFUSCATION=1,
        WRAITH_USE_ETW_PATCH=1,
        WRAITH_USE_AMSI_PATCH=1,
        WRAITH_USE_STACK_SPOOF=1,
        WRAITH_USE_HWBP_HOOKS=1,
        WRAITH_USE_PRIVATE_NTDLL=1,
        WRAITH_USE_THREADLESS_EXEC=1,
        WRAITH_USE_PAGE_GUARD_ENCRYPT=1,
        WRAITH_USE_HEAP_MASQUERADE=1,
        WRAITH_USE_ANTI_DEBUG_SPOOF=1,
        WRAITH_SLEEP_ALGO="cronos",
        WRAITH_SC_RESOLVER="freshycalls",
        WRAITH_MAP_DEFAULT="phantom",
    ),
}

WRAITH_VERSION = (ROOT / "cmake" / "version.cmake").read_text(encoding="utf-8")
m = re.search(r"WRAITH_VERSION_MAJOR\s+(\d+)", WRAITH_VERSION)
n = re.search(r"WRAITH_VERSION_MINOR\s+(\d+)", WRAITH_VERSION)
p = re.search(r"WRAITH_VERSION_PATCH\s+(\d+)", WRAITH_VERSION)
VERSION_STRING = f"{m.group(1)}.{n.group(1)}.{p.group(1)}" if (m and n and p) else "1.0.0"

# ---------------------------------------------------------------------------
# Static-symbol collisions across compilation units. Each .c file's
# contents are wrapped in `#define <name> _wr_amalg_<name>__<file>` /
# `#undef <name>` so that statics with the same identifier in different
# files do not collide once concatenated into a single TU.
# ---------------------------------------------------------------------------

KNOWN_STATIC_COLLISIONS = {
    # Static functions defined in multiple TUs.
    "align_up",  # loader/loader_finalize.c, pe/pe_image_metrics.c

    # File-local struct/typedef tags reused across TUs (each redeclaration is
    # a fully internal mirror of an OS struct, so renaming is safe).
    "wr_ldr_entry",       # runtime/rt_pebwalk.c, stealth/peb_link/peb_link.c
}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

REL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"\s*$', re.MULTILINE)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def strip_relative_includes(content: str) -> str:
    """Drop every ``#include "..."`` (project-relative). System includes
    written as ``#include <...>`` are preserved.
    """
    return REL_INCLUDE_RE.sub("", content)


def stem(rel_path: str) -> str:
    """Convert ``loader/loader_finalize.c`` -> ``loader_finalize``."""
    return Path(rel_path).stem


def wrap_with_static_renames(content: str, file_stem: str) -> str:
    """Bracket ``content`` with ``#define`` / ``#undef`` for known
    cross-file static-name collisions, scoped to this file only.
    """
    pre = []
    post = []
    for sym in sorted(KNOWN_STATIC_COLLISIONS):
        # only rewrite if the file actually defines/uses the symbol
        if re.search(rf"\b{re.escape(sym)}\b", content):
            pre.append(f"#define {sym} _wr_amalg_{sym}__{file_stem}")
            post.append(f"#undef {sym}")
    if not pre:
        return content
    return "\n".join(pre) + "\n" + content + "\n" + "\n".join(post) + "\n"


def banner(title: str) -> str:
    return (
        "\n"
        "/* " + "=" * 74 + "\n"
        " * " + title + "\n"
        " * " + "=" * 74 + " */\n"
    )


# ---------------------------------------------------------------------------
# Inline-asm conversion of sc_trampoline_x64.S
# ---------------------------------------------------------------------------

ASM_NAKED_BLOCK = r'''
/* ============================================================================
 * Hell's Hall trampolines - GCC inline-asm naked variants.
 *
 * Equivalent to src/syscalls/sc_trampoline_x64.S in the canonical CMake
 * build but re-encoded as `__attribute__((naked))` C functions so the
 * single-file distribution has no companion .S file. Only meaningful when
 * WRAITH_USE_INDIRECT_SYSCALLS is set; the C engine populates `g_gadget`,
 * `g_ret_gadget`, and the per-syscall `ssn_*` slots before the first call.
 *
 * Each stub:
 *   1. Moves RCX -> R10 (NTAPI -> kernel calling convention transfer).
 *   2. Loads the SSN from a writable global into EAX.
 *   3. Indirect-jumps to a `syscall; ret` gadget located inside ntdll.
 *
 * The `_spoof` variants additionally push a `ret`-only gadget address so
 * a stack walker observed during the kernel transition sees the call
 * origin at an unmodified ntdll page rather than inside the private
 * region. RSP shift compensation copies stack arguments back down to
 * [rsp+0x28..] after the synthetic push.
 * ========================================================================== */

#if WRAITH_USE_INDIRECT_SYSCALLS

void *g_gadget = 0;
void *g_ret_gadget = 0;

uint32_t ssn_NtAllocateVirtualMemory  = 0;
uint32_t ssn_NtProtectVirtualMemory   = 0;
uint32_t ssn_NtFreeVirtualMemory      = 0;
uint32_t ssn_NtFlushInstructionCache  = 0;
uint32_t ssn_NtCreateSection          = 0;
uint32_t ssn_NtMapViewOfSection       = 0;
uint32_t ssn_NtUnmapViewOfSection     = 0;
uint32_t ssn_NtClose                  = 0;

/* Identical asm body for every stub - the C signature only exists to
 * satisfy the engine's extern declarations; the body is naked and the
 * kernel reads its own arg slots directly from RCX/RDX/R8/R9 + stack. */
#define WR_SC_STUB_BODY(name)                                                \
    __asm__ volatile (                                                       \
        ".intel_syntax noprefix\n\t"                                         \
        "mov r10, rcx\n\t"                                                   \
        "mov eax, dword ptr [rip + ssn_" #name "]\n\t"                       \
        "jmp qword ptr [rip + g_gadget]\n\t"                                 \
        ".att_syntax prefix\n\t"                                             \
    )

__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtAllocateVirtualMemory(HANDLE p, PVOID *base, ULONG_PTR zb,
                                   PSIZE_T sz, ULONG type, ULONG prot) {
    (void)p; (void)base; (void)zb; (void)sz; (void)type; (void)prot;
    WR_SC_STUB_BODY(NtAllocateVirtualMemory);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtProtectVirtualMemory(HANDLE p, PVOID *base, PSIZE_T sz,
                                  ULONG newp, PULONG oldp) {
    (void)p; (void)base; (void)sz; (void)newp; (void)oldp;
    WR_SC_STUB_BODY(NtProtectVirtualMemory);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtFreeVirtualMemory(HANDLE p, PVOID *base, PSIZE_T sz, ULONG type) {
    (void)p; (void)base; (void)sz; (void)type;
    WR_SC_STUB_BODY(NtFreeVirtualMemory);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtFlushInstructionCache(HANDLE p, PVOID base, SIZE_T sz) {
    (void)p; (void)base; (void)sz;
    WR_SC_STUB_BODY(NtFlushInstructionCache);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtCreateSection(PHANDLE out_section, ULONG access, PVOID oa,
                           PLARGE_INTEGER max_size, ULONG prot,
                           ULONG attribs, HANDLE file) {
    (void)out_section; (void)access; (void)oa; (void)max_size;
    (void)prot; (void)attribs; (void)file;
    WR_SC_STUB_BODY(NtCreateSection);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtMapViewOfSection(HANDLE section, HANDLE process, PVOID *base,
                              ULONG_PTR zb, SIZE_T commit_size,
                              PLARGE_INTEGER offset, PSIZE_T view_size,
                              DWORD inherit, ULONG alloc_type, ULONG prot) {
    (void)section; (void)process; (void)base; (void)zb; (void)commit_size;
    (void)offset; (void)view_size; (void)inherit; (void)alloc_type; (void)prot;
    WR_SC_STUB_BODY(NtMapViewOfSection);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtUnmapViewOfSection(HANDLE process, PVOID base) {
    (void)process; (void)base;
    WR_SC_STUB_BODY(NtUnmapViewOfSection);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_NtClose(HANDLE handle) {
    (void)handle;
    WR_SC_STUB_BODY(NtClose);
}

/* Stack-arg shift macros. After `push ret_gadget`, RSP has decreased by 8
 * so any stack-passed arg the wrapper deposited at [rsp+0x28..] is now at
 * [rsp+0x30..]; copy it back down before issuing the syscall.
 *
 *   NtAllocateVirtualMemory   6 args -> 2 stack slots
 *   NtProtectVirtualMemory    5 args -> 1 stack slot
 *   NtFreeVirtualMemory       4 args -> 0 (no shift)
 *   NtFlushInstructionCache   3 args -> 0 (no shift)
 *   NtCreateSection           7 args -> 3 stack slots
 *   NtMapViewOfSection       10 args -> 6 stack slots
 *   NtUnmapViewOfSection      2 args -> 0 (no shift)
 *   NtClose                   1 arg  -> 0 (no shift)
 */

#define WR_SHIFT_NONE ""
#define WR_SHIFT_1                                                           \
    "mov r11, [rsp + 0x30]\n\t" "mov [rsp + 0x28], r11\n\t"
#define WR_SHIFT_2                                                           \
    WR_SHIFT_1                                                               \
    "mov r11, [rsp + 0x38]\n\t" "mov [rsp + 0x30], r11\n\t"
#define WR_SHIFT_3                                                           \
    WR_SHIFT_2                                                               \
    "mov r11, [rsp + 0x40]\n\t" "mov [rsp + 0x38], r11\n\t"
#define WR_SHIFT_6                                                           \
    WR_SHIFT_3                                                               \
    "mov r11, [rsp + 0x48]\n\t" "mov [rsp + 0x40], r11\n\t"                  \
    "mov r11, [rsp + 0x50]\n\t" "mov [rsp + 0x48], r11\n\t"                  \
    "mov r11, [rsp + 0x58]\n\t" "mov [rsp + 0x50], r11\n\t"

#define WR_SC_STUB_SPOOF_BODY(name, shift)                                   \
    __asm__ volatile (                                                       \
        ".intel_syntax noprefix\n\t"                                         \
        "push qword ptr [rip + g_ret_gadget]\n\t"                            \
        shift                                                                \
        "mov r10, rcx\n\t"                                                   \
        "mov eax, dword ptr [rip + ssn_" #name "]\n\t"                       \
        "jmp qword ptr [rip + g_gadget]\n\t"                                 \
        ".att_syntax prefix\n\t"                                             \
    )

__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtAllocateVirtualMemory(HANDLE p, PVOID *base, ULONG_PTR zb,
                                         PSIZE_T sz, ULONG type, ULONG prot) {
    (void)p; (void)base; (void)zb; (void)sz; (void)type; (void)prot;
    WR_SC_STUB_SPOOF_BODY(NtAllocateVirtualMemory, WR_SHIFT_2);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtProtectVirtualMemory(HANDLE p, PVOID *base, PSIZE_T sz,
                                        ULONG newp, PULONG oldp) {
    (void)p; (void)base; (void)sz; (void)newp; (void)oldp;
    WR_SC_STUB_SPOOF_BODY(NtProtectVirtualMemory, WR_SHIFT_1);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtFreeVirtualMemory(HANDLE p, PVOID *base, PSIZE_T sz, ULONG type) {
    (void)p; (void)base; (void)sz; (void)type;
    WR_SC_STUB_SPOOF_BODY(NtFreeVirtualMemory, WR_SHIFT_NONE);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtFlushInstructionCache(HANDLE p, PVOID base, SIZE_T sz) {
    (void)p; (void)base; (void)sz;
    WR_SC_STUB_SPOOF_BODY(NtFlushInstructionCache, WR_SHIFT_NONE);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtCreateSection(PHANDLE out_section, ULONG access, PVOID oa,
                                 PLARGE_INTEGER max_size, ULONG prot,
                                 ULONG attribs, HANDLE file) {
    (void)out_section; (void)access; (void)oa; (void)max_size;
    (void)prot; (void)attribs; (void)file;
    WR_SC_STUB_SPOOF_BODY(NtCreateSection, WR_SHIFT_3);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtMapViewOfSection(HANDLE section, HANDLE process, PVOID *base,
                                    ULONG_PTR zb, SIZE_T commit_size,
                                    PLARGE_INTEGER offset, PSIZE_T view_size,
                                    DWORD inherit, ULONG alloc_type, ULONG prot) {
    (void)section; (void)process; (void)base; (void)zb; (void)commit_size;
    (void)offset; (void)view_size; (void)inherit; (void)alloc_type; (void)prot;
    WR_SC_STUB_SPOOF_BODY(NtMapViewOfSection, WR_SHIFT_6);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtUnmapViewOfSection(HANDLE process, PVOID base) {
    (void)process; (void)base;
    WR_SC_STUB_SPOOF_BODY(NtUnmapViewOfSection, WR_SHIFT_NONE);
}
__attribute__((naked, used)) NTSTATUS NTAPI
wr_sc_stub_spoof_NtClose(HANDLE handle) {
    (void)handle;
    WR_SC_STUB_SPOOF_BODY(NtClose, WR_SHIFT_NONE);
}

#endif /* WRAITH_USE_INDIRECT_SYSCALLS */
'''


# ---------------------------------------------------------------------------
# Generation
# ---------------------------------------------------------------------------

PROLOGUE_TEMPLATE = """\
/*
 * wraith.{ext} - amalgamated single-file build of Wraith {version}
 *               profile: {profile}
 *               generated: {today}
 *
 * This is a self-contained drop-in build of the Wraith stealth PE
 * loader. It is functionally equivalent to compiling the canonical
 * CMake source tree with -DWRAITH_PROFILE={profile}, but condensed
 * into two files (.c + .h) for fast integration.
 *
 * To regenerate: from the project root, run
 *     python3 tools/amalgamate.py {profile}
 *
 * License: MIT - see the LICENSE file shipped beside this header.
 */
"""


def emit_defines(profile_cfg: dict) -> str:
    lines: list[str] = []
    lines.append("/* === profile-locked feature gates === */")
    for f in ALL_RELIABILITY_FLAGS + ALL_USE_FLAGS + ALL_DIAG_FLAGS:
        v = profile_cfg.get(f, 0)
        lines.append(f"#ifndef {f}")
        lines.append(f"#  define {f} {v}")
        lines.append(f"#endif")
    # Algorithm selectors are emitted as both numeric (=1) and string flavors,
    # mirroring wr_apply_feature_defines in cmake/modules/HardenFlags.cmake.
    for kind in ("HASH_ALGO", "SLEEP_ALGO", "SC_RESOLVER", "MAP_DEFAULT"):
        sel = profile_cfg[f"WRAITH_{kind}"]
        lines.append(f"#ifndef WRAITH_{kind}_{sel}")
        lines.append(f"#  define WRAITH_{kind}_{sel} 1")
        lines.append(f"#endif")
    lines.append(f'#ifndef WRAITH_HASH_ALGO_NAME')
    lines.append(f'#  define WRAITH_HASH_ALGO_NAME "{profile_cfg["WRAITH_HASH_ALGO"]}"')
    lines.append(f'#endif')
    lines.append(f'#ifndef WRAITH_SLEEP_ALGO_NAME')
    lines.append(f'#  define WRAITH_SLEEP_ALGO_NAME "{profile_cfg["WRAITH_SLEEP_ALGO"]}"')
    lines.append(f'#endif')
    lines.append(f'#ifndef WRAITH_SC_RESOLVER_NAME')
    lines.append(f'#  define WRAITH_SC_RESOLVER_NAME "{profile_cfg["WRAITH_SC_RESOLVER"]}"')
    lines.append(f'#endif')
    lines.append(f'#ifndef WRAITH_PROFILE_NAME')
    lines.append(f'#  define WRAITH_PROFILE_NAME "{profile_cfg["WRAITH_PROFILE_NAME"]}"')
    lines.append(f'#endif')
    lines.append(f'#ifndef WRAITH_VERSION_STRING')
    lines.append(f'#  define WRAITH_VERSION_STRING "{VERSION_STRING}"')
    lines.append(f'#endif')
    return "\n".join(lines) + "\n"


def gather_sources(profile_cfg: dict) -> list[str]:
    """Return list of .c sources (relative to src/) for ``profile_cfg``."""
    files = list(ALWAYS_SOURCES)
    for flag, file_list in FLAG_SOURCES.items():
        if profile_cfg.get(flag, 0):
            files.extend(file_list)
    # When MODULE_STOMPING is on but PHANTOM_HOLLOWING is off, the host
    # picker still has to be pulled in (shared with the phantom backend).
    if (profile_cfg.get("WRAITH_USE_MODULE_STOMPING")
            and not profile_cfg.get("WRAITH_USE_PHANTOM_HOLLOWING")):
        if "mapping/map_phantom_host_picker.c" not in files:
            files.append("mapping/map_phantom_host_picker.c")
    # Stable ordering keeps generated diffs minimal.
    seen: set[str] = set()
    ordered: list[str] = []
    for f in files:
        if f not in seen:
            ordered.append(f)
            seen.add(f)
    return ordered


def build_wraith_h(profile: str, profile_cfg: dict) -> str:
    """Return the single-file ``wraith.h`` for ``profile``."""
    today = date.today().isoformat()
    out: list[str] = [PROLOGUE_TEMPLATE.format(
        ext="h", version=VERSION_STRING, profile=profile, today=today)]
    out.append("#ifndef WRAITH_AMALGAMATED_H\n#define WRAITH_AMALGAMATED_H\n")
    out.append("#ifdef __cplusplus\nextern \"C\" {\n#endif\n")
    out.append(banner("public surface (concatenated headers)"))
    for hname in PUBLIC_HEADERS:
        path = INC / hname
        out.append(banner(f"include/wraith/{hname}"))
        body = strip_relative_includes(read_text(path))
        out.append(body)
    out.append("\n#ifdef __cplusplus\n}\n#endif\n")
    out.append("#endif /* WRAITH_AMALGAMATED_H */\n")
    return "\n".join(out)


def build_wraith_c(profile: str, profile_cfg: dict) -> str:
    today = date.today().isoformat()
    parts: list[str] = []
    parts.append(PROLOGUE_TEMPLATE.format(
        ext="c", version=VERSION_STRING, profile=profile, today=today))

    # Profile-locked compile defines (override only if the consumer hasn't
    # explicitly defined them on the command line).
    parts.append(emit_defines(profile_cfg))

    # System includes pulled by every TU. Listed once up front so the
    # per-file content can rely on them being present.
    parts.append(banner("system includes"))
    parts.append(textwrap.dedent("""\
        #include <windows.h>
        #include <winternl.h>
        #include <winnt.h>

        #include <stdarg.h>
        #include <stdatomic.h>
        #include <stdbool.h>
        #include <stddef.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <wchar.h>
        #include <intrin.h>
    """))

    # Public headers (declarations needed by the .c files below).
    parts.append(banner("public API headers (inlined)"))
    for hname in PUBLIC_HEADERS:
        parts.append(banner(f"include/wraith/{hname}"))
        body = strip_relative_includes(read_text(INC / hname))
        parts.append(body)

    # Internal headers in dependency order.
    parts.append(banner("internal headers (inlined)"))
    for hname in INTERNAL_HEADERS:
        parts.append(banner(f"src/{hname}"))
        body = strip_relative_includes(read_text(SRC / hname))
        parts.append(body)

    # Source TUs. Each one is wrapped in a `#define <sym> _wr_amalg_<sym>__<file>`
    # block for any known cross-file static-name collisions.
    sources = gather_sources(profile_cfg)
    parts.append(banner(f"compilation units ({len(sources)} files)"))
    for rel in sources:
        path = SRC / rel
        parts.append(banner(f"src/{rel}"))
        body = strip_relative_includes(read_text(path))
        body = wrap_with_static_renames(body, stem(rel))
        parts.append(body)

    # If indirect syscalls are on, append the inline-asm naked stubs that
    # replace sc_trampoline_x64.S.
    if profile_cfg.get("WRAITH_USE_INDIRECT_SYSCALLS"):
        parts.append(ASM_NAKED_BLOCK)

    return "\n".join(parts)


# ---------------------------------------------------------------------------
# Per-profile threat-model description used in QUICKSTART.md
# ---------------------------------------------------------------------------

PROFILE_BLURBS = {
    "default": (
        "Reliability features ON, all stealth OFF. The smallest and most "
        "predictable build - useful for first-time integration smoke tests "
        "or for embedding where stealth is irrelevant (e.g. CTF judge)."
    ),
    "minimal": (
        "Bare loader. No forwarder resolution, no delay-load imports, no "
        "bound-import handling. The right pick when you control the payload "
        "DLL and can guarantee it has no exotic import dependencies."
    ),
    "teaching": (
        "Every feature compiled in, runtime flags drive activation. Lets "
        "the consumer flip individual stealth bits via wraith_load_options "
        "without re-running CMake. Largest binary; recommended for the "
        "Sec4us classroom track and for IOC-comparison labs."
    ),
    "paranoid-classic": (
        "Hell's Hall indirect syscalls + Phantom DLL Hollowing + Ekko "
        "sleep + ETW patch + stack spoof + private ntdll. Mirrors the "
        "feature set of the canonical 2023-2024 stealth chain - good "
        "baseline for evaluating modern EDR posture without paying the "
        "cost of bleeding-edge primitives."
    ),
    "paranoid-full": (
        "Everything in paranoid-classic plus AMSI patch, hardware-breakpoint "
        "hooks, threadless execution, page-guard self-encryption, heap "
        "masquerade, anti-debug spoofing, and Cronos sleep. Built without "
        "CRT in the canonical CMake path; the amalgamated build keeps the "
        "CRT for portability since the dist/ artifact is meant to be "
        "dropped into existing projects."
    ),
}


def build_quickstart(profile: str, profile_cfg: dict, source_count: int) -> str:
    blurb = PROFILE_BLURBS[profile]
    return textwrap.dedent(f"""\
        # Wraith {VERSION_STRING} - {profile} profile (amalgamated)

        {blurb}

        ## Drop-in build (3 commands)

        ```sh
        # 1. Pull these files into your project (alongside your loader)
        cp wraith.c wraith.h LICENSE /path/to/your/project/

        # 2. Compile against the MinGW-w64 cross toolchain
        x86_64-w64-mingw32-gcc -c wraith.c -o wraith.o

        # 3. Link your loader against wraith.o
        x86_64-w64-mingw32-gcc loader.c wraith.o -o loader.exe
        ```

        ## Minimal usage

        ```c
        #include "wraith.h"

        int main(void) {{
            unsigned char *dll_bytes = /* ... */;
            size_t dll_size           = /* ... */;

            wraith_load_options opt = {{0}};
            opt.flags = WRAITH_F_RELIABILITY_ALL;

            wraith_handle_t h = NULL;
            wraith_status_t rc = wraith_load_library(dll_bytes, dll_size, &opt, &h);
            if (WRAITH_SUCCESS(rc)) wraith_free_library(h);
            return rc;
        }}
        ```

        ## What's inlined

        - {source_count} compilation units from `src/`
        - {len(PUBLIC_HEADERS)} public + {len(INTERNAL_HEADERS)} internal headers
        {"- 16 indirect-syscall stubs (inline-asm naked C)" if profile_cfg.get("WRAITH_USE_INDIRECT_SYSCALLS") else "- (no asm - indirect syscalls compiled out)"}

        ## Caveats vs the CMake build

        - No `__has_include("wr_api_hashes.h")` codegen path. The amalgamation
          relies on the in-source fallback constants (kernel32 + LoadLibraryA
          + FreeLibrary). If you need the full hash table, regenerate via
          `python3 tools/hashgen.py` from the canonical tree.
        - No CET / CFG / -nostdlib hardening. The dist/ artifact targets
          portability; rebuild from the CMake tree with the appropriate
          `WRAITH_HARDEN_*` flags if you need them.
        - Headers are written for x86_64. Compile flags must include
          `-DCMAKE_C_STANDARD=11` (or `-std=c11`) and `-m64` (default for
          MinGW-w64 x86_64).
        """)


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def write_dist(profile: str, profile_cfg: dict) -> None:
    out_dir = DIST / profile
    out_dir.mkdir(parents=True, exist_ok=True)
    sources = gather_sources(profile_cfg)

    (out_dir / "wraith.h").write_text(build_wraith_h(profile, profile_cfg), encoding="utf-8")
    (out_dir / "wraith.c").write_text(build_wraith_c(profile, profile_cfg), encoding="utf-8")
    (out_dir / "QUICKSTART.md").write_text(
        build_quickstart(profile, profile_cfg, len(sources)), encoding="utf-8")
    if LICENSE.exists():
        shutil.copy2(LICENSE, out_dir / "LICENSE")

    h_size = (out_dir / "wraith.h").stat().st_size
    c_size = (out_dir / "wraith.c").stat().st_size
    print(f"  dist/{profile:<18s} wraith.h={h_size:>7d}B  "
          f"wraith.c={c_size:>9d}B  ({len(sources)} TUs)")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("profiles", nargs="*",
                    help=f"profile name(s) to build; default = all ({', '.join(PROFILES)})")
    args = ap.parse_args()

    targets = args.profiles or list(PROFILES)
    for p in targets:
        if p not in PROFILES:
            print(f"unknown profile: {p}", file=sys.stderr)
            print(f"valid: {', '.join(PROFILES)}", file=sys.stderr)
            return 2

    print(f"Wraith {VERSION_STRING} amalgamation -> dist/")
    for p in targets:
        write_dist(p, PROFILES[p])
    print("done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
