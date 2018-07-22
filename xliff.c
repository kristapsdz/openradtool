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

enum	xnesttype {
	NEST_TARGET,
	NEST_SOURCE
};

struct	xliff {
	char		 *source; /* key */
	char		 *target; /* value */
};

/*
 * Parse tracker for an XLIFF file that we're parsing.
 * This will be passed in (assuming success) to the "struct hparse" as
 * the xliffs pointer.
 */
struct	xparse {
	XML_Parser	  p;
	const char	 *fname; /* xliff filename */
	struct xliff	 *xliffs; /* current xliffs */
	size_t		  xliffsz; /* current size of xliffs */
	size_t		  xliffmax; /* xliff buffer size */
	char		 *source; /* current source in segment */
	char		 *target; /* current target in segment */
	size_t		  nest; /* nesting in extraction */
	enum xnesttype	  nesttype; /* type of nesting */
	char	 	 *srclang; /* <xliff> srcLang definition */
	char	 	 *trglang; /* <xliff> trgLang definition */
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

static void
perr(const char *fn, XML_Parser p)
{

	lerr(fn, p, "%s", XML_ErrorString(XML_GetErrorCode(p)));
}

static struct xparse *
xparse_alloc(const char *xliff, XML_Parser p)
{
	struct xparse	*xp;

	if (NULL == (xp = calloc(1, sizeof(struct xparse))))
		err(EXIT_FAILURE, NULL);

	xp->fname = xliff;
	xp->p = p;
	return(xp);
}

static void
xparse_free(struct xparse *xp)
{
	size_t	 i;

	for (i = 0; i < xp->xliffsz; i++) {
		free(xp->xliffs[i].source);
		free(xp->xliffs[i].target);
	}

	free(xp->target);
	free(xp->source);
	free(xp->xliffs);
	free(xp->srclang);
	free(xp->trglang);
	free(xp);
}

static void
xtext(void *dat, const XML_Char *s, int len)
{
}

/*
 * Start an element in an XLIFF parse sequence.
 * We only care about <source>, <target>, and <segment>.
 * For the former two, start buffering for content.
 */
static void
xstart(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct xparse	 *p = dat;
	const XML_Char	**attp;
	const char	 *ver;

	if (0 == strcmp(s, "xliff")) {
		ver = NULL;
		for (attp = atts; NULL != *attp; attp += 2) 
			if (0 == strcmp(attp[0], "version"))
				ver = attp[1];
		if (NULL == ver) {
			lerr(p->fname, p->p, "<xliff> without version");
			XML_StopParser(p->p, 0);
			return;
		} else if (strcmp(ver, "1.2")) {
			lerr(p->fname, p->p, "<xliff> version must be 1.2");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (0 == strcmp(s, "file")) {
		if (NULL != p->srclang || NULL != p->trglang) {
			lerr(p->fname, p->p, "<file> already invoked");
			XML_StopParser(p->p, 0);
			return;
		}
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcmp(attp[0], "source-language")) {
				free(p->srclang);
				p->srclang = strdup(attp[1]);
				if (NULL == p->srclang)
					err(EXIT_FAILURE, NULL);
			} else if (0 == strcmp(attp[0], "target-language")) {
				free(p->trglang);
				p->trglang = strdup(attp[1]);
				if (NULL == p->trglang)
					err(EXIT_FAILURE, NULL);
			}
	} else if (0 == strcmp(s, "source")) {
		p->nest = 1;
		p->nesttype = NEST_SOURCE;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else if (0 == strcmp(s, "target")) {
		p->nest = 1;
		p->nesttype = NEST_TARGET;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else if (0 == strcmp(s, "trans-unit"))
		p->source = p->target = NULL;
}

/*
 * Finished a <source>, <target>, or <segment>.
 * For the former tow, we want to verify that we've buffered a real word
 * we're going to use.
 * For the latter, we want both terms in place.
 * We then add it to the dictionary of translations.
 */
static void
xend(void *dat, const XML_Char *s)
{
	struct xparse	*p = dat;

	XML_SetDefaultHandlerExpand(p->p, NULL);

	if (0 == strcmp(s, "trans-unit")) {
		if (NULL == p->source || NULL == p->target) {
			lerr(p->fname, p->p, "no <source> or <target>");
			free(p->source);
			free(p->target);
			p->source = p->target = NULL;
			return;
		}
		if (p->xliffsz + 1 > p->xliffmax) {
			p->xliffmax += 512;
			p->xliffs = reallocarray
				(p->xliffs, 
				 p->xliffmax,
				 sizeof(struct xliff));
			if (NULL == p->xliffs)
				err(EXIT_FAILURE, NULL);
		}
		p->xliffs[p->xliffsz].source = p->source;
		p->xliffs[p->xliffsz].target = p->target;
		p->source = p->target = NULL;
		p->xliffsz++;
	}
}

static int
map_open(const char *fn, size_t *mapsz, char **map)
{
	struct stat	 st;
	int	 	 fd;

	if (-1 == (fd = open(fn, O_RDONLY))) {
		perror(fn);
		return(-1);
	} else if (-1 == fstat(fd, &st)) {
		perror(fn);
		close(fd);
		return(-1);
	} else if ( ! S_ISREG(st.st_mode)) {
		fprintf(stderr, "%s: not regular\n", fn);
		close(fd);
		return(-1);
	} else if (st.st_size >= (1U << 31)) {
		fprintf(stderr, "%s: too large\n", fn);
		close(fd);
		return(-1);
	}

	*mapsz = st.st_size;
	*map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

	if (MAP_FAILED == *map) {
		perror(fn);
		close(fd);
		fd = -1;
	}

	return(fd);
}

static void
map_close(int fd, void *map, size_t mapsz)
{

	munmap(map, mapsz);
	close(fd);
}

/*
 * Invoke the HTML5 parser on a series of files; or if no files are
 * specified, as read from standard input.
 */
static int
scanner(int argc, char *argv[])
{
}

/*
 * Extract all translatable strings from argv and create an XLIFF file
 * template from the results.
 */
static int
extract(XML_Parser p, int argc, char *argv[])
{
}

/*
 * Translate the files in argv with the dictionary in xliff, echoing the
 * translated versions.
 */
static int
join(const char *xliff, XML_Parser p, int argc, char *argv[])
{
	struct xparse	*xp;
	char		*map;
	size_t		 mapsz;
	int		 fd, rc;

	if (-1 == (fd = map_open(xliff, &mapsz, &map)))
		return(0);

	xp = xparse_alloc(xliff, p);

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);
	rc = XML_Parse(p, map, mapsz, 1);
	map_close(fd, map, mapsz);

	if (XML_STATUS_OK == rc) {
	} else
		perr(xliff, p);

	xparse_free(xp);
	return(XML_STATUS_OK == rc);
}

/*
 * Update (not in-line) the dictionary file xliff with the contents of
 * argv, outputting the merged XLIFF file.
 */
static int
update(const char *xliff, XML_Parser p, int keep, int argc, char *argv[])
{
	struct xparse	*xp;
	char		*map;
	size_t		 mapsz;
	int		 fd, rc;

	if (-1 == (fd = map_open(xliff, &mapsz, &map)))
		return(0);

	xp = xparse_alloc(xliff, p);

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);
	rc = XML_Parse(p, map, mapsz, 1);

	map_close(fd, map, mapsz);

	if (XML_STATUS_OK == rc) {
	} else
		perr(xliff, p);

	xparse_free(xp);
	return(XML_STATUS_OK == rc);
}

static void
gen_xliff(const struct config *cfg, size_t argc, const char **argv)
{

	parse_write(stdout, cfg);
}

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL;
	const char	*confile = NULL;
	struct config	*cfg;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	if (-1 != getopt(argc, argv, ""))
		goto usage;

	argc -= optind;
	argv += optind;

	if (0 == argc) {
		confile = "<stdin>";
		conf = stdin;
	} else
		confile = argv[0];

	if (NULL == conf &&
	    NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	cfg = parse_config(conf, confile);
	fclose(conf);

	if (NULL == cfg || ! parse_link(cfg)) {
		parse_free(cfg);
		return EXIT_FAILURE;
	}

	gen_xliff(cfg, argc, (const char **)argv);
	parse_free(cfg);
	return EXIT_SUCCESS;
usage:
	fprintf(stderr, 
		"usage: %s [config]\n",
		getprogname());
	return EXIT_FAILURE;
}
