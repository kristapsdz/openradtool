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
#include "ort-lang-xliff.h"

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

	if ((p = calloc(1, sizeof(struct xparse))) == NULL)
		err(EXIT_FAILURE, "calloc");
	p->fname = xliff;
	p->p = parse;
	return p;
}

static void
xparse_xliff_free(struct xliffset *p)
{
	size_t	 i;

	if (p == NULL)
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

	assert(p != NULL);
	xparse_xliff_free(p->set);
	free(p);
}

static void
xtext(void *dat, const XML_Char *s, int len)
{
	struct xparse	 *p = dat;
	size_t		 sz;

	assert(p->curunit != NULL);
	assert(p->type);

	if (p->type > 0) {
		if (p->curunit->source != NULL) {
			sz = strlen(p->curunit->source);
			p->curunit->source = realloc
				(p->curunit->source,
				 sz + len + 1);
			if (p->curunit->source == NULL)
				err(EXIT_FAILURE, "realloc");
			memcpy(p->curunit->source + sz, s, len);
			p->curunit->source[sz + len] = '\0';
		} else {
			p->curunit->source = strndup(s, len);
			if (p->curunit->source == NULL)
				err(EXIT_FAILURE, "strdnup");
		}
	} else {
		if (p->curunit->target != NULL) {
			sz = strlen(p->curunit->target);
			p->curunit->target = realloc
				(p->curunit->target,
				 sz + len + 1);
			if (p->curunit->target == NULL)
				err(EXIT_FAILURE, "realloc");
			memcpy(p->curunit->target + sz, s, len);
			p->curunit->target[sz + len] = '\0';
		} else {
			p->curunit->target = strndup(s, len);
			if (p->curunit->target == NULL)
				err(EXIT_FAILURE, "strndup");
		}
	}
}

