/*
 * src/syscalls/sc_table.h
 *
 * Catalogue of Nt* syscalls the engine knows about. Adding a new
 * syscall is a 3-step change:
 *  1. Append it to WRAITH_SC_LIST below.
 *  2. Add the corresponding `wr_sc_<name>` stub to sc_trampoline_x64.S.
 *  3. Add the typed wrapper signature to sc_engine.h.
 *
 * The list also drives sc_ssn_resolver.c (one resolution per entry) and
 * sc_engine.c initialization.
 */

#ifndef WRAITH_SC_TABLE_H
#define WRAITH_SC_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * X(name, hash) - "name" matches the ntdll export and the asm symbol
 * suffix (wr_sc_<name>, ssn_<name>); "hash" is the case-insensitive
 * DJB2 of "name", precomputed via tools/hashgen.py and inlined here so
 * the engine can resolve without depending on the generated header at
 * build time.
 */
#define WRAITH_SC_LIST(X) \
  X(NtAllocateVirtualMemory,  0xc66d2fccu) \
  X(NtProtectVirtualMemory,  0x191ec748u) \
  X(NtFreeVirtualMemory,  0xf429f469u) \
  X(NtFlushInstructionCache,  0x31532f5fu) \
  X(NtCreateSection,  0xc444a130u) \
  X(NtMapViewOfSection,  0x873f020au) \
  X(NtUnmapViewOfSection,  0xbbb10d4du) \
  X(NtClose,  0x2d18bb7du)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_SC_TABLE_H */
