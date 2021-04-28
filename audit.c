/*	$Id$ */
/*
 * Copyright (c) 2021 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"

static int
rolemap_has(const struct rolemap *rm, const struct role *r)
{
	const struct rref	*rr;
	const struct role	*rc;

	if (rm == NULL)
		return 0;

	TAILQ_FOREACH(rr, &rm->rq, entries) 
		for (rc = r; rc != NULL; rc = rc->parent)
			if (rc == rr->role)
				return 1;
	return 0;
}

static int
follow(const struct strct *st, const struct role *r, struct auditq *aq,
	const struct search *sr, int exported, const char *path)
{
	struct audit		*a;
	void			*pp;
	int			 nexp, c;
	size_t			 i;
	const struct field	*fd;
	char			*newpath;

	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_REACHABLE && a->ar.st == st)
			break;

	if (a == NULL) {
		if ((a = calloc(1, sizeof(struct audit))) == NULL)
			return 0;
		TAILQ_INSERT_TAIL(aq, a, entries);
		a->type = AUDIT_REACHABLE;
		a->ar.st = st;
		a->ar.exported = exported;
		TAILQ_FOREACH(fd, &st->fq, entries)
			a->ar.fdsz++;
		if (a->ar.fdsz > 0 && (a->ar.fds = calloc
		    (a->ar.fdsz, sizeof(struct auditfield))) == NULL)
		    	return 0;
		i = 0;
		TAILQ_FOREACH(fd, &st->fq, entries) {
			a->ar.fds[i].fd = fd;
			a->ar.fds[i].exported = 
				!(fd->flags & FIELD_NOEXPORT) &&
				!rolemap_has(fd->rolemap, r);
			i++;
		}
		assert(i == a->ar.fdsz);
	}

	pp = recallocarray(a->ar.srs, a->ar.srsz,
		a->ar.srsz + 1, sizeof(struct auditpaths));
	if (pp == NULL)
		return 0;
	a->ar.srs = pp;
	a->ar.srs[a->ar.srsz].exported = exported;
	a->ar.srs[a->ar.srsz].sr = sr;
	if (path != NULL &&
	    (a->ar.srs[a->ar.srsz].path = strdup(path)) == NULL)
		return 0;
	a->ar.srsz++;

	/* 
	 * This is the global export marker.  It's possible to have a
	 * "distinct" keyword start a structure as exported that is
	 * otherwise not exported if following through the chain of
	 * structures, so we override any prior non-exported values
	 * here.
	 */

	if (a->ar.exported == 0 && exported == 1)
		a->ar.exported = 1;

	TAILQ_FOREACH(fd, &st->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		assert(fd->ref != NULL);

		c = asprintf(&newpath, "%s%s%s",
			path != NULL ? path : "",
			path != NULL ? "." : "",
			fd->name);
		if (c < 0)
			return 0;

		/*
		 * Mark as not exported if it appears in the rolemap,
		 * inherits from a non-export, or is marked not
		 * exportable on the field.
		 */

		nexp = 1;
		if (exported == 0 || (fd->flags & FIELD_NOEXPORT) || 
		    rolemap_has(fd->rolemap, r))
			nexp = 0;
		c = follow(fd->ref->target->parent, r, aq, sr, nexp, 
			newpath);
		free(newpath);
		if (!c)
			return 0;
	}

	return 1;
}

void
ort_auditq_free(struct auditq *aq)
{
	struct audit	*a;
	size_t		 i;

	if (aq == NULL)
		return;
	while ((a = TAILQ_FIRST(aq)) != NULL) {
		TAILQ_REMOVE(aq, a, entries);
		if (a->type == AUDIT_REACHABLE) {
			for (i = 0; i < a->ar.srsz; i++)
				free(a->ar.srs[i].path);
			free(a->ar.srs);
			free(a->ar.fds);
		}
		free(a);
	}

	free(aq);
}

struct auditq *
ort_audit(const struct role *r, const struct config *cfg)
{
	const struct strct	*st, *sst;
	const struct search	*sr;
	const struct update	*up;
	const struct rolemap	*rm;
	struct audit		*a;
	struct auditq		*aq = NULL;
	int			 exported;

	if ((aq = calloc(1, sizeof(struct auditq))) == NULL)
		goto err;

	TAILQ_INIT(aq);

	TAILQ_FOREACH(st, &cfg->sq, entries) {
		exported = 1;
		TAILQ_FOREACH(rm, &st->rq, entries) 
			if (rm->type == ROLEMAP_NOEXPORT &&
			    rm->f == NULL &&
			    rolemap_has(rm, r)) {
				exported = 0;
				break;
			}

		if (st->ins != NULL &&
		    rolemap_has(st->ins->rolemap, r)) {
			a = calloc(1, sizeof(struct audit));
			if (a == NULL)
				goto err;
			TAILQ_INSERT_TAIL(aq, a, entries);
			a->type = AUDIT_INSERT;
			a->st = st;
		}
				 
		TAILQ_FOREACH(up, &st->uq, entries)
			if (rolemap_has(up->rolemap, r)) {
				a = calloc(1, sizeof(struct audit));
				if (a == NULL)
					goto err;
				TAILQ_INSERT_TAIL(aq, a, entries);
				a->type = AUDIT_UPDATE;
				a->up = up;
			}
		TAILQ_FOREACH(up, &st->dq, entries)
			if (rolemap_has(up->rolemap, r)) {
				a = calloc(1, sizeof(struct audit));
				if (a == NULL)
					goto err;
				TAILQ_INSERT_TAIL(aq, a, entries);
				a->type = AUDIT_UPDATE;
				a->up = up;
			}
		TAILQ_FOREACH(sr, &st->sq, entries)
			if (rolemap_has(sr->rolemap, r)) {
				a = calloc(1, sizeof(struct audit));
				if (a == NULL)
					goto err;
				TAILQ_INSERT_TAIL(aq, a, entries);
				a->type = AUDIT_QUERY;
				a->sr = sr;
				sst = sr->dst != NULL ? 
					sr->dst->strct : st;
				if (!follow(sst, r, aq, sr, exported, NULL))
					goto err;
			}
	}

	return aq;
err:
	ort_auditq_free(aq);
	return NULL;
}

