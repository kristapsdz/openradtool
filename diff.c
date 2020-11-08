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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"

static struct diff *
diff_alloc(struct diffq *q, enum difftype type)
{
	struct diff	*d;

	if ((d = calloc(1, sizeof(struct diff))) == NULL)
		return NULL;
	TAILQ_INSERT_TAIL(q, d, entries);
	d->type = type;
	return d;
}

/*
 * Check if two comments are the same (both empty or both with same
 * string contents).
 * Return zero if dissimilar, non-zero if similar.
 */
static int
ort_check_comment(const char *from, const char *into)
{

	if (from != NULL && into != NULL)
		return strcmp(from, into) == 0;

	return from == NULL && into == NULL;
}

/*
 * Return <0 on failure, 0 if the same, >0 if modified.
 */
static int
ort_diff_bitidx(struct diffq *q,
	const struct bitidx *ifrom, const struct bitidx *iinto)
{
	struct diff	*d;
	int		 same = 1;

	assert(iinto != NULL);

	if (ifrom == NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_BITIDX)) == NULL)
			return -1;
		d->bitidx = iinto;
		return 0;
	}

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_BITIDX_COMMENT)) == NULL)
			return -1;
		d->bitidx_pair.from = ifrom;
		d->bitidx_pair.into = iinto;
		same = 0;
	}

	d = diff_alloc(q, same ? DIFF_SAME_BITIDX : DIFF_MOD_BITIDX);
	if (d == NULL)
		return -1;
	d->bitidx_pair.from = ifrom;
	d->bitidx_pair.into = iinto;

	return same ? 0 : 1;
}

/*
 * Return <0 on failure, 0 if the same, >0 if modified.
 */
static int
ort_diff_eitem(struct diffq *q,
	const struct eitem *ifrom, const struct eitem *iinto)
{
	struct diff	*d;
	int		 same = 1;

	assert(iinto != NULL);

	if (ifrom == NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_EITEM)) == NULL)
			return -1;
		d->eitem = iinto;
		return 0;
	}

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (ifrom->value != iinto->value) {
		if ((d = diff_alloc(q, DIFF_MOD_EITEM_VALUE)) == NULL)
			return -1;
		d->eitem_pair.from = ifrom;
		d->eitem_pair.into = iinto;
		same = 0;
	}

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_EITEM_COMMENT)) == NULL)
			return -1;
		d->eitem_pair.from = ifrom;
		d->eitem_pair.into = iinto;
		same = 0;
	}

	d = diff_alloc(q, same ? DIFF_SAME_EITEM : DIFF_MOD_EITEM);
	if (d == NULL)
		return -1;
	d->eitem_pair.from = ifrom;
	d->eitem_pair.into = iinto;

	return same ? 0 : 1;
}

