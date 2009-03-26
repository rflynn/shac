
#ifndef MNT_H
#define MNT_H

#include "shac.h"

/* mntpt_t functions */
mntpt_t *mntpt_alloc(void);
void mntpt_init(mntpt_t *);
void mntpt_dump(const void *);
void *mntpt_dupe(const void *);
void mntpt_free(void *);

/* mount functions */
list_head *mnt_load(void);
int mntpt_mntdir_cmp(const void *, const void *);
mntpt_t *mnt_mntdir_find(list_head *, const char *);

#endif


