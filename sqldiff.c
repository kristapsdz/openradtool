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

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "ort-lang-sql.h"

int
main(int argc, char *argv[])
{
	FILE		**confs = NULL, **dconfs = NULL;
	struct config	 *cfg = NULL, *dcfg = NULL;
	int		  rc = 0, c, destruct = 0;
	size_t		  confsz = 0, dconfsz = 0, i, j, 
			  confst = 0;
	struct diffq	 *q = NULL;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "d")) != -1)
		switch (c) {
		case 'd':
			destruct = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	/* Read up until "-f" (or argc) for old configs. */

	for (dconfsz = 0; dconfsz < (size_t)argc; dconfsz++)
		if (strcmp(argv[dconfsz], "-f") == 0)
			break;
	
	/* If we have >2 w/o -f, error (which old/new?). */

	if (dconfsz == (size_t)argc && argc > 2)
		goto usage;

	if ((i = dconfsz) < (size_t)argc)
		i++;

	confst = i;
	confsz = argc - i;

	/* If we have 2 w/o -f, it's old-new. */

	if (confsz == 0 && argc == 2)
		confsz = dconfsz = confst = 1;

	/* Need at least one configuration. */

	if (confsz + dconfsz == 0)
		goto usage;

	if (confsz > 0 &&
	    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
		err(EXIT_FAILURE, NULL);

	if (dconfsz > 0 &&
	    (dconfs = calloc(dconfsz, sizeof(FILE *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < dconfsz; i++)
		if ((dconfs[i] = fopen(argv[i], "r")) == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

	if (i < (size_t)argc && strcmp(argv[i], "-f") == 0)
		i++;

	for (j = 0; i < (size_t)argc; j++, i++)
		if ((confs[j] = fopen(argv[i], "r")) == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	assert(confsz + dconfsz > 0);

	if ((cfg = ort_config_alloc()) == NULL ||
	    (dcfg = ort_config_alloc()) == NULL)
		err(EXIT_FAILURE, NULL);

	/* Parse the input files themselves. */

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[confst + i]))
			goto out;
	for (i = 0; i < dconfsz; i++)
		if (!ort_parse_file(dcfg, dconfs[i], argv[i]))
			goto out;

	/* If we don't have input, parse from stdin. */

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;
	if (dconfsz == 0 && !ort_parse_file(dcfg, stdin, "<stdin>"))
		goto out;

	/* Linker. */

	if (!ort_parse_close(cfg))
		goto out;
	if (!ort_parse_close(dcfg))
		goto out;

	/* Generate diffs. */
	
	if ((q = ort_diff(dcfg, cfg)) == NULL)
		warn(NULL);
	else if (!(rc = ort_lang_diff_sql(q, destruct, stdout)))
		warn(NULL);
out:
	for (i = 0; i < confsz; i++)
		fclose(confs[i]);
	for (i = 0; i < dconfsz; i++)
		fclose(dconfs[i]);

	free(confs);
	free(dconfs);
	ort_diff_free(q);

	if (cfg != NULL)
		ort_write_msg_file(stderr, &cfg->mq);
	ort_config_free(cfg);
	if (dcfg != NULL)
		ort_write_msg_file(stderr, &dcfg->mq);
	ort_config_free(dcfg);

	return rc ? EXIT_SUCCESS : EXIT_FAILURE;

usage:
	fprintf(stderr, 
		"usage: %s [-d] oldconfig [config...]\n"
		"       %s [-d] [oldconfig...] -f [config...]\n",
		getprogname(), getprogname());
	return EXIT_FAILURE;
}
