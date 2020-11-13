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
	enum difftype	 type = DIFF_SAME_BITIDX;

	assert(iinto != NULL);

	if (ifrom == NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_BITIDX)) == NULL)
			return -1;
		d->bitidx = iinto;
		return 1;
	}

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (ifrom->value != iinto->value) {
		if ((d = diff_alloc(q, DIFF_MOD_BITIDX_VALUE)) == NULL)
			return -1;
		d->bitidx_pair.from = ifrom;
		d->bitidx_pair.into = iinto;
		type = DIFF_MOD_BITIDX;
	}

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_BITIDX_COMMENT)) == NULL)
			return -1;
		d->bitidx_pair.from = ifrom;
		d->bitidx_pair.into = iinto;
		type = DIFF_MOD_BITIDX;
	}

	if ((d = diff_alloc(q, type)) == NULL)
		return -1;
	d->bitidx_pair.from = ifrom;
	d->bitidx_pair.into = iinto;
	return type == DIFF_SAME_BITIDX ? 0 : 1;
}

/*
 * Return <0 on failure, 0 if the same, >0 if modified.
 */
static int
ort_diff_field(struct diffq *q,
	const struct field *ifrom, const struct field *iinto)
{
	struct diff	*d;
	enum difftype	 type = DIFF_SAME_FIELD;

	assert(iinto != NULL);

	if (ifrom == NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_FIELD)) == NULL)
			return -1;
		d->field = iinto;
		return 1;
	}

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (ifrom->type != iinto->type) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_TYPE)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if (ifrom->actdel != iinto->actdel ||
	    ifrom->actup != iinto->actup) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_ACTIONS)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if (ifrom->flags != iinto->flags) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_FLAGS)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if (ifrom->bitf != NULL && iinto->bitf != NULL &&
	    strcasecmp(ifrom->bitf->name, iinto->bitf->name)) {
		d = diff_alloc(q, DIFF_MOD_FIELD_BITF);
		if (d == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if (ifrom->enm != NULL && iinto->enm != NULL &&
	    strcasecmp(ifrom->enm->name, iinto->enm->name)) {
		d = diff_alloc(q, DIFF_MOD_FIELD_ENM);
		if (d == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if ((ifrom->ref != NULL && iinto->ref == NULL) ||
	    (ifrom->ref == NULL && iinto->ref != NULL) ||
	    (ifrom->ref != NULL && iinto->ref != NULL &&
	     (strcasecmp(ifrom->ref->source->parent->name, 
			 iinto->ref->source->parent->name) != 0 ||
	      strcasecmp(ifrom->ref->source->name, 
		         iinto->ref->source->name) != 0 ||
	      strcasecmp(ifrom->ref->target->parent->name, 
		         iinto->ref->target->parent->name) != 0 ||
	      strcasecmp(ifrom->ref->target->name, 
		         iinto->ref->target->name) != 0))) {
		d = diff_alloc(q, DIFF_MOD_FIELD_REFERENCE);
		if (d == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_COMMENT)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		type = DIFF_MOD_FIELD;
	}

	if ((d = diff_alloc(q, type)) == NULL)
		return -1;
	d->field_pair.from = ifrom;
	d->field_pair.into = iinto;
	return type == DIFF_SAME_FIELD ? 0 : 1;
}

/*
 * Return <0 on failure, 0 if the same, >0 if modified.
 */
static int
ort_diff_eitem(struct diffq *q,
	const struct eitem *ifrom, const struct eitem *iinto)
{
	struct diff	*d;
	enum difftype	 type = DIFF_SAME_EITEM;

	assert(iinto != NULL);

	if (ifrom == NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_EITEM)) == NULL)
			return -1;
		d->eitem = iinto;
		return 1;
	}

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (ifrom->value != iinto->value) {
		if ((d = diff_alloc(q, DIFF_MOD_EITEM_VALUE)) == NULL)
			return -1;
		d->eitem_pair.from = ifrom;
		d->eitem_pair.into = iinto;
		type = DIFF_MOD_EITEM;
	}

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_EITEM_COMMENT)) == NULL)
			return -1;
		d->eitem_pair.from = ifrom;
		d->eitem_pair.into = iinto;
		type = DIFF_MOD_EITEM;
	}

	if ((d = diff_alloc(q, type)) == NULL)
		return -1;
	d->eitem_pair.from = ifrom;
	d->eitem_pair.into = iinto;
	return type == DIFF_SAME_EITEM ? 0 : 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_bitf(struct diffq *q,
	const struct bitf *efrom, const struct bitf *einto)
{
	const struct bitidx	*ifrom, *iinto;
	struct diff		*d;
	enum difftype		 type = DIFF_SAME_BITF;
	int			 rc;

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
		else if (rc > 0)
			type = DIFF_MOD_BITF;
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
			type = DIFF_MOD_BITF;
		}
	}

	/* More checks. */

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		d = diff_alloc(q, DIFF_MOD_BITF_COMMENT);
		if (d == NULL)
			return 0;
		d->bitf_pair.from = efrom;
		d->bitf_pair.into = einto;
		type = DIFF_MOD_BITF;
	}

	/* Encompassing check. */

	if ((d = diff_alloc(q, type)) == NULL)
		return 0;
	d->bitf_pair.from = efrom;
	d->bitf_pair.into = einto;
	return 1;
}

