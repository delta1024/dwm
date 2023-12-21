/* See LICENSE file for copyright and license details. */
#ifndef dwm_util_h
#define dwm_util_h
#include <stddef.h>
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
#endif // dwm_util_h
