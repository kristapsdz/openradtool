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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"

/*
 * Print the line of source code given by "fmt" and varargs.
 * This is indented according to "indent", which changes depending on
 * whether there are curly braces around a newline.
 * This is useful when printing the same text with different
 * indentations (e.g., when being surrounded by a conditional).
 */
void
print_src(size_t indent, const char *fmt, ...)
{
	va_list	 ap;
	char	*cp;
	int	 ret;
	size_t	 i, pos;

	va_start(ap, fmt);
	ret = vasprintf(&cp, fmt, ap);
	va_end(ap);
	if (-1 == ret)
		return;

	for (pos = 0; '\0' != cp[pos]; pos++) {
		if (0 == pos || '\n' == cp[pos]) {
			if (pos && '{' == cp[pos - 1])
				indent++;
			if ('}' == cp[pos + 1])
				indent--;
			if (pos)
				putchar('\n');
			for (i = 0; i < indent; i++)
				putchar('\t');
		} 

		if ('\n' != cp[pos])
			putchar(cp[pos]);
	} 

	putchar('\n');
	free(cp);
}
