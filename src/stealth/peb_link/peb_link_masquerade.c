/*
 * src/stealth/peb_link/peb_link_masquerade.c
 *
 * Build the two `UNICODE_STRING`s an LDR entry needs:
 *  - BaseDllName: bare filename ("winnet.dll")
 *  - FullDllName: full Windows path ("C:\Windows\System32\winnet.dll")
 *
 * The strings are heap-allocated and owned by the LDR entry; the
 * companion `wr_peb_free_masquerade_strings` frees both at unlink
 * time.
 *
 * Selection rules:
 *  - If `ctx->masquerade_name` is non-NULL, use it verbatim for the
 *  base name. Otherwise default to L"wr_module.dll".
 *  - If `ctx->masquerade_path` is non-NULL, use it verbatim for the
 *  full path. Otherwise synthesize "C:\Windows\System32\<base>".
 */

#include "core/wr_context_internal.h"
#include "wraith/wraith_status.h"

#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <winternl.h>

#define WRAITH_DEFAULT_BASE_NAME  L"wr_module.dll"

static wraith_status_t make_unicode(UNICODE_STRING *out, const wchar_t *src)
{
  size_t chars = wcslen(src);
  if (chars > 0xFFFF / sizeof(wchar_t) - 1) {
  return WRAITH_E_OOM;
  }
  USHORT bytes = (USHORT)((chars + 1) * sizeof(wchar_t));
  wchar_t *buf = (wchar_t *)calloc(chars + 1, sizeof(wchar_t));
  if (!buf) {
  return WRAITH_E_OOM;
  }
  wcscpy(buf, src);
  out->Buffer  = buf;
  out->Length  = (USHORT)(chars * sizeof(wchar_t));
  out->MaximumLength = bytes;
  return WRAITH_OK;
}

wraith_status_t wr_peb_make_masquerade_strings(struct wr_ctx *ctx,
  UNICODE_STRING *out_full,
  UNICODE_STRING *out_base)
{
  if (!ctx || !out_full || !out_base) {
  return WRAITH_E_NULL_ARG;
  }

  const wchar_t *base = ctx->masquerade_name ? ctx->masquerade_name
  : WRAITH_DEFAULT_BASE_NAME;

  wraith_status_t rc = make_unicode(out_base, base);
  if (rc != WRAITH_OK) {
  return rc;
  }

  if (ctx->masquerade_path) {
  rc = make_unicode(out_full, ctx->masquerade_path);
  if (rc != WRAITH_OK) {
  free(out_base->Buffer);
  out_base->Buffer = NULL;
  return rc;
  }
  return WRAITH_OK;
  }

  /* Synthesize "<system32>\<base>". GetSystemDirectoryW gives us the
  * canonical case (e.g. "C:\Windows\System32"), which is what the
  * native loader produces for legitimate modules. */
  wchar_t sysdir[MAX_PATH];
  UINT n = GetSystemDirectoryW(sysdir, (UINT)(sizeof(sysdir) /
  sizeof(sysdir[0])));
  if (n == 0 || n >= sizeof(sysdir) / sizeof(sysdir[0])) {
  free(out_base->Buffer);
  out_base->Buffer = NULL;
  return WRAITH_E_UNEXPECTED;
  }

  size_t baselen = wcslen(base);
  size_t total  = (size_t)n + 1 + baselen + 1;
  if (total > 0xFFFF / sizeof(wchar_t)) {
  free(out_base->Buffer);
  out_base->Buffer = NULL;
  return WRAITH_E_OOM;
  }
  wchar_t *buf = (wchar_t *)calloc(total, sizeof(wchar_t));
  if (!buf) {
  free(out_base->Buffer);
  out_base->Buffer = NULL;
  return WRAITH_E_OOM;
  }
  wcscpy(buf, sysdir);
  buf[n]  = L'\\';
  wcscpy(buf + n + 1, base);

  size_t actual = wcslen(buf);
  out_full->Buffer  = buf;
  out_full->Length  = (USHORT)(actual * sizeof(wchar_t));
  out_full->MaximumLength = (USHORT)((actual + 1) * sizeof(wchar_t));
  return WRAITH_OK;
}

void wr_peb_free_masquerade_strings(UNICODE_STRING *full,
  UNICODE_STRING *base)
{
  if (full && full->Buffer) {
  free(full->Buffer);
  full->Buffer = NULL;
  }
  if (base && base->Buffer) {
  free(base->Buffer);
  base->Buffer = NULL;
  }
}
