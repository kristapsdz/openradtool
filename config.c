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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"

static void
parse_free_field(struct field *p)
{
	struct fvalid *fv;

	while ((fv = TAILQ_FIRST(&p->fvq)) != NULL) {
		TAILQ_REMOVE(&p->fvq, fv, entries);
		free(fv);
	}
	if (p->ref != NULL)
		free(p->ref);
	if (FIELD_HASDEF && 
	    (FTYPE_TEXT == p->type ||
	     FTYPE_EMAIL == p->type))
		free(p->def.string);
	free(p->doc);
	free(p->name);
	free(p);
}

static void
parse_free_aggr(struct aggr *p)
{

	free(p->fname);
	free(p->name);
	free(p);
}

static void
parse_free_group(struct group *p)
{
	
	free(p->fname);
	free(p->name);
	free(p);
}

static void
parse_free_ordq(struct ordq *q)
{
	struct ord	*ord;

	while ((ord = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, ord, entries);
		free(ord->fname);
		free(ord->name);
		free(ord);
	}
}

static void
parse_free_distinct(struct dstnct *p)
{

	free(p->fname);
	free(p);
}

static void
parse_free_search(struct search *p)
{
	struct sent	*sent;

	if (p->dst != NULL)
		parse_free_distinct(p->dst);
	if (p->aggr != NULL)
		parse_free_aggr(p->aggr);
	parse_free_ordq(&p->ordq);
	if (p->group != NULL)
		parse_free_group(p->group);

	while ((sent = TAILQ_FIRST(&p->sntq)) != NULL) {
		TAILQ_REMOVE(&p->sntq, sent, entries);
		free(sent->uname);
		free(sent->fname);
		free(sent->name);
		free(sent);
	}
	free(p->doc);
	free(p->name);
	free(p);
}

static void
parse_free_unique(struct unique *p)
{
	struct nref	*u;

	while ((u = TAILQ_FIRST(&p->nq)) != NULL) {
		TAILQ_REMOVE(&p->nq, u, entries);
		free(u);
	}
	free(p);
}

static void
parse_free_update(struct update *p)
{
	struct uref	*u;

	while ((u = TAILQ_FIRST(&p->mrq)) != NULL) {
		TAILQ_REMOVE(&p->mrq, u, entries);
		free(u);
	}
	while ((u = TAILQ_FIRST(&p->crq)) != NULL) {
		TAILQ_REMOVE(&p->crq, u, entries);
		free(u);
	}

	free(p->doc);
	free(p->name);
	free(p);
}

static void
parse_free_label(struct labelq *q)
{
	struct label	*l;

	while ((l = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, l, entries);
		free(l->label);
		free(l);
	}
}

