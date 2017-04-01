/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include "extern.h"

/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 *
 * FIXME: don't allow comment-end string.
 * FIXME: wrap at 72 characters.
 */
static void
gen_comment(const char *doc, size_t tabs)
{
	const char	*cp;
	size_t		 i;
	char		 last = '\0';

	if (NULL == doc)
		return;

	for (i = 0; i < tabs; i++)
		putchar('\t');
	puts("/*");
	for (i = 0; i < tabs; i++)
		putchar('\t');
	printf(" * ");

	for (cp = doc; '\0' != *cp; cp++) {
		if ('\n' == *cp) {
			putchar('\n');
			for (i = 0; i < tabs; i++)
				putchar('\t');
			printf(" * ");
			last = *cp;
			continue;
		}
		putchar(*cp);
		last = *cp;
	}
	if ('\n' != last)
		putchar('\n');
	for (i = 0; i < tabs; i++)
		putchar('\t');
	puts(" */");
}

/*
 * Generate the C API for a given field.
 */
static void
gen_strct_field(const struct field *p)
{

	gen_comment(p->doc, 1);

	switch (p->type) {
	case (FTYPE_STRUCT):
		printf("\tstruct %s %s;\n", 
			p->ref->tstrct, p->name);
		break;
	case (FTYPE_INT):
		printf("\tint64_t %s;\n", p->name);
		break;
	case (FTYPE_TEXT):
		printf("\tchar *%s;\n", p->name);
		break;
	}
}

/*
 * Generate the C API for a given structure.
 */
static void
gen_strct_structs(const struct strct *p)
{
	const struct field *f;

	gen_comment(p->doc, 0);

	printf("struct\t%s {\n", p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_field(f);
	puts("};\n"
	     "");
}

/*
 * Generate the function declarations for a given structure.
 */
static void
gen_strct_funcs(const struct strct *p)
{

	printf("struct %s *db_%s_get(void *);\n"
	       "void db_%s_free(struct %s *);\n"
	       "\n",
		p->name, p->name,
		p->name, p->name);
}

void
gen_header(const struct strctq *q)
{
	const struct strct *p;

	puts("#ifndef DB_H\n"
	     "#define DB_H\n"
	     "");

	TAILQ_FOREACH(p, q, entries)
		gen_strct_structs(p);

	puts("__BEGIN_DECLS\n"
	     "");

	TAILQ_FOREACH(p, q, entries)
		gen_strct_funcs(p);

	puts("__END_DECLS\n"
	     "\n"
	     "#endif /* ! DB_H */");
}
