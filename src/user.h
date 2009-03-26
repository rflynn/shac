/* ex: set ts=4: */

#ifndef USER_H
#define USER_H

#include "shac.h"

/* group functions, used by user functions */
group_t * group_alloc(void);
void group_init(group_t *);
void group_dump(const void *); /* list_dump() callback */
group_t * group_gen(const struct group *);
void * group_dupe(const void *);
void group_free(void *);

/* user_t functions */
user_t * user_init(void);
void user_dump(const user_t *); /* used by us */
void user_free(user_t *);

/* user functions */
user_t * user_load(const char *);
void user_group_add(user_t *, const struct group *);
void user_groups_load(user_t *);
int gid_cmp(const void *, const void *);
int user_in_group(const user_t *, const gid_t);
char *user_get_current(void);
char *username_from_uid(const char *);

#endif


