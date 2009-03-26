/* ex: set ts=4: */

#ifndef PERM_H
#define PERM_H

#include "shac.h"

/* perm functions */
perm_t decode_perms(const char *);
char * encode_perms(perm_t);
permdsc_t *permdsc_alloc(void);
void permdsc_dump(const void *);
void permdsc_init(permdsc_t *);
void permdsc_set_desc(permdsc_t *, const char *);
void permdsc_set_mask(permdsc_t * ,perm_t);
void permdsc_free(void *);

#endif

