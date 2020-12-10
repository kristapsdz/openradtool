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
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "parser.h"

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYPE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYPE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	"streq", /* OPTYPE_STREQ */
	"strneq", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"isnull", /* OPTYPE_ISNULL */
	"notnull", /* OPTYPE_NOTNULL */
};

static	const char *const rolemapts[ROLEMAP__MAX] = {
	"all", /* ROLEMAP_ALL */
	"count", /* ROLEMAP_COUNT */
	"delete", /* ROLEMAP_DELETE */
	"insert", /* ROLEMAP_INSERT */
	"iterate", /* ROLEMAP_ITERATE */
	"list", /* ROLEMAP_LIST */
	"search", /* ROLEMAP_SEARCH */
	"update", /* ROLEMAP_UPDATE */
	"noexport", /* ROLEMAP_NOEXPORT */
};

static	const char *const modtypes[MODTYPE__MAX] = {
	"concat", /* MODTYPE_CONCAT */
	"dec", /* MODTYPE_DEC */
	"inc", /* MODTYPE_INC */
	"set", /* MODTYPE_SET */
	"strset", /* MODTYPE_STRSET */
};

/*
 * Allocate and initialise a struct named "name", returning the new
 * structure or NULL on memory allocation failure or bad name.
 */
static struct strct *
strct_alloc(struct parse *p, const char *name)
{
	struct strct	*s;

	/* Check reserved identifiers and dupe names. */

	if (!parse_check_badidents(p, name)) {
		parse_errx(p, "reserved identifier");
		return NULL;
	} else if (!parse_check_dupetoplevel(p, name))
		return NULL;

	if ((s = calloc(1, sizeof(struct strct))) == NULL ||
	    (s->name = strdup(name)) == NULL) {
		parse_err(p);
		if (NULL != s) {
			free(s->name);
			free(s);
		}
		return NULL;
	}

	s->cfg = p->cfg;
	parse_point(p, &s->pos);
	TAILQ_INSERT_TAIL(&p->cfg->sq, s, entries);
	TAILQ_INIT(&s->fq);
	TAILQ_INIT(&s->sq);
	TAILQ_INIT(&s->aq);
	TAILQ_INIT(&s->uq);
	TAILQ_INIT(&s->nq);
	TAILQ_INIT(&s->dq);
	TAILQ_INIT(&s->rq);
	return s;
}

static int
name_append(char ***p, size_t *sz, const char *v)
{
	void	*pp;

	if (*p == NULL) {
		if ((*p = calloc(1, sizeof(char *))) == NULL)
			return 0;
		*sz = 1;
		if (((*p)[0] = strdup(v)) == NULL)
			return 0;
	} else {
		pp = reallocarray(*p, *sz + 1, sizeof(char *));
		if (pp == NULL)
			return 0;
		*p = pp;
		if (((*p)[*sz] = strdup(v)) == NULL)
			return 0;
		(*sz)++;
	}

	return 1;
}

static int
name_truncate(char **p, const char *v)
{
	char	*cp;

	if (strrchr(v, '.') != NULL) {
		if ((*p = strdup(v)) == NULL)
			return 0;
		cp = strrchr(*p, '.');
		assert(cp != NULL);
		*cp = '\0';
	} else
		*p = NULL;

	return 1;
}

static int
ref_append2(char **p, const char *v, char delim)
{
	size_t		 sz, cursz;
	void		*pp;
	const char	*src;
	char		*dst;

	assert(v != NULL);
	sz = strlen(v);
	assert(sz > 0);

	if (*p == NULL) {
		if ((*p = malloc(sz + 1)) == NULL) 
			return 0;
		dst = *p;
	} else {
		cursz = strlen(*p);
		if ((pp = realloc(*p, cursz + sz + 2)) == NULL)
			return 0;
		*p = pp;
		(*p)[cursz] = delim;
		dst = *p + cursz + 1;
	}

	if (delim == '_')
		for (src = v; *src != '\0'; dst++, src++) 
			*dst = tolower((unsigned char)*src);
	else
		for (src = v; *src != '\0'; dst++, src++) 
			*dst = *src;

	*dst = '\0';
	return 1;
}

/*
 * Allocate uref and register resolve request.
 * If "mod" is non-zero, it means this is a modifier; otherwise, this is
 * a constraint uref.
 * Returns the created pointer or NULL.
 */
