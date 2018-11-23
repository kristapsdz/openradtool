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
	WTYPE_FILE, /* FILE stream output */
	WTYPE_BUF /* buffer output */
};

struct	wbuf {
	char		*buf; /* NUL-terminated buffer */
	size_t		 len; /* length of buffer w/o NUL */
};

struct	writer {
	enum wtype	 type; /* type of output */
	union {
		FILE	*f; /* WTYPE_FILE */
		struct wbuf buf; /* WTYPE_BUF */
	};
};

static int
wprint(struct writer *w, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

/*
 * Like fputs(3).
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
wputs(struct writer *w, const char *buf)
{
	size_t	 sz;
	void	*pp;

	if (WTYPE_FILE == w->type) 
		return EOF != fputs(buf, w->f);

	sz = strlen(buf);
	pp = realloc(w->buf.buf, w->buf.len + sz + 1);
	if (NULL == pp)
		return 0;
	w->buf.buf = pp;

	memcpy(w->buf.buf + w->buf.len, buf, sz);
	w->buf.len += sz;
	w->buf.buf[w->buf.len] = '\0';
	return 1;
}

/*
 * Like fputc(3).
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
wputc(struct writer *w, char c)
{
	char	buf[] = { '\0', '\0' };

	buf[0] = c;
	return wputs(w, buf);
}

/*
 * Like fprintf(3).
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
wprint(struct writer *w, const char *fmt, ...)
{
	va_list	 ap;
	int	 len;
	void	*pp;

	if (WTYPE_BUF == w->type) {
		va_start(ap, fmt);
		len = vsnprintf(NULL, 0, fmt, ap);
		va_end(ap);

		if (len < 0)
			return 0;
		pp = realloc(w->buf.buf, w->buf.len + len + 1);
		if (NULL == pp)
			return 0;
		w->buf.buf = pp;

		va_start(ap, fmt);
		vsnprintf(w->buf.buf + w->buf.len,
			len + 1, fmt, ap);
		va_end(ap);

		w->buf.len += len;
		w->buf.buf[w->buf.len] = '\0';
	} else {
		va_start(ap, fmt);
		len = vfprintf(w->f, fmt, ap);
		va_end(ap);
		if (len < 0)
			return 0;
	}

	return 1;
}

/*
 * Write a field/enumeration/whatever comment.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_comment(struct writer *w, const char *cp, size_t tabs)
{
	size_t	 i;

	if (NULL == cp)
		return 1;

	if (tabs > 1)
		if ( ! wputc(w, '\n'))
			return 0;
	for (i = 0; i < tabs; i++)
		if ( ! wputc(w, '\t'))
			return 0;

	if ( ! wputs(w, "comment \""))
		return 0;

	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			if ( ! wputc(w, *cp))
				return 0;
			cp++;
			continue;
		}
		if ( ! wputc(w, ' '))
			return 0;
		while (isspace((unsigned char)*cp))
			cp++;
	}

	return wputs(w, "\"");
}

/*
 * Write a structure field.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_field(struct writer *w, const struct field *p)
{
	const struct fvalid *fv;
	unsigned int	 fl;
	int		 rc;

	/* Name, type, refs. */

	if ( ! wprint(w, "\tfield %s", p->name))
		return 0;
	if (NULL != p->ref && FTYPE_STRUCT != p->type)
		if ( ! wprint(w, ":%s.%s", 
		    p->ref->tstrct, p->ref->tfield))
			return 0;
	if ( ! wprint(w, " %s", ftypes[p->type]))
		return 0;
	if (NULL != p->ref && FTYPE_STRUCT == p->type)
		if ( ! wprint(w, " %s", p->ref->sfield))
			return 0;

	if (FTYPE_ENUM == p->type)
		if ( ! wprint(w, " %s", p->eref->ename))
			return 0;
	if (FTYPE_BITFIELD == p->type)
		if ( ! wprint(w, " %s", p->bref->name))
			return 0;

	/* Flags. */

	fl = p->flags;
	if (FIELD_ROWID & fl) {
		if ( ! wputs(w, " rowid"))
			return 0;
		fl &= ~FIELD_ROWID;
	}
	if (FIELD_UNIQUE & fl) {
		if ( ! wputs(w, " unique"))
			return 0;
		fl &= ~FIELD_UNIQUE;
	}
	if (FIELD_NULL & fl) {
		if ( ! wputs(w, " null"))
			return 0;
		fl &= ~FIELD_NULL;
	}
	if (FIELD_NOEXPORT & fl) {
		if ( ! wputs(w, " noexport"))
			return 0;
		fl &= ~FIELD_NOEXPORT;
	}
	if (FIELD_HASDEF & fl) {
		switch (p->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			rc = wprint(w, " default %" PRId64,
				p->def.integer);
			break;
		case FTYPE_REAL:
			rc = wprint(w, " default %g",
				p->def.decimal);
			break;
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
			rc = wprint(w, " default \"%s\"",
				p->def.string);
			break;
		default:
			abort();
			break;
		}
		if (0 == rc)
			return 0;
		fl &= ~FIELD_HASDEF;
	}

	assert(0 == fl);

	/* Validations. */

	TAILQ_FOREACH(fv, &p->fvq, entries) 
		switch (p->type) {
		case (FTYPE_BIT):
		case (FTYPE_DATE):
		case (FTYPE_EPOCH):
		case (FTYPE_INT):
			if ( ! wprint(w, " limit %s %" PRId64, 
			    vtypes[fv->type], fv->d.value.integer))
				return 0;
			break;
		case (FTYPE_REAL):
			if ( ! wprint(w, " limit %s %g",
			    vtypes[fv->type], fv->d.value.decimal))
				return 0;
			break;
		case (FTYPE_BLOB):
		case (FTYPE_EMAIL):
		case (FTYPE_TEXT):
		case (FTYPE_PASSWORD):
			if ( ! wprint(w, " limit %s %zu",
			    vtypes[fv->type], fv->d.value.len))
				return 0;
			break;
		default:
			abort();
		}

	/* Actions. */

	if (UPACT_NONE != p->actdel) 
		if ( ! wprint(w, " actdel %s", upacts[p->actdel]))
			return 0;
	if (UPACT_NONE != p->actup) 
		if ( ! wprint(w, " actup %s", upacts[p->actup]))
			return 0;

	/* Comments and close. */

	if ( ! parse_write_comment(w, p->doc, 2))
		return 0;

	return wprint(w, ";\n");
}

