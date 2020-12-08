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
 * See if the rolemap exists and has the same top-level roles.
 * Return zero if dissimilar, non-zero if similar.
 */
static int
ort_check_rolemap_roles(const struct rolemap *from,
	const struct rolemap *into)
{
	const struct rref	*rinto, *rfrom;
	size_t			 fromsz = 0, intosz = 0;

	if (from == NULL && into == NULL)
		return 1;

	if ((from != NULL && into == NULL) ||
	    (from == NULL && into != NULL))
		return 0;

	TAILQ_FOREACH(rfrom, &from->rq, entries)
		fromsz++;
	TAILQ_FOREACH(rinto, &into->rq, entries)
		intosz++;

	if (fromsz != intosz)
		return 0;

	TAILQ_FOREACH(rfrom, &from->rq, entries) {
		TAILQ_FOREACH(rinto, &into->rq, entries)
			if (strcasecmp
			    (rinto->role->name, rfrom->role->name) == 0)
				break;
		if (rinto == NULL)
			return 0;
	}

	return 1;
}

/*
 * Make sure that the queue of labels contains the same data.
 * Return zero if dissimilar, non-zero if similar.
 */
static int
ort_check_labels(const struct labelq *from, const struct labelq *into)
{
	size_t	 		 fromsz = 0, intosz = 0;
	const struct label	*linto, *lfrom;

	assert(from != NULL);
	assert(into != NULL);

	TAILQ_FOREACH(lfrom, from, entries)
		fromsz++;
	TAILQ_FOREACH(linto, into, entries)
		intosz++;

	if (fromsz != intosz)
		return 0;

	TAILQ_FOREACH(lfrom, from, entries) {
		TAILQ_FOREACH(linto, into, entries) {
			if (linto->lang != lfrom->lang)
				continue;
			if (strcmp(linto->label, lfrom->label))
				continue;
			break;
		}
		if (linto == NULL)
			return 0;
	}

	return 1;
}

/*
 * See if two urefs are the same.
 * No need to check field->parent, as we know we're in the same struct.
 * Return zero on dissimilar, non-zero on similar.
 */
static int
ort_check_urefs(const struct uref *from, const struct uref *into)
{

	if (strcasecmp(from->field->name, into->field->name) ||
	    from->op != into->op ||
	    from->mod != into->mod)
		return 0;

	return 1;
}

/*
 * Make sure that validations queues contain the same constraints.
 * If the fields have changed type, then the queues are considered
 * different if non-empty.
 * Return zero if dissimilar, non-zero if similar.
 */
