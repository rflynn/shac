/* ex: set ts=4: */
/****************************************************************************/
/* Written by Ryan Flynn aka pizza                                          */
/* Copyright 2004 by Ryan Flynn (pizza@parseerror.com)                      */
/****************************************************************************/

#ifndef SHAC_H
#define SHAC_H

#include <unistd.h>
#include "llist.h"
#ifdef LINUX
	#include <mntent.h> /* glibc-ish: setgrent(), getgrent() endgrent(), etc. */
#else
	#include <fstab.h> /* freebsd-ish:  */
#endif
#include <unistd.h> /* getcwd(), getopt() */
#include <pwd.h> /* struct passwd, getpwnam */
#include <grp.h> /* struct group, setgrent, getgrent, endgrent */
#include <sys/stat.h> /* struct stat, stat */


#if 0
#define DEBUG
#undef DEBUG
#endif

#define TRUE 0
#define FALSE (!(TRUE))

#define UID_ROOT (0)
#define USER_NONE (-1)
#define GROUP_NONE (-1)

#define PATHSEP '/'

/* figure out where the mount file location is */
#ifdef _PATH_MOUNTED
	#define SHAC_MNTFILE _PATH_MOUNTED
#endif

/* permissions for which we can test */
#define PERM_NONE 0
#define PERM_READ 1
#define PERM_WRIT 2
#define PERM_EXEC 4
#define PERM_CREA 8
#define PERM_DELE 16
#define PERM_MASK ((PERM_READ | PERM_WRIT | PERM_EXEC | PERM_CREA | PERM_DELE))

#define DEFAULT_PERMS "r"

/* things that can go wrong when we try to access a file */
/* STATUS_* trumps REAS_*... REAS_* is calculated based on perms requested and
perms discovered, STATUS_* usually means there was an error even reading a file.
i should probably try to combine these, but it needs to be thought out more */
#define STATUS_OK 0
#define STATUS_UNKNOWN (1) 
#define STATUS_SYMLINKS_TOO_DEEP (2)

/* reasons why we might not have access to a file */
#define REAS_NONE			PERM_NONE
#define REAS_NO_READ		PERM_READ
#define REAS_NO_WRIT		PERM_WRIT
#define REAS_NO_EXEC		PERM_EXEC
#define REAS_NO_CREA		PERM_CREA
#define REAS_NO_DELE		PERM_DELE
#define REAS_NO_MNTPT_RO	32
#define REAS_NO_MNTPT_NX	64
#define REAS_NO_STICKY		128
#define REAS_NO_DEPENDANCY	256		/* files under a directory cannot be deleted */
#define REAS_NO_CERTAIN		512		/* we were unable to read all the files, so we don't know */

/* specific reasons why we can have access to a file */
#define REAS_YES_UR	1
#define REAS_YES_UW	2
#define REAS_YES_UX	4
#define REAS_YES_GR	16
#define REAS_YES_GW	32
#define REAS_YES_GX	64
#define REAS_YES_OR	256
#define REAS_YES_OW	512
#define REAS_YES_OX	1024
#define REAS_YES_STICKY	2048
#define REAS_YES_OWNER 4096
#define REAS_YES_ROOT 8192

#define STR_USER	"u"
#define STR_GROUP	"g"
#define STR_OTH		"o"
#define STR_READ	"r"
#define STR_WRIT	"w"
#define STR_EXEC	"x"
#define STR_OWNER	"owner"
#define STR_ROOT	"root"

/**
 * every type of report line
 */
enum rpt {
	RPT_NONE	= 0,
	RPT_OK		= 1,
	RPT_NOT_OK	= 2,
	RPT_SYMLNK	= 3
};

#define OUTPUT_NONE	0
#define OUTPUT_ALL	1
#define OUTPUT_ERR	2

/* whether or not to follow symlinks */
#define DONT_FOLLOW 0
#define FOLLOW 1

/* which end should we add to? */
#define ADD_PREPEND 0
#define ADD_APPEND 1

/* deal with verbosity levels */
enum {
	VERBOSE_NONE = 0,
	VERBOSE = 1,
	VERBOSE_VERY = 2,
	VERBOSE_VERY_VERY = 3
};

#define VERBOSE_MAX VERBOSE_VERY_VERY

/* our data structures */

typedef unsigned perm_t;

typedef struct {
	char *mntdir;
	char *mntdev;
	perm_t perms;
} mntpt_t;

#define mntpt_is_readonly(mnt) (PERM_NONE != (mnt->perms & PERM_WRIT))

typedef struct {
	char *name;
	char is_user;
	uid_t uid;
	gid_t gid; /* primary group */
	list_head * groups; /* all users group, including primary */
} user_t;

typedef struct {
	gid_t gid;
	char *name;
} group_t;

/**
 *
 */
typedef struct {
	char *abspath;
	char *dir;
	char *component;
	char *symlink; /* path this file points to if it's a symlink */
	uid_t uid;
	gid_t gid;
	perm_t status; /* could we access this file? why or why not? */
	mode_t mode; /* file info and perm bits */
	mntpt_t *mntpt; /* mnt data or NULL if none */
} path_t;

#define path_is_symlink(path)	((NULL != path->symlink ? 1 : 0))
#define path_is_dir(path)		((S_ISDIR(path->mode) ? 1 : 0))
#define path_is_file(path)		((S_ISREG(path->mode) ? 1 : 0))
#define path_is_sticky(path)	(((S_ISVTX & path->mode) ? 1 : 0))
#define path_is_mntpt(path)		(((NULL != path->mntpt && 0 == strcmp(path->abspath, path->mntpt->mntdir)) ? 1 : 0))
#define path_status_not_ok(path) ((STATUS_OK != path->status))

/* represents information necessary to generate a line of a report */
typedef struct {
	path_t *path;
	int label; /* FIXME: enum rpt ? */
	perm_t yes; /* reasons why */
	perm_t no; /* reasons why not */
} reason_t;

/* holds permissions in human and machine readable format */
typedef struct {
	char *dsc;
	perm_t mask;
} permdsc_t;


#endif
