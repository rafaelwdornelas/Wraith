/*
 * src/stealth/hashing/hash_djb2.h
 *
 * Case-insensitive DJB2 hash. Used by:
 *  - rt_pebwalk : hashing UNICODE_STRING base names of loaded modules
 *  - rt_resolver: hashing ASCII export names from a PE
 *  - tools/hashgen.py : compile-time pre-computed constants
 *
 * Definition (matches the Python generator):
 *  h0 = 5381
 *  h_n = (h_{n-1} * 33 + lower(c_n))
 *  final = h_n & 0xFFFFFFFF
 *
 * Lower-casing is restricted to the ASCII range A..Z so the hash remains
 * stable across locales. UTF-16 names are folded the same way.
 */

#ifndef WRAITH_HASH_DJB2_H
#define WRAITH_HASH_DJB2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t wr_djb2_a(const char *s);
uint32_t wr_djb2_a_n(const char *s, size_t n);
uint32_t wr_djb2_w(const wchar_t *s);
uint32_t wr_djb2_w_n(const wchar_t *s, size_t n);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_HASH_DJB2_H */
