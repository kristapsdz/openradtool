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
#include "ort-lang-json.h"

static const char *const ordtypes[] = {
	"asc", /* ORDTYPE_ASC */
	"desc", /* ORDTYPE_DESC */
};

static const char *const aggrtypes[] = {
	"maxrow", /* AGGR_MAXROW */
	"minrow", /* AGGR_MINROW */
};

static const char *const modtypes[MODTYPE__MAX] = {
	"concat", /* MODTYPE_CONCAT */
	"dec", /* MODTYPE_DEC */
	"inc", /* MODTYPE_INC */
	"set", /* MODTYPE_SET */
	"strset", /* MODTYPE_STRSET */
};

static const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYPE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYPE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	"streq", /* OPTYPE_STREQ */
	"strneq", /* OPTYPE_STRNEQ */
	"isnull", /* OPTYPE_ISNULL */
	"notnull", /* OPTYPE_NOTNULL */
};

static const char *const ftypes[FTYPE__MAX] = {
	"bit", /* FTYPE_BIT */
	"date", /* FTYPE_DATE */
	"epoch", /* FTYPE_EPOCH */
	"int", /* FTYPE_INT */
	"real", /* FTYPE_REAL */
	"blob", /* FTYPE_BLOB */
	"text", /* FTYPE_TEXT */
	"password", /* FTYPE_PASSWORD */
	"email", /* FTYPE_EMAIL */
	"struct", /* FTYPE_STRUCT */
	"enum", /* FTYPE_ENUM */
	"bitfield", /* FTYPE_BITFIELD */
};

static const char *const vtypes[VALIDATE__MAX] = {
	"ge", /* VALIDATE_GE */
	"le", /* VALIDATE_LE */
	"gt", /* VALIDATE_GT */
	"lt", /* VALIDATE_LT */
	"eq", /* VALIDATE_EQ */
};

static const char *const upacts[UPACT__MAX] = {
	"none", /* UPACT_NONE */
	"restrict", /* UPACT_RESTRICT */
	"nullify", /* UPACT_NULLIFY */
	"cascade", /* UPACT_CASCADE */
	"default", /* UPACT_DEFAULT */
};

static const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"search", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static const char *const rolemapts[ROLEMAP__MAX] = {
	"all", /* ROLEMAP_ALL */
	"count", /* ROLEMAP_COUNT */
	"delete", /* ROLEMAP_DELETE */
	"insert", /* ROLEMAP_INSERT */
	"iterate", /* ROLEMAP_ITERATE */
	"list", /* ROLEMAP_LIST */
	"search", /* ROLEMAP_SEARCH */
	"update", /* ROLEMAP_UPDATE */
	"noexport", /* ROLEMAP_NOEXPORT */
};

/*
 * Format the string according to the JSON specification.
 * Note about escaping the double-quote: in ort(3), double-quotes are
 * already escaped, so we need not do so again here.
 * Return zero on failure, non-zero on sucess.
 */
static int
gen_string(FILE *f, const char *cp)
{
	int	 rc;

	if (fputs(" \"", f) == EOF)
		return 0;
	for ( ; *cp != '\0'; cp++) {
		switch (*cp) {
		case '\b':
			rc = fputs("\\b", f) != EOF;
			break;
		case '\f':
			rc = fputs("\\f", f) != EOF;
			break;
		case '\n':
			rc = fputs("\\n", f) != EOF;
			break;
		case '\r':
			rc = fputs("\\r", f) != EOF;
			break;
		case '\t':
			rc = fputs("\\t", f) != EOF;
			break;
		default:
			if ((unsigned char)*cp < 32)
				rc = fprintf(f, "\\u%.4u", *cp) > 0;
			else
				rc = fputc(*cp, f) != EOF;
		}
		if (!rc)
			return 0;
	}
	return fputc('\"', f) != EOF;
}

