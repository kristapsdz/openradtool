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

/*
 * Internal logging function: logs to stderr and (optionally) queues
 * message.
 * This accepts an optionally-NULL "msg" at optionally-NULL "pos" with
 * errno "er" or zero (only recognised on MSGTYPE_FATAL).
 * If "cfg" is not NULL, this enqueues the message into the "msgs" array
 * for that configuration.
 * The "msg" pointer will be freed if "cfg" is NULL, as otherwise its
 * ownership is transferred to the "msgs" array.
 */
static void
ort_log(struct config *cfg, enum msgtype type, 
	int er, const struct pos *pos, char *msg)
{
	struct msg	*m;

	/* Shit. */

	if ((m = calloc(1, sizeof(struct msg))) == NULL) {
		free(msg);
		return;
	}

	TAILQ_INSERT_HEAD(&cfg->mq, m, entries);

	m->type = type;
	m->er = er;
	m->buf = msg;
	if (pos != NULL)
		m->pos = *pos;
}

/*
 * Generic message formatting (va_list version).
 * Puts all messages into the message array (if "cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_msgv(struct config *cfg, enum msgtype type, 
	int er, const struct pos *pos, 
	const char *fmt, va_list ap)
{
	char	*buf = NULL;

	if (fmt != NULL && vasprintf(&buf, fmt, ap) == -1)
		return;
	ort_log(cfg, type, er, pos, buf);
}

/*
 * Generic message formatting.
 * Puts all messages into the message array if ("cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_msg(struct config *cfg, enum msgtype type, 
	int er, const struct pos *pos, 
	const char *fmt, ...)
{
	va_list	 ap;

	if (fmt == NULL) {
		ort_log(cfg, type, er, pos, NULL);
		return;
	}

	va_start(ap, fmt);
	ort_msgv(cfg, type, er, pos, fmt, ap);
	va_end(ap);
}

static int
gen_msg(FILE *f, const struct msg *m)
{

	if (m->pos.fname != NULL && m->pos.line > 0) {
		if (fprintf(f, "%s:%zu:%zu: %s: ", 
		    m->pos.fname, m->pos.line, m->pos.column, 
		    msgtypes[m->type]) < 0)
			return 0;
	} else if (m->pos.fname != NULL) {
		if (fprintf(f, "%s: %s: ", 
		    m->pos.fname, msgtypes[m->type]) < 0)
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

	TAILQ_FOREACH(m, q, entries)
		if (!gen_msg(f, m))
			return 0;

	return 1;
}
