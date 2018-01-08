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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static void gen_errx(const struct pos *, const char *, ...)
	__attribute__((format(printf, 2, 3)));
static void gen_warnx(const struct pos *, const char *, ...)
	__attribute__((format(printf, 2, 3)));

static	const char *const rolemapts[ROLEMAP__MAX] = {
	"all", /* ROLEMAP_ALL */
	"delete", /* ROLEMAP_DELETE */
	"insert", /* ROLEMAP_INSERT */
	"iterate", /* ROLEMAP_ITERATE */
	"list", /* ROLEMAP_LIST */
	"search", /* ROLEMAP_SEARCH */
	"update", /* ROLEMAP_UPDATE */
	"noexport", /* ROLEMAP_NOEXPORT */
};

static void
gen_warnx(const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s:%zu:%zu: warning: %s\n", 
		pos->fname, pos->line, pos->column, buf);
}

static void
gen_errx(const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s:%zu:%zu: error: %s\n", 
		pos->fname, pos->line, pos->column, buf);
}


/*
 * Check that a given row identifier is valid.
 * The rules are that only one row identifier can exist on a structure
 * and that it must happen on a native type.
 */
static int
checkrowid(const struct field *f, int hasrowid)
{

	if (hasrowid)
		gen_errx(&f->pos, "multiple rowids");
	else if (FTYPE_STRUCT == f->type)
		gen_errx(&f->pos, "rowid on non-native type");
	else
		return(1);

	return(0);
}

/*
 * Check the source field (case insensitive).
 * This applies to all reference types, "struct" and otherwise.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "source" field for the referrer.
 */
static int
resolve_field_source(struct field *f)
{
	struct field	*ff;

	assert(NULL == f->ref->source);
	assert(NULL == f->ref->target);

	if (FTYPE_STRUCT != f->type) {
		/* 
		 * This is a non-struct reference, where the sfield is
		 * going to be the field name itself.
		 * We also know that it's valid, so no need to check
		 * more.
		 */
		assert(0 == strcmp(f->name, f->ref->sfield));
		f->ref->source = f;
		return(1);
	}

	/*
	 * Look up the source.
	 * Then copy over the source's targets into our own.
	 */

	assert(NULL == f->ref->tfield);
	assert(NULL == f->ref->tstrct);

	TAILQ_FOREACH(ff, &f->parent->fq, entries)
		if (0 == strcasecmp(ff->name, f->ref->sfield)) {
			f->ref->source = ff;
			break;
		}

	if (NULL == f->ref->source) {
		gen_errx(&f->pos, "unknown reference source");
		return(0);
	} else if (NULL == f->ref->source->ref) {
		gen_errx(&f->pos, "reference to non-foreign key");
		return(0);
	} else if (FTYPE_STRUCT == f->ref->source->type) {
		gen_errx(&f->pos, "reference to non-native type");
		return(0);
	} 

	assert(NULL != f->ref->source->ref->tfield);
	assert(NULL != f->ref->source->ref->tstrct);

	f->ref->tfield = strdup(f->ref->source->ref->tfield);
	f->ref->tstrct = strdup(f->ref->source->ref->tstrct);

	if (NULL == f->ref->tfield || NULL == f->ref->tstrct) 
		err(EXIT_FAILURE, NULL);

	return(1);
}

/*
 * Check that the target structure and field exist (case insensitive)
 * and are appropriate.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "target" field for the referrent.
 */
static int
resolve_field_target(struct field *f, struct strctq *q)
{
	struct strct	*p;
	struct field	*ff;

	assert(NULL != f->ref->source);
	assert(NULL == f->ref->target);
	assert(NULL != f->ref->tfield);
	assert(NULL != f->ref->tstrct);

	TAILQ_FOREACH(p, q, entries) {
		if (strcasecmp(p->name, f->ref->tstrct))
			continue;
		TAILQ_FOREACH(ff, &p->fq, entries) {
			if (strcasecmp(ff->name, f->ref->tfield))
				continue;
			f->ref->target = ff;
			break;
		}
	}

	if (NULL == f->ref->target) {
		gen_errx(&f->pos, "unknown reference target");
		return(0);
	} else if (f->ref->source->type != f->ref->target->type) {
		gen_errx(&f->pos, "target type mismatch");
		return(0);
	} else if ( ! (FIELD_ROWID & f->ref->target->flags) &&
	            ! (FIELD_UNIQUE & f->ref->target->flags)) {
		gen_errx(&f->pos, "target is not a rowid or unique");
		return(0);
	}

	return(1);
}

/*
 * Resolve a bitfield.
 * This returns zero if the resolution fails, non-zero otherwise.
 * In the success case, it sets the bitfield link.
 */