/*
 * See if "os" contains the unique entry "u".
 * Return zero on failure, non-zero on success.
 */
static int
ort_has_unique(struct diffq *q,
	const struct unique *u, const struct strct *os)
{
	const struct unique	*ou;
	const struct nref	*nf, *onf;
	size_t			 sz = 0, osz;

	TAILQ_FOREACH(nf, &u->nq, entries)
		sz++;

	TAILQ_FOREACH(ou, &os->nq, entries) {
		TAILQ_FOREACH(nf, &u->nq, entries) {
			osz = 0;
			TAILQ_FOREACH(onf, &ou->nq, entries)
				osz++;
			if (osz != sz)
				break;
			TAILQ_FOREACH(onf, &ou->nq, entries)
				if (strcasecmp(onf->field->name,
				    nf->field->name) == 0)
					break;
			if (onf == NULL)
				break;
		}
		if (nf == NULL)
			return 1;
	}

	return 0;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_strct(struct diffq *q,
	const struct strct *efrom, const struct strct *einto)
{
	const struct field	*ifrom, *iinto;
	const struct unique	*u;
	struct diff		*d;
	int			 rc;
	enum difftype		 type = DIFF_SAME_STRCT;

	TAILQ_FOREACH(iinto, &einto->fq, entries) {
		TAILQ_FOREACH(ifrom, &efrom->fq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if ((rc = ort_diff_field(q, ifrom, iinto)) < 0)
			return 0;
		else if (rc > 0)
			type = DIFF_MOD_STRCT;
	}

	TAILQ_FOREACH(ifrom, &efrom->fq, entries) {
		TAILQ_FOREACH(iinto, &einto->fq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if (iinto == NULL) {
			if ((d = diff_alloc(q, DIFF_DEL_FIELD)) == NULL)
				return 0;
			d->field = ifrom;
			type = DIFF_MOD_STRCT;
		}
	}

	TAILQ_FOREACH(u, &einto->nq, entries) {
		if (ort_has_unique(q, u, efrom))
			continue;
		if ((d = diff_alloc(q, DIFF_ADD_UNIQUE)) == NULL)
			return 0;
		d->unique = u;
		type = DIFF_MOD_STRCT;
	}

	TAILQ_FOREACH(u, &efrom->nq, entries) {
		if (ort_has_unique(q, u, einto))
			continue;
		if ((d = diff_alloc(q, DIFF_DEL_UNIQUE)) == NULL)
			return 0;
		d->unique = u;
		type = DIFF_MOD_STRCT;
	}

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		d = diff_alloc(q, DIFF_MOD_STRCT_COMMENT);
		if (d == NULL)
			return 0;
		d->strct_pair.from = efrom;
		d->strct_pair.into = einto;
		type = DIFF_MOD_STRCT;
	}

	if ((d = diff_alloc(q, type)) == NULL)
		return 0;
	d->strct_pair.from = efrom;
	d->strct_pair.into = einto;
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
	enum difftype		 type = DIFF_SAME_ENM;
	int			 rc;

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
		else if (rc > 0)
			type = DIFF_MOD_ENM;
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
			type = DIFF_MOD_ENM;
		}
	}

	/* More checks. */

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_ENM_COMMENT)) == NULL)
			return 0;
		d->enm_pair.from = efrom;
		d->enm_pair.into = einto;
		type = DIFF_MOD_ENM;
	}

	/* Encompassing check. */

	if ((d = diff_alloc(q, type)) == NULL)
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
			if ((d = diff_alloc(q, DIFF_DEL_BITF)) == NULL)
				return 0;
			d->bitf = efrom;
		}
	}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_strcts(struct diffq *q, 
	const struct config *from, const struct config *into)
{
	const struct strct	*efrom, *einto;
	struct diff		*d;

	TAILQ_FOREACH(einto, &into->sq, entries) {
		TAILQ_FOREACH(efrom, &from->sq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;

		if (efrom == NULL) {
			if ((d = diff_alloc(q, DIFF_ADD_STRCT)) == NULL)
				return 0;
			d->strct = einto;
		} else if (!ort_diff_strct(q, efrom, einto))
			return 0; 
	}

	TAILQ_FOREACH(efrom, &from->sq, entries) {
		TAILQ_FOREACH(einto, &into->sq, entries)
			if (strcasecmp(efrom->name, einto->name) == 0)
				break;
		if (einto == NULL) {
			if ((d = diff_alloc(q, DIFF_DEL_STRCT)) == NULL)
				return 0;
			d->strct = efrom;
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
			if ((d = diff_alloc(q, DIFF_DEL_ENM)) == NULL)
				return 0;
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
	if (!ort_diff_strcts(q, from, into))
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
