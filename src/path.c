/* ex: set ts=4: */

#include <stdio.h>
#include <string.h>
#include <sys/param.h> /* MAXSYMLINKS */
#include <errno.h>
#include "mnt.h"
#include "path.h"
#include "util.h"

path_t * path_alloc(void)
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
void path_init(path_t *path)
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

void * path_dupe(const void *v)
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
void path_free(void *v)
{
	path_t *p;
	if (NULL == v)
		return;
	p = v;
	path_init(p); /* clears sub-items */
	xfree(p);
}

/* callback for list_dump to inspect a path_t */
void path_dump(const void *v)
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


/*********************** "algorithm" functions ******************************/

/* suppled could be dirty, we need to figure out if it's absolute, if it's not... */
/* we combine it with cwd */
/* if it is, we just clean it up */
list_head *path_calc_target(const char *supplied, const char *cwd)
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

extern list_head *MNTPTS; /* mount points */

/* get cwd, split into list */
/* most of the path logic is here */
/* FIXME: this function is too long, needs to be broken up */
list_head *path_split(list_head **rawpath, int follow_symlinks)
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


