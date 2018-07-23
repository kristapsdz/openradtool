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
#include <sys/mman.h>
#include <sys/stat.h>

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

#include "extern.h"

struct	xliffunit {
	char		*name;
	char		*source;
	char		*target;
	size_t		 xsz;
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
	const struct config *cfg;
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

	if (NULL != p->set) 
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
			strlcpy(p->curunit->source + sz,
				s, len + 1);
		} else {
			p->curunit->source = 
				strndup(s, len);
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
			strlcpy(p->curunit->target + sz,
				s, len + 1);
		} else {
			p->curunit->target = 
				strndup(s, len);
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

static int
map_open(const char *fn, size_t *mapsz, char **map)
{
	struct stat	 st;
	int	 	 fd;

	if (-1 == (fd = open(fn, O_RDONLY, 0))) {
		warn("%s", fn);
		return -1;
	} else if (-1 == fstat(fd, &st)) {
		warn("%s", fn);
		close(fd);
		return -1;
	} else if ( ! S_ISREG(st.st_mode)) {
		warnx("%s: not regular", fn);
		close(fd);
		return -1;
	} else if (st.st_size >= (1U << 31)) {
		warnx("%s: too large", fn);
		close(fd);
		return -1;
	}

	*mapsz = st.st_size;
	*map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == *map) {
		warn("%s", fn);
		close(fd);
		fd = -1;
	}

	return fd;
}

static void
map_close(int fd, void *map, size_t mapsz)
{

	munmap(map, mapsz);
	close(fd);
}

static struct xliffset *
xliff_read(const struct config *cfg, const char *fn, XML_Parser p)
{
	struct xparse	*xp;
	struct xliffset	*res = NULL;
	char		*map;
	size_t		 mapsz;
	int		 fd, rc;

	if (-1 == (fd = map_open(fn, &mapsz, &map)))
		return NULL;

	xp = xparse_alloc(fn, p);
	assert(NULL != xp);
	xp->cfg = cfg;

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);
	rc = XML_Parse(p, map, mapsz, 1);

	map_close(fd, map, mapsz);

	if (XML_STATUS_OK == rc) {
		res = xp->set;
		xp->set = NULL;
	} else
		lerr(fn, p, "%s", XML_ErrorString
			(XML_GetErrorCode(p)));

	xparse_free(xp);
	return res;
}

static void
xliff_print_unit(const struct labelq *lq, 
	const struct pos *pos, size_t *id)
{
	const struct label *l;

	TAILQ_FOREACH(l, lq, entries)
		if (0 == l->lang)
			break;

	if (NULL == l) {
		fprintf(stderr, "%s:%zu:%zu: missing "
			"jslabel for translation\n",
			pos->fname, pos->line, pos->column);
		return;
	}

	printf("\t\t\t<trans-unit id=\"%zu\">\n"
	       "\t\t\t\t<source>%s</source>\n"
	       "\t\t\t\t<target>TODO</target>\n"
	       "\t\t\t</trans-unit>\n",
	       ++(*id), l->label);
}

static int
xliff_extract(const struct config *cfg)
{
	const struct enm *e;
	const struct eitem *ei;
	size_t		 i = 0;

	printf("<xliff version=\"1.2\">\n"
	       "\t<file target-language=\"TODO\" "
	          "tool=\"kwebapp-xliff\">\n"
	       "\t\t<body>\n");

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			xliff_print_unit(&ei->labels, &ei->pos, &i);

	puts("\t\t</body>\n"
	     "\t</file>\n"
	     "</xliff>");

	return 1;
}

static void
xliff_join_xliff(struct config *cfg, 
	size_t lang, const struct xliffset *x)
{
	struct enm 	*e;
	struct eitem 	*ei;
	size_t		 i;
	struct label	*l;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries) {
			TAILQ_FOREACH(l, &ei->labels, entries)
				if (l->lang == 0)
					break;
			if (NULL == l) {
				fprintf(stderr, "%s:%zu:%zu: no "
					"default translation\n",
					ei->pos.fname, 
					ei->pos.line, 
					ei->pos.column);
				continue;
			}

			for (i = 0; i < x->usz; i++)
				if (0 == strcmp(x->u[i].source, l->label))
					break;

			if (i == x->usz) {
				fprintf(stderr, "%s:%zu:%zu: missing "
					"translation\n",
					ei->pos.fname, 
					ei->pos.line, 
					ei->pos.column);
				continue;
			}

			TAILQ_FOREACH(l, &ei->labels, entries)
				if (l->lang == lang)
					break;

			if (NULL != l) {
				fprintf(stderr, "%s:%zu:%zu: not "
					"overriding existing\n",
					ei->pos.fname, 
					ei->pos.line, 
					ei->pos.column);
				continue;
			}

			l = calloc(1, sizeof(struct label));
			l->lang = lang;
			l->label = strdup(x->u[i].target);
			if (NULL == l->label)
				err(EXIT_FAILURE, NULL);
			TAILQ_INSERT_TAIL(&ei->labels, l, entries);
		}
}

static int
xliff_join(struct config *cfg, size_t argc, const char **argv)
{
	struct xliffset	*x;
	size_t		 i;
	XML_Parser	 p;

	if (NULL == (p = XML_ParserCreate(NULL))) {
		warn("XML_ParserCreate");
		return 0;
	}

	for (i = 0; i < argc; i++) {
		x = xliff_read(cfg, argv[i], p);
		if (NULL == x)
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
			fprintf(stderr, "%s: language \"%s\" "
				"is already noted\n",
				argv[i], x->trglang);

		xliff_join_xliff(cfg, i, x);
		xparse_xliff_free(x);
	}

	XML_ParserFree(p);
	parse_write(stdout, cfg);
	return 1;
}

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL;
	const char	*confile = NULL;
	struct config	*cfg;
	int		 c, op = 0;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	while (-1 != (c = getopt(argc, argv, "j:")))
		switch (c) {
		case 'j':
			op = 1;
			confile = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == op) {
		if (0 == argc) {
			confile = "<stdin>";
			conf = stdin;
		} else
			confile = argv[0];
	}

	if (NULL == conf &&
	    NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

	cfg = parse_config(conf, confile);
	fclose(conf);

	if (NULL == cfg || ! parse_link(cfg)) {
		parse_free(cfg);
		return EXIT_FAILURE;
	}

	c = 0 == op ? 
		xliff_extract(cfg) :
		xliff_join(cfg, argc, (const char **)argv);

	parse_free(cfg);
	return c ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s -j config xliffs...\n"
		"       %s [config]\n",
		getprogname(), getprogname());
	return EXIT_FAILURE;
}
