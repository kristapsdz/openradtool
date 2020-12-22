/*	$Id$ */
/*
 * Copyright (c) 2017--2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "linker.h"

/*
 * Generate a warning message at position "pos".
 */
void
gen_warnx(struct config *cfg, 
	const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		ort_msgv(cfg, MSGTYPE_WARN, 0, pos, fmt, ap);
		va_end(ap);
	} else
		ort_msg(cfg, MSGTYPE_WARN, 0, pos, NULL);
}

/*
 * Generate a fatal error at position "pos" with only an errno.
 * This is exclusively used for allocation failures.
 * The caller is still responsible for exiting.
 */
void
gen_err(struct config *cfg, const struct pos *pos)
{
	int	 er = errno;

	ort_msg(cfg, MSGTYPE_FATAL, er, pos, NULL);
}

/*
 * Generate a fatal error at position "pos" without an errno.
 * The caller is still responsible for exiting.
 */
void
gen_errx(struct config *cfg, 
	const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		ort_msgv(cfg, MSGTYPE_ERROR, 
			0, pos, fmt, ap);
		va_end(ap);
	} else
		ort_msg(cfg, MSGTYPE_ERROR, 0, pos, NULL);
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
 * Sort by reverse height.
 */
static int
parse_cmp(const void *a1, const void *a2)
{
	const struct strct 
	      *p1 = *(const struct strct **)a1, 
	      *p2 = *(const struct strct **)a2;

	return (ssize_t)p1->height - (ssize_t)p2->height;
}

/*
 * Make sure that we have a well-defined grouprow and aggregation
 * function.
 * Returns zero on failure, non-zero on success.
 */
static int
check_aggrtype(struct config *cfg, const struct search *srch)
{
	size_t	errs = 0;

	if (srch->group != NULL) {
		if (srch->aggr == NULL) {
			gen_errx(cfg, &srch->group->pos, 
				"group without a constraint");
			errs++;
		}
	}

	if (srch->aggr != NULL && srch->group != NULL) {
		if (srch->aggr->field == srch->group->field) {
			gen_errx(cfg, &srch->group->pos, "same "
				"column for group and constraint");
			errs++;
		}
		if (srch->aggr->field->parent != 
		    srch->group->field->parent) {
			gen_errx(cfg, &srch->group->pos, 
				"structure for group and constraint "
				"must be the same");
			errs++;
		}
	}

	return errs == 0;
}

/*
 * Check to see that our query type consistent with the fields that
 * we're searching on.
 * Return zero on failure, non-zero on success.
 */
