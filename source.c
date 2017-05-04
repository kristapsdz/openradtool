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
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * SQL operators.
 * Some of these binary, some of these are unary.
 * Use the OPTYPE_ISUNARY or OPTYPE_ISBINARY macro to determine where
 * within the expressions this should sit.
 */
static	const char *const optypes[OPTYPE__MAX] = {
	"=", /* OPTYPE_EQUAL */
	"!=", /* OPTYPE_NEQUAL */
	"ISNULL", /* OPTYPE_ISNULL */
	"NOTNULL", /* OPTYPE_NOTNULL */
};

/*
 * Functions extracting from a statement.
 * Note that FTYPE_TEXT and FTYPE_PASSWORD need a surrounding strdup.
 */
static	const char *const coltypes[FTYPE__MAX] = {
	"ksql_stmt_int", /* FTYPE_INT */
	"ksql_stmt_double", /* FTYPE_REAL */
	"ksql_stmt_blob", /* FTYPE_BLOB (XXX: is special) */
	"ksql_stmt_str", /* FTYPE_TEXT */
	"ksql_stmt_str", /* FTYPE_PASSWORD */
	NULL, /* FTYPE_STRUCT */
};

static	const char *const puttypes[FTYPE__MAX] = {
	"kjson_putintp", /* FTYPE_INT */
	"kjson_putdoublep", /* FTYPE_REAL */
	"kjson_putstringp", /* FTYPE_BLOB (XXX: is special) */
	"kjson_putstringp", /* FTYPE_TEXT */
	NULL, /* FTYPE_PASSWORD (don't print) */
	NULL, /* FTYPE_STRUCT */
};

/*
 * Functions binding an argument to a statement.
 */
static	const char *const bindtypes[FTYPE__MAX] = {
	"ksql_bind_int", /* FTYPE_INT */
	"ksql_bind_double", /* FTYPE_REAL */
	"ksql_bind_blob", /* FTYPE_BLOB (XXX: is special) */
	"ksql_bind_str", /* FTYPE_TEXT */
	"ksql_bind_str", /* FTYPE_PASSWORD */
	NULL, /* FTYPE_STRUCT */
};

/*
 * Fill an individual field from the database.
 */
static void
gen_strct_fill_field(const struct field *f)
{

	if (FTYPE_STRUCT == f->type)
		return;

	if (FIELD_NULL & f->flags)
		printf("\tp->has_%s = ! "
			"ksql_stmt_isnull(stmt, *pos);\n",
			f->name);

	/*
	 * Blob types need to have space allocated (and the space
	 * variable set) before we extract from the database.
	 * This sequence is very different from the other types, so make
	 * it into its own conditional block for clarity.
	 */

	if (FTYPE_BLOB == f->type) {
		/* 
		 * First, prepare for the data.
		 * We get the data size, then allocate space enough to
		 * fill with that blob.
		 * Error-check the allocation, of course.
		 */

		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s) {\n"
			       "\t\tp->%s_sz = ksql_stmt_bytes"
			        "(stmt, *pos);\n"
			       "\t\tp->%s = malloc(p->%s_sz);\n"
			       "\t\tif (NULL == p->%s) {\n"
			       "\t\t\tperror(NULL);\n"
			       "\t\t\texit(EXIT_FAILURE);\n"
			       "\t\t}\n" 
			       "\t}\n", f->name, f->name, 
			       f->name, f->name, f->name);
		else 
			printf("\tp->%s_sz = ksql_stmt_bytes"
				"(stmt, *pos);\n"
			       "\tp->%s = malloc(p->%s_sz);\n"
			       "\tif (NULL == p->%s) {\n"
			       "\t\tperror(NULL);\n"
			       "\t\texit(EXIT_FAILURE);\n"
			       "\t}\n", f->name,
			       f->name, f->name, f->name);

		/* Next, copy the data from the database. */

		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s)\n"
			       "\t\tmemcpy(p->%s, "
			        "%s(stmt, (*pos)++), p->%s_sz);\n"
			       "\telse\n"
			       "\t\t(*pos)++;\n", f->name, 
			       f->name, coltypes[f->type], f->name);
		else 
			printf("\tmemcpy(p->%s, "
			        "%s(stmt, (*pos)++), p->%s_sz);\n",
			       f->name, coltypes[f->type], f->name);
	} else {
		/*
		 * Assign from the database.
		 * Text fields use a strdup for the allocation.
		 */

		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s)\n"
			       "\t\tp->%s = ", f->name, f->name);
		else 
			printf("\tp->%s = ", f->name);

		if (FTYPE_TEXT == f->type || 
		    FTYPE_PASSWORD == f->type)
			printf("strdup(%s(stmt, (*pos)++));\n", 
				coltypes[f->type]);
		else
			printf("%s(stmt, (*pos)++);\n", 
				coltypes[f->type]);

		/* NULL check for allocation. */

		if (FIELD_NULL & f->flags) {
			puts("\telse\n"
			     "\t\t(*pos)++;");
			if (FTYPE_TEXT == f->type || 
			    FTYPE_PASSWORD == f->type)
				printf("\tif (p->has_%s && "
					"NULL == p->%s) {\n"
				       "\t\tperror(NULL);\n"
				       "\t\texit(EXIT_FAILURE);\n"
				       "\t}\n", f->name, f->name);
		} else if (FTYPE_TEXT == f->type || 
			   FTYPE_PASSWORD == f->type) 
			printf("\tif (NULL == p->%s) {\n"
			       "\t\tperror(NULL);\n"
			       "\t\texit(EXIT_FAILURE);\n"
			       "\t}\n", f->name);
	}
}