static int
resolve_field_bitfield(struct bref *ref, struct bitfq *q)
{
	struct bitf	*b;

	TAILQ_FOREACH(b, q, entries)
		if (0 == strcasecmp(b->name, ref->name)) {
			ref->bitf = b;
			return(1);
		}

	gen_errx(&ref->parent->pos, "unknown bitfield reference");
	return(0);
}

/*
 * Resolve an enumeration.
 * This returns zero if the resolution fails, non-zero otherwise.
 * In the success case, it sets the enumeration link.
 */
static int
resolve_field_enum(struct eref *ref, struct enmq *q)
{
	struct enm	*e;

	TAILQ_FOREACH(e, q, entries)
		if (0 == strcasecmp(e->name, ref->ename)) {
			ref->enm = e;
			return(1);
		}

	gen_errx(&ref->parent->pos, "unknown enum reference");
	return(0);
}

/*
 * Recursively check for... recursion.
 * Returns zero if the reference is recursive, non-zero otherwise.
 */
static int
check_recursive(struct ref *ref, const struct strct *check)
{
	struct field	*f;
	struct strct	*p;

	assert(NULL != ref);

	if ((p = ref->target->parent) == check)
		return(0);

	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			if ( ! check_recursive(f->ref, check))
				return(0);

	return(1);
}

/*
 * Recursively annotate our height from each node.
 * We only do this for FTYPE_STRUCT objects.
 */
static void
annotate(struct ref *ref, size_t height, size_t colour)
{
	struct field	*f;
	struct strct	*p;

	p = ref->target->parent;

	if (p->colour == colour)
		return;

	p->colour = colour;
	p->height += height;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			annotate(f->ref, height + 1, colour);
}

/*
 * Resolve a specific update reference by looking it up in our parent
 * structure.
 * Return zero on failure, non-zero on success.
 */
static int
resolve_uref(struct uref *ref, int crq)
{
	struct field	*f;

	assert(NULL == ref->field);
	assert(NULL != ref->parent);

	TAILQ_FOREACH(f, &ref->parent->parent->fq, entries)
		if (0 == strcasecmp(f->name, ref->name))
			break;

	if (NULL == (ref->field = f))
		gen_errx(&ref->pos, "term not found");
	else if (FTYPE_STRUCT == f->type)
		gen_errx(&ref->pos, "term is a struct");
	else if (crq && FTYPE_PASSWORD == f->type)
		gen_errx(&ref->pos, "term is a password");
	else
		return(1);

	return(0);
}

/*
 * Make sure that our constraint operator is consistent with the type
 * named in the constraint.
 * Returns zero on failure, non-zero on success.
 * (For the time being, this always returns non-zero.)
 */
static int
check_updatetype(struct update *up)
{
	struct uref	*ref;

	TAILQ_FOREACH(ref, &up->crq, entries) {
		if ((OPTYPE_NOTNULL == ref->op ||
		     OPTYPE_ISNULL == ref->op) &&
		    ! (FIELD_NULL & ref->field->flags))
			gen_warnx(&ref->pos, "null operator "
				"on field that's never null");
		if (OPTYPE_LIKE == ref->op &&
		    FTYPE_TEXT != ref->field->type &&
		    FTYPE_EMAIL != ref->field->type) {
			gen_errx(&ref->pos, "LIKE operator "
				"on non-textual field.");
			return(0);
		}
	}
	return(1);
}

/*
 * Make sure that our modification type is numeric.
 * (Text-based modifications with "add" or "sub" or otherwise don't
 * really make sense.
 */
static int
check_modtype(const struct uref *ref)
{

	assert(MODTYPE__MAX != ref->mod);

	if (MODTYPE_SET == ref->mod ||
	    FTYPE_BIT == ref->field->type ||
	    FTYPE_DATE == ref->field->type ||
	    FTYPE_EPOCH == ref->field->type ||
	    FTYPE_INT == ref->field->type ||
	    FTYPE_REAL == ref->field->type)
		return(1);

	gen_errx(&ref->pos, "update modification on "
		"invalid field type (not numeric)");
	return(0);
}

/*
 * Resolve all of the fields managed by struct update.
 * These are all local to the current structure.
 * (This is a constraint of SQL.)
 * Return zero on failure, non-zero on success.
 */