static struct uref *
uref_alloc(struct parse *p, struct update *up, int mod)
{
	struct uref	*ref;
	struct resolve	*r;

	if ((ref = calloc(1, sizeof(struct uref))) == NULL) {
		parse_err(p);
		return NULL;
	}
	ref->parent = up;
	ref->op = OPTYPE_EQUAL;
	ref->mod = MODTYPE_SET;
	parse_point(p, &ref->pos);
	TAILQ_INSERT_TAIL(mod ? &up->mrq : &up->crq, ref, entries);

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return NULL;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);

	if (mod) {
		r->type = RESOLVE_UP_MODIFIER;
		r->struct_up_mod.name = strdup(p->last.string);
		r->struct_up_mod.result = ref;
		if (r->struct_up_mod.name == NULL) {
			parse_err(p);
			return NULL;
		}
	} else {
		r->type = RESOLVE_UP_CONSTRAINT;
		r->struct_up_const.name = strdup(p->last.string);
		r->struct_up_const.result = ref;
		if (r->struct_up_const.name == NULL) {
			parse_err(p);
			return NULL;
		}
	}
	return ref;
}

/*
 * Allocate a search entity and add it to the parent queue.
 * Returns the created pointer or NULL.
 */
static struct sent *
sent_alloc(struct parse *p, struct search *up)
{
	struct sent	*sent;

	if (NULL == (sent = calloc(1, sizeof(struct sent)))) {
		parse_err(p);
		return NULL;
	}

	sent->op = OPTYPE_EQUAL;
	sent->parent = up;
	parse_point(p, &sent->pos);
	TAILQ_INSERT_TAIL(&up->sntq, sent, entries);
	return sent;
}

/*
 * Like parse_config_search_terms() but for distinction terms.
 * If just a period, the distinction is for the whole result set.
 * Otherwise, it's for a specific field we'll look up later.
 *
 *  "." | field["." field]*
 */
static void
parse_config_distinct_term(struct parse *p, struct search *srch)
{
	struct resolve	*r;
	struct dstnct	*d;

	if (srch->dst != NULL) {
		parse_errx(p, "redeclaring distinct");
		return;
	} else if ((d = calloc(1, sizeof(struct dstnct))) == NULL) {
		parse_err(p);
		return;
	}

	srch->dst = d;
	d->parent = srch;
	parse_point(p, &d->pos);

	/* If a period, refers to the current structure. */

	if (p->lasttype == TOK_PERIOD) {
		d->strct = srch->parent;
		parse_next(p);
		return;
	} else if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected distinct field");
		return;
	}

	/* Initialise the resolver and canonical name. */

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_DISTINCT;
	r->struct_distinct.result = d;
	if (!name_append(&r->struct_distinct.names, 
	    &r->struct_distinct.namesz, p->last.string)) {
		parse_err(p);
		return;
	}
	if (!ref_append2(&d->fname, p->last.string, '.')) {
		parse_err(p);
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_SEMICOLON ||
		    p->lasttype == TOK_IDENT)
			break;

		if (p->lasttype != TOK_PERIOD) {
			parse_errx(p, "expected field separator");
			return;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected distinct field");
			return;
		}

		/* Append to resolve and canonical name. */

		if (!name_append(&r->struct_distinct.names, 
		    &r->struct_distinct.namesz, p->last.string)) {
			parse_err(p);
			return;
		}
		if (!ref_append2(&d->fname, p->last.string, '.')) {
			parse_err(p);
			return;
		}
	}
}

/*
 * Like parse_config_search_terms() but for aggregate terms.
 *
 *   field[.field]* 
 */
static void
parse_config_aggr_terms(struct parse *p,
	enum aggrtype type, struct search *srch)
{
	struct resolve	*r;
	struct aggr	*aggr;

	if (srch->aggr != NULL) {
		parse_errx(p, "redeclaring aggregate term");
		return;
	} else if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected aggregate identifier");
		return;
	} else if ((aggr = calloc(1, sizeof(struct aggr))) == NULL) {
		parse_err(p);
		return;
	}

	srch->aggr = aggr;
	aggr->parent = srch;
	aggr->op = type;
	parse_point(p, &aggr->pos);

	/* Initialise the resolver. */

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_AGGR;
	r->struct_aggr.result = aggr;
	if (!name_append(&r->struct_aggr.names, 
	    &r->struct_aggr.namesz, p->last.string)) {
		parse_err(p);
		return;
	}

	/* Initialise canonical name. */

	if (!ref_append2(&aggr->fname, p->last.string, '.')) {
		parse_err(p);
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_SEMICOLON ||
		    p->lasttype == TOK_IDENT)
			break;

		if (p->lasttype != TOK_PERIOD) {
			parse_errx(p, "expected field separator");
			return;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected field identifier");
			return;
		}

		/* Append to resolver and canonical name. */

		if (!name_append(&r->struct_aggr.names, 
		    &r->struct_aggr.namesz, p->last.string)) {
			parse_err(p);
			return;
		}
		if (!ref_append2(&aggr->fname, p->last.string, '.')) {
			parse_err(p);
			return;
		}
	}

	if (!PARSE_STOP(p) &&
	    !name_truncate(&aggr->name, aggr->fname))
		parse_err(p);
}

