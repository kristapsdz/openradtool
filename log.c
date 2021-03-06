/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

static	const char *const msgtypes[] = {
	"warning", /* MSGTYPE_WARN */
	"error", /* MSGTYPE_ERROR */
	"fatal" /* MSGTYPE_FATAL */
};

static void
ort_log(struct msgq *mq, enum msgtype type, 
	int er, const struct pos *pos, char *msg)
{
	struct msg	*m;

	/* Shit. */

	if ((m = calloc(1, sizeof(struct msg))) == NULL) {
		free(msg);
		return;
	}

	TAILQ_INSERT_HEAD(mq, m, entries);

	m->type = type;
	m->er = er;
	m->buf = msg;
	if (pos != NULL) {
		if (pos->fname != NULL)
			m->fname = strdup(pos->fname);
		m->line = pos->line;
		m->column = pos->column;
	}
}

/*
 * Generic message formatting (va_list version).
 * Copies all messages into the message array.
 * On memory exhaustion, does nothing.
 */
void
ort_msgv(struct msgq *mq, enum msgtype type, int er, 
	const struct pos *pos, const char *fmt, va_list ap)
{
	char	*buf = NULL;

	if (fmt != NULL && vasprintf(&buf, fmt, ap) == -1)
		return;
	ort_log(mq, type, er, pos, buf);
}

/*
 * Generic message formatting.
 * Copies all messages into the message array.
 * On memory exhaustion, does nothing.
 */
void
ort_msg(struct msgq *mq, enum msgtype type, int er, 
	const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;

	if (fmt == NULL) {
		ort_log(mq, type, er, pos, NULL);
		return;
	}

	va_start(ap, fmt);
	ort_msgv(mq, type, er, pos, fmt, ap);
	va_end(ap);
}

static int
gen_msg(FILE *f, const struct msg *m)
{

	if (m->fname != NULL && m->line > 0) {
		if (fprintf(f, "%s:%zu:%zu: %s: ", m->fname, 
		    m->line, m->column, msgtypes[m->type]) < 0)
			return 0;
	} else if (m->fname != NULL) {
		if (fprintf(f, "%s: %s: ", 
		    m->fname, msgtypes[m->type]) < 0)
			return 0;
	} else 
		if (fprintf(f, "%s: ", msgtypes[m->type]) < 0)
			return 0;

	if (m->buf != NULL && fputs(m->buf, f) == EOF)
		return 0;

	if (m->er != 0 && fprintf(f, "%s%s", 
	    m->buf != NULL ? ": " : "", strerror(m->er)) < 0)
		return 0;

	return fputc('\n', f) != EOF;
}


int
ort_write_msg_file(FILE *f, const struct msgq *q)
{
	const struct msg	 *m;

	if (q == NULL)
		return 1;

	TAILQ_FOREACH(m, q, entries)
		if (!gen_msg(f, m))
			return 0;

	return 1;
}
