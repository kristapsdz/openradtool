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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "lang.h"
#include "ort-lang-json.h"

static const char *const ftypes[FTYPE__MAX] = {
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
	"bitfield", /* FTYPE_BITFIELD */
};

static const char *const vtypes[VALIDATE__MAX] = {
	"ge", /* VALIDATE_GE */
	"le", /* VALIDATE_LE */
	"gt", /* VALIDATE_GT */
	"lt", /* VALIDATE_LT */
	"eq", /* VALIDATE_EQ */
};

static const char *const upacts[UPACT__MAX] = {
	"none", /* UPACT_NONE */
	"restrict", /* UPACT_RESTRICT */
	"nullify", /* UPACT_NULLIFY */
	"cascade", /* UPACT_CASCADE */
	"default", /* UPACT_DEFAULT */
};

static int
gen_ws(FILE *f, size_t sz)
{
	size_t	 i;

	for (i = 0; i < sz; i++)
		if (fputc('\t', f) == EOF)
			return 0;
	return 1;
}

/*
 * Format the string according to the JSON specification.
 * Note about escaping the double-quote: in ort(3), double-quotes are
 * already escaped, so we need not do so again here.
 * Return zero on failure, non-zero on sucess.
 */
static int
gen_string(FILE *f, const char *cp)
{
	int	 rc;

	if (fputc('\"', f) == EOF)
		return 0;
	for ( ; *cp != '\0'; cp++) {
		switch (*cp) {
		case '\b':
			rc = fputs("\\b", f) != EOF;
			break;
		case '\f':
			rc = fputs("\\f", f) != EOF;
			break;
		case '\r':
			rc = fputs("\\r", f) != EOF;
			break;
		case '\n':
			rc = fputs("\\n", f) != EOF;
			break;
		case '\t':
			rc = fputs("\\t", f) != EOF;
			break;
		default:
			if (*cp < ' ')
				rc = fprintf(f, "\\u%.4u", *cp) > 0;
			else
				rc = fputc(*cp, f) != EOF;
		}
		if (!rc)
			return 0;
	}
	return fputc('\"', f) != EOF;
}

static int
gen_rolemap(FILE *f, const struct rolemap *map)
{
	const struct rref	*ref;

	if (fputs("\"rolemap\": ", f) == EOF)
		return 0;
	if (map == NULL)
		return fputs("null", f) != EOF;
	if (fputc('[', f) == EOF)
		return 0;
	TAILQ_FOREACH(ref, &map->rq, entries) {
		if (fprintf(f, "\"%s\"", ref->role->name) < 0)
			return 0;
		if (TAILQ_NEXT(ref, entries) != NULL &&
		    fputs(", ", f) == EOF)
			return 0;
	}
	return fputc(']', f) != EOF;
}

static int
gen_pos(FILE *f, size_t tabs, const struct pos *pos)
{

	assert(pos != NULL);
	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"pos\": {\n", f) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\"fname\": ", f) == EOF)
		return 0;
	if (!gen_string(f, pos->fname))
		return 0;
	if (fputs(",\n", f) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fprintf(f, "\"line\": %zu,\n", pos->line) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fprintf(f, "\"column\": %zu\n", pos->column) == EOF)
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	return fputs("},\n", f) != EOF;
}

static int
gen_label(FILE *f, size_t tabs, const char *name,
	const struct label *l, const struct config *cfg)
{

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"%s\": {\n", name) < 0)
		return 0;
	if (!gen_pos(f, tabs + 1, &l->pos))
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\"value\": ", f) == EOF)
		return 0;
	if (!gen_string(f, l->label))
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	return fputs("}", f) != EOF;
}