static int
check_searchtype(struct config *cfg, const struct search *srch)
{
	const struct sent	*sent;
	size_t			 errs = 0;

	/* XXX: there should be facility to do this. */

	if (srch->type == STYPE_SEARCH && TAILQ_EMPTY(&srch->sntq)) {
		gen_errx(cfg, &srch->pos, 
			"unique result search without parameters");
		errs++;
	}

	/*
	 * XXX: we use SQL's "count" function for this, so we can't
	 * currently use any of the password equality checks.
	 */

	if (srch->type == STYPE_COUNT)
		TAILQ_FOREACH(sent, &srch->sntq, entries)
			if (!OPTYPE_ISUNARY(sent->op) &&
			    sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ &&
			    sent->field->type == FTYPE_PASSWORD) {
				gen_errx(cfg, &sent->pos, "passwords "
					"for count only accept unary "
					"and string operators");
				errs++;
			}
	
	/*
	 * Start by checking that singleton returns don't occur on
	 * multiple searches and vice versa.
	 */

	if (srch->type != STYPE_SEARCH &&
	    (srch->flags & SEARCH_IS_UNIQUE))
		gen_warnx(cfg, &srch->pos, 
			"multiple-result search on a unique field");
	if (!(srch->flags & SEARCH_IS_UNIQUE) && 
	    srch->type == STYPE_SEARCH && srch->limit != 1)
		gen_warnx(cfg, &srch->pos, 
			"single-result search on a non-unique field "
			"without a limit of one");

	/* Check that unary operations act on possibly-null. */

	TAILQ_FOREACH(sent, &srch->sntq, entries)
		if ((sent->op == OPTYPE_NOTNULL ||
		     sent->op == OPTYPE_ISNULL) &&
		    !(sent->field->flags & FIELD_NULL))
			gen_warnx(cfg, &sent->pos, 
				"null operator on non-null field");

	/* Logical AND/OR should only occur with bitfields. */

	TAILQ_FOREACH(sent, &srch->sntq, entries)
		if ((sent->op == OPTYPE_AND ||
		     sent->op == OPTYPE_OR) &&
		    (sent->field->type != FTYPE_BIT &&
		     sent->field->type != FTYPE_BITFIELD &&
		     sent->field->type != FTYPE_INT)) {
			gen_errx(cfg, &sent->pos, "bit operations "
				"only available on bit, bitfield, "
				"and integer types");
			errs++;
		}

	/* Passwords can't use inequalities. */

	TAILQ_FOREACH(sent, &srch->sntq, entries)
		if (!OPTYPE_ISUNARY(sent->op) &&
		    sent->op != OPTYPE_EQUAL &&
		    sent->op != OPTYPE_NEQUAL &&
		    sent->op != OPTYPE_STREQ &&
		    sent->op != OPTYPE_STRNEQ &&
		    sent->field->type == FTYPE_PASSWORD) {
			gen_errx(cfg, &sent->pos, "passwords only "
				"accept unary or equality operators");
			errs++;
		}

	/* Require text types for LIKE operator. */

	TAILQ_FOREACH(sent, &srch->sntq, entries)
		if (sent->op == OPTYPE_LIKE &&
		    sent->field->type != FTYPE_TEXT &&
		    sent->field->type != FTYPE_EMAIL) {
			gen_errx(cfg, &sent->pos, "LIKE "
				"operator on non-textual field.");
			errs++;
		}

	/* 
	 * Disallow passwords in distinct.
	 * TODO: this should allow for passwords *within* the
	 * distinct subparts.
	 */

	if (srch->dst != NULL)
		TAILQ_FOREACH(sent, &srch->sntq, entries) {
			if (OPTYPE_ISUNARY(sent->op) ||
			    sent->op == OPTYPE_STREQ ||
			    sent->op == OPTYPE_STRNEQ ||
			    sent->field->type != FTYPE_PASSWORD) 
				continue;
			gen_errx(cfg, &sent->pos, 
				"password queries not allowed when "
				"searching on distinct subsets");
			errs++;
		}

	return errs == 0;
}


/*
 * Given the rolemap "rm", make sure that the matched roles, if any,
 * don't overlap in the tree of roles.
 * Return non-zero on success, zero on failure.
 */
static ssize_t
check_unique_roles_tree(struct config *cfg, const struct rolemap *rm)
{
	const struct rref	*rs, *rrs;
	const struct role	*rp;
	size_t		 	 i = 0;

	/*
	 * Don't use top-level roles, since by definition they don't
	 * overlap with anything else.
	 */

	TAILQ_FOREACH(rs, &rm->rq, entries)
		TAILQ_FOREACH(rrs, &rm->rq, entries) {
			if (rrs == rs || (rp = rrs->role) == NULL)
				continue;
			assert(rp != rs->role);
			for ( ; rp != NULL; rp = rp->parent)
				if (rp == rs->role)
					break;
			if (rp == NULL)
				continue;
			gen_errx(cfg, &rs->pos, 
				"overlapping role: %s, %s", 
				rrs->role->name, rs->role->name);
			i++;
		}

	return i == 0;
}

/*
 * Recursive check to see whether a given structure "p" directly or
 * indirectly contains any structs that have null foreign key
 * references.
 * FIXME: this can be made much more efficient by having the bit be set
 * during the query itself.
 * Return zero on failure, non-zero on success.
 */
static int
check_reffind(struct config *cfg, const struct strct *p)
{
	const struct field *f;

	if (STRCT_HAS_NULLREFS & p->flags)
		return 1;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type &&
		    FIELD_NULL & f->ref->source->flags)
			return 1;
		if (FTYPE_STRUCT == f->type &&
		    check_reffind(cfg, f->ref->target->parent))
			return 1;
	}

	return 0;
}

/*
 * Make sure that the unique statements noted in "u" are not equal
 * to any other unique statements.
 * Returns zero on failure, non-zero on success.
 */
static int
check_unique_unique(struct config *cfg, const struct strct *s)
{
	const struct unique	*u, *uu;
	const struct nref	*nf, *unf;
	size_t			 errs = 0, sz, usz;

	/*
	 * "Duplicate" means same number of fields, same fields by
	 * pointer equality.
	 */

	TAILQ_FOREACH(u, &s->nq, entries) {
		sz = 0;
		TAILQ_FOREACH(nf, &u->nq, entries)
			sz++;
		TAILQ_FOREACH(uu, &s->nq, entries) {
			if (uu == u)
				continue;
			usz = 0;
			TAILQ_FOREACH(unf, &uu->nq, entries) 
				usz++;
			if (usz != sz)
				continue;
			TAILQ_FOREACH(nf, &u->nq, entries) {
				TAILQ_FOREACH(unf, &uu->nq, entries)
					if (nf->field == unf->field)
						break;
				if (unf == NULL)
					break;
			}
			if (nf != NULL)
				break;
			gen_errx(cfg, &u->pos, "duplicate "
				"unique statements: %s:%zu:%zu",
				uu->pos.fname, uu->pos.line,
				uu->pos.column);
			errs++;
		}
	}

	return errs == 0;
}

