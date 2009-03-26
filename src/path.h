/* ex: set ts=4: */

#ifndef PATH_H
#define PATH_H

#include "shac.h"

/* path functions */
path_t * path_alloc(void);
void path_init(path_t *);
void *path_dupe(const void *);
void path_free(void *);
void path_dump(const void *);
list_head *path_calc_target(const char *, const char *);
list_head *path_split(list_head **, int);

#endif

