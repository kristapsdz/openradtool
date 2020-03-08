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

static struct role *
role_alloc(struct parse *p, const char *name, struct role *parent)
{
	struct role	*r;
	char		*cp;

	if ((r = calloc(1, sizeof(struct role))) == NULL) {
		parse_err(p);
		return NULL;
	} else if ((r->name = strdup(name)) == NULL) {
		parse_err(p);
		free(r);
		return NULL;
	}

	/* Add a lowercase version. */

	for (cp = r->name; '\0' != *cp; cp++)
		*cp = tolower((unsigned char)*cp);

	r->parent = parent;
	parse_point(p, &r->pos);
	TAILQ_INIT(&r->subrq);
	if (parent != NULL)
		TAILQ_INSERT_TAIL(&parent->subrq, r, entries);
	return r;
}

/*
 * Return zero if the name already exists, non-zero otherwise.
 * This is a recursive function.
 */
static int
parse_check_rolename(const struct roleq *rq, const char *name) 
{
	const struct role *r;

	TAILQ_FOREACH(r, rq, entries)
		if (strcmp(r->name, name) == 0 ||
		    !parse_check_rolename(&r->subrq, name))
			return 0;

	return 1;
}

/*
 * Parse an individual role, which may be a subset of another role
 * designation, and possibly its documentation.
 * It may not be a reserved role.
 * Its syntax is:
 *
 *  "role" name ["comment" quoted_string]? ["{" [ ROLE ]* "}"]? ";"
 */
static void
parse_role(struct parse *p, struct role *parent)
{
	struct role	*r;

	if (p->lasttype != TOK_IDENT ||
	    strcasecmp(p->last.string, "role")) {
		parse_errx(p, "expected \"role\"");
		return;
	} else if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected role name");
		return;
	}

	if (strcasecmp(p->last.string, "default") == 0 ||
	    strcasecmp(p->last.string, "none") == 0 ||
	    strcasecmp(p->last.string, "all") == 0) {
		parse_errx(p, "reserved role name");
		return;
	} else if (!parse_check_rolename(&p->cfg->rq, p->last.string)) {
		parse_errx(p, "duplicate role name");
		return;
	} else if (!parse_check_badidents(p, p->last.string))
		return;

	if ((r = role_alloc(p, p->last.string, parent)) == NULL)
		return;

	/* Parse optional bits. */

	if (parse_next(p) == TOK_IDENT) {
		if (strcasecmp(p->last.string, "comment")) {
			parse_errx(p, "expected comment");
			return;
		}
		if (!parse_comment(p, &r->doc))
			return;
		parse_next(p);
	}

	if (p->lasttype == TOK_LBRACE) {
		while (!PARSE_STOP(p)) {
			if (parse_next(p) == TOK_RBRACE)
				break;
			parse_role(p, r);
		}
		parse_next(p);
	}

	if (PARSE_STOP(p))
		return;
	if (p->lasttype != TOK_SEMICOLON)
		parse_errx(p, "expected semicolon");
}

/*
 * This means that we're a role-based system.
 * Parse out our role tree.
 * See parse_role() for the ROLE sequence;
 * Its syntax is:
 *
 *  "roles" "{" [ ROLE ]* "}" ";"
 */
void
parse_roles(struct parse *p)
{
	struct role	*r;

	/*
	 * FIXME: if we're doing this again, just start as if we were
	 * passing in under the "all" role again.
	 */

	if (!TAILQ_EMPTY(&p->cfg->rq)) {
		parse_errx(p, "roles already specified");
		return;
	} 

	/*
	 * Start by allocating the reserved roles.
	 * These are "none", "default", and "all".
	 * Make the "all" one the parent of everything.
	 */

	if ((r = role_alloc(p, "none", NULL)) == NULL)
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	if ((r = role_alloc(p, "default", NULL)) == NULL)
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	/* Pass in "all" role as top-level. */

	if ((r = role_alloc(p, "all", NULL)) == NULL)
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	if (parse_next(p) != TOK_LBRACE) {
		parse_errx(p, "expected left brace");
		return;
	}

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_RBRACE)
			break;
		parse_role(p, r);
	}

	if (PARSE_STOP(p))
		return;
	if (parse_next(p) != TOK_SEMICOLON) 
		parse_errx(p, "expected semicolon");
}