static int
gen_labelq(FILE *f, size_t tabs, const char *name,
	const struct labelq *q, const struct config *cfg)
{
	const struct label	*l;
	const char		*lang;

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"%s\": ", name) < 0)
		return 0;
	if (TAILQ_EMPTY(q))
		return fputs("null,\n", f) != EOF;

	if (fputs("{\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(l, q, entries) {
		lang = cfg->langs[l->lang][0] == '\0' ?
			"_default" : cfg->langs[l->lang];
		if (!gen_label(f, tabs + 1, lang, l, cfg))
			return 0;
		if (TAILQ_NEXT(l, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (!gen_ws(f, tabs))
		return 0;
	return fputs("},\n", f) != EOF;
}

static int
gen_doc(FILE *f, size_t tabs, const char *doc)
{

	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"doc\": ", f) == EOF)
		return 0;
	if (doc == NULL)
		return fputs("null,\n", f) != EOF;
	if (!gen_string(f, doc))
		return 0;
	return fputs(",\n", f) != EOF;
}

static int
gen_eitem(FILE *f, const struct eitem *ei, const struct config *cfg)
{

	if (!gen_pos(f, 5, &ei->pos))
		return 0;
	if (!gen_doc(f, 5, ei->doc))
		return 0;
	if (!gen_labelq(f, 5, "labels", &ei->labels, cfg))
		return 0;
	if (!gen_ws(f, 5))
		return 0;
	if (fputs("\"value\": ", f) == EOF)
		return 0;
	if (ei->flags & EITEM_AUTO) {
		if (fputs("null", f) == EOF)
			return 0;
	} else
		if (fprintf(f, "\"%" PRId64 "\"", ei->value) < 0)
			return 0;
	return fputc('\n', f) != EOF;
}

static int
gen_enm(FILE *f, const struct enm *enm, const struct config *cfg)
{
	const struct eitem	*ei;

	if (!gen_pos(f, 3, &enm->pos))
		return 0;
	if (!gen_doc(f, 3, enm->doc))
		return 0;
	if (!gen_labelq(f, 3, "labelsNull", &enm->labels_null, cfg))
		return 0;
	if (!gen_ws(f, 3))
		return 0;
	if (fputs("\"items\": {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(ei, &enm->eq, entries) {
		if (!gen_ws(f, 4))
			return 0;
		if (fprintf(f, "\"%s\": {\n", ei->name) < 0)
			return 0;
		if (!gen_eitem(f, ei, cfg))
			return 0;
		if (!gen_ws(f, 4))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(ei, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}
	if (!gen_ws(f, 3))
		return 0;
	return fputs("}\n", f) != EOF;
}

static int
gen_enms(FILE *f, const struct config *cfg)
{
	const struct enm	*enm;

	if (!gen_ws(f, 1))
		return 0;
	if (fputs("\"enums\": ", f) == EOF)
		return 0;

	if (TAILQ_EMPTY(&cfg->eq))
		return fputs("null,\n", f) != EOF;

	if (fputs("{\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(enm, &cfg->eq, entries) {
		if (!gen_ws(f, 2))
			return 0;
		if (fprintf(f, "\"%s\": {\n", enm->name) < 0)
			return 0;
		if (!gen_enm(f, enm, cfg))
			return 0;
		if (!gen_ws(f, 2))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(enm, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (!gen_ws(f, 1))
		return 0;
	return fputs("},\n", f) != EOF;
}

static int
gen_bitidx(FILE *f, size_t tabs,
	const struct bitidx *bi, const struct config *cfg)
{

	if (!gen_pos(f, tabs, &bi->pos))
		return 0;
	if (!gen_doc(f, tabs, bi->doc))
		return 0;
	if (!gen_labelq(f, tabs, "labels", &bi->labels, cfg))
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	return fprintf(f, "\"value\": "
		"\"%" PRId64 "\"\n", bi->value) > 0;
}

static int
gen_bitf(FILE *f, size_t tabs,
	const struct bitf *bitf, const struct config *cfg)
{
	const struct bitidx	*bi;

	if (!gen_pos(f, tabs, &bitf->pos))
		return 0;
	if (!gen_doc(f, tabs, bitf->doc))
		return 0;
	if (!gen_labelq(f, tabs, "labelsNull", &bitf->labels_null, cfg))
		return 0;
	if (!gen_labelq(f, tabs, "labelsUnset", &bitf->labels_unset, cfg))
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"items\": {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(bi, &bitf->bq, entries) {
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fprintf(f, "\"%s\": {\n", bi->name) < 0)
			return 0;
		if (!gen_bitidx(f, tabs + 2, bi, cfg))
			return 0;
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(bi, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}
	if (!gen_ws(f, tabs))
		return 0;
	return fputs("}\n", f) != EOF;
}

static int
gen_bitfs(FILE *f, const struct config *cfg)
{
	const struct bitf	*bitf;

	if (!gen_ws(f, 1))
		return 0;
	if (fputs("\"bitfs\": ", f) == EOF)
		return 0;

	if (TAILQ_EMPTY(&cfg->bq))
		return fputs("null,\n", f) != EOF;

	if (fputs("{\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(bitf, &cfg->bq, entries) {
		if (!gen_ws(f, 2))
			return 0;
		if (fprintf(f, "\"%s\": {\n", bitf->name) < 0)
			return 0;
		if (!gen_bitf(f, 3, bitf, cfg))
			return 0;
		if (!gen_ws(f, 2))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(bitf, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}
	if (!gen_ws(f, 1))
		return 0;
	return fputs("},\n", f) != EOF;
}

static int
gen_role(FILE *f, size_t tabs, const struct role *r)
{
	const struct role	*rr;

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"%s\": {\n", r->name) < 0)
		return 0;
	if (!gen_pos(f, tabs + 1, &r->pos))
		return 0;
	if (!gen_doc(f, tabs + 1, r->doc))
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\"children\": ", f) == EOF)
		return 0;

	if (!TAILQ_EMPTY(&r->subrq)) {
		if (fputs("{\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(rr, &r->subrq, entries) {
			if (!gen_role(f, tabs + 2, rr))
				return 0;
			if (TAILQ_NEXT(rr, entries) != NULL &&
			    fputc(',', f) == EOF)
				return 0;
			if (fputc('\n', f) == EOF)
				return 0;
		}
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fputs("}\n", f) == EOF)
			return 0;
	} else
		if (fputs("null\n", f) == EOF)
			return 0;

	if (!gen_ws(f, tabs))
		return 0;
	return fputc('}', f) != EOF;
}

static int
gen_roles(FILE *f, const struct config *cfg)
{
	const struct role	*r, *rr;

	if (!gen_ws(f, 1))
		return 0;
	if (fputs("\"roles\": ", f) == EOF)
		return 0;

	if (TAILQ_EMPTY(&cfg->rq))
		return fputs("null,\n", f) != EOF;
	if (fputs("{\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(r, &cfg->rq, entries) {
		if (strcasecmp(r->name, "all"))
			continue;
		TAILQ_FOREACH(rr, &r->subrq, entries) {
			if (!gen_role(f, 2, rr))
				return 0;
			if (TAILQ_NEXT(rr, entries) != NULL &&
			    fputc(',', f) == EOF)
				return 0;
			if (fputc('\n', f) == EOF)
				return 0;
		}
		break;
	}

	if (!gen_ws(f, 1))
		return 0;
	return fputs("},\n", f) != EOF;
}

static int
gen_field(FILE *f, size_t tabs, const struct field *fd)
{
	struct fvalid	*fv;

	if (!gen_pos(f, tabs, &fd->pos))
		return 0;
	if (!gen_doc(f, tabs, fd->doc))
		return 0;
	if (fd->enm != NULL) {
		if (!gen_ws(f, tabs))
			return 0;
		if (fprintf(f, 
		    "\"enm\": \"%s\",\n", fd->enm->name) < 0)
			return 0;
	}
	if (fd->bitf != NULL) {
		if (!gen_ws(f, tabs))
			return 0;
		if (fprintf(f, 
		    "\"bitf\": \"%s\",\n", fd->bitf->name) < 0)
			return 0;
	}
	if (fd->ref != NULL) {
		if (!gen_ws(f, tabs))
			return 0;
		if (fputs("\"ref\": {\n", f) == EOF)
			return 0;
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fprintf(f, "\"target\": "
		    "{ \"strct\": \"%s\", \"field\": \"%s\" },\n",
		    fd->ref->target->parent->name,
		    fd->ref->target->name) < 0)
			return 0;
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fprintf(f, "\"source\": "
		    "{ \"strct\": \"%s\", \"field\": \"%s\" }\n",
		    fd->ref->source->parent->name,
		    fd->ref->source->name) < 0)
			return 0;
		if (!gen_ws(f, tabs))
			return 0;
		if (fputs("},\n", f) == EOF)
			return 0;
	}

	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"def\": ", f) == EOF)
		return 0;
	if (fd->flags & FIELD_HASDEF) {
		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			if (fprintf
		  	    (f, "\"%" PRId64 "\"", fd->def.integer) < 0)
				return 0;
			break;
		case FTYPE_REAL:
			if (fprintf(f, "\"%g\"", fd->def.decimal) < 0)
				return 0;
			break;
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
			if (!gen_string(f, fd->def.string))
				return 0;
			break;
		case FTYPE_ENUM:
			if (fprintf
			    (f, "\"%s\"", fd->def.eitem->name) < 0)
				return 0;
			break;
		default:
			abort();
			break;
		}
		if (fputs(",\n", f) == EOF)
			return 0;
	} else
		if (fputs("null,\n", f) == EOF)
			return 0;

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"actdel\": \"%s\",\n", upacts[fd->actdel]) < 0)
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"actup\": \"%s\",\n", upacts[fd->actup]) < 0)
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	if (!gen_rolemap(f, fd->rolemap))
		return 0;
	if (fputs(",\n", f) == EOF)
		return 0;

	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"valids\": [", f) == EOF)
		return 0;	
	if (!TAILQ_EMPTY(&fd->fvq) && fputc('\n', f) == EOF)
		return 0;	
	TAILQ_FOREACH(fv, &fd->fvq, entries) {
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fprintf(f, "{ \"type\": \"%s\", \"limit\": \"", 
		    vtypes[fv->type]) < 0)
			return 0;
		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			if (fprintf(f, "%" PRId64, 
			    fv->d.value.integer) < 0)
				return 0;
			break;
		case FTYPE_REAL:
			if (fprintf(f, "%g", 
			    fv->d.value.decimal) < 0)
				return 0;
			break;
		case FTYPE_BLOB:
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
			if (fprintf(f, "%zu", 
			    fv->d.value.len) < 0)
				return 0;
			break;
		default:
			abort();
		}
		if (fputs("\"}", f) == EOF)
			return 0;
		if (TAILQ_NEXT(fv, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}
	if (!TAILQ_EMPTY(&fd->fvq) && !gen_ws(f, tabs))
		return 0;
	if (fputs("],\n", f) == EOF)
		return 0;

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\"type\": \"%s\"\n", ftypes[fd->type]) < 0)
		return 0;
	return 1;
}