/*
 * Like parse_config_search_terms() but for grouping terms.
 *
 *  field[.field]*
 */
static void
parse_config_group_terms(struct parse *p, struct search *srch)
{
	struct group	*grp;
	struct resolve	*r;

	if (srch->group != NULL) {
		parse_errx(p, "duplicate grouprow identifier");
		return;
	} else if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected grouprow identifier");
		return;
	} else if ((grp = calloc(1, sizeof(struct group))) == NULL) {
		parse_err(p);
		return;
	}

	srch->group = grp;
	grp->parent = srch;
	parse_point(p, &grp->pos);

	/* Initialise the resolver. */

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_GROUPROW;
	r->struct_grouprow.result = grp;
	if (!name_append(&r->struct_grouprow.names, 
	    &r->struct_grouprow.namesz, p->last.string)) {
		parse_err(p);
		return;
	}

	/* Initialise canonical name. */

	if (!ref_append2(&grp->fname, p->last.string, '.')) {
		parse_err(p);
		return;
	}
	
	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_SEMICOLON ||
		    p->lasttype == TOK_IDENT)
			break;

		if (p->lasttype != TOK_PERIOD) {
			parse_errx(p, "expected field separator");
			return;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected field identifier");
			return;
		}

		/* Append to resolver and canonical name. */

		if (!name_append(&r->struct_grouprow.names, 
		    &r->struct_grouprow.namesz, p->last.string)) {
			parse_err(p);
			return;
		}
		if (!ref_append2(&grp->fname, p->last.string, '.')) {
			parse_err(p);
			return;
		}
	}

	if (!PARSE_STOP(p) &&
	    !name_truncate(&grp->name, grp->fname))
		parse_err(p);
}

/*
 * Like parse_config_search_terms() but for order terms.
 *
 *  field[.field]* ["asc"|"desc"]?
 */
static void
parse_config_order_terms(struct parse *p, struct search *srch)
{
	struct ord	*ord;
	struct resolve	*r;

	if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected order identifier");
		return;
	} else if ((ord = calloc(1, sizeof(struct ord))) == NULL) {
		parse_err(p);
		return;
	}

	ord->parent = srch;
	ord->op = ORDTYPE_ASC;
	parse_point(p, &ord->pos);
	TAILQ_INSERT_TAIL(&srch->ordq, ord, entries);

	/* Initialise the resolver. */

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_ORDER;
	r->struct_order.result = ord;
	if (!name_append(&r->struct_order.names, 
	    &r->struct_order.namesz, p->last.string)) {
		parse_err(p);
		return;
	}

	/* Initialise canonical name. */

	if (!ref_append2(&ord->fname, p->last.string, '.')) {
		parse_err(p);
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_COMMA ||
		    p->lasttype == TOK_SEMICOLON)
			break;

		if (p->lasttype == TOK_IDENT) {
			if (strcasecmp(p->last.string, "asc") == 0)
				ord->op = ORDTYPE_ASC;
			if (strcasecmp(p->last.string, "desc") == 0)
				ord->op = ORDTYPE_DESC;
			if (strcasecmp(p->last.string, "asc") == 0 ||
			    strcasecmp(p->last.string, "desc") == 0)
				parse_next(p);
			break;
		}

		if (p->lasttype != TOK_PERIOD) {
			parse_errx(p, "expected field separator");
			return;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected field identifier");
			return;
		}

		/* Append to resolver and canonical name. */

		if (!name_append(&r->struct_order.names, 
		    &r->struct_order.namesz, p->last.string)) {
			parse_err(p);
			return;
		}
		if (!ref_append2(&ord->fname, p->last.string, '.')) {
			parse_err(p);
			return;
		}
	}

	/* Set "name" to be all but the last component of fname. */

	if (!PARSE_STOP(p) &&
	    !name_truncate(&ord->name, ord->fname)) {
		parse_err(p);
		return;
	} 
}

/*
 * Parse the field used in a search.
 * This is the search_terms designation in parse_config_search().
		}
 * This may consist of nested structures, which uses dot-notation to
 * signify the field within a field's reference structure.
 *
 *  field.[field]* [operator]?
 */
