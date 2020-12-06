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

static	const char *difftypes[DIFF__MAX] = {
	NULL, /* DIFF_ADD_BITF */
	NULL, /* DIFF_ADD_BITIDX */
	NULL, /* DIFF_ADD_EITEM */
	NULL, /* DIFF_ADD_ENM */
	NULL, /* DIFF_ADD_FIELD */
	NULL, /* DIFF_ADD_INSERT */
	NULL, /* DIFF_ADD_ROLE */
	NULL, /* DIFF_ADD_ROLES */
	NULL, /* DIFF_ADD_STRCT */
	NULL, /* DIFF_ADD_UNIQUE */
	NULL, /* DIFF_ADD_UPDATE */
	NULL, /* DIFF_DEL_BITF */
	NULL, /* DIFF_DEL_BITIDX */
	NULL, /* DIFF_DEL_EITEM */
	NULL, /* DIFF_DEL_ENM */
	NULL, /* DIFF_DEL_FIELD */
	NULL, /* DIFF_DEL_INSERT */
	NULL, /* DIFF_DEL_ROLE */
	NULL, /* DIFF_DEL_ROLES */
	NULL, /* DIFF_DEL_STRCT */
	NULL, /* DIFF_DEL_UNIQUE */
	NULL, /* DIFF_DEL_UPDATE */
	NULL, /* DIFF_MOD_BITF */
	NULL, /* DIFF_MOD_BITF_COMMENT */
	NULL, /* DIFF_MOD_BITF_LABELS */
	NULL, /* DIFF_MOD_BITIDX */
	"comment", /* DIFF_MOD_BITIDX_COMMENT */
	"labels", /* DIFF_MOD_BITIDX_LABELS */
	"value", /* DIFF_MOD_BITIDX_VALUE */
	NULL, /* DIFF_MOD_EITEM */
	"comment", /* DIFF_MOD_EITEM_COMMENT */
	"labels", /* DIFF_MOD_EITEM_LABELS */
	"value", /* DIFF_MOD_EITEM_VALUE */
	NULL, /* DIFF_MOD_ENM */
	NULL, /* DIFF_MOD_ENM_COMMENT */
	NULL, /* DIFF_MOD_ENM_LABELS */
	NULL, /* DIFF_MOD_FIELD */
	"actions", /* DIFF_MOD_FIELD_ACTIONS */
	"bitf", /* DIFF_MOD_FIELD_BITF */
	"comment", /* DIFF_MOD_FIELD_COMMENT */
	"def", /* DIFF_MOD_FIELD_DEF */
	"enum", /* DIFF_MOD_FIELD_ENM */
	"flags", /* DIFF_MOD_FIELD_FLAGS */
	"ref", /* DIFF_MOD_FIELD_REFERENCE */
	"rolemap", /* DIFF_MOD_FIELD_ROLEMAP */
	"type", /* DIFF_MOD_FIELD_TYPE */
	"valids", /* DIFF_MOD_FIELD_VALIDS */
	NULL, /* DIFF_MOD_INSERT */
	NULL, /* DIFF_MOD_INSERT_ROLEMAP */
	NULL, /* DIFF_MOD_ROLE */
	"children", /* DIFF_MOD_ROLE_CHILDREN */
	"comment", /* DIFF_MOD_ROLE_COMMENT */
	"parent", /* DIFF_MOD_ROLE_PARENT */
	NULL, /* DIFF_MOD_ROLES */
	NULL, /* DIFF_MOD_STRCT */
	NULL, /* DIFF_MOD_STRCT_COMMENT */
	NULL, /* DIFF_MOD_UPDATE */
	"comment", /* DIFF_MOD_UPDATE_COMMENT */
	"flags", /* DIFF_MOD_UPDATE_FLAGS */
	"params", /* DIFF_MOD_UPDATE_PARAMS */
	"rolemap", /* DIFF_MOD_UPDATE_ROLEMAP */
	NULL, /* DIFF_SAME_BITF */
	NULL, /* DIFF_SAME_BITIDX */
	NULL, /* DIFF_SAME_EITEM */
	NULL, /* DIFF_SAME_ENM */
	NULL, /* DIFF_SAME_FIELD */
	NULL, /* DIFF_SAME_INSERT */
	NULL, /* DIFF_SAME_ROLE */
	NULL, /* DIFF_SAME_ROLES */
	NULL, /* DIFF_SAME_STRCT */
	NULL, /* DIFF_SAME_UPDATE */
};

