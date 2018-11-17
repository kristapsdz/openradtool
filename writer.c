/*	$Id$ */
/*
 * Copyright (c) 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kwebapp.h"
#include "extern.h"

static	const char *const stypes[] = {
	"search", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const upts[] = {
	"update", /* UP_MODIFY */
	"delete", /* UP_DELETE */
};

static	const char *const modtypes[MODTYPE__MAX] = {
	"set", /* MODTYPE_SET */
	"inc", /* MODTYPE_INC */
	"dec", /* MODTYPE_DEC */
};

static	const char *const ftypes[FTYPE__MAX] = {
	"bit", /* FTYPE_BIT */
	"date", /* FTYPE_DATE */
	"epoch", /* FTYPE_EPOCH */
	"int", /* FTYPE_INT */
	"real", /* FTYPE_REAL */
	"blob", /* FTYPE_BLOB */
	"text", /* FTYPE_TEXT */
	"password", /* FTYPE_PASSWORD */
	"email", /* FTYPE_EMAIL */
	"struct", /* FTYPE_STRUCT */
	"enum", /* FTYPE_ENUM */
	"bits", /* FTYPE_BITFIELD */
};

static 	const char *const upacts[UPACT__MAX] = {
	NULL, /* UPACT_NONE */
	"restrict", /* UPACT_RESTRICT */
	"nullify", /* UPACT_NULLIFY */
	"cascade", /* UPACT_CASCADE */
	"default", /* UPACT_DEFAULT */
};

static	const char *const vtypes[VALIDATE__MAX] = {
	"ge", /* VALIDATE_GE */
	"le", /* VALIDATE_LE */
	"gt", /* VALIDATE_GT */
	"lt", /* VALIDATE_LT */
	"eq", /* VALIDATE_EQ */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	"isnull", /* OPTYE_ISNULL */
	"notnull", /* OPTYE_NOTNULL */
};

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

enum	wtype {
	WTYPE_FILE,
	WTYPE_BUF
};

struct	wbuf {
	char		*buf;
	size_t		 len;
};

struct	writer {
	enum wtype	 type;
	union {
		FILE	*f;
		struct wbuf buf;
	};
};

static void
wprint(struct writer *w, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void
wputs(struct writer *w, const char *buf)
{
	size_t	 sz;

	if (WTYPE_FILE == w->type) {
		fputs(buf, w->f);
		return;
	}

	sz = strlen(buf);
	w->buf.buf = realloc
		(w->buf.buf, w->buf.len + sz + 1);
	if (NULL == w->buf.buf)
		err(EXIT_FAILURE, NULL);

	memcpy(w->buf.buf + w->buf.len, buf, sz);
	w->buf.len += sz;
	w->buf.buf[w->buf.len] = '\0';
}

static void
wputc(struct writer *w, char c)
{
	char	buf[] = { '\0', '\0' };

	buf[0] = c;
	wputs(w, buf);
}

static void
wprint(struct writer *w, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	if (WTYPE_FILE == w->type) {
		vfprintf(w->f, fmt, ap);
	}
	va_end(ap);
}

/*
 * Write a field/enumeration/whatever comment.
 */
static void
parse_write_comment(struct writer *w, const char *cp, size_t tabs)
{
	size_t	 i;

	if (NULL == cp)
		return;

	if (tabs > 1)
		wputc(w, '\n');
	for (i = 0; i < tabs; i++)
		wputc(w, '\t');

	wputs(w, "comment \"");
	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			wputc(w, *cp);
			cp++;
			continue;
		}
		wputc(w, ' ');
		while (isspace((unsigned char)*cp))
			cp++;
	}
	wputs(w, "\"");
}

/*
 * Write a structure field.
 */
static void
parse_write_field(struct writer *w, const struct field *p)
{
	const struct fvalid *fv;
	unsigned int	 fl;

	/* Name, type, refs. */

	wprint(w, "\tfield %s", p->name);
	if (NULL != p->ref && FTYPE_STRUCT != p->type)
		wprint(w, ":%s.%s", p->ref->tstrct, p->ref->tfield);
	wprint(w, " %s", ftypes[p->type]);
	if (NULL != p->ref && FTYPE_STRUCT == p->type)
		wprint(w, " %s", p->ref->sfield);

	if (FTYPE_ENUM == p->type)
		wprint(w, " %s", p->eref->ename);
	if (FTYPE_BITFIELD == p->type)
		wprint(w, " %s", p->bref->name);

	/* Flags. */

	fl = p->flags;
	if (FIELD_ROWID & fl) {
		wputs(w, " rowid");
		fl &= ~FIELD_ROWID;
	}
	if (FIELD_UNIQUE & fl) {
		wputs(w, " unique");
		fl &= ~FIELD_UNIQUE;
	}
	if (FIELD_NULL & fl) {
		wputs(w, " null");
		fl &= ~FIELD_NULL;
	}
	if (FIELD_NOEXPORT & fl) {
		wputs(w, " noexport");
		fl &= ~FIELD_NOEXPORT;
	}
	assert(0 == fl);

	/* Validations. */

	TAILQ_FOREACH(fv, &p->fvq, entries) 
		switch (p->type) {
		case (FTYPE_BIT):
		case (FTYPE_DATE):
		case (FTYPE_EPOCH):
		case (FTYPE_INT):
			wprint(w, " limit %s %" PRId64, 
				vtypes[fv->type], 
				fv->d.value.integer);
			break;
		case (FTYPE_REAL):
			wprint(w, " limit %s %g",
				vtypes[fv->type], 
				fv->d.value.decimal);
			break;
		case (FTYPE_BLOB):
		case (FTYPE_EMAIL):
		case (FTYPE_TEXT):
		case (FTYPE_PASSWORD):
			wprint(w, " limit %s %zu",
				vtypes[fv->type], 
				fv->d.value.len);
			break;
		default:
			abort();
		}

	/* Actions. */

	if (UPACT_NONE != p->actdel) 
		wprint(w, " actdel %s", upacts[p->actdel]);
	if (UPACT_NONE != p->actup) 
		wprint(w, " actup %s", upacts[p->actup]);

	/* Comments and close. */

	parse_write_comment(w, p->doc, 2);
	wprint(w, ";\n");
}