static int
ort_check_fvalids(const struct field *fdfrom, const struct field *fdinto)
{
	const struct fvalid	*finto, *ffrom;
	const struct fvalidq	*from, *into;
	size_t			 intosz = 0, fromsz = 0;

	assert(fdfrom != NULL);
	from = &fdfrom->fvq;
	assert(fdinto != NULL);
	into = &fdinto->fvq;

	TAILQ_FOREACH(finto, into, entries)
		intosz++;
	TAILQ_FOREACH(ffrom, from, entries)
		fromsz++;

	if (intosz != fromsz)
		return 0;
	if (fdfrom->type != fdinto->type && intosz > 0)
		return 0;

	TAILQ_FOREACH(ffrom, from, entries) {
		TAILQ_FOREACH(finto, into, entries) {
			if (ffrom->type != finto->type)
				continue;
			switch (fdfrom->type) {
			case FTYPE_BIT:
			case FTYPE_BITFIELD:
			case FTYPE_DATE:
			case FTYPE_EPOCH:
			case FTYPE_INT:
				if (ffrom->d.value.integer != 
				    finto->d.value.integer)
					continue;
				break;
			case FTYPE_REAL:
				if (ffrom->d.value.decimal != 
				    finto->d.value.decimal)
					continue;
				break;
			case FTYPE_BLOB:
			case FTYPE_EMAIL:
			case FTYPE_TEXT:
			case FTYPE_PASSWORD:
				if (ffrom->d.value.len != 
				    finto->d.value.len)
					continue;
				break;
			default:
				abort();
			}
			break;
		}
		if (finto == NULL)
			return 0;
	}

	return 1;
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

	if (!ort_check_labels(&ifrom->labels, &iinto->labels)) {
		if ((d = diff_alloc(q, DIFF_MOD_BITIDX_LABELS)) == NULL)
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
 * Emits DIFF_MOD_FIELD with depending DIFF_MOD_FIELD_xxxx or
 * DIFF_SAME_FIELD.
 * Return <0 on failure, 0 if the same, >0 if modified.
 */
static int
ort_diff_field(struct diffq *q,
	const struct field *ifrom, const struct field *iinto)
{
	struct diff	*d;
	int		 diff, rc = 1;

	assert(iinto != NULL);
	assert(ifrom != NULL);

	assert(strcasecmp(ifrom->name, iinto->name) == 0);

	if (!ort_check_rolemap_roles(ifrom->rolemap, iinto->rolemap)) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_ROLEMAP)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (ifrom->type != iinto->type) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_TYPE)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (ifrom->actdel != iinto->actdel ||
	    ifrom->actup != iinto->actup) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_ACTIONS)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (ifrom->flags != iinto->flags) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_FLAGS)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (ifrom->bitf != NULL && iinto->bitf != NULL &&
	    strcasecmp(ifrom->bitf->name, iinto->bitf->name)) {
		d = diff_alloc(q, DIFF_MOD_FIELD_BITF);
		if (d == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (ifrom->enm != NULL && iinto->enm != NULL &&
	    strcasecmp(ifrom->enm->name, iinto->enm->name)) {
		d = diff_alloc(q, DIFF_MOD_FIELD_ENM);
		if (d == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	/* Only raise this if we already have defaults in both. */

	if ((ifrom->flags & FIELD_HASDEF) &&
	    (iinto->flags & FIELD_HASDEF) &&
	    ifrom->type == iinto->type) {
		diff = 1;
		switch (ifrom->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			diff = ifrom->def.integer != 
				iinto->def.integer;
			break;
		case FTYPE_REAL:
			diff = ifrom->def.decimal != 
				iinto->def.decimal;
			break;
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
			diff = strcmp(ifrom->def.string, 
				iinto->def.string) != 0;
			break;
		case FTYPE_ENUM:
			diff = strcasecmp
				(ifrom->def.eitem->parent->name,
				 iinto->def.eitem->parent->name) != 0 ||
				strcasecmp
				(ifrom->def.eitem->name,
				 iinto->def.eitem->name) != 0;
			break;
		default:
			abort();
		}
		if (diff) {
			d = diff_alloc(q, DIFF_MOD_FIELD_DEF);
			if (d == NULL)
				return -1;
			d->field_pair.from = ifrom;
			d->field_pair.into = iinto;
			rc = 0;
		}
	}

	/*
	 * Foreign or local references changing can be either that we
	 * have or lost a referenence, or either the source or target of
	 * the reference (by name) have changed.
	 */

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
		rc = 0;
	}

	if (!ort_check_fvalids(ifrom, iinto)) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_VALIDS)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	if (!ort_check_comment(ifrom->doc, iinto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_FIELD_COMMENT)) == NULL)
			return -1;
		d->field_pair.from = ifrom;
		d->field_pair.into = iinto;
		rc = 0;
	}

	d = diff_alloc(q, rc ? DIFF_SAME_FIELD : DIFF_MOD_FIELD);
	if (d == NULL)
		return -1;
	d->field_pair.from = ifrom;
	d->field_pair.into = iinto;
	return rc;
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

	if (!ort_check_labels(&ifrom->labels, &iinto->labels)) {
		if ((d = diff_alloc(q, DIFF_MOD_EITEM_LABELS)) == NULL)
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

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		d = diff_alloc(q, DIFF_MOD_BITF_COMMENT);
		if (d == NULL)
			return 0;
		d->bitf_pair.from = efrom;
		d->bitf_pair.into = einto;
		type = DIFF_MOD_BITF;
	}

	if (!ort_check_labels
	     (&efrom->labels_unset, &einto->labels_unset) ||
	    !ort_check_labels
	     (&efrom->labels_null, &einto->labels_null)) {
		d = diff_alloc(q, DIFF_MOD_BITF_LABELS);
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
 * Order-preserving check for updateq.  Emits DIFF_MOD_UPDATE_PARAMS if
 * "q" is not NULL.
 * Return <0 on failure, 0 on dissimilar, >0 on similar.
 */
static int
ort_diff_urefq(struct diffq *q,
	const struct update *from, const struct update *into,
	const struct urefq *fromq, const struct urefq *intoq)
{
	const struct uref	*fref, *iref;
	struct diff		*d;
	int			 rc = 1;

	iref = TAILQ_FIRST(intoq);
	TAILQ_FOREACH(fref, fromq, entries) {
		if (iref == NULL || !ort_check_urefs(iref, fref)) {
			rc = 0;
			break;
		}
		iref = TAILQ_NEXT(iref, entries);
	}

	if (rc == 1 && (iref != NULL || iref != fref))
		rc = 0;

	if (rc == 0 && q != NULL) {
		d = diff_alloc(q, DIFF_MOD_UPDATE_PARAMS);
		if (d == NULL)
			return -1;
		d->update_pair.from = from;
		d->update_pair.into = into;
	}

	return rc;
}

/*
 * If "q" is non-NULL, emit modification diffs; otherwise, this just
 * checks whether the update clauses are the same.
 * Return <0 on failure, 0 on dissimilar, >0 on similar.
 */
static int
ort_diff_update(struct diffq *q, 
	const struct update *from, const struct update *into)
{
	struct diff		*d;
	int			 rc = 1, c;

	/* Both mrq and crq must be order-preserving and similar. */

	c = ort_diff_urefq(q, from, into, &from->mrq, &into->mrq);
	if (c < 0)
		return -1;
	else if (c == 0)
		rc = 0;

	c = ort_diff_urefq(q, from, into, &from->crq, &into->crq);
	if (c < 0)
		return -1;
	else if (c == 0)
		rc = 0;

	/* Comments. */

	if (!ort_check_comment(from->doc, into->doc)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_UPDATE_COMMENT);
			if (d == NULL)
				return -1;
			d->update_pair.from = from;
			d->update_pair.into = into;
		}
		rc = 0;
	}

	/* Rolemap. */

	if (!ort_check_rolemap_roles(from->rolemap, into->rolemap)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_UPDATE_ROLEMAP);
			if (d == NULL)
				return -1;
			d->update_pair.from = from;
			d->update_pair.into = into;
		}
		rc = 0;
	}

	/* Flags. */

	if (from->flags != into->flags) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_UPDATE_FLAGS);
			if (d == NULL)
				return -1;
			d->update_pair.from = from;
			d->update_pair.into = into;
		}
		rc = 0;
	}

	return rc;
}

