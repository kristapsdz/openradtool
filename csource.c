/*	$Id$ */
/*
 * Copyright (c) 2017--2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
# include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "paths.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "lang-c.h"

static	const char *const externals[EX__MAX] = {
	FILE_GENSALT, /* EX_GENSALT */
	FILE_B64_NTOP, /* EX_B64_NTOP */
	FILE_JSMN /* EX_JSMN */
};

int
main(int argc, char *argv[])
{
	const char	 *header = NULL, *incls = NULL, 
	     		 *sharedir = SHAREDIR;
	struct config	 *cfg = NULL;
	int		  c, json = 0, jsonparse = 0, valids = 0,
			  dbin = 1, rc = 0;
	FILE		**confs = NULL;
	size_t		  i, confsz;
	int		  exs[EX__MAX], sz;
	char		  buf[MAXPATHLEN];

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "h:I:jJN:sS:v")) != -1)
		switch (c) {
		case 'h':
			header = optarg;
			break;
		case 'I':
			incls = optarg;
			break;
		case 'j':
			json = 1;
			break;
		case 'J':
			jsonparse = 1;
			break;
		case 'N':
			if (strchr(optarg, 'd') != NULL)
				dbin = 0;
			break;
		case 's':
			/* Ignore. */
			break;
		case 'S':
			sharedir = optarg;
			break;
		case 'v':
			valids = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;
	confsz = (size_t)argc;
	
	/* Read in all of our files now so we can repledge. */

	if (confsz > 0) {
		if ((confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
	}

	/* 
	 * Open all of the source files we might optionally embed in the
	 * output source code.
	 */

	for (i = 0; i < EX__MAX; i++) {
		sz = snprintf(buf, sizeof(buf), 
			"%s/%s", sharedir, externals[i]);
		if (sz < 0 || (size_t)sz >= sizeof(buf))
			errx(EXIT_FAILURE, "%s/%s: too long",
				sharedir, externals[i]);
		if ((exs[i] = open(buf, O_RDONLY, 0)) == -1)
			err(EXIT_FAILURE, "%s/%s", sharedir, externals[i]);
	}

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		goto out;

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		rc = gen_c_source(cfg, json, jsonparse, 
			valids, dbin, header, incls, exs);

out:
	for (i = 0; i < EX__MAX; i++)
		if (close(exs[i]) == -1)
			warn("%s: close", externals[i]);
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s: close", argv[i]);
	free(confs);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s "
		"[-jJv] "
		"[-h header[,header...] "
		"[-I bjJv] "
		"[-N b] "
		"[config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