static void
parse_config_search_terms(struct parse *p, struct search *srch)
{
	struct sent	*sent;
	struct resolve	*r;

	if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected field identifier");
		return;
	} else if ((sent = sent_alloc(p, srch)) == NULL)
		return;

	/* Initialise the resolver. */

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}
	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_SENT;
	r->struct_sent.result = sent;
	if (!name_append(&r->struct_sent.names, 
	    &r->struct_sent.namesz, p->last.string)) {
		parse_err(p);
		return;
	}

	/* Initialise the canonical names. */
	
	if (!ref_append2(&sent->fname, p->last.string, '.') ||
	    !ref_append2(&sent->uname, p->last.string, '_')) {
		parse_err(p);
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_COMMA ||
		    p->lasttype == TOK_SEMICOLON ||
		    p->lasttype == TOK_COLON)
			break;

		/* 
		 * Parse the optional operator.
		 * After the operator, we'll have no more fields.
		 */

		if (p->lasttype == TOK_IDENT) {
			for (sent->op = 0; 
			     sent->op != OPTYPE__MAX; sent->op++)
				if (strcasecmp(p->last.string, 
				    optypes[sent->op]) == 0)
					break;
			if (sent->op == OPTYPE__MAX) {
				parse_errx(p, "unknown operator");
				return;
			}
			if (parse_next(p) == TOK_COMMA ||
			    p->lasttype == TOK_SEMICOLON ||
			    p->lasttype == TOK_COLON)
				break;
			parse_errx(p, "expected field separator");
			return;
		}

		/* Parse next field name in chain. */

		if (p->lasttype != TOK_PERIOD) {
			parse_errx(p, "expected field separator");
			return;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected field identifier");
			return;
		}

		/* Append to resolver and canonical name. */

		if (!name_append(&r->struct_sent.names, 
		    &r->struct_sent.namesz, p->last.string)) {
			parse_err(p);
			return;
		}
		if (!ref_append2(&sent->fname, p->last.string, '.') ||
		    !ref_append2(&sent->uname, p->last.string, '_')) {
			parse_err(p);
			return;
		}
	}

	/* Set "name" to be all but the last component of fname. */

	if (!PARSE_STOP(p) &&
	    !name_truncate(&sent->name, sent->fname)) {
		parse_err(p);
		return;
	} 
}

/*
 * Parse the limit/offset parameters, where the first integer is the
 * limit, the second is the offset.
 *
 *   integer [ "," integer ]
 */
static void
parse_config_limit_params(struct parse *p, struct search *s)
{
	if (p->lasttype != TOK_INTEGER) {
		parse_errx(p, "expected limit value");
		return;
	} else if (p->last.integer <= 0) {
		parse_errx(p, "expected limit >0");
		return;
	} else if (s->limit)
		parse_warnx(p, "redeclaring limit");

	s->limit = p->last.integer;
	if (parse_next(p) != TOK_COMMA)
		return;

	if (parse_next(p) != TOK_INTEGER) {
		parse_errx(p, "expected offset value");
		return;
	} else if (p->last.integer <= 0) {
		parse_errx(p, "expected offset >0");
		return;
	} else if (s->offset)
		parse_warnx(p, "redeclaring offset");

	s->offset = p->last.integer;
	parse_next(p);
}

/*
 * Parse the search parameters following the search fields:
 *
 *   [ "name" name |
 *     "comment" quoted_string |
 *     "distinct" distinct_struct |
 *     "minrow"|"maxrow" aggr_fields ]* |
 *     "grouprow" group_fields |
 *     "order" order_fields ]* ";"
 */
static void
parse_config_search_params(struct parse *p, struct search *s)
{
	struct search	*ss;

	if (parse_next(p) == TOK_SEMICOLON)
		return;

	while (!PARSE_STOP(p)) {
		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected query parameter name");
			break;
		}
		if (strcasecmp("name", p->last.string) == 0) {
			if (parse_next(p) != TOK_IDENT) {
				parse_errx(p, "expected query name");
				break;
			}
			TAILQ_FOREACH(ss, &s->parent->sq, entries) {
				if (s->type != ss->type ||
				    ss->name == NULL ||
				    strcasecmp(ss->name, p->last.string))
					continue;
				parse_errx(p, "duplicate query name");
				break;
			}
			if (s->name != NULL)
				parse_warnx(p, "redeclaring name");
			free(s->name);
			s->name = strdup(p->last.string);
			if (s->name == NULL) {
				parse_err(p);
				return;
			}
			parse_next(p);
		} else if (strcasecmp("comment", p->last.string) == 0) {
			if (!parse_comment(p, &s->doc))
				break;
			parse_next(p);
		} else if (strcasecmp("limit", p->last.string) == 0) {
			parse_next(p);
			parse_config_limit_params(p, s);
		} else if (strcasecmp("minrow", p->last.string) == 0) {
			parse_next(p);
			parse_config_aggr_terms(p, AGGR_MINROW, s);
		} else if (strcasecmp("maxrow", p->last.string) == 0) {
			parse_next(p);
			parse_config_aggr_terms(p, AGGR_MAXROW, s);
		} else if (strcasecmp("order", p->last.string) == 0) {
			parse_next(p);
			parse_config_order_terms(p, s);
			while (p->lasttype == TOK_COMMA) {
				parse_next(p);
				parse_config_order_terms(p, s);
			}
		} else if (strcasecmp("grouprow", p->last.string) == 0) {
			parse_next(p);
			parse_config_group_terms(p, s);
		} else if (strcasecmp("distinct", p->last.string) == 0) {
			parse_next(p);
			parse_config_distinct_term(p, s);
		} else {
			parse_errx(p, "unknown search parameter");
			break;
		}
		if (p->lasttype == TOK_SEMICOLON)
			break;
	}

	if (s->group != NULL && s->aggr == NULL)
		parse_errx(p, "group without a constraint");
	if (s->aggr != NULL && s->group == NULL)
		parse_errx(p, "constraint without a group");
}

