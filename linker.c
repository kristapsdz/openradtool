#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Check the source, which must fall within the same structure
 * (obviously) as the referrent.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "source" field for the referrent.
 */
static int
checksource(struct ref *ref, struct strct *s)
{
	struct field	*f;

	TAILQ_FOREACH(f, &s->fq, entries) {
		if (strcmp(f->name, ref->sfield))
			continue;
		ref->source = f;
		return(1);
	}

	warnx("%s: unknown source", ref->sfield);
	return(0);
}

/*
 * Check the target structure and field.
 * Return zero on failure, non-zero on success.
 * On success, this sets the "target" field for the referrent.
 */
static int
checktarget(struct ref *ref, struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	TAILQ_FOREACH(p, q, entries) {
		if (strcmp(p->name, ref->tstrct))
			continue;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (strcmp(f->name, ref->tfield))
				continue;
			ref->target = f;
			return(1);
		}
	}

	warnx("%s.%s: unknown target",
		ref->tstrct, ref->tfield);
	return(0);
}

int
parse_link(struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	TAILQ_FOREACH(p, q, entries)
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (FTYPE_REF != f->type)
				continue;
			if ( ! checksource(f->ref, p) ||
			     ! checktarget(f->ref, q))
				return(0);
			return(1);
		}

	return(0);
}