static int
resolve_update(struct update *up)
{
	struct uref	*ref;
	struct field	*f;

	/*
	 * If this is an update clause and we didn't include any fields
	 * to update, that means we want to update all of them.
	 */

	if (UP_MODIFY == up->type && TAILQ_EMPTY(&up->mrq)) {
		up->flags |= UPDATE_ALL;
		TAILQ_FOREACH(f, &up->parent->fq, entries) {
			if (FIELD_ROWID & f->flags)
				continue;
			if (FTYPE_STRUCT == f->type)
				continue;
			ref = calloc(1, sizeof(struct uref));
			if (NULL == ref)
				err(EXIT_FAILURE, NULL);
			ref->name = strdup(f->name);
			if (NULL == ref->name)
				err(EXIT_FAILURE, NULL);
			ref->parent = up;
			ref->pos = up->pos;
			TAILQ_INSERT_TAIL(&up->mrq, ref, entries);
		}
	}

	/* Will always be empty for UP_DELETE. */

	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if ( ! resolve_uref(ref, 0))
			return(0);
		if ( ! check_modtype(ref))
			return(0);
	}
	TAILQ_FOREACH(ref, &up->crq, entries)
		if ( ! resolve_uref(ref, 1))
			return(0);

	return(1);
}

/*
 * Like resolve_sref() but for distinct references.
 * These must be always non-null struct refs.
 */
static int
resolve_dref(struct dref *ref, struct strct *s)
{
	struct field	*f;

	TAILQ_FOREACH(f, &s->fq, entries)
		if (0 == strcasecmp(f->name, ref->name))
			break;

	/* Make sure we're a non-null struct. */

	if (NULL == f) {
		gen_errx(&ref->pos, "distinct field "
			"not found: %s", ref->name);
		return(0);
	} else if (FTYPE_STRUCT != f->type) {
		gen_errx(&ref->pos, "distinct field "
			"not a struct: %s", f->name);
		return(0);
	} else if (FIELD_NULL & f->ref->source->flags) {
		gen_errx(&ref->pos, "distinct field "
			"is a null struct: %s", f->name);
		return(0);
	}

	if (NULL != TAILQ_NEXT(ref, entries)) {
		ref = TAILQ_NEXT(ref, entries);
		return(resolve_dref(ref, f->ref->target->parent));
	}

	ref->parent->strct = f->ref->target->parent;
	return(1);
}

/*
 * Like resolve_sref() but for order references.
 * The non-terminal fields must be non-null structs; the terminal field
 * must be a regular field.
 */
static int
resolve_oref(struct oref *ref, struct strct *s)
{
	struct field	*f;

	TAILQ_FOREACH(f, &s->fq, entries)
		if (0 == strcasecmp(f->name, ref->name))
			break;

	if (NULL == (ref->field = f)) {
		gen_errx(&ref->pos, "order term "
			"not found: %s", ref->name);
		return(0);
	}

	/* 
	 * Terminal field must be a non-struct.
	 * Null is ok but it'll make for strange results...?
	 */

	if (NULL == TAILQ_NEXT(ref, entries)) {
		if (FTYPE_STRUCT != f->type) 
			return(1);
		gen_errx(&ref->pos, "order terminal field "
			"is a struct: %s", f->name);
		return(0);
	} 

	/* Non-terminals must be non-null structs. */
	
	if (FTYPE_STRUCT != f->type) {
		gen_errx(&ref->pos, "order non-terminal "
			"field not a struct: %s", f->name);
		return(0);
	} else if (FIELD_NULL & f->ref->source->flags) {
		gen_errx(&ref->pos, "order non-terminal "
			"field is a null struct: %s", f->name);
		return(0);
	}

	ref = TAILQ_NEXT(ref, entries);
	return(resolve_oref(ref, f->ref->target->parent));
}

/*
 * Recursively follow the chain of references in a search target,
 * finding out whether it's well-formed in the process.
 * Returns zero on failure, non-zero on success.
 */
static int
resolve_sref(struct sref *ref, struct strct *s)
{
	struct field	*f;

	TAILQ_FOREACH(f, &s->fq, entries)
		if (0 == strcasecmp(f->name, ref->name))
			break;

	if (NULL == (ref->field = f)) {
		gen_errx(&ref->pos, "search term "
			"not found: %s", ref->name);
		return(0);
	}

	/* 
	 * If we're following a chain, we must have a "struct" type;
	 * otherwise, we must have a native type.
	 */

	if (NULL == TAILQ_NEXT(ref, entries)) {
		if (FTYPE_STRUCT != f->type) 
			return(1);
		gen_errx(&ref->pos, "search terminal field "
			"is a struct: %s", f->name);
		return(0);
	} 
	
	if (FTYPE_STRUCT != f->type) {
		gen_errx(&ref->pos, "search non-terminal "
			"field not a struct: %s", f->name);
		return(0);
	} else if (FIELD_NULL & f->ref->source->flags) {
		gen_errx(&ref->pos, "search non-terminal "
			"field is a null struct: %s", f->name);
		return(0);
	}

	ref = TAILQ_NEXT(ref, entries);
	return(resolve_sref(ref, f->ref->target->parent));
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
 * FIXME: limited to 26*26*26 entries.
 */
static void
resolve_aliases(struct strct *orig, struct strct *p, 
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

		assert(*offs < 26 * 26 * 26);

		if (*offs >= 26 * 26)
			c = asprintf(&a->alias, "_%c%c%c", 
				(char)(*offs / 26 / 26) + 97,
				(char)(*offs / 26) + 97,
				(char)(*offs % 26) + 97);
		else if (*offs >= 26)
			c = asprintf(&a->alias, "_%c%c", 
				(char)(*offs / 26) + 97,
				(char)(*offs % 26) + 97);
		else
			c = asprintf(&a->alias, 
				"_%c", (char)*offs + 97);

		if (c < 0)
			err(EXIT_FAILURE, NULL);

		(*offs)++;
		TAILQ_INSERT_TAIL(&orig->aq, a, entries);
		resolve_aliases(orig, f->ref->target->parent, offs, a);
	}
}