/*
 * Parse a unique clause.
 * This has the following syntax:
 *
 *  "unique" field ["," field]+ ";"
 *
 * The fields are within the current structure.
 */
static void
parse_struct_unique(struct parse *p, struct strct *s)
{
	struct nref	 *nf;
	struct unique	 *up;
	struct resolve	 *r;
	size_t		  fieldsz = 0;

	if (NULL == (up = calloc(1, sizeof(struct unique)))) {
		parse_err(p);
		return;
	}

	up->parent = s;
	parse_point(p, &up->pos);
	TAILQ_INIT(&up->nq);
	TAILQ_INSERT_TAIL(&s->nq, up, entries);

	while (!PARSE_STOP(p)) {
		if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected unique field");
			return;
		}

		/* Append to resolver. */

		if ((nf = calloc(1, sizeof(struct nref))) == NULL) {
			parse_err(p);
			return;
		}
		TAILQ_INSERT_TAIL(&up->nq, nf, entries);
		parse_point(p, &nf->pos);
		nf->parent = up;

		if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
			parse_err(p);
			return;
		}
		r->type = RESOLVE_UNIQUE;
		TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
		r->struct_unique.result = nf;
		r->struct_unique.name = strdup(p->last.string);
		if (r->struct_unique.name == NULL) {
			parse_err(p);
			return;
		}
		fieldsz++;

		/* Next statement. */

		if (TOK_SEMICOLON == parse_next(p))
			break;
		if (TOK_COMMA != p->lasttype) {
			parse_errx(p, "expected semicolon or comma");
			return;
		}
	}

	if (fieldsz < 2)
		parse_errx(p, "at least two fields "
			"required for unique constraint");
}

/*
 * Parse an update clause.
 * This has the following syntax:
 *
 *  "update" [ ufield [,ufield]* ]?
 *     [ ":" sfield [,sfield]*
 *       [ ":" [ "name" name | "comment" quot | "action" action ]* ]? 
 *     ]? ";"
 *
 * The fields ("ufield" for update field and "sfield" for select field)
 * are within the current structure.
 * The former are only for UPT_MODIFY parses.
 * Note that "sfield" also contains an optional operator, just like in
 * the search parameters.
 */
static void
parse_struct_update(struct parse *p, struct strct *s, enum upt type)
{
	struct update	*up;
	struct uref	*ur;

	if (NULL == (up = calloc(1, sizeof(struct update)))) {
		parse_err(p);
		return;
	}
	up->parent = s;
	up->type = type;
	parse_point(p, &up->pos);
	TAILQ_INIT(&up->mrq);
	TAILQ_INIT(&up->crq);

	if (up->type == UP_MODIFY)
		TAILQ_INSERT_TAIL(&s->uq, up, entries);
	else
		TAILQ_INSERT_TAIL(&s->dq, up, entries);

	/* 
	 * For "update" statements, start with the fields that will be
	 * updated (from the self-same structure).
	 * This is followed by a colon (continue) or a semicolon (end).
	 */

	parse_next(p);

	if (up->type == UP_MODIFY) {
		if (p->lasttype == TOK_COLON) {
			parse_next(p);
			goto crq;
		} else if (p->lasttype == TOK_SEMICOLON)
			return;

		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected field to modify");
			return;
		}

		/* Parse modifier and delay field name resolution. */

		if ((ur = uref_alloc(p, up, 1)) == NULL)
			return;
		while (parse_next(p) != TOK_COLON) {
			if (p->lasttype == TOK_SEMICOLON)
				return;

			/*
			 * See if we're going to accept a modifier,
			 * which defaults to "set".
			 * We only allow non-setters for numeric types,
			 * but we'll check that during linking.
			 */

			if (TOK_IDENT == p->lasttype) {
				for (ur->mod = 0; 
				     MODTYPE__MAX != ur->mod; ur->mod++)
					if (0 == strcasecmp
					    (p->last.string, 
					     modtypes[ur->mod]))
						break;
				if (MODTYPE__MAX == ur->mod) {
					parse_errx(p, "bad modifier");
					return;
				} 
				parse_next(p);
				if (TOK_COLON == p->lasttype)
					break;
				if (TOK_SEMICOLON == p->lasttype)
					return;
			}

			if (TOK_COMMA != p->lasttype) {
				parse_errx(p, "expected separator");
				return;
			} else if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected modify field");
				return;
			}
			if ((ur = uref_alloc(p, up, 1)) == NULL)
				return;
		}
		assert(TOK_COLON == p->lasttype);
		parse_next(p);
	}

	/*
	 * Now the fields that will be used to constrain the update
	 * mechanism.
	 * Usually this will be a rowid.
	 * This is followed by a semicolon or colon.
	 * If it's left empty, we either have a semicolon or colon.
	 */
