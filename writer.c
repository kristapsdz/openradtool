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

/*
 * Write a field/enumeration/whatever comment.
 */
static void
parse_write_comment(FILE *f, const char *cp, size_t tabs)
{
	size_t	 i;

	if (NULL == cp)
		return;

	if (tabs > 1)
		fputc('\n', f);
	for (i = 0; i < tabs; i++)
		fputc('\t', f);

	fputs("comment \"", f);
	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			fputc((unsigned char)*cp, f);
			cp++;
			continue;
		}
		fputc(' ', f);
		while (isspace((unsigned char)*cp))
			cp++;
	}
	fputs("\"", f);
}

/*
 * Write a structure field.
 */
static void
parse_write_field(FILE *f, const struct field *p)
{
	const struct fvalid *fv;

	/* Name, type, refs. */

	fprintf(f, "\tfield %s", p->name);
	if (NULL != p->ref && FTYPE_STRUCT != p->type)
		fprintf(f, ":%s.%s", p->ref->tstrct, p->ref->tfield);
	fprintf(f, " %s", ftypes[p->type]);
	if (NULL != p->ref && FTYPE_STRUCT == p->type)
		fprintf(f, " %s", p->ref->sfield);

	if (FTYPE_ENUM == p->type)
		fprintf(f, " %s", p->eref->ename);
	if (FTYPE_BITFIELD == p->type)
		fprintf(f, " %s", p->bref->name);

	/* Flags. */

	if (FIELD_ROWID & p->flags)
		fputs(" rowid", f);
	if (FIELD_UNIQUE & p->flags)
		fputs(" unique", f);
	if (FIELD_NULL & p->flags)
		fputs(" null", f);
	if (FIELD_NOEXPORT & p->flags)
		fputs(" noexport", f);

	/* Validations. */

	TAILQ_FOREACH(fv, &p->fvq, entries) 
		switch (p->type) {
		case (FTYPE_BIT):
		case (FTYPE_DATE):
		case (FTYPE_EPOCH):
		case (FTYPE_INT):
			fprintf(f, " limit %s %" PRId64, 
				vtypes[fv->type], 
				fv->d.value.integer);
			break;
		case (FTYPE_REAL):
			fprintf(f, " limit %s %g",
				vtypes[fv->type], 
				fv->d.value.decimal);
			break;
		case (FTYPE_BLOB):
		case (FTYPE_EMAIL):
		case (FTYPE_TEXT):
		case (FTYPE_PASSWORD):
			fprintf(f, " limit %s %zu",
				vtypes[fv->type], 
				fv->d.value.len);
			break;
		default:
			abort();
		}

	/* Actions. */

	if (UPACT_NONE != p->actdel) 
		fprintf(f, " actdel %s", upacts[p->actdel]);
	if (UPACT_NONE != p->actup) 
		fprintf(f, " actup %s", upacts[p->actup]);

	/* Comments and close. */

	parse_write_comment(f, p->doc, 2);
	fprintf(f, ";\n");
}

/*
 * Write a structure modifier.
 */
static void
parse_write_modify(FILE *f, const struct update *p)
{
	const struct uref *u;
	size_t	 nf;

	fprintf(f, "\t%s", upts[p->type]);

	if (UP_MODIFY == p->type) {
		nf = 0;
		TAILQ_FOREACH(u, &p->mrq, entries) {
			fprintf(f, "%s %s", nf++ ? "," : "", u->name);
			if (MODTYPE_SET != u->mod)
				fprintf(f, " %s", modtypes[u->mod]);
		}

		fputs(":", f);
	}

	nf = 0;
	TAILQ_FOREACH(u, &p->crq, entries) {
		fprintf(f, "%s %s", nf++ ? "," : "", u->name);
		if (OPTYPE_EQUAL != u->op)
			fprintf(f, " %s", optypes[u->op]);
	}

	fputs(":", f);

	if (NULL != p->name)
		fprintf(f, " name %s", p->name);

	parse_write_comment(f, p->doc, 2);
	fprintf(f, ";\n");
}

