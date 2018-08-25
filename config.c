/*	$Id$ */
/*
 * Copyright (c) 2017--2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Free a field entity.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_field(struct field *p)
{
	struct fvalid *fv;

	if (NULL == p)
		return;
	while (NULL != (fv = TAILQ_FIRST(&p->fvq))) {
		TAILQ_REMOVE(&p->fvq, fv, entries);
		free(fv);
	}
	if (NULL != p->ref) {
		free(p->ref->sfield);
		free(p->ref->tfield);
		free(p->ref->tstrct);
		free(p->ref);
	}
	if (NULL != p->eref) {
		free(p->eref->ename);
		free(p->eref);
	}
	if (NULL != p->bref) {
		free(p->bref->name);
		free(p->bref);
	}
	free(p->doc);
	free(p->name);
	free(p);
}

/*
 * Free a search series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_search(struct search *p)
{
	struct sref	*s;
	struct sent	*sent;
	struct ord	*ord;
	struct oref	*oref;
	struct dref	*d;

	if (NULL == p)
		return;

	if (NULL != p->dst) {
		while (NULL != (d = TAILQ_FIRST(&p->dst->drefq))) {
			TAILQ_REMOVE(&p->dst->drefq, d, entries);
			free(d->name);
			free(d);
		}
		free(p->dst->cname);
		free(p->dst);
	}

	while (NULL != (ord = TAILQ_FIRST(&p->ordq))) {
		TAILQ_REMOVE(&p->ordq, ord, entries);
		while (NULL != (oref = TAILQ_FIRST(&ord->orq))) {
			TAILQ_REMOVE(&ord->orq, oref, entries);
			free(oref->name);
			free(oref);
		}
		free(ord->fname);
		free(ord->name);
		free(ord);
	}

	while (NULL != (sent = TAILQ_FIRST(&p->sntq))) {
		TAILQ_REMOVE(&p->sntq, sent, entries);
		while (NULL != (s = TAILQ_FIRST(&sent->srq))) {
			TAILQ_REMOVE(&sent->srq, s, entries);
			free(s->name);
			free(s);
		}
		free(sent->fname);
		free(sent->name);
		free(sent);
	}
	free(p->doc);
	free(p->name);
	free(p);
}

/*
 * Free a unique reference.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_nref(struct nref *u)
{

	if (NULL == u)
		return;
	free(u->name);
	free(u);
}

/*
 * Free an update reference.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_uref(struct uref *u)
{

	if (NULL == u)
		return;
	free(u->name);
	free(u);
}

/*
 * Free a unique series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_unique(struct unique *p)
{
	struct nref	*u;

	if (NULL == p)
		return;

	while (NULL != (u = TAILQ_FIRST(&p->nq))) {
		TAILQ_REMOVE(&p->nq, u, entries);
		parse_free_nref(u);
	}

	free(p);
}

/*
 * Free an update series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_update(struct update *p)
{
	struct uref	*u;

	if (NULL == p)
		return;

	while (NULL != (u = TAILQ_FIRST(&p->mrq))) {
		TAILQ_REMOVE(&p->mrq, u, entries);
		parse_free_uref(u);
	}
	while (NULL != (u = TAILQ_FIRST(&p->crq))) {
		TAILQ_REMOVE(&p->crq, u, entries);
		parse_free_uref(u);
	}

	free(p->doc);
	free(p->name);
	free(p);
}

static void
parse_free_label(struct labelq *q)
{
	struct label	*l;

	while (NULL != (l = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, l, entries);
		free(l->label);
		free(l);
	}
}

/*
 * Free an enumeration.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_enum(struct enm *e)
{
	struct eitem	*ei;

	if (NULL == e)
		return;

	while (NULL != (ei = TAILQ_FIRST(&e->eq))) {
		TAILQ_REMOVE(&e->eq, ei, entries);
		parse_free_label(&ei->labels);
		free(ei->name);
		free(ei->doc);
		free(ei);
	}

	free(e->name);
	free(e->cname);
	free(e->doc);
	free(e);
}

/*
 * Free a role (recursive function).
 * Does nothing if "r" is NULL.
 */
static void
parse_free_role(struct role *r)
{
	struct role	*rr;

	if (NULL == r)
		return;
	while (NULL != (rr = TAILQ_FIRST(&r->subrq))) {
		TAILQ_REMOVE(&r->subrq, rr, entries);
		parse_free_role(rr);
	}
	free(r->doc);
	free(r->name);
	free(r);
}

/*
 * Free a bitfield (set of bit indices).
 * Does nothing if "bf" is NULL.
 */
