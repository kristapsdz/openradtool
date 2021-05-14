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

#include "ort.h"
#include "ort-lang-c.h"
#include "lang-c.h"

int
main(int argc, char *argv[])
{
	struct ort_lang_c	  args;
	struct config		 *cfg = NULL;
	int			  c, rc = 0;
	FILE			**confs = NULL;
	size_t		  	  i;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	memset(&args, 0, sizeof(struct ort_lang_c));

	args.flags = ORT_LANG_C_CORE | ORT_LANG_C_DB_SQLBOX;
	args.guard = "DB_H";

	while ((c = getopt(argc, argv, "g:jJN:sv")) != -1)
		switch (c) {
		case 'g':
			args.guard = optarg[0] == '\0' ? NULL : optarg;
			break;
		case 'j':
			args.flags |= ORT_LANG_C_JSON_KCGI;
			break;
		case 'J':
			args.flags |= ORT_LANG_C_JSON_JSMN;
			break;
		case 'N':
			if (strchr(optarg, 'b') != NULL)
				args.flags &= ~ORT_LANG_C_CORE;
			if (strchr(optarg, 'd') != NULL)
				args.flags &= ~ORT_LANG_C_DB_SQLBOX;
			break;
		case 's':
			/* Ignore. */
			break;
		case 'v':
			args.flags |= ORT_LANG_C_VALID_KCGI;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;
	
	/* Read in all of our files now so we can repledge. */

	if (argc > 0 &&
	    (confs = calloc(argc, sizeof(FILE *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if ((confs[i] = fopen(argv[i], "r")) == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (argc == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		if (!(rc = ort_lang_c_header(&args, cfg, stdout)))
			warn(NULL);
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_config_free(cfg);

	for (i = 0; i < (size_t)argc; i++)
		fclose(confs[i]);

	free(confs);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s "
		"[-jJv] "
		"[-N[b|d]] "
		"[config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
