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

	while (-1 != (c = getopt(argc, argv, "ch")))
		switch (c) {
		case ('c'):
			op = OP_SOURCE;
			break;
		case ('h'):
			op = OP_HEADER;
			break;
		default:
			goto usage;
		}

	if (NULL == confile) {
		confile = "<stdin>";
		conf = stdin;
	} else if (NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	sq = parse_config(conf, confile);
	fclose(conf);

	if (NULL == sq)
		errx(EXIT_FAILURE, "%s: no structures", confile);

	if (OP_SOURCE == op)
		gen_source(sq);
	else
		gen_header(sq);

	parse_free(sq);
	return(EXIT_SUCCESS);

usage:
	fprintf(stderr, "usage: %s [-ch] [config]\n",
		getprogname());
	return(EXIT_FAILURE);
}
