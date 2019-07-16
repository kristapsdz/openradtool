/*	$Id$ */
/*
 * Copyright (c) 2018--2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#if HAVE_ERR
# include <err.h>
#endif
#include <expat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "extern.h"

struct	xliffunit {
	char		*name;
	char		*source;
	char		*target;
};

struct	xliffset {
	struct xliffunit *u;
	size_t		  usz;
	char	 	 *trglang; /* target language */
};

/*
 * Parse tracker for an XLIFF file that we're parsing.
 * This will be passed in (assuming success) to the "struct hparse" as
 * the xliffs pointer.
 */
struct	xparse {
	XML_Parser	  p;
	const char	 *fname; /* xliff filename */
	struct xliffset	 *set; /* resulting translation units */
	struct xliffunit *curunit; /* current translating unit */
	int		  type; /* >0 source, <0 target, 0 none */
};

static void
lerr(const char *fn, XML_Parser p, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

static void
xend(void *dat, const XML_Char *s);

static void
xstart(void *dat, const XML_Char *s, const XML_Char **atts);

static void
lerr(const char *fn, XML_Parser p, const char *fmt, ...)
{
	va_list	 ap;

	fprintf(stderr, "%s:%zu:%zu: ", fn, 
		XML_GetCurrentLineNumber(p),
		XML_GetCurrentColumnNumber(p));

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
}

static struct xparse *
xparse_alloc(const char *xliff, XML_Parser parse)
{
	struct xparse	*p;

	if (NULL == (p = calloc(1, sizeof(struct xparse))))
		err(EXIT_FAILURE, NULL);

	p->fname = xliff;
	p->p = parse;
	return p;
}

static void
xparse_xliff_free(struct xliffset *p)
{
	size_t	 i;

	if (NULL == p)
		return;

	for (i = 0; i < p->usz; i++) {
		free(p->u[i].source);
		free(p->u[i].target);
		free(p->u[i].name);
	}

	free(p->trglang);
	free(p->u);
	free(p);
}

static void
xparse_free(struct xparse *p)
{

	assert(NULL != p);
	xparse_xliff_free(p->set);
	free(p);
}

static void
xtext(void *dat, const XML_Char *s, int len)
{
	struct xparse	 *p = dat;
	size_t		 sz;

	assert(NULL != p->curunit);
	assert(p->type);

	if (p->type > 0) {
		if (NULL != p->curunit->source) {
			sz = strlen(p->curunit->source);
			p->curunit->source = realloc
				(p->curunit->source,
				 sz + len + 1);
			if (NULL == p->curunit->source)
				err(EXIT_FAILURE, NULL);
			memcpy(p->curunit->source + sz, s, len);
			p->curunit->source[sz + len] = '\0';
		} else {
			p->curunit->source = strndup(s, len);
			if (NULL == p->curunit->source)
				err(EXIT_FAILURE, NULL);
		}
	} else {
		if (NULL != p->curunit->target) {
			sz = strlen(p->curunit->target);
			p->curunit->target = realloc
				(p->curunit->target,
				 sz + len + 1);
			if (NULL == p->curunit->target)
				err(EXIT_FAILURE, NULL);
			memcpy(p->curunit->target + sz, s, len);
			p->curunit->target[sz + len] = '\0';
		} else {
			p->curunit->target = strndup(s, len);
			if (NULL == p->curunit->target)
				err(EXIT_FAILURE, NULL);
		}
	}
}

static void
xstart(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct xparse	 *p = dat;
	const XML_Char	**attp;
	const char	 *ver, *id;

	if (0 == strcmp(s, "xliff")) {
		if (NULL != p->set) {
			lerr(p->fname, p->p, "nested <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->set = calloc(1, sizeof(struct xliffset));
		if (NULL == p->set)
			err(EXIT_FAILURE, NULL);
		ver = NULL;
		for (attp = atts; NULL != *attp; attp += 2) 
			if (0 == strcmp(attp[0], "version"))
				ver = attp[1];
		if (NULL == ver) {
			lerr(p->fname, p->p, 
				"<xliff> without version");
			XML_StopParser(p->p, 0);
			return;
		} else if (strcmp(ver, "1.2")) {
			lerr(p->fname, p->p, 
				"<xliff> version must be 1.2");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (0 == strcmp(s, "file")) {
		if (NULL == p->set) {
			lerr(p->fname, p->p, "<file> not in <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		if (NULL != p->set->trglang) {
			lerr(p->fname, p->p, "nested <file>");
			XML_StopParser(p->p, 0);
			return;
		}
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcmp(attp[0], "target-language")) {
				free(p->set->trglang);
				p->set->trglang = strdup(attp[1]);
				if (NULL == p->set->trglang)
					err(EXIT_FAILURE, NULL);
			}
		if (NULL == p->set->trglang) {
			lerr(p->fname, p->p, "<file> "
				"target-language not given");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (0 == strcmp(s, "trans-unit")) {
		if (NULL == p->set->trglang) {
			lerr(p->fname, p->p, 
				"<trans-unit> not in <file>");
			XML_StopParser(p->p, 0);
			return;
		} else if (NULL != p->curunit) {
			lerr(p->fname, p->p, 
				"nested <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}

		id = NULL;
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcmp(attp[0], "id")) {
				id = attp[1];
				break;
			}
		if (NULL == id) {
			lerr(p->fname, p->p, 
				"<trans-unit> without id");
			XML_StopParser(p->p, 0);
			return;
		}

		p->set->u = reallocarray
			(p->set->u,
			 p->set->usz + 1,
			 sizeof(struct xliffunit));
		if (NULL == p->set->u)
			err(EXIT_FAILURE, NULL);
		p->curunit = &p->set->u[p->set->usz++];
		memset(p->curunit, 0, sizeof(struct xliffunit));
		p->curunit->name = strdup(id);
		if (NULL == p->curunit->name)
			err(EXIT_FAILURE, NULL);
	} else if (0 == strcmp(s, "source")) {
		if (NULL == p->curunit) {
			lerr(p->fname, p->p, 
				"<soure> not in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->type) {
			lerr(p->fname, p->p, "nested <source>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->type = 1;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else if (0 == strcmp(s, "target")) {
		if (NULL == p->curunit) {
			lerr(p->fname, p->p, 
				"<target> not in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->type) {
			lerr(p->fname, p->p, "nested <target>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->type = -1;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else {
		if (p->type) {
			lerr(p->fname, p->p, "element "
				"in translatable content");
			XML_StopParser(p->p, 0);
			return;
		}
	}
}

static void
xend(void *dat, const XML_Char *s)
{
	struct xparse	*p = dat;

	XML_SetDefaultHandlerExpand(p->p, NULL);

	if (0 == strcmp(s, "trans-unit")) {
		assert(NULL != p->curunit);
		if (NULL == p->curunit->source || 
		    NULL == p->curunit->target) {
			lerr(p->fname, p->p, "missing <source> "
				"or <target> in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->curunit = NULL;
	} else if (0 == strcmp(s, "target")) {
		assert(NULL != p->curunit);
		p->type = 0;
	} else if (0 == strcmp(s, "source")) {
		assert(NULL != p->curunit);
		p->type = 0;
	}
}

/*
 * Parse an XLIFF file from stdin.
 * Accepts "fn", which is usually "<stdin>".
 * Returns NULL on failure or the xliffset on success.
 */
static struct xliffset *
xliff_read(int fd, const char *fn, XML_Parser p)
{
	struct xparse	*xp;
	struct xliffset	*res = NULL;
	ssize_t		 ssz;
	int		 rc;
	char		 buf[BUFSIZ];

	xp = xparse_alloc(fn, p);
	assert(NULL != xp);

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);

	do {
		ssz = read(fd, buf, sizeof(buf));
		if (ssz < 0) {
			warn("%s", fn);
			xparse_free(xp);
			return NULL;
		} 
		rc = XML_Parse(p, buf, ssz, 0 == ssz);
		if (XML_STATUS_OK != rc) {
			lerr(fn, p, "%s", 
				XML_ErrorString
				(XML_GetErrorCode(p)));
			xparse_free(xp);
			return NULL;
		}
	} while (ssz > 0);

	res = xp->set;
	xp->set = NULL;
	xparse_free(xp);
	return res;
}

static void
xliff_extract_unit(const struct labelq *lq, const char *type,
	const struct pos *pos, const char ***s, size_t *ssz)
{
	const struct label *l;
	size_t		    i;

	TAILQ_FOREACH(l, lq, entries)
		if (0 == l->lang)
			break;

	if (NULL == l && NULL == type) {
		fprintf(stderr, "%s:%zu:%zu: missing "
			"jslabel for translation\n",
			pos->fname, pos->line, pos->column);
		return;
	} else if (NULL == l) {
		fprintf(stderr, "%s:%zu:%zu: missing "
			"\"%s\" jslabel for translation\n",
			pos->fname, pos->line, pos->column, type);
		return;
	}

	for (i = 0; i < *ssz; i++)
		if (0 == strcmp((*s)[i], l->label))
			return;

	*s = reallocarray
		(*s, *ssz + 1, sizeof(char **));
	if (NULL == *s)
		err(EXIT_FAILURE, NULL);
	(*s)[*ssz] = l->label;
	(*ssz)++;
}

static int
xliffunit_sort(const void *a1, const void *a2)
{
	struct xliffunit *p1 = (struct xliffunit *)a1,
		 	 *p2 = (struct xliffunit *)a2;

	return(strcmp(p1->source, p2->source));
}

static int
xliff_sort(const void *p1, const void *p2)
{

	return(strcmp(*(const char **)p1, *(const char **)p2));
}

static int
xliff_extract(const struct config *cfg, int copy)
{
	const struct enm *e;
	const struct eitem *ei;
	const struct bitf *b;
	const struct bitidx *bi;
	size_t		  i, ssz = 0;
	const char	**s = NULL;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			xliff_extract_unit(&ei->labels, 
				NULL, &ei->pos, &s, &ssz);

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			xliff_extract_unit(&bi->labels, 
				NULL, &bi->pos, &s, &ssz);
		xliff_extract_unit(&b->labels_unset, 
			"unset", &b->pos, &s, &ssz);
		xliff_extract_unit(&b->labels_null, 
			"isnull", &b->pos, &s, &ssz);
	}

	qsort(s, ssz, sizeof(char *), xliff_sort);

	printf("<xliff version=\"1.2\">\n"
	       "\t<file target-language=\"TODO\" "
	          "tool=\"%s\">\n"
	       "\t\t<body>\n", getprogname());

	for (i = 0; i < ssz; i++)
		if (copy) 
			printf("\t\t\t<trans-unit id=\"%zu\">\n"
			       "\t\t\t\t<source>%s</source>\n"
			       "\t\t\t\t<target>%s</target>\n"
			       "\t\t\t</trans-unit>\n",
			       i + 1, s[i], s[i]);
		else
			printf("\t\t\t<trans-unit id=\"%zu\">\n"
			       "\t\t\t\t<source>%s</source>\n"
			       "\t\t\t</trans-unit>\n",
			       i + 1, s[i]);

	puts("\t\t</body>\n"
	     "\t</file>\n"
	     "</xliff>");

	return 1;
}

static int
xliff_join_unit(struct labelq *q, int copy, const char *type,
	size_t lang, const struct xliffset *x, const struct pos *pos)
{
	struct label	*l;
	size_t		 i;
	const char	*targ = NULL;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	/* 
	 * See if we have a default translation (lang == 0). 
	 * This is going to be the material we want to translate.
	 */

	if (NULL == l && NULL == type) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation\n",
			pos->fname, pos->line, pos->column);
		return 0;
	} else if (NULL == l) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (0 == strcmp(x->u[i].source, l->label))
			break;

	if (i == x->usz && NULL == type) {
		if (copy) {
			fprintf(stderr, "%s:%zu:%zu: using "
				"source for translation\n", 
				pos->fname, pos->line, 
				pos->column);
			targ = l->label;
		} else {
			fprintf(stderr, "%s:%zu:%zu: missing "
				"translation\n", pos->fname, 
				pos->line, pos->column);
			return 0;
		}
	} else if (i == x->usz) {
		if (copy) {
			fprintf(stderr, "%s:%zu:%zu: using "
				"source for translating "
				"\"%s\" clause\n", pos->fname, 
				pos->line, pos->column, type);
			targ = l->label;
		} else {
			fprintf(stderr, "%s:%zu:%zu: missing "
				"translation for \"%s\" clause\n",
				pos->fname, pos->line, 
				pos->column, type);
			return 0;
		}
	} else
		targ = x->u[i].target;

	assert(NULL != targ);

	/* 
	 * We have what we want to translate, now make sure that we're
	 * not overriding an existing translation.
	 */

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == lang)
			break;

	if (NULL != l && NULL == type) {
		fprintf(stderr, "%s:%zu:%zu: not "
			"overriding existing translation\n",
			pos->fname, pos->line, pos->column);
		return 1;
	} else if (NULL != l) {
		fprintf(stderr, "%s:%zu:%zu: not "
			"overriding existing translation "
			"for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 1;
	}

	/* Add the translation. */

	l = calloc(1, sizeof(struct label));
	l->lang = lang;
	l->label = strdup(targ);
	if (NULL == l->label)
		err(EXIT_FAILURE, NULL);
	TAILQ_INSERT_TAIL(q, l, entries);

	return 1;
}

static int
xliff_update_unit(struct labelq *q, const char *type,
	struct xliffset *x, const struct pos *pos)
{
	struct label	 *l;
	size_t		  i;
	struct xliffunit *u;
	char		  nbuf[32];

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	/* 
	 * See if we have a default translation (lang == 0). 
	 * This is going to be the material we want to translate.
	 */

	if (NULL == l && NULL == type) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation\n",
			pos->fname, pos->line, pos->column);
		return 0;
	} else if (NULL == l) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (0 == strcmp(x->u[i].source, l->label))
			break;

	if (i == x->usz) {
		x->u = reallocarray
			(x->u, x->usz + 1,
			 sizeof(struct xliffunit));
		if (NULL == x->u)
			err(EXIT_FAILURE, NULL);
		u = &x->u[x->usz++];
		snprintf(nbuf, sizeof(nbuf), "%zu", x->usz);
		memset(u, 0, sizeof(struct xliffunit));
		u->name = strdup(nbuf);
		if (NULL == u->name)
			err(EXIT_FAILURE, NULL);
		u->source = strdup(l->label);
		if (NULL == u->source)
			err(EXIT_FAILURE, NULL);
		fprintf(stderr, "%s:%zu:%zu: new translation\n",
			l->pos.fname, l->pos.line, l->pos.column);
	}

	return 1;
}

static int
xliff_join_xliff(struct config *cfg, int copy,
	size_t lang, const struct xliffset *x)
{
	struct enm 	*e;
	struct eitem 	*ei;
	struct bitf	*b;
	struct bitidx	*bi;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			if ( ! xliff_join_unit
			    (&ei->labels, copy, NULL, 
			     lang, x, &ei->pos))
				return 0;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			if ( ! xliff_join_unit
			    (&bi->labels, copy, NULL, 
			     lang, x, &bi->pos))
				return 0;
		if ( ! xliff_join_unit
		    (&b->labels_unset, copy, 
		     "isunset", lang, x, &b->pos))
			return 0;
		if ( ! xliff_join_unit
		    (&b->labels_null, copy, 
		     "isnull", lang, x, &b->pos))
			return 0;
	}

	return 1;
}

static int
xliff_update(struct config *cfg, int copy, 
	const int *xmls, size_t xmlsz, const char **argv)
{
	struct xliffset	*x;
	int		 rc = 0;
	size_t		 i;
	XML_Parser	 p;
	struct enm 	*e;
	struct eitem 	*ei;
	struct bitf	*b;
	struct bitidx	*bi;

	assert(xmlsz < 2);

	if (NULL == (p = XML_ParserCreate(NULL))) {
		warn("XML_ParserCreate");
		return 0;
	}

	x = xmlsz ?
		xliff_read(xmls[0], argv[0], p) :
		xliff_read(STDIN_FILENO, "<stdin>", p);

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			if ( ! xliff_update_unit
			    (&ei->labels, NULL, x, &ei->pos))
				goto out;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			if ( ! xliff_update_unit
			    (&bi->labels, NULL, x, &bi->pos))
				goto out;
		if ( ! xliff_update_unit
		    (&b->labels_unset, "isunset", x, &b->pos))
			goto out;
		if ( ! xliff_update_unit
		    (&b->labels_null, "isnull", x, &b->pos))
			goto out;
	}

	qsort(x->u, x->usz, sizeof(struct xliffunit), xliffunit_sort);

	printf("<xliff version=\"1.2\">\n"
	       "\t<file target-language=\"%s\" "
	          "tool=\"%s\">\n"
	       "\t\t<body>\n",
	       x->trglang, getprogname());

	for (i = 0; i < x->usz; i++)
		if (NULL == x->u[i].target && copy)
			printf("\t\t\t<trans-unit id=\"%s\">\n"
			       "\t\t\t\t<source>%s</source>\n"
			       "\t\t\t\t<target>%s</target>\n"
			       "\t\t\t</trans-unit>\n",
			       x->u[i].name, x->u[i].source,
			       x->u[i].source);
		else if (NULL == x->u[i].target)
			printf("\t\t\t<trans-unit id=\"%s\">\n"
			       "\t\t\t\t<source>%s</source>\n"
			       "\t\t\t</trans-unit>\n",
			       x->u[i].name, x->u[i].source);
		else
			printf("\t\t\t<trans-unit id=\"%s\">\n"
			       "\t\t\t\t<source>%s</source>\n"
			       "\t\t\t\t<target>%s</target>\n"
			       "\t\t\t</trans-unit>\n",
			       x->u[i].name, x->u[i].source,
			       x->u[i].target);

	puts("\t\t</body>\n"
	     "\t</file>\n"
	     "</xliff>");

	return 1;

	rc = 1;
out:
	xparse_xliff_free(x);
	XML_ParserFree(p);
	return rc;
}

/*
 * Parse an XLIFF file with open descriptor "xml", which may be
 * STDIN_FILENO (and thus not mmap-able), and merge the translations
 * with labels in "cfg".
 * Returns zero on XLIFF parse failure or merge failure, non-zero on
 * success.
 */
static int
xliff_join(struct config *cfg, int copy, 
	int xml, const char *fn, XML_Parser p)
{
	struct xliffset	*x;
	size_t		 i;

	if (NULL == (x = xliff_read(xml, fn, p)))
		return 0;

	assert(NULL != x->trglang);
	for (i = 0; i < cfg->langsz; i++)
		if (0 == strcmp(cfg->langs[i], x->trglang))
			break;

	if (i == cfg->langsz) {
		cfg->langs = reallocarray
			(cfg->langs,
			 cfg->langsz + 1,
			 sizeof(char *));
		if (NULL == cfg->langs)
			err(EXIT_FAILURE, NULL);
		cfg->langs[cfg->langsz] =
			strdup(x->trglang);
		if (NULL == cfg->langs[cfg->langsz])
			err(EXIT_FAILURE, NULL);
		cfg->langsz++;
	} else
		fprintf(stderr, "%s: language \"%s\" is "
			"already noted\n", fn, x->trglang);

	if ( ! xliff_join_xliff(cfg, copy, i, x)) {
		xparse_xliff_free(x);
		return 0;
	}

	xparse_xliff_free(x);
	return 1;
}

/*
 * Given file descriptors "xmls" of size "xmlsz" identified by "argv",
 * which may be NULL and zero and NULL, join to configuration "cfg".
 * Returns zero on XLIFF parse failure or merge failure, non-zero on
 * success.
 * On success, the output is printed.
 */
static int
xliff_join_fds(struct config *cfg, int copy, 
	const int *xmls, size_t xmlsz, const char **argv)
{
	int	 	 rc = 1;
	size_t		 i;
	XML_Parser	 p;

	if (NULL == (p = XML_ParserCreate(NULL))) {
		warn("XML_ParserCreate");
		return 0;
	}

	for (i = 0; rc && i < xmlsz; i++) 
		rc = xliff_join(cfg, copy, xmls[i], argv[i], p);
	if (0 == xmlsz)
		rc = xliff_join(cfg, copy, STDIN_FILENO, "<stdin>", p);

	XML_ParserFree(p);

	if (rc)
		ort_write_file(stdout, cfg);

	return rc;
}

int
main(int argc, char *argv[])
{
	FILE		**confs = NULL;
	int		 *xmls = NULL;
	struct config	 *cfg = NULL;
#define	OP_EXTRACT	  0
#define	OP_JOIN		  1
#define	OP_UPDATE	 (-1)
	int		  c, op = OP_EXTRACT, copy = 0, rc = 0;
	size_t		  confsz = 0, i, j, xmlsz = 0, xmlstart = 0;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "cju")) != -1)
		switch (c) {
		case 'c':
			copy = 1;
			break;
		case 'j':
			op = OP_JOIN;
			break;
		case 'u':
			op = OP_UPDATE;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (op == OP_JOIN || op == OP_UPDATE) {
		for (confsz = 0; confsz < (size_t)argc; confsz++)
			if (strcmp(argv[confsz], "-x") == 0)
				break;

		/* If we have >2 w/o -x, error (which conf/xml?). */

		if (confsz == (size_t)argc && argc > 2)
			goto usage;

		if ((i = confsz) < (size_t)argc)
			i++;

		xmlstart = i;
		xmlsz = argc - i;

		/* If we have 2 w/o -f, it's old-new. */

		if (xmlsz == 0 && argc == 2)
			xmlsz = confsz = xmlstart = 1;

		if (OP_UPDATE == op && xmlsz > 1)
			goto usage;

		xmls = calloc(xmlsz, sizeof(int));
		confs = calloc(confsz, sizeof(FILE *));
		if (NULL == xmls || NULL == confs)
			err(EXIT_FAILURE, NULL);

		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
		if (i < (size_t)argc && 0 == strcmp(argv[i], "-x"))
			i++;
		for (j = 0; i < (size_t)argc; j++, i++) {
			assert(j < xmlsz);
			if ((xmls[j] = open(argv[i], O_RDONLY, 0)) == -1)
				err(EXIT_FAILURE, "%s", argv[i]);
		}
	} else {
		confsz = (size_t)argc;
		confs = calloc(confsz, sizeof(FILE *));
		if (NULL == confs)
			err(EXIT_FAILURE, NULL);

		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
	}

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		goto out;

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file_r(cfg, confs[i], argv[i]))
			goto out;
	if (confsz == 0 && !ort_parse_file_r(cfg, stdin, "<stdin>"))
		goto out;
	if (!ort_parse_close(cfg))
		goto out;

	switch (op) {
	case OP_EXTRACT:
		rc = xliff_extract(cfg, copy);
		break;
	case OP_JOIN:
		rc = xliff_join_fds(cfg, copy, xmls, xmlsz, 
			(const char **)&argv[xmlstart + i]);
		break;
	case OP_UPDATE:
		rc = xliff_update(cfg, copy, xmls, xmlsz, 
			(const char **)&argv[xmlstart + i]);
		break;
	}
out:
	for (i = 0; i < xmlsz; i++)
		if (close(xmls[i]) == -1)
			warn("%s", argv[xmlstart + i]);
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s", argv[i]);

	free(confs);
	free(xmls);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s [-c] -j [config...] -x [xliff...]\n"
		"       %s [-c] -j config [xliff]\n"
		"       %s [-c] -u [config...] -x [xliff]\n"
		"       %s [-c] -u config [xliff]\n"
		"       %s [-c] [config...]\n",
		getprogname(), getprogname(), getprogname(),
		getprogname(), getprogname());
	return EXIT_FAILURE;
}
