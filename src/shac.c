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
#include <stdarg.h>
#include "llist.h"
#include "shac.h"
#include "mnt.h"
#include "perm.h"
#include "user.h"
#include "path.h"
#include "util.h"

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


const char *RPT_STATUS[] = {
	/* do not include an OK msg, we calculate which message to print by (STATUS_* / log(2)) */
	"unknown ???",
	"symlink loop, gave up after MAXSYMLINKS levels"
};

/* these entries are tied to REAS_NO_* values */
const char *RPT_REASONS[] = {
	"??no reason??", /* REAS_NONE */
	"can't read", /* REAS_NO_READ */
	"can't write", /* REAS_NO_WRIT */
	"can't exec", /* REAS_NO_EXEC */
	"can't create", /* REAS_NO_CREA */
	"can't delete", /* REAS_NO_DELE */
	"mounted readonly", /* REAS_MNTPT_RO */
	"mounted noexec", /* REAS_MNTPT_NX */
	"ownership required", /* REAS_NO_STICKY */
	"contains undeletable files", /* REAS_NO_DEPENDANCY */
	"inaccessible" /* REAS_NO_CERTAIN */
};

/**
 * 
 * @note must be synced with "enum rpt" above
 */
const char *RPT_LABELS[] = {
	"??",
	"OK",
	"!!",
	"LN"
};


/* initialize global variables before the program starts */
static void init_globals(void);

/* verbose output functions */
static void verbose_add(const char *, int);
static void verbose_flush(void);
static void verbose_clear(void);

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

/* * * * * * * * * globals * * * * * * * * * * */

list_head *MNTPTS; /* mount points */
static list_head *VERBOSE_MSG; /* verbose output queue, to deal with output order issues */
static unsigned Flag_Verbose = 0;

static void Verbose(unsigned level, int append, const char *format, ...)
{
    va_list args;
    va_start(args, format);
	if (Flag_Verbose >= level) {
		char buf[1024];
		vsnprintf(buf, sizeof buf, format, args);
		verbose_add(buf, append);
	}
    va_end(args);
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
		if (Flag_Verbose >= 1 && OUTPUT_ALL == output) {
			/* normal stuff */
			report(reas, user);
		} else if (Flag_Verbose >= 3 && (OUTPUT_ERR == output && 0 == able)) {
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

/* FIXME: path is completely valid, but for some reason this breaks the variable-argument macro Verbose() */
/* for now we just turn it off, grrrr */
#if 0
	printf("path(%p): \"%s\"\n", (void *)path, path);
	Verbose(2, ADD_APPEND, "VB checking file '%s'...\n", path);
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
				Verbose(2, ADD_APPEND, "VB username '%s' from uid '%s'...\n", username, optarg);
			} else {
				username = strdup(optarg);
				Verbose(2, ADD_APPEND, "VB username '%s'...\n", username);
			}
			break;
		case 'p': /* perms */
			/* allow multiple perm args */
			rawperms = strapp(rawperms, optarg);
			break;
		case 'v': /* verbose */
			if (Flag_Verbose < VERBOSE_MAX)
				++Flag_Verbose;
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
	switch (Flag_Verbose) {
	case 0:
		/* nothing */
		break;
	case 1:
		Verbose(1, ADD_PREPEND, "VB verbose mode...\n");
		break;
	case 2:
		Verbose(2, ADD_PREPEND, "VB very verbose mode...\n");
		break;
	case 3:
		Verbose(3, ADD_PREPEND, "VB very very verbose mode...\n");
		break;
	default:
		fprintf(stderr, "Flag_Verbose: %d\n", Flag_Verbose);
		err_bail(__FILE__, __LINE__, "PE invalid verbosity level");
		break;
	}

#if 0
	printf("main:%d Flag_Verbose: %d\n", __LINE__, Flag_Verbose);
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
		Verbose(2, ADD_APPEND, "VB using default user '%s'...\n", username);
	}
		
	/* no perms, use default */
	if (NULL == rawperms) {
		perm_t mask = decode_perms(DEFAULT_PERMS); /* get canonical perms */
		permdsc_set_mask(perms, mask);
		Verbose(2, ADD_APPEND, "VB using default perms '%s'...\n", perms->dsc);
	} else { /* set up perms */
		perm_t mask = decode_perms(rawperms); /* get canonical perms */
		permdsc_set_mask(perms, mask);
		Verbose(2, ADD_APPEND, "VB using perms '%s'...\n", perms->dsc);
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