/*
 * Generate the binding for a field of type "t" at field "pos".
 * Set "ptr" to be non-zero if this is passed in as a pointer.
 */
static void
gen_bindfunc(enum ftype t, size_t pos, int ptr)
{

	assert(FTYPE_STRUCT != t);
	if (FTYPE_BLOB == t)
		printf("\t%s(stmt, %zu, %sv%zu, v%zu_sz);\n",
			bindtypes[t], pos - 1, 
			ptr ? "*" : "", pos, pos);
	else if (FTYPE_PASSWORD != t)
		printf("\t%s(stmt, %zu, %sv%zu);\n", 
			bindtypes[t], pos - 1, 
			ptr ? "*" : "", pos);
}

/*
 * Print out a search function for an STYPE_ITERATE.
 * This calls a function pointer with the retrieved data.
 */
static void
gen_strct_func_iter(const struct search *s, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_ITERATE == s->type);

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s p;\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, s->parent->cname, num, 
	       s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tdb_%s_fill_r(&p, stmt, NULL);\n",
	       s->parent->name);

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, don't run the callback.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (crypt_checkpass(v%zu, p.%s) < 0) {\n"
		       "\t\t\tdb_%s_unfill_r(&p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	printf("\t\t(*cb)(&p, arg);\n"
	       "\t\tdb_%s_unfill_r(&p);\n"
	       "\t}\n"
	       "\tksql_stmt_free(stmt);\n"
	       "}\n"
	       "\n",
	       s->parent->name);
}

/*
 * Print out a search function for an STYPE_LIST.
 * This searches for a multiplicity of values.
 */
static void
gen_strct_func_list(const struct search *s, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_LIST == s->type);

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s_q *q;\n"
	       "\tstruct %s *p;\n"
	       "\n"
	       "\tq = malloc(sizeof(struct %s_q));\n"
	       "\tif (NULL == q) {\n"
	       "\t\tperror(NULL);\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\t}\n"
	       "\tTAILQ_INIT(q);\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, s->parent->name, 
	       s->parent->name, s->parent->cname, num, 
	       s->parent->cname, num);

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, don't insert into the tailq.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(p, stmt, NULL);\n",
	       s->parent->name, s->parent->name);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (crypt_checkpass(v%zu, p->%s) < 0) {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	puts("\t\tTAILQ_INSERT_TAIL(q, p, _entries);\n"
	     "\t}\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(q);\n"
	     "}\n"
	     "");
}