static int
gen_strct(FILE *f, size_t tabs, const struct strct *s)
{
	const struct field	*fd;

	if (!gen_pos(f, tabs, &s->pos))
		return 0;
	if (!gen_doc(f, tabs, s->doc))
		return 0;
	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\"fields\": {\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fprintf(f, "\"%s\": {\n", fd->name) < 0)
			return 0;
		if (!gen_field(f, tabs + 2, fd))
			return 0;
		if (!gen_ws(f, tabs + 1))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(fd, entries) && fputc(',', f) == EOF)
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (!gen_ws(f, tabs))
		return 0;
	return fputs("}\n", f) != EOF;
}

static int
gen_strcts(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	if (!gen_ws(f, 1))
		return 0;
	if (fputs("\"strcts\": {\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (!gen_ws(f, 2))
			return 0;
		if (fprintf(f, "\"%s\": {\n", s->name) < 0)
			return 0;
		if (!gen_strct(f, 3, s))
			return 0;
		if (!gen_ws(f, 2))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(s, entries) && fputc(',', f) == EOF)
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (!gen_ws(f, 1))
		return 0;
	return fputs("}\n", f) != EOF;
}

int
ort_lang_json(const struct config *cfg, FILE *f)
{

	if (fputs("{ \"config\": {\n", f) == EOF)
		return 0;
	if (!gen_roles(f, cfg))
		return 0;
	if (!gen_enms(f, cfg))
		return 0;
	if (!gen_bitfs(f, cfg))
		return 0;
	if (!gen_strcts(f, cfg))
		return 0;
	return fputs("} }\n", f) != EOF;
}

