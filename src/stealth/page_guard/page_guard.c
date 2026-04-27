/*
 * src/stealth/page_guard/page_guard.c
 *
 * Implementation. Single armed-module module-level state -
 * concurrent arming of multiple modules is a future refinement.
 *
 * The bitmap tracks which pages are still encrypted: 1 = encrypted,
 * 0 = plain. The handler clears the bit after decrypting on first
 * touch; Disarm walks the bitmap and decrypts any that are still
 * set.
 *
 * Wine note: wine 9.0 supports PAGE_GUARD page-attribute and the
 * EXCEPTION_GUARD_PAGE_VIOLATION dispatch path, so this works
 * end-to-end under wine64.
 */

#include "core/wr_context_internal.h"
#include "wraith/wraith_stealth.h"
#include "stealth/page_guard/page_guard.h"

#include <intrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WRAITH_PG_KEY_BYTES 256

#ifndef EXCEPTION_GUARD_PAGE
#  define EXCEPTION_GUARD_PAGE 0x80000001L
#endif

static struct {
  volatile LONG armed;
  uint8_t  *exec_base;
  size_t  exec_size;
  DWORD  page_size;
  uint8_t  *bitmap;  /* 1 byte per page (0/1 flag). */
  size_t  page_count;
  uint8_t  key[WRAITH_PG_KEY_BYTES];
  PVOID  veh;
} g_pg;

static void pg_xor_page(uint8_t *page, DWORD page_size,
  const uint8_t *key, size_t key_len)
{
  for (DWORD i = 0; i < page_size; ++i) {
  page[i] ^= key[i % key_len];
  }
}

static int pg_owns(const void *p)
{
  if (!g_pg.exec_base || g_pg.exec_size == 0) return 0;
  uintptr_t v = (uintptr_t)p;
  return v >= (uintptr_t)g_pg.exec_base &&
  v <  (uintptr_t)g_pg.exec_base + g_pg.exec_size;
}

static size_t pg_index(const void *page_addr)
{
  return ((uintptr_t)page_addr - (uintptr_t)g_pg.exec_base) /
  g_pg.page_size;
}

static LONG NTAPI pg_handler(PEXCEPTION_POINTERS info)
{
  if (info->ExceptionRecord->ExceptionCode != EXCEPTION_GUARD_PAGE) {
  return EXCEPTION_CONTINUE_SEARCH;
  }
  /* ExceptionInformation[1] is the address that triggered the
  * guard. */
  void *fault = (void *)info->ExceptionRecord->ExceptionInformation[1];
  if (!pg_owns(fault)) {
  return EXCEPTION_CONTINUE_SEARCH;
  }

  uintptr_t page_addr =
  (uintptr_t)fault & ~((uintptr_t)g_pg.page_size - 1);
  size_t idx = pg_index((void *)page_addr);
  if (idx >= g_pg.page_count) {
  return EXCEPTION_CONTINUE_SEARCH;
  }
  if (!g_pg.bitmap[idx]) {
  /* Page already plain - nothing to do. The CPU has cleared
  * the guard so subsequent accesses won't fault. */
  return EXCEPTION_CONTINUE_EXECUTION;
  }

  DWORD old = 0;
  if (!VirtualProtect((LPVOID)page_addr, g_pg.page_size,
  PAGE_READWRITE, &old)) {
  return EXCEPTION_CONTINUE_SEARCH;
  }
  pg_xor_page((uint8_t *)page_addr, g_pg.page_size,
  g_pg.key, sizeof(g_pg.key));
  DWORD ignore = 0;
  VirtualProtect((LPVOID)page_addr, g_pg.page_size,
  PAGE_EXECUTE_READ, &ignore);
  FlushInstructionCache(GetCurrentProcess(),
  (LPCVOID)page_addr, g_pg.page_size);
  g_pg.bitmap[idx] = 0;
  return EXCEPTION_CONTINUE_EXECUTION;
}

static void pg_derive_key(uint8_t *key, size_t len)
{
  uint64_t r = __rdtsc() ^ ((uint64_t)GetTickCount64() << 17);
  for (size_t i = 0; i < len; ++i) {
  r = r * 6364136223846793005ULL + 1442695040888963407ULL;
  key[i] = (uint8_t)(r >> 56);
  }
}

