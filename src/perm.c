/* ex: set ts=4: */

#include <stdio.h>
#include <string.h>
#include "perm.h"
#include "util.h"

/*  convert human-readable repr of perms to internal repr */
perm_t decode_perms(const char *rawperms)
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
char *encode_perms(const perm_t perm)
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
permdsc_t * permdsc_alloc(void)
{
	permdsc_t *p = xmalloc(sizeof *p);
	p->dsc = NULL;
	p->mask = PERM_NONE;
	return p;
}

/* clear out existing permdsc_t */
void permdsc_init(permdsc_t *p)
{
#ifdef DEBUG
	assert(NULL != p);
#endif
	xfree(p->dsc);
	p->dsc = NULL;
	p->mask = PERM_NONE;
}

/* set human-readable perms, set mask based on new perms */
void permdsc_set_desc(permdsc_t *p, const char *dsc)
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
void permdsc_set_mask(permdsc_t *p, const perm_t mask)
{
#ifdef DEBUG
	assert(NULL != p);
#endif
	permdsc_init(p);
	p->mask = mask;
	p->dsc = encode_perms(p->mask);
}

/* examine permdsc_t struct */
void permdsc_dump(const void *v)
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
void permdsc_free(void *v)
{
	permdsc_t *p;
#ifdef DEBUG
	assert(NULL != v);
#endif
	p = v;
	permdsc_init(p);
	xfree(p);
}
