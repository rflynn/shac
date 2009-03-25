/* ex: set ts=4: */
/****************************************************************************/
/*                                                                          */
/* shac (SHow ACess) v0.01                                                  */
/*                                                                          */
/* Written by Ryan Flynn aka pizza                                          */
/* Copyright 2004 by Ryan Flynn (pizza@parseerror.com)                      */
/*                                                                          */
/* Inspired by Michael Zalewski's "Fallen" Ideas #3                         */
/*  (http://lcamtuf.coredump.cx/soft/STUPID_IDEAS.txt)                      */
/*                                                                          */
/****************************************************************************
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h> /* malloc, getsubopt */
#include <string.h> /* strchr, strdup, etc */
#ifdef LINUX
	#include <mntent.h> /* glibc-ish: setgrent(), getgrent() endgrent(), etc. */
#else
	#include <fstab.h> /* freebsd-ish:  */
#endif
#include <unistd.h> /* getcwd(), getopt() */
#include <pwd.h> /* struct passwd, getpwnam */
#include <grp.h> /* struct group, setgrent, getgrent, endgrent */
#include <sys/stat.h> /* struct stat, stat */
#include <math.h> /* log() */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* errno, of course */
#include <sys/param.h> /* MAXSYMLINKS */
#include <dirent.h> /* directory entry */
#include <limits.h> /* PATH_MAX */
#include "llist.h"
#include "shac.h"

#define USAGE	"Usage: shac [-u user] [-p perms] file\n" \
				"Type shac -h to see details\n"

#define HELP	"Usage: shac [options] file\n" \
				"Options:\n" \
				"  -u user    user/uid you want to check\n" \
				"             defaults to current user\n" \
				"  -p perms   perms can be a combination of:\n" \
				"               r (read)\n" \
				"               w (write)\n" \
				"               x (execute)\n" \
				"               c (create)\n" \
				"               d (delete)\n" \
				"             defaults to 'r'\n" \
				"  -v         toggle verbose mode\n" \
				"  -vv        toggle very verbose mode\n" \
				"  -vvv       toggle very very verbose mode\n" \
				"\n" \
				"Example: shac -u root -p rw /etc/hosts\n" \
				"  checks if root can read and write the file /etc/hosts\n" \
				"\n" \
				"Info: http://parseerror.com/shac/\n" \
				"Bugs: pizza@parseerror.com\n" \
				"\n" \
				"Anonymous read-only access to the latest source is available\n" \
				"via subversion with the following command:\n" \
				"  git clone git://github.com/pizza/shac.git ~/shac\n\n"
/* following line used as a ruler for the above HELP message so i can keep it 78 chars wide + \n */
/*				"                                                                              \n" */

