/*	$Id$ */
/*
 * Copyright (c) 2017, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kwebapp.h"

int
main(int argc, char *argv[])
{
	struct config	 *cfg = NULL;
	size_t		  i, confsz;
	int		  rc = 0;
	FILE		**confs = NULL;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	if (-1 != getopt(argc, argv, ""))
		goto usage;

	argc -= optind;
	argv += optind;
	confsz = (size_t)argc;
	
	/* Read in all of our files now so we can repledge. */

	if (confsz > 0) {
		confs = calloc(confsz, sizeof(FILE *));
		if (NULL == confs)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < confsz; i++) {
			confs[i] = fopen(argv[i], "r");
			if (NULL == confs[i]) {
				warn("%s", argv[i]);
				goto out;
			}
		}
	}

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	cfg = kwbp_config_alloc();

	for (i = 0; i < confsz; i++)
		if ( ! kwbp_parse_file_r(cfg, confs[i], argv[i]))
			goto out;

	if (0 == confsz && 
	    ! kwbp_parse_file_r(cfg, stdin, "<stdin>"))
		goto out;

	/* Only echo output on success. */

	if (0 != (rc = parse_link(cfg)))
		parse_write(stdout, cfg);

out:
	for (i = 0; i < confsz; i++)
		if (NULL != confs[i] && EOF == fclose(confs[i]))
			warn("%s", argv[i]);
	free(confs);
	kwbp_config_free(cfg);

	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, "usage: %s [config...]\n", getprogname());
	return EXIT_FAILURE;
}