/*
 * Check to see that our search type (e.g., list or iterate) is
 * consistent with the fields that we're searching for.
 * In other words, running an iterator search on a unique row isn't
 * generally useful.
 * Also warn if null-sensitive operators (isnull, notnull) will be run
 * on non-null fields.
 * Return zero on failure, non-zero on success.
 */
static int
check_searchtype(struct strct *p)
{
	const struct sent *sent;
	const struct sref *sr;
	struct search 	*srch;
	struct dref	*dr;

	TAILQ_FOREACH(srch, &p->sq, entries) {
		/*
		 * FIXME: this should be possible if we have
		 *   search: limit 1
		 * This uses random ordering (which should be warned
		 * about as well), but it's sometimes desirable like in
		 * the case of having a single-entry table.
		 */
		if (STYPE_SEARCH == srch->type &&
		    TAILQ_EMPTY(&srch->sntq)) {
			gen_errx(&srch->pos, "unique result search "
				"without parameters");
			return(0);
		}
		if (SEARCH_IS_UNIQUE & srch->flags && 
		    STYPE_SEARCH != srch->type) 
			gen_warnx(&srch->pos, "multiple-result search "
				"on a unique field");
		if ( ! (SEARCH_IS_UNIQUE & srch->flags) && 
		    STYPE_SEARCH == srch->type && 1 != srch->limit)
			gen_warnx(&srch->pos, "single-result search "
				"on a non-unique field without a "
				"limit of one");

		TAILQ_FOREACH(sent, &srch->sntq, entries) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			if ((OPTYPE_NOTNULL == sent->op ||
			     OPTYPE_ISNULL == sent->op) &&
			    ! (FIELD_NULL & sr->field->flags))
				gen_warnx(&sent->pos, "null operator "
					"on field that's never null");

			/* 
			 * FIXME: we should (in theory) allow for the
			 * unary types and equality binary types.
			 * But for now, mandate equality.
			 */

			if (OPTYPE_EQUAL != sent->op &&
			    FTYPE_PASSWORD == sr->field->type) {
				gen_errx(&sent->pos, "password field "
					"only processes equality");
				return(0);
			}

			/* Require text types for LIKE operator. */

			if (OPTYPE_LIKE == sent->op &&
			    FTYPE_TEXT != sr->field->type &&
			    FTYPE_EMAIL != sr->field->type) {
				gen_errx(&sent->pos, "LIKE operator "
					"on non-textual field.");
				return(0);
			}
		}

		if (NULL == srch->dst)
			continue;

		/*
		 * Resolve the "distinct" structure.
		 * If this is unset, then our entire structure is marked
		 * with the distinction keyword.
		 */

		if (NULL != srch->dst->cname) {
			dr = TAILQ_FIRST(&srch->dst->drefq);
			if ( ! resolve_dref(dr, p))
				return(0);
		} else {
			srch->dst->strct = p;
			continue;
		}

		/* 
		 * Disallow passwords.
		 * TODO: this should allow for passwords *within* the
		 * distinct subparts.
		 */

		TAILQ_FOREACH(sent, &srch->sntq, entries) {
			if (OPTYPE_ISUNARY(sent->op))
				continue;
			sr = TAILQ_LAST(&sent->srq, srefq);
			if (FTYPE_PASSWORD != sr->field->type) 
				continue;
			gen_errx(&sent->pos, "password search "
				"types not allowed when searching "
				"on distinct subsets");
			return(0);
		}
	}

	return(1);
}

/*
 * Resolve the chain of search terms.
 * To do so, descend into each set of search terms for the structure and
 * resolve the fields.
 * Also set whether we have row identifiers within the search expansion.
 */
