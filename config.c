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

static	const char *const msgtypes[] = {
	"warning", /* MSGTYPE_WARN */
	"error", /* MSGTYPE_ERROR */
	"fatal" /* MSGTYPE_FATAL */
};

/*
 * Disallowed field names.
 * The SQL ones are from https://sqlite.org/lang_keywords.html.
 * FIXME: think about this more carefully, as in SQL, there are many
 * things that we can put into string literals.
 */
static	const char *const badidents[] = {
	/* Things not allowed in C. */
	"auto",
	"break",
	"case",
	"char",
	"const",
	"continue",
	"default",
	"do",
	"double",
	"enum",
	"extern",
	"float",
	"goto",
	"long",
	"register",
	"short",
	"signed",
	"static",
	"struct",
	"typedef",
	"union",
	"unsigned",
	"void",
	"volatile",
	/* Things not allowed in SQLite. */
	"ABORT",
	"ACTION",
	"ADD",
	"AFTER",
	"ALL",
	"ALTER",
	"ANALYZE",
	"AND",
	"AS",
	"ASC",
	"ATTACH",
	"AUTOINCREMENT",
	"BEFORE",
	"BEGIN",
	"BETWEEN",
	"BY",
	"CASCADE",
	"CASE",
	"CAST",
	"CHECK",
	"COLLATE",
	"COLUMN",
	"COMMIT",
	"CONFLICT",
	"CONSTRAINT",
	"CREATE",
	"CROSS",
	"CURRENT_DATE",
	"CURRENT_TIME",
	"CURRENT_TIMESTAMP",
	"DATABASE",
	"DEFAULT",
	"DEFERRABLE",
	"DEFERRED",
	"DELETE",
	"DESC",
	"DETACH",
	"DISTINCT",
	"DROP",
	"EACH",
	"ELSE",
	"END",
	"ESCAPE",
	"EXCEPT",
	"EXCLUSIVE",
	"EXISTS",
	"EXPLAIN",
	"FAIL",
	"FOR",
	"FOREIGN",
	"FROM",
	"FULL",
	"GLOB",
	"GROUP",
	"HAVING",
	"IF",
	"IGNORE",
	"IMMEDIATE",
	"IN",
	"INDEX",
	"INDEXED",
	"INITIALLY",
	"INNER",
	"INSERT",
	"INSTEAD",
	"INTERSECT",
	"INTO",
	"IS",
	"ISNULL",
	"JOIN",
	"KEY",
	"LEFT",
	"LIKE",
	"LIMIT",
	"MATCH",
	"NATURAL",
	"NOT",
	"NOTNULL",
	"NULL",
	"OF",
	"OFFSET",
	"ON",
	"OR",
	"ORDER",
	"OUTER",
	"PLAN",
	"PRAGMA",
	"PRIMARY",
	"QUERY",
	"RAISE",
	"RECURSIVE",
	"REFERENCES",
	"REGEXP",
	"REINDEX",
	"RELEASE",
	"RENAME",
	"REPLACE",
	"RESTRICT",
	"RIGHT",
	"ROLLBACK",
	"ROW",
	"SAVEPOINT",
	"SELECT",
	"SET",
	"TABLE",
	"TEMP",
	"TEMPORARY",
	"THEN",
	"TO",
	"TRANSACTION",
	"TRIGGER",
	"UNION",
	"UNIQUE",
	"UPDATE",
	"USING",
	"VACUUM",
	"VALUES",
	"VIEW",
	"VIRTUAL",
	"WHEN",
	"WHERE",
	"WITH",
	"WITHOUT",
	NULL
};

/*
 * See if the given "name" is a reserved identifier.
 */
