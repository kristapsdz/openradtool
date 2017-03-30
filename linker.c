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
 * Check the source, which must fall within the same structure
 * (obviously) as the referrent.
 * Do not do anything for non-structure references, since they're
 * already tied to a source.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "source" field for the referrent.
 */
static int
checksource(struct ref *ref, struct strct *s)
{
	struct field	*f;

	if (NULL != ref->source)
		return(1);

	TAILQ_FOREACH(f, &s->fq, entries) {
		if (strcmp(f->name, ref->sfield))
			continue;
		ref->source = f;
		return(1);
	}

	warnx("%s: unknown source", ref->sfield);
	return(0);
}

/*
 * Reference rules: we can't reference from or to a struct, nor can the
 * target and source be of a different type.
 */
static int
checktargettype(const struct ref *ref)
{

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

	/* We can't (yet) redeclare our types. */

	if (FTYPE_STRUCT == ref->parent->type &&
	    NULL != ref->source->ref) {
		warnx("%s.%s: redeclaration of reference",
			ref->parent->parent->name,
			ref->parent->name);
		return(0);
	}

	return(1);
}

/*
 * Check the target structure and field.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "target" field for the referrent.
 */
static int
checktarget(struct ref *ref, struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	TAILQ_FOREACH(p, q, entries) {
		if (strcmp(p->name, ref->tstrct))
			continue;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (strcmp(f->name, ref->tfield))
				continue;
			ref->target = f;
			return(checktargettype(ref));
		}
	}

	warnx("%s.%s: unknown target",
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
			     ! checktarget(f->ref, q))
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
			if ( ! annotate(f->ref, 0, colour))
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
