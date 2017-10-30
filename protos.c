/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/queue.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"int64_t ", /* FTYPE_BIT */
	"time_t ", /* FTYPE_EPOCH */
	"int64_t ", /* FTYPE_INT */
	"double ", /* FTYPE_REAL */
	"const void *", /* FTYPE_BLOB */
	"const char *", /* FTYPE_TEXT */
	"const char *", /* FTYPE_PASSWORD */
	"const char *", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYE_NEQUAL */
	/* Unary types... */
	"isnull", /* OPTYE_ISNULL */
	"notnull", /* OPTYE_NOTNULL */
};

/*
 * Generate the convenience "open" function.
 * If "priv" is non-zero, return a kwbp instead of ksql.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_open(int priv, int decl)
{

	printf("struct %s *%sdb_open(const char *file)%s\n",
		priv ? "kwbp" : "ksql",
		decl ? "" : "\n", decl ? ";" : "");
}

void
print_func_db_role(int decl)
{

	printf("void%sdb_role(struct kwbp *ctx, enum kwbp_role r)%s\n",
		decl ? " " : "\n", 
		decl ? ";" : "");
}

/*
 * Generate the convenience "close" function.
 * If "priv" is non-zero, accept a kwbp instead of ksql.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_close(int priv, int decl)
{

	printf("void%sdb_close(struct %s *p)%s\n",
		decl ? " " : "\n", priv ? "kwbp" : "ksql",
		decl ? ";" : "");
}

/*
 * Print the variables in a function declaration.
 * The "col" is the current position in the output line.
 * Returns the current position in the output line.
 */
static int
print_var(size_t pos, size_t col, 
	const struct field *f, unsigned int flags)
{

	putchar(',');
	if (col >= 72) {
		printf("\n\t");
		col = 0;
	} else {
		putchar(' ');
		col++;
	}

	/* Handle enumeration first. */

	if (FTYPE_ENUM == f->type) {
		col += printf("enum %s %sv%zu", f->eref->ename,
			FIELD_NULL & flags ? "*" : "", pos);
		return(col);
	}

	/* 
	 * We don't have structures, so assert.
	 * Then print blob size followed by data.
	 */

	assert(NULL != ftypes[f->type]);

	if (FTYPE_BLOB == f->type)
		col += printf("size_t v%zu_sz, ", pos);
	col += printf("%s%sv%zu", ftypes[f->type], 
		FIELD_NULL & flags ?  "*" : "", pos);

	return(col);
}

/*
 * Generate the "update" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a kwbp instead of ksql.
 */
