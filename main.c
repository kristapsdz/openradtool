#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

enum	op {
	OP_NOOP,
	OP_HEADER,
	OP_SOURCE
};

int
main(int argc, char *argv[])
{
	FILE		*conf;
	const char	*confile = NULL;
	struct strctq	*sq;
	int		 c;
	enum op		 op = OP_HEADER;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	while (-1 != (c = getopt(argc, argv, "chn")))
		switch (c) {
		case ('c'):
			op = OP_SOURCE;
			break;
		case ('h'):
			op = OP_HEADER;
			break;
		case ('n'):
			op = OP_NOOP;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) {
		confile = "<stdin>";
		conf = stdin;
	} else if (argc > 1)
		goto usage;

	confile = argv[0];
	if (NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	/*
	 * First, parse the file.
	 * This pulls all of the data from the configuration file.
	 * If there are any errors, it will return NULL.
	 */

	sq = parse_config(conf, confile);
	fclose(conf);

	/*
	 * After parsing, we need to link.
	 * Linking connects foreign key references.
	 */

	if (NULL == sq || ! parse_link(sq)) {
		parse_free(sq);
		return(EXIT_FAILURE);
	}

	/* Finally, (optionally) generate output. */

	if (OP_SOURCE == op)
		gen_source(sq);
	else if (OP_HEADER == op)
		gen_header(sq);

	parse_free(sq);
	return(EXIT_SUCCESS);

usage:
	fprintf(stderr, "usage: %s [-chn] [config]\n",
		getprogname());
	return(EXIT_FAILURE);
}
