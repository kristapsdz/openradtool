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
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"int64_t ",
	"const char *",
	NULL,
};

/*
 * Generate the listing for a search function "s".
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 *
 * FIXME: line wrapping.
 */
void
print_func_search(const struct search *s, int decl)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos = 1;

	printf("struct %s *%sdb_%s_by", 
		s->parent->name, decl ? "" : "\n", 
		s->parent->name);

	if (NULL == s->name) {
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			putchar('_');
			TAILQ_FOREACH(sr, &sent->srq, entries)
				printf("_%s", sr->name);
		}
	} else 
		printf("_%s", s->name);

	printf("(struct ksql *db");
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		sr = TAILQ_LAST(&sent->srq, srefq);
		assert(NULL != ftypes[sr->field->type]);
		printf(", %sv%zu", 
			ftypes[sr->field->type], pos++);
	}
	putchar(')');
}

void
print_func_fill(const struct strct *p, int decl)
{

	printf("void%sdb_%s_fill(struct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)",
	       decl ? " " : "\n",
	       p->name, p->name);
}

/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 *
 * FIXME: don't allow comment-end string.
 * FIXME: wrap at 72 characters.
 */
void
print_comment(const char *doc, size_t tabs, 
	const char *pre, const char *in, const char *post)
{
	const char	*cp;
	size_t		 i;
	char		 last = '\0';

	if (NULL == doc)
		return;

	assert(NULL != in);

	if (NULL != pre) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(pre);
	}

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

	if (NULL != post) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(post);
	}
}