static int
resolve_search(struct search *srch)
{
	struct sent	*sent;
	struct sref	*sref;
	struct oref	*oref;
	struct alias	*a;
	struct strct	*p;
	struct ord	*ord;

	p = srch->parent;

	TAILQ_FOREACH(sent, &srch->sntq, entries) {
		sref = TAILQ_FIRST(&sent->srq);
		if ( ! resolve_sref(sref, p))
			return(0);

		/*
		 * Check if this is a unique search result that will
		 * reduce our search to a singleton result always.
		 * This happens if the field value itself is unique
		 * (i.e., rowid or unique) AND the check is for
		 * equality.
		 */

		sref = TAILQ_LAST(&sent->srq, srefq);
		if ((FIELD_ROWID & sref->field->flags ||
		     FIELD_UNIQUE & sref->field->flags) &&
		     OPTYPE_EQUAL == sent->op) {
			sent->flags |= SENT_IS_UNIQUE;
			srch->flags |= SEARCH_IS_UNIQUE;
		}
		if (NULL == sent->name)
			continue;

		/* 
		 * Look up our alias name.
		 * Our resolve_sref() function above
		 * makes sure that the reference exists,
		 * so just assert on lack of finding.
		 */

		TAILQ_FOREACH(a, &p->aq, entries)
			if (0 == strcasecmp(a->name, sent->name))
				break;
		assert(NULL != a);
		sent->alias = a;
	}

	/* Now the same but for order statements. */

	TAILQ_FOREACH(ord, &srch->ordq, entries) {
		oref = TAILQ_FIRST(&ord->orq);
		if ( ! resolve_oref(oref, p))
			return(0);

		if (NULL == ord->name)
			continue;
		TAILQ_FOREACH(a, &p->aq, entries)
			if (0 == strcasecmp(a->name, ord->name))
				break;
		assert(NULL != a);
		ord->alias = a;
	}

	return(1);
}

static int
check_unique(const struct unique *u)
{
	const struct nref *n;

	TAILQ_FOREACH(n, &u->nq, entries) {
		if (FTYPE_STRUCT != n->field->type)
			continue;
		gen_errx(&n->pos, "field not a native type");
		return(0);
	}

	return(1);
}

/*
 * Resolve the chain of unique fields.
 * These are all in the local structure.
 */
static int
resolve_unique(struct unique *u)
{
	struct nref	*n;
	struct field	*f;

	TAILQ_FOREACH(n, &u->nq, entries) {
		TAILQ_FOREACH(f, &u->parent->fq, entries) 
			if (0 == strcasecmp(f->name, n->name))
				break;
		if (NULL != (n->field = f))
			continue;
		gen_errx(&n->pos, "field not found");
		return(0);
	}

	return(1);
}

/*
 * Given a single roleset "rs", look it up recursively in the queue
 * given by "rq", which should initially be invoked by the top-level
 * queue of the configuration.
 * On success, sets the associated role and returns non-zero.
 * On failure, returns zero.
 */
static int
resolve_roles(struct roleset *rs, struct roleq *rq)
{
	struct role 	*r;

	TAILQ_FOREACH(r, rq, entries) {
		if (0 == strcasecmp(rs->name, r->name)) {
			rs->role = r;
			return(1);
		} else if (resolve_roles(rs, &r->subrq))
			return(1);
	}

	return(0);
}

/*
 * Given the rolemap "rm", look through all its rolesets ("roles") and
 * match the roles to those given in the configuration "cfg".
 * Then make sure that the matched roles, if any, don't overlap in the
 * tree of roles.
 * Return the number of bad matches (zero means all good).
 * Reports its errors.
 */
static size_t
resolve_roleset(struct rolemap *rm, struct config *cfg)
{
	struct roleset	*rs, *rrs;
	struct role	*rp;
	size_t		 i = 0;

	TAILQ_FOREACH(rs, &rm->setq, entries) {
		if (resolve_roles(rs, &cfg->rq))
			continue;
		gen_errx(&rs->parent->pos, "unknown role: %s", rs->name);
		i++;
	}

	/*
	 * For each valid role in the roleset, see if another role is
	 * specified that's a parent of the current role.
	 * Don't use top-level roles, since by definition they don't
	 * overlap with anything else.
	 */

	TAILQ_FOREACH(rs, &rm->setq, entries) {
		if (NULL == rs->role)
			continue;
		TAILQ_FOREACH(rrs, &rm->setq, entries) {
			if (rrs == rs || NULL == (rp = rrs->role))
				continue;
			assert(rp != rs->role);
			for ( ; NULL != rp; rp = rp->parent)
				if (rp == rs->role)
					break;
			if (NULL == rp)
				continue;
			gen_errx(&rs->parent->pos, 
				"overlapping role: %s", rs->name);
			i++;
		}
	}

	return(i);
}