static void
parse_free_enum(struct enm *e)
{
	struct eitem	*ei;

	while ((ei = TAILQ_FIRST(&e->eq)) != NULL) {
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
 * Recursively frees all roles within "r".
 */
static void
parse_free_role(struct role *r)
{
	struct role	*rr;

	while ((rr = TAILQ_FIRST(&r->subrq)) != NULL) {
		TAILQ_REMOVE(&r->subrq, rr, entries);
		parse_free_role(rr);
	}

	free(r->doc);
	free(r->name);
	free(r);
}

static void
parse_free_bitfield(struct bitf *bf)
{
	struct bitidx	*bi;

	while ((bi = TAILQ_FIRST(&bf->bq)) != NULL) {
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

static void
parse_free_rolemap(struct rolemap *rm)
{
	struct roleset	*r;

	while ((r = TAILQ_FIRST(&rm->setq)) != NULL) {
		TAILQ_REMOVE(&rm->setq, r, entries);
		free(r->name);
		free(r);
	}

	free(rm->name);
	free(rm);
}

static void
parse_free_strct(struct strct *p)
{
	struct field	*f;
	struct search	*s;
	struct alias	*a;
	struct update	*u;
	struct unique	*n;
	struct rolemap	*rm;

	while ((f = TAILQ_FIRST(&p->fq)) != NULL) {
		TAILQ_REMOVE(&p->fq, f, entries);
		parse_free_field(f);
	}
	while ((s = TAILQ_FIRST(&p->sq)) != NULL) {
		TAILQ_REMOVE(&p->sq, s, entries);
		parse_free_search(s);
	}
	while ((rm = TAILQ_FIRST(&p->rq)) != NULL) {
		TAILQ_REMOVE(&p->rq, rm, entries);
		parse_free_rolemap(rm);
	}
	while ((a = TAILQ_FIRST(&p->aq)) != NULL) {
		TAILQ_REMOVE(&p->aq, a, entries);
		free(a->name);
		free(a->alias);
		free(a);
	}
	while ((u = TAILQ_FIRST(&p->uq)) != NULL) {
		TAILQ_REMOVE(&p->uq, u, entries);
		parse_free_update(u);
	}
	while ((u = TAILQ_FIRST(&p->dq)) != NULL) {
		TAILQ_REMOVE(&p->dq, u, entries);
		parse_free_update(u);
	}
	while ((n = TAILQ_FIRST(&p->nq)) != NULL) {
		TAILQ_REMOVE(&p->nq, n, entries);
		parse_free_unique(n);
	}

	free(p->doc);
	free(p->name);
	free(p->cname);
	free(p->ins);
	free(p);
}

static void
parse_free_resolve(struct resolve *p)
{
	size_t	 i;

	switch (p->type) {
	case RESOLVE_FIELD_BITS:
		free(p->field_bits.name);
		break;
	case RESOLVE_FIELD_ENUM:
		free(p->field_enum.name);
		break;
	case RESOLVE_FIELD_FOREIGN:
		free(p->field_foreign.tstrct);
		free(p->field_foreign.tfield);
		break;
	case RESOLVE_FIELD_STRUCT:
		free(p->field_struct.sfield);
		break;
	case RESOLVE_AGGR:
		for (i = 0; i < p->struct_aggr.namesz; i++)
			free(p->struct_aggr.names[i]);
		free(p->struct_aggr.names);
		break;
	case RESOLVE_DISTINCT:
		for (i = 0; i < p->struct_distinct.namesz; i++)
			free(p->struct_distinct.names[i]);
		free(p->struct_distinct.names);
		break;
	case RESOLVE_GROUPROW:
		for (i = 0; i < p->struct_grouprow.namesz; i++)
			free(p->struct_grouprow.names[i]);
		free(p->struct_grouprow.names);
		break;
	case RESOLVE_ORDER:
		for (i = 0; i < p->struct_order.namesz; i++)
			free(p->struct_order.names[i]);
		free(p->struct_order.names);
		break;
	case RESOLVE_SENT:
		for (i = 0; i < p->struct_sent.namesz; i++)
			free(p->struct_sent.names[i]);
		free(p->struct_sent.names);
		break;
	case RESOLVE_UNIQUE:
		free(p->struct_unique.name);
		break;
	case RESOLVE_UP_CONSTRAINT:
		free(p->struct_up_const.name);
		break;
	case RESOLVE_UP_MODIFIER:
		free(p->struct_up_mod.name);
		break;
	}

	free(p);
}

/*
 * Free all configuration resources.
 * Does nothing if "q" is empty or NULL.
 */
void
ort_config_free(struct config *cfg)
{
	struct strct	*p;
	struct enm	*e;
	struct role	*r;
	struct bitf	*bf;
	struct resolve	*res;
	size_t		 i;

	if (cfg == NULL)
		return;

	while ((e = TAILQ_FIRST(&cfg->eq)) != NULL) {
		TAILQ_REMOVE(&cfg->eq, e, entries);
		parse_free_enum(e);
	}

	while ((r = TAILQ_FIRST(&cfg->rq)) != NULL) {
		TAILQ_REMOVE(&cfg->rq, r, entries);
		parse_free_role(r);
	}

	while ((bf = TAILQ_FIRST(&cfg->bq)) != NULL) {
		TAILQ_REMOVE(&cfg->bq, bf, entries);
		parse_free_bitfield(bf);
	}

	while ((p = TAILQ_FIRST(&cfg->sq)) != NULL) {
		TAILQ_REMOVE(&cfg->sq, p, entries);
		parse_free_strct(p);
	}

	while ((res = TAILQ_FIRST(&cfg->priv->rq)) != NULL) {
		TAILQ_REMOVE(&cfg->priv->rq, res, entries);
		parse_free_resolve(res);
	}

	for (i = 0; i < cfg->langsz; i++)
		free(cfg->langs[i]);
	free(cfg->langs);

	for (i = 0; i < cfg->fnamesz; i++)
		free(cfg->fnames[i]);
	free(cfg->fnames);

	for (i = 0; i < cfg->msgsz; i++)
		free(cfg->msgs[i].buf);
	free(cfg->msgs);

	free(cfg->priv);
	free(cfg);
}

/*
 * Allocate a config object or return NULL on failure.
 * The allocated object, on success, is loaded with a single default
 * language and that's it.
 */
struct config *
ort_config_alloc(void)
{
	struct config	*cfg;

	cfg = calloc(1, sizeof(struct config));
	if (cfg == NULL) {
		ort_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		return NULL;
	}

	cfg->priv = calloc(1, sizeof(struct config_private));
	if (cfg->priv == NULL) {
		ort_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		free(cfg);
		return NULL;
	}

	/* The default language must always exist. */

	cfg->langsz = 1;
	if ((cfg->langs = calloc(1, sizeof(char *))) == NULL) {
		ort_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		free(cfg->priv);
		free(cfg);
		return NULL;
	} else if ((cfg->langs[0] = strdup("")) == NULL) {
		ort_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		free(cfg->langs);
		free(cfg->priv);
		free(cfg);
		return NULL;
	}

	TAILQ_INIT(&cfg->sq);
	TAILQ_INIT(&cfg->eq);
	TAILQ_INIT(&cfg->rq);
	TAILQ_INIT(&cfg->bq);
	TAILQ_INIT(&cfg->priv->rq);
	return cfg;
}