/*
 * Write a structure modifier.
 */
static void
parse_write_modify(struct writer *w, const struct update *p)
{
	const struct uref *u;
	size_t	 nf;

	wprint(w, "\t%s", upts[p->type]);

	if (UP_MODIFY == p->type) {
		nf = 0;
		TAILQ_FOREACH(u, &p->mrq, entries) {
			wprint(w, "%s %s", nf++ ? "," : "", u->name);
			if (MODTYPE_SET != u->mod)
				wprint(w, " %s", modtypes[u->mod]);
		}

		wputc(w, ':');
	}

	nf = 0;
	TAILQ_FOREACH(u, &p->crq, entries) {
		wprint(w, "%s %s", nf++ ? "," : "", u->name);
		if (OPTYPE_EQUAL != u->op)
			wprint(w, " %s", optypes[u->op]);
	}

	wputc(w, ':');

	if (NULL != p->name)
		wprint(w, " name %s", p->name);

	parse_write_comment(w, p->doc, 2);
	wputs(w, ";\n");
}

/*
 * Write a structure unique constraint.
 */
static void
parse_write_unique(struct writer *w, const struct unique *p)
{
	const struct nref *n;
	size_t	 nf = 0;

	wputs(w, "\tunique");

	TAILQ_FOREACH(n, &p->nq, entries)
		wprint(w, "%s %s", nf++ ? "," : "", n->name);

	wputs(w, ";\n");
}

/*
 * Write a structure query.
 */
static void
parse_write_query(struct writer *w, const struct search *p)
{
	const struct sent *s;
	const struct ord *o;
	size_t	 nf;

	wprint(w, "\t%s", stypes[p->type]);

	/* Search reference queue. */

	nf = 0;
	TAILQ_FOREACH(s, &p->sntq, entries) {
		wprint(w, "%s %s", nf++ ? "," : "", s->fname);
		if (OPTYPE_EQUAL != s->op)
			wprint(w, " %s", optypes[s->op]);
	}
	wputc(w, ':');

	if (NULL != p->name)
		wprint(w, " name %s", p->name);

	nf = 0;
	if (TAILQ_FIRST(&p->ordq))
		wputs(w, " order");
	TAILQ_FOREACH(o, &p->ordq, entries)
		wprint(w, "%s %s", nf++ ? "," : "", o->fname);

	if (p->limit)
		wprint(w, " limit %" PRId64, p->limit);
	if (p->offset)
		wprint(w, " offset %" PRId64, p->offset);
	if (NULL != p->dst)
		wprint(w, " distinct %s", p->dst->cname);

	parse_write_comment(w, p->doc, 2);

	wprint(w, ";\n");
}

/*
 * Print structure role assignments.
 */
static void
parse_write_rolemap(struct writer *w, const struct rolemap *p)
{
	const struct roleset *r;
	size_t	 nf = 0;

	wputs(w, "\troles");

	TAILQ_FOREACH(r, &p->setq, entries)
		wprint(w, "%s %s", nf++ ? "," : "", r->name);
	wprint(w, " { %s", rolemapts[p->type]);
	if (NULL != p->name)
		wprint(w, " %s", p->name);

	wputs(w, "; };\n");
}

/*
 * Write a top-level structure.
 */
static void
parse_write_strct(struct writer *w, const struct strct *p)
{
	const struct field   *fd;
	const struct search  *s;
	const struct update  *u;
	const struct unique  *n;
	const struct rolemap *r;

	wprint(w, "struct %s {\n", p->name);

	TAILQ_FOREACH(fd, &p->fq, entries)
		parse_write_field(w, fd);
	TAILQ_FOREACH(s, &p->sq, entries)
		parse_write_query(w, s);
	TAILQ_FOREACH(u, &p->uq, entries)
		parse_write_modify(w, u);
	TAILQ_FOREACH(u, &p->dq, entries)
		parse_write_modify(w, u);
	if (NULL != p->ins) 
		wputs(w, "\tinsert;\n");
	TAILQ_FOREACH(n, &p->nq, entries)
		parse_write_unique(w, n);
	TAILQ_FOREACH(r, &p->rq, entries) 
		parse_write_rolemap(w, r);

	if (NULL != p->doc) {
		parse_write_comment(w, p->doc, 1);
		wputs(w, ";\n");
	}

	wputs(w, "};\n\n");
}

