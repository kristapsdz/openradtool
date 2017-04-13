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

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"int64_t ",
	"const char *",
	NULL,
};

void
print_func_open(int decl)
{

	printf("struct ksql *%sdb_open(const char *file)%s\n",
		decl ? "" : "\n", decl ? ";" : "");
}

/*
 * Generate the "update" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_update(const struct update *u, int decl)
{
	const struct uref *ur;
	size_t	 pos = 1;

	printf("int%sdb_%s_update",
		decl ? " " : "\n", u->parent->name);

	if (NULL == u->name) {
		TAILQ_FOREACH(ur, &u->mrq, entries)
			printf("_%s", ur->name);
		printf("_by");
		TAILQ_FOREACH(ur, &u->crq, entries)
			printf("_%s", ur->name);
	} else 
		printf("_%s", u->name);

	printf("(struct ksql *db");

	TAILQ_FOREACH(ur, &u->mrq, entries)
		printf(", %sv%zu", ftypes[ur->field->type], pos++);
	TAILQ_FOREACH(ur, &u->crq, entries)
		printf(", %sv%zu", ftypes[ur->field->type], pos++);

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the declaration for a search function "s".
 * The format of the declaration depends upon the search type.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * FIXME: line wrapping.
 */
void
print_func_search(const struct search *s, int decl)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos = 1;

	if (STYPE_SEARCH == s->type)
		printf("struct %s *%sdb_%s_by", 
			s->parent->name, decl ? "" : "\n", 
			s->parent->name);
	else if (STYPE_LIST == s->type)
		printf("struct %s_q *%sdb_%s_list_by", 
			s->parent->name, decl ? "" : "\n", 
			s->parent->name);
	else
		printf("void%sdb_%s_iterate_by",
			decl ? " " : "\n", s->parent->name);

	if (NULL == s->name) {
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			putchar('_');
			TAILQ_FOREACH(sr, &sent->srq, entries)
				printf("_%s", sr->name);
		}
	} else 
		printf("_%s", s->name);

	printf("(struct ksql *db");

	if (STYPE_ITERATE == s->type)
		printf(", %s_cb cb, void *arg", 
			s->parent->name);

	TAILQ_FOREACH(sent, &s->sntq, entries) {
		sr = TAILQ_LAST(&sent->srq, srefq);
		assert(NULL != ftypes[sr->field->type]);
		printf(", %sv%zu", 
			ftypes[sr->field->type], pos++);
	}

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the "insert" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_insert(const struct strct *p, int decl)
{
	const struct field *f;
	size_t	 pos = 1;

	printf("int%sdb_%s_insert(struct ksql *db",
		decl ? " " : "\n", p->name);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		printf(", %sv%zu", ftypes[f->type], pos++);
	}
	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the "freeq" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_freeq(const struct strct *p, int decl)
{

	assert(STRCT_HAS_QUEUE & p->flags);
	printf("void%sdb_%s_freeq(struct %s_q *q)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "free" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_free(const struct strct *p, int decl)
{

	printf("void%sdb_%s_free(struct %s *p)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "unfill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_unfill(const struct strct *p, int decl)
{

	printf("void%sdb_%s_unfill(struct %s *p)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "get by rowid" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_by_rowid(const struct strct *p, int decl)
{

	printf("struct %s *%sdb_%s_by_rowid"
	       "(struct ksql *db, int64_t id)%s",
	       p->name, decl ? "" : "\n", p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "fill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_fill(const struct strct *p, int decl)
{

	printf("void%sdb_%s_fill(struct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)%s",
	       decl ? " " : "\n",
	       p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 *
 * FIXME: don't allow comment-end string.
 * FIXME: wrap at 72 characters.
 */
static void
print_comment(const char *doc, size_t tabs, 
	const char *pre, const char *in, const char *post)
{
	const char	*cp;
	size_t		 i;
	char		 last = '\0';

	assert(NULL != in);

	if (NULL != pre) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(pre);
	}

	if (NULL != doc) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%s", in);

		for (cp = doc; '\0' != *cp; cp++) {
			if ('\n' == *cp) {
				putchar('\n');
				for (i = 0; i < tabs; i++)
					putchar('\t');
				printf("%s", in);
				last = *cp;
				continue;
			}
			if ('\\' == *cp && '"' == cp[1])
				cp++;
			putchar(*cp);
			last = *cp;
		}
		if ('\n' != last)
			putchar('\n');
	}

	if (NULL != post) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(post);
	}
}

/*
 * Print comments with a fixed string.
 */
void
print_commentt(size_t tabs, enum cmtt type, const char *cp)
{

	if (COMMENT_C == type) 
		print_comment(cp, tabs, "/*", " * ", " */");
	else if (COMMENT_C_FRAG == type) 
		print_comment(cp, tabs, NULL, " * ", NULL);
	else if (COMMENT_C_FRAG_CLOSE == type) 
		print_comment(cp, tabs, NULL, " * ", " */");
	else if (COMMENT_C_FRAG_OPEN == type) 
		print_comment(cp, tabs, "/*", " * ", NULL);
	else
		print_comment(cp, tabs, NULL, "-- ", NULL);
}

/*
 * Print comments with varargs.
 */
void
print_commentv(size_t tabs, enum cmtt type, const char *fmt, ...)
{
	va_list		 ap;
	char		*cp;

	va_start(ap, fmt);
	if (-1 == vasprintf(&cp, fmt, ap))
		err(EXIT_FAILURE, NULL);
	va_end(ap);
	print_commentt(tabs, type, cp);
	free(cp);
}
