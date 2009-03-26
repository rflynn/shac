/* ex: set ts=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "user.h"
#include "util.h"

group_t * group_alloc(void)
{
	group_t *grp = xmalloc(sizeof *grp);
	grp->name = NULL;
	group_init(grp);
	return grp;
}

/* initialize group_t */
void group_init(group_t *grp)
{
#ifdef DEBUG
	assert(NULL != grp);
#endif
	xfree(grp->name);
	grp->name = NULL;
	grp->gid = GROUP_NONE;
}

/* generate a group_t from a struct group */
group_t *group_gen(const struct group *grp)
{
	group_t *g = NULL;
#ifdef DEBUG
	assert(NULL != grp);
#endif

	g = group_alloc();

	/* copy gid */
	g->gid = grp->gr_gid;

	/* copy group name if there is one */
	if (NULL != grp->gr_name)
		if (NULL == (g->name = strdup(grp->gr_name)))
			err_bail(__FILE__, __LINE__, "could not strdup group name!");

	return g;
	
}

/* dump group, callback to list_dump() by ways of user_dump() */
void group_dump(const void *ptr)
{
	const group_t *grp = NULL;
#ifdef DEBUG
	assert(NULL != ptr);
#endif
	grp = ptr;
	fprintf(stdout, "\tgroup_t(%p){\n\t\tgid: %d\n\t\tname: \"%s\"\n\t}\n",
		(void *)grp, grp->gid, grp->name);
}

void * group_dupe(const void *v)
{
	const group_t *orig;
	group_t *dupe;
#ifdef DEBUG
	assert(NULL != v);
#endif

	orig = v;
	dupe = group_alloc();

	dupe->gid = orig->gid;
	if (NULL != orig->name)
		dupe->name = strdup(orig->name);

	return dupe;
}

void group_free(void *g)
{
	group_t *grp;
#ifdef DEBUG
	assert(NULL != grp);
#endif
	grp = g;
	if (NULL == grp)
		return;
	xfree(grp->name);
	xfree(grp);
}

/**************** end groups *******************/


/* initialize user_t */
user_t * user_init(void)
{
	user_t *user;
#ifdef DEBUG
	assert(NULL != user);
#endif

	/* fetch memory or die trying */
	user = xmalloc(sizeof *user);

	user->name = NULL;
	user->is_user = 0;
	user->uid = USER_NONE;
	user->gid = GROUP_NONE;

	/* create empty list for groups */
	if (NULL == (user->groups = list_head_create()))
		err_bail(__FILE__, __LINE__, "can't create user->groups!");

	return user;
}

/* meant for our own use */
void user_dump(const user_t *user)
{
#ifdef DEBUG
	assert(NULL != user);
#endif

	fprintf(stdout, "user_t(%p){\n\tname: \"%s\"\n\tis_user: %d\n\tuid: %d\n\tgid: %d\n\tgroups:\n",
		(void *)user, user->name, (int)user->is_user, (int)user->uid, (int)user->gid);

	list_dump(user->groups, group_dump);

	fprintf(stdout, "}\n");

}

void user_free(user_t *user)
{
#ifdef DEBUG
	assert(NULL != user);
#endif

	xfree(user->name); /* free name */
	list_free(user->groups, group_free);
	xfree(user); /* free the pointer */
}

/* resolves our user,  */
user_t * user_load(const char *username)
{

	struct passwd *pw = NULL;
	user_t *u = NULL;

#ifdef DEBUG
	assert(NULL != username);
#if 1
	printf("user_load(\"%s\"):%d\n", username, __LINE__);
#endif
#endif

	/* get passwd entry or die trying */
	if (NULL == (pw = getpwnam(username)))
		fatal_invalid_user(username);

	/* user exists, init struct */
	u = user_init();

	/* copy username or die trying */
	if (NULL == (u->name = strdup(username)))
		err_nomem(__FILE__, __LINE__, strlen(username) + 1);
	
	u->uid = pw->pw_uid;
	u->gid = pw->pw_gid;

	/* load users' groups */
	user_groups_load(u);

	return u;
}

/* append data from a group entry to the user's group list */
void user_group_add(user_t *user, const struct group *grp)
{
	group_t *g = NULL;
	list_node *node = NULL;

#ifdef DEBUG
	assert(NULL != user);
#endif

	g = group_gen(grp);
	/* create shallow copy */
	node = list_node_create_deep(g, group_dupe);
	/* add entry to groups list */
	if (NULL == list_append(user->groups, node))
		err_bail(__FILE__, __LINE__, "could not append group node!");
	/* stop processing group */
	group_free(g);
}

/* loads user's groups */
void user_groups_load(user_t *user)
{

	struct group *grp = NULL;
	char **u = NULL;

#ifdef DEBUG
	assert(NULL != user);
#endif

	/* open stream to group db */
	setgrent();

	while (NULL != (grp = getgrent())) {
#ifdef DEBUG
#if 0
		printf("user_groups_load(%p):%d grp(%p)\n",
			(void *)user, __LINE__, (void *)grp);
#endif
#endif
		if (user->gid == grp->gr_gid) {
			/* matched user's gid entry from /etc/passwd */
			user_group_add(user, grp);
		} else {
			/* see if it's in /etc/group */
			u = grp->gr_mem;
			while (NULL != *u) {
#ifdef DEBUG
#if 0
			printf("user_groups_load(%p):%d u(%p) \"%s\"\n",
				(void *)user, __LINE__, (void *)u, *u);
#endif
#endif
				/* if user is in group... */
				if (0 == strcmp(user->name, *u)) {
					user_group_add(user, grp);
					break;
				}
				u++;
			}
		}
	}

	/* close group db */
	endgrent();

}

int gid_cmp(const void *a, const void *b)
{
#ifdef DEBUG
	assert(NULL != a);
	assert(NULL != b);
#if 0
	printf("gid_cmp(%d, %d)\n",
		*((gid_t *)a), *((gid_t *)b));
#endif
#endif
	return (*((gid_t *)a) == *((gid_t *)b) ? 0 : 1);
}

/* returns !0 if user is in group by gid. FIXME: perhaps this should be a macro? */
int user_in_group(const user_t *user, const gid_t gid)
{
#ifdef DEBUG
	assert(NULL != user);
	assert(NULL != user->groups);
#if 0
	printf("user_in_group(%p, %d)\n",
		(void *)user, (int)gid);
#endif
#endif
	return (NULL == list_search(list_first(user->groups), gid_cmp, &gid) ? 0 : 1);
}

/* fetch current username */
char * user_get_current(void)
{
	char *username = NULL;
	struct passwd *pw = NULL;
	uid_t uid = getuid();

	if (NULL == (pw = getpwuid(uid)))
		err_bail(__FILE__, __LINE__, "cannot fetch current username");
	
	if (NULL == (username = strdup(pw->pw_name)))
		err_bail(__FILE__, __LINE__, "cannot duplicate username");
	
	return username;
}

/* detect if we are passed a uid instead of a username, if so, translate to username or die */
char * username_from_uid(const char *user)
{
	uid_t uid = 0;
	struct passwd *pw = NULL;
	char *username = NULL;

#ifdef DEBUG
	assert(NULL != user);
#endif

	uid = (uid_t)atoi(user);

	if (NULL == (pw = getpwuid(uid)))
		fatal_invalid_user(user);

	if (NULL == (username = strdup(pw->pw_name)))
		err_bail(__FILE__, __LINE__, "cannot duplicate username");
	
	return username;
}