static int
ort_write_one(FILE *f, int add, const char *name, const struct pos *pos)
{

	return fprintf(f, "%s %s %s:%zu:%zu\n", add ? "+" : "-", 
		name, pos->fname, pos->line, pos->column);
}

static int
ort_write_pair(FILE *f, int chnge, const char *name, 
	const struct pos *from, const struct pos *into)
{

	return fprintf(f, "%s%s %s:%zu:%zu -> %s:%zu:%zu%s\n",
		chnge ? "@@ " : "  ", name,
		from->fname, from->line, from->column,
		into->fname, into->line, into->column,
		chnge ? " @@" : "");
}

static int
ort_write_mod(FILE *f, const char *name, const char *type,
	const struct pos *from, const struct pos *into)
{

	return fprintf(f, "! %s %s %s:%zu:%zu -> %s:%zu:%zu\n",
		type, name,
		from->fname, from->line, from->column,
		into->fname, into->line, into->column);
}

static int
ort_write_unique(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "unique", &d->unique->pos);
}

static int
ort_write_field(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "field", &d->field->pos);
}

static int
ort_write_field_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "field",
		&d->field_pair.from->pos, &d->field_pair.into->pos);
}

static int
ort_write_field_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "field",
		&d->field_pair.from->pos, &d->field_pair.into->pos);
}

static int
ort_write_insert(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "insert", &d->strct->ins->pos);
}

static int
ort_write_insert_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "insert",
		&d->strct_pair.from->ins->pos, 
		&d->strct_pair.into->ins->pos);
}

static int
ort_write_insert_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "insert",
		&d->strct_pair.from->ins->pos, 
		&d->strct_pair.into->ins->pos);
}

static int
ort_write_strct(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "strct", &d->strct->pos);
}

static int
ort_write_strct_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "strct",
		&d->strct_pair.from->pos, &d->strct_pair.into->pos);
}

static int
ort_write_strct_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "strct",
		&d->strct_pair.from->pos, &d->strct_pair.into->pos);
}

static int
ort_write_bitf(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "bitf", &d->bitf->pos);
}

static int
ort_write_bitf_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "bitf",
		&d->bitf_pair.from->pos, &d->bitf_pair.into->pos);
}

static int
ort_write_bitf_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "bitf",
		&d->bitf_pair.from->pos, &d->bitf_pair.into->pos);
}

static int
ort_write_role(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "role", &d->role->pos);
}

static int
ort_write_role_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "role",
		&d->role_pair.from->pos, &d->role_pair.into->pos);
}

static int
ort_write_role_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "role",
		&d->role_pair.from->pos, &d->role_pair.into->pos);
}

static int
ort_write_roles(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "roles", &d->role->pos);
}

static int
ort_write_roles_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "roles",
		&d->role_pair.from->pos, &d->role_pair.into->pos);
}

static int
ort_write_enm(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "enm", &d->enm->pos);
}

static int
ort_write_enm_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "enm",
		&d->enm_pair.from->pos, &d->enm_pair.into->pos);
}

static int
ort_write_enm_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "enm",
		&d->enm_pair.from->pos, &d->enm_pair.into->pos);
}

static int
ort_write_bitidx(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "bitidx", &d->bitidx->pos);
}

static int
ort_write_bitidx_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "bitidx",
		&d->bitidx_pair.from->pos, &d->bitidx_pair.into->pos);
}

static int
ort_write_bitidx_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "bitidx",
		&d->bitidx_pair.from->pos, &d->bitidx_pair.into->pos);
}

static int
ort_write_eitem(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "eitem", &d->eitem->pos);
}

static int
ort_write_eitem_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "eitem",
		&d->eitem_pair.from->pos, &d->eitem_pair.into->pos);
}

static int
ort_write_eitem_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "eitem",
		&d->eitem_pair.from->pos, &d->eitem_pair.into->pos);
}

static int
ort_write_update(FILE *f, int add, const struct diff *d)
{

	return ort_write_one(f, add, "update", &d->update->pos);
}

