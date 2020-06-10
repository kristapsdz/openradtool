/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "linker.h"

/*
 * Map all "parent.child" chains of foreign references from a given
 * structure (recursively) into alias names.
 * This is used when creating SQL queries because we might join on the
 * same structure more than once, so it requires "AS" statements.
 * The "AS" name is the alias name.
 * FIXME: limited to 26*26*26 entries.
 * Return zero on fatal error, non-zero on success.
 */
static int
linker_aliases_create(struct config *cfg, struct strct *orig, 
	struct strct *p, size_t *offs, const struct alias *prior)
{
	struct field	*f;
	struct alias	*a;
	int		 c;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT)
			continue;
		assert(f->ref != NULL);
		
		if ((a = calloc(1, sizeof(struct alias))) == NULL) {
			gen_err(cfg, &f->pos);
			return 0;
		}
		TAILQ_INSERT_TAIL(&orig->aq, a, entries);

		if (prior != NULL) {
			c = asprintf(&a->name, "%s.%s",
				prior->name, f->name);
			if (c == -1) {
				gen_err(cfg, &f->pos);
				return 0;
			}
		} else if ((a->name = strdup(f->name)) == NULL) {
			gen_err(cfg, &f->pos);
			return 0;
		}

		if (*offs >= 26 * 26 * 26) {
			gen_errx(cfg, &f->pos, "too many aliases");
			return 0;
		}
		
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

		if (c == -1) {
			gen_err(cfg, &f->pos);
			return 0;
		}

		(*offs)++;

		if (!linker_aliases_create
		    (cfg, orig, f->ref->target->parent, offs, a))
			return 0;
	}

	return 1;
}

/*
 * Let linker_aliases_resolve() find unique entries that use the "unique"
 * clause for multiple fields instead of a "unique" or "rowid" on the
 * field itself.
 * All of the search terms ("sent") must be for equality, otherwise the
 * uniqueness is irrelevant.
 * Returns zero if not found, non-zero if found.
 */
static int
check_search_unique(struct config *cfg, const struct search *srch)
{
	const struct unique *uq;
	const struct sent *sent;
	const struct nref *nr;

	TAILQ_FOREACH(uq, &srch->parent->nq, entries) {
		TAILQ_FOREACH(nr, &uq->nq, entries) {
			assert(NULL != nr->field);
			TAILQ_FOREACH(sent, &srch->sntq, entries) {
				if (OPTYPE_EQUAL != sent->op) 
					continue;
				if (sent->field == nr->field)
					break;
			}
			if (NULL == sent)
				break;
		}
		if (NULL == nr)
			return 1;
	}

	return 0;
}

/*
 * Resolve search terms.
 * To do so, descend into each set of search terms for the structure and
 * resolve the fields.
 * Also set whether we have row identifiers within the search expansion.
 * Return zero on failure, non-zero on success.
 */
static int
linker_aliases_resolve(struct config *cfg, struct search *srch)
{
	struct sent	*s;
	struct alias	*a;
	struct strct	*p = srch->parent;
	struct ord	*o;

	/*
	 * Check if this is a unique search result that will reduce our
	 * search to a singleton result always.
	 * This happens if the field value itself is unique (i.e., rowid
	 * or unique) AND the check is for equality.
	 */

	TAILQ_FOREACH(s, &srch->sntq, entries)
		if (((s->field->flags & FIELD_ROWID) ||
		     (s->field->flags & FIELD_UNIQUE)) &&
		     s->op == OPTYPE_EQUAL) {
			s->flags |= SENT_IS_UNIQUE; /* TODO: remove */
			srch->flags |= SEARCH_IS_UNIQUE;
		}


	/* 
	 * If we're not unique on a per-field basis, see if our "unique"
	 * clause stipulates a unique search.
	 */

	if (!(srch->flags & SEARCH_IS_UNIQUE) &&
	    check_search_unique(cfg, srch))
		srch->flags |= SEARCH_IS_UNIQUE;

	/* Resolve search alias. */

	TAILQ_FOREACH(s, &srch->sntq, entries)
		if (s->name != NULL) {
			TAILQ_FOREACH(a, &p->aq, entries)
				if (strcasecmp(a->name, s->name) == 0)
					break;
			assert(a != NULL);
			s->alias = a;
		}

	/* Resolve order alias. */

	TAILQ_FOREACH(o, &srch->ordq, entries)
		if (o->name != NULL) {
			TAILQ_FOREACH(a, &p->aq, entries)
				if (strcasecmp(a->name, o->name) == 0)
					break;
			assert(a != NULL);
			o->alias = a;
		}

	/* 
	 * Only resolve the group if we have an aggregator.
	 * The group identifier cannot be NULL because that would ruin
	 * the post-join filter of NULL fields.
	 */

	if (NULL != srch->group) {
		if (NULL == srch->aggr) {
			gen_errx(cfg, &srch->group->pos, 
				"group without a constraint");
			return 0;
		}
		if (NULL != srch->group->name) {
			TAILQ_FOREACH(a, &p->aq, entries)
				if (0 == strcasecmp
				    (a->name, srch->group->name))
					break;
			assert(NULL != a);
			srch->group->alias = a;
		}
		if (FIELD_NULL & srch->group->field->flags) {
			gen_errx(cfg, &srch->group->pos,
				"group cannot be null");
			return 0;
		}
	}

	/*
	 * For the aggregate function, we also want to make sure that we
	 * haven't defined the same columns to aggregate as the ones we
	 * want to group.
	 * It doesn't make a lot of sense to have a NULL field but it
	 * also doesn't break anything, so it's ok.
	 */

	if (srch->aggr != NULL) {
		if (srch->aggr->field == srch->group->field) {
			gen_errx(cfg, &srch->group->pos, "same "
				"column for group and constraint");
			return 0;
		}
		if (srch->aggr->field->parent != 
		    srch->group->field->parent) {
			gen_errx(cfg, &srch->group->pos, 
				"structure for group and constraint "
				"must be the same");
			return 0;
		}
		if (NULL != srch->aggr->name) {
			TAILQ_FOREACH(a, &p->aq, entries)
				if (0 == strcasecmp
				    (a->name, srch->aggr->name))
					break;
			assert(NULL != a);
			srch->aggr->alias = a;
		}
	}

	return 1;
}

int
linker_aliases(struct config *cfg)
{
	struct strct	*p;
	size_t		 count = 0;
	struct search	*srch;

	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!linker_aliases_create(cfg, p, p, &count, NULL))
			return 0;

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(srch, &p->sq, entries)
			if (!linker_aliases_resolve(cfg, srch))
				return 0;

	return 1;
}
