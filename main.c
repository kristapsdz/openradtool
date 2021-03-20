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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"

int
main(int argc, char *argv[])
{
	struct config		 *cfg = NULL;
	size_t			  i;
	int			  rc = 0, c;
	FILE			**confs = NULL;
	struct ort_write_args	  args;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif
	memset(&args, 0, sizeof(struct ort_write_args));

	while ((c = getopt(argc, argv, "i")) != -1) 
		switch (c) {
		case 'i':
			args.flags |= ORT_WRITE_LOWERCASE;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;
	
	if (argc > 0 &&
	    (confs = calloc(argc, sizeof(FILE *))) == NULL)
		err(1, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if ((confs[i] = fopen(argv[i], "r")) == NULL)
			err(1, "%s", argv[i]);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");
#endif
	if ((cfg = ort_config_alloc()) == NULL)
		err(1, NULL);

	/* From here on out we use the "out" label to bail. */

	for (i = 0; i < (size_t)argc; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (argc == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		if (!(rc = ort_write_file(&args, stdout, cfg)))
			warn(NULL);
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_config_free(cfg);

	for (i = 0; i < (size_t)argc; i++)
		fclose(confs[i]);

	free(confs);
	return rc ? 0 : 1;
usage:
	fprintf(stderr, "usage: %s [-i] [config...]\n", getprogname());
	return 1;
}
