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
#include <string.h>

#include "extern.h"

#define	MAXCOLS 70

/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 * FIXME: don't allow comment-end string.
 */
static void
print_comment(const char *doc, size_t tabs, 
	const char *pre, const char *in, const char *post)
{
	const char	*cp, *cpp;
	size_t		 i, curcol, maxcol;
	char		 last = '\0';

	assert(NULL != in);

	if (tabs >= 4)
		maxcol = 40;
	else
		maxcol = MAXCOLS - (tabs * 4);

	if (NULL != pre) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(pre);
	}

	if (NULL != doc) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%s", in);

		curcol = 0;
		for (cp = doc; '\0' != *cp; cp++) {
			/*
			 * Newline check.
			 * If we're at a newline, then emit the leading
			 * in-comment marker.
			 */

			if ('\n' == *cp) {
				putchar('\n');
				for (i = 0; i < tabs; i++)
					putchar('\t');
				printf("%s", in);
				last = *cp;
				curcol = 0;
				continue;
			}

			/* Escaped quotation marks. */

			if ('\\' == *cp && '"' == cp[1])
				cp++;

			/*
			 * If we're starting a word, see whether the
			 * word will extend beyond our line boundaries.
			 * If it does, and if the last character wasn't
			 * a newline, then emit a newline.
			 */

			if ((' ' == last || '\t' == last) && 
			    (' ' != *cp && '\t' != *cp)) {
				for (cpp = cp; '\0' != *cpp; cpp++)
					if (' ' == *cpp || 
					    '\t' == *cpp)
						break;
				if (curcol + (cpp - cp) > maxcol) {
					putchar('\n');
					for (i = 0; i < tabs; i++)
						putchar('\t');
					printf("%s", in);
					curcol = 0;
				}
			}

			putchar(*cp);
			last = *cp;
			curcol++;
		}

		/* Newline-terminate. */

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
	size_t	maxcol, i;

	if (tabs >= 4)
		maxcol = 40;
	else
		maxcol = MAXCOLS - (tabs * 4);

	if (COMMENT_C == type && NULL != cp &&
	    tabs >= 1 && NULL == strchr(cp, '\n') && 
	    strlen(cp) < maxcol) {
		for (i = 0; i < tabs; i++) 
			putchar('\t');
		printf("/* %s */\n", cp);
		return;
	}

	if (COMMENT_JS == type && NULL != cp &&
	    2 == tabs && NULL == strchr(cp, '\n') && 
	    strlen(cp) < maxcol) {
		printf("\t\t/** %s */\n", cp);
		return;
	}

	switch (type) {
	case (COMMENT_C):
		print_comment(cp, tabs, "/*", " * ", " */");
		break;
	case (COMMENT_JS):
		print_comment(cp, tabs, "/**", " * ", " */");
		break;
	case (COMMENT_C_FRAG_CLOSE):
	case (COMMENT_JS_FRAG_CLOSE):
		print_comment(cp, tabs, NULL, " * ", " */");
		break;
	case (COMMENT_C_FRAG_OPEN):
		print_comment(cp, tabs, "/*", " * ", NULL);
		break;
	case (COMMENT_JS_FRAG_OPEN):
		print_comment(cp, tabs, "/**", " * ", NULL);
		break;
	case (COMMENT_C_FRAG):
	case (COMMENT_JS_FRAG):
		print_comment(cp, tabs, NULL, " * ", NULL);
		break;
	default:
		print_comment(cp, tabs, NULL, "-- ", NULL);
		break;
	}
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