/*
 * Write a per-language jslabel.
 */
static void
parse_write_label(struct writer *w, 
	const struct config *cfg, 
	const struct label *p, size_t tabs)
{
	const char *cp = p->label;

	if (p->lang)
		wprint(w, "\n%sjslabel.%s \"", tabs > 1 ? 
			"\t\t" : "\t", cfg->langs[p->lang]);
	else
		wprint(w, "\n%sjslabel \"", 
			tabs > 1 ? "\t\t" : "\t");

	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			wputc(w, *cp);
			cp++;
			continue;
		}
		wputc(w, ' ');
		while (isspace((unsigned char)*cp))
			cp++;
	}

	wputc(w, '"');
}

/*
 * Write a top-level bitfield. 
 */
static void
parse_write_bitf(struct writer *w, 
	const struct config *cfg, const struct bitf *p)
{
	const struct bitidx *b;
	const struct label  *l;

	wprint(w, "bitfield %s {\n", p->name);

	TAILQ_FOREACH(b, &p->bq, entries) {
		wprint(w, "\titem %s %" PRId64, b->name, b->value);
		TAILQ_FOREACH(l, &b->labels, entries)
			parse_write_label(w, cfg, l, 2);
		parse_write_comment(w, b->doc, 2);
		wputs(w, ";\n");
	}

	if (TAILQ_FIRST(&p->labels_unset)) {
		wputs(w, "\tisunset");
		TAILQ_FOREACH(l, &p->labels_unset, entries)
			parse_write_label(w, cfg, l, 2);
		wputs(w, ";\n");
	}

	if (TAILQ_FIRST(&p->labels_null)) {
		wputs(w, "\tisnull");
		TAILQ_FOREACH(l, &p->labels_null, entries)
			parse_write_label(w, cfg, l, 2);
		wputs(w, ";\n");
	}

	if (NULL != p->doc) {
		parse_write_comment(w, p->doc, 1);
		wputs(w, ";\n");
	}

	wputs(w, "};\n\n");
}

/*
 * Write a top-level enumeration.
 */
static void
parse_write_enm(struct writer *w, 
	const struct config *cfg, const struct enm *p)
{
	const struct eitem *e;
	const struct label *l;

	wprint(w, "enum %s {\n", p->name);

	TAILQ_FOREACH(e, &p->eq, entries) {
		wprint(w, "\titem %s", e->name);
		if ( ! (ENM_AUTO & e->flags))
			wprint(w, " %" PRId64, e->value);
		TAILQ_FOREACH(l, &e->labels, entries)
			parse_write_label(w, cfg, l, 2);
		parse_write_comment(w, e->doc, 2);
		wputs(w, ";\n");
	}

	if (NULL != p->doc) {
		parse_write_comment(w, p->doc, 1);
		wputs(w, ";\n");
	}

	wputs(w, "};\n\n");
}

/*
 * Write individual role declarations.
 */
static void
parse_write_role(struct writer *w, const struct role *r, size_t tabs)
{
	size_t	 i;
	const struct role *rr;

	for (i = 0; i < tabs; i++)
		wputc(w, '\t');

	wprint(w, "role %s", r->name);

	if (NULL != r->doc) 
		parse_write_comment(w, r->doc, tabs + 1);

	if (TAILQ_FIRST(&r->subrq)) {
		wputs(w, " {\n");
		TAILQ_FOREACH(rr, &r->subrq, entries)
			parse_write_role(w, rr, tabs + 1);
		for (i = 0; i < tabs; i++)
			wputc(w, '\t');
		wputs(w, "};\n");
	} else
		wputs(w, ";\n");

}

/*
 * Write the top-level role block.
 */
static void
parse_write_roles(struct writer *w, const struct config *cfg)
{
	const struct role *r, *rr;

	wputs(w, "roles {\n");
	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (0 == strcmp(r->name, "all"))
			TAILQ_FOREACH(rr, &r->subrq, entries) 
				parse_write_role(w, rr, 1);
	wputs(w, "};\n\n");
}

void
kwbp_write_file(FILE *f, const struct config *cfg)
{
	const struct strct *s;
	const struct enm   *e;
	const struct bitf  *b;
	struct writer	    w;

	memset(&w, 0, sizeof(struct writer));
	w.type = WTYPE_FILE;
	w.f = f;

	if (CFG_HAS_ROLES & cfg->flags)
		parse_write_roles(&w, cfg);
	TAILQ_FOREACH(s, &cfg->sq, entries)
		parse_write_strct(&w, s);
	TAILQ_FOREACH(e, &cfg->eq, entries)
		parse_write_enm(&w, cfg, e);
	TAILQ_FOREACH(b, &cfg->bq, entries)
		parse_write_bitf(&w, cfg, b);

}