/*
 * Write a structure modifier.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_modify(struct writer *w, const struct update *p)
{
	const struct uref *u;
	size_t	 nf;

	if ( ! wprint(w, "\t%s", upts[p->type]))
		return 0;

	if (UP_MODIFY == p->type) {
		nf = 0;
		TAILQ_FOREACH(u, &p->mrq, entries) {
			if ( ! wprint(w, "%s %s", 
			    nf++ ? "," : "", u->name))
				return 0;
			if (MODTYPE_SET != u->mod)
				if ( ! wprint(w, " %s", 
				    modtypes[u->mod]))
					return 0;
		}
		if ( ! wputc(w, ':'))
			return 0;
	}

	nf = 0;
	TAILQ_FOREACH(u, &p->crq, entries) {
		if ( ! wprint(w, "%s %s", 
		    nf++ ? "," : "", u->name))
			return 0;
		if (OPTYPE_EQUAL != u->op)
			if ( ! wprint(w, " %s", optypes[u->op]))
				return 0;
	}

	if ( ! wputc(w, ':'))
		return 0;

	if (NULL != p->name)
		if ( ! wprint(w, " name %s", p->name))
			return 0;

	if ( ! parse_write_comment(w, p->doc, 2))
		return 0;

	return wputs(w, ";\n");
}

/*
 * Write a structure unique constraint.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_unique(struct writer *w, const struct unique *p)
{
	const struct nref *n;
	size_t	 nf = 0;

	if ( ! wputs(w, "\tunique"))
		return 0;

	TAILQ_FOREACH(n, &p->nq, entries)
		if ( ! wprint(w, "%s %s", nf++ ? "," : "", n->name))
			return 0;

	return wputs(w, ";\n");
}

/*
 * Write a structure query.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_query(struct writer *w, const struct search *p)
{
	const struct sent *s;
	const struct ord *o;
	size_t	 nf;

	if ( ! wprint(w, "\t%s", stypes[p->type]))
		return 0;

	/* Search reference queue. */

	nf = 0;
	TAILQ_FOREACH(s, &p->sntq, entries) {
		if ( ! wprint(w, "%s %s", nf++ ? "," : "", s->fname))
			return 0;
		if (OPTYPE_EQUAL != s->op)
			if ( ! wprint(w, " %s", optypes[s->op]))
				return 0;
	}

	if ( ! wputc(w, ':'))
		return 0;

	if (NULL != p->name)
		if ( ! wprint(w, " name %s", p->name))
			return 0;

	nf = 0;
	if (TAILQ_FIRST(&p->ordq))
		if ( ! wputs(w, " order"))
			return 0;
	TAILQ_FOREACH(o, &p->ordq, entries)
		if ( ! wprint(w, "%s %s", nf++ ? "," : "", o->fname))
			return 0;

	if (p->limit)
		if ( ! wprint(w, " limit %" PRId64, p->limit))
			return 0;
	if (p->offset)
		if ( ! wprint(w, " offset %" PRId64, p->offset))
			return 0;
	if (NULL != p->dst)
		if ( ! wprint(w, " distinct %s", p->dst->cname))
			return 0;

	if ( ! parse_write_comment(w, p->doc, 2))
		return 0;

	return wprint(w, ";\n");
}