/*
 * Performs the work of resolve_roleset_cover() by actually accepting a
 * (possibly-NULL) rolemap, looking through the roleset, and seeing if
 * we're clobbering an existing role.
 * If we're not, then we add our role to the roleset.
 * This might mean that we're going to create a rolemap in the process.
 */
static size_t
resolve_roleset_coverset(const struct roleset *rs, 
	struct rolemap **rm, enum rolemapt type, 
	const char *name, struct strct *p)
{
	struct roleset	*rrs;

	/*
	 * If there are no roles defined for a function at all, we need
	 * to fake up a rolemap on the spot.
	 * Obviously, this will lead to inserting for the "all"
	 * statement, below.
	 */

	if (NULL == *rm) {
		*rm = calloc(1, sizeof(struct rolemap));
		if (NULL == *rm)
			err(EXIT_FAILURE, NULL);
		(*rm)->type = type;
		if (NULL != name &&
		    NULL == ((*rm)->name = strdup(name)))
			err(EXIT_FAILURE, NULL);
		TAILQ_INIT(&(*rm)->setq);
		(*rm)->pos = rs->parent->pos;
		TAILQ_INSERT_TAIL(&p->rq, *rm, entries);
	}

	/* See if our role overlaps. */

	TAILQ_FOREACH(rrs, &(*rm)->setq, entries) {
		if (NULL == rrs->role ||
		    rs->role != rrs->role)
			continue;
		gen_errx(&(*rm)->pos, "role overlapped "
			"by \"all\" statement");
		return(1);
	}

	/*
	 * Our role didn't appear in the roleset of the give rolemap.
	 * Add it now.
	 */

	if (NULL == (rrs = malloc(sizeof(struct roleset))))
		err(EXIT_FAILURE, NULL);
	if (NULL == (rrs->name = strdup(rs->name)))
		err(EXIT_FAILURE, NULL);
	rrs->role = rs->role;
	rrs->parent = *rm;
	TAILQ_INSERT_TAIL(&(*rm)->setq, rrs, entries);
	return(0);
}

/*
 * Handle the "all" role assignment, i.e., those where a roleset is
 * defined over the function "all".
 * This is tricky.
 * At this level, we just go through all possible functions (queries,
 * updates, deletes, and inserts, all named but for the latter) and look
 * through their rolemaps.
 */
static size_t
resolve_roleset_cover(struct strct *p, struct config *cfg)
{
	struct roleset  *rs;
	struct update	*u;
	struct search	*s;
	size_t		 i = 0;

	assert(NULL != p->arolemap);

	TAILQ_FOREACH(rs, &p->arolemap->setq, entries) {
		if (NULL == rs->role)
			continue;
		TAILQ_FOREACH(u, &p->dq, entries) 
			i += resolve_roleset_coverset
				(rs, &u->rolemap,
				 ROLEMAP_DELETE, u->name, p);
		TAILQ_FOREACH(u, &p->uq, entries) 
			i += resolve_roleset_coverset
				(rs, &u->rolemap,
				 ROLEMAP_UPDATE, u->name, p);
		TAILQ_FOREACH(s, &p->sq, entries)
			if (STYPE_ITERATE == s->type)
				i += resolve_roleset_coverset
					(rs, &s->rolemap, 
					 ROLEMAP_ITERATE, s->name, p);
			else if (STYPE_LIST == s->type)
				i += resolve_roleset_coverset
					(rs, &s->rolemap, 
					 ROLEMAP_LIST, s->name, p);
			else if (STYPE_SEARCH == s->type)
				i += resolve_roleset_coverset
					(rs, &s->rolemap, 
					 ROLEMAP_SEARCH, s->name, p);
		i += resolve_roleset_coverset
			(rs, &p->irolemap, ROLEMAP_INSERT, NULL, p);
	}
	
	return(i);
}

/*
 * Given the rolemap "rm", which we know to have a unique name and type
 * in "p", assign to the correspending function type in "p".
 * Return zero on failure to lookup, non-zero on success.
 * On success, the rolemap is assigned into the structure.
 */