/*
 * Write a structure unique constraint.
 */
static void
parse_write_unique(FILE *f, const struct unique *p)
{
	const struct nref *n;
	size_t	 nf = 0;

	fputs("\tunique", f);

	TAILQ_FOREACH(n, &p->nq, entries)
		fprintf(f, "%s %s", nf++ ? "," : "", n->name);

	fprintf(f, ";\n");
}

/*
 * Write a structure query.
 */
static void
parse_write_query(FILE *f, const struct search *p)
{
	const struct sent *s;
	const struct ord *o;
	size_t	 nf;

	fprintf(f, "\t%s", stypes[p->type]);

	/* Search reference queue. */

	nf = 0;
	TAILQ_FOREACH(s, &p->sntq, entries) {
		fprintf(f, "%s %s", nf++ ? "," : "", s->fname);
		if (OPTYPE_EQUAL != s->op)
			fprintf(f, " %s", optypes[s->op]);
	}
	fputs(":", f);

	if (NULL != p->name)
		fprintf(f, " name %s", p->name);

	nf = 0;
	if (TAILQ_FIRST(&p->ordq))
		fputs(" order", f);
	TAILQ_FOREACH(o, &p->ordq, entries)
		fprintf(f, "%s %s", nf++ ? "," : "", o->fname);

	if (p->limit)
		fprintf(f, " limit %" PRId64, p->limit);
	if (p->offset)
		fprintf(f, " offset %" PRId64, p->offset);
	if (NULL != p->dst)
		fprintf(f, " distinct %s", p->dst->cname);

	parse_write_comment(f, p->doc, 2);
	fprintf(f, ";\n");
}

/*
 * Print structure role assignments.
 */
static void
parse_write_rolemap(FILE *f, const struct rolemap *p)
{
	const struct roleset *r;
	size_t	 nf = 0;

	fputs("\troles", f);
	TAILQ_FOREACH(r, &p->setq, entries)
		fprintf(f, "%s %s", nf++ ? "," : "", r->name);
	fprintf(f, " { %s", rolemapts[p->type]);
	if (NULL != p->name)
		fprintf(f, " %s", p->name);
	fputs("; };\n", f);
}

/*
 * Write a top-level structure.
 */
static void
parse_write_strct(FILE *f, const struct strct *p)
{
	const struct field   *fd;
	const struct search  *s;
	const struct update  *u;
	const struct unique  *n;
	const struct rolemap *r;

	fprintf(f, "struct %s {\n", p->name);

	TAILQ_FOREACH(fd, &p->fq, entries)
		parse_write_field(f, fd);
	TAILQ_FOREACH(s, &p->sq, entries)
		parse_write_query(f, s);
	TAILQ_FOREACH(u, &p->uq, entries)
		parse_write_modify(f, u);
	TAILQ_FOREACH(u, &p->dq, entries)
		parse_write_modify(f, u);
	if (NULL != p->ins) 
		fputs("\tinsert;\n", f);
	TAILQ_FOREACH(n, &p->nq, entries)
		parse_write_unique(f, n);
	TAILQ_FOREACH(r, &p->rq, entries) 
		parse_write_rolemap(f, r);

	if (NULL != p->doc) {
		parse_write_comment(f, p->doc, 1);
		fputs(";\n", f);
	}
	fputs("};\n\n", f);
}

/*
 * Write a per-language jslabel.
 */
static void
parse_write_label(FILE *f, const struct config *cfg, 
	const struct label *p, size_t tabs)
{
	const char *cp = p->label;

	if (p->lang)
		fprintf(f, "\n%sjslabel.%s \"", tabs > 1 ? 
			"\t\t" : "\t", cfg->langs[p->lang]);
	else
		fprintf(f, "\n%sjslabel \"", 
			tabs > 1 ? "\t\t" : "\t");

	while ('\0' != *cp) {
		if ( ! isspace((unsigned char)*cp)) {
			fputc((unsigned char)*cp, f);
			cp++;
			continue;
		}
		fputc(' ', f);
		while (isspace((unsigned char)*cp))
			cp++;
	}
	fputs("\"", f);
}