static void
gen_func_open(void)
{

	print_func_db_open(0);
	puts("{\n"
	     "\tstruct ksqlcfg cfg;\n"
	     "\tstruct ksql *sql;\n"
	     "\n"
	     "\tmemset(&cfg, 0, sizeof(struct ksqlcfg));\n"
	     "\tcfg.flags = KSQL_EXIT_ON_ERR |\n"
	     "\t\tKSQL_FOREIGN_KEYS | KSQL_SAFE_EXIT;\n"
	     "\tcfg.err = ksqlitemsg;\n"
	     "\tcfg.dberr = ksqlitedbmsg;\n"
	     "\n"
	     "\tif (NULL == (sql = ksql_alloc(&cfg)))\n"
	     "\t\treturn(NULL);\n"
	     "\tksql_open(sql, file);\n"
	     "\treturn(sql);\n"
	     "}\n"
	     "");
}

static void
gen_func_close(void)
{

	print_func_db_close(0);
	puts("{\n"
	     "\tif (NULL == p)\n"
	     "\t\treturn;\n"
	     "\tksql_free(p);\n"
	     "}\n"
	     "");
}

/*
 * Print out a search function for an STYPE_SEARCH.
 * This searches for a singular value.
 */
static void
gen_strct_func_srch(const struct search *s, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_SEARCH == s->type);

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s *p = NULL;\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, s->parent->cname, num, 
	       s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(p, stmt, NULL);\n",
	       s->parent->name, s->parent->name);

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, nullify.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (NULL != p && "
			"crypt_checkpass(v%zu, p->%s) < 0) {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tp = NULL;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	puts("\t}\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(p);\n"
	     "}\n"
	     "");
}

/*
 * Generate the "freeq" function.
 * This must have STRCT_HAS_QUEUE defined in its flags, otherwise the
 * function does nothing.
 */
static void
gen_func_freeq(const struct strct *p)
{

	if ( ! (STRCT_HAS_QUEUE & p->flags))
		return;

	print_func_db_freeq(p, 0);
	printf("\n"
	       "{\n"
	       "\tstruct %s *p;\n\n"
	       "\tif (NULL == q)\n"
	       "\t\treturn;\n"
	       "\twhile (NULL != (p = TAILQ_FIRST(q))) {\n"
	       "\t\tTAILQ_REMOVE(q, p, _entries);\n"
	       "\t\tdb_%s_free(p);\n"
	       "\t}\n\n"
	       "\tfree(q);\n"
	       "}\n"
	       "\n", 
	       p->name, p->name);
}

/*
 * Generate the "insert" function.
 */
static void
gen_func_insert(const struct strct *p)
{
	const struct field *f;
	size_t	 pos, npos;

	print_func_db_insert(p, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tint64_t id = -1;\n");

	/* We need temporary space for hash generation. */

	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_PASSWORD == f->type)
			printf("\tchar hash%zu[64];\n", pos++);

	/* Actually generate hashes, if necessary. */

	puts("");
	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_PASSWORD == f->type) {
			if (FIELD_NULL & f->flags)
				printf("\tif (NULL != v%zu)\n"
				       "\t", npos);
			printf("\tcrypt_newhash(%sv%zu, "
				"\"blowfish,a\", hash%zu, "
				"sizeof(hash%zu));\n",
				FIELD_NULL & f->flags ? "*" : "",
				npos, pos, pos);
			pos++;
		} 
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		npos++;
	}
	if (pos > 1)
		puts("");

	printf("\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_INSERT],\n"
	       "\t\tSTMT_%s_INSERT);\n",
	       p->cname, p->cname);

	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		assert(FTYPE_STRUCT != f->type);
		if (FIELD_NULL & f->flags)
			printf("\tif (NULL == v%zu)\n"
			       "\t\tksql_bind_null(stmt, %zu);\n"
			       "\telse\n"
			       "\t", npos, npos - 1);
		if (FTYPE_PASSWORD == f->type) {
			printf("\t%s(stmt, %zu, hash%zu);\n",
				bindtypes[f->type],
				npos - 1, pos);
			npos++;
			pos++;
		} else
			gen_bindfunc(f->type, 
				npos++, FIELD_NULL & f->flags);
	}
	puts("\tif (KSQL_DONE == ksql_stmt_cstep(stmt))\n"
	     "\t\tksql_lastid(db, &id);\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(id);\n"
	     "}\n"
	     "");
}