static int
resolve_rolemap(struct rolemap *rm, struct strct *p)
{
	struct update	*u;
	struct search	*s;
	struct field	*f;

	switch (rm->type) {
	case ROLEMAP_DELETE:
		TAILQ_FOREACH(u, &p->dq, entries) 
			if (NULL != u->name &&
			    0 == strcasecmp(u->name, rm->name)) {
				assert(NULL == u->rolemap);
				u->rolemap = rm;
				return(1);
			}
		break;
	case ROLEMAP_ALL:
		assert(NULL == p->arolemap);
		p->arolemap = rm;
		return(1);
	case ROLEMAP_INSERT:
		assert(NULL == p->irolemap);
		p->irolemap = rm;
		return(1);
	case ROLEMAP_ITERATE:
		TAILQ_FOREACH(s, &p->sq, entries) 
			if (STYPE_ITERATE == s->type &&
		  	    NULL != s->name &&
			    0 == strcasecmp(s->name, rm->name)) {
				assert(NULL == s->rolemap);
				s->rolemap = rm;
				return(1);
			}
		break;
	case ROLEMAP_LIST:
		TAILQ_FOREACH(s, &p->sq, entries) 
			if (STYPE_LIST == s->type &&
		  	    NULL != s->name &&
			    0 == strcasecmp(s->name, rm->name)) {
				assert(NULL == s->rolemap);
				s->rolemap = rm;
				return(1);
			}
		break;
	case ROLEMAP_SEARCH:
		TAILQ_FOREACH(s, &p->sq, entries)
			if (STYPE_SEARCH == s->type &&
			    NULL != s->name &&
			    0 == strcasecmp(s->name, rm->name)) {
				assert(NULL == s->rolemap);
				s->rolemap = rm;
				return(1);
			}
		break;
	case ROLEMAP_NOEXPORT:
		if (NULL == rm->name) {
			TAILQ_FOREACH(f, &p->fq, entries) {
				if (NULL == f->rolemap) {
					f->rolemap = rm;
					continue;
				}
				gen_errx(&rm->pos, "role already "
					"assigned to field");
				return(0);
			}
			return(1);
		}
		TAILQ_FOREACH(f, &p->fq, entries) 
			if (0 == strcasecmp(f->name, rm->name)) {
				assert(NULL == f->rolemap);
				f->rolemap = rm;
				return(1);
			}
		break;
	case ROLEMAP_UPDATE:
		TAILQ_FOREACH(u, &p->uq, entries) 
			if (NULL != u->name &&
			    0 == strcasecmp(u->name, rm->name)) {
				assert(NULL == u->rolemap);
				u->rolemap = rm;
				return(1);
			}
		break;
	default:
		abort();
	}

	gen_errx(&rm->pos, "corresponding %s %s not found%s%s",
		rolemapts[rm->type],
		ROLEMAP_NOEXPORT == rm->type ? "field" : "function",
		ROLEMAP_INSERT == rm->type ? "" : ": ",
		ROLEMAP_INSERT == rm->type ? "" : rm->name);
	return(0);
}

/*
 * Recursive check to see whether a given structure "p" directly or
 * indirectly contains any structs that have null foreign key
 * references.
 * FIXME: this can be made much more efficient by having the bit be set
 * during the query itself.
 */
static int
check_reffind(const struct strct *p)
{
	const struct field *f;

	if (STRCT_HAS_NULLREFS & p->flags)
		return(1);

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type &&
		    FIELD_NULL & f->ref->source->flags)
			return(1);
		if (FTYPE_STRUCT == f->type &&
		    check_reffind(f->ref->target->parent))
			return(1);
	}

	return(0);
}

static void
resolve_enum_auto(struct enm *en)
{
	struct eitem	*ei;
	int64_t		 v = INT64_MIN;

	TAILQ_FOREACH(ei, &en->eq, entries)
		if ( ! (EITEM_AUTO & ei->flags))
			if (ei->value > v)
				v = ei->value;

	v = v < 0 ? 0 : v + 1;

	TAILQ_FOREACH(ei, &en->eq, entries)
		if (EITEM_AUTO & ei->flags)
			ei->value = v++;
}