#define VERBOSE(level, append, format, rest...) \
	if (flag_verbose >= level) { \
		char buf[1024]; \
		snprintf(buf, sizeof buf, format, ## rest); \
		verbose_add(buf, append); \
	}


/* error functions */
static void err_bail(const char *, int, const char *);
static void err_nomem(const char *, int, size_t);
static void err_cantopen(const char *, int, const char *);
static void err_opendir(const char *, int, const char *);

/* specific, fatal app errors */
static void fatal(const char *);
static void fatal_invalid_user(const char *);
static void fatal_invalid_path(const char *, int, const char *, int);

/* provide more info so i know why errors are happening */
static void str_examine(const char *);
static void debug_lstat(const char *, int, const char *, int);

/* initialize global variables before the program starts */
static void init_globals(void);

/* wrappers/helper functions for sanity */
static void *xmalloc(size_t); /* malloc() wrapper, bails on fails */
static void *xrealloc(void *, size_t); /* checks NULL, will realloc */
static void xfree(void *);
static char *readlink_malloc(const char *);
static char *strnchr(const char *, char);
static char *strndup(const char *, int);
static char *strapp(char *, const char *);
static char *strcapp(char *, const char);
static int strisnum(const char *);
static void shac_list_list_free(void *); /* function to free list of lists */
/* calculate which power of 2 a number is */
#define log2(i) ((int)(((double)log(i))/log(2.0)))

/* verbose output functions */
static void verbose_add(const char *, int);
static void verbose_flush(void);
static void verbose_clear(void);

/* mntpt_t functions */
static mntpt_t *mntpt_alloc(void);
static void mntpt_init(mntpt_t *);
static void mntpt_dump(const void *);
static void *mntpt_dupe(const void *);
static void mntpt_free(void *);

/* mount functions */
static list_head *mnt_load(void);
static int mntpt_mntdir_cmp(const void *, const void *);
static mntpt_t *mnt_mntdir_find(list_head *, const char *);

/* perm functions */
static perm_t decode_perms(const char *);
static char * encode_perms(perm_t);
static permdsc_t *permdsc_alloc(void);
static void permdsc_dump(const void *);
static void permdsc_init(permdsc_t *);
static void permdsc_set_desc(permdsc_t *, const char *);
static void permdsc_set_mask(permdsc_t * ,perm_t);
static void permdsc_free(void *);

/* group functions, used by user functions */
static group_t * group_alloc(void);
static void group_init(group_t *);
static void group_dump(const void *); /* list_dump() callback */
static group_t * group_gen(const struct group *);
static void * group_dupe(const void *);
static void group_free(void *);

/* user_t functions */
static user_t * user_init(void);
static void user_dump(const user_t *); /* used by us */
static void user_free(user_t *);

/* user functions */
static user_t * user_load(const char *);
static void user_group_add(user_t *, const struct group *);
static void user_groups_load(user_t *);
static int gid_cmp(const void *, const void *);
static int user_in_group(const user_t *, const gid_t);
static char *user_get_current(void);
static char *username_from_uid(const char *);

/* path functions */
static path_t * path_alloc(void);
static void path_init(path_t *);
static void *path_dupe(const void *);
static void path_free(void *);
static void path_dump(const void *);
static list_head *path_calc_target(const char *, const char *);
static list_head *path_split(list_head **, int);

/* reason functions */
static reason_t *reason_alloc(void);
static void reason_init(reason_t *);
static void reason_dump(const void *);
static void reason_free(reason_t *);

static void perm_calc(const char *, const char *, permdsc_t *);
static void report(reason_t *, user_t *);
static void report_calc(reason_t *, path_t *, user_t *, perm_t, permdsc_t *, perm_t *, int);
static int report_gen(list_head *, user_t *, permdsc_t *, int);

/* strictly for testing */
static void test_stuff(void);
static void test_path_split(void);
static void test_mnt_load(void);


/* * * * * * * * * external decl * * * * * * * * * * */
extern char *optarg;
extern int optind, opterr, optopt;
extern char *suboptarg;
extern int getsubopt(char **, char * const *, char **);

/* * * * * * * * * globals * * * * * * * * * * */

static list_head *MNTPTS; /* mount points */
static list_head *VERBOSE_MSG; /* verbose output queue, to deal with output order issues */
static int unsigned flag_verbose = 0;



/* general fatal error... print msg and then exit with failure */
static void err_bail(const char *file, int line, const char *msg)
{
#ifdef DEBUG
	assert(NULL != file);
	assert(NULL != msg);
#endif
	fprintf(stderr, "%s:%d:%s!\nbailing!\n", file, line, msg);
	exit(EXIT_FAILURE);
}

/* could not allocate memory. this is called from xmalloc */
static void err_nomem(const char *file, int line, size_t bytes)
{
#ifdef DEBUG
	assert(NULL != file);
#endif

	fprintf(stderr, "%s:%d: could not allocate %d bytes!\n",
		file, line, bytes);

	exit(EXIT_FAILURE);
}

/* could not open file fatal error */
static void err_cantopen(const char *file, int line, const char *filename)
{
#ifdef DEBUG
	assert(NULL != file);
	assert(NULL != filename);
#endif

	fprintf(stderr, "%s:%d: could not open file '%s'\n",
		file, line, filename);

	exit(EXIT_FAILURE);
}

/* could not open dir fatal error */
static void err_opendir(const char *file, int line, const char *dirname)
{
#ifdef DEBUG
	assert(NULL != file);
	assert(NULL != dirname);
#endif

	fprintf(stderr, "%s:%d: could not open directory '%s'\n",
		file, line, dirname);

	exit(EXIT_FAILURE);
}

/* user specified does not exist */
static void fatal_invalid_user(const char *username)
{
#ifdef DEBUG
	assert(NULL != username);
#endif
	fprintf(stderr, "ERR %s '%s' does not exist\n",
		(strisnum(username) ? "uid" : "username"),
		username
	);
	exit(EXIT_FAILURE);
}

/* path does not exist */
static void fatal_invalid_path(const char *file, int line, const char *path, int err)
{
#ifdef DEBUG
	assert(NULL != path);
#endif
	fprintf(stderr, "(%s:%d) ERR file '%s' %s\n",
		file, line, path, (0 == err ? "" : strerror(err)));
	exit(EXIT_FAILURE);
}

/* general fatal error, like an option */
static void fatal(const char *msg)
{
#ifdef DEBUG
	assert(NULL != msg);
#endif
	fprintf(stderr, "ERR %s\n", msg);
	exit(EXIT_FAILURE);
}

#if 0

void debug_lstat(const char *file, int line, const char *lstatme, int errno)
{
#ifdef DEBUG
	assert(NULL != file);
	assert(NULL != lstatme);
#endif
	fprintf(stderr, "%s:%d: received errno %d (%s) on lstat(\"%s\"): ",
		file, line, errno,
		(errno == EBADF ? "EBADF" :
		errno == ENOENT ? "ENOENT" :
		errno == ENOTDIR ? "ENOTDIR" :
		errno == ELOOP ? "ELOOP" :
		errno == EFAULT ? "EFAULT" :
		errno == EACCES ? "EACCES" :
		errno == ENOMEM ? "ENOMEM" :
		errno == ENAMETOOLONG ? "ENAMETOOLONG" :
		"unknown???"),
		lstatme
	);
}

#endif

/* die if mem allocation fails */
static void * xmalloc(size_t bytes)
{
	void *ptr;

	if (NULL == (ptr = malloc(bytes)))
		err_nomem(__FILE__, __LINE__, bytes);

#ifdef DEBUG
	printf("xmalloc() %u bytes at %p\n", bytes, (void *)ptr);
#endif

	return ptr;
}

/* realloc if !NULL, malloc otherwise */
static void * xrealloc(void *ptr, size_t bytes)
{
	if (NULL == (ptr = realloc(ptr, bytes)))
		err_nomem(__FILE__, __LINE__, bytes);
#ifdef DEBUG
	printf("xrealloc() %u bytes at %p\n", bytes, (void *)ptr);
#endif
	return ptr;
}

/* don't try to free if already NULL */
static void xfree(void *ptr)
{
	if (NULL != ptr) {
#ifdef DEBUG
		printf("xfree(%p)\n", ptr);
#endif
		free(ptr);
	}
}

/* allocate and read buffer for the filename that filename, a symlink, points to */
static char * readlink_malloc(const char *filename)
{
	int size = 64;
	char *buf = NULL;

#ifdef DEBUG
	assert(NULL != filename);
#endif

	while (1) {
		int nchars;
		buf = xrealloc(buf, size); /* grab some [more] space */
		nchars = readlink(filename, buf, size); /* read filename */
		if (nchars < 0) { /* error */
			xfree(buf);
			return NULL;
		} else if (nchars < size) /* life is good */
			return buf;
		size *= 2; /* increase buffer size */
	}
}

/* a general, crappy little function for examining the contents of a char*, escapes non-printable chars */
static void str_examine(const char *c)
{
	printf("string(%p): ", (void *)c);
	if (NULL != c) {
		while (*c != '\0') {
			if (*c < 0x20 || *c > 0x7E) /* <32 || >126 */
				printf("0x%x", *c);
			else
				printf("%c", *c);
			c++;
		}
		printf("\\0");
	}
	printf("\n");
}

/* returns pointer to first char in s that's *NOT* c, or NULL if we run out */
static char * strnchr(const char *s, char c)
{
#ifdef DEBUG
	assert(NULL != s);
#endif
	if (NULL == s)
		return NULL;
	for (; *s == c && *s != '\0'; s++)
		;
	return (char *)('\0' == *s ? NULL : s);
}

/* duplicates s to a max of n bytes */
static char * strndup(const char *s, int n)
{
	char *dupe = NULL;
#ifdef DEBUG
	assert(NULL != s);
	assert(n >= 0);
	printf("strndup(\"%s\", %d):%d\n", s, n, __LINE__);
#endif
	dupe = xmalloc(n + 1);
	strncpy(dupe, s, n);
	memset(dupe + n, '\0', 1); /* append \0 */
	return dupe;
}

/* make enough room in s for app */
static char * strapp(char *s, const char *app)
{
	char *c = NULL;
	char *ends = NULL, *enda = NULL;
#ifdef DEBUG
	printf("strapp(%p, %p):%d: s: %p, app: \"%s\"\n",
		(void *)s, (void *)app, __LINE__, s, app);
#endif
	ends = (char *)((NULL != s) ? strchr(s, '\0') : s);
	enda = (char *)((NULL != app) ? strchr(app, '\0') : app);
#ifdef DEBUG
	printf("strapp():%d ends-s: %d, enda-app: %d, total: %d\n",
		__LINE__, ends-s, enda-app, (ends - s) + (enda - app) + 1);
#endif
	c = xmalloc((ends - s) + (enda - app) + 1);
#if 1
	*c = '\0';
	*(c + 1) = '\0';
#endif
#if 1
	if (NULL != s)
		c = strcat(c, s);
#ifdef DEBUG
	printf("strapp():%d ", __LINE__);
	str_examine(c);
#endif
#endif
	xfree(s);
#if 1
	if (NULL != app)
		c = strcat(c, app);
#endif
#ifdef DEBUG
	printf("strapp():%d ", __LINE__);
	str_examine(c);
#endif
	return c;
}

/* appends char c to string s */
static char *strcapp(char *s, const char c)
{
	char *t = NULL;
	char *end = NULL;
#ifdef DEBUG
#endif
	if (NULL != s)
		end = strchr(s, '\0');
	else
		end = s;
#ifdef DEBUG
	printf("strcapp(%p, %c)%d: s: %p, s: \"%s\", end: %p, (end - s): %d\n",
		(void *)s, c, __LINE__, (void *)s, s, (void *)end, end - s);
#endif
	t = xmalloc((end - s) + 1 + 1);
	*t = '\0';
	*(t + 1) = '\0';
	if (NULL != s) {
		t = strcat(t, s);
		xfree(s);
		end = strchr(t, '\0');
	} else {
		end = t;
	}
#ifdef DEBUG
	printf("strcapp(%p, %c)%d: t: %p, t: \"%s\", end: %p, (end - t): %d\n",
		(void *)s, c, __LINE__, (void *)t, t, (void *)end, end - t);
#endif
	*end++ = c;
	*end = '\0';
	return t;
}

/* returns 1 if s is <1 char or not a valid integer */
static int strisnum(const char *s)
{
	const char *c = NULL;
#ifdef DEBUG
	assert(NULL != s);
#endif
	for (c = s; *c != '\0'; c++)
		if (!isdigit(*c))
			return 0;
	return (int)(c != s);
}

/* normal list_free doesn't have the right signature to be used in list_free() */
/* there is no practical way */
static void shac_list_list_free(void *v)
{
	list_head *head;
#ifdef DEBUG
	assert(NULL != v);
#endif
	head = v;
	list_free(head, NULL);
}

/****************************** perm functions ******************************/

/*  convert human-readable repr of perms to internal repr */
static perm_t decode_perms(const char *rawperms)
{
	perm_t perms = PERM_NONE;
	char *c = NULL;
#ifdef DEBUG
	assert(NULL != rawperms);
#if 1
	printf("decode_perms:%d rawperms: \"%s\"\n",
		__LINE__, rawperms);
#endif
#endif
	for (c = (char *)rawperms; *c != '\0'; c++)
		switch (*c) {
		case 'r':
			perms |= PERM_READ;
			break;
		case 'w':
			perms |= PERM_WRIT;
			break;
		case 'x':
			perms |= PERM_EXEC;
			break;
		case 'c':
			perms |= PERM_CREA;
			break;
		case 'd':
			perms |= PERM_DELE;
			break;
		default:
#ifdef DEBUG
			printf("decode_perms:%d unknown perm '%c'\n", __LINE__, *c);
#endif
			break;
		}
	return perms;
}

/* convert perm_t to nicely formatted human readable */
/* why not just use user input? because we want the stuff normalized */
static char *encode_perms(const perm_t perm)
{
	const unsigned char N = 6; /* number of one-letter perms (r,w,x,c,d,s) */
	unsigned char n = 0;
	char *p; /* has to be a pointer since we can't return local var */

	p = xmalloc(N + 1); /* plus \0 */

	memset(p, '\0', N + 1); /* zero out, guarentee NUL-terminated */

	if (PERM_NONE != (perm & PERM_READ))
		*(p + n++) = 'r';

	if (PERM_NONE != (perm & PERM_WRIT))
		*(p + n++) = 'w';

	if (PERM_NONE != (perm & PERM_EXEC))
		*(p + n++) = 'x';

	if (PERM_NONE != (perm & PERM_CREA))
		*(p + n++) = 'c';

	if (PERM_NONE != (perm & PERM_DELE))
		*(p + n++) = 'd';

	return p;
}

/*************************** permdsc_t functions ***************************/

/* allocate and return permdsc_t structure */
static permdsc_t * permdsc_alloc(void)
{
	permdsc_t *p = xmalloc(sizeof *p);
	p->dsc = NULL;
	p->mask = PERM_NONE;
	return p;
}

/* clear out existing permdsc_t */
static void permdsc_init(permdsc_t *p)
{
#ifdef DEBUG
	assert(NULL != p);
#endif
	xfree(p->dsc);
	p->dsc = NULL;
	p->mask = PERM_NONE;
}

/* set human-readable perms, set mask based on new perms */
static void permdsc_set_desc(permdsc_t *p, const char *dsc)
{
#ifdef DEBUG
	assert(NULL != p);
	assert(NULL != dsc);
#endif
	permdsc_init(p);
	p->mask = decode_perms(dsc);
	p->dsc = encode_perms(p->mask);
}

/* set mask, generate hman-readable perms based on it */
static void permdsc_set_mask(permdsc_t *p, const perm_t mask)
{
#ifdef DEBUG
	assert(NULL != p);
#endif
	permdsc_init(p);
	p->mask = mask;
	p->dsc = encode_perms(p->mask);
}

/* examine permdsc_t struct */
static void permdsc_dump(const void *v)
{
	const permdsc_t *perm;
#ifdef DEBUG
	assert(NULL != v);
#endif
	perm = v;
	printf("permdsc_t(%p){\n", (void *)perm);
	if (NULL != perm)
		printf("\tdsc(%p): \"%s\"\n\tmask: %d\n",
			(void *)perm->dsc, perm->dsc, perm->mask);
	printf("}\n");
}

/* free allocate permdsc_t struct */
/* doesn't need to be void *, but what the hell */
static void permdsc_free(void *v)
{
	permdsc_t *p;
#ifdef DEBUG
	assert(NULL != v);
#endif
	p = v;
	permdsc_init(p);
	xfree(p);
}


/*************************** mntpt_t functions ******************************/

/* allocate and return mntpt_t struct */
static mntpt_t *mntpt_alloc(void)
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
static void mntpt_init(mntpt_t *mnt)
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
static void mntpt_dump(const void *v)
{
	const mntpt_t *mnt = v;
	printf("\t\tmntpt_t(%p){\n", (void *)mnt);
	if (NULL != mnt)
		printf("\t\t\tdir: \"%s\"\n\t\t\tdev: \"%s\"\n\t\t\tperms: %x\n",
			mnt->mntdir, mnt->mntdev, mnt->perms);
	printf("\t\t}\n");
}

/* duplicates mntpt_t, can be used for list's list_node_create_deep */
static void *mntpt_dupe(const void *v)
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

static void mntpt_free(void *v)
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
static list_head * mnt_load(void)
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

static int mntpt_mntdir_cmp(const void *mnt, const void *find)
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

static mntpt_t *mnt_mntdir_find(list_head *mnts, const char *path)
{
	void * search = NULL;
#ifdef DEBUG
	assert(NULL != mnts);
	assert(NULL != path);
#endif
	search = list_search(list_first(mnts), mntpt_mntdir_cmp, path);
	return (mntpt_t *)list_node_data(search);
}

/************************** group functions *********************************/

static group_t * group_alloc(void)
{
	group_t *grp = xmalloc(sizeof *grp);
	grp->name = NULL;
	group_init(grp);
	return grp;
}

/* initialize group_t */
static void group_init(group_t *grp)
{
#ifdef DEBUG
	assert(NULL != grp);
#endif
	xfree(grp->name);
	grp->name = NULL;
	grp->gid = GROUP_NONE;
}

/* generate a group_t from a struct group */
static group_t *group_gen(const struct group *grp)
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
static void group_dump(const void *ptr)
{
	const group_t *grp = NULL;
#ifdef DEBUG
	assert(NULL != ptr);
#endif
	grp = ptr;
	fprintf(stdout, "\tgroup_t(%p){\n\t\tgid: %d\n\t\tname: \"%s\"\n\t}\n",
		(void *)grp, grp->gid, grp->name);
}

static void * group_dupe(const void *v)
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

static void group_free(void *g)
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
static user_t * user_init(void)
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
static void user_dump(const user_t *user)
{
#ifdef DEBUG
	assert(NULL != user);
#endif

	fprintf(stdout, "user_t(%p){\n\tname: \"%s\"\n\tis_user: %d\n\tuid: %d\n\tgid: %d\n\tgroups:\n",
		(void *)user, user->name, (int)user->is_user, (int)user->uid, (int)user->gid);

	list_dump(user->groups, group_dump);

	fprintf(stdout, "}\n");

}

static void user_free(user_t *user)
{
#ifdef DEBUG
	assert(NULL != user);
#endif

	xfree(user->name); /* free name */
	list_free(user->groups, group_free);
	xfree(user); /* free the pointer */
}

/* resolves our user,  */
static user_t * user_load(const char *username)
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
static void user_group_add(user_t *user, const struct group *grp)
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
static void user_groups_load(user_t *user)
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

static int gid_cmp(const void *a, const void *b)
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
static int user_in_group(const user_t *user, const gid_t gid)
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
static char * user_get_current(void)
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
static char * username_from_uid(const char *user)
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

/************************ path_t functions **********************************/

static path_t * path_alloc(void)
{
	path_t *path;
	path = xmalloc(sizeof *path);
#ifdef DEBUG
	printf("%s:%d:path:%p\n", __FILE__, __LINE__, (void *)path);
#endif
	/* set all pointers to NULL explicitly, because if we alloc and there's junk floating around
	init will think it's an allocated pointer and try to free it */
	path->abspath = NULL;
	path->dir = NULL;
	path->component = NULL;
	path->symlink = NULL;
	path->mntpt = NULL;
	path_init(path);
	return path;
}

/* clear out already-allocated path_t */
static void path_init(path_t *path)
{
	if (NULL == path)
		return;
#ifdef DEBUG
	printf("path_init:%d(%p)\n", __LINE__, (void *)path);
	path_dump(path);
#endif
	xfree(path->abspath);
		path->abspath = NULL;
	xfree(path->dir);
		path->dir = NULL;
	xfree(path->component);
		path->component = NULL;
	xfree(path->symlink);
		path->symlink = NULL;
	path->uid = USER_NONE;
	path->gid = GROUP_NONE;
	path->mode = 0;
	path->status = 0;
	path->mntpt = NULL; /* don't free mntpt, it's not ours */
}

static void * path_dupe(const void *v)
{
	const path_t *orig;
	path_t *dupe;
#ifdef DEBUG
	assert(NULL != v);
#endif
	if (NULL == v)
		err_bail(__FILE__, __LINE__, "trying to duplicate a NULL path!");
	orig = v; /* pointer passed is to a path_t */
	dupe = path_alloc(); /* allocate new path_t */
	if (NULL != orig->abspath)
		dupe->abspath = strdup(orig->abspath);
	if (NULL != orig->dir)
		dupe->dir = strdup(orig->dir);
	if (NULL != orig->component)
		dupe->component = strdup(orig->component);
	if (NULL != orig->symlink)
		dupe->symlink = strdup(orig->symlink);
	dupe->mode = orig->mode;
	dupe->uid = orig->uid;
	dupe->gid = orig->gid;
	dupe->status = orig->status;
#if 0
	if (NULL != orig->mntpt)
		dupe->mntpt = mntpt_dupe(orig->mntpt);
#endif
	/* don't want to actually copy mntpt, just point to original copy */
	dupe->mntpt = orig->mntpt;
	return dupe;
}

/* callback for list_node_free to free path_t */
static void path_free(void *v)
{
	path_t *p;
	if (NULL == v)
		return;
	p = v;
	path_init(p); /* clears sub-items */
	xfree(p);
}

/* callback for list_dump to inspect a path_t */
static void path_dump(const void *v)
{
	const path_t *p;
#ifdef DEBUG
#if 0
	printf("path_dump(%p)\n", v);
#endif
#endif
	p = v;
	/* these are all split up because i was getting segfaults on invalid path_ts */
	printf("\tpath_t(%p){\n", (void *)p);
	if (NULL != p) { /* handle NULLs ok */
		printf("\t\tabspath(%p): \"%s\"\n", (void *)p->abspath, p->abspath);
		printf("\t\tdir(%p): \"%s\"\n", (void *)p->dir, p->dir);
		printf("\t\tcomponent(%p): \"%s\"\n", (void *)p->component, p->component);
		printf("\t\tsymlink(%p): \"%s\"\n", (void *)p->symlink, p->symlink);
		printf("\t\tuid: %d\n\t\tgid: %d\n\t\tstatus: %d\n\t\tmode: %d\n", p->uid, p->gid, p->status, p->mode);
		printf("\t\t\tur:%d, uw:%d, ux:%d, us:%d, gr:%d, gw:%d, gx:%d, gs:%d, or:%d, ow:%d, ox:%d, sb:%d\n",
			p->mode & S_IRUSR, p->mode & S_IWUSR, p->mode & S_IXUSR, p->mode & S_ISUID,
			p->mode & S_IRGRP, p->mode & S_IWGRP, p->mode & S_IXGRP, p->mode & S_ISGID,
			p->mode & S_IROTH, p->mode & S_IWOTH, p->mode & S_IXOTH,
			p->mode & S_ISVTX
		);
		/* dump mntpt */
		mntpt_dump(p->mntpt);
	}
	printf("\t}\n");
}

/*************************** reason_t functions *****************************/

static reason_t * reason_alloc(void)
{
	reason_t *reas = xmalloc(sizeof *reas);
	reas->path = NULL;
	return reas;
}


/* clear reason_t struct */
static void reason_init(reason_t *reas)
{
#ifdef DEBUG
	assert(NULL != reas);
#endif
	if (NULL == reas)
		return;
	reas->label = RPT_NONE;
	reas->yes = REAS_NONE;
	reas->no = REAS_NONE;
	reas->path = NULL;
}

static void reason_dump(const void *v)
{
	const reason_t *reas;
#ifdef DEBUG
	assert(NULL != v);
#endif
	reas = v;
	printf("reason_t(%p){\n", (void *)reas);
	if (NULL != reas) {
		printf("\tlabel: %d\n\tyes: %d\n\tno: %d\n",
			reas->label, reas->yes, reas->no);
	}
	path_dump(reas->path);
	printf("}\n");
}

static void reason_free(reason_t *reas)
{
#ifdef DEBUG
	assert(NULL != reas);
#endif
	reason_init(reas);
	free(reas);
}

/*********************** "algorithm" functions ******************************/

/* suppled could be dirty, we need to figure out if it's absolute, if it's not... */
/* we combine it with cwd */
/* if it is, we just clean it up */
static list_head *path_calc_target(const char *supplied, const char *cwd)
{
	list_head *list = NULL;
	list_node *node = NULL;
	char *path = NULL, *pos1 = NULL, *pos2 = NULL, *end = NULL;
	char *tmp = NULL;
	int len = 0;

#ifdef DEBUG
	assert(NULL != supplied);
	printf("path_calc_target ---------------\n");
#endif

	if (*supplied != PATHSEP) { /* non-absolute path */
#ifdef DEBUG
		printf("path_calc_target():%d supplied: \"%s\", cwd \"%s\"\n",__LINE__,
			supplied, cwd);
#endif
		tmp = strchr(cwd, '\0');
#ifdef DEBUG
		printf("path_calc_target():%d allocating %d bytes for path\n",
			__LINE__, (tmp - cwd) + strlen(supplied) + 1 + 1);
#endif
		path = xmalloc((tmp - cwd) + strlen(supplied) + 1 + 1);
		strncpy(path, cwd, tmp - cwd);
		tmp = path + (tmp - cwd); /* set tmp at the end of path */
		*tmp++ = PATHSEP; /* add sep */
		strcpy(tmp, supplied);
#ifdef DEBUG
	printf("path_calc_target:%d path: \"%s\"\n", __LINE__, path);
#endif
	} else { /* absolute path */
		if (NULL == (path = strdup(supplied)))
			err_bail(__FILE__, __LINE__, "could not duplicate supplied into path");
	}

#ifdef DEBUG
	printf("path_calc_target:%d ", __LINE__);
	str_examine(path);
#endif

	/* path now contains full, possibly ugly path */

	list = list_head_create();

	pos1 = path; /* beginning of path */
	pos2 = pos1 + 1; /* right after first / */
	end = strchr(path, '\0'); /* end of path */

	do {
		int tmplen;
		/* end decl */
		tmplen = pos2 - pos1;
		tmp = strndup(pos1, pos2 - pos1);
#ifdef DEBUG
		printf("path_calc_target:%d path: \"%s\", tmp: \"%s\", pos1: \"%s\", pos2: \"%s\", tmplen: %d\n",
			__LINE__, path, tmp, pos1, pos2, tmplen);
#endif
		if (0 == strcmp(tmp, "..")) { /* going back one */
#ifdef DEBUG
			printf("going back (..)\n");
#endif
			if (list_size(list) == 0) /* if we rewind past the beginning *f the path, bail! */
				fatal_invalid_path(__FILE__, __LINE__, path, 0);
			list_remove(list, list_last(list), NULL); /* remove the last entry to the list */
		} else if (0 != strcmp(tmp, ".")) {
			/* if we're not talking about current dir ".", then add the entry */
#ifdef DEBUG
	printf("path_calc_target:%d ", __LINE__);
	str_examine(tmp);
#endif
			len += tmplen + 1; /* remember total length of path */
			node = list_node_create(tmp, tmplen + 1);
			list_append(list, node);
#ifdef DEBUG
		} else {
			printf("same dir (.)\n");
#endif
		}

		/* special case for first iteration */
		pos1 = pos2 + ((pos1 > path) ? 1 : 0);
		
		xfree(tmp);

		if (NULL == (pos1 = strnchr(pos1, PATHSEP))) /* find the first non-sep char */
			break;
		if (NULL == (pos2 = strchr(pos1, PATHSEP))) /* find up until the next sep char */
			pos2 = end;
	 } while (pos1 < end);

	xfree(path);

#ifdef DEBUG
	printf("end path_calc_target:%d ---------------\n", __LINE__);
	list_dump(list, list_dump_cb_chars);
#endif

	return list;

}

/* get cwd, split into list */
/* most of the path logic is here */
/* FIXME: this function is too long, needs to be broken up */
static list_head *path_split(list_head **rawpath, int follow_symlinks)
{
	list_head *paths = NULL, *links = NULL;
	list_node *loopnode = NULL, *node = NULL;
	path_t *path = NULL, *prevpath = NULL;
	char bail = 0; /* loop bail flag */
	int symcnt = 0; /* symlink depth counter */
	short c = 0; /* loop counter */

#ifdef DEBUG
	assert(NULL != rawpath);
	printf("path_split:%d ---------------\n", __LINE__);
	list_dump(*rawpath, list_dump_cb_chars);
#endif

	/* allocate paths */
	if (NULL == (paths = list_head_create()))
		err_bail(__FILE__, __LINE__, "couldn't create list");

	/* allocate links */
	if (NULL == (links = list_head_create()))
		err_bail(__FILE__, __LINE__, "couldn't create list");

	/* allocate path_t, we keep this the whole way through */
	path = path_alloc();

	/* for each part of the path */
	for (
		loopnode = list_first(*rawpath);
		bail == 0 && loopnode != NULL;
		c++
	) {
#if 0
#if 0
		printf("path_split%d LOOP loopnode:%p, prevpath:%p\n",
			__LINE__, (void *)loopnode, (void *)prevpath);
		list_node_dump(loopnode, list_dump_cb_chars);
#endif
#endif


#ifdef DEBUG
		printf("%%%%%%%%%%%%%%%% path_alloc(%p):%d\n", path, __LINE__);
#endif

		path_init(path);

#ifdef DEBUG
		printf("path_split:%d: ", __LINE__);
		path_dump(path);
#endif
		/* construct abspath to this point */
		if (NULL != prevpath) {
#ifdef DEBUG
#if 0
			printf("path_split:%d prevpath->abspath: \"%s\"\n",
				__LINE__, prevpath->abspath);
			path_dump(prevpath);
#endif
#endif
			path->dir = strdup(prevpath->abspath);
			path->abspath = strdup(prevpath->abspath);
#ifdef DEBUG
			printf("path_split:%d c:%d, ", __LINE__, c);
			str_examine(prevpath->abspath);
#endif
		}
		if (c >= 2) {
			/* only throw PATHSEP in after second... ("/", "path", *HERE* "path2"... ) */
#ifdef DEBUG
			printf("path_split:%d path: ", __LINE__);
			path_dump(path);
#endif
			path->abspath = strcapp(path->abspath, PATHSEP);
		}
#ifdef DEBUG
		printf("path_split:%d c:%d, ", __LINE__, c);
		str_examine(path->abspath);
#endif
		path->abspath = strapp(path->abspath, loopnode->data); /* append current path */
#if 0
#if 0
		printf("%s:%d:prevpath(%p), path->abspath:%s, loopnode->data(%p) \"%s\"\n",
			__FILE__, __LINE__, (void *)prevpath, path->abspath,
			(void *)loopnode->data, (char *)loopnode->data);
		path_dump(path);
#endif
#endif
		/* add component */
		if (NULL == (path->component = strdup(loopnode->data)))
			err_bail(__FILE__, __LINE__, "could not dupe loopnode->data");

		{ /* new block */
			struct stat st;
			char is_lnk;
			/* end decl */
			is_lnk = 0;
			errno = 0; /* reset errno */
			/* get file stats */
#ifdef DEBUG
#if 0
			printf("path_split:%d\n", __LINE__);
			path_dump(path);
#endif
#endif
			if (-1 == lstat(path->abspath, &st)) { /* error reading file */ 
				int save_err = errno;
#ifdef DEBUG
				str_examine(path->abspath);
				fprintf(stderr, "%s\n", strerror(save_err));
#endif
				/* FIXME: should i bail immediately? */
				fatal_invalid_path(__FILE__, __LINE__, path->abspath, save_err);
				break;
			}
			/* file exists and is accessible */
			/* is abspath a symlink? */
			if (FOLLOW == follow_symlinks && S_ISLNK(st.st_mode)) {
				/* figure out where the symlink points */
				if (NULL == (path->symlink = readlink_malloc(path->abspath)))
					err_bail(__FILE__, __LINE__, "could not resolve symlink");
				/* if symlinks too deep, make a note (we'll report later) and bail */
				if (++symcnt > MAXSYMLINKS) {
					list_nodes_free(paths, path_free); /* nuke where we are */
					/* mark last symlink we saw as an error */
					path->status = STATUS_SYMLINKS_TOO_DEEP;
					bail = 1; /* need to add node and leave loop */
				}
				if (0 == bail) { /* follow symlink, reset everything */
#ifdef DEBUG
#if 1
					printf("path_split:%d following symlink from \"%s\" to \"%s\", resetting...\n",
						__LINE__, path->abspath, path->symlink);
					list_dump(paths, path_dump);
#endif
#endif
					list_nodes_free(paths, path_free); /* clear current */
#ifdef DEBUG
#if 0
					printf("path_split%d: *rawpath: ", __LINE__);
					list_dump(*rawpath, list_dump_cb_chars);
					printf("path_split%d: path (check dir): ", __LINE__);
					path_dump(path);
#endif
#endif
					list_free(*rawpath, NULL); /* destroy rawpath */
					*rawpath = path_calc_target(path->symlink, path->dir); /* recalc path from symlink */
					loopnode = list_first(*rawpath); /* reset loop */
#ifdef DEBUG
					printf("path_split:%d everything reset, continuing...\n", __LINE__);
					list_dump(*rawpath, list_dump_cb_chars);
#endif
					prevpath = NULL;
					c = -1; /* gets incremented next loop back to zero */
					is_lnk = 1;
#ifdef DEBUG
					printf("path_split:%d path after ln: ", __LINE__);
					path_dump(path);
#endif
				}
			} else { /* dir or file */
				/* copy data about the file to our own structure */
				path->mode = st.st_mode;
				path->uid = st.st_uid;
				path->gid = st.st_gid;
				/* resolve mntpt, if any */
#ifdef DEBUG
#if 0
				printf("&&&&&&&&&& path_split:%d mnts(%p), path->abspath(%p): \"%s\"\n",
					__LINE__, (void *)mnts, (void *)path->abspath, path->abspath);
#endif
#endif
				/* if we're not a mntpt... */
				if (NULL == (path->mntpt = mnt_mntdir_find(MNTPTS, path->abspath))) {
#ifdef DEBUG
#if 0
					if (NULL == prevpath)
						err_bail(__FILE__, __LINE__,
							"either this is / and unmounted or prevpath is pointing to the wrong place");
#endif
#endif
					/* point to prev mntpt, which is the mntpt we need to respect */
					if (NULL != prevpath)
						path->mntpt = prevpath->mntpt;
				}
			}
#ifdef DEBUG
#if 0
			printf("******** path_split%d: path:", __LINE__);
			path_dump(path);
			printf("path_split:%d is_lnk: %d\n", __LINE__, (int)is_lnk);
#endif
#endif
			/* copy path_entry into node, append to paths */

			node = list_node_create_deep(path, path_dupe);
			/* if symlink, append to links, we need to hold our symlinks separately because
			paths gets completely reset every time, but we still want to report symlinks */
			if (NULL == (node = list_append((is_lnk ? links : paths), node)))
				err_bail(__FILE__, __LINE__, "could not append path_entry to paths!");

			/* point to node as long as it's related to current path */
			if (!is_lnk) {
				prevpath = node->data;
				loopnode = list_node_next(loopnode);
			}

#ifdef DEBUG
		printf("path_split:%d c: %d, *rawpath: ", __LINE__, c);
		list_dump(*rawpath, list_dump_cb_chars);
#if 0
			printf("^^^^^^^^^^ after list_append: ");
			printf("links: ");
			list_dump(links, path_dump);
			printf("paths: ");
			list_dump(paths, path_dump);
			printf("^^^^^^^^^^^^^^^^\n");
#endif
#endif


		}
	}

#ifdef DEBUG
	printf("path_split loop over! why? bail:%d, loopnode:%p\n",
		(int)bail, (void *)loopnode);
#endif

	path_free(path);

#ifdef DEBUG
#if 0
	printf("######### path_split:%d pre-concat links and paths: ", __LINE__);
	list_dump(links, path_dump);
	list_dump(paths, path_dump);
#endif
#endif

	/* append paths onto links */
	list_concat(links, paths);

#ifdef DEBUG
#if 1
	printf("######### path_split:%d post-concat links and paths: ", __LINE__);
	list_dump(links, path_dump);
#if 0
	 list_dump(paths, path_dump);
#endif
#endif
#endif

	list_head_free(paths); /* no longer need second list_head */

#ifdef DEBUG
#if 0
	printf("end path_split:%d c: %d, *rawpath: ", __LINE__, c);
	list_dump(*rawpath, list_dump_cb_chars);
#endif
#endif

	return links;

}


/* main reporting function, once we've goat all necessary data */
/* output: 0: silent, 1: normal, 2: only report errors */
/* */
static int report_gen(list_head *paths, user_t *user, permdsc_t *permreq, int output)
{
	perm_t reasmask = REAS_NONE; /* permanent mask, carries sticky mask */
	perm_t permeff = PERM_NONE; /* effective local copy of permreq, because it may change */
	list_node *node;
	reason_t *reas;
	path_t *path;
	int able = 1, last_entry;

#ifdef DEBUG
	assert(NULL != paths);
	assert(NULL != user);
	assert(NULL != permreq);
#endif

#ifdef DEBUG
	printf("report_gen:%d paths(%p), user(%p), permreq(%p), output:%d ",
		__LINE__, (void *)paths, (void *)user, (void *)permreq, output);
	list_dump(paths, path_dump);
#endif

	reas = reason_alloc();

	permeff = permreq->mask;

	/* translate what CREA and DELE really mean */
	if (permeff & PERM_CREA) {
		permeff |= PERM_WRIT; /* ensure WRIT on */
		permeff ^= PERM_CREA; /* turn CREA off */
	} else if (permeff & PERM_DELE) {
		permeff |= PERM_WRIT;
	}

#ifdef DEBUG
		printf("report_gen:%d ", __LINE__);
		list_dump(paths, path_dump);
#endif

	for (node = list_first(paths); node != NULL; node = list_node_next(node)) {
		last_entry = (node == list_last(paths));
		path = node->data;

#if 0
		if (last_entry)
			if (!path_is_dir(path) && (permeff | PERM_DELE))
				permeff ^= PERM_DELE; /* DELE only matters for dir */
#endif

		if (path_is_sticky(path))
			reasmask |= REAS_NO_STICKY;

#ifdef DEBUG
		printf("report_gen:%d ", __LINE__);
		path_dump(path);
#endif

		report_calc(reas, path, user, reasmask, permreq, &permeff, last_entry);

#ifdef DEBUG
		printf("report_gen:%d path->abspath:\"%s\", reas->no:%d\n",
			__LINE__, path->abspath, reas->no);

		reason_dump(reas);
#endif

		if (1 == able && PERM_NONE != reas->no) /* user unable */
			able = 0;

		/* actually print report if output all or err and output err */
		if (flag_verbose >= 1 && OUTPUT_ALL == output) {
			/* normal stuff */
			report(reas, user);
		} else if (flag_verbose >= 3 && (OUTPUT_ERR == output && 0 == able)) {
			/* print each file that failed a test if we're on verbosity level 3 */
			report(reas, user);
		}

		/* we're in recursive mode and shouldn't go any farther */
		if (OUTPUT_ERR == output && 0 == able)
			break;
	}

#if 0
	printf("report_gen:%d path->abspath:\"%s\", able:%d\n",
		__LINE__, path->abspath, able);
#endif

	/* final line of output */
	if (OUTPUT_ALL == output) {
		path_t *path = ((path_t *)(list_last(paths)->data));
		printf("%s user %s %s perms %s on file %s\n",
			(able ? "OK" : "!!"), /* lead */
			user->name,
			(able ? "has" : "doesn't have"),
			permreq->dsc,
			path->abspath
		);
	}

	/* empty */
	reason_free(reas);

#if 0
	printf("report_gen:%d returning able == %d...\n", __LINE__, able);
#endif
	return able;

}

/* figure out if we have abilities on this path */
/* reas: is empty, holds return value */
/* path: contains all info about path we're checking */
/* user: user we're checking on */
/* reasmask: holds sticky mask, if present */
/* permreq: passed in case we need to do recursive delete checking */
/* permeff: perms we're checking on */
/* last_entry: 1 if last item in list */
static void report_calc(reason_t *reas, path_t *path, user_t *user, perm_t reasmask, permdsc_t *permreq, perm_t *permeff, int last_entry)
{

#if 0
	printf("%%%%%%%%%%%%%%%% report_calc:%d launched for \"%s\"\n", __LINE__, path->abspath);
#endif

#ifdef DEBUG
	assert(NULL != reas);
	assert(NULL != path);
	assert(NULL != user);
	assert(NULL != permeff);
#endif

	reason_init(reas);

#ifdef DEBUG
	printf("report_calc:%d(%p, %p) path_dump(%p)\n",
		__LINE__, (void *)path, (void *)user, (void *)path);
	reason_dump(reas);
#endif
	reas->path = path;

#ifdef DEBUG
	printf("report_calc:%d path->mode: %d\n", __LINE__, path->mode);
#endif

	if (path_status_not_ok(path)) {
		reas->label = RPT_NOT_OK;
	} else if (path_is_symlink(path)) {
		reas->label = RPT_SYMLNK; /* set label for output, do nothing else */
	} else {

		char mntpt_bail; /* mntpt perms make dir checking unnecessary */
		/* end decl */
		
		mntpt_bail = 0;
		/* for every file we assume that we don't have the permissions that we want... */
		/* if we are able to disprove these pessimistic assumptions, user CAN access file */
		reas->no = *permeff;

#if DEBUG && 0
		printf("report_calc:%d path->mode: %d\n", __LINE__, path->mode);
#endif

		/* adjust required perms based on where in the path we currently are */
		if (!last_entry) /* not final file */
			reas->no = REAS_NO_EXEC; /* in *MOST* cases we only need exec to get to the next dir */

		/* check mounts */
		if (path_is_mntpt(path)) {
			if ((reas->no & PERM_WRIT) && mntpt_is_readonly(path->mntpt)) {
				/* write required and mounted ro */
				reas->no |= REAS_NO_MNTPT_RO;
				mntpt_bail = 1; /* mntpt restrictions trump fs perms */
			}
		}

#ifdef DEBUG
#if 0
		printf("report_calc:%d reas->yes:%d, reas->no:%d, mntpt_bail:%d\n",
			__LINE__, reas->yes, reas->no, mntpt_bail);
#endif
#endif

		if (!mntpt_bail) {

			/* special case for deleting directory */
			if (last_entry) {
				if (path_is_dir(path)) {
					if (reas->no & PERM_DELE) {
						reas->no |= (REAS_NO_READ | REAS_NO_WRIT | REAS_NO_EXEC);
					}
				}
			}

			/* test read */
			if (reas->no & REAS_NO_READ) {
				if (((path->mode & S_IRUSR) && path->uid == user->uid) || UID_ROOT == user->uid) {
				/* readable by owner and user is owner */
					reas->no ^= REAS_NO_READ;
					reas->yes |= REAS_YES_UR;
				} else if ((path->mode & S_IRGRP) && user_in_group(user, path->gid)) {
				/* readable by group */
					reas->no ^= REAS_NO_READ;
					reas->yes |= REAS_YES_GR;
				} else if (0 != (path->mode & S_IROTH)) { /* readable by other */
					reas->no ^= REAS_NO_READ;
					reas->yes |= REAS_YES_OR;
#ifdef DEBUG
				} else { /* file is unreadable and read is required */
					
#endif
				}
			}

#if DEBUG && 0
			printf("report_calc:%d path->mode: %d\n", __LINE__, path->mode);
#endif

			/* test write */
			if (reas->no & REAS_NO_WRIT) {
				if (((path->mode & S_IWUSR) && path->uid == user->uid) || UID_ROOT == user->uid) {
				/* writeable by owner and user is owner */
					reas->no ^= REAS_NO_WRIT;
					reas->yes |= REAS_YES_UW;

				} else if ((path->mode & S_IWGRP) && user_in_group(user, path->gid)) {
				/* writeable by group */
					reas->no ^= REAS_NO_WRIT;
					reas->yes |= REAS_YES_GW;
				} else if (0 != (path->mode & S_IWOTH)) { /* writeable by other */
					reas->no ^= REAS_NO_WRIT;
					reas->yes |= REAS_YES_OW;
#ifdef DEBUG
				} else { /* file is unwriteable and write is required */
					
#endif
				}
			}

#if DEBUG && 0
			printf("report_calc:%d path->mode: %d\n", __LINE__, path->mode);
#endif

			/* test exec */
			if (reas->no & REAS_NO_EXEC) {
#if DEBUG && 0
				printf("report_calc:%d reas->yes:%d, reas->no:%d\n", __LINE__, reas->yes, reas->no);
#endif

				if (
					/* special root case... "x" must be turned on somewhere or even root cannot x it */
					(UID_ROOT == user->uid && (path->mode & (S_IXUSR | S_IXGRP | S_IXOTH))) ||
					((path->mode & S_IXUSR) && path->uid == user->uid) /* regular case */
				){ /* writeable by owner and user is owner */
#ifdef DEBUG
#if 0
					printf("report_calc:%d path->uid:%d, user->uid:%d\n",
						__LINE__, path->uid, user->uid);
#endif
#endif
					reas->no ^= REAS_NO_EXEC;
					reas->yes |= REAS_YES_UX;
				} else if ((path->mode & S_IXGRP) && user_in_group(user, path->gid)) {
				/* execable by group */
					reas->no ^= REAS_NO_EXEC;
					reas->yes |= REAS_YES_GX;
				} else if ((path->mode & S_IXOTH)) { /* execable by other */
					reas->no ^= REAS_NO_EXEC;
					reas->yes |= REAS_YES_OX;
#ifdef DEBUG
					printf("report_calc:%d reas->yes: %d\n", __LINE__, reas->yes);
				} else { /* file is unexecable and exec is required */
					fprintf(stderr, "can't exec!!!! (\"%s\"=%d)\n", path->abspath, path->mode);
#endif
				}
			}
		} /* if mntpt_bail */

		if (last_entry) { /* final dir/file */
			/* do a final check if we've got what it takes to create/delete this file */
			if (!path_is_dir(path)) {
				if (reas->no & PERM_CREA) { /* can user create a file that already exists... */
					/* FIXME: what do we do here? */
				} else if (reas->no & PERM_DELE) { /* can we delete existing file? */
#ifdef DEBUG
					printf("%d reasmask: %d, path->uid: %d, user->uid: %d\n",
						__LINE__, reasmask, path->uid, user->uid);
#endif

					/* get rid of REAS_NO_DELE because it's a meta-reason, not a real one */
					reas->no ^= REAS_NO_DELE;

					/* if is owner or root... */
					if (path->uid == user->uid || UID_ROOT == user->uid) {
						/* may delete normal file even with all perms off */
						reas->no = REAS_NONE;
						if (UID_ROOT == user->uid) {
							reas->yes |= REAS_YES_ROOT;
						} else {
							reas->yes |= REAS_YES_OWNER;
						}
					} else { /* non-owner */
						if (reasmask & REAS_NO_STICKY) { /* file exists inside a sticky dir */
							reas->no |= REAS_NO_STICKY;
						}
					}

				}
			} else { /* referring to a directory */
				if (reas->no & PERM_CREA) { /* can user create a new file inside this directory? */
					if (reasmask & REAS_NO_STICKY) { /* sticky encountered */
						if (path->uid != user->uid) { /* not the owner of current dir! */
							reas->no |= REAS_NO_STICKY;
						}
					}
				} else if (reas->no & PERM_DELE) { /* can we delete existing dir? */
/*
	this gets tricky. in order to delete a directory, we need the ability to delete
	every single file and directory recursively underneath it. we need to write code
	that will search a directory tree for any file that we *CAN'T* delete, respecting
	sticky bits, uid, gid, etc. if we don't find anything we can't delete, then the
	user would be able to delete this directory.

	recursively searching a directory could take a lot of machine time and machine resources...

	the idea is to recurse bread-first, showing only error for files we can't delete. assuming
	no errors in current level, recurse into dirs and do same

	FIXME: this whole block is its own function, but is so tightly tied it wouldn't make sense
	to rip it out. redesign would be good.
*/
					if (REAS_NONE == (reas->no & (REAS_NO_READ | REAS_NO_WRIT | REAS_NO_EXEC))) {
						reas->no ^= PERM_DELE;
					}

					/* root can delete everything */
					if (UID_ROOT == user->uid) {
						reas->no = REAS_NONE;
						return; /* no need to check further */
					}

					if (REAS_NONE == reas->no) { /* if things are successful for this dir... */

						DIR *dir;
						list_head *dir_list;
						list_node *dir_node;
						struct dirent *ent;
						perm_t dir_perms;
						int able = 1;

						if (NULL == (dir = opendir(path->abspath))) {
#if 0
							err_opendir(__FILE__, __LINE__, path->abspath);
#endif
							reas->no |= REAS_NO_CERTAIN;
							return;
						}

						dir_list = list_head_create();
						dir_perms = REAS_NO_DEPENDANCY; /* assume there are undeletable child files */
						
						while (NULL != (ent = readdir(dir))) {
							struct stat st;
							list_head *target, *paths;
							char *abspath;
							/* end decl */
#if 0
							printf("report_calc:%d ent->d_name:\"%s\"\n",
								__LINE__, ent->d_name);
#endif
							if (0 == strcmp(ent->d_name, ".") || 0 == strcmp(ent->d_name, ".."))
								continue; /* skip "special" entries */

							/* FIXME: this is inefficient... */
							abspath = strdup(path->abspath);
							abspath = strcapp(abspath, PATHSEP);
							abspath = strapp(abspath, ent->d_name);
#if 0
							printf("report_calc:%d path->abspath:\"%s\", abspath:\"%s\"\n",
								__LINE__, path->abspath, abspath);
							str_examine(abspath);
#endif
							if (-1 == lstat(abspath, &st)) /* stat file (don't follow symlinks) */
								err_cantopen(__FILE__, __LINE__, abspath); /* or die trying */
							target = path_calc_target(abspath, NULL); /* cwd == NULL */
							paths = path_split(&target, DONT_FOLLOW);

/*
NOTE: next block contains the behavior for the recursion...
*/

							if (!S_ISDIR(st.st_mode)) { /* if no problems so far, save dirs, otherwise we'll never go into them */
								if (0 == report_gen(paths, user, permreq, OUTPUT_ERR)) { /* run for every entry in current dir */
									able = 0;
									reas->no |= REAS_NO_DEPENDANCY;
								}
								list_free(paths, path_free);
							} else {
								/* save in a list for later processing */
								dir_node = list_node_create(paths, sizeof(list_head)); /* create list node */
								if (NULL == list_append(dir_list, dir_node)) /* append or die trying */
									err_bail(__FILE__, __LINE__, "could not append dir_node to dir_list");
								list_head_free(paths);
							}
							list_free(target, NULL); /* always free target */
							free(abspath);
						} /* readdir loop */

						closedir(dir);

						if (1 == able) { /* if no problems at current level, recurse down */
#if 0
							printf("report_calc:%d able==1 for \"%s\"\n", __LINE__, path->abspath);
#endif
							/* if we found any subdirs, search them too */
							for (dir_node = list_first(dir_list); dir_node != NULL; dir_node = list_node_next(dir_node)) {

								/* dir_node represents a node with a list_head for another list as its data */
#if 0
								printf("report_calc:%d\n", __LINE__);
								list_dump(list_node_data(dir_node), list_dump_cb_chars);
#endif
								if (0 == report_gen(list_node_data(dir_node), user, permreq, OUTPUT_ERR)) {
									reas->no |= REAS_NO_DEPENDANCY;
									break;
								}
							}
						}

						list_free(dir_list, shac_list_list_free); /* free list */
					}
				}
			}
		} /* if last_entry */
	} /* if symlink */
	/* report on what we've found */

#ifdef DEBUG
#if 0
	printf("report_calc:%d ", __LINE__);
	reason_dump(&reas);
#endif
#endif

}


/* actually produce output to the screen for a single */
static void report(reason_t *reas, user_t *user)
{
	path_t *path;
	mntpt_t *mntpt;
	char comma = 0; /* flag as to whether to print a comma or not */
#ifdef DEBUG
	assert(NULL != reas);
	assert(NULL != user);
#endif
	path = reas->path;
	mntpt = path->mntpt;

#ifdef DEBUG
	printf("report:%d reas: ", __LINE__);
	reason_dump(reas);
#endif

	/* this should be sane, we depend on this... FIXME: does this check work? */
	assert(NULL != RPT_LABELS[reas->label]);

#ifdef DEBUG
	printf("report:%d reas->yes: %d\n", __LINE__, reas->yes);
#endif

	if (path_is_symlink(path)) { /* symlink output */
		printf("%s %s -> %s", RPT_LABELS[reas->label],
			path->abspath, path->symlink);
		if (STATUS_OK != path->status) { /* report status */
			printf(" %s", RPT_STATUS[log2(path->status)]);
		}
	} else { /* non-symlink */
		reas->label = (reas->no ? RPT_NOT_OK : RPT_OK);
		printf("%s %s", RPT_LABELS[reas->label], path->abspath); /* output label and filename */
		/* status means there was a fundamental error with the file */
		/* we just print out the status, not extra info */
		if (STATUS_OK != path->status) { /* report status */
			printf(" %s", RPT_STATUS[log2(path->status)]);
		} else {
			/* print things we CAN do, even if ultimately we're unsuccessful */
			printf(" (");
			if (REAS_NONE != reas->yes) {
				/* print "user" perms that helped us */
				if (REAS_NONE != (reas->yes & (REAS_YES_UR | REAS_YES_UW | REAS_YES_UX))) {
					printf("%s+%s%s%s",
						STR_USER,
						(reas->yes & REAS_YES_UR ? STR_READ : ""),
						(reas->yes & REAS_YES_UW ? STR_WRIT : ""),
						(reas->yes & REAS_YES_UX ? STR_EXEC : "")
					);
					comma = 1;
				}
				/* print "group" perms that helped us */
				if (REAS_NONE != (reas->yes & (REAS_YES_GR | REAS_YES_GW | REAS_YES_GX))) {
					printf("%s%s+%s%s%s",
						(1 == comma ? "," : ""),
						STR_GROUP,
						(reas->yes & REAS_YES_GR ? STR_READ : ""),
						(reas->yes & REAS_YES_GW ? STR_WRIT : ""),
						(reas->yes & REAS_YES_GX ? STR_EXEC : "")
						);
					comma = 1;
				}
				/* print "other" perms that helped us */
				if (REAS_NONE != (reas->yes & (REAS_YES_OR | REAS_YES_OW | REAS_YES_OX))) {
					printf("%s%s+%s%s%s",
						(1 == comma ? "," : ""),
						STR_OTH,
						(reas->yes & REAS_YES_OR ? STR_READ : ""),
						(reas->yes & REAS_YES_OW ? STR_WRIT : ""),
						(reas->yes & REAS_YES_OX ? STR_EXEC : "")
					);
				}
				if ((REAS_YES_OWNER & reas->yes)) {
					printf("%s%s",
						(1 == comma ? "," : ""),
						STR_OWNER
					);
				} else if ((REAS_YES_ROOT & reas->yes)) { /* can't have both */
					printf("%s%s",
						(1 == comma ? "," : ""),
						STR_ROOT
					);
				}
			} /* if reas->yes */
			printf(")");
			/* print mntpt data if path is mntpt */
			if (path_is_mntpt(path)) {
				printf(" (mnt %s)", mntpt->mntdev);
			}

			/* if group perms helped, list which group */
			if (REAS_NONE != (reas->yes & (REAS_YES_GR | REAS_YES_GW | REAS_YES_GX))) {
				list_node *node;
				group_t *g;
				char *s = NULL;
				for (node = list_first(user->groups); node != NULL; node = list_node_next(node)) {
					g = list_node_data(node);
					if (g->gid == path->gid) {
						s = g->name;
						break;
					}
				}
				printf(" (group %s)", (NULL == s ? "?" : s));
			}

			if (REAS_NONE != reas->no) { /* print reasons why we didn't succeed */
				int unsigned i;
				perm_t c;
				/* iterate through all possible error messages and print any that match */
				for (c = 1, comma = 0, i = 1; c < 32; c++, i <<= 1) {
					if (0 != (reas->no & i)) {
						printf("%s%s",
							(1 == comma ? ", " : " "),
							RPT_REASONS[c]
						);
						comma = 1;
					}
				} /* for */
			} /* if reas->no */
		} /* status bad */
	}
#ifdef DEBUG
	printf(" (yes:%d, no:%d)", reas->yes, reas->no);
#endif
	printf("\n");
}

static void perm_calc(const char *username, const char *path, permdsc_t *perms)
{
	list_head *target = NULL, *paths = NULL;
	user_t *user = NULL;
	char *cwd = NULL;
	int able = 0;

#ifdef DEBUG
	assert(NULL != username);
	assert(NULL != path);
	assert(NULL != perms);
#endif

/* FIXME: path is completely valid, but for some reason this breaks the variable-argument macro VERBOSE() */
/* for now we just turn it off, grrrr */
#if 0
	printf("path(%p): \"%s\"\n", (void *)path, path);
	VERBOSE(2, ADD_APPEND, "VB checking file '%s'...\n", path);
#endif

	cwd = xmalloc(PATH_MAX);
	cwd = getcwd(cwd, PATH_MAX);

	/* split up our target, if path is invalid, program dies here */
	target = path_calc_target(path, cwd);

	user = user_load(username);

#ifdef DEBUG
	user_dump(user);
#endif

	/* load mntpt data into global var */
	MNTPTS = mnt_load();

#ifdef DEBUG
	list_dump(MNTPTS, mntpt_dump);
#endif

	/* read all path information */
	/* FIXME: target is getting corrupted somehow... looks ok in the function, */
	/* but what i get back is junk */
	paths = path_split(&target, FOLLOW);
#ifdef DEBUG
	printf("perm_calc:%d target: ", __LINE__);
	list_dump(target, list_dump_cb_chars);
#endif

#ifdef DEBUG
	printf("perm_calc:%d ", __LINE__);
	list_dump(paths, path_dump);
	exit(EXIT_SUCCESS);
#endif

	/* generate a report, figure out if we actually have perms */
	able = report_gen(paths, user, perms, OUTPUT_ALL);

	list_free(paths, path_free);
	list_free(target, NULL);

	list_free(MNTPTS, mntpt_free);
	user_free(user);
	xfree(cwd);

}

static void verbose_add(const char *msg, int whichend)
{
	list_node *node;
#ifdef DEBUG
	assert(NULL != msg);
#endif
	if (NULL == (node = list_node_create(msg, strlen(msg) + 1)))
		err_bail(__FILE__, __LINE__, "could not allocate node for verbose message");
	if (ADD_PREPEND == whichend) {
		if (NULL == (node = list_prepend(VERBOSE_MSG, node)))
			err_bail(__FILE__, __LINE__, "could not add prepend message");
	} else {
		if (NULL == (node = list_append(VERBOSE_MSG, node)))
			err_bail(__FILE__, __LINE__, "could not add prepend message");
	}
}

/* print all messages in verbose list */
static void verbose_flush(void)
{
	list_node *node;
	for (node = list_first(VERBOSE_MSG); node != NULL; node = list_node_next(node))
		fprintf(stdout, list_node_data(node));
	list_nodes_free(VERBOSE_MSG, NULL); /* clear entries */
}

static void init_globals(void)
{
	if (NULL == VERBOSE_MSG)
		if (NULL == (VERBOSE_MSG = list_head_create()))
			err_bail(__FILE__, __LINE__, "could not create VERBOSE_MSG");
}

static void cleanup_globals(void)
{
	list_free(VERBOSE_MSG, NULL); /* destroy list */
}

/* parses options, launches */
int main(int argc, char **argv)
{

	char *username = NULL, *rawperms = NULL;
	permdsc_t *perms = NULL;
	int opt;

#ifdef DEBUG
	test_stuff();
#endif

	init_globals();

	/* set defaults */
	username = rawperms = NULL;

	/* allocate perm structure */
	perms = permdsc_alloc();

	/* parse options */
	opterr = 0;

	while ((opt = getopt(argc, argv, "u:p:vh")) != -1) {
#ifdef DEBUG
		printf("main:%d optind: %d, opterr: %d, opt: \'%c\', optarg: \"%s\"\n",
			__LINE__, optind, opterr, opt, optarg);
#endif
		switch (opt) {
		case 'u': /* user */
			if (NULL == optarg)
				fatal("you must specify a username");
			if (NULL != username) /* username already set */
				fatal("you may only specify one username");
			if (strisnum(optarg)) { /* passed a uid */
				username = username_from_uid(optarg);
				VERBOSE(2, ADD_APPEND, "VB username '%s' from uid '%s'...\n", username, optarg);
			} else {
				username = strdup(optarg);
				VERBOSE(2, ADD_APPEND, "VB username '%s'...\n", username);
			}
			break;
		case 'p': /* perms */
			/* allow multiple perm args */
			rawperms = strapp(rawperms, optarg);
			break;
		case 'v': /* verbose */
			if (flag_verbose < VERBOSE_MAX)
				++flag_verbose;
			break;
		case 'h': /* help */
		case '?':
			printf(HELP);
			/* cleanup */
#if 0
			xfree(username);
			xfree(rawperms);
			permdsc_free(perms);
			cleanup_globals();
#endif
			/* bail */
			exit(EXIT_SUCCESS);
			break;
		default:
#ifdef DEBUG
			printf("main:%d opterr: %d, option with no arg: \"%s\"\n",
				__LINE__, opterr, optarg);
#endif
			break;
		}
	}

	/* report on our verbosity level, if we are > 0 */
	switch (flag_verbose) {
	case 0:
		/* nothing */
		break;
	case 1:
		VERBOSE(1, ADD_PREPEND, "VB verbose mode...\n");
		break;
	case 2:
		VERBOSE(2, ADD_PREPEND, "VB very verbose mode...\n");
		break;
	case 3:
		VERBOSE(3, ADD_PREPEND, "VB very very verbose mode...\n");
		break;
	default:
		fprintf(stderr, "flag_verbose: %d\n", flag_verbose);
		err_bail(__FILE__, __LINE__, "PE invalid verbosity level");
		break;
	}

#if 0
	printf("main:%d flag_verbose: %d\n", __LINE__, flag_verbose);
#endif

	/* getopt reorders argv and sets optind to the first args pass that it didn't process */

	if (optind == argc) { /* no files left */
		printf(USAGE);
		exit(EXIT_FAILURE);
	}

	/* fill in default args if they weren't set */
	/* no user, use default */
	if (NULL == username) {
		username = user_get_current();
		VERBOSE(2, ADD_APPEND, "VB using default user '%s'...\n", username);
	}
		
	/* no perms, use default */
	if (NULL == rawperms) {
		perm_t mask = decode_perms(DEFAULT_PERMS); /* get canonical perms */
		permdsc_set_mask(perms, mask);
		VERBOSE(2, ADD_APPEND, "VB using default perms '%s'...\n", perms->dsc);
	} else { /* set up perms */
		perm_t mask = decode_perms(rawperms); /* get canonical perms */
		permdsc_set_mask(perms, mask);
		VERBOSE(2, ADD_APPEND, "VB using perms '%s'...\n", perms->dsc);
	}

	/* all possible verbose messages have been processed, now print them all, and nuke them */
	verbose_flush();

	{ /* new block */
		char **tmp = NULL;
#ifdef DEBUG
		printf("main():%d optind: %d, username: \"%s\", rawperms: \"%s\"\n",
			__LINE__, optind, username, rawperms);
		for (tmp = argv; *tmp != NULL; tmp++)
			printf("main:%d argv[%p]: \"%s\"\n", __LINE__, (void *)tmp, *tmp);
#endif

		/* calc perms on all paths sent to us */
		/* FIXME: the loop should be inside perm_calc so we can load user, mount info once */
		for (tmp = argv + optind; *tmp != NULL; tmp++)
			perm_calc(username, *tmp, perms);

	}

	/* clean up */
	xfree(rawperms);
	xfree(username);
	permdsc_free(perms);

	cleanup_globals();

	return EXIT_SUCCESS;
}

/*************** test functions are relegated to the basement ***************/
/* FIXME: these are outdated and used to test pieces as they were built */
/* they either need to get updated or nuked */

static void test_stuff(void)
{
#if 0
	test_path_calc_target();
	test_path_split();
	test_mnt_load();
	test_user_load();
	exit(EXIT_SUCCESS);
#endif
}

static void test_path_calc_target()
{
	list_head *list;
	const char **curr;
	int i;
	char cwd[PATH_MAX];
	const char *paths[] = {
		"///a//b/c/"
		,"d/e/f"
		,"../.."
		,"1/2/3/../../.."
		,"1/2/3/../../../"
		,"./."
		,""
		,NULL
	};

	assert(NULL != (list = list_head_create()));

	printf("testing path_calc_target...\n");

	(void)getcwd(cwd, sizeof cwd);

	for (i = 0, curr = paths; NULL != curr; i++, curr++) {
		printf("iteration %d: \"%s\":\n", i, *curr);
		list = path_calc_target(*curr, cwd);
		list_dump(list, list_dump_cb_chars);
		printf("\n===============\n");
		list_nodes_free(list, NULL);
	}

	printf("\ndone.\n");
}


static void test_path_split(void)
{
#if 0
	list_head *calc = NULL, *split = NULL;

	assert(NULL != (calc = list_head_create()));
	assert(NULL != (split = list_head_create()));

	calc = path_calc_target("");
	printf("calc:\n");
#if 0
	list_dump(calc, list_dump_cb_chars);
#endif
	
	printf("splitting...\n");
	split = path_split(calc);
#if 0
	printf("post split calc:\n");
	list_dump(calc, path_dump);
#endif
	printf("post split split:\n");
	list_dump(split, path_dump);
	
#endif
}

static void test_mnt_load(void)
{
	list_head *mnts = NULL;
	mnts = mnt_load();
	printf("mnt_load dump:\n");
	list_dump(mnts, mntpt_dump);
}


static void test_user_load(void)
{
	user_t *user = user_load("andy");
	user_dump(user);
}

