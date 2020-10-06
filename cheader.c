/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "lang-c.h"

int
main(int argc, char *argv[])
{
	const char	 *guard = "DB_H";
	struct config	 *cfg = NULL;
	int		  c, json = 0, valids = 0, rc = 0,
			  dbin = 1, dstruct = 1,
			  jsonparse = 0;
	FILE		**confs = NULL;
	size_t		  i, confsz;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "g:jJN:sv")) != -1)
		switch (c) {
		case 'g':
			guard = optarg;
			break;
		case 'j':
			json = 1;
			break;
		case 'J':
			jsonparse = 1;
			break;
		case 'N':
			if (strchr(optarg, 'b') != NULL)
				dstruct = 0;
			if (strchr(optarg, 'd') != NULL)
				dbin = 0;
			break;
		case 's':
			/* Ignore. */
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
		gen_c_header(cfg, guard, json, jsonparse,
			valids, dbin, dstruct);

out:
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s", argv[i]);
	free(confs);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s "
		"[-jJv] "
		"[-N bd] "
		"[config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
