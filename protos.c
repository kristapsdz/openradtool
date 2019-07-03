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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"

static	const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"get", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const ftypes[FTYPE__MAX] = {
	"int64_t ", /* FTYPE_BIT */
	"time_t ", /* FTYPE_DATE */
	"time_t ", /* FTYPE_EPOCH */
	"int64_t ", /* FTYPE_INT */
	"double ", /* FTYPE_REAL */
	"const void *", /* FTYPE_BLOB */
	"const char *", /* FTYPE_TEXT */
	"const char *", /* FTYPE_PASSWORD */
	"const char *", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"int64_t ", /* FTYPE_BITFIELD */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	/* Unary types... */
	"isnull", /* OPTYE_ISNULL */
	"notnull", /* OPTYE_NOTNULL */
};

/*
 * Generate the convenience "open" function.
 * If "priv" is non-zero, return a ort instead of ksql.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_open(int priv, int decl)
{

	printf("struct %s *%sdb_open(const char *file)%s\n",
		priv ? "ort" : "ksql",
		decl ? "" : "\n", decl ? ";" : "");
}

void
print_func_db_role(int decl)
{

	printf("void%sdb_role(struct ort *ctx, enum ort_role r)%s\n",
		decl ? " " : "\n", 
		decl ? ";" : "");
}

void
print_func_db_role_current(int decl)
{

	printf("enum ort_role%sdb_role_current(struct ort *ctx)%s\n",
		decl ? " " : "\n", 
		decl ? ";" : "");
}

void
print_func_db_role_stored(int decl)
{

	printf("enum ort_role%sdb_role_stored(struct ort_store *s)%s\n",
		decl ? " " : "\n", 
		decl ? ";" : "");
}

void
print_func_db_trans_rollback(int priv, int decl)
{

	printf("void%sdb_trans_rollback(struct %s *p, size_t id)%s\n",
		decl ? " " : "\n", priv ? "ort" : "ksql",
		decl ? ";" : "");
}

void
print_func_db_trans_commit(int priv, int decl)
{

	printf("void%sdb_trans_commit(struct %s *p, size_t id)%s\n",
		decl ? " " : "\n", priv ? "ort" : "ksql",
		decl ? ";" : "");
}

void
print_func_db_trans_open(int priv, int decl)
{

	printf("void%sdb_trans_open(struct %s *p, size_t id, int mode)%s\n",
		decl ? " " : "\n", priv ? "ort" : "ksql",
		decl ? ";" : "");
}

/*
 * Generate the convenience "close" function.
 * If "priv" is non-zero, accept a ort instead of ksql.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_close(int priv, int decl)
{

	printf("void%sdb_close(struct %s *p)%s\n",
		decl ? " " : "\n", priv ? "ort" : "ksql",
		decl ? ";" : "");
}

/*
 * Print the variables in a function declaration, breaking the line at
 * 72 characters to indent 5 spaces.
 * The "col" is the current position in the output line.
 * Returns the current position in the output line.
 */
static size_t
print_var(size_t pos, size_t col, 
	const struct field *f, unsigned int flags)
{
	int	rc;

	putchar(',');
	col++;

	if (col >= 72)
		col = (rc = printf("\n     ")) > 0 ? rc : 0;
	else
		col += (rc = printf(" ")) > 0 ? rc : 0;

	if (FTYPE_ENUM == f->type) {
		rc = printf("enum %s %sv%zu", f->eref->ename,
			(flags & FIELD_NULL) ? "*" : "", pos);
		col += rc > 0 ? rc : 0;
		return col;
	}

	assert(NULL != ftypes[f->type]);

	if (FTYPE_BLOB == f->type) {
		rc = printf("size_t v%zu_sz, ", pos);
		col += rc > 0 ? rc : 0;
	}

	rc = printf("%s%sv%zu", ftypes[f->type], 
		(flags & FIELD_NULL) ?  "*" : "", pos);
	col += rc > 0 ? rc : 0;

	return col;
}

size_t
print_name_db_update(const struct update *u)
{
	const struct uref *ur;
	int	 col = 0;

	if (UP_MODIFY == u->type)
		col += printf("db_%s_update", u->parent->name);
	else
		col += printf("db_%s_delete", u->parent->name);

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

	return(col);
}

/*
 * Generate the "update" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a ort instead of ksql.
 */
void
print_func_db_update(const struct update *u, int priv, int decl)
{
	const struct uref *ur;
	size_t	 pos = 1;
	int	 col = 0;

	if (UP_MODIFY == u->type)
		col += printf("int%s", decl ? " " : "\n");
	else
		col += printf("int%s", decl ? " " : "\n");

	col += print_name_db_update(u);

	if (priv)
		col += printf("(struct ort *ctx");
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
 * Print just the name of a search function for "s".
 * Returns the number of characters printed.
 */
size_t
print_name_db_search(const struct search *s)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t		   sz = 0;
	int	 	   rc;

	rc = printf("db_%s_%s", s->parent->name, stypes[s->type]);
	sz += rc > 0 ? rc : 0;

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		sz += (rc = printf("_by")) > 0 ? rc : 0;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			TAILQ_FOREACH(sr, &sent->srq, entries) {
				rc = printf("_%s", sr->name);
				sz += rc > 0 ? rc : 0;
			}
			rc = printf("_%s", optypes[sent->op]);
			sz += rc > 0 ? rc : 0;
		}
	} else if (s->name != NULL)
		sz += (rc = printf("_%s", s->name)) > 0 ? rc : 0;

	return sz;
}

