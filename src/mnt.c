
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "mnt.h"

extern int getsubopt(char **, char * const *, char **);

/* allocate and return mntpt_t struct */
mntpt_t *mntpt_alloc(void)
{
	mntpt_t *mnt;
#ifdef DEBUG
	assert(NULL != mnt);
#endif
	mnt = xmalloc(sizeof *mnt);
	mnt->mntdir = NULL;
	mnt->mntdev = NULL;
	return mnt;
}

/* initialize a mntpt_t struct */
/* can also be used to clear out existing mntpt */
void mntpt_init(mntpt_t *mnt)
{
#ifdef DEBUG
	assert(NULL != mnt);
#endif
	/* free members if they're allocated */
	if (NULL != mnt->mntdir) {
#ifdef DEBUG
		printf("mntpt_init:%d mnt(%p)->mntdir(%p)\n",
			__LINE__, (void *)mnt, (void *)mnt->mntdir);
#endif
		xfree(mnt->mntdir);
	}
	if (NULL != mnt->mntdev) {
#ifdef DEBUG
		printf("mntpt_init:%d mnt(%p)->mntdev(%p)\n",
			__LINE__, (void *)mnt, (void *)mnt->mntdev);
#endif
		xfree(mnt->mntdev);
	}
	mnt->perms = 0;
}

/* meant to be used as callback from list_dump() */
/* may be NULL */
void mntpt_dump(const void *v)
{
	const mntpt_t *mnt = v;
	printf("\t\tmntpt_t(%p){\n", (void *)mnt);
	if (NULL != mnt)
		printf("\t\t\tdir: \"%s\"\n\t\t\tdev: \"%s\"\n\t\t\tperms: %x\n",
			mnt->mntdir, mnt->mntdev, mnt->perms);
	printf("\t\t}\n");
}

/* duplicates mntpt_t, can be used for list's list_node_create_deep */
void *mntpt_dupe(const void *v)
{
	const mntpt_t *orig;
	mntpt_t *dupe;
#ifdef DEBUG
	assert(NULL != v);
#endif
	orig = v;
	dupe = mntpt_alloc();
#if 0
	mntpt_init(dupe);
#endif
	if (NULL != orig->mntdir)
		dupe->mntdir = strdup(orig->mntdir);
	if (NULL != orig->mntdev)
		dupe->mntdev = strdup(orig->mntdev);
	dupe->perms = orig->perms;
	return dupe;
}

void mntpt_free(void *v)
{
	mntpt_t *mnt;
#ifdef DEBUG
	assert(NULL != v);
#endif
	mnt = v;
	if (NULL == mnt)
		return;
	xfree(mnt->mntdir);
	xfree(mnt->mntdev);
	xfree(mnt);
}