/*
 * Make sure that the rolemap contains unique roles.
 * Returns zero on failure (duplicate roles), non-zero otherwise.
 */
static int
check_unique_roles(struct config *cfg, const struct rolemap *rm)
{
	const struct rref	*rs, *rrs;
	size_t			 errs = 0;

	TAILQ_FOREACH(rs, &rm->rq, entries)
		TAILQ_FOREACH(rrs, &rm->rq, entries)
			if (rs != rrs && rs->role == rrs->role) {
				gen_errx(cfg, &rrs->role->pos,
					"duplicate operation role");
				errs++;
			}

	return errs == 0;
}

/* 
 * See whether operations are defined in a role.
 * These aren't errors, but should be warned about.
 */
static void
check_roleops(struct config *cfg, const struct strct *p)
{
	const struct search	*srch;
	const struct update	*u;

	TAILQ_FOREACH(srch, &p->sq, entries)
		if (srch->rolemap == NULL)
			gen_warnx(cfg, &srch->pos, "role "
				"not assigned to query function");
	TAILQ_FOREACH(u, &p->dq, entries)
		if (u->rolemap == NULL)
			gen_warnx(cfg, &u->pos, "role "
				"not assigned to delete function");
	TAILQ_FOREACH(u, &p->uq, entries) 
		if (u->rolemap == NULL)
			gen_warnx(cfg, &u->pos, "role "
				"not assigned to update function");
	if (p->ins != NULL && p->ins->rolemap == NULL)
		gen_warnx(cfg, &p->ins->pos, 
			"role not assigned to insert function");
}

/*
 * This is the "linking" phase where a fully-parsed configuration file
 * has its components linked together and checked for correctness.
 * It MUST be called following ort_parse_file() or the configuration is
 * not complete.
 * Returns zero on failure, non-zero on success.
 */
int
ort_parse_close(struct config *cfg)
{
	struct strct	 *p;
	struct strct	**pa;
	struct field	 *f;
	struct rolemap	 *rm;
	struct search	 *srch;
	size_t		  colour = 1, sz = 0, i = 0;

	if (TAILQ_EMPTY(&cfg->sq)) {
		gen_errx(cfg, NULL, "no structures in configuration");
		return 0;
	}

	if (!linker_resolve(cfg))
		return 0;
	if (!linker_aliases(cfg))
		return 0;

	/*
	 * At this point we know (1) that all names link to objects, (2)
	 * that all queries have aliases created (this is for the SQL),
	 * and (3) that the configuration has non-empty structures that
	 * are not recursive.
	 */

	/* Check for unique statement duplicates. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		i += !check_unique_unique(cfg, p);
	if (i > 0)
		return 0;

	/* Check that each rolemap has no duplicate roles. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(rm, &p->rq, entries)
			i += !check_unique_roles(cfg, rm);
	if (i > 0)
		return 0;

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(rm, &p->rq, entries)
			i += !check_unique_roles_tree(cfg, rm);
	if (i > 0)
		return 0;

	/* See whether operations are defined in a role. */

	if (!TAILQ_EMPTY(&cfg->rq))
		TAILQ_FOREACH(p, &cfg->sq, entries)
			check_roleops(cfg, p);

	/* Check type for grouprow and aggregate function. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(srch, &p->sq, entries)
			i += !check_aggrtype(cfg, srch);
	if (i > 0)
		return 0;

	/* See if our search types are wonky. */

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(srch, &p->sq, entries)
			i += !check_searchtype(cfg, srch);
	if (i > 0)
		return 0;

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
	 * Copy the list into a temporary array.
	 * Then sort the list by reverse-height.
	 * Finally, re-create the list from the sorted elements.
	 */

	if (NULL == (pa = calloc(sz, sizeof(struct strct *)))) {
		gen_err(cfg, NULL);
		return 0;
	}

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
		if (check_reffind(cfg, p))
			p->flags |= STRCT_HAS_NULLREFS;

	free(pa);
	return 1;
}