/*
 * Print structure role assignments.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_rolemap(struct writer *w, const struct rolemap *p)
{
	const struct roleset *r;
	size_t	 nf = 0;

	if ( ! wputs(w, "\troles"))
		return 0;

	TAILQ_FOREACH(r, &p->setq, entries)
		if ( ! wprint(w, "%s %s", nf++ ? "," : "", r->name))
			return 0;
	if ( ! wprint(w, " { %s", rolemapts[p->type]))
		return 0;
	if (NULL != p->name)
		if ( ! wprint(w, " %s", p->name))
			return 0;

	return wputs(w, "; };\n");
}

/*
 * Write a top-level structure.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_strct(struct writer *w, const struct strct *p)
{
	const struct field   *fd;
	const struct search  *s;
	const struct update  *u;
	const struct unique  *n;
	const struct rolemap *r;

	if ( ! wprint(w, "struct %s {\n", p->name))
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if ( ! parse_write_field(w, fd))
			return 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if ( ! parse_write_query(w, s))
			return 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		if ( ! parse_write_modify(w, u))
			return 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		if ( ! parse_write_modify(w, u))
			return 0;
	if (NULL != p->ins) 
		if ( ! wputs(w, "\tinsert;\n"))
			return 0;
	TAILQ_FOREACH(n, &p->nq, entries)
		if ( ! parse_write_unique(w, n))
			return 0;
	TAILQ_FOREACH(r, &p->rq, entries) 
		if ( ! parse_write_rolemap(w, r))
			return 0;

	if (NULL != p->doc) {
		if ( ! parse_write_comment(w, p->doc, 1))
			return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	return wputs(w, "};\n\n");
}

/*
 * Write a per-language jslabel.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_label(struct writer *w, 
	const struct config *cfg, 
	const struct label *p, size_t tabs)
{
	const char *cp = p->label;
	int	 rc;

	rc = p->lang ?
		wprint(w, "\n%sjslabel.%s \"", tabs > 1 ? 
			"\t\t" : "\t", cfg->langs[p->lang]) :
		wprint(w, "\n%sjslabel \"", 
			tabs > 1 ? "\t\t" : "\t");

	if ( ! rc)
		return 0;

	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			if ( ! wputc(w, *cp))
				return 0;
			cp++;
			continue;
		}
		if ( ! wputc(w, ' '))
			return 0;
		while (isspace((unsigned char)*cp))
			cp++;
	}

	return wputc(w, '"');
}

/*
 * Write a top-level bitfield. 
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_bitf(struct writer *w, 
	const struct config *cfg, const struct bitf *p)
{
	const struct bitidx *b;
	const struct label  *l;

	if ( ! wprint(w, "bitfield %s {\n", p->name))
		return 0;

	TAILQ_FOREACH(b, &p->bq, entries) {
		if ( ! wprint(w, "\titem %s %" 
		    PRId64, b->name, b->value))
			return 0;
		TAILQ_FOREACH(l, &b->labels, entries)
			if ( ! parse_write_label(w, cfg, l, 2))
				return 0;
		if ( ! parse_write_comment(w, b->doc, 2))
			return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	if (TAILQ_FIRST(&p->labels_unset)) {
		if ( ! wputs(w, "\tisunset"))
			return 0;
		TAILQ_FOREACH(l, &p->labels_unset, entries)
			if ( ! parse_write_label(w, cfg, l, 2))
				return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	if (TAILQ_FIRST(&p->labels_null)) {
		if ( ! wputs(w, "\tisnull"))
			return 0;
		TAILQ_FOREACH(l, &p->labels_null, entries)
			if ( ! parse_write_label(w, cfg, l, 2))
				return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	if (NULL != p->doc) {
		if ( ! parse_write_comment(w, p->doc, 1))
			return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	return wputs(w, "};\n\n");
}

/*
 * Write a top-level enumeration.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_enm(struct writer *w, 
	const struct config *cfg, const struct enm *p)
{
	const struct eitem *e;
	const struct label *l;

	if ( ! wprint(w, "enum %s {\n", p->name))
		return 0;

	TAILQ_FOREACH(e, &p->eq, entries) {
		if ( ! wprint(w, "\titem %s", e->name))
			return 0;
		if ( ! (ENM_AUTO & e->flags))
			if ( ! wprint(w, " %" PRId64, e->value))
				return 0;
		TAILQ_FOREACH(l, &e->labels, entries)
			if ( ! parse_write_label(w, cfg, l, 2))
				return 0;
		if ( ! parse_write_comment(w, e->doc, 2))
			return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	if (NULL != p->doc) {
		if ( ! parse_write_comment(w, p->doc, 1))
			return 0;
		if ( ! wputs(w, ";\n"))
			return 0;
	}

	return wputs(w, "};\n\n");
}

/*
 * Write individual role declarations.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_role(struct writer *w, 
	const struct role *r, size_t tabs)
{
	size_t	 i;
	const struct role *rr;

	for (i = 0; i < tabs; i++)
		if ( ! wputc(w, '\t'))
			return 0;

	if ( ! wprint(w, "role %s", r->name))
		return 0;

	if (NULL != r->doc &&
	    ! parse_write_comment(w, r->doc, tabs + 1))
		return 0;

	if (TAILQ_FIRST(&r->subrq)) {
		if ( ! wputs(w, " {\n"))
			return 0;
		TAILQ_FOREACH(rr, &r->subrq, entries)
			if ( ! parse_write_role(w, rr, tabs + 1))
				return 0;
		for (i = 0; i < tabs; i++)
			if ( ! wputc(w, '\t'))
				return 0;
		if ( ! wputc(w, '}'))
			return 0;
	} 

	return wputs(w, ";\n");
}

/*
 * Write the top-level role block.
 * Returns zero on failure (memory), non-zero otherwise.
 */
