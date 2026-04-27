/*
 * tests/integration/test_load_trivial.c
 *
 * End-to-end smoke test.
 *
 * Reads `payload.dll` from disk, hands the bytes to wraith_load_library +
 * wraith_get_proc_address, calls `addNumbers(2, 3)`, and asserts the
 * result is 5.
 *
 * On any deviation the test exits with a non-zero status and prints a
 * one-line diagnostic - good enough for ctest under wine64.
 */

#include "wraith/wraith.h"

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef int (*add_fn)(int, int);

static int read_file_bytes(const char *path, void **out_buf, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "FAIL: fopen(\"%s\") errno=%d\n", path, errno);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n <= 0) {
        fclose(fp);
        fprintf(stderr, "FAIL: empty file \"%s\"\n", path);
        return -1;
    }
    void *buf = malloc((size_t)n);
    if (!buf) {
        fclose(fp);
        fprintf(stderr, "FAIL: malloc(%ld)\n", n);
        return -1;
    }
    size_t r = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    if (r != (size_t)n) {
        free(buf);
        fprintf(stderr, "FAIL: short read %zu/%ld\n", r, n);
        return -1;
    }
    *out_buf  = buf;
    *out_size = (size_t)n;
    return 0;
}

static int verify(const void *bytes, size_t size)
{
    wraith_handle_t h = NULL;
    wraith_status_t rc = wraith_load_library(bytes, size, NULL, &h);
    if (rc != WRAITH_OK || !h) {
        fprintf(stderr, "FAIL: wraith_load_library -> %s (%s)\n",
                wraith_status_string(rc), wraith_last_error());
        return -1;
    }

    void *proc = NULL;
    rc = wraith_get_proc_address(h, "addNumbers", &proc);
    if (rc != WRAITH_OK || !proc) {
        fprintf(stderr, "FAIL: wraith_get_proc_address(\"addNumbers\") -> %s\n",
                wraith_status_string(rc));
        wraith_free_library(h);
        return -1;
    }
    int result = ((add_fn)proc)(2, 3);
    if (result != 5) {
        fprintf(stderr, "FAIL: addNumbers(2,3) = %d (expected 5)\n", result);
        wraith_free_library(h);
        return -1;
    }

    rc = wraith_free_library(h);
    if (rc != WRAITH_OK) {
        fprintf(stderr, "FAIL: wraith_free_library -> %s\n",
                wraith_status_string(rc));
        return -1;
    }
    printf("PASS: addNumbers(2,3)=5\n");
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "payload.dll";

    void  *bytes = NULL;
    size_t size  = 0;
    if (read_file_bytes(path, &bytes, &size) != 0) {
        return 1;
    }

    int rc = verify(bytes, size);
    free(bytes);
    if (rc == 0) {
        printf("OK: %s\n", wraith_version());
    }
    return rc == 0 ? 0 : 1;
}
