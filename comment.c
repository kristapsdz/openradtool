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

/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 *
 * FIXME: don't allow comment-end string.
 * FIXME: wrap at 72 characters.
 */
void
gen_comment(const char *doc, size_t tabs, 
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