/*
 * Generate the declaration for a search function "s".
 * The format of the declaration depends upon the search type.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a ort instead of ksql.
 */
void
print_func_db_search(const struct search *s, int priv, int decl)
{
	const struct sent *sent;
	const struct sref *sr;
	const struct strct *retstr;
	size_t	 	    pos = 1, col = 0;
	int	 	    rc;

	/* 
	 * If we have a "distinct" clause, we use that to generate
	 * responses, not the structure itself.
	 */

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Start with return value. */

	if (s->type == STYPE_SEARCH)
		rc = printf("struct %s *", retstr->name);
	else if (s->type == STYPE_LIST)
		rc = printf("struct %s_q *", retstr->name);
	else if (s->type == STYPE_ITERATE)
		rc = printf("void");
	else
		rc = printf("uint64_t");

	col += rc > 0 ? rc : 0;
	if (decl) {
		printf("\n");
		col = 0;
	} else
		col += (rc = printf(" ")) > 0 ? rc : 0;

	/* Now function name. */

	if ((col += print_name_db_search(s)) >= 72) {
		puts("");
		col = (rc = printf("    ") > 0) ? rc : 0;
	}

	/* Arguments starting with database pointer. */

	if (priv)
		col += (rc = printf("(struct ort *ctx")) > 0 ? rc : 0;
	else
		col += (rc = printf("(struct ksql *db")) > 0 ? rc : 0;

	if (s->type == STYPE_ITERATE) {
		rc = printf(", %s_cb cb, void *arg", retstr->name);
		col += rc > 0 ? rc : 0;
	}

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (!OPTYPE_ISUNARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			col = print_var(pos++, col, sr->field, 0);
		}

	printf(")%s", decl ? ";\n" : "");
}

/*
 * Print just the name of a insert function for "p".
 * Returns the number of characters printed.
 */
size_t
print_name_db_insert(const struct strct *p)
{
	int	 rc;

	return (rc = printf("db_%s_insert", p->name)) > 0 ? rc : 0;
}

/*
 * Generate the "insert" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is non-zero, accept a ort instead of ksql.
 */
void
print_func_db_insert(const struct strct *p, int priv, int decl)
{
	const struct field *f;
	size_t	 	    pos = 1, col = 0;
	int		    rc;

	/* Start with return value. */

	if (decl)
		printf("int64_t\n");
	else
		col += (rc = printf("int64_t ")) > 0 ? rc : 0;

	/* Now function name. */

	if ((col += print_name_db_insert(p)) >= 72) {
		puts("");
		col = (rc = printf("    ") > 0) ? rc : 0;
	}

	/* Arguments starting with database pointer. */

	if (priv)
		col += (rc = printf("(struct ort *ctx")) > 0 ? rc : 0;
	else
		col += (rc = printf("(struct ksql *db")) > 0 ? rc : 0;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (!(f->type == FTYPE_STRUCT || 
		      (f->flags & FIELD_ROWID)))
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
 * Generate the "unfill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 */
void
print_func_db_unfill(const struct strct *p, int priv, int decl)
{

	if (priv && decl)
		return;
	printf("%svoid%sdb_%s_unfill(struct %s *p)%s",
	       priv ? "static " : "",
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
 * Generate the "fill" function for a given structure.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line.
 * If "priv" is declared, then we're running in roles mode and should
 * only produce the prototype for the definition.
 */
void
print_func_db_fill(const struct strct *p, int priv, int decl)
{

	if (priv && decl)
		return;
	printf("%svoid%sdb_%s_fill(%sstruct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)%s",
	       priv ? "static " : "",
	       decl ? " " : "\n",
	       p->name, 
	       priv ? "struct ort *ctx, " : "",
	       p->name,
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
 * Function freeing value used during JSON parse.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_clear(const struct strct *p, int decl)
{

	printf("void%sjsmn_%s_clear(struct %s *p)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n");
}

/*
 * Function freeing array returned from JSON parse.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_free_array(const struct strct *p, int decl)
{

	printf("void%sjsmn_%s_free_array(struct %s *p, size_t sz)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n");
}

/*
 * JSON parsing routine for an array of structures w/o allocation.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_parse_array(const struct strct *p, int decl)
{

	printf("int%sjsmn_%s_array(struct %s **p, size_t *sz, "
		"const char *buf, const jsmntok_t *t, size_t toksz)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n");
}

/*
 * JSON parsing routine for a given structure w/o allocation.
 * If this is NOT a declaration ("decl"), then print a newline after the
 * return type; otherwise, have it on one line followed by a newline.
 */
void
print_func_json_parse(const struct strct *p, int decl)
{

	printf("int%sjsmn_%s(struct %s *p, "
		"const char *buf, const jsmntok_t *t, size_t toksz)%s",
		decl ? " " : "\n", p->name, p->name, 
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

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 */
void
print_define_schema(const struct strct *p)
{
	const struct field *f;
	const char *s = "";

	printf("#define DB_SCHEMA_%s(_x) \\", p->cname);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		puts(s);
		printf("\t#_x \".%s\"", f->name);
		s = " \",\" \\";
	}
	puts("");
}