/*
 * Generate the "free" function.
 */
static void
gen_func_free(const struct strct *p)
{

	print_func_db_free(p, 0);
	printf("\n"
	       "{\n"
	       "\tdb_%s_unfill_r(p);\n"
	       "\tfree(p);\n"
	       "}\n"
	       "\n", 
	       p->name);
}

/*
 * Generate the "unfill" function.
 */
static void
gen_func_unfill(const struct strct *p)
{
	const struct field *f;

	print_func_db_unfill(p, 0);
	puts("\n"
	     "{\n"
	     "\tif (NULL == p)\n"
	     "\t\treturn;");
	TAILQ_FOREACH(f, &p->fq, entries)
		switch(f->type) {
		case (FTYPE_BLOB):
		case (FTYPE_PASSWORD):
		case (FTYPE_TEXT):
			printf("\tfree(p->%s);\n", f->name);
			break;
		default:
			break;
		}
	puts("}\n"
	     "");
}

/*
 * Generate the nested "unfill" function.
 */
static void
gen_func_unfill_r(const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_unfill_r(struct %s *p)\n"
	       "{\n"
	       "\tif (NULL == p)\n"
	       "\t\treturn;\n"
	       "\n"
	       "\tdb_%s_unfill(p);\n",
	       p->name, p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			printf("\tdb_%s_unfill_r(&p->%s);\n",
				f->ref->tstrct, f->name);
	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill_r(const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_fill_r(struct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)\n"
	       "{\n"
	       "\tsize_t i = 0;\n"
	       "\n"
	       "\tif (NULL == pos)\n"
	       "\t\tpos = &i;\n"
	       "\tdb_%s_fill(p, stmt, pos);\n",
	       p->name, p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			printf("\tdb_%s_fill_r(&p->%s, "
				"stmt, pos);\n", 
				f->ref->tstrct, f->name);
	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill(const struct strct *p)
{
	const struct field *f;

	print_func_db_fill(p, 0);
	puts("\n"
	     "{\n"
	     "\tsize_t i = 0;\n"
	     "\n"
	     "\tif (NULL == pos)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
	puts("}\n"
	     "");
}

/*
 * Generate an update or delete function.
 */
static void
gen_func_update(const struct update *up, size_t num)
{
	const struct uref *ref;
	size_t	 pos, npos;

	print_func_db_update(up, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tenum ksqlc c;\n");

	/* Create hash buffer for modifying hashes. */

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries)
		if (FTYPE_PASSWORD == ref->field->type)
			printf("\tchar hash%zu[64];\n", pos++);
	puts("");

	/* Create hash from password. */

	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (FTYPE_PASSWORD == ref->field->type) {
			if (FIELD_NULL & ref->field->flags)
				printf("if (NULL != v%zu)\n"
				       "\t", npos);
			printf("\tcrypt_newhash(v%zu, "
				"\"blowfish,a\", hash%zu, "
				"sizeof(hash%zu));\n",
				npos, pos, pos);
			pos++;
		} 
		npos++;
	}

	if (pos > 1)
		puts("");

	if (UP_MODIFY == up->type)
		printf("\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_UPDATE_%zu],\n"
		       "\t\tSTMT_%s_UPDATE_%zu);\n",
		       up->parent->cname, num, 
		       up->parent->cname, num);
	else
		printf("\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_DELETE_%zu],\n"
		       "\t\tSTMT_%s_DELETE_%zu);\n",
		       up->parent->cname, num, 
		       up->parent->cname, num);

	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		assert(FTYPE_STRUCT != ref->field->type);
		if (FIELD_NULL & ref->field->flags)
			printf("\tif (NULL == v%zu)\n"
			       "\t\tksql_bind_null(stmt, %zu);\n"
			       "\telse\n"
			       "\t", npos, npos - 1);
		if (FTYPE_PASSWORD == ref->field->type)
			printf("\t%s(stmt, %zu, hash%zu);\n",
			       bindtypes[ref->field->type],
			       npos - 1, pos++);
		else
			gen_bindfunc(ref->field->type, npos,
				FIELD_NULL & ref->field->flags);
		npos++;
	}
	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(FTYPE_STRUCT != ref->field->type);
		assert(FTYPE_PASSWORD != ref->field->type);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		printf("\t%s(stmt, %zu, v%zu);\n", 
			bindtypes[ref->field->type],
			npos - 1, npos);
		npos++;
	}
	puts("\tc = ksql_stmt_cstep(stmt);\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(KSQL_CONSTRAINT != c);\n"
	     "}\n"
	     "");
}

