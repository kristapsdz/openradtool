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

static int
gen_ws(FILE *f, size_t sz)
{
	size_t	 i;

	for (i = 0; i < sz; i++)
		if (fputc('\t', f) == EOF)
			return 0;
	return 1;
}

static int
gen_string(FILE *f, const char *cp)
{

	if (fputc('\'', f) == EOF)
		return 0;
	for ( ; *cp != '\0'; cp++) {
		if (*cp == '\'' && fputc('\\', f) == EOF)
			return 0;
		if (fputc(*cp, f) == EOF)
			return 0;
	}
	return fputc('\'', f) != EOF;
}

static int
gen_pos(FILE *f, size_t tabs, const struct pos *pos)
{

	assert(pos != NULL);
	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\'pos\': {\n", f) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\'fname\': ", f) == EOF)
		return 0;
	if (!gen_string(f, pos->fname))
		return 0;
	if (fputs(",\n", f) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fprintf(f, "\'line\': %zu,\n", pos->line) == EOF)
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fprintf(f, "\'column\': %zu\n", pos->column) == EOF)
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
	if (fprintf(f, "\'%s\': {\n", name) < 0)
		return 0;
	if (!gen_pos(f, tabs + 1, &l->pos))
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\'value\': ", f) == EOF)
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
	if (fprintf(f, "\'%s\': ", name) < 0)
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
	return fputs("}\n", f) != EOF;
}

static int
gen_doc(FILE *f, size_t tabs, const char *doc)
{

	if (!gen_ws(f, tabs))
		return 0;
	if (fputs("\'doc\': ", f) == EOF)
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
	if (fputs("\'value\': ", f) == EOF)
		return 0;
	if (ei->flags & EITEM_AUTO) {
		if (fputs("null", f) == EOF)
			return 0;
	} else
		if (fprintf(f, "\'%" PRId64 "\'", ei->value) < 0)
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
	if (fputs("\'items\': {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(ei, &enm->eq, entries) {
		if (!gen_ws(f, 4))
			return 0;
		if (fprintf(f, "\'%s\': {\n", ei->name) < 0)
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
	if (fputs("\'enums\': ", f) == EOF)
		return 0;

	if (TAILQ_EMPTY(&cfg->eq))
		return fputs("null,\n", f) != EOF;

	if (fputs("{\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(enm, &cfg->eq, entries) {
		if (!gen_ws(f, 2))
			return 0;
		if (fprintf(f, "\'%s\': {\n", enm->name) < 0)
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
	return fputs("}\n", f) != EOF;
}

static int
gen_bitidx(FILE *f, const struct bitidx *bi, const struct config *cfg)
{

	if (!gen_pos(f, 5, &bi->pos))
		return 0;
	if (!gen_doc(f, 5, bi->doc))
		return 0;
	if (!gen_labelq(f, 5, "labels", &bi->labels, cfg))
		return 0;
	if (!gen_ws(f, 5))
		return 0;
	return fprintf(f, "\'value\': "
		"\'%" PRId64 "\'\n", bi->value) > 0;
}

static int
gen_bitf(FILE *f, const struct bitf *bitf, const struct config *cfg)
{
	const struct bitidx	*bi;

	if (!gen_pos(f, 3, &bitf->pos))
		return 0;
	if (!gen_doc(f, 3, bitf->doc))
		return 0;
	if (!gen_labelq(f, 3, "labelsNull", &bitf->labels_null, cfg))
		return 0;
	if (!gen_labelq(f, 3, "labelsUnset", &bitf->labels_unset, cfg))
		return 0;
	if (!gen_ws(f, 3))
		return 0;
	if (fputs("\'items\': {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(bi, &bitf->bq, entries) {
		if (!gen_ws(f, 4))
			return 0;
		if (fprintf(f, "\'%s\': {\n", bi->name) < 0)
			return 0;
		if (!gen_bitidx(f, bi, cfg))
			return 0;
		if (!gen_ws(f, 4))
			return 0;
		if (fputc('}', f) == EOF)
			return 0;
		if (TAILQ_NEXT(bi, entries) != NULL)
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
gen_bitfs(FILE *f, const struct config *cfg)
{
	const struct bitf	*bitf;

	if (!gen_ws(f, 1))
		return 0;
	if (fputs("\'bitfs\': ", f) == EOF)
		return 0;

	if (TAILQ_EMPTY(&cfg->bq))
		return fputs("null,\n", f) != EOF;

	if (fputs("{\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(bitf, &cfg->bq, entries) {
		if (!gen_ws(f, 2))
			return 0;
		if (fprintf(f, "\'%s\': {\n", bitf->name) < 0)
			return 0;
		if (!gen_bitf(f, bitf, cfg))
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
	return fputs("}\n", f) != EOF;
}

static int
gen_role(FILE *f, size_t tabs, const struct role *r)
{
	const struct role	*rr;

	if (!gen_ws(f, tabs))
		return 0;
	if (fprintf(f, "\'%s\': {\n", r->name) < 0)
		return 0;
	if (!gen_pos(f, tabs + 1, &r->pos))
		return 0;
	if (!gen_doc(f, tabs + 1, r->doc))
		return 0;
	if (!gen_ws(f, tabs + 1))
		return 0;
	if (fputs("\'children\': ", f) == EOF)
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
	if (fputs("\'roles\': ", f) == EOF)
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

int
ort_lang_json(const struct config *cfg, FILE *f)
{

	if (fputs("{\n", f) == EOF)
		return 0;
	if (!gen_roles(f, cfg))
		return 0;
	if (!gen_enms(f, cfg))
		return 0;
	if (!gen_bitfs(f, cfg))
		return 0;
	return fputs("}\n", f) != EOF;
}