static void
parse_free_bitfield(struct bitf *bf)
{
	struct bitidx	*bi;

	if (NULL == bf)
		return;
	while (NULL != (bi = TAILQ_FIRST(&bf->bq))) {
		TAILQ_REMOVE(&bf->bq, bi, entries);
		parse_free_label(&bi->labels);
		free(bi->name);
		free(bi->doc);
		free(bi);
	}
	parse_free_label(&bf->labels_unset);
	parse_free_label(&bf->labels_null);
	free(bf->name);
	free(bf->cname);
	free(bf->doc);
	free(bf);
}

/*
 * Free a rolemap and its rolesets.
 * Does nothing if "rm" is NULL.
 */
static void
parse_free_rolemap(struct rolemap *rm)
{
	struct roleset	*r;

	if (NULL == rm)
		return;
	while (NULL != (r = TAILQ_FIRST(&rm->setq))) {
		TAILQ_REMOVE(&rm->setq, r, entries);
		free(r->name);
		free(r);
	}
	free(rm->name);
	free(rm);
}

/*
 * Free all resources from the queue of structures.
 * Does nothing if "q" is empty or NULL.
 */
void
config_free(struct config *cfg)
{
	struct strct	*p;
	struct field	*f;
	struct search	*s;
	struct alias	*a;
	struct update	*u;
	struct unique	*n;
	struct enm	*e;
	struct role	*r;
	struct rolemap	*rm;
	struct bitf	*bf;
	size_t		 i;

	if (NULL == cfg)
		return;

	while (NULL != (e = TAILQ_FIRST(&cfg->eq))) {
		TAILQ_REMOVE(&cfg->eq, e, entries);
		parse_free_enum(e);
	}

	while (NULL != (r = TAILQ_FIRST(&cfg->rq))) {
		TAILQ_REMOVE(&cfg->rq, r, entries);
		parse_free_role(r);
	}

	while (NULL != (bf = TAILQ_FIRST(&cfg->bq))) {
		TAILQ_REMOVE(&cfg->bq, bf, entries);
		parse_free_bitfield(bf);
	}

	while (NULL != (p = TAILQ_FIRST(&cfg->sq))) {
		TAILQ_REMOVE(&cfg->sq, p, entries);
		while (NULL != (f = TAILQ_FIRST(&p->fq))) {
			TAILQ_REMOVE(&p->fq, f, entries);
			parse_free_field(f);
		}
		while (NULL != (s = TAILQ_FIRST(&p->sq))) {
			TAILQ_REMOVE(&p->sq, s, entries);
			parse_free_search(s);
		}
		while (NULL != (rm = TAILQ_FIRST(&p->rq))) {
			TAILQ_REMOVE(&p->rq, rm, entries);
			parse_free_rolemap(rm);
		}
		while (NULL != (a = TAILQ_FIRST(&p->aq))) {
			TAILQ_REMOVE(&p->aq, a, entries);
			free(a->name);
			free(a->alias);
			free(a);
		}
		while (NULL != (u = TAILQ_FIRST(&p->uq))) {
			TAILQ_REMOVE(&p->uq, u, entries);
			parse_free_update(u);
		}
		while (NULL != (u = TAILQ_FIRST(&p->dq))) {
			TAILQ_REMOVE(&p->dq, u, entries);
			parse_free_update(u);
		}
		while (NULL != (n = TAILQ_FIRST(&p->nq))) {
			TAILQ_REMOVE(&p->nq, n, entries);
			parse_free_unique(n);
		}
		free(p->doc);
		free(p->name);
		free(p->cname);
		free(p->ins);
		free(p);
	}

	for (i = 0; i < cfg->langsz; i++)
		free(cfg->langs[i]);
	free(cfg->langs);

	for (i = 0; i < cfg->fnamesz; i++)
		free(cfg->fnames[i]);
	free(cfg->fnames);

	free(cfg);
}

struct config *
config_alloc(void)
{
	struct config	*cfg;

	if (NULL == (cfg = calloc(1, sizeof(struct config))))
		return(NULL);

	TAILQ_INIT(&cfg->sq);
	TAILQ_INIT(&cfg->eq);
	TAILQ_INIT(&cfg->rq);
	TAILQ_INIT(&cfg->bq);

	/* 
	 * Start with the default language.
	 * This must always exist.
	 */

	cfg->langs = calloc(1, sizeof(char *));
	if (NULL == cfg->langs)
		err(EXIT_FAILURE, NULL);
	cfg->langs[0] = strdup("");
	if (NULL == cfg->langs[0])
		err(EXIT_FAILURE, NULL);
	cfg->langsz = 1;

	return cfg;
}