static void
gen_func_json_obj(const struct strct *p)
{

	print_func_json_obj(p, 0);
	puts("\n"
	     "{");
	printf("\tkjson_objp_open(r, \"%s\");\n"
	       "\tjson_%s_data(r, p);\n", p->name, p->name);
	puts("\tkjson_obj_close(r);\n"
	     "}\n"
	     "");
}

static void
gen_func_json_data(const struct strct *p)
{
	const struct field *f;
	size_t	 pos;

	print_func_json_data(p, 0);
	puts("\n"
	     "{");

	/* Declare our base64 buffers. */

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_BLOB == f->type) 
			printf("\tchar *buf%zu;\n", ++pos);

	if (pos > 0)
		puts("\tsize_t sz;\n");

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_BLOB != f->type) 
			continue;
		pos++;
		printf("\tsz = (p->%s_sz + 2) / 3 * 4 + 1;\n"
		       "\tbuf%zu = malloc(sz);\n"
		       "\tif (NULL == buf%zu) {\n"
		       "\t\tperror(NULL);\n"
		       "\t\texit(EXIT_FAILURE);\n"
		       "\t}\n", f->name, pos, pos);
		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s)\n"
			       "\t", f->name);
		printf("\tb64_ntop(p->%s, p->%s_sz, buf%zu, sz);\n",
		       f->name, f->name, pos);
	}

	if (pos > 0)
		puts("");

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_NULL & f->flags)
			printf("\tif ( ! p->has_%s)\n"
			       "\t\tkjson_putnullp(r, \"%s\");\n"
			       "\telse\n"
			       "\t",
			       f->name, f->name);
		if (FTYPE_BLOB == f->type)
			printf("\t%s(r, \"%s\", buf%zu);\n",
				puttypes[f->type], f->name, ++pos);
		else if (FTYPE_STRUCT == f->type)
			printf("\tjson_%s_obj(r, &p->%s);\n",
				f->ref->tstrct, f->name);
		else if (NULL != puttypes[f->type])
			printf("\t%s(r, \"%s\", p->%s);\n", 
				puttypes[f->type], f->name, f->name);
	}

	/* Free our temporary base64 buffers. */

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_BLOB == f->type && 0 == pos)
			puts("");
		if (FTYPE_BLOB == f->type) 
			printf("\tfree(buf%zu);\n", ++pos);
	}

	puts("}\n"
	     "");
}

/*
 * Generate all of the functions we've defined in our header for the
 * given structure "s".
 */