/*
 * Return >0 on failure, 0 if dissimilar, >0 if similar.
 */
static int
ort_diff_strct_updateq(struct diffq *q,
	const struct updateq *fromq, const struct updateq *intoq)
{
	const struct update	*fdel, *idel;
	struct diff		*d;
	int			 rc = 1, c;

	/* 
	 * Start by scanning for named matches.
	 * Named can also modify.
	 */

	TAILQ_FOREACH(fdel, fromq, entries) {
		if (fdel->name == NULL)
			continue;
		TAILQ_FOREACH(idel, intoq, entries)
			if (idel->name != NULL &&
			    strcasecmp(fdel->name, idel->name) == 0)
				break;
		if (idel == NULL) {
			d = diff_alloc(q, DIFF_DEL_UPDATE);
			if (d == NULL)
				return -1;
			d->update = fdel;
			rc = 0;
			continue;
		}
		if ((c = ort_diff_update(q, fdel, idel)) < 0)
			return -1;
		if (c == 0) {
			d = diff_alloc(q, DIFF_MOD_UPDATE);
			if (d == NULL)
				return -1;
			d->update_pair.from = fdel;
			d->update_pair.into = idel;
			rc = 0;
		} else {
			d = diff_alloc(q, DIFF_SAME_UPDATE);
			if (d == NULL)
				return -1;
			d->update_pair.from = fdel;
			d->update_pair.into = idel;
		}
	}

	TAILQ_FOREACH(idel, intoq, entries) {
		if (idel->name == NULL)
			continue;
		TAILQ_FOREACH(fdel, fromq, entries)
			if (fdel->name != NULL &&
			    strcasecmp(fdel->name, idel->name) == 0)
				break;
		if (fdel == NULL) {
			d = diff_alloc(q, DIFF_ADD_UPDATE);
			if (d == NULL)
				return -1;
			d->update = idel;
			rc = 0;
		}
	}

	/*
	 * Next, check for unnamed matches.
	 * These can't be modified---only added, deleted, or the same.
	 */

	TAILQ_FOREACH(fdel, fromq, entries) {
		if (fdel->name != NULL)
			continue;
		TAILQ_FOREACH(idel, intoq, entries)
			if (idel->name == NULL &&
			    ort_diff_update(NULL, fdel, idel) > 0)
				break;

		if (idel == NULL) {
			d = diff_alloc(q, DIFF_DEL_UPDATE);
			if (d == NULL)
				return -1;
			d->update = fdel;
			rc = 0;
		} else {
			d = diff_alloc(q, DIFF_SAME_UPDATE);
			if (d == NULL)
				return -1;
			d->update_pair.from = fdel;
			d->update_pair.into = idel;
		}
	}

	TAILQ_FOREACH(idel, intoq, entries) {
		if (idel->name != NULL)
			continue;
		TAILQ_FOREACH(fdel, fromq, entries)
			if (fdel->name == NULL &&
			    ort_diff_update(NULL, fdel, idel) > 0)
				break;
		if (fdel == NULL) {
			d = diff_alloc(q, DIFF_ADD_UPDATE);
			if (d == NULL)
				return -1;
			d->update = idel;
			rc = 0;
		}
	}

	return rc;
}