int
ort_check_ident(struct config *cfg,
	const struct pos *pos, const char *name)
{
	const char *const *cp;

	for (cp = badidents; NULL != *cp; cp++)
		if (strcasecmp(*cp, name) == 0) {
			ort_config_msg(cfg, MSGTYPE_ERROR, __func__,
				0, pos, "reserved identifier");
			return 0;
		}

	return 1;
}

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
	if (FIELD_HASDEF && 
	    (FTYPE_TEXT == p->type ||
	     FTYPE_EMAIL == p->type))
		free(p->def.string);
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
 * Free a chain reference.
 * Must not be NULL.
 */
static void
parse_free_sref(struct sref *p)
{

	free(p->name);
	free(p);
}

/*
 * Free the aggregate and its pointer.
 * Passing a NULL pointer is a noop.
 */
void
ort_config_free_aggr(struct aggr *aggr)
{
	struct sref	*ref;

	if (aggr == NULL)
		return;

	while ((ref = TAILQ_FIRST(&aggr->arq)) != NULL) {
		TAILQ_REMOVE(&aggr->arq, ref, entries);
		parse_free_sref(ref);
	}
	free(aggr->fname);
	free(aggr->name);
	free(aggr);
}

/*
 * Free the group and its pointer.
 * Passing a NULL pointer is a noop.
 */
static void
parse_free_group(struct group *grp)
{
	struct sref	*ref;
	
	if (NULL == grp)
		return;

	while (NULL != (ref = TAILQ_FIRST(&grp->grq))) {
		TAILQ_REMOVE(&grp->grq, ref, entries);
		parse_free_sref(ref);
	}
	free(grp->fname);
	free(grp->name);
	free(grp);
}

/*
 * Free the order queue.
 * Does not free the "q" pointer, which may not be NULL.
 */
static void
parse_free_ordq(struct ordq *q)
{
	struct ord	*ord;
	struct sref	*ref;

	while (NULL != (ord = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, ord, entries);
		while (NULL != (ref = TAILQ_FIRST(&ord->orq))) {
			TAILQ_REMOVE(&ord->orq, ref, entries);
			parse_free_sref(ref);
		}
		free(ord->fname);
		free(ord->name);
		free(ord);
	}
}

/*
 * Free a distinct series.
 * Does nothing if "p" is NULL.
 */
