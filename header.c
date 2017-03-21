#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

static void
gen_strct_field(const struct field *p)
{

	switch (p->type) {
	case (FTYPE_INT):
		printf("\tint64_t %s;\n", p->name);
		break;
	case (FTYPE_TEXT):
		printf("\tchar *%s;\n", p->name);
		break;
	}
}

static void
gen_strct_structs(const struct strct *p)
{
	const struct field *f;

	/* Free object. */

	printf("struct\t%s {\n", p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_field(f);
	puts("};");
	puts("");
}

static void
gen_strct_funcs(const struct strct *p)
{

	/* Free object. */

	printf("void db_%s_free(struct %s *);\n",
		p->name, p->name);
	puts("");
}

void
gen_header(const struct strctq *q)
{
	const struct strct *p;

	puts("#ifndef DB_H");
	puts("#define DB_H");
	puts("");

	TAILQ_FOREACH(p, q, entries)
		gen_strct_structs(p);

	puts("__BEGIN_DECLS");
	puts("");

	TAILQ_FOREACH(p, q, entries)
		gen_strct_funcs(p);

	puts("__END_DECLS");

	puts("#endif /* ! DB_H */");
}