/*
 * See if the sentq are the same (must be order-preserving).
 * Return zero if not the same, non-zero if the same.
 */
static int
ort_check_sentq(const struct sentq *from, const struct sentq *into)
{
	const struct sent	*sfrom, *sinto;

	sinto = TAILQ_FIRST(into);
	TAILQ_FOREACH(sfrom, from, entries) {
		if (sinto == NULL || 
		    sfrom->op != sinto->op ||
		    strcmp(sfrom->fname, sinto->fname))
			return 0;
		sinto = TAILQ_NEXT(sinto, entries);
	}

	return sinto == NULL && sinto == sfrom;
}

/*
 * Check ordering (must be order-preserving).
 * Return zero if not the same, non-zero if the same.
 */
static int
ort_check_ordq(const struct ordq *from, const struct ordq *into)
{
	const struct ord	*ofrom, *ointo;

	ointo = TAILQ_FIRST(into);
	TAILQ_FOREACH(ofrom, from, entries) {
		if (ointo == NULL || 
		    ofrom->op != ointo->op ||
		    strcmp(ofrom->fname, ointo->fname))
			return 0;
		ointo = TAILQ_NEXT(ointo, entries);
	}

	return ointo == NULL && ointo == ofrom;
}

/*
 * Emit DIFF_MOD_SEARCH_xxxx if "q" is not NULL.
 * Return <0 on failure, 0 if dissimilar, >0 if similar.
 */
