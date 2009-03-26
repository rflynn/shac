/* ex: set ts=4: */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  /* isdigit */
#include <unistd.h> /* readlink */
#include "shac.h"
#include "util.h"

/* general fatal error... print msg and then exit with failure */
void err_bail(const char *file, int line, const char *msg)
{
#ifdef DEBUG
	assert(NULL != file);
	assert(NULL != msg);
#endif
	fprintf(stderr, "%s:%d:%s!\nbailing!\n", file, line, msg);
	exit(EXIT_FAILURE);
}

/* could not allocate memory. this is called from xmalloc */
void err_nomem(const char *file, int line, size_t bytes)
{
#ifdef DEBUG
	assert(NULL != file);
#endif

	fprintf(stderr, "%s:%d: could not allocate %d bytes!\n",
		file, line, bytes);

	exit(EXIT_FAILURE);
}

/* could not open file fatal error */
void err_cantopen(const char *file, int line, const char *filename)
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
void err_opendir(const char *file, int line, const char *dirname)
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
void fatal_invalid_user(const char *username)
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
void fatal_invalid_path(const char *file, int line, const char *path, int err)
{
#ifdef DEBUG
	assert(NULL != path);
#endif
	fprintf(stderr, "(%s:%d) ERR file '%s' %s\n",
		file, line, path, (0 == err ? "" : strerror(err)));
	exit(EXIT_FAILURE);
}

/* general fatal error, like an option */
void fatal(const char *msg)
{
#ifdef DEBUG
	assert(NULL != msg);
#endif
	fprintf(stderr, "ERR %s\n", msg);
	exit(EXIT_FAILURE);
}


/* die if mem allocation fails */
void * xmalloc(size_t bytes)
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
void * xrealloc(void *ptr, size_t bytes)
{
	if (NULL == (ptr = realloc(ptr, bytes)))
		err_nomem(__FILE__, __LINE__, bytes);
#ifdef DEBUG
	printf("xrealloc() %u bytes at %p\n", bytes, (void *)ptr);
#endif
	return ptr;
}

/* don't try to free if already NULL */
void xfree(void *ptr)
{
	if (NULL != ptr) {
#ifdef DEBUG
		printf("xfree(%p)\n", ptr);
#endif
		free(ptr);
	}
}

/* allocate and read buffer for the filename that filename, a symlink, points to */
char * readlink_malloc(const char *filename)
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
void str_examine(const char *c)
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
char * strnchr(const char *s, char c)
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
char * strndup(const char *s, int n)
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
char * strapp(char *s, const char *app)
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
char *strcapp(char *s, const char c)
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
int strisnum(const char *s)
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
void shac_list_list_free(void *v)
{
	list_head *head;
#ifdef DEBUG
	assert(NULL != v);
#endif
	head = v;
	list_free(head, NULL);
}