crq:
	if (TOK_COLON == p->lasttype)
		goto terms;
	else if (TOK_SEMICOLON == p->lasttype)
		return;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected constraint field");
		return;
	}

	if ((ur = uref_alloc(p, up, 0)) == NULL)
		return;
	while (TOK_COLON != parse_next(p)) {
		if (TOK_SEMICOLON == p->lasttype)
			return;

		/* Parse optional operator. */

		if (TOK_IDENT == p->lasttype) {
			for (ur->op = 0; 
			     OPTYPE__MAX != ur->op; ur->op++)
				if (0 == strcasecmp(p->last.string, 
				    optypes[ur->op]))
					break;
			if (OPTYPE__MAX == ur->op) {
				parse_errx(p, "unknown operator");
				return;
			}
			if (TOK_COLON == parse_next(p))
				break;
			else if (TOK_SEMICOLON == p->lasttype)
				return;
		}

		if (TOK_COMMA != p->lasttype) {
			parse_errx(p, "expected fields separator");
			return;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected constraint field");
			return;
		}
		if ((ur = uref_alloc(p, up, 0)) == NULL)
			return;
	}
	assert(TOK_COLON == p->lasttype);

	/*
	 * Lastly, process update terms.
	 * This now consists of "name" and "comment".
	 */
terms:
	parse_next(p);

	while ( ! PARSE_STOP(p)) {
		if (TOK_SEMICOLON == p->lasttype)
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected terms");
			return;
		}

		if (0 == strcasecmp(p->last.string, "name")) {
			if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected term name");
				return;
			}
			free(up->name);
			up->name = strdup(p->last.string);
			if (NULL == up->name) {
				parse_err(p);
				return;
			}
		} else if (0 == strcasecmp(p->last.string, "comment")) {
			if ( ! parse_comment(p, &up->doc))
				return;
		} else
			parse_errx(p, "unknown term: %s", p->last.string);

		parse_next(p);
	}
}

/*
 * Parse a search clause as follows:
 *
 *  ["search"|"list"|"iterate"|"count"] [ search_terms ]* 
 *  [":" search_params ]? ";"
 *
 * The optional terms (searchable field) parts are parsed in
 * parse_config_search_terms().
 * The optional params are in parse_config_search_params().
 */
static void
parse_struct_search(struct parse *p, struct strct *s, enum stype stype)
{
	struct search	*srch;

	if ((srch = calloc(1, sizeof(struct search))) == NULL) {
		parse_err(p);
		return;
	}

	srch->parent = s;
	srch->type = stype;
	parse_point(p, &srch->pos);
	TAILQ_INIT(&srch->sntq);
	TAILQ_INIT(&srch->ordq);
	TAILQ_INSERT_TAIL(&s->sq, srch, entries);

	/*
	 * If we have an identifier up next, then consider it the
	 * prelude to a set of search terms.
	 * If we don't, we either have a semicolon (end), a colon (start
	 * of info), or error.
	 */

	if (parse_next(p) == TOK_IDENT) {
		parse_config_search_terms(p, srch);
		while (p->lasttype == TOK_COMMA) {
			parse_next(p);
			parse_config_search_terms(p, srch);
		}
	} else {
		if (p->lasttype == TOK_SEMICOLON || PARSE_STOP(p))
			return;
		if (p->lasttype != TOK_COLON) {
			parse_errx(p, "expected field identifier");
			return;
		}
	}

	if (p->lasttype == TOK_COLON)
		parse_config_search_params(p, srch);

	/*
	 * If we have no distinct, then mark our struct as needing the
	 * queue or iterator functionality.
	 * If we're returning distinct results, then the queue or
	 * iterator status are for the referenced structure, which we'll
	 * set when linking.
	 */

	if (srch->dst == NULL || srch->dst->strct == s) {
		if (stype == STYPE_LIST)
			s->flags |= STRCT_HAS_QUEUE;
		else if (stype == STYPE_ITERATE)
			s->flags |= STRCT_HAS_ITERATOR;
	}
}

/*
 * Look up or create a rolemap (type and name of role permission).
 * Assign roles in "ns" to the map.
 */
static int
rolemap_assign(struct parse *p, struct strct *s, 
	const char **ns, size_t nsz, 
	enum rolemapt type, const char *name)
{
	struct rref	*rs;
	struct rolemap	*rm = NULL;
	size_t		 i;
	struct resolve	*r;