static int
ort_diff_search(struct diffq *q,
	const struct search *from, const struct search *into)
{
	int		 rc = 1;
	struct diff	*d;

	assert(from->type == into->type);

	if (!ort_check_sentq(&from->sntq, &into->sntq)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_PARAMS);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if (!ort_check_ordq(&from->ordq, &into->ordq)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_ORDER);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if ((from->aggr != NULL && into->aggr == NULL) ||
	    (from->aggr == NULL && into->aggr != NULL) ||
	    (from->aggr != NULL && into->aggr != NULL &&
	     ((from->aggr->op != into->aggr->op) ||
	      strcmp(from->aggr->fname, into->aggr->fname)))) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_AGGR);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if ((from->group != NULL && into->group == NULL) ||
	    (from->group == NULL && into->group != NULL) ||
	    (from->group != NULL && into->group != NULL &&
	     strcmp(from->group->fname, into->group->fname))) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_GROUP);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if ((from->dst != NULL && into->dst == NULL) ||
	    (from->dst == NULL && into->dst != NULL) ||
	    (from->dst != NULL && into->dst != NULL &&
	     strcmp(from->dst->fname, into->dst->fname))) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_DISTINCT);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if (!ort_check_comment(from->doc, into->doc)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_COMMENT);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if (from->limit != into->limit) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_LIMIT);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if (from->offset != into->offset) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_OFFSET);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	if (!ort_check_rolemap_roles(from->rolemap, into->rolemap)) {
		if (q != NULL) {
			d = diff_alloc(q, DIFF_MOD_SEARCH_ROLEMAP);
			if (d == NULL)
				return -1;
			d->search_pair.from = from;
			d->search_pair.into = into;
		}
		rc = 0;
	}

	return rc;
}

/*
 * Emit DIFF_ADD_SEARCH or DIFF_DEL_SEARCH, using ort_diff_search() for
 * same or mod testing.
 * Returns <0 on failure, 0 on dissimilar, >0 on similar.
 */
static int
ort_diff_searchq(struct diffq *q,
	const struct strct *from, const struct strct *into)
{
	const struct search	*sfrom, *sinto;
	struct diff		*d;
	int			 rc = 1, c;

	TAILQ_FOREACH(sfrom, &from->sq, entries) {
		if (sfrom->name == NULL)
			continue;
		TAILQ_FOREACH(sinto, &into->sq, entries)
			if (sinto->name != NULL &&
			    sinto->type == sfrom->type &&
			    strcasecmp(sinto->name, sfrom->name) == 0)
				break;
		if (sinto == NULL) {
			d = diff_alloc(q, DIFF_DEL_SEARCH);
			if (d == NULL)
				return -1;
			d->search = sfrom;
			rc = 0;
			continue;
		}
		if ((c = ort_diff_search(q, sfrom, sinto)) < 0)
			return -1;
		if (c == 0) {
			rc = 0;
			d = diff_alloc(q, DIFF_MOD_SEARCH);
		} else
			d = diff_alloc(q, DIFF_SAME_SEARCH);
		if (d == NULL)
			return -1;
		d->search_pair.from = sfrom;
		d->search_pair.into = sinto;
	}

	TAILQ_FOREACH(sinto, &into->sq, entries) {
		if (sinto->name == NULL)
			continue;
		TAILQ_FOREACH(sfrom, &from->sq, entries)
			if (sfrom->name != NULL &&
			    sinto->type == sfrom->type &&
			    strcasecmp(sinto->name, sfrom->name) == 0)
				break;
		if (sfrom == NULL) {
			d = diff_alloc(q, DIFF_ADD_SEARCH);
			if (d == NULL)
				return -1;
			d->search = sinto;
			rc = 0;
		}
	}

	TAILQ_FOREACH(sfrom, &from->sq, entries) {
		if (sfrom->name != NULL)
			continue;
		TAILQ_FOREACH(sinto, &into->sq, entries) {
			if (sinto->name != NULL ||
			    sinto->type != sfrom->type)
				continue;
			c = ort_diff_search(q, sfrom, sinto);
			if (c < 0)
				return -1;
			else if (c > 0)
				break;
		}
		if (sinto == NULL) {
			d = diff_alloc(q, DIFF_DEL_SEARCH);
			if (d == NULL)
				return -1;
			d->search = sfrom;
			rc = 0;
		}
	}

	TAILQ_FOREACH(sinto, &into->sq, entries) {
		if (sinto->name != NULL)
			continue;
		TAILQ_FOREACH(sfrom, &from->sq, entries) {
			if (sfrom->name != NULL ||
			    sfrom->type != sinto->type)
				continue;
			c = ort_diff_search(q, sfrom, sinto);
			if (c < 0)
				return -1;
			else if (c > 0)
				break;
		}
		if (sfrom == NULL) {
			d = diff_alloc(q, DIFF_ADD_SEARCH);
			if (d == NULL)
				return -1;
			d->search = sinto;
			rc = 0;
		}
	}

	return rc;
}

