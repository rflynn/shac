/* ex: set ts=4: */

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h> /* size_t */

/* error functions */
void err_bail(const char *, int, const char *);
void err_nomem(const char *, int, size_t);
void err_cantopen(const char *, int, const char *);
void err_opendir(const char *, int, const char *);

/* specific, fatal app errors */
void fatal(const char *);
void fatal_invalid_user(const char *);
void fatal_invalid_path(const char *, int, const char *, int);

/* provide more info so i know why errors are happening */
void str_examine(const char *);
void debug_lstat(const char *, int, const char *, int);

/* wrappers/helper functions for sanity */
void *xmalloc(size_t); /* malloc() wrapper, bails on fails */
void *xrealloc(void *, size_t); /* checks NULL, will realloc */
void xfree(void *);
char *readlink_malloc(const char *);
char *strnchr(const char *, char);
char *strndup(const char *, size_t);
char *strapp(char *, const char *);
char *strcapp(char *, const char);
int strisnum(const char *);
void shac_list_list_free(void *); /* function to free list of lists */
/* calculate which power of 2 a number is */
#define log2(i) ((int)(((double)log(i))/log(2.0)))

#endif