static void
gen_funcs(const struct strct *p, int json)
{
	const struct search *s;
	const struct update *u;
	size_t	 pos;

	gen_func_fill_r(p);
	gen_func_fill(p);
	gen_func_unfill_r(p);
	gen_func_unfill(p);
	gen_func_free(p);
	gen_func_freeq(p);
	gen_func_insert(p);

	if (json) {
		gen_func_json_data(p);
		gen_func_json_obj(p);
	}

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_SEARCH == s->type)
			gen_strct_func_srch(s, pos++);
		else if (STYPE_LIST == s->type)
			gen_strct_func_list(s, pos++);
		else
			gen_strct_func_iter(s, pos++);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_func_update(u, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_func_update(u, pos++);
}

/*
 * Generate a set of statements that will be used for this structure.
 */
static void
gen_enum(const struct strct *p)
{
	const struct search *s;
	const struct update *u;
	size_t	 pos;

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		printf("\tSTMT_%s_BY_SEARCH_%zu,\n", p->cname, pos++);

	printf("\tSTMT_%s_INSERT,\n", p->cname);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		printf("\tSTMT_%s_UPDATE_%zu,\n", p->cname, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		printf("\tSTMT_%s_DELETE_%zu,\n", p->cname, pos++);
}

/*
 * Recursively generate a series of DB_SCHEMA_xxx statements for getting
 * data on a structure.
 * This will specify the schema of the top-level structure (pname is
 * NULL), then generate aliased schemas for all of the recursive
 * structures.
 * See gen_stmt_joins().
 */
static void
gen_stmt_schema(const struct strct *orig, 
	const struct strct *p, const char *pname)
{
	const struct field *f;
	const struct alias *a = NULL;
	int	 c;
	char	*name = NULL;

	/* 
	 * If applicable, looks up our alias and emit it as the alias
	 * for the table.
	 * Otherwise, use the table name itself.
	 */

	if (NULL != pname) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (0 == strcasecmp(a->name, pname))
				break;
		assert(NULL != a);
		printf("\",\" DB_SCHEMA_%s(%s) ", p->cname, a->alias);
	} else
		printf("\" DB_SCHEMA_%s(%s) ", p->cname, p->name);

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;

		if (NULL != pname) {
			c = asprintf(&name, 
				"%s.%s", pname, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if (NULL == (name = strdup(f->name)))
			err(EXIT_FAILURE, NULL);

		gen_stmt_schema(orig, 
			f->ref->target->parent, name);
		free(name);
	}
}

/*
 * Recursively generate a series of INNER JOIN statements for any
 * structure object.
 * If the structure object has no inner nested components, this will not
 * do anything.
 * See gen_stmt_schema().
 */
static void
gen_stmt_joins(const struct strct *orig, 
	const struct strct *p, const struct alias *parent)
{
	const struct field *f;
	const struct alias *a;
	int	 c;
	char	*name;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;

		if (NULL != parent) {
			c = asprintf(&name, "%s.%s", 
				parent->name, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if (NULL == (name = strdup(f->name)))
			err(EXIT_FAILURE, NULL);

		TAILQ_FOREACH(a, &orig->aq, entries)
			if (0 == strcasecmp(a->name, name))
				break;
		assert(NULL != a);

		printf(" INNER JOIN %s AS %s ON %s.%s=%s.%s",
			f->ref->tstrct, a->alias,
			a->alias, f->ref->tfield,
			NULL == parent ? p->name : parent->alias,
			f->ref->sfield);
		gen_stmt_joins(orig, 
			f->ref->target->parent, 
			a);
		free(name);
	}
}

/*
 * Fill in the statements noted in gen_enum().
 */
static void
gen_stmt(const struct strct *p)
{
	const struct search *s;
	const struct sent *sent;
	const struct sref *sr;
	const struct field *f;
	const struct update *up;
	const struct uref *ur;
	int	 first;
	size_t	 pos;

	/* 
	 * Print custom search queries.
	 * This also uses the recursive selection.
	 */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		printf("\t/* STMT_%s_BY_SEARCH_%zu */\n"
		       "\t\"SELECT ",
			p->cname, pos++);
		gen_stmt_schema(p, p, NULL);
		printf("\" FROM %s", p->name);
		gen_stmt_joins(p, p, NULL);
		printf(" WHERE");
		first = 1;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			if (FTYPE_PASSWORD == sr->field->type)
				continue;
			if ( ! first)
				printf(" AND");
			first = 0;
			if (OPTYPE_ISUNARY(sent->op))
				printf(" %s.%s %s",
					NULL == sent->alias ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
			else
				printf(" %s.%s %s ?", 
					NULL == sent->alias ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
		}
		puts("\",");
	}

	/* 
	 * Insertion of a new record.
	 * TODO: DEFAULT_VALUES.
	 */

	printf("\t/* STMT_%s_INSERT */\n"
	       "\t\"INSERT INTO %s (", p->cname, p->name);
	first = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		printf("%s%s", first ? "" : ",", f->name);
		first = 0;
	}
	printf(") VALUES (");
	first = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		printf("%s?", first ? "" : ",");
		first = 0;
	}
	puts(")\",");
	
	/* Custom update queries. */

	pos = 0;
	TAILQ_FOREACH(up, &p->uq, entries) {
		printf("\t/* STMT_%s_UPDATE_%zu */\n"
		       "\t\"UPDATE %s SET",
		       p->cname, pos++, p->name);
		first = 1;
		TAILQ_FOREACH(ur, &up->mrq, entries) {
			putchar(first ? ' ' : ',');
			first = 0;
			printf("%s=?", ur->name);
		}
		printf(" WHERE");
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf("%s", first ? " " : " AND ");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->name,
					optypes[ur->op]);
			first = 0;
		}
		puts("\",");
	}

	/* Custom delete queries. */

	pos = 0;
	TAILQ_FOREACH(up, &p->dq, entries) {
		printf("\t/* STMT_%s_DELETE_%zu */\n"
		       "\t\"DELETE FROM %s WHERE",
		       p->cname, pos++, p->name);
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf("%s", first ? " " : " AND ");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->name,
					optypes[ur->op]);
			first = 0;
		}
		puts("\",");
	}
}