/*
 * Generate a full rolemap: rolemapObj[] w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_rolemap_full(FILE *f, const struct rolemap *map)
{
	const struct rref	*ref;

	if (fprintf(f, " { \"type\": \"%s\", \"rq\": [", 
	   rolemapts[map->type]) < 0)
		return 0;

	TAILQ_FOREACH(ref, &map->rq, entries) {
		if (fprintf(f, " \"%s\"", ref->role->name) < 0)
			return 0;
		if (TAILQ_NEXT(ref, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"name\": ", f) == EOF)
		return 0;

	switch (map->type) {
	case ROLEMAP_NOEXPORT:
		if (map->f == NULL) {
			if (fputs("null", f) == EOF)
				return 0;
			break;
		}
		if (fprintf(f, "\"%s\"", map->f->name) < 0)
			return 0;
		break;
	case ROLEMAP_UPDATE:
	case ROLEMAP_DELETE:
		if (fprintf(f, "\"%s\"", map->u->name) < 0)
			return 0;
		break;
	case ROLEMAP_INSERT:
	case ROLEMAP_ALL:
		if (fputs("null", f) == EOF)
			return 0;
		break;
	default:
		if (fprintf(f, "\"%s\"", map->s->name) < 0)
			return 0;
		break;
	}

	return fputc('}', f) != EOF;
}

/*
 * Generate "rolemap": string[] w/comma if comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_rolemap(FILE *f, int comma, const struct rolemap *map)
{
	const struct rref	*ref;

	if (fputs( "\"rolemap\": [", f) == EOF)
		return 0;
	if (map != NULL)
		TAILQ_FOREACH(ref, &map->rq, entries) {
			if (fprintf(f, " \"%s\"", ref->role->name) < 0)
				return 0;
			if (TAILQ_NEXT(ref, entries) != NULL &&
			    fputc(',', f) == EOF)
				return 0;
		}
	if (fputc(']', f) == EOF)
		return 0;
	return comma ? fputc(',', f) != EOF : 1;
}

/*
 * Emit posObj w/comma.
 * Return zero on failure, non-zero on success
 */
static int
gen_pos(FILE *f, const struct pos *pos)
{

	if (fputs(" \"pos\": { \"fname\": ", f) == EOF)
		return 0;
	if (!gen_string(f, pos->fname))
		return 0;
	return fprintf(f, ", \"line\": %zu, \"column\": %zu },", 
		pos->column, pos->line) > 0;
}

/*
 * Emit "name": { labelObj } w/o comma.
 * The name is the language key.
 * Return zero on failure, non-zero on success
 */
static int
gen_label(FILE *f, const char *name,
	const struct label *l, const struct config *cfg)
{

	if (fprintf(f, " \"%s\": { \"lang\": \"%s\",", name, name) < 0)
		return 0;
	if (!gen_pos(f, &l->pos))
		return 0;
	if (fputs(" \"value\":", f) == EOF)
		return 0;
	if (!gen_string(f, l->label))
		return 0;
	return fputs(" }", f) != EOF;
}