int
parse_link(struct config *cfg)
{
	struct update	 *u;
	struct strct	 *p;
	struct strct	**pa;
	struct field	 *f;
	struct unique	 *n;
	struct rolemap	 *rm;
	struct search	 *srch;
	struct enm	 *en;
	size_t		  colour = 1, sz = 0, i, hasrowid = 0;

	TAILQ_FOREACH(en, &cfg->eq, entries)
		if (ENM_AUTO & en->flags)
			resolve_enum_auto(en);

	/* Check for row identifier validity. */

	TAILQ_FOREACH(p, &cfg->sq, entries) {
		hasrowid = 0;
		TAILQ_FOREACH(f, &p->fq, entries)
			if (FIELD_ROWID & f->flags &&
			    ! checkrowid(f, hasrowid++))
				return(0);
	}

	/* 
	 * Check rolemap function name and role name linkage.
	 * Also make sure that any given rolemap doesn't refer to nested
	 * roles (e.g., a role and one if its ancestors).
	 */

	i = 0;
	TAILQ_FOREACH(p, &cfg->sq, entries) {
		TAILQ_FOREACH(rm, &p->rq, entries)
			if ( ! resolve_rolemap(rm, p))
				i++;
			else
				i += resolve_roleset(rm, cfg);
		if (NULL != p->irolemap)
			i += resolve_roleset(p->irolemap, cfg);
		if (NULL != p->arolemap)
			i += resolve_roleset_cover(p, cfg);
	}
	if (i > 0)
		return(0);

	/* 
	 * Some role warnings.
	 * Look through all of our function classes and make sure that
	 * we've defined role assignments for each one.
	 * Otherwise, they're unreachable.
	 */

	if (CFG_HAS_ROLES & cfg->flags)
		TAILQ_FOREACH(p, &cfg->sq, entries) {
			TAILQ_FOREACH(srch, &p->sq, entries) {
				if (srch->rolemap)
					continue;
				gen_warnx(&srch->pos,
					"no roles defined for "
					"query function");
			}
			TAILQ_FOREACH(u, &p->dq, entries) {
				if (u->rolemap)
					continue;
				gen_warnx(&u->pos,
					"no roles defined for "
					"delete function");
			}
			TAILQ_FOREACH(u, &p->uq, entries) {
				if (u->rolemap)
					continue;
				gen_warnx(&u->pos,
					"no roles defined for "
					"update function");
			}
			if (NULL == p->irolemap &&
			    STRCT_HAS_INSERT & p->flags)
				gen_warnx(&p->pos,
					"no roles defined for "
					"insert function");
		}

	/* 
	 * Establish linkage between nodes.
	 * While here, check for duplicate rowids.
	 */

	TAILQ_FOREACH(p, &cfg->sq, entries) {
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (NULL != f->ref &&
			    (! resolve_field_source(f) ||
			     ! resolve_field_target(f, &cfg->sq)))
				return(0);
			if (NULL != f->eref &&
			    ! resolve_field_enum(f->eref, &cfg->eq))
				return(0);
			if (NULL != f->bref &&
			    ! resolve_field_bitfield(f->bref, &cfg->bq))
				return(0);
		}
		TAILQ_FOREACH(u, &p->uq, entries)
			if ( ! resolve_update(u) ||
			     ! check_updatetype(u))
				return(0);
		TAILQ_FOREACH(u, &p->dq, entries)
			if ( ! resolve_update(u) ||
			     ! check_updatetype(u))
				return(0);
	}

	/* Check for reference recursion. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(f, &p->fq, entries)
			if (FTYPE_STRUCT == f->type) {
				if (check_recursive(f->ref, p))
					continue;
				gen_errx(&f->pos, "recursive ref");
				return(0);
			}

	/* 
	 * Now follow and order all outbound links for structs.
	 * From the get-go, we don't descend into structures that we've
	 * already coloured.
	 * This establishes a "height" that we'll use when ordering our
	 * structures in the header file.
	 */

	TAILQ_FOREACH(p, &cfg->sq, entries) {
		sz++;
		if (p->colour)
			continue;
		TAILQ_FOREACH(f, &p->fq, entries)
			if (FTYPE_STRUCT == f->type) {
				p->colour = colour;
				annotate(f->ref, 1, colour);
			}
		colour++;
	}
	assert(sz > 0);

	/*
	 * Next, create unique names for all joins within a structure.
	 * We do this by creating a list of all search patterns (e.g.,
	 * user.name and user.company.name, which assumes two structures
	 * "user" and "company", the first pointing into the second,
	 * both of which contain "name").
	 */

	i = 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		resolve_aliases(p, p, &i, NULL);

	/* Resolve search terms. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(srch, &p->sq, entries)
			if ( ! resolve_search(srch))
				return(0);

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(n, &p->nq, entries)
			if ( ! resolve_unique(n) ||
			     ! check_unique(n))
				return(0);

	/* See if our search type is wonky. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		if ( ! check_searchtype(p))
			return(0);

	/* 
	 * Copy the list into a temporary array.
	 * Then sort the list by reverse-height.
	 * Finally, re-create the list from the sorted elements.
	 */

	if (NULL == (pa = calloc(sz, sizeof(struct strct *))))
		err(EXIT_FAILURE, NULL);

	i = 0;
	while (NULL != (p = TAILQ_FIRST(&cfg->sq))) {
		TAILQ_REMOVE(&cfg->sq, p, entries);
		assert(i < sz);
		pa[i++] = p;
	}
	assert(i == sz);
	qsort(pa, sz, sizeof(struct strct *), parse_cmp);
	for (i = 0; i < sz; i++)
		TAILQ_INSERT_HEAD(&cfg->sq, pa[i], entries);

	/* Check for null struct references. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (check_reffind(p))
			p->flags |= STRCT_HAS_NULLREFS;

	free(pa);
	return(1);
}