/*
 * Return >0 on failure, 0 if modified, >0 if same.
 */
static int
ort_diff_strct_insert(struct diffq *q,
	const struct strct *from, const struct strct *into)
{
	const struct insert	*fins = from->ins, *iins = into->ins;
	struct diff		*d;

	if (fins == NULL && iins == NULL)
		return 1;

	if (fins == NULL && iins != NULL) {
		if ((d = diff_alloc(q, DIFF_ADD_INSERT)) == NULL)
			return -1;
		d->strct = into;
		return 0;
	} else if (fins != NULL && iins == NULL) {
		if ((d = diff_alloc(q, DIFF_DEL_INSERT)) == NULL)
			return -1;
		d->strct = from;
		return 0;
	} 

	assert(fins != NULL && iins != NULL);

	if (ort_check_rolemap_roles(fins->rolemap, iins->rolemap)) {
		if ((d = diff_alloc(q, DIFF_SAME_INSERT)) == NULL)
			return -1;
		d->strct_pair.into = into;
		d->strct_pair.from = from;
		return 1;
	}

	if ((d = diff_alloc(q, DIFF_MOD_INSERT_ROLEMAP)) == NULL)
		return -1;
	d->strct_pair.into = into;
	d->strct_pair.from = from;
	if ((d = diff_alloc(q, DIFF_MOD_INSERT)) == NULL)
		return -1;
	d->strct_pair.into = into;
	d->strct_pair.from = from;

	return 0;
}

/*
 * Emit DIFF_ADD_FIELD and DIFF_DEL_FIELD, using ort_diff_field() for
 * same or different fields.
 * Return <0 on failure, 0 on dissimilar, >0 on similar.
 */
