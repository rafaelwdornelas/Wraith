/*
 * tests/integration/test_load_sleep_cronos.c
 *
 * Verifies : Cronos-flavor sleep obfuscation.
 *
 * The structure mirrors test_load_sleep_obf.c but selects
 * `WRAITH_SLEEP_CRONOS` as the algorithm. Cronos's defining property
 * is that the calling thread parks in kernel space (the worker
 * thread does the decrypt); the observer assertion still works -
 * during the wait window the image bytes are encrypted.
 *
 *  1. Pre-sleep: image_base[0..1] equals 'M','Z'.
 *  2. During sleep: an observer thread captures image_base[0..3]
 *  ~150ms into the wait and confirms NON-MZ bytes.
 *  3. Post-sleep: header restored AND addNumbers callable.
 */

#include "wraith/wraith.h"
#include "wraith/wraith_stealth.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

static int read_file_bytes(const char *path, void **out_buf, size_t *out_size)
{
  FILE *fp = fopen(path, "rb");
  if (!fp) return -1;
  fseek(fp, 0, SEEK_END);
  long n = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (n <= 0) { fclose(fp); return -1; }
  void *buf = malloc((size_t)n);
  if (!buf) { fclose(fp); return -1; }
  size_t r = fread(buf, 1, (size_t)n, fp);
  fclose(fp);
  if (r != (size_t)n) { free(buf); return -1; }
  *out_buf = buf;
  *out_size = (size_t)n;
  return 0;
}

typedef struct observer_ctx {
  volatile uint8_t *image_base;
  HANDLE  start_event;
  uint8_t  captured[4];
  int  saw_encrypted;
} observer_ctx;

static DWORD WINAPI observer_thread(LPVOID arg)
{
  observer_ctx *o = (observer_ctx *)arg;
  WaitForSingleObject(o->start_event, INFINITE);
  Sleep(150);
  o->captured[0] = o->image_base[0];
  o->captured[1] = o->image_base[1];
  o->captured[2] = o->image_base[2];
  o->captured[3] = o->image_base[3];
  o->saw_encrypted = (o->captured[0] != 'M' || o->captured[1] != 'Z');
  return 0;
}

int main(int argc, char **argv)
{
  const char *path = (argc > 1) ? argv[1] : "payload.dll";
  void  *bytes = NULL;
  size_t size  = 0;
  if (read_file_bytes(path, &bytes, &size) != 0) {
  fprintf(stderr, "FAIL: cannot read %s\n", path);
  return 1;
  }

  wraith_load_options opt = {0};
  opt.flags  = WRAITH_F_SLEEP_OBFUSCATION | WRAITH_F_RELIABILITY_ALL;
  opt.sleep_algo = WRAITH_SLEEP_CRONOS;

  wraith_handle_t h = NULL;
  wraith_status_t rc = wraith_load_library(bytes, size, &opt, &h);
  if (rc != WRAITH_OK || !h) {
  fprintf(stderr, "FAIL: wraith_load_library -> %s (%s)\n",
  wraith_status_string(rc), wraith_last_error());
  free(bytes);
  return 1;
  }

  void  *image_base = NULL;
  size_t  image_size = 0;
  if (wraith_get_image_base(h, &image_base, &image_size) != WRAITH_OK
  || !image_base) {
  fprintf(stderr, "FAIL: wraith_get_image_base\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  const uint8_t *p = (const uint8_t *)image_base;
  if (p[0] != 'M' || p[1] != 'Z') {
  fprintf(stderr, "FAIL: pre-sleep header is %02x %02x\n",
  p[0], p[1]);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: pre-sleep image_base starts with MZ\n");

  observer_ctx obs = {0};
  obs.image_base  = (uint8_t *)image_base;
  obs.start_event = CreateEventW(NULL, TRUE, FALSE, NULL);
  HANDLE worker  = CreateThread(NULL, 0, observer_thread, &obs, 0, NULL);
  if (!obs.start_event || !worker) {
  fprintf(stderr, "FAIL: spawn observer\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  SetEvent(obs.start_event);
  rc = wraith_sleep(h, 600);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wraith_sleep -> %s\n", wraith_status_string(rc));
  TerminateThread(worker, 1);
  CloseHandle(worker);
  CloseHandle(obs.start_event);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }

  WaitForSingleObject(worker, INFINITE);
  CloseHandle(worker);
  CloseHandle(obs.start_event);

  if (!obs.saw_encrypted) {
  fprintf(stderr,
  "FAIL: observer saw plaintext during cronos sleep "
  "(captured %02x %02x %02x %02x)\n",
  obs.captured[0], obs.captured[1],
  obs.captured[2], obs.captured[3]);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: observer saw encrypted bytes during cronos sleep "
  "(captured %02x %02x %02x %02x)\n",
  obs.captured[0], obs.captured[1],
  obs.captured[2], obs.captured[3]);

  if (p[0] != 'M' || p[1] != 'Z') {
  fprintf(stderr, "FAIL: post-sleep header is %02x %02x\n",
  p[0], p[1]);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  void *proc = NULL;
  if (wraith_get_proc_address(h, "addNumbers", &proc) != WRAITH_OK || !proc) {
  fprintf(stderr, "FAIL: post-sleep GetProc failed\n");
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  int sum = ((add_fn)proc)(7, 35);
  if (sum != 42) {
  fprintf(stderr, "FAIL: post-sleep addNumbers(7,35)=%d\n", sum);
  wraith_free_library(h);
  free(bytes);
  return 1;
  }
  printf("PASS: post-cronos module intact, addNumbers(7,35)=42\n");

  wraith_free_library(h);
  free(bytes);
  return 0;
}