static int
ort_diff_bitf(struct diffq *q,
	const struct bitf *efrom, const struct bitf *einto)
{
	const struct bitidx	*ifrom, *iinto;
	struct diff		*d;
	int			 rc, same = 1;

	/* 
	 * Look at new indices, see if they exist in the old.  If they
	 * do, then check them for sameness.
	 */

	TAILQ_FOREACH(iinto, &einto->bq, entries) {
		TAILQ_FOREACH(ifrom, &efrom->bq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if ((rc = ort_diff_bitidx(q, ifrom, iinto)) < 0)
			return 0;
		if (same)
			same = rc > 0;
	}

	/* Look at old indices no longer in the new. */

	TAILQ_FOREACH(ifrom, &efrom->bq, entries) {
		TAILQ_FOREACH(iinto, &einto->bq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if (iinto == NULL) {
			if ((d = diff_alloc(q, DIFF_DEL_BITIDX)) == NULL)
				return 0;
			d->bitidx = ifrom;
			same = 0;
		}
	}

	/* More checks. */

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		d = diff_alloc(q, DIFF_MOD_BITF_COMMENT);
		if (d == NULL)
			return 0;
		d->bitf_pair.from = efrom;
		d->bitf_pair.into = einto;
		same = 0;
	}

	/* Encompassing check. */

	d = diff_alloc(q, same ? DIFF_SAME_BITF : DIFF_MOD_BITF);
	if (d == NULL)
		return 0;
	d->bitf_pair.from = efrom;
	d->bitf_pair.into = einto;

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_enm(struct diffq *q,
	const struct enm *efrom, const struct enm *einto)
{
	const struct eitem	*ifrom, *iinto;
	struct diff		*d;
	int			 rc, same = 1;

	/* 
	 * Look at new enumerations, see if they exist in the old.  If
	 * they do, then check them for sameness.
	 */

	TAILQ_FOREACH(iinto, &einto->eq, entries) {
		TAILQ_FOREACH(ifrom, &efrom->eq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if ((rc = ort_diff_eitem(q, ifrom, iinto)) < 0)
			return 0;
		if (same)
			same = rc > 0;
	}

	/* Look at old enumerations no longer in the new. */

	TAILQ_FOREACH(ifrom, &efrom->eq, entries) {
		TAILQ_FOREACH(iinto, &einto->eq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if (iinto == NULL) {
			if ((d = diff_alloc(q, DIFF_DEL_EITEM)) == NULL)
				return 0;
			d->eitem = ifrom;
			same = 0;
		}
	}

	/* More checks. */

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		d = diff_alloc(q, DIFF_MOD_ENM_COMMENT);
		if (d == NULL)
			return 0;
		d->enm_pair.from = efrom;
		d->enm_pair.into = einto;
		same = 0;
	}

	/* Encompassing check. */

	d = diff_alloc(q, same ? DIFF_SAME_ENM : DIFF_MOD_ENM);
	if (d == NULL)
		return 0;
	d->enm_pair.from = efrom;
	d->enm_pair.into = einto;

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_bitfs(struct diffq *q, 
	const struct config *from, const struct config *into)
{
	const struct bitf	*efrom, *einto;
	struct diff		*d;

	TAILQ_FOREACH(einto, &into->bq, entries) {
		TAILQ_FOREACH(efrom, &from->bq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;

		if (efrom == NULL) {
			if ((d = diff_alloc(q, DIFF_ADD_BITF)) == NULL)
				return 0;
			d->bitf = einto;
		} else if (!ort_diff_bitf(q, efrom, einto))
			return 0;
	}

	TAILQ_FOREACH(efrom, &from->bq, entries) {
		TAILQ_FOREACH(einto, &into->bq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;
		if (einto == NULL) {
			if ((d = calloc(1, sizeof(struct diff))) == NULL)
				return 0;
			TAILQ_INSERT_TAIL(q, d, entries);
			d->type = DIFF_DEL_BITF;
			d->bitf = efrom;
		}
	}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_enms(struct diffq *q, 
	const struct config *from, const struct config *into)
{
	const struct enm	*efrom, *einto;
	struct diff		*d;

	TAILQ_FOREACH(einto, &into->eq, entries) {
		TAILQ_FOREACH(efrom, &from->eq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;

		if (efrom == NULL) {
			if ((d = diff_alloc(q, DIFF_ADD_ENM)) == NULL)
				return 0;
			d->enm = einto;
		} else if (!ort_diff_enm(q, efrom, einto))
			return 0;
	}

	TAILQ_FOREACH(efrom, &from->eq, entries) {
		TAILQ_FOREACH(einto, &into->eq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;
		if (einto == NULL) {
			if ((d = calloc(1, sizeof(struct diff))) == NULL)
				return 0;
			TAILQ_INSERT_TAIL(q, d, entries);
			d->type = DIFF_DEL_ENM;
			d->enm = efrom;
		}
	}

	return 1;
}

struct diffq *
ort_diff(const struct config *from, const struct config *into)
{
	struct diffq	*q;

	if ((q = calloc(1, sizeof(struct diffq))) == NULL)
		return NULL;

	TAILQ_INIT(q);

	if (!ort_diff_enms(q, from, into))
		goto err;
	if (!ort_diff_bitfs(q, from, into))
		goto err;

	return q;
err:
	ort_diff_free(q);
	return NULL;
}

void
ort_diff_free(struct diffq *q)
{
	struct diff	*d;

	if (q == NULL)
		return;

	while ((d = TAILQ_FIRST(q)) != NULL) {
		TAILQ_REMOVE(q, d, entries);
		free(d);
	}

	free(q);
}
