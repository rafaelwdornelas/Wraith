/*
 * src/stealth/hashing/hash_djb2.c
 *
 * The four entry points (a/a_n/w/w_n) share an inline core. The lower-
 * case fold is restricted to the ASCII A..Z range so the hash stays
 * stable across system locales (and matches what tools/hashgen.py
 * produces - any divergence between compile-time and runtime hashing
 * silently breaks resolution, so the Python implementation MUST mirror
 * this exact arithmetic).
 */

#include "stealth/hashing/hash_djb2.h"

static inline uint32_t fold_ascii(uint32_t c)
{
  if (c >= 0x41u && c <= 0x5Au) {  /* 'A' .. 'Z' */
  c += 0x20u;
  }
  return c;
}

uint32_t wr_djb2_a(const char *s)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (; *s; ++s) {
  uint32_t c = (uint8_t)*s;
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_a_n(const char *s, size_t n)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (size_t i = 0; i < n; ++i) {
  uint32_t c = (uint8_t)s[i];
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_w(const wchar_t *s)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (; *s; ++s) {
  uint32_t c = (uint32_t)*s;
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}

uint32_t wr_djb2_w_n(const wchar_t *s, size_t n)
{
  if (!s) {
  return 0;
  }
  uint32_t h = 5381u;
  for (size_t i = 0; i < n; ++i) {
  uint32_t c = (uint32_t)s[i];
  h = ((h << 5) + h) + fold_ascii(c);
  }
  return h;
}