void
gen_c_source(const struct strctq *q, int json, const char *header)
{
	const struct strct *p;

	print_commentt(0, COMMENT_C, 
		"WARNING: automatically generated by "
		"kwebapp " VERSION ".\n"
		"DO NOT EDIT!");

	/* Start with all headers we'll need. */

	puts("#include <sys/queue.h>");

	TAILQ_FOREACH(p, q, entries) 
		if (STRCT_HAS_BLOB & p->flags) {
			print_commentt(0, COMMENT_C,
				"Required for b64_ntop().");
			puts("#include <netinet/in.h>\n"
			     "#include <resolv.h>");
			break;
		}

	puts("\n"
	     "#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include <string.h>\n"
	     "#include <unistd.h>\n"
	     "\n"
	     "#include <ksql.h>");
	if (json)
		puts("#include <kcgijson.h>");


	printf("\n"
	       "#include \"%s\"\n"
	       "\n",
	       header);

	/* Enumeration for statements. */

	print_commentt(0, COMMENT_C,
		"All SQL statements we'll define in \"stmts\".");
	puts("enum\tstmt {");
	TAILQ_FOREACH(p, q, entries)
		gen_enum(p);
	puts("\tSTMT__MAX\n"
	     "};\n"
	     "");

	/* Now the array of all statement. */

	print_commentt(0, COMMENT_C,
		"Our full set of SQL statements.\n"
		"We define these beforehand because that's how ksql\n"
		"handles statement generation.\n"
		"Notice the \"AS\" part: this allows for multiple\n"
		"inner joins without ambiguity.");
	puts("static\tconst char *const stmts[STMT__MAX] = {");
	TAILQ_FOREACH(p, q, entries)
		gen_stmt(p);
	puts("};");
	puts("");

	/* Define our functions. */

	print_commentt(0, COMMENT_C,
		"Finally, all of the functions we'll use.");
	puts("");

	gen_func_open();
	gen_func_close();

	TAILQ_FOREACH(p, q, entries)
		gen_funcs(p, json);
}
