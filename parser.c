#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct strctq *
parse_config(FILE *f, const char *fname)
{
	struct strctq	*q;
	struct strct	*p;
	struct field	*fp;

	q = malloc(sizeof(struct strctq));
	if (NULL == q)
		err(EXIT_FAILURE, NULL);
	TAILQ_INIT(q);

	p = calloc(1, sizeof(struct strct));
	TAILQ_INSERT_TAIL(q, p, entries);
	p->name = strdup("foo");
	if (NULL == p->name)
		err(EXIT_FAILURE, NULL);
	TAILQ_INIT(&p->fq);

	fp = calloc(1, sizeof(struct field));
	TAILQ_INSERT_TAIL(&p->fq, fp, entries);
	fp->name = strdup("bar");
	fp->type = FTYPE_INT;

	fp = calloc(1, sizeof(struct field));
	TAILQ_INSERT_TAIL(&p->fq, fp, entries);
	fp->name = strdup("baz");
	fp->type = FTYPE_TEXT;

	return(q);
}

void
parse_free(struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	if (NULL == q)
		return;

	while (NULL != (p = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, p, entries);
		while (NULL != (f = TAILQ_FIRST(&p->fq))) {
			TAILQ_REMOVE(&p->fq, f, entries);
			free(f->name);
			free(f);
		}
		free(p->name);
		free(p);
	}
	free(p);
}
