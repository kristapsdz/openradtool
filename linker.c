/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

	if ( ! (FIELD_ROWID & ref->target->flags)) 
		warnx("%s.%s: referenced target %s.%s is not "
			"a unique field",
			ref->parent->parent->name,
			ref->parent->name,
			ref->target->parent->name,
			ref->target->name);

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
resolvesource(struct ref *ref, struct strct *s)
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
resolvetarget(struct ref *ref, struct strctq *q)
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
 * Recursively follow the chain of references in a search target,
 * finding out whether it's well-formed in the process.
 * Returns zero on failure, non-zero on success.
 */
static int
resolvesref(struct sref *ref, struct strct *s)
{
	struct field	*f;

	TAILQ_FOREACH(f, &s->fq, entries)
		if (0 == strcasecmp(f->name, ref->name))
			break;

	/* Did we find the field in our structure? */

	if (NULL == (ref->field = f)) {
		warnx("%s.%s: search term not found",
			s->name, ref->name);
		return(0);
	}

	/* 
	 * If we're following a chain, we must have a "struct" type;
	 * otherwise, we must have a native type.
	 */

	if (NULL == TAILQ_NEXT(ref, entries)) {
		if (FTYPE_STRUCT != f->type) 
			return(1);
		warnx("%s.%s: search term leaf field "
			"is a struct", f->parent->name,
			f->name);
		return(0);
	} else if (FTYPE_STRUCT != f->type) {
		warnx("%s.%s: search term node field "
			"is not a struct", f->parent->name,
			f->name);
		return(0);
	}

	/* Follow the chain of our reference. */

	ref = TAILQ_NEXT(ref, entries);
	return(resolvesref(ref, f->ref->target->parent));
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

/*
 * Recursively create the list of all possible search prefixes we're
 * going to see in this structure.
 * This consists of all "parent.child" chains of structure that descend
 * from the given "orig" original structure.
 * FIXME: artificially limited to 26 entries.
 */
static void
aliases(struct strct *orig, struct strct *p, 
	size_t *offs, const struct alias *prior)
{
	struct field	*f;
	struct alias	*a;
	int		 c;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		assert(NULL != f->ref);
		
		a = calloc(1, sizeof(struct alias));
		if (NULL == a)
			err(EXIT_FAILURE, NULL);

		if (NULL != prior) {
			c = asprintf(&a->name, "%s.%s",
				prior->name, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else
			a->name = strdup(f->name);

		if (NULL == a->name)
			err(EXIT_FAILURE, NULL);

		assert(*offs < 26);
		c = asprintf(&a->alias, 
			"_%c", (char)*offs + 97);
		if (c < 0)
			err(EXIT_FAILURE, NULL);

		(*offs)++;
		TAILQ_INSERT_TAIL(&orig->aq, a, entries);
		aliases(orig, f->ref->target->parent, offs, a);
	}
}

int
parse_link(struct strctq *q)
{
	struct strct	 *p;
	struct strct	**pa;
	struct alias	 *a;
	struct field	 *f;
	struct sent	 *sent;
	struct search	 *srch;
	struct sref	 *ref;
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
			if ( ! resolvesource(f->ref, p) ||
			     ! resolvetarget(f->ref, q) ||
			     ! linkref(f->ref) ||
			     ! checktargettype(f->ref))
				return(0);
		}
	}

	/* 
	 * Now follow and order all outbound links for structs.
	 * From the get-go, we don't descend into structures that we've
	 * already coloured.
	 * FIXME: instead, have this just descend from each node and
	 * search for finding "again" this node, which would mean a
	 * loop.
	 * We can envision situations where we might want to allow
	 * nested situations, but we'll leave that up to the user not to
	 * use a struct and do it all manually.
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
	 * Next, create unique names for all joins within a structure.
	 * We do this by creating a list of all search patterns (e.g.,
	 * user.name and user.company.name, which assumes two structures
	 * "user" and "company", the first pointing into the second,
	 * both of which contain "name").
	 */

	i = 0;
	TAILQ_FOREACH(p, q, entries)
		aliases(p, p, &i, NULL);

	/*
	 * Resolve the chain of search terms.
	 * To do so, we need to descend into each set of search terms
	 * for the structure and resolve the fields.
	 */

	TAILQ_FOREACH(p, q, entries)
		TAILQ_FOREACH(srch, &p->sq, entries)
			TAILQ_FOREACH(sent, &srch->sntq, entries) {
				ref = TAILQ_FIRST(&sent->srq);
				if ( ! resolvesref(ref, p))
					return(0);
				if (NULL == sent->name)
					continue;

				/* 
				 * Look up our alias name.
				 * Our resolvesref() function above
				 * makes sure that the reference exists,
				 * so just assert on lack of finding.
				 */

				TAILQ_FOREACH(a, &p->aq, entries)
					if (0 == strcasecmp
					    (a->name, sent->name))
						break;
				assert(NULL != a);
				sent->alias = a;
			}

	/* 
	 * Copy the list into a temporary array.
	 * Then sort the list by reverse-height.
	 * Finally, re-create the list from the sorted elements.
	 */

	if (NULL == (pa = calloc(sz, sizeof(struct strct *))))
		err(EXIT_FAILURE, NULL);
	i = 0;
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