	/* Look up existing maps by type and possibly name. */

	TAILQ_FOREACH(r, &p->cfg->priv->rq, entries)
		if (RESOLVE_ROLEMAP == r->type &&
		    r->struct_rolemap.result->parent == s &&
		    r->struct_rolemap.type == type &&
		    ((name == NULL &&
		      r->struct_rolemap.name == NULL) ||
		     (name != NULL &&
		      r->struct_rolemap.name != NULL &&
		      strcasecmp(name, r->struct_rolemap.name) == 0))) {
			rm = r->struct_rolemap.result;
			break;
		}

	/* We haven't defined this map. */

	if (rm == NULL) {
		rm = calloc(1, sizeof(struct rolemap));
		if (NULL == rm) {
			parse_err(p);
			return 0;
		}
		TAILQ_INIT(&rm->rq);
		rm->type = type;
		rm->parent = s;
		TAILQ_INSERT_TAIL(&s->rq, rm, entries);

		r = calloc(1, sizeof(struct resolve));
		if (r == NULL) {
			parse_err(p);
			return 0;
		}
		TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
		r->type = RESOLVE_ROLEMAP;
		if (name != NULL &&
		    (r->struct_rolemap.name = strdup(name)) == NULL) {
			parse_err(p);
			return 0;
		}
		r->struct_rolemap.type = type;
		r->struct_rolemap.result = rm;
	}

	assert(rm->parent == s);

	/* Assign roles and add resolvers for them. */

	for (i = 0; i < nsz; i++) {
		rs = calloc(1, sizeof(struct rref));
		if (rs == NULL) {
			parse_err(p);
			return 0;
		}
		parse_point(p, &rs->pos);
		rs->parent = rm;
		TAILQ_INSERT_TAIL(&rm->rq, rs, entries);

		r = calloc(1, sizeof(struct resolve));
		if (r == NULL) {
			parse_err(p);
			return 0;
		}
		TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
		r->type = RESOLVE_ROLE;
		r->struct_role.result = rs;
		r->struct_role.name = strdup(ns[i]);
		if (r->struct_role.name == NULL) {
			parse_err(p);
			return 0;
		}
	}

	return 1;
}

/*
 * Parse out a set of role assignments.
 * Assignment begins with a set of role names (into "rq") then a list of
 * operations available to those roles.
 */
static void
parse_struct_roles(struct parse *p, struct strct *s)
{
	enum rolemapt	  type;
	const char	 *name;
	char		**ns = NULL;
	size_t		  i, nsz = 0;

	/* Get all role names we're going to assign. */

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected role name");
		return;
	} else if (strcasecmp("none", p->last.string) == 0) {
		parse_errx(p, "cannot assign \"none\" role");
		return;
	} else if (!name_append(&ns, &nsz, p->last.string)) {
		parse_err(p);
		return;
	}

	while (!PARSE_STOP(p) && parse_next(p) != TOK_LBRACE) {
		if (p->lasttype != TOK_COMMA) {
			parse_errx(p, "expected comma");
			goto cleanup;
		} else if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected role name");
			goto cleanup;
		} else if (strcasecmp(p->last.string, "none") == 0) {
			parse_errx(p, "cannot assign \"none\" role");
			goto cleanup;
		}
		if (!name_append(&ns, &nsz, p->last.string)) {
			parse_err(p);
			return;
		}
	}

	if (PARSE_STOP(p) || p->lasttype != TOK_LBRACE)
		goto cleanup;

	/* Assign each role to each given operation. */

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_RBRACE)
			break;
		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected role constraint type");
			goto cleanup;
		}

		/* What kind of operation. */

		for (type = 0; type < ROLEMAP__MAX; type++) 
			if (strcasecmp
			    (p->last.string, rolemapts[type]) == 0)
				break;
		if (type == ROLEMAP__MAX) {
			parse_errx(p, "unknown role constraint type");
			goto cleanup;
		}

		/* Some constraints are named, some aren't. */

		parse_next(p);
		if (p->lasttype == TOK_IDENT) {
			if (type == ROLEMAP_INSERT || 
			    type == ROLEMAP_ALL) {
				parse_errx(p, "unexpected "
					"role constraint name");
				goto cleanup;
			}
			name = p->last.string;
			parse_next(p);
		} else if (p->lasttype == TOK_SEMICOLON) {
			if (type != ROLEMAP_INSERT &&
			    type != ROLEMAP_NOEXPORT &&
			    type != ROLEMAP_ALL) {
				parse_errx(p, "expected "
					"role constraint name");
				goto cleanup;
			}
			name = NULL;
		} else {
			parse_errx(p, "expected role constraint "
				"name or semicolon");
			goto cleanup;
		} 

		/* Assign all roles to the given operation. */

		if (!rolemap_assign(p, s, 
		    (const char **)ns, nsz, type, name))
			goto cleanup;

		if (p->lasttype != TOK_SEMICOLON) {
			parse_errx(p, "expected semicolon");
			goto cleanup;
		}
	}

	if (!PARSE_STOP(p) && p->lasttype == TOK_RBRACE)
		if (parse_next(p) != TOK_SEMICOLON)
			parse_errx(p, "expected semicolon");

