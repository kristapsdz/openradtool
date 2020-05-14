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
 * The "chan" value may not be NULL.
 * If "cfg" is not NULL, this enqueues the message into the "msgs" array
 * for that configuration.
 * The "msg" pointer will be freed if "cfg" is NULL, as otherwise its
 * ownership is transferred to the "msgs" array.
 */
static void
ort_log(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, char *msg)
{
	void		*pp;
	struct msg	*m;

	if (cfg != NULL) {
		pp = reallocarray(cfg->msgs, 
			cfg->msgsz + 1, sizeof(struct msg));
		/* Well, shit. */
		if (NULL == pp) {
			free(msg);
			return;
		}
		cfg->msgs = pp;
		m = &cfg->msgs[cfg->msgsz++];
		memset(m, 0, sizeof(struct msg));
		m->type = type;
		m->er = er;
		m->buf = msg;
		if (pos != NULL)
			m->pos = *pos;
	}

	/* Now we also print the message to stderr. */

	if (pos != NULL && pos->fname != NULL && pos->line > 0)
		fprintf(stderr, "%s:%zu:%zu: %s %s: ", 
			pos->fname, pos->line, pos->column, 
			chan, msgtypes[type]);
	else if (pos != NULL && pos->fname != NULL)
		fprintf(stderr, "%s: %s %s: ", 
			pos->fname, chan, msgtypes[type]);
	else 
		fprintf(stderr, "%s %s: ", chan, msgtypes[type]);

	if (msg != NULL)
		fputs(msg, stderr);

	if (type == MSGTYPE_FATAL)
		fprintf(stderr, "%s%s", 
			msg != NULL ? ": " : "", strerror(er)); 

	fputc('\n', stderr);

	if (cfg == NULL)
		free(msg);
}

/*
 * Generic message formatting (va_list version).
 * Puts all messages into the message array (if "cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_msgv(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, 
	const char *fmt, va_list ap)
{
	char	*buf = NULL;

	if (fmt != NULL && vasprintf(&buf, fmt, ap) == -1)
		return;
	ort_log(cfg, type, chan, er, pos, buf);
}

/*
 * Generic message formatting.
 * Puts all messages into the message array if ("cfg" isn't NULL) and
 * prints them to stderr as well.
 * On memory exhaustion, does nothing.
 */
void
ort_msg(struct config *cfg, enum msgtype type, 
	const char *chan, int er, const struct pos *pos, 
	const char *fmt, ...)
{
	va_list	 ap;

	if (fmt == NULL) {
		ort_log(cfg, type, chan, er, pos, NULL);
		return;
	}

	va_start(ap, fmt);
	ort_msgv(cfg, type, chan, er, pos, fmt, ap);
	va_end(ap);
}