static int
gen_labelq(FILE *f, const char *name,
	const struct labelq *q, const struct config *cfg)
{
	const struct label	*l;
	const char		*lang;

	if (fprintf(f, " \"%s\": {", name) < 0)
		return 0;
	TAILQ_FOREACH(l, q, entries) {
		lang = cfg->langs[l->lang][0] == '\0' ?
			"_default" : cfg->langs[l->lang];
		if (!gen_label(f, lang, l, cfg))
			return 0;
		if (TAILQ_NEXT(l, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
	}

	return fputs(" },", f) != EOF;
}

/*
 * Emit "doc": string|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_doc(FILE *f, const char *doc)
{

	if (fputs(" \"doc\": ", f) == EOF)
		return 0;
	if (doc == NULL)
		return fputs("null,", f) != EOF;
	if (!gen_string(f, doc))
		return 0;
	return fputc(',', f) != EOF;
}

/*
 * Emit "name": { enumItemObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_eitem(FILE *f, const struct eitem *ei, const struct config *cfg)
{

	if (fprintf(f, " \"%s\": { \"name\": \"%s\", "
	    "\"parent\": \"%s\", ", 
	    ei->name, ei->name, ei->parent->name) < 0)
		return 0;
	if (!gen_pos(f, &ei->pos))
		return 0;
	if (!gen_doc(f, ei->doc))
		return 0;
	if (!gen_labelq(f, "labels", &ei->labels, cfg))
		return 0;
	if (fputs(" \"value\": ", f) == EOF)
		return 0;
	if (ei->flags & EITEM_AUTO)
		return fputs("null }", f) != EOF;
	return fprintf(f, "\"%" PRId64 "\" }", ei->value) > 0;
}

/*
 * Emit "name": { enumObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_enm(FILE *f, const struct enm *enm, const struct config *cfg)
{
	const struct eitem	*ei;

	if (fprintf(f, " \"%s\": { \"name\": \"%s\", ", 
	    enm->name, enm->name) < 0)
		return 0;
	if (!gen_pos(f, &enm->pos))
		return 0;
	if (!gen_doc(f, enm->doc))
		return 0;
	if (!gen_labelq(f, "labelsNull", &enm->labels_null, cfg))
		return 0;

	if (fputs(" \"eq\": {", f) == EOF)
		return 0;
	TAILQ_FOREACH(ei, &enm->eq, entries) {
		if (!gen_eitem(f, ei, cfg))
			return 0;
		if (TAILQ_NEXT(ei, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}

	return fputs(" } }", f) != EOF;
}

/*
 * Emit "eq": { enumSet } w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_enms(FILE *f, const struct config *cfg)
{
	const struct enm	*enm;

	if (fputs(" \"eq\": {", f) == EOF)
		return 0;
	TAILQ_FOREACH(enm, &cfg->eq, entries) {
		if (!gen_enm(f, enm, cfg))
			return 0;
		if (TAILQ_NEXT(enm, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs(" },", f) != EOF;
}

/*
 * Emit "name": { bitIndexObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_bitidx(FILE *f, const struct bitidx *bi, const struct config *cfg)
{

	if (fprintf(f, " \"%s\": { \"parent\": \"%s\", "
	    "\"name\": \"%s\", ", 
	    bi->name, bi->parent->name, bi->name) < 0)
		return 0;
	if (!gen_pos(f, &bi->pos))
		return 0;
	if (!gen_doc(f, bi->doc))
		return 0;
	if (!gen_labelq(f, "labels", &bi->labels, cfg))
		return 0;
	return fprintf(f, " \"mask\": \"%" PRIu64 "\", \"value\": "
		"\"%" PRId64 "\" }", 
		UINT64_C(1) << (uint64_t)bi->value, 
		bi->value) >= 0;
}

/*
 * Emit "name": { bitfObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_bitf(FILE *f, const struct bitf *bitf, const struct config *cfg)
{
	const struct bitidx	*bi;

	if (fprintf(f, " \"%s\": { \"name\": \"%s\", ", 
	    bitf->name, bitf->name) < 0)
		return 0;
	if (!gen_pos(f, &bitf->pos))
		return 0;
	if (!gen_doc(f, bitf->doc))
		return 0;
	if (!gen_labelq(f, "labelsNull", &bitf->labels_null, cfg))
		return 0;
	if (!gen_labelq(f, "labelsUnset", &bitf->labels_unset, cfg))
		return 0;
	if (fputs(" \"bq\": {", f) == EOF)
		return 0;
	TAILQ_FOREACH(bi, &bitf->bq, entries) {
		if (!gen_bitidx(f, bi, cfg))
			return 0;
		if (TAILQ_NEXT(bi, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs(" } }", f) != EOF;
}

/*
 * Emit "bq": { bitfSet }|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_bitfs(FILE *f, const struct config *cfg)
{
	const struct bitf	*bitf;

	if (fputs(" \"bq\": {", f) == EOF)
		return 0;
	TAILQ_FOREACH(bitf, &cfg->bq, entries) {
		if (!gen_bitf(f, bitf, cfg))
			return 0;
		if (TAILQ_NEXT(bitf, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}

	return fputs(" },", f) != EOF;
}

static int
gen_role(FILE *f, const struct role *r)
{
	const struct role	*rr;

	if (fprintf(f, " \"%s\": { \"name\": \"%s\", \"parent\": ", 
	    r->name, r->name) < 0)
		return 0;
	if (r->parent == NULL) {
		if (fputs("null, ", f) == EOF)
			return 0;
	} else if (fprintf(f, "\"%s\", ", r->parent->name) < 0)
		return 0;
	if (!gen_pos(f, &r->pos))
		return 0;
	if (!gen_doc(f, r->doc))
		return 0;
	if (fputs(" \"subrq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(rr, &r->subrq, entries) {
		if (fprintf(f, "\"%s\"", rr->name) < 0)
			return 0;
		if (TAILQ_NEXT(rr, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs("] }", f) != EOF;
}

static int
gen_roles(FILE *f, const struct config *cfg)
{
	const struct role	*r;

	if (fputs(" \"rq\": ", f) == EOF)
		return 0;
	if (TAILQ_EMPTY(&cfg->rq))
		return fputs("null,", f) != EOF;
	if (fputc('{', f) == EOF)
		return 0;
	TAILQ_FOREACH(r, &cfg->arq, allentries) {
		if (!gen_role(f, r))
			return 0;
		if (TAILQ_NEXT(r, allentries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs(" },", f) != EOF;
}

/*
 * Emit "name": { fieldObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_field(FILE *f, const struct field *fd)
{
	struct fvalid	*fv;
	unsigned int	 fl;

	if (fprintf(f, " \"%s\": { \"name\": \"%s\", "
	    "\"parent\": \"%s\", ", 
	    fd->name, fd->name, fd->parent->name) < 0)
		return 0;
	if (!gen_pos(f, &fd->pos))
		return 0;
	if (!gen_doc(f, fd->doc))
		return 0;

	fl = (fd->flags & FIELD_ROWID) |
		(fd->flags & FIELD_UNIQUE) | 
		(fd->flags & FIELD_NOEXPORT) | 
		(fd->flags & FIELD_NULL);
	if (fputs(" \"flags\": [", f) == EOF)
		return 0;
	if (fl & FIELD_ROWID) {
		if (fputs("\"rowid\"", f) == EOF)
			return 0;
		if ((fl &= ~FIELD_ROWID) && fputs(", ", f) == EOF)
			return 0;
	}
	if (fl & FIELD_UNIQUE) { 
		if (fputs("\"unique\"", f) == EOF)
			return 0;
		if ((fl &= ~FIELD_UNIQUE) && fputs(", ", f) == EOF)
			return 0;
	}
	if (fl & FIELD_NOEXPORT) { 
		if (fputs("\"noexport\"", f) == EOF)
			return 0;
		if ((fl &= ~FIELD_NOEXPORT) && fputs(", ", f) == EOF)
			return 0;
	}
	if ((fl & FIELD_NULL) && fputs("\"null\"", f) == EOF)
		return 0;
	if (fputs("],", f) == EOF)
		return 0;

	if (fd->enm != NULL &&
	    fprintf(f, " \"enm\": \"%s\",", fd->enm->name) < 0)
		return 0;
	if (fd->bitf != NULL &&
	    fprintf(f, " \"bitf\": \"%s\",", fd->bitf->name) < 0)
		return 0;
	if (fd->ref != NULL && fprintf(f, " \"ref\": {"
	    " \"target\": { \"strct\": \"%s\", \"field\": \"%s\" },"
	    " \"source\": { \"strct\": \"%s\", \"field\": \"%s\" } },",
	    fd->ref->target->parent->name, fd->ref->target->name,
	    fd->ref->source->parent->name, fd->ref->source->name) < 0)
		return 0;
	if (fputs(" \"def\": ", f) == EOF)
		return 0;
	if (fd->flags & FIELD_HASDEF) {
		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			if (fprintf
		  	    (f, "\"%" PRId64 "\"", fd->def.integer) < 0)
				return 0;
			break;
		case FTYPE_REAL:
			if (fprintf(f, "\"%g\"", fd->def.decimal) < 0)
				return 0;
			break;
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
			if (!gen_string(f, fd->def.string))
				return 0;
			break;
		case FTYPE_ENUM:
			if (fprintf
			    (f, "\"%s\"", fd->def.eitem->name) < 0)
				return 0;
			break;
		default:
			abort();
			break;
		}
	} else if (fputs("null", f) == EOF)
		return 0;

	if (fprintf(f, ","
	    " \"actdel\": \"%s\", \"actup\": \"%s\",", 
	    upacts[fd->actdel], upacts[fd->actup]) < 0)
		return 0;
	if (!gen_rolemap(f, 1, fd->rolemap))
		return 0;
	if (fputs(" \"fvq\": [", f) == EOF)
		return 0;	
	TAILQ_FOREACH(fv, &fd->fvq, entries) {
		if (fprintf(f, " { \"type\": \"%s\", \"limit\": \"", 
		    vtypes[fv->type]) < 0)
			return 0;
		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			if (fprintf(f, "%" PRId64, 
			    fv->d.value.integer) < 0)
				return 0;
			break;
		case FTYPE_REAL:
			if (fprintf(f, "%g", 
			    fv->d.value.decimal) < 0)
				return 0;
			break;
		case FTYPE_BLOB:
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
			if (fprintf(f, "%zu", 
			    fv->d.value.len) < 0)
				return 0;
			break;
		default:
			abort();
		}
		if (fputs("\"}", f) == EOF)
			return 0;
		if (TAILQ_NEXT(fv, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}

	return fprintf(f, " ], \"type\": \"%s\" }", 
		ftypes[fd->type]) > 0;
}

/*
 * Emit { insertObj }|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_insert(FILE *f, const struct insert *insert)
{

	if (insert == NULL)
		return fputs(" null,", f) != EOF;
	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &insert->pos))
		return 0;
	if (!gen_rolemap(f, 0, insert->rolemap))
		return 0;
	return fputs(" },", f) != EOF;
}

static int
gen_chain(FILE *f, const struct field **chain, size_t chainsz)
{
	size_t	 i;

	if (fputs(" \"chain\": [", f) == EOF)
		return 0;
	for (i = 0; i < chainsz; i++) 
		if (fprintf(f, "%s\"%s.%s\"", i > 0 ? ", " : "", 
		    chain[i]->parent->name, chain[i]->name) < 0)
			return 0;
	return fputs(" ],", f) != EOF;
}

/*
 * Emit { orderObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_order(FILE *f, const struct ord *o)
{

	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &o->pos))
		return 0;
	if (!gen_chain(f, (const struct field **)o->chain, o->chainsz))
		return 0;
	return fprintf(f, 
		" \"fname\": \"%s\", \"op\": \"%s\" }", 
		o->fname, ordtypes[o->op]) > 0;
}

/*
 * Emit { sentObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_sent(FILE *f, const struct sent *s)
{

	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &s->pos))
		return 0;
	if (!gen_chain(f, (const struct field **)s->chain, s->chainsz))
		return 0;
	return fprintf(f, " \"fname\": \"%s\", \"uname\": \"%s\", "
		"\"field\": { \"strct\": \"%s\", \"field\": \"%s\" }, "
		"\"op\": \"%s\" }", s->fname, s->uname, 
		s->field->parent->name, s->field->name,
		optypes[s->op]) > 0;
}

/*
 * Emit { groupObj }|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_group(FILE *f, const struct group *g)
{

	if (g == NULL)
		return fputs(" null,", f) != EOF;
	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &g->pos))
		return 0;
	if (!gen_chain(f, (const struct field **)g->chain, g->chainsz))
		return 0;
	return fprintf(f, " \"fname\": \"%s\" },", g->fname) > 0;
}

/*
 * Emit { dstnctObj }|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_distinct(FILE *f, const struct dstnct *d)
{

	if (d == NULL)
		return fputs(" null", f) != EOF;
	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &d->pos))
		return 0;
	if (!gen_chain(f, (const struct field **)d->chain, d->chainsz))
		return 0;
	return fprintf(f, " \"strct\": \"%s\", \"fname\": \"%s\" }", 
		d->strct->name, d->fname) > 0;
}

/*
 * Emit { aggrObj }|null w/comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_aggr(FILE *f, const struct aggr *a)
{

	if (a == NULL)
		return fputs(" null,", f) != EOF;
	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &a->pos))
		return 0;
	if (!gen_chain(f, (const struct field **)a->chain, a->chainsz))
		return 0;
	return fprintf(f, 
		" \"fname\": \"%s\", \"op\": \"%s\" },", 
		a->fname, aggrtypes[a->op]) > 0;
}

/*
 * Emit "name": { searchObj } w/o comma.
 * As an exception to the norm, start with a comma depending upon
 * "first".
 * This is to ease iteration through the search queue.
 * Return zero on failure, non-zero on success.
 */
static int
gen_search(FILE *f, int *first, const struct search *s)
{
	const struct sent	*sent;
	const struct ord	*ord;

	if (*first == 0) {
		if (fputc(',', f) == EOF)
			return 0;
	} else
		*first = 0;

	if (s->name != NULL && fprintf(f, " \"%s\":", s->name) < 0)
		return 0;
	if (fputs(" {", f) == EOF)
		return 0;
	if (s->name != NULL && 
	    fprintf(f, " \"name\": \"%s\",", s->name) < 0)
		return 0;
	if (s->name == NULL && 
	    fputs(" \"name\": null,", f) == EOF)
		return 0;
	if (fprintf(f, " \"parent\": \"%s\",", s->parent->name) < 0)
		return 0;
	if (!gen_pos(f, &s->pos))
		return 0;
	if (!gen_doc(f, s->doc))
		return 0;
	if (!gen_rolemap(f, 1, s->rolemap))
		return 0;
	if (fprintf(f, 
	    " \"limit\": \"%" PRId64 "\","
	    " \"offset\": \"%" PRId64 "\", \"type\": \"%s\",", 
	    s->limit, s->offset, stypes[s->type]) < 0)
		return 0;
	if (fputs(" \"sntq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (!gen_sent(f, sent))
			return 0;
		if (TAILQ_NEXT(sent, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"ordq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(ord, &s->ordq, entries) {
		if (!gen_order(f, ord))
			return 0;
		if (TAILQ_NEXT(ord, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"aggr\": ", f) == EOF)
		return 0;
	if (!gen_aggr(f, s->aggr))
		return 0;
	if (fputs(" \"group\": ", f) == EOF)
		return 0;
	if (!gen_group(f, s->group))
		return 0;
	if (fputs(" \"dst\": ", f) == EOF)
		return 0;
	if (!gen_distinct(f, s->dst))
		return 0;
	return fputs(" }", f) != EOF;
}

/*
 * Emit { urefObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_uref(FILE *f, const struct uref *ref)
{

	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &ref->pos))
		return 0;
	return fprintf(f,
		" \"field\": \"%s\", \"op\": \"%s\","
		" \"mod\": \"%s\" }", ref->field->name,
		optypes[ref->op], modtypes[ref->mod]) > 0;
}

/*
 * Emit "name": { updateObj } w/o comma.
 * As an exception to the norm, start with a comma depending upon
 * "first".
 * This is to ease iteration through the queue.
 * Return zero on failure, non-zero on success.
 */
static int
gen_update(FILE *f, int *first, const struct update *u)
{
	const struct uref	*ref;

	if (*first == 0) {
		if (fputc(',', f) == EOF)
			return 0;
	} else
		*first = 0;

	if (u->name != NULL && fprintf(f, " \"%s\":", u->name) < 0)
		return 0;
	if (fputs(" {", f) == EOF)
		return 0;
	if (u->name != NULL && 
	    fprintf(f, " \"name\": \"%s\",", u->name) < 0)
		return 0;
	if (u->name == NULL && 
	    fputs(" \"name\": null,", f) == EOF)
		return 0;
	if (fprintf(f, " \"parent\": \"%s\",", u->parent->name) < 0)
		return 0;
	if (!gen_pos(f, &u->pos))
		return 0;
	if (!gen_doc(f, u->doc))
		return 0;
	if (fprintf(f, "\"type\": \"%s\"," ,
	    u->type == UP_MODIFY ? "update" : "delete") < 0)
		return 0;
	if (fputs(" \"mrq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(ref, &u->mrq, entries) {
		if (!gen_uref(f, ref))
			return 0;
		if (TAILQ_NEXT(ref, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"crq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(ref, &u->crq, entries) {
		if (!gen_uref(f, ref))
			return 0;
		if (TAILQ_NEXT(ref, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fprintf(f, "], \"flags\": [") < 0)
		return 0;
	if (u->flags & UPDATE_ALL)
		if (fputs(" \"all\"", f) == EOF)
			return 0;
	if (fputs(" ], ", f) == EOF)
		return 0;
	if (!gen_rolemap(f, 0, u->rolemap))
		return 0;
	return fputs(" }", f) != EOF;
}

static int
gen_unique(FILE *f, const struct unique *u)
{
	const struct nref	*ref;

	if (fputs(" {", f) == EOF)
		return 0;
	if (!gen_pos(f, &u->pos))
		return 0;
	if (fputs(" \"nq\": [", f) == EOF)
		return 0;
	TAILQ_FOREACH(ref, &u->nq, entries) {
		if (fprintf(f, " \"%s\"", ref->field->name) < 0)
			return 0;
		if (TAILQ_NEXT(ref, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs(" ] }", f) != EOF;
}

/*
 * Emit "name": { strctObj } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_strct(FILE *f, const struct strct *s)
{
	const struct field	*fd;
	const struct search	*sr;
	const struct update	*up;
	const struct unique	*un;
	const struct rolemap	*rm;
	int			 first;

	if (fprintf(f, " \"%s\": {", s->name) < 0)
		return 0;
	if (!gen_pos(f, &s->pos))
		return 0;
	if (!gen_doc(f, s->doc))
		return 0;
	if (fprintf(f, " \"name\": \"%s\", \"fq\": {", s->name) < 0)
		return 0;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (!gen_field(f, fd))
			return 0;
		if (TAILQ_NEXT(fd, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" }, \"insert\":", f) == EOF)
		return 0;
	if (!gen_insert(f, s->ins))
		return 0;
	if (fputs(" \"rq\": [ ", f) == EOF)
		return 0;
	TAILQ_FOREACH(rm, &s->rq, entries) {
		if (!gen_rolemap_full(f, rm))
			return 0;
		if (TAILQ_NEXT(rm, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"nq\": [ ", f) == EOF)
		return 0;
	TAILQ_FOREACH(un, &s->nq, entries) {
		if (!gen_unique(f, un))
			return 0;
		if (TAILQ_NEXT(un, entries) != NULL &&
		    fputc(',', f) == EOF)
			return 0;
	}
	if (fputs(" ], \"uq\": { \"named\": {", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(up, &s->uq, entries)
		if (up->name != NULL &&
		    !gen_update(f, &first, up))
			return 0;
	if (fputs(" }, \"anon\": [", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(up, &s->uq, entries)
		if (up->name == NULL &&
		    !gen_update(f, &first, up))
			return 0;
	if (fputs(" ] },", f) == EOF)
		return 0;

	if (fputs(" \"dq\": { \"named\": {", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(up, &s->dq, entries)
		if (up->name != NULL &&
		    !gen_update(f, &first, up))
			return 0;
	if (fputs(" }, \"anon\": [", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(up, &s->dq, entries)
		if (up->name == NULL &&
		    !gen_update(f, &first, up))
			return 0;
	if (fputs(" ] },", f) == EOF)
		return 0;

	if (fputs(" \"sq\": { \"named\": {", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(sr, &s->sq, entries)
		if (sr->name != NULL &&
		    !gen_search(f, &first, sr))
			return 0;
	if (fputs(" }, \"anon\": [", f) == EOF)
		return 0;
	first = 1;
	TAILQ_FOREACH(sr, &s->sq, entries)
		if (sr->name == NULL &&
		    !gen_search(f, &first, sr))
			return 0;
	return fputs(" ] } }", f) != EOF;
}

/*
 * Emit "sq": { ... } w/o comma.
 * Return zero on failure, non-zero on success.
 */
static int
gen_strcts(FILE *f, const struct strctq *q)
{
	const struct strct	*s;

	if (fputs(" \"sq\": {", f) == EOF)
		return 0;
	TAILQ_FOREACH(s, q, entries) {
		if (!gen_strct(f, s))
			return 0;
		if (TAILQ_NEXT(s, entries) != NULL && 
		    fputc(',', f) == EOF)
			return 0;
	}
	return fputs(" }", f) != EOF;
}

int
ort_lang_json(const struct ort_lang_json *args,
	const struct config *cfg, FILE *f)
{

	/* If we're a fragment, don't emit the surrounding braces. */

	if (!(args->flags & ORT_LANG_JSON_FRAGMENT) &&
	    fputs("{ ", f) == EOF)
		return 0;
	if (fputs("\"config\": {", f) == EOF)
		return 0;

	/*
	 * The general rule for all of these is that the writer for a
	 * token is responsible for left-padding spaces.  This way, we
	 * don't need to anticipate for spacing.  There is no pretty
	 * printing: this uses simple ' ' for separation.
	 */

	if (!gen_roles(f, cfg) ||
	    !gen_enms(f, cfg) ||
	    !gen_bitfs(f, cfg) ||
	    !gen_strcts(f, &cfg->sq))
		return 0;
	if (!(args->flags & ORT_LANG_JSON_FRAGMENT) &&
	    fputs(" }", f) == EOF)
		return 0;
	return fputs("}\n", f) != EOF;
}

