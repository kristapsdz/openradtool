/*	$Id$ */
/*
 * Copyright (c) 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

static int
ort_write_strct(FILE *f, int add, const struct diff *d)
{
	return fprintf(f, "%s strct %s:%zu:%zu\n",
		add ? "+" : "-",
		d->strct->pos.fname,
		d->strct->pos.line,
		d->strct->pos.column);
}

static int
ort_write_bitf_pair(FILE *f, int nsame, const struct diff *d)
{

	return fprintf(f, "%s bitf %s:%zu:%zu -> %s:%zu:%zu %s\n",
		nsame ? "@@ " : "  ",
		d->bitf_pair.from->pos.fname,
		d->bitf_pair.from->pos.line,
		d->bitf_pair.from->pos.column,
		d->bitf_pair.into->pos.fname,
		d->bitf_pair.into->pos.line,
		d->bitf_pair.into->pos.column,
		nsame ? " @@" : "");
}

static int
ort_write_enm_pair(FILE *f, int nsame, const struct diff *d)
{

	return fprintf(f, "%s enm %s:%zu:%zu -> %s:%zu:%zu %s\n",
		nsame ? "@@ " : "  ",
		d->enm_pair.from->pos.fname,
		d->enm_pair.from->pos.line,
		d->enm_pair.from->pos.column,
		d->enm_pair.into->pos.fname,
		d->enm_pair.into->pos.line,
		d->enm_pair.into->pos.column,
		nsame ? " @@" : "");
}

static int
ort_write_bitidx_pair(FILE *f, const char *name, const struct diff *d)
{

	return fprintf(f, "! bitidx %s %s:%zu:%zu -> %s:%zu:%zu\n", 
		name,
		d->bitidx_pair.from->pos.fname,
		d->bitidx_pair.from->pos.line,
		d->bitidx_pair.from->pos.column,
		d->bitidx_pair.into->pos.fname,
		d->bitidx_pair.into->pos.line,
		d->bitidx_pair.into->pos.column);
}

static int
ort_write_eitem_pair(FILE *f, const char *name, const struct diff *d)
{
	return fprintf(f, "! eitem %s %s:%zu:%zu -> %s:%zu:%zu\n",
		name,
		d->eitem_pair.from->pos.fname,
		d->eitem_pair.from->pos.line,
		d->eitem_pair.from->pos.column,
		d->eitem_pair.into->pos.fname,
		d->eitem_pair.into->pos.line,
		d->eitem_pair.into->pos.column);
}

static int
ort_write_diff_bitidx(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_BITIDX);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_MOD_BITIDX_COMMENT:
			if (dd->bitidx_pair.into != d->bitidx_pair.into)
				break;
			rc = ort_write_bitidx_pair(f, "comment", dd);
			break;
		case DIFF_MOD_BITIDX_VALUE:
			if (dd->bitidx_pair.into != d->bitidx_pair.into)
				break;
			rc = ort_write_bitidx_pair(f, "value", dd);
			break;
		default:
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

static int
ort_write_diff_eitem(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_EITEM);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_MOD_EITEM_COMMENT:
			if (dd->eitem_pair.into != d->eitem_pair.into)
				break;
			rc = ort_write_eitem_pair(f, "comment", dd);
			break;
		case DIFF_MOD_EITEM_VALUE:
			if (dd->eitem_pair.into != d->eitem_pair.into)
				break;
			rc = ort_write_eitem_pair(f, "value", dd);
			break;
		default:
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

static int
ort_write_diff_bitf(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_BITF);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_MOD_BITF_COMMENT:
			if (dd->bitf_pair.into != d->bitf_pair.into)
				break;
			rc = fprintf(f, "! bitf comment "
				"%s:%zu:%zu -> %s:%zu:%zu\n",
				dd->bitf_pair.from->pos.fname,
				dd->bitf_pair.from->pos.line,
				dd->bitf_pair.from->pos.column,
				dd->bitf_pair.into->pos.fname,
				dd->bitf_pair.into->pos.line,
				dd->bitf_pair.into->pos.column);
			break;
		case DIFF_ADD_BITIDX:
			if (dd->bitidx->parent != d->bitf_pair.into)
				break;
			rc = fprintf(f, "+ bitidx %s:%zu:%zu\n",
				dd->bitidx->pos.fname,
				dd->bitidx->pos.line,
				dd->bitidx->pos.column);
			break;
		case DIFF_DEL_BITIDX:
			if (dd->bitidx->parent != d->bitf_pair.from)
				break;
			rc = fprintf(f, "- bitidx %s:%zu:%zu\n",
				dd->bitidx->pos.fname,
				dd->bitidx->pos.line,
				dd->bitidx->pos.column);
			break;
		case DIFF_MOD_BITIDX:
			if (dd->bitidx_pair.into->parent != 
			    d->bitf_pair.into)
				break;
			rc = fprintf(f, "@@ bitidx "
				"%s:%zu:%zu -> %s:%zu:%zu @@\n",
				dd->bitidx_pair.from->pos.fname,
				dd->bitidx_pair.from->pos.line,
				dd->bitidx_pair.from->pos.column,
				dd->bitidx_pair.into->pos.fname,
				dd->bitidx_pair.into->pos.line,
				dd->bitidx_pair.into->pos.column);
			if (!ort_write_diff_bitidx(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_BITIDX:
			if (dd->bitidx_pair.into->parent != 
			    d->bitf_pair.into)
				break;
			rc = fprintf(f, "  bitidx "
				"%s:%zu:%zu -> %s:%zu:%zu\n",
				dd->bitidx_pair.from->pos.fname,
				dd->bitidx_pair.from->pos.line,
				dd->bitidx_pair.from->pos.column,
				dd->bitidx_pair.into->pos.fname,
				dd->bitidx_pair.into->pos.line,
				dd->bitidx_pair.into->pos.column);
			break;
		default:
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

static int
ort_write_diff_enm(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_ENM);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_MOD_ENM_COMMENT:
			if (dd->enm_pair.into != d->enm_pair.into)
				break;
			rc = fprintf(f, "! enm comment "
				"%s:%zu:%zu -> %s:%zu:%zu\n",
				dd->enm_pair.from->pos.fname,
				dd->enm_pair.from->pos.line,
				dd->enm_pair.from->pos.column,
				dd->enm_pair.into->pos.fname,
				dd->enm_pair.into->pos.line,
				dd->enm_pair.into->pos.column);
			break;
		case DIFF_ADD_EITEM:
			if (dd->eitem->parent != d->enm_pair.into)
				break;
			rc = fprintf(f, "+ eitem %s:%zu:%zu\n",
				dd->eitem->pos.fname,
				dd->eitem->pos.line,
				dd->eitem->pos.column);
			break;
		case DIFF_DEL_EITEM:
			if (dd->eitem->parent != d->enm_pair.from)
				break;
			rc = fprintf(f, "- eitem %s:%zu:%zu\n",
				dd->eitem->pos.fname,
				dd->eitem->pos.line,
				dd->eitem->pos.column);
			break;
		case DIFF_MOD_EITEM:
			if (dd->eitem_pair.into->parent != 
			    d->enm_pair.into)
				break;
			rc = fprintf(f, "@@ eitem "
				"%s:%zu:%zu -> %s:%zu:%zu @@\n",
				dd->eitem_pair.from->pos.fname,
				dd->eitem_pair.from->pos.line,
				dd->eitem_pair.from->pos.column,
				dd->eitem_pair.into->pos.fname,
				dd->eitem_pair.into->pos.line,
				dd->eitem_pair.into->pos.column);
			if (!ort_write_diff_eitem(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_EITEM:
			if (dd->eitem_pair.into->parent != 
			    d->enm_pair.into)
				break;
			rc = fprintf(f, "  eitem "
				"%s:%zu:%zu -> %s:%zu:%zu\n",
				dd->eitem_pair.from->pos.fname,
				dd->eitem_pair.from->pos.line,
				dd->eitem_pair.from->pos.column,
				dd->eitem_pair.into->pos.fname,
				dd->eitem_pair.into->pos.line,
				dd->eitem_pair.into->pos.column);
			break;
		default:
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_strcts(FILE *f, const struct diffq *q)
{
	const struct diff	*d;
	int		 	 rc;

	if (fprintf(f, "@@ strcts @@\n") < 0)
		return 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_ADD_STRCT:
			rc = ort_write_strct(f, 1, d);
			break;
		case DIFF_DEL_STRCT:
			rc = ort_write_strct(f, 0, d);
			break;
		default:
			rc = 1;
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_bitfs(FILE *f, const struct diffq *q)
{
	const struct diff	*d;
	int		 	 rc;

	if (fprintf(f, "@@ bitfields @@\n") < 0)
		return 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_ADD_BITF:
			rc = fprintf(f, "+ bitf %s:%zu:%zu\n",
				d->bitf->pos.fname,
				d->bitf->pos.line,
				d->bitf->pos.column);
			break;
		case DIFF_DEL_BITF:
			rc = fprintf(f, "- bitf %s:%zu:%zu\n",
				d->bitf->pos.fname,
				d->bitf->pos.line,
				d->bitf->pos.column);
			break;
		case DIFF_SAME_BITF:
			rc = ort_write_bitf_pair(f, 0, d);
			break;
		case DIFF_MOD_BITF:
			rc = ort_write_bitf_pair(f, 1, d);
			if (!ort_write_diff_bitf(f, q, d))
				return 0;
			break;
		default:
			rc = 1;
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_enms(FILE *f, const struct diffq *q)
{
	const struct diff	*d;
	int		 	 rc;

	if (fprintf(f, "@@ enumerations @@\n") < 0)
		return 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_ADD_ENM:
			rc = fprintf(f, "+ enm %s:%zu:%zu\n",
				d->enm->pos.fname,
				d->enm->pos.line,
				d->enm->pos.column);
			break;
		case DIFF_DEL_ENM:
			rc = fprintf(f, "- enm %s:%zu:%zu\n",
				d->enm->pos.fname,
				d->enm->pos.line,
				d->enm->pos.column);
			break;
		case DIFF_SAME_ENM:
			rc = ort_write_enm_pair(f, 0, d);
			break;
		case DIFF_MOD_ENM:
			rc = ort_write_enm_pair(f, 1, d);
			if (!ort_write_diff_enm(f, q, d))
				return 0;
			break;
		default:
			rc = 1;
			break;
		}
		if (rc < 0)
			return 0;
	}

	return 1;
}

int
ort_write_diff_file(FILE *f, const struct diffq *q, 
	const char **into, size_t intosz,
	const char **from, size_t fromsz)
{
	const struct diff	*d;
	size_t			 i;

	for (i = 0; i < fromsz; i++)
		fprintf(f, "--- %s\n", from[i]);
	if (fromsz == 0)
		fprintf(f, "--- <stdin>\n");

	for (i = 0; i < intosz; i++)
		fprintf(f, "+++ %s\n", into[i]);
	if (intosz == 0)
		fprintf(f, "+++ <stdin>\n");

	/* Write enumerations iff any have changed. */

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_ADD_ENM ||
		    d->type == DIFF_DEL_ENM ||
		    d->type == DIFF_MOD_ENM) {
			if (!ort_write_diff_enms(f, q))
				return 0;
			break;
		}

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_ADD_BITF ||
		    d->type == DIFF_DEL_BITF ||
		    d->type == DIFF_MOD_BITF) {
			if (!ort_write_diff_bitfs(f, q))
				return 0;
			break;
		}

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_ADD_STRCT ||
		    d->type == DIFF_DEL_STRCT) {
			if (!ort_write_diff_strcts(f, q))
				return 0;
			break;
		}

	return 1;
}
