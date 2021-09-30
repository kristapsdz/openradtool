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
 * Parse an enumeration item whose value may be defined or automatically
 * assigned at link time.  Its syntax is:
 *
 *  "item" ident [value]? ["comment" quoted_string]? ";"
 *
 * The identifier has already been parsed: this starts at the value.
 * Both the identifier and the value (if provided) must be unique within
 * the parent enumeration.
 */
static void
parse_enum_item(struct parse *p, struct eitem *ei)
{
	struct eitem	*eei;
	const char	*next;

	if (parse_next(p) == TOK_INTEGER) {
		ei->value = p->last.integer;
		if (ei->value == INT64_MAX ||
		    ei->value == INT64_MIN) {
			parse_errx(p, "enum item value too big or small");
			return;
		}
		TAILQ_FOREACH(eei, &ei->parent->eq, entries) {
			if (ei == eei || (eei->flags & EITEM_AUTO) ||
			    ei->value != eei->value) 
				continue;
			parse_errx(p, "duplicate enum item value");
			return;
		}
		parse_next(p);
	} else
		ei->flags |= EITEM_AUTO;

	while (!PARSE_STOP(p) && p->lasttype == TOK_IDENT) {
		next = p->last.string;
		if (strcasecmp(next, "comment") == 0) {
			if (!parse_comment(p, &ei->doc))
				return;
			parse_next(p);
		} else if (strcasecmp(next, "jslabel") == 0) {
			parse_label(p, &ei->labels);
			parse_next(p);
		} else
			parse_errx(p, "unknown enum item attribute");
	}

	if (!PARSE_STOP(p) && p->lasttype != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
}

/*
 * Parse semicolon-terminated labels of special phrase.
 * Return zero on failure, non-zero on success.
 */
static int
parse_enum_label(struct parse *p, struct labelq *q)
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
 * Read an individual enumeration.
 * This opens and closes the enumeration, then reads all of the enum
 * data within.
 * Its syntax is:
 * 
 *  "{" 
 *    ["item" ident ITEM]+ 
 *    ["comment" quoted_string]?
 *  "};"
 */
static void
parse_enum_data(struct parse *p, struct enm *e)
{
	struct eitem	*ei;
	int64_t	 	 maxvalue = INT64_MIN;
	int		 hasauto = 0;
	char		*cp;

	if (parse_next(p) != TOK_LBRACE) {
		parse_errx(p, "expected left brace");
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_RBRACE)
			break;
		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected enum attribute");
			return;
		}

		if (strcasecmp(p->last.string, "comment") == 0) {
			if (!parse_comment(p, &e->doc))
				return;
			if (parse_next(p) != TOK_SEMICOLON) {
				parse_errx(p, "expected semicolon");
				return;
			}
			continue;
		} else if (strcasecmp(p->last.string, "isnull") == 0) {
			if (!parse_enum_label(p, &e->labels_null))
				return;
			continue;
		} else if (strcasecmp(p->last.string, "item")) {
			parse_errx(p, "unknown enum attribute");
			return;
		}

		/* Now we have a new item: validate and parse it. */

		if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected enum item name");
			return;
		} else if (!parse_check_badidents(p, p->last.string))
			return;

		if (strcasecmp(p->last.string, "format") == 0) {
			parse_errx(p, "cannot use reserved name");
			return;
		}

		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (strcasecmp(ei->name, p->last.string))
				continue;
			parse_errx(p, "duplicate enum item name");
			return;
		}

		if ((ei = calloc(1, sizeof(struct eitem))) == NULL) {
			parse_err(p);
			return;
		}
		TAILQ_INSERT_TAIL(&e->eq, ei, entries);

		TAILQ_INIT(&ei->labels);
		parse_point(p, &ei->pos);
		ei->parent = e;
		if ((ei->name = strdup(p->last.string)) == NULL) {
			parse_err(p);
			return;
		}

		for (cp = ei->name; *cp != '\0'; cp++)
			*cp = (char)tolower((unsigned char)*cp);

		parse_enum_item(p, ei);

		if (hasauto == 0 && (ei->flags & EITEM_AUTO))
			hasauto = 1;
	}

	/*
	 * If we have any values to assign automatically, do so here.
	 * First we determine the greatest current value.
	 * If negative, start from zero.
	 * Then increase monotonically.
	 */

	if (hasauto) {
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!(ei->flags & EITEM_AUTO))
				if (ei->value > maxvalue)
					maxvalue = ei->value;
		maxvalue = maxvalue < 0 ? 0 : maxvalue + 1;
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (maxvalue == INT64_MAX) {
				parse_errx(p, "integer overflow "
					"when assigning dynamic "
					"enum value");
				return;
			} else if ((ei->flags & EITEM_AUTO))
				ei->value = maxvalue++;
	}

	if (PARSE_STOP(p))
		return;

	if (parse_next(p) != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
	else if (TAILQ_EMPTY(&e->eq))
		parse_errx(p, "no items in enum");
}

/*
 * Verify and allocate an enum, then start parsing it.
 */
void
parse_enum(struct parse *p)
{
	struct enm	*e;
	struct eitem	*ei;
	int		 foundauto = 0;
	char		*cp;
	int64_t		 maxv = INT64_MIN;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected enum name");
		return;
	}

	/* Disallow top-level duplicate names and bad names. */

	if (!parse_check_dupetoplevel(p, p->last.string) ||
	    !parse_check_badidents(p, p->last.string))
		return;

	if ((e = calloc(1, sizeof(struct enm))) == NULL) {
		parse_err(p);
		return;
	}

	parse_point(p, &e->pos);
	TAILQ_INSERT_TAIL(&p->cfg->eq, e, entries);
	TAILQ_INIT(&e->eq);
	TAILQ_INIT(&e->labels_null);

	if ((e->name = strdup(p->last.string)) == NULL) {
		parse_err(p);
		return;
	}

	for (cp = e->name; *cp != '\0'; cp++)
		*cp = (char)tolower((unsigned char)*cp);

	parse_enum_data(p, e);

	/* Get the sup(max) bount below at zero. */

	TAILQ_FOREACH(ei, &e->eq, entries)
		if (!(ei->flags & EITEM_AUTO) && ei->value > maxv) {
			maxv = ei->value;
			foundauto = 1;
		}

	/* Auto-assign values, if not given. */

	if (foundauto) {
		maxv = maxv < 0 ? 0 : maxv + 1;
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (ei->flags & EITEM_AUTO)
				ei->value = maxv++;
	}
}

