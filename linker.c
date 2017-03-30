#include "config.h"

#include <sys/queue.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Check that a given row identifier is valid.
 * The rules are that only one row identifier can exist on a structure
 * and that it must happen on a native type.
 */
static int
checkrowid(const struct field *f, int hasrowid)
{

	if (hasrowid) {
		warnx("%s.%s: multiple rowids on "
			"structure", f->parent->name, f->name);
		return(0);
	}

	if (FTYPE_INT != f->type && FTYPE_TEXT != f->type) {
		warnx("%s.%s: rowid on non-native field"
			"type", f->parent->name, f->name);
		return(0);
	}

	return(1);
}

/*
 * Reference rules: we can't reference from or to a struct, nor can the
 * target and source be of a different type.
 */
static int
checktargettype(const struct ref *ref)
{

	if (ref->target->parent == ref->source->parent) {
		warnx("%s.%s: referencing within struct",
			ref->parent->parent->name,
			ref->parent->name);
		return(0);
	}

	/* Our actual reference objects may not be structs. */

	if (FTYPE_STRUCT == ref->target->type ||
	    FTYPE_STRUCT == ref->source->type) {
		warnx("%s.%s: referencing a struct",
			ref->parent->parent->name,
			ref->parent->name);
		return(0);
	} 

	/* Our reference objects must have equivalent types. */

	if (ref->source->type != ref->target->type) {
		warnx("%s.%s: referencing a different type",
			ref->parent->parent->name,
			ref->parent->name);
		return(0);
	}

	return(1);
}

/*
 * When we're parsing a structure's reference, we need to create the
 * referring information to the source field, which is the actual
 * reference itself.
 * Return zero on failure, non-zero on success.
 */
static int
linkref(struct ref *ref)
{

	assert(NULL != ref->parent);
	assert(NULL != ref->source);
	assert(NULL != ref->target);

	if (FTYPE_STRUCT != ref->parent->type)
		return(1);

	/*
	 * If our source field is already a reference, make sure it
	 * points to the same thing we point to.
	 * Otherwise, it's an error.
	 */

	if (NULL != ref->source->ref &&
	    (strcasecmp(ref->tfield, ref->source->ref->tfield) ||
	     strcasecmp(ref->tstrct, ref->source->ref->tstrct))) {
		warnx("%s.%s: redeclaration of reference",
			ref->parent->parent->name,
			ref->parent->name);
		return(0);
	} else if (NULL != ref->source->ref)
		return(1);

	/* Create linkage. */

	ref->source->ref = calloc(1, sizeof(struct ref));
	if (NULL == ref->source->ref)
		err(EXIT_FAILURE, NULL);

	ref->source->ref->parent = ref->source;
	ref->source->ref->source = ref->source;
	ref->source->ref->target = ref->target;

	ref->source->ref->sfield = strdup(ref->sfield);
	ref->source->ref->tfield = strdup(ref->tfield);
	ref->source->ref->tstrct = strdup(ref->tstrct);

	if (NULL == ref->source->ref->sfield ||
	    NULL == ref->source->ref->tfield ||
	    NULL == ref->source->ref->tstrct)
		err(EXIT_FAILURE, NULL);

	return(1);
}

/*
 * Check the source field (case insensitive).
 * Return zero on failure, non-zero on success.
 * On success, this sets the "source" field for the referrent.
 */
static int
checksource(struct ref *ref, struct strct *s)
{
	struct field	*f;

	if (NULL != ref->source)
		return(1);

	assert(NULL == ref->source);
	assert(NULL == ref->target);

	TAILQ_FOREACH(f, &s->fq, entries) {
		if (strcasecmp(f->name, ref->sfield))
			continue;
		ref->source = f;
		return(1);
	}

	warnx("%s.%s: unknown reference source", 
		s->name, ref->sfield);
	return(0);
}

/*
 * Check that the target structure and field exist (case insensitive).
 * Return zero on failure, non-zero on success.
 * On success, this sets the "target" field for the referrent.
 */
static int
checktarget(struct ref *ref, struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	if (NULL != ref->target)
		return(1);

	assert(NULL != ref->source);
	assert(NULL == ref->target);

	TAILQ_FOREACH(p, q, entries) {
		if (strcasecmp(p->name, ref->tstrct))
			continue;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (strcasecmp(f->name, ref->tfield))
				continue;
			ref->target = f;
			return(1);
		}
	}

	warnx("%s.%s: unknown reference target",
		ref->tstrct, ref->tfield);
	return(0);
}

/*
 * Recursively annotate our height from each node.
 * We only do this for FTYPE_STRUCT objects.
 * This makes sure that we don't loop around at any point in our
 * dependencies: this means that we don't do a chain of structures
 * that ends up being self-referential.
 */
static int
annotate(struct ref *ref, size_t height, size_t colour)
{
	struct field	*f;
	struct strct	*p;

	p = ref->target->parent;

	if (p->colour == colour &&
	    ref->source->parent->height > p->height) {
		warnx("loop in assignment: %s.%s -> %s.%s",
			ref->source->parent->name,
			ref->source->name,
			ref->target->parent->name,
			ref->target->name);
		return(0);
	} else if (p->colour == colour)
		return(1);

	p->colour = colour;
	p->height += height;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		assert(NULL != f->ref);
		if ( ! annotate(f->ref, height + 1, colour))
			return(0);
	}

	return(1);
}

/*
 * Sort by reverse height.
 */
static int
parse_cmp(const void *a1, const void *a2)
{
	const struct strct 
	      *p1 = *(const struct strct **)a1, 
	      *p2 = *(const struct strct **)a2;

	return((ssize_t)p1->height - (ssize_t)p2->height);
}

int
parse_link(struct strctq *q)
{
	struct strct	 *p;
	struct strct	**pa;
	struct field	 *f;
	size_t		  colour = 1, sz = 0, i = 0, hasrowid = 0;

	/* 
	 * First, establish linkage between nodes.
	 * While here, check for duplicate rowids.
	 */

	TAILQ_FOREACH(p, q, entries) {
		hasrowid = 0;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (FIELD_ROWID & f->flags)
				if ( ! checkrowid(f, hasrowid++))
					return(0);
			if (NULL == f->ref)
				continue;
			if ( ! checksource(f->ref, p) ||
			     ! checktarget(f->ref, q) ||
			     ! linkref(f->ref) ||
			     ! checktargettype(f->ref))
				return(0);
		}
	}

	/* 
	 * Now follow and order all outbound links for structs.
	 * From the get-go, we don't descend into structures that we've
	 * already coloured.
	 */

	TAILQ_FOREACH(p, q, entries) {
		sz++;
		if (p->colour)
			continue;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (FTYPE_STRUCT != f->type)
				continue;
			assert(NULL != f->ref);
			p->colour = colour;
			if ( ! annotate(f->ref, 1, colour))
				return(0);
		}
		colour++;
	}

	/* 
	 * Copy the list into a temporary array.
	 * Then sort the list by reverse-height.
	 * Finally, re-create the list from the sorted elements.
	 */

	if (NULL == (pa = calloc(sz, sizeof(struct strct *))))
		err(EXIT_FAILURE, NULL);
	while (NULL != (p = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, p, entries);
		assert(i < sz);
		pa[i++] = p;
	}
	assert(i == sz);
	qsort(pa, sz, sizeof(struct strct *), parse_cmp);
	for (i = 0; i < sz; i++)
		TAILQ_INSERT_HEAD(q, pa[i], entries);

	free(pa);
	return(1);
}