wraith_status_t wraith_pageguard_arm(wraith_handle_t h)
{
#if WRAITH_USE_PAGE_GUARD_ENCRYPT
  struct wr_ctx *ctx = NULL;
  wraith_status_t rc = wr_ctx_check(h, &ctx);
  if (rc != WRAITH_OK) return rc;
  if (!ctx->image_base || !ctx->headers) return WRAITH_E_INVALID_HANDLE;

  /* Single-armed-module constraint for */
  if (InterlockedCompareExchange(&g_pg.armed, 1, 0) != 0) {
  return WRAITH_E_STEALTH_INCOMPATIBLE;
  }

  /* Pick the first executable section as our guard region. Most
  * single-export DLLs have a single .text. */
  PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
  PIMAGE_SECTION_HEADER s = IMAGE_FIRST_SECTION(nt);
  PIMAGE_SECTION_HEADER text = NULL;
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s) {
  if (s->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
  text = s;
  break;
  }
  }
  if (!text) {
  InterlockedExchange(&g_pg.armed, 0);
  return WRAITH_E_STEALTH_INCOMPATIBLE;
  }

  g_pg.page_size = ctx->page_size ? ctx->page_size : 4096;
  g_pg.exec_base = (uint8_t *)ctx->image_base + text->VirtualAddress;
  /* Round up to whole pages. */
  size_t vsize = text->Misc.VirtualSize ? text->Misc.VirtualSize
  : text->SizeOfRawData;
  g_pg.exec_size = (vsize + g_pg.page_size - 1) &
  ~(size_t)(g_pg.page_size - 1);
  g_pg.page_count = g_pg.exec_size / g_pg.page_size;

  g_pg.bitmap = (uint8_t *)calloc(g_pg.page_count, 1);
  if (!g_pg.bitmap) {
  InterlockedExchange(&g_pg.armed, 0);
  return WRAITH_E_OOM;
  }

  pg_derive_key(g_pg.key, sizeof(g_pg.key));

  g_pg.veh = AddVectoredExceptionHandler(1u, pg_handler);
  if (!g_pg.veh) {
  free(g_pg.bitmap);
  memset(&g_pg, 0, sizeof(g_pg));
  return WRAITH_E_STEALTH_INSTALL;
  }

  /* Encrypt + guard each page. */
  for (size_t i = 0; i < g_pg.page_count; ++i) {
  uint8_t *page = g_pg.exec_base + i * g_pg.page_size;
  DWORD old = 0;
  if (!VirtualProtect(page, g_pg.page_size, PAGE_READWRITE, &old)) {
  continue;
  }
  pg_xor_page(page, g_pg.page_size, g_pg.key, sizeof(g_pg.key));
  DWORD ignore = 0;
  VirtualProtect(page, g_pg.page_size,
  PAGE_EXECUTE_READ | PAGE_GUARD, &ignore);
  g_pg.bitmap[i] = 1;
  }
  return WRAITH_OK;
#else
  (void)h;
  return WRAITH_E_FEATURE_DISABLED;
#endif
}

wraith_status_t wraith_pageguard_disarm(wraith_handle_t h)
{
#if WRAITH_USE_PAGE_GUARD_ENCRYPT
  (void)h;  /* armed state is module-global */
  if (!InterlockedCompareExchange(&g_pg.armed, 0, 0)) {
  return WRAITH_OK;  /* not armed */
  }

  /* Decrypt anything still encrypted; clear PAGE_GUARD on all
  * pages. */
  for (size_t i = 0; i < g_pg.page_count; ++i) {
  uint8_t *page = g_pg.exec_base + i * g_pg.page_size;
  DWORD old = 0;
  if (g_pg.bitmap[i]) {
  if (VirtualProtect(page, g_pg.page_size,
  PAGE_READWRITE, &old)) {
  pg_xor_page(page, g_pg.page_size, g_pg.key, sizeof(g_pg.key));
  g_pg.bitmap[i] = 0;
  }
  }
  DWORD ignore = 0;
  VirtualProtect(page, g_pg.page_size, PAGE_EXECUTE_READ, &ignore);
  }
  FlushInstructionCache(GetCurrentProcess(),
  g_pg.exec_base, g_pg.exec_size);

  if (g_pg.veh) {
  RemoveVectoredExceptionHandler(g_pg.veh);
  g_pg.veh = NULL;
  }
  SecureZeroMemory(g_pg.key, sizeof(g_pg.key));
  free(g_pg.bitmap);
  memset(&g_pg, 0, sizeof(g_pg));
  return WRAITH_OK;
#else
  (void)h;
  return WRAITH_E_FEATURE_DISABLED;
#endif
}