/* copying info about system mounts into a list */
list_head * mnt_load(void)
{

	list_head *list; /* list we'll be returning */
	list_node *node; /* list node */
#ifdef SHAC_MNTFILE
	FILE *mnt_fp; /* fp from mtab */
	struct mntent *mnt_ent; /* mntpt entries from mtab */
#else
	struct fstab *fs_ent;
#endif
	mntpt_t *mnt;
	char *subopts, *suboptval;
	enum {
		OPT_RO = 0, /* read only */
		OPT_RW, /* explict read and writable */
		OPT_RDONLY, /* "rdonly" for freebsd */
		OPT_NOEXEC, /* noexec option */
		OPT_DEFAULTS /* defaults, assuming rwx */
	};
	char * const mount_opts[] = {
		"ro",
		"rw",
		"rdonly",
		"noexec",
		"defaults",
		NULL
	};

	/* init list */
	if (NULL == (list = list_head_create()))
		err_bail(__FILE__, __LINE__, "could not create list");

#ifdef DEBUG
#ifdef SHAC_MNTFILE
	printf("mnt_load:%d SHAC_MNTFILE: \"%s\"\n", __LINE__, SHAC_MNTFILE);
#endif
#endif

	mnt = mntpt_alloc(); /* make room */

/* if this is glibc, open file explicitly... */
#ifdef LINUX
	/* open mtab or die trying */
	if (NULL == (mnt_fp = setmntent(SHAC_MNTFILE, "r")))
		err_cantopen(__FILE__, __LINE__, SHAC_MNTFILE);
	/* read each mtab entry and copy what we need out of it */
	while (NULL != (mnt_ent = getmntent(mnt_fp))) {
		mntpt_init(mnt); /* clear out mnt */
		if (NULL == (mnt->mntdir = strdup(mnt_ent->mnt_dir)))
			err_bail(__FILE__, __LINE__, "could not strdup mnt_ent->mnt_dir");
		if (NULL == (mnt->mntdev = strdup(mnt_ent->mnt_fsname)))
			err_bail(__FILE__, __LINE__, "could not strdup mnt_ent->mnt_dev");
		/* read mount subopts */
		subopts = mnt_ent->mnt_opts;
#else /* if this is FreeBSD (and others?) use this: */
		/* freebsd doesn't define the file, we just call setgrent */
		if (0 == setfsent())
			err_bail(__FILE__, __LINE__, "setgrent() failed, can't read mount file");
		while (NULL != (fs_ent = getfsent())) {
			mntpt_init(mnt);
			if (NULL == (mnt->mntdir = strdup(fs_ent->fs_file)))
				err_bail(__FILE__, __LINE__, "could not strdup fs_ent->fs_file");
			if (NULL == (mnt->mntdev = strdup(fs_ent->fs_spec)))
				err_bail(__FILE__, __LINE__, "could not strdup fs_ent->fs_spec");
			/* read mount subopts */
			subopts = fs_ent->fs_mntops;
#endif

#ifdef DEBUG
#if 1
			printf("mnt_load()%d: mnt:\n", __LINE__);
			mntpt_dump(mnt);
#endif
#endif

			while ('\0' != *subopts) {
#ifdef DEBUG
				printf("mnt_load()%d: subopts(%p) \"%s\"\n",
					__LINE__, (void *)subopts, subopts);
#endif
				switch (getsubopt(&subopts, mount_opts, &suboptval)) {
				case OPT_RO:
				case OPT_RDONLY:
#ifdef DEBUG
#if 1
					printf("mnt_load()%d:OPT_(RO|RDONLY)!\n", __LINE__);
#endif
#endif
					mnt->perms |= PERM_READ; /* reading allowed */
					mnt->perms &= (PERM_MASK ^ PERM_WRIT); /* writing disallowed */
					break;
				case OPT_NOEXEC:
#ifdef DEBUG
#if 1
					printf("mnt_load()%d:OPT_NOEXEC!\n", __LINE__);
#endif
#endif
					mnt->perms &= (PERM_MASK ^ PERM_EXEC); /* exec disallowed */
					break;
				case OPT_RW:
#ifdef DEBUG
#if 1
					printf("mnt_load()%d:OPT_RW!\n", __LINE__);
#endif
#endif
				/* fall through... */
				case OPT_DEFAULTS: /* we'll assume default is rw */
#ifdef DEBUG
#if 1
					printf("mnt_load()%d:OPT_DEFAULTS!\n", __LINE__);
#endif
#endif
					mnt->perms |= PERM_READ; /* reading allowed */
					mnt->perms |= PERM_WRIT; /* writing allowed */
					break;
				default:
					break;
				}
				/* mnt now complete, goes onto the list */
				/* first make a duplicate */
			}
			/* subopts read */
			/* create copy of mnt */
			node = list_node_create_deep(mnt, mntpt_dupe);
			/* append node to list */
			if (NULL == list_append(list, node))
				err_bail(__FILE__, __LINE__, "could not append mnt to list");

		}

#ifdef SHAC_MNTFILE
	/* close mtab */
	endmntent(mnt_fp);
#else
	endfsent();
#endif

	mntpt_free(mnt);

	return list;

}

int mntpt_mntdir_cmp(const void *mnt, const void *find)
{
#ifdef DEBUG
	assert(NULL != mnt);
	assert(NULL != find);
#if 0
	printf("mntpt_mntdir_cmp()%d mnt(%p)->mntdir: \"%s\", find(%p) \"%s\"\n",
		__LINE__, mnt, ((mntpt_t *)mnt)->mntdir, find, (char *)find);
#endif
#endif
	return strcmp(((mntpt_t *)mnt)->mntdir, (char *)find);
}

mntpt_t *mnt_mntdir_find(list_head *mnts, const char *path)
{
	void * search = NULL;
#ifdef DEBUG
	assert(NULL != mnts);
	assert(NULL != path);
#endif
	search = list_search(list_first(mnts), mntpt_mntdir_cmp, path);
	return (mntpt_t *)list_node_data(search);
}