void
print_func_db_update(const struct update *u, int priv, int decl)
{
	const struct uref *ur;
	size_t	 pos = 1;
	int	 col = 0;

	if (UP_MODIFY == u->type)
		col += printf("int%sdb_%s_update",
			decl ? " " : "\n", u->parent->name);
	else
		col += printf("int%sdb_%s_delete",
			decl ? " " : "\n", u->parent->name);

	if (NULL == u->name && UP_MODIFY == u->type) {
		if ( ! (UPDATE_ALL & u->flags))
			TAILQ_FOREACH(ur, &u->mrq, entries)
				col += printf("_%s", ur->name);
		col += printf("_by");
		TAILQ_FOREACH(ur, &u->crq, entries)
			col += printf("_%s_%s", 
				ur->name, optypes[ur->op]);
	} else if (NULL == u->name) {
		col += printf("_by");
		TAILQ_FOREACH(ur, &u->crq, entries)
			col += printf("_%s_%s", 
				ur->name, optypes[ur->op]);
	} else 
		col += printf("_%s", u->name);

	if (priv)
		col += printf("(struct kwbp *ctx");
	else
		col += printf("(struct ksql *db");

	TAILQ_FOREACH(ur, &u->mrq, entries)
		col = print_var(pos++, col, 
			ur->field, ur->field->flags);

	/* Don't accept input for unary operation. */

	TAILQ_FOREACH(ur, &u->crq, entries)
		if ( ! OPTYPE_ISUNARY(ur->op))
			col = print_var(pos++, col, ur->field, 0);

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the declaration for a search function "s".
 * The format of the declaration depends upon the search type.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a kwbp instead of ksql.
 * FIXME: line wrapping.
 */
void
print_func_db_search(const struct search *s, int priv, int decl)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos = 1;
	int	 col = 0;

	if (STYPE_SEARCH == s->type)
		col += printf("struct %s *%sdb_%s_get", 
			s->parent->name, decl ? "" : "\n", 
			s->parent->name);
	else if (STYPE_LIST == s->type)
		col += printf("struct %s_q *%sdb_%s_list", 
			s->parent->name, decl ? "" : "\n", 
			s->parent->name);
	else
		col += printf("void%sdb_%s_iterate",
			decl ? " " : "\n", s->parent->name);

	if (NULL == s->name && ! TAILQ_EMPTY(&s->sntq)) {
		col += printf("_by");
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			TAILQ_FOREACH(sr, &sent->srq, entries)
				col += printf("_%s", sr->name);
			col += printf("_%s", optypes[sent->op]);
		}
	} else if (NULL != s->name)
		col += printf("_%s", s->name);

	if (priv)
		col += printf("(struct kwbp *ctx");
	else
		col += printf("(struct ksql *db");

	if (STYPE_ITERATE == s->type)
		col += printf(", %s_cb cb, void *arg", 
			s->parent->name);

	/* Don't accept input for unary operation. */

	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		col = print_var(pos++, col, sr->field, 0);
	}

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the "insert" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a kwbp instead of ksql.
 */
void
print_func_db_insert(const struct strct *p, int priv, int decl)
{
	const struct field *f;
	size_t	 pos = 1;
	int	 col = 0;

	col += printf("int64_t%sdb_%s_insert(", 
		decl ? " " : "\n", p->name);

	if (priv)
		col += printf("struct kwbp *ctx");
	else
		col += printf("struct ksql *db");

	TAILQ_FOREACH(f, &p->fq, entries)
		if ( ! (FTYPE_STRUCT == f->type ||
		        FIELD_ROWID & f->flags))
			col = print_var(pos++, col, f, f->flags);

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Generate the "freeq" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_freeq(const struct strct *p, int decl)
{

	assert(STRCT_HAS_QUEUE & p->flags);
	printf("void%sdb_%s_freeq(struct %s_q *q)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "free" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_free(const struct strct *p, int decl)
{

	printf("void%sdb_%s_free(struct %s *p)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "unfill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_unfill(const struct strct *p, int decl)
{

	printf("void%sdb_%s_unfill(struct %s *p)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "");
}

/*
 * Generate the "fill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_fill(const struct strct *p, int decl)
{

	printf("void%sdb_%s_fill(struct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)%s",
	       decl ? " " : "\n",
	       p->name, p->name,
	       decl ? ";\n" : "");
}

void
print_func_valid(const struct field *p, int decl)
{

	printf("int%svalid_%s_%s(struct kpair *p)%s",
		decl ? " " : "\n", 
		p->parent->name, p->name,
		decl ? ";\n" : "\n");
}

/*
 * Generate the JSON internal data function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_data(const struct strct *p, int decl)
{

	printf("void%sjson_%s_data(struct kjsonreq *r, "
		"const struct %s *p)%s",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";\n" : "");
}

/*
 * Generate the JSON array function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_array(const struct strct *p, int decl)
{

	printf("void%sjson_%s_array(struct kjsonreq *r, "
		"const struct %s_q *q)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "");
}

/*
 * Generate the JSON object function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_obj(const struct strct *p, int decl)
{

	printf("void%sjson_%s_obj(struct kjsonreq *r, "
		"const struct %s *p)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "");
}

/*
 * Create the iterator function for JSON.
 * This is meant to be called by an "iterator" function with the
 * kjsonreq set to be the private data.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_iterate(const struct strct *p, int decl)
{

	printf("void%sjson_%s_iterate(const struct %s *p, "
	        "void *arg)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "");
}