static void
xstart(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct xparse	 *p = dat;
	const XML_Char	**attp;
	const char	 *ver, *id;

	if (strcmp(s, "xliff") == 0) {
		if (p->set != NULL) {
			lerr(p->fname, p->p, "nested <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->set = calloc(1, sizeof(struct xliffset));
		if (p->set == NULL)
			err(EXIT_FAILURE, "calloc");
		ver = NULL;
		for (attp = atts; *attp != NULL; attp += 2) 
			if (strcmp(attp[0], "version") == 0)
				ver = attp[1];
		if (ver == NULL) {
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
	} else if (strcmp(s, "file") == 0) {
		if (p->set == NULL) {
			lerr(p->fname, p->p, "<file> not in <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		if (p->set->trglang != NULL) {
			lerr(p->fname, p->p, "nested <file>");
			XML_StopParser(p->p, 0);
			return;
		}
		for (attp = atts; *attp != NULL; attp += 2)
			if (0 == strcmp(attp[0], "target-language")) {
				free(p->set->trglang);
				p->set->trglang = strdup(attp[1]);
				if (p->set->trglang == NULL)
					err(EXIT_FAILURE, "strdup");
			}
		if (p->set->trglang == NULL) {
			lerr(p->fname, p->p, "<file> "
				"target-language not given");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (strcmp(s, "trans-unit") == 0) {
		if (p->set->trglang == NULL) {
			lerr(p->fname, p->p, 
				"<trans-unit> not in <file>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->curunit != NULL) {
			lerr(p->fname, p->p, 
				"nested <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}

		id = NULL;
		for (attp = atts; *attp != NULL; attp += 2)
			if (strcmp(attp[0], "id") == 0) {
				id = attp[1];
				break;
			}
		if (id == NULL) {
			lerr(p->fname, p->p, 
				"<trans-unit> without id");
			XML_StopParser(p->p, 0);
			return;
		}

		p->set->u = reallocarray
			(p->set->u, p->set->usz + 1,
			 sizeof(struct xliffunit));
		if (p->set->u == NULL)
			err(EXIT_FAILURE, "reallocarray");
		p->curunit = &p->set->u[p->set->usz++];
		memset(p->curunit, 0, sizeof(struct xliffunit));
		p->curunit->name = strdup(id);
		if (p->curunit->name == NULL)
			err(EXIT_FAILURE, "strdup");
	} else if (strcmp(s, "source") == 0) {
		if (p->curunit == NULL) {
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
	} else if (strcmp(s, "target") == 0) {
		if (p->curunit == NULL) {
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

	if (strcmp(s, "trans-unit") == 0) {
		assert(p->curunit != NULL);
		if (p->curunit->source == NULL || 
		    p->curunit->target == NULL) {
			lerr(p->fname, p->p, "missing <source> "
				"or <target> in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->curunit = NULL;
	} else if (strcmp(s, "target") == 0) {
		assert(p->curunit != NULL);
		p->type = 0;
	} else if (strcmp(s, "source") == 0) {
		assert(p->curunit != NULL);
		p->type = 0;
	}
}

/*
 * Parse an XLIFF file symbolically named "fn".
 * Returns NULL on failure or the xliffset on success.
 */
static struct xliffset *
xliff_read(FILE *f, const char *fn, XML_Parser p)
{
	struct xparse	*xp;
	struct xliffset	*res = NULL;
	size_t		 sz;
	int		 rc;
	char		 buf[BUFSIZ];

	xp = xparse_alloc(fn, p);
	assert(xp != NULL);

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);

	do {
		sz = fread(buf, 1, sizeof(buf), f);
		if (ferror(f))
			return NULL;
		rc = XML_Parse(p, buf, sz, feof(f));
		if (rc != XML_STATUS_OK) {
			lerr(fn, p, "%s", 
				XML_ErrorString
				(XML_GetErrorCode(p)));
			xparse_free(xp);
			return NULL;
		}
	} while (!feof(f));

	res = xp->set;
	xp->set = NULL;
	xparse_free(xp);
	return res;
}

static int
xliffunit_sort(const void *a1, const void *a2)
{
	struct xliffunit *p1 = (struct xliffunit *)a1,
		 	 *p2 = (struct xliffunit *)a2;

	return strcmp(p1->source, p2->source);
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

	if (l == NULL && type == NULL) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation\n",
			pos->fname, pos->line, pos->column);
		return 0;
	} else if (l == NULL) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (strcmp(x->u[i].source, l->label) == 0)
			break;

	if (i == x->usz && type == NULL) {
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

	assert(targ != NULL);

	/* 
	 * We have what we want to translate, now make sure that we're
	 * not overriding an existing translation.
	 */

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == lang)
			break;

	if (l != NULL && type == NULL) {
		fprintf(stderr, "%s:%zu:%zu: not "
			"overriding existing translation\n",
			pos->fname, pos->line, pos->column);
		return 1;
	} else if (l != NULL) {
		fprintf(stderr, "%s:%zu:%zu: not "
			"overriding existing translation "
			"for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 1;
	}

	/* Add the translation. */

	if ((l = calloc(1, sizeof(struct label))) == NULL)
		err(EXIT_FAILURE, "calloc");
	l->lang = lang;
	if ((l->label = strdup(targ)) == NULL)
		err(EXIT_FAILURE, "calloc");
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

	if (l == NULL && type == NULL) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation\n",
			pos->fname, pos->line, pos->column);
		return 0;
	} else if (l == NULL) {
		fprintf(stderr, "%s:%zu:%zu: no "
			"default translation for \"%s\" clause\n",
			pos->fname, pos->line, pos->column, type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (strcmp(x->u[i].source, l->label) == 0)
			break;

	if (i == x->usz) {
		x->u = reallocarray(x->u, 
			x->usz + 1, sizeof(struct xliffunit));
		if (x->u == NULL)
			err(EXIT_FAILURE, "reallocarray");
		u = &x->u[x->usz++];
		snprintf(nbuf, sizeof(nbuf), "%zu", x->usz);
		memset(u, 0, sizeof(struct xliffunit));
		if ((u->name = strdup(nbuf)) == NULL)
			err(EXIT_FAILURE, "strdup");
		if ((u->source = strdup(l->label)) == NULL)
			err(EXIT_FAILURE, "strdup");
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

/*
 * Parse XLIFF files with existing translations and update them (using a
 * single file) with new labels in "cfg".
 * Returns zero on failure, non-zero on success.
 */
static int
xliff_update(const struct ort_lang_xliff *args, 
	struct config *cfg, FILE *f)
{
	struct xliffset	*x;
	int		 rc = 0;
	size_t		 i;
	XML_Parser	 p;
	struct enm 	*e;
	struct eitem 	*ei;
	struct bitf	*b;
	struct bitidx	*bi;

	if ((p = XML_ParserCreate(NULL)) == NULL)
		return 0;

	assert(args->insz == 1);
	x = xliff_read(args->in[0], args->fnames[0], p);
	if (x == NULL)
		goto out;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!xliff_update_unit
			    (&ei->labels, NULL, x, &ei->pos))
				goto out;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			if (!xliff_update_unit
			    (&bi->labels, NULL, x, &bi->pos))
				goto out;
		if (!xliff_update_unit
		    (&b->labels_unset, "isunset", x, &b->pos))
			goto out;
		if (!xliff_update_unit
		    (&b->labels_null, "isnull", x, &b->pos))
			goto out;
	}

	qsort(x->u, x->usz, sizeof(struct xliffunit), xliffunit_sort);

	if (fprintf(f, 
	    "<xliff version=\"1.2\" "
	    "xmlns=\"urn:oasis:names:tc:xliff:document:1.2\">\n"
	    "\t<file target-language=\"%s\">\n"
            "\t\t<body>\n", x->trglang) < 0)
		goto out;

	for (i = 0; i < x->usz; i++)
		if (x->u[i].target == NULL && 
		    (args->flags & ORT_LANG_XLIFF_COPY)) {
			if (fprintf(f,
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, x->u[i].source,
			    x->u[i].source) < 0)
				goto out;
		} else if (x->u[i].target == NULL) {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, x->u[i].source) < 0)
				goto out;
		} else {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, x->u[i].source,
			    x->u[i].target) < 0)
				goto out;
		}

	if (fputs("\t\t</body>\n\t</file>\n</xliff>\n", f) == EOF)
		goto out;

	rc = 1;
out:
	xparse_xliff_free(x);
	XML_ParserFree(p);
	return rc;
}

/*
 * Parse an XLIFF file with open descriptor "xml" and merge the
 * translations with labels in "cfg".
 * Returns zero on XLIFF parse failure or merge failure, non-zero on
 * success.
 */
static int
xliff_join_single(struct config *cfg, int copy, 
	FILE *xml, const char *fn, XML_Parser p)
{
	struct xliffset	*x;
	size_t		 i;

	if ((x = xliff_read(xml, fn, p)) == NULL)
		return 0;

	assert(x->trglang != NULL);
	for (i = 0; i < cfg->langsz; i++)
		if (strcmp(cfg->langs[i], x->trglang) == 0)
			break;

	if (i == cfg->langsz) {
		cfg->langs = reallocarray(cfg->langs,
			 cfg->langsz + 1, sizeof(char *));
		if (cfg->langs == NULL)
			err(EXIT_FAILURE, "reallocarray");
		cfg->langs[cfg->langsz] = strdup(x->trglang);
		if (cfg->langs[cfg->langsz] == NULL)
			err(EXIT_FAILURE, "strdup");
		cfg->langsz++;
	} else
		fprintf(stderr, "%s: language \"%s\" is "
			"already noted\n", fn, x->trglang);

	if (!xliff_join_xliff(cfg, copy, i, x)) {
		xparse_xliff_free(x);
		return 0;
	}

	xparse_xliff_free(x);
	return 1;
}

/*
 * Join XLIFF files to configuration "cfg" and emit on "f".
 * Returns zero on failure, non-zero on success.
 * On success, the output is printed.
 */
static int
xliff_join(const struct ort_lang_xliff *args,
	struct config *cfg, FILE *f)
{
	int	 	 rc = 1;
	size_t		 i;
	XML_Parser	 p;

	if ((p = XML_ParserCreate(NULL)) == NULL)
		return 0;

	for (i = 0; rc && i < args->insz; i++) 
		rc = xliff_join_single(cfg, 
			args->flags & ORT_LANG_XLIFF_COPY, 
			args->in[i], args->fnames[i], p);

	XML_ParserFree(p);

	if (rc)
		ort_write_file(f, cfg);
	return rc;
}

int
main(int argc, char *argv[])
{
	struct ort_lang_xliff	  args;
	FILE			**confs = NULL;
	FILE			 *defin = stdin;
	const char		 *deffname = "<stdin>";
	struct config		 *cfg = NULL;
#define	OP_EXTRACT		  0
#define	OP_JOIN			  1
#define	OP_UPDATE		 (-1)
	int			  c, op = OP_EXTRACT, rc = 0;
	size_t			  confsz = 0, i, j, xmlstart = 0;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	memset(&args, 0, sizeof(struct ort_lang_xliff));

	while ((c = getopt(argc, argv, "cju")) != -1)
		switch (c) {
		case 'c':
			args.flags |= ORT_LANG_XLIFF_COPY;
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

	/*
	 * Open all of our files prior to the pledge(2), failing
	 * immediately if any open fails.
	 * This makes cleanup easier.
	 */

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
		args.insz = argc - i;

		if (confsz == 0 && args.insz == 0)
			goto usage;

		/* If we have 2 w/o -x, it's old-new. */

		if (args.insz == 0 && argc == 2)
			args.insz = confsz = xmlstart = 1;

		if (OP_UPDATE == op && args.insz > 1)
			goto usage;

		args.fnames = (const char **)&argv[xmlstart];

		if (args.insz > 0 &&
		    (args.in = calloc(args.insz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, "calloc");
		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, "calloc");

		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
		if (i < (size_t)argc && 0 == strcmp(argv[i], "-x"))
			i++;
		for (j = 0; i < (size_t)argc; j++, i++)
			if ((args.in[j] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);

		/* Handle stdin case. */

		if (args.insz == 0) {
			args.insz = 1;
			args.fnames = &deffname;
			args.in = &defin;
		}

	} else {
		confsz = (size_t)argc;
		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
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
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if (!ort_parse_close(cfg))
		goto out;

	/* Operate on the XLIFF/configuration file(s). */

	switch (op) {
	case OP_EXTRACT:
		rc = ort_lang_xliff_extract(&args, cfg, stdout);
		break;
	case OP_JOIN:
		rc = xliff_join(&args, cfg, stdout);
		break;
	case OP_UPDATE:
		rc = xliff_update(&args, cfg, stdout);
		break;
	}

	if (!rc)
		warn(NULL);
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_config_free(cfg);

	for (i = 0; i < args.insz; i++)
		fclose(args.in[i]);
	for (i = 0; i < confsz; i++)
		fclose(confs[i]);

	free(confs);
	free(args.in);
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