/*
 * Write a top-level bitfield. 
 */
static void
parse_write_bitf(FILE *f, 
	const struct config *cfg, const struct bitf *p)
{
	const struct bitidx *b;
	const struct label  *l;

	fprintf(f, "bitfield %s {\n", p->name);

	TAILQ_FOREACH(b, &p->bq, entries) {
		fprintf(f, "\titem %s %" PRId64, b->name, b->value);
		TAILQ_FOREACH(l, &b->labels, entries)
			parse_write_label(f, cfg, l, 2);
		parse_write_comment(f, b->doc, 2);
		fputs(";\n", f);
	}

	if (TAILQ_FIRST(&p->labels_unset)) {
		fputs("\tisunset", f);
		TAILQ_FOREACH(l, &p->labels_unset, entries)
			parse_write_label(f, cfg, l, 2);
		fputs(";\n", f);
	}

	if (TAILQ_FIRST(&p->labels_null)) {
		fputs("\tisnull", f);
		TAILQ_FOREACH(l, &p->labels_null, entries)
			parse_write_label(f, cfg, l, 2);
		fputs(";\n", f);
	}

	if (NULL != p->doc) {
		parse_write_comment(f, p->doc, 1);
		fputs(";\n", f);
	}
	fputs("};\n\n", f);
}

/*
 * Write a top-level enumeration.
 */
static void
parse_write_enm(FILE *f, 
	const struct config *cfg, const struct enm *p)
{
	const struct eitem *e;
	const struct label *l;

	fprintf(f, "enum %s {\n", p->name);

	TAILQ_FOREACH(e, &p->eq, entries) {
		fprintf(f, "\titem %s", e->name);
		if ( ! (ENM_AUTO & e->flags))
			fprintf(f, " %" PRId64, e->value);
		TAILQ_FOREACH(l, &e->labels, entries)
			parse_write_label(f, cfg, l, 2);
		parse_write_comment(f, e->doc, 2);
		fputs(";\n", f);
	}

	if (NULL != p->doc) {
		parse_write_comment(f, p->doc, 1);
		fputs(";\n", f);
	}
	fputs("};\n\n", f);
}

/*
 * Write individual role declarations.
 */
static void
parse_write_role(FILE *f, const struct role *r, size_t tabs)
{
	size_t	 i;
	const struct role *rr;

	for (i = 0; i < tabs; i++)
		fputc('\t', f);
	fprintf(f, "role %s", r->name);
	if (NULL != r->doc) 
		parse_write_comment(f, r->doc, tabs + 1);

	if (TAILQ_FIRST(&r->subrq)) {
		fputs(" {\n", f);
		TAILQ_FOREACH(rr, &r->subrq, entries)
			parse_write_role(f, rr, tabs + 1);
		for (i = 0; i < tabs; i++)
			fputc('\t', f);
		fputs("};\n", f);
	} else
		fputs(";\n", f);

}

/*
 * Write the top-level role block.
 */
static void
parse_write_roles(FILE *f, const struct config *cfg)
{
	const struct role *r, *rr;

	fputs("roles {\n", f);
	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (0 == strcmp(r->name, "all"))
			TAILQ_FOREACH(rr, &r->subrq, entries) 
				parse_write_role(f, rr, 1);
	fputs("};\n\n", f);
}

void
parse_write(FILE *f, const struct config *cfg)
{
	const struct strct *s;
	const struct enm   *e;
	const struct bitf  *b;

	if (CFG_HAS_ROLES & cfg->flags)
		parse_write_roles(f, cfg);
	TAILQ_FOREACH(s, &cfg->sq, entries)
		parse_write_strct(f, s);
	TAILQ_FOREACH(e, &cfg->eq, entries)
		parse_write_enm(f, cfg, e);
	TAILQ_FOREACH(b, &cfg->bq, entries)
		parse_write_bitf(f, cfg, b);

}