static int
parse_write_roles(struct writer *w, const struct config *cfg)
{
	const struct role *r, *rr;

	if ( ! wputs(w, "roles {\n"))
		return 0;

	TAILQ_FOREACH(r, &cfg->rq, entries) {
		if (strcmp(r->name, "all"))
			continue;
		TAILQ_FOREACH(rr, &r->subrq, entries) 
			if ( ! parse_write_role(w, rr, 1))
				return 0;
	}

	return wputs(w, "};\n\n");
}

int
kwbp_write_file(FILE *f, const struct config *cfg)
{
	const struct strct *s;
	const struct enm   *e;
	const struct bitf  *b;
	struct writer	    w;
	int		    rc = 0;

	memset(&w, 0, sizeof(struct writer));
	w.type = WTYPE_FILE;
	w.f = f;

	if (CFG_HAS_ROLES & cfg->flags)
		if ( ! parse_write_roles(&w, cfg))
			goto out;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		parse_write_strct(&w, s);
	TAILQ_FOREACH(e, &cfg->eq, entries)
		parse_write_enm(&w, cfg, e);
	TAILQ_FOREACH(b, &cfg->bq, entries)
		parse_write_bitf(&w, cfg, b);

	rc = 1;
out:
	free(w.buf.buf);
	return rc;
}

char *
kwbp_write_buf(const struct config *cfg)
{
	const struct strct *s;
	const struct enm   *e;
	const struct bitf  *b;
	struct writer	    w;

	memset(&w, 0, sizeof(struct writer));
	w.type = WTYPE_BUF;

	if (CFG_HAS_ROLES & cfg->flags)
		if ( ! parse_write_roles(&w, cfg))
			goto out;
	TAILQ_FOREACH(s, &cfg->sq, entries)
		parse_write_strct(&w, s);
	TAILQ_FOREACH(e, &cfg->eq, entries)
		parse_write_enm(&w, cfg, e);
	TAILQ_FOREACH(b, &cfg->bq, entries)
		parse_write_bitf(&w, cfg, b);

	return w.buf.buf;
out:
	free(w.buf.buf);
	return NULL;
}