void
ort_config_free_distinct(struct dstnct *p)
{
	struct dref	*d;

	if (p == NULL)
		return;

	while (NULL != (d = TAILQ_FIRST(&p->drefq))) {
		TAILQ_REMOVE(&p->drefq, d, entries);
		free(d->name);
		free(d);
	}
	free(p->cname);
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

	if (NULL == p)
		return;

	ort_config_free_distinct(p->dst);
	ort_config_free_aggr(p->aggr);
	parse_free_ordq(&p->ordq);
	parse_free_group(p->group);

	while (NULL != (sent = TAILQ_FIRST(&p->sntq))) {
		TAILQ_REMOVE(&p->sntq, sent, entries);
		while (NULL != (s = TAILQ_FIRST(&sent->srq))) {
			TAILQ_REMOVE(&sent->srq, s, entries);
			parse_free_sref(s);
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

	free(p->cname);
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
 * For all structs, enumerations, and bitfields, make sure that we don't
 * have any of the same top-level names.
 * That's because the C output for these will be, given "name", "struct
 * name", "enum name", which can't be the same.
 */
static int
check_dupetoplevel(struct config *cfg, 
	const struct pos *pos, const char *name)
{
	const struct enm *e;
	const struct bitf *b;
	const struct strct *s;
	struct pos	 npos;

	memset(&npos, 0, sizeof(struct pos));

	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (0 == strcasecmp(e->name, name)) {
			npos = e->pos;
			goto found;
		}
	TAILQ_FOREACH(b, &cfg->bq, entries)
		if (0 == strcasecmp(b->name, name)) {
			npos = b->pos;
			goto found;
		}
	TAILQ_FOREACH(s, &cfg->sq, entries) 
		if (0 == strcasecmp(s->name, name)) {
			npos = s->pos;
			goto found;
		}

	return 1;

found:
	/* 
	 * If we were called from the parser, we'll have a file and line
	 * number; otherwise, this will be empty.
	 */

	if (NULL != npos.fname && npos.line)
		ort_config_msg(cfg, MSGTYPE_ERROR, __func__, 0,
			pos, "duplicate top-level name: %s:%zu:%zu",
			npos.fname, npos.line, npos.column);
	else
		ort_config_msg(cfg, MSGTYPE_ERROR, __func__, 0, 
			pos, "duplicate top-level name");

	return 0;
}

struct field *
ort_field_alloc(struct config *cfg, struct strct *s,
	const struct pos *pos, const char *name)
{
	struct field	  *fd;
	const char *const *cp;

	/* Check reserved identifiers. */

	for (cp = badidents; NULL != *cp; cp++)
		if (0 == strcasecmp(*cp, name)) {
			ort_config_msg(cfg, MSGTYPE_ERROR, __func__, 
				0, NULL, "reserved identifier");
			return NULL;
		}

	/* Check other fields in struct having same name. */

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (strcasecmp(fd->name, name))
			continue;
		if (NULL != fd->pos.fname && fd->pos.line)
			ort_config_msg(cfg, MSGTYPE_ERROR, __func__, 
				0, pos, "duplicate field name: "
				"%s:%zu:%zu", fd->pos.fname, 
				fd->pos.line, fd->pos.column);
		else
			ort_config_msg(cfg, MSGTYPE_ERROR, __func__, 
				0, pos, "duplicate field name");
		return NULL;
	}

	/* Now the actual allocation. */

	if (NULL == (fd = calloc(1, sizeof(struct field)))) {
		ort_config_msg(cfg, MSGTYPE_FATAL, __func__, 
			errno, pos, NULL);
		return NULL;
	} else if (NULL == (fd->name = strdup(name))) {
		ort_config_msg(cfg, MSGTYPE_FATAL, __func__, 
			errno, pos, NULL);
		free(fd);
		return NULL;
	}

	if (NULL != pos)
		fd->pos = *pos;
	fd->type = FTYPE_INT;
	fd->parent = s;
	TAILQ_INIT(&fd->fvq);
	TAILQ_INSERT_TAIL(&s->fq, fd, entries);
	return fd;
}

struct strct *
ort_strct_alloc(struct config *cfg, 
	const struct pos *pos, const char *name)
{
	struct strct	  *s;
	char		  *caps;
	const char *const *cp;

	/* Check reserved identifiers. */

	for (cp = badidents; NULL != *cp; cp++)
		if (0 == strcasecmp(*cp, name)) {
			ort_config_msg(cfg, MSGTYPE_ERROR, 
				__func__, 0, NULL, 
				"reserved identifier");
			return NULL;
		}

	/* Check other toplevels having same name. */

	if ( ! check_dupetoplevel(cfg, pos, name))
		return NULL;

	/* Now make allocation. */

	if (NULL == (s = calloc(1, sizeof(struct strct))) ||
	    NULL == (s->name = strdup(name)) ||
	    NULL == (s->cname = strdup(s->name))) {
		ort_config_msg(cfg, MSGTYPE_FATAL, __func__, 
			errno, pos, NULL);
		if (NULL != s) {
			free(s->name);
			free(s->cname);
		}
		free(s);
		return NULL;
	}

	for (caps = s->cname; '\0' != *caps; caps++)
		*caps = toupper((unsigned char)*caps);

	if (NULL != pos)
		s->pos = *pos;
	s->cfg = cfg;
	TAILQ_INSERT_TAIL(&cfg->sq, s, entries);
	TAILQ_INIT(&s->fq);
	TAILQ_INIT(&s->sq);
	TAILQ_INIT(&s->aq);
	TAILQ_INIT(&s->uq);
	TAILQ_INIT(&s->nq);
	TAILQ_INIT(&s->dq);
	TAILQ_INIT(&s->rq);
	return s;
}

/*
 * Free all configuration resources.
 * Does nothing if "q" is empty or NULL.
 */
void
ort_config_free(struct config *cfg)
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

	for (i = 0; i < cfg->msgsz; i++)
		free(cfg->msgs[i].buf);
	free(cfg->msgs);

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

	if ((cfg = calloc(1, sizeof(struct config))) == NULL) {
		ort_config_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		return NULL;
	}

	TAILQ_INIT(&cfg->sq);
	TAILQ_INIT(&cfg->eq);
	TAILQ_INIT(&cfg->rq);
	TAILQ_INIT(&cfg->bq);

	/* 
	 * Start with the default language.
	 * This must always exist.
	 */

	if ((cfg->langs = calloc(1, sizeof(char *))) == NULL) {
		ort_config_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		free(cfg);
		return NULL;
	} else if ((cfg->langs[0] = strdup("")) == NULL) {
		ort_config_msg(NULL, MSGTYPE_FATAL, 
			"config", errno, NULL, NULL);
		free(cfg->langs);
		free(cfg);
		return NULL;
	}
	cfg->langsz = 1;
	return cfg;
}

/*
 * Internal logging function: logs to stderr and (optionally) queues
 * message.
 * This accepts an optionally-NULL "msg" at optionally-NULL "pos" with
 * errno "er" or zero (only recognised on MSGTYPE_FATAL).
 * The "chan" value may not be NULL.
 * If "cfg" is not NULL, this enqueues the message into the "msgs" array
 * for that configuration.
 * The "msg" pointer will be freed if "cfg" is NULL, as otherwise its
 * ownership is transferred to the "msgs" array.
 */
static void
ort_config_log(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, char *msg)
{
	void		*pp;
	struct msg	*m;

	if (cfg != NULL) {
		pp = reallocarray(cfg->msgs, 
			cfg->msgsz + 1, sizeof(struct msg));
		/* Well, shit. */
		if (NULL == pp) {
			free(msg);
			return;
		}
		cfg->msgs = pp;
		m = &cfg->msgs[cfg->msgsz++];
		memset(m, 0, sizeof(struct msg));
		m->type = type;
		m->er = er;
		m->buf = msg;
		if (pos != NULL)
			m->pos = *pos;
	}

	/* Now we also print the message to stderr. */

	if (pos != NULL && pos->fname != NULL && pos->line > 0)
		fprintf(stderr, "%s:%zu:%zu: %s %s: ", 
			pos->fname, pos->line, pos->column, 
			chan, msgtypes[type]);
	else if (pos != NULL && pos->fname != NULL)
		fprintf(stderr, "%s: %s %s: ", 
			pos->fname, chan, msgtypes[type]);
	else 
		fprintf(stderr, "%s %s: ", chan, msgtypes[type]);

	if (msg != NULL)
		fputs(msg, stderr);

	if (type == MSGTYPE_FATAL)
		fprintf(stderr, "%s%s", 
			msg != NULL ? ": " : "", strerror(er)); 

	fputc('\n', stderr);

	if (cfg == NULL)
		free(msg);
}

/*
 * Generic message formatting (va_list version).
 * Puts all messages into the message array (if "cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_config_msgv(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, 
	const char *fmt, va_list ap)
{
	char	*buf = NULL;

	if (fmt != NULL && vasprintf(&buf, fmt, ap) == -1)
		return;
	ort_config_log(cfg, type, chan, er, pos, buf);
}

/*
 * Generic message formatting.
 * Puts all messages into the message array if ("cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_config_msg(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, 
	const char *fmt, ...)
{
	va_list	 ap;

	if (fmt == NULL) {
		ort_config_log(cfg, type, chan, er, pos, NULL);
		return;
	}

	va_start(ap, fmt);
	ort_config_msgv(cfg, type, chan, er, pos, fmt, ap);
	va_end(ap);
}
