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

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "parser.h"

/*
 * Parse a bitfield item with syntax:
 *
 *  NUMBER ["comment" quoted_string]? ";"
 */
static void
parse_bitidx_item(struct parse *p, struct bitidx *bi)
{
	struct bitidx	*bbi;

	if (parse_next(p) != TOK_INTEGER) {
		parse_errx(p, "expected item value");
		return;
	}

	bi->value = p->last.integer;
	if (bi->value < 0 || bi->value >= 64) {
		parse_errx(p, "bit index out of range");
		return;
	} else if (bi->value >= 32)
		parse_warnx(p, "bit index will not work with "
			"JavaScript applications (32-bit)");

	TAILQ_FOREACH(bbi, &bi->parent->bq, entries) 
		if (bi != bbi && bi->value == bbi->value) {
			parse_errx(p, "duplicate item value");
			return;
		}

	while (!PARSE_STOP(p) && parse_next(p) == TOK_IDENT)
		if (strcasecmp(p->last.string, "comment") == 0)
			parse_comment(p, &bi->doc);
		else if (strcasecmp(p->last.string, "jslabel") == 0)
			parse_label(p, &bi->labels);
		else
			parse_errx(p, "unknown item data type");

	if (!PARSE_STOP(p) && p->lasttype != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
}

/*
 * Parse semicolon-terminated labels of special phrase.
 * Return zero on failure, non-zero on success.
 */
static int
parse_bitidx_label(struct parse *p, struct labelq *q)
{

	for (;;) {
		if (parse_next(p) == TOK_SEMICOLON)
			return 1;
		if (p->lasttype != TOK_IDENT ||
		    strcasecmp(p->last.string, "jslabel")) {
			parse_errx(p, "expected \"jslabel\"");
			return 0;
		} 
		if (!parse_label(p, q))
			return 0;
	}
}

/*
 * Parse a full bitfield index.
 * Its syntax is:
 *
 *  "{" 
 *    ["item" ident ITEM]+ 
 *    ["comment" quoted_string]?
 *  "};"
 *
 *  The "ITEM" clause is handled by parse_bididx_item.
 */
static void
parse_bitidx(struct parse *p, struct bitf *b)
{
	struct bitidx	*bi;

	if (parse_next(p) != TOK_LBRACE) {
		parse_errx(p, "expected left brace");
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_RBRACE)
			break;
		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected bitfield data type");
			return;
		}

		if (strcasecmp(p->last.string, "comment") == 0) {
			if (!parse_comment(p, &b->doc))
				return;
			if (parse_next(p) != TOK_SEMICOLON) {
				parse_errx(p, "expected end of comment");
				return;
			}
			continue;
		} else if (strcasecmp(p->last.string, "isunset") == 0 ||
		  	   strcasecmp(p->last.string, "unset") == 0) {
			if (strcasecmp(p->last.string, "unset") == 0)
				parse_warnx(p, "\"unset\" is "
					"deprecated: use \"isunset\"");
			if (!parse_bitidx_label(p, &b->labels_unset))
				return;
			continue;
		} else if (strcasecmp(p->last.string, "isnull") == 0) {
			if (!parse_bitidx_label(p, &b->labels_null))
				return;
			continue;
		} else if (strcasecmp(p->last.string, "item")) {
			parse_errx(p, "unknown bitfield data type");
			return;
		}

		/* Now we have a new item: validate and parse it. */

		if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected item name");
			return;
		} else if (!parse_check_badidents(p, p->last.string))
			return;

		TAILQ_FOREACH(bi, &b->bq, entries) {
			if (strcasecmp(bi->name, p->last.string))
				continue;
			parse_errx(p, "duplicate item name");
			return;
		}

		if ((bi = calloc(1, sizeof(struct bitidx))) == NULL) {
			parse_err(p);
			return;
		}
		TAILQ_INIT(&bi->labels);
		parse_point(p, &bi->pos);
		TAILQ_INSERT_TAIL(&b->bq, bi, entries);
		bi->parent = b;
		if ((bi->name = strdup(p->last.string)) == NULL) {
			parse_err(p);
			return;
		}
		parse_bitidx_item(p, bi);
	}

	if (PARSE_STOP(p))
		return;

	if (parse_next(p) != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
	else if (TAILQ_EMPTY(&b->bq))
		parse_errx(p, "no items in bitfield");
}

/*
 * Parse a "bitfield", which is a named set of bit indices.
 * Its syntax is:
 *
 *  "bits" name "{" ... "};"
 */
void
parse_bitfield(struct parse *p)
{
	struct bitf	*b;
	char		*caps;

	/* 
	 * Disallow duplicate and bad names.
	 * Duplicates are for both structures and enumerations.
	 */

	if (!parse_check_dupetoplevel(p, p->last.string) ||
	    !parse_check_badidents(p, p->last.string))
		return;

	if ((b = calloc(1, sizeof(struct bitf))) == NULL) {
		parse_err(p);
		return;
	}

	TAILQ_INIT(&b->labels_unset);
	TAILQ_INIT(&b->labels_null);
	parse_point(p, &b->pos);
	TAILQ_INSERT_TAIL(&p->cfg->bq, b, entries);
	TAILQ_INIT(&b->bq);

	if ((b->name = strdup(p->last.string)) == NULL) {
		parse_err(p);
		return;
	} else if ((b->cname = strdup(b->name)) == NULL) {
		parse_err(p);
		return;
	}

	for (caps = b->cname; *caps != '\0'; caps++)
		*caps = toupper((unsigned char)*caps);

	parse_bitidx(p, b);
}
