#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

static void
gen_fkeys(const struct field *f, int *first)
{

	if (FTYPE_STRUCT == f->type || NULL == f->ref)
		return;

	printf("%s\n\tFOREIGN KEY(%s) REFERENCES %s(%s)",
		*first ? "" : ",",
		f->ref->source->name,
		f->ref->target->parent->name,
		f->ref->target->name);
	*first = 0;
}

static void
gen_field(const struct field *f, int *first)
{

	switch (f->type) {
	case (FTYPE_INT):
		printf("%s\n\t%s INTEGER", 
			*first ? "" : ",", f->name);
		if (FIELD_ROWID & f->flags)
			printf(" PRIMARY KEY");
		*first = 0;
		break;
	case (FTYPE_TEXT):
		printf("%s\n\t%s TEXT", 
			*first ? "" : ",", f->name);
		*first = 0;
		break;
	default:
		break;
	}
}

static void
gen_struct(const struct strct *p)
{
	const struct field *f;
	int	 first = 1;

	printf("CREATE TABLE %s (", p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_field(f, &first);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_fkeys(f, &first);
	puts("\n);\n"
	     "");
}

void
gen_sql(const struct strctq *q)
{
	const struct strct *p;

	TAILQ_FOREACH(p, q, entries)
		gen_struct(p);
}