static int
ort_diff_fields(struct diffq *q,
	const struct strct *from, const struct strct *into)
{
	const struct field	*ifrom, *iinto;
	struct diff		*d;
	int			 rc = 1, c;

	TAILQ_FOREACH(iinto, &into->fq, entries) {
		TAILQ_FOREACH(ifrom, &from->fq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if (ifrom == NULL) {
			d = diff_alloc(q, DIFF_ADD_FIELD);
			if (d == NULL)
				return -1;
			d->field = iinto;
			rc = 0;
			continue;
		}
		if ((c = ort_diff_field(q, ifrom, iinto)) < 0)
			return -1;
		else if (c == 0)
			rc = 0;
	}

	TAILQ_FOREACH(ifrom, &from->fq, entries) {
		TAILQ_FOREACH(iinto, &into->fq, entries)
			if (strcasecmp(iinto->name, ifrom->name) == 0)
				break;
		if (iinto == NULL) {
			d = diff_alloc(q, DIFF_DEL_FIELD);
			if (d == NULL)
				return -1;
			d->field = ifrom;
			rc = 0;
		}
	}

	return rc;
}

/*
 * Emit DIFF_MOD_STRCT or DIFF_SAME_STRCT along with all subtypes.
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_strct(struct diffq *q,
	const struct strct *efrom, const struct strct *einto)
{
	const struct unique	*u;
	struct diff		*d;
	int			 rc;
	enum difftype		 type = DIFF_SAME_STRCT;

	/* All query types. */

	if ((rc = ort_diff_searchq(q, efrom, einto)) < 0)
		return 0;
	else if (rc == 0)
		type = DIFF_MOD_STRCT;

	/* Deletes and updates. */

	if ((rc = ort_diff_strct_updateq
	    (q, &efrom->uq, &einto->uq)) < 0)
		return 0;
	else if (rc == 0)
		type = DIFF_MOD_STRCT;

	if ((rc = ort_diff_strct_updateq
	    (q, &efrom->dq, &einto->dq)) < 0)
		return 0;
	else if (rc == 0)
		type = DIFF_MOD_STRCT;

	/* Insertions. */

	if ((rc = ort_diff_strct_insert(q, efrom, einto)) < 0)
		return 0;
	else if (rc == 0)
		type = DIFF_MOD_STRCT;

	/* Field add/del/mod. */

	if ((rc = ort_diff_fields(q, efrom, einto)) < 0)
		return 0;
	else if (rc == 0)
		type = DIFF_MOD_STRCT;

	/* Unique add/del. */

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

	/* Comment. */

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

	if (!ort_check_comment(efrom->doc, einto->doc)) {
		if ((d = diff_alloc(q, DIFF_MOD_ENM_COMMENT)) == NULL)
			return 0;
		d->enm_pair.from = efrom;
		d->enm_pair.into = einto;
		type = DIFF_MOD_ENM;
	}

	if (!ort_check_labels
	    (&efrom->labels_null, &einto->labels_null)) {
		if ((d = diff_alloc(q, DIFF_MOD_ENM_LABELS)) == NULL)
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
 * Emit DIFF_ADD_STRCT or DIFF_DEL_STRCT, else ort_diff_strct() to see
 * if they're different.
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
			d = diff_alloc(q, DIFF_ADD_STRCT);
			if (d == NULL)
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
			d = diff_alloc(q, DIFF_DEL_STRCT);
			if (d == NULL)
				return 0;
			d->strct = efrom;
		}
	}

	return 1;
}

/*
 * Emit DIFF_MOD_ROLE_xxx if roles differ.
 * Return <0 on failure, on if dissimilar, >0 if similar.
 */
static int
ort_diff_role(struct diffq *q,
	const struct role *rfrom, const struct role *rinto)
{
	const struct role	*cfrom, *cinto;
	size_t			 fsz = 0, isz = 0;
	struct diff		*d;
	int			 rc = 1;

	/* 
	 * If a parent is NULL, that will never change (only our
	 * "virtual" roles have NULL parents, and these are always
	 * fixed), so it suffices to just check names.
	 */

	if (rfrom->parent != NULL && rinto->parent != NULL &&
	    strcasecmp(rfrom->parent->name, rinto->parent->name)) {
		d = diff_alloc(q, DIFF_MOD_ROLE_PARENT);
		if (d == NULL)
			return -1;
		d->role_pair.from = rfrom;
		d->role_pair.into = rinto;
		rc = 0;
	}

	if (!ort_check_comment(rfrom->doc, rinto->doc)) {
		d = diff_alloc(q, DIFF_MOD_ROLE_COMMENT);
		if (d == NULL)
			return -1;
		d->role_pair.from = rfrom;
		d->role_pair.into = rinto;
		rc = 0;
	}

	/* 
	 * Check if the children have changed: if the count has changed,
	 * they've changed; otherwise, if any one has changed, they've
	 * changed as well.
	 */

	TAILQ_FOREACH(cfrom, &rfrom->subrq, entries)
		fsz++;
	TAILQ_FOREACH(cinto, &rinto->subrq, entries)
		isz++;

	if (fsz != isz) {
		d = diff_alloc(q, DIFF_MOD_ROLE_CHILDREN);
		if (d == NULL)
			return -1;
		d->role_pair.from = rfrom;
		d->role_pair.into = rinto;
		rc = 0;
	} else
		TAILQ_FOREACH(cfrom, &rfrom->subrq, entries) {
			TAILQ_FOREACH(cinto, &rinto->subrq, entries)
				if (strcasecmp
				    (cfrom->name, cinto->name) == 0)
					break;
			if (cinto != NULL)
				continue;
			d = diff_alloc(q, DIFF_MOD_ROLE_CHILDREN);
			if (d == NULL)
				return -1;
			d->role_pair.from = rfrom;
			d->role_pair.into = rinto;
			rc = 0;
			break;
		}

	return rc;
}

