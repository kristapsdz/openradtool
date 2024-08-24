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
#include <sys/param.h>
#include <sys/stat.h>

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

#include "ort-paths.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "lang-c.h"

/*
 * Read a file into memory.
 * If the file contains NUL characters, these will prematurely end the
 * file when printed, as it's interpreted as a string.
 * Return the file contents (never fails).
 */
static char *
readfile(const char *dir, const char *fname)
{
	int		 fd;
	size_t		 sz;
	ssize_t		 ssz;
	struct stat	 st;
	char		*buf, *file;

	if (asprintf(&file, "%s/%s", dir, fname) == -1)
		err(1, NULL);

	if ((fd = open(file, O_RDONLY, 0)) == -1)
		err(1, "%s", file);
	if (fstat(fd, &st) == -1)
		err(1, "%s", file);

	/* FIXME: overflow check. */

	assert(st.st_size > 0 && (uint64_t)st.st_size < SIZE_MAX);

	sz = (size_t)st.st_size;
	if ((buf = malloc(sz + 1)) == NULL)
		err(1, NULL);
	if ((ssz = read(fd, buf, sz)) < 0)
		err(1, "%s", file);
	else if ((size_t)ssz != sz)
		errx(1, "%s: bad file length", file);
	buf[sz] = '\0';

	close(fd);
	free(file);
	return buf;
}

int
main(int argc, char *argv[])
{
	struct ort_lang_c	  args;
	const char		 *sharedir = SHAREDIR;
	struct config		 *cfg = NULL;
	int			  c, rc = 0;
	FILE			**confs = NULL;
	size_t			  i;
	char			 *ext_gensalt, *ext_jsmn, *ext_b64_ntop;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif

	memset(&args, 0, sizeof(struct ort_lang_c));
	args.header = "db.h";
	args.flags = ORT_LANG_C_DB_SQLBOX;

	while ((c = getopt(argc, argv, "h:I:jJN:S:v")) != -1)
		switch (c) {
		case 'h':
			args.header = optarg;
			if (*optarg == '\0')
				args.header = NULL;
			break;
		case 'I':
			if (strchr(optarg, 'd') != NULL)
				args.includes |= ORT_LANG_C_DB_SQLBOX;
			if (strchr(optarg, 'v') != NULL)
				args.includes |= ORT_LANG_C_VALID_KCGI;
			if (strchr(optarg, 'j') != NULL)
				args.includes |= ORT_LANG_C_JSON_KCGI;
			break;
		case 'j':
			args.flags |= ORT_LANG_C_JSON_KCGI;
			break;
		case 'J':
			args.flags |= ORT_LANG_C_JSON_JSMN;
			break;
		case 'N':
			if (strchr(optarg, 'd') != NULL)
				args.flags &= ~ORT_LANG_C_DB_SQLBOX;
			break;
		case 'S':
			sharedir = optarg;
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
	    (confs = calloc((size_t)argc, sizeof(FILE *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if ((confs[i] = fopen(argv[i], "r")) == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

	/* Files we might embed in source. */

	args.ext_gensalt = ext_gensalt = 
		readfile(sharedir, "gensalt.c");
	args.ext_b64_ntop = ext_b64_ntop = 
		readfile(sharedir, "b64_ntop.c");
	args.ext_jsmn = ext_jsmn = 
		readfile(sharedir, "jsmn.c");

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
		if (!(rc = ort_lang_c_source(&args, cfg, stdout)))
			warn(NULL);
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_config_free(cfg);

	for (i = 0; i < (size_t)argc; i++)
		fclose(confs[i]);

	free(confs);
	free(ext_gensalt);
	free(ext_b64_ntop);
	free(ext_jsmn);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s "
		"[-jJv] "
		"[-h header[,header...] "
		"[-I jJv] "
		"[-N d] "
		"[config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