static int
ort_write_update_mod(FILE *f, const char *name, const struct diff *d)
{

	return ort_write_mod(f, name, "update",
		&d->update_pair.from->pos, &d->update_pair.into->pos);
}

static int
ort_write_update_pair(FILE *f, int chnge, const struct diff *d)
{

	return ort_write_pair(f, chnge, "update",
		&d->update_pair.from->pos, &d->update_pair.into->pos);
}

static int
ort_write_diff_bitidx(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;

	assert(d->type == DIFF_MOD_BITIDX);

	TAILQ_FOREACH(dd, q, entries)
		switch (dd->type) {
		case DIFF_MOD_BITIDX_COMMENT:
		case DIFF_MOD_BITIDX_LABELS:
		case DIFF_MOD_BITIDX_VALUE:
			if (dd->bitidx_pair.into != 
			     d->bitidx_pair.into &&
			    dd->bitidx_pair.from != 
			     d->bitidx_pair.from)
				break;
			assert(dd->bitidx_pair.into ==
				d->bitidx_pair.into);
			assert(dd->bitidx_pair.from ==
				d->bitidx_pair.from);
			assert(difftypes[dd->type] != NULL);
			if (ort_write_bitidx_mod
			    (f, difftypes[dd->type], dd) < 0)
				return 0;
			break;
		default:
			break;
		}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_update(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;

	assert(d->type == DIFF_MOD_UPDATE);

	TAILQ_FOREACH(dd, q, entries)
		switch (dd->type) {
		case DIFF_MOD_UPDATE_COMMENT:
		case DIFF_MOD_UPDATE_FLAGS:
		case DIFF_MOD_UPDATE_PARAMS:
		case DIFF_MOD_UPDATE_ROLEMAP:
			if (dd->update_pair.into != 
			     d->update_pair.into &&
			    dd->update_pair.from != 
			     d->update_pair.from)
				break;
			assert(dd->update_pair.into ==
				d->update_pair.into);
			assert(dd->update_pair.from ==
				d->update_pair.from);
			assert(difftypes[dd->type] != NULL);
			if (ort_write_update_mod
			    (f, difftypes[dd->type], dd) < 0)
				return 0;
			break;
		default:
			break;
		}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_insert(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_INSERT);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_MOD_INSERT_ROLEMAP:
			if (dd->strct_pair.into != d->strct_pair.into)
				break;
			rc = ort_write_insert_mod(f, "rolemap", dd);
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
ort_write_diff_field(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;

	assert(d->type == DIFF_MOD_FIELD);

	TAILQ_FOREACH(dd, q, entries)
		switch (dd->type) {
		case DIFF_MOD_FIELD_ACTIONS:
		case DIFF_MOD_FIELD_BITF:
		case DIFF_MOD_FIELD_COMMENT:
		case DIFF_MOD_FIELD_DEF:
		case DIFF_MOD_FIELD_ENM:
		case DIFF_MOD_FIELD_FLAGS:
		case DIFF_MOD_FIELD_REFERENCE:
		case DIFF_MOD_FIELD_ROLEMAP:
		case DIFF_MOD_FIELD_TYPE:
		case DIFF_MOD_FIELD_VALIDS:
			if (dd->field_pair.into != d->field_pair.into &&
			    dd->field_pair.from != d->field_pair.from)
				break;
			assert(dd->field_pair.into == 
				d->field_pair.into);
			assert(dd->field_pair.from == 
				d->field_pair.from);
			assert(difftypes[dd->type] != NULL);
			if (ort_write_field_mod
			    (f, difftypes[dd->type], dd) < 0)
				return 0;
			break;
		default:
			break;
		}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_eitem(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;

	assert(d->type == DIFF_MOD_EITEM);

	TAILQ_FOREACH(dd, q, entries)
		switch (dd->type) {
		case DIFF_MOD_EITEM_COMMENT:
		case DIFF_MOD_EITEM_LABELS:
		case DIFF_MOD_EITEM_VALUE:
			if (dd->eitem_pair.into != d->eitem_pair.into &&
			    dd->eitem_pair.from != d->eitem_pair.from)
				break;
			assert(dd->eitem_pair.into == 
				d->eitem_pair.into);
			assert(dd->eitem_pair.from == 
				d->eitem_pair.from);
			assert(difftypes[dd->type] != NULL);
			if (ort_write_eitem_mod
			    (f, difftypes[dd->type], dd) < 0)
				return 0;
			break;
		default:
			break;
		}

	return 1;
}

/*
 * Return zero on failure, non-zero on success.
 */
static int
ort_write_diff_strct(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_STRCT);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_ADD_INSERT:
			if (dd->strct == d->strct_pair.into)
				rc = ort_write_insert(f, 1, dd);
			break;
		case DIFF_ADD_FIELD:
			if (dd->field->parent == d->strct_pair.into)
				rc = ort_write_field(f, 1, dd);
			break;
		case DIFF_ADD_UNIQUE:
			if (dd->unique->parent == d->strct_pair.into)
				rc = ort_write_unique(f, 1, dd);
			break;
		case DIFF_ADD_UPDATE:
			if (dd->update->parent == d->strct_pair.into)
				rc = ort_write_update(f, 1, dd);
			break;
		case DIFF_DEL_FIELD:
			if (dd->field->parent == d->strct_pair.from)
				rc = ort_write_field(f, 0, dd);
			break;
		case DIFF_DEL_INSERT:
			if (dd->strct == d->strct_pair.from)
				rc = ort_write_insert(f, 0, dd);
			break;
		case DIFF_DEL_UNIQUE:
			if (dd->unique->parent == d->strct_pair.from)
				rc = ort_write_unique(f, 0, dd);
			break;
		case DIFF_DEL_UPDATE:
			if (dd->update->parent == d->strct_pair.from)
				rc = ort_write_update(f, 0, dd);
			break;
		case DIFF_MOD_FIELD:
			if (dd->field_pair.into->parent != 
			    d->strct_pair.into)
				break;
			rc = ort_write_field_pair(f, 1, dd);
			if (!ort_write_diff_field(f, q, dd))
				return 0;
			break;
		case DIFF_MOD_INSERT:
			if (dd->strct_pair.into != d->strct_pair.into)
				break;
			rc = ort_write_insert_pair(f, 1, dd);
			if (!ort_write_diff_insert(f, q, dd))
				return 0;
			break;
		case DIFF_MOD_STRCT_COMMENT:
			if (dd->strct_pair.into == d->strct_pair.into)
				rc = ort_write_strct_mod(f, "comment", dd);
			break;
		case DIFF_MOD_UPDATE:
			if (dd->update_pair.into->parent != 
			    d->strct_pair.into)
				break;
			rc = ort_write_update_pair(f, 1, dd);
			if (!ort_write_diff_update(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_FIELD:
			if (dd->field_pair.into->parent != 
			    d->strct_pair.into)
				break;
			rc = ort_write_field_pair(f, 0, dd);
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
			rc = ort_write_bitf_mod(f, "comment", dd);
			break;
		case DIFF_MOD_BITF_LABELS:
			if (dd->bitf_pair.into != d->bitf_pair.into)
				break;
			rc = ort_write_bitf_mod(f, "labels", dd);
			break;
		case DIFF_ADD_BITIDX:
			if (dd->bitidx->parent != d->bitf_pair.into)
				break;
			rc = ort_write_bitidx(f, 1, dd);
			break;
		case DIFF_DEL_BITIDX:
			if (dd->bitidx->parent != d->bitf_pair.from)
				break;
			rc = ort_write_bitidx(f, 0, dd);
			break;
		case DIFF_MOD_BITIDX:
			if (dd->bitidx_pair.into->parent != 
			    d->bitf_pair.into)
				break;
			rc = ort_write_bitidx_pair(f, 1, dd);
			if (!ort_write_diff_bitidx(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_BITIDX:
			if (dd->bitidx_pair.into->parent != 
			    d->bitf_pair.into)
				break;
			rc = ort_write_bitidx_pair(f, 0, dd);
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
			rc = ort_write_enm_mod(f, "comment", dd);
			break;
		case DIFF_MOD_ENM_LABELS:
			if (dd->enm_pair.into != d->enm_pair.into)
				break;
			rc = ort_write_enm_mod(f, "labels", dd);
			break;
		case DIFF_ADD_EITEM:
			if (dd->eitem->parent != d->enm_pair.into)
				break;
			rc = ort_write_eitem(f, 1, dd);
			break;
		case DIFF_DEL_EITEM:
			if (dd->eitem->parent != d->enm_pair.from)
				break;
			rc = ort_write_eitem(f, 0, dd);
			break;
		case DIFF_MOD_EITEM:
			if (dd->eitem_pair.into->parent != 
			    d->enm_pair.into)
				break;
			rc = ort_write_eitem_pair(f, 1, dd);
			if (!ort_write_diff_eitem(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_EITEM:
			if (dd->eitem_pair.into->parent != 
			    d->enm_pair.into)
				break;
			rc = ort_write_eitem_pair(f, 0, dd);
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
		case DIFF_SAME_STRCT:
			rc = ort_write_strct_pair(f, 0, d);
			break;
		case DIFF_MOD_STRCT:
			rc = ort_write_strct_pair(f, 1, d);
			if (!ort_write_diff_strct(f, q, d))
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
ort_write_diff_bitfs(FILE *f, const struct diffq *q)
{
	const struct diff	*d;
	int		 	 rc;

	if (fprintf(f, "@@ bitfields @@\n") < 0)
		return 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_ADD_BITF:
			rc = ort_write_bitf(f, 1, d);
			break;
		case DIFF_DEL_BITF:
			rc = ort_write_bitf(f, 0, d);
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

static int
ort_write_diff_role(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;

	assert(d->type == DIFF_MOD_ROLE);

	TAILQ_FOREACH(dd, q, entries)
		switch (dd->type) {
		case DIFF_MOD_ROLE_COMMENT:
		case DIFF_MOD_ROLE_PARENT:
		case DIFF_MOD_ROLE_CHILDREN:
			if (dd->role_pair.into != d->role_pair.into &&
			    dd->role_pair.from != d->role_pair.from)
				break;
			assert(dd->role_pair.into == 
				d->role_pair.into);
			assert(dd->role_pair.from == 
				d->role_pair.from);
			assert(difftypes[dd->type] != NULL);
			if (ort_write_role_mod
			    (f, difftypes[dd->type], dd) < 0)
				return 0;
			break;
		default:
			break;
		}

	return 1;
}

static int
ort_write_diff_roleq(FILE *f, const struct diffq *q, const struct diff *d)
{
	const struct diff	*dd;
	int			 rc;

	assert(d->type == DIFF_MOD_ROLES);

	TAILQ_FOREACH(dd, q, entries) {
		rc = 1;
		switch (dd->type) {
		case DIFF_ADD_ROLE:
			rc = ort_write_role(f, 1, dd);
			break;
		case DIFF_DEL_EITEM:
			rc = ort_write_role(f, 0, dd);
			break;
		case DIFF_MOD_ROLE:
			rc = ort_write_role_pair(f, 1, dd);
			if (!ort_write_diff_role(f, q, dd))
				return 0;
			break;
		case DIFF_SAME_ROLE:
			rc = ort_write_role_pair(f, 0, dd);
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
ort_write_diff_roles(FILE *f, const struct diffq *q)
{
	const struct diff	*d;
	int		 	 rc;

	if (fprintf(f, "@@ roles @@\n") < 0)
		return 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_ADD_ROLES:
			rc = ort_write_roles(f, 1, d);
			break;
		case DIFF_DEL_ROLES:
			rc = ort_write_roles(f, 0, d);
			break;
		case DIFF_SAME_ROLES:
			rc = ort_write_roles_pair(f, 0, d);
			break;
		case DIFF_MOD_ROLES:
			rc = ort_write_roles_pair(f, 1, d);
			if (!ort_write_diff_roleq(f, q, d))
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
			rc = ort_write_enm(f, 1, d);
			break;
		case DIFF_DEL_ENM:
			rc = ort_write_enm(f, 0, d);
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
		if (d->type == DIFF_ADD_ROLES ||
		    d->type == DIFF_DEL_ROLES ||
		    d->type == DIFF_MOD_ROLES) {
			if (!ort_write_diff_roles(f, q))
				return 0;
			break;
		}

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
		    d->type == DIFF_DEL_STRCT ||
		    d->type == DIFF_MOD_STRCT) {
			if (!ort_write_diff_strcts(f, q))
				return 0;
			break;
		}

	return 1;
}