/*
 * Emit DIFF_DEL_ROLE, DIFF_ADD_ROLE.  Use ort_diff_role() for
 * DIFF_MOD_ROLE and DIFF_SAME_ROLE.
 * Return <0 on failure, 0 on dissimilar, >0 on similar.
 */
static int
ort_diff_roleq(struct diffq *q,
	const struct config *from, const struct config *into)
{
	const struct role	*rinto, *rfrom;
	struct diff		*d;
	int			 rc = 1, c;

	TAILQ_FOREACH(rfrom, &from->arq, allentries) {
		TAILQ_FOREACH(rinto, &into->arq, allentries)
			if (strcasecmp(rfrom->name, rinto->name) == 0)
				break;
		if (rinto == NULL) {
			d = diff_alloc(q, DIFF_DEL_ROLE);
			if (d == NULL)
				return -1;
			d->role = rfrom;
			rc = 0;
			continue;
		} else if (rfrom->parent == NULL)
			continue;

		if ((c = ort_diff_role(q, rfrom, rinto)) < 0)
			return -1;
		if (c == 0) {
			rc = 0;
			d = diff_alloc(q, DIFF_MOD_ROLE);
		} else
			d = diff_alloc(q, DIFF_SAME_ROLE);

		if (d == NULL)
			return -1;
		d->role_pair.into = rinto;
		d->role_pair.from = rfrom;
	}

	TAILQ_FOREACH(rinto, &into->arq, allentries) {
		TAILQ_FOREACH(rfrom, &from->arq, allentries)
			if (strcasecmp(rfrom->name, rinto->name) == 0)
				break;
		if (rfrom == NULL) {
			d = diff_alloc(q, DIFF_ADD_ROLE);
			if (d == NULL)
				return -1;
			d->role = rinto;
			rc = 0;
		}
	}

	return rc;
}

/*
 * Emit DIFF_ADD_ROLES or DIFF_DEL_ROLES.  Use ort_diff_roleq() for
 * DIFF_SAME_ROLES or DIFF_MOD_ROLES.
 * Return zero on failure, non-zero on success.
 */
static int
ort_diff_roles(struct diffq *q, 
	const struct config *from, const struct config *into)
{
	struct diff		*d;
	int			 rc;

	/* Do nothing if no roles in either. */

	if (TAILQ_EMPTY(&from->arq) && TAILQ_EMPTY(&into->arq))
		return 1;

	if (TAILQ_EMPTY(&from->arq) && !TAILQ_EMPTY(&into->arq)) {
		if ((d = diff_alloc(q, DIFF_ADD_ROLES)) == NULL)
			return 0;
		d->role = TAILQ_FIRST(&into->arq);
		return 1;
	}
	if (!TAILQ_EMPTY(&from->arq) && TAILQ_EMPTY(&into->arq)) {
		if ((d = diff_alloc(q, DIFF_DEL_ROLES)) == NULL)
			return 0;
		d->role = TAILQ_FIRST(&from->arq);
		return 1;
	}

	if ((rc = ort_diff_roleq(q, from, into)) < 0)
		return 0;
	d = diff_alloc(q, rc ? DIFF_SAME_ROLES : DIFF_MOD_ROLES);
	if (d == NULL)
		return 0;
	d->role_pair.into = TAILQ_FIRST(&into->arq);
	d->role_pair.from = TAILQ_FIRST(&from->arq);
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
	if (!ort_diff_roles(q, from, into))
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