cleanup:
	for (i = 0; i < nsz; i++)
		free(ns[i]);
	free(ns);
}

/*
 * Parse the comment field of a struct until and including the trailing
 * semicolon.
 */
static void
parse_struct_comment(struct parse *p, struct strct *s)
{

	if (!parse_comment(p, &s->doc))
		return;
	if (parse_next(p) != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
}

/*
 * Parse the insert statement of a struct until and including the
 * trailing semicolon.
 */
static void
parse_struct_insert(struct parse *p, struct strct *s)
{

	if (s->ins != NULL) {
		parse_errx(p, "insert already defined");
		return;
	}
	if ((s->ins = calloc(1, sizeof(struct insert))) == NULL) {
		parse_err(p);
		return;
	}
	s->ins->parent = s;
	parse_point(p, &s->ins->pos);
	if (parse_next(p) != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
}

/*
 * Parse a full struct until and including the semicolon following.
 */
static void
parse_struct_data(struct parse *p, struct strct *s)
{

	if (parse_next(p) != TOK_LBRACE) {
		parse_errx(p, "expected left brace");
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_RBRACE)
			break;
		if (p->lasttype != TOK_IDENT)
			parse_errx(p, "expected statement type");
		else if (strcasecmp(p->last.string, "comment") == 0)
			parse_struct_comment(p, s);
		else if (strcasecmp(p->last.string, "search") == 0)
			parse_struct_search(p, s, STYPE_SEARCH);
		else if (strcasecmp(p->last.string, "count") == 0)
			parse_struct_search(p, s, STYPE_COUNT);
		else if (strcasecmp(p->last.string, "list") == 0)
			parse_struct_search(p, s, STYPE_LIST);
		else if (strcasecmp(p->last.string, "iterate") == 0)
			parse_struct_search(p, s, STYPE_ITERATE);
		else if (strcasecmp(p->last.string, "update") == 0)
			parse_struct_update(p, s, UP_MODIFY);
		else if (strcasecmp(p->last.string, "delete") == 0)
			parse_struct_update(p, s, UP_DELETE);
		else if (strcasecmp(p->last.string, "insert") == 0)
			parse_struct_insert(p, s);
		else if (strcasecmp(p->last.string, "unique") == 0)
			parse_struct_unique(p, s);
		else if (strcasecmp(p->last.string, "roles") == 0)
			parse_struct_roles(p, s);
		else if (strcasecmp(p->last.string, "field") == 0)
			parse_field(p, s);
		else
			parse_errx(p, "unknown statement type: %s",
				p->last.string);
	}

	if (PARSE_STOP(p))
		return;
	if (parse_next(p) != TOK_SEMICOLON) 
		parse_errx(p, "expected semicolon");
	if (TAILQ_EMPTY(&s->fq))
		parse_errx(p, "no fields in struct");
}

/*
 * Run any post-parsing operations.
 */
static void
parse_struct_post(struct parse *p, struct strct *s)
{
	struct update	*up;
	struct field	*f;
	struct uref	*ref;

	if (PARSE_STOP(p))
		return;

	/*
	 * Update clauses without any modification fields inherit all
	 * the fields of the structure.
	 */

	TAILQ_FOREACH(up, &s->uq, entries) {
		assert(up->type == UP_MODIFY);
		if (!TAILQ_EMPTY(&up->mrq))
			continue;
		up->flags |= UPDATE_ALL;
		TAILQ_FOREACH(f, &s->fq, entries) {
			if (f->flags & FIELD_ROWID)
				continue;
			if (f->type == FTYPE_STRUCT)
				continue;
			ref = calloc(1, sizeof(struct uref));
			if (ref == NULL) {
				parse_err(p);
				return;
			}
			TAILQ_INSERT_TAIL(&up->mrq, ref, entries);
			ref->mod = MODTYPE_SET;
			ref->op = OPTYPE_EQUAL;
			ref->field = f;
			ref->parent = up;
			ref->pos = up->pos;
		}
	}
}

/*
 * Verify and allocate a struct, then start parsing its fields and
 * ancillary entries.
 */
void
parse_struct(struct parse *p)
{
	struct strct	*s;
	struct pos	 pos;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected struct name");
		return;
	}
	parse_point(p, &pos);
	if ((s = strct_alloc(p, p->last.string)) == NULL)
		return;
	parse_struct_data(p, s);
	parse_struct_post(p, s);
}
