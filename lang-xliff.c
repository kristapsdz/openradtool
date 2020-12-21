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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-xliff.h"

static int
xliff_extract_unit(FILE *f, const struct labelq *lq, const char *type,
	const struct pos *pos, const char ***s, size_t *ssz)
{
	const struct label *l;
	size_t		    i;

	TAILQ_FOREACH(l, lq, entries)
		if (l->lang == 0)
			break;

#if 0
	if (l == NULL && type == NULL) {
		fprintf(stderr, "%s:%zu:%zu: missing "
			"jslabel for translation\n",
			pos->fname, pos->line, pos->column);
		return;
	} else if (l == NULL) {
		fprintf(stderr, "%s:%zu:%zu: missing "
			"\"%s\" jslabel for translation\n",
			pos->fname, pos->line, pos->column, type);
		return;
	}
#endif
	if (l == NULL)
		return 1;

	for (i = 0; i < *ssz; i++)
		if (strcmp((*s)[i], l->label) == 0)
			return 1;

	*s = reallocarray(*s, *ssz + 1, sizeof(char **));
	if (NULL == *s)
		return 0;
	(*s)[*ssz] = l->label;
	(*ssz)++;
	return 1;
}

static int
xliff_sort(const void *p1, const void *p2)
{

	return(strcmp(*(const char **)p1, *(const char **)p2));
}

int
ort_lang_xliff_extract(const struct ort_lang_xliff *args,
	const struct config *cfg, FILE *f)
{
	const struct enm	 *e;
	const struct eitem	 *ei;
	const struct bitf	 *b;
	const struct bitidx	 *bi;
	size_t			  i, ssz = 0;
	const char		**s = NULL;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!xliff_extract_unit(f, &ei->labels, 
			    NULL, &ei->pos, &s, &ssz))
				return 0;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			if (!xliff_extract_unit(f, &bi->labels, 
			    NULL, &bi->pos, &s, &ssz))
				return 0;
		if (!xliff_extract_unit(f, &b->labels_unset, 
		    "unset", &b->pos, &s, &ssz))
			return 0;
		if (!xliff_extract_unit(f, &b->labels_null,
		    "isnull", &b->pos, &s, &ssz))
			return 0;
	}

	qsort(s, ssz, sizeof(char *), xliff_sort);

	if (fputs("<xliff version=\"1.2\" "
	    "xmlns=\"urn:oasis:names:tc:xliff:document:1.2\">\n"
	    "\t<file source-language=\"TODO\" original=\"\" "
	    "target-language=\"TODO\" datatype=\"plaintext\">\n"
            "\t\t<body>\n", f) == EOF)
		return 0;

	for (i = 0; i < ssz; i++)
		if (args->flags & ORT_LANG_XLIFF_COPY) {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%zu\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    i + 1, s[i], s[i]) < 0)
				return 0;
		} else {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%zu\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t</trans-unit>\n",
			    i + 1, s[i]) < 0)
				return 0;
		}

	return fputs("\t\t</body>\n\t</file>\n</xliff>\n", f) != EOF;
}

