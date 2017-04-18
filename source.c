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
	"ksql_stmt_str", /* FTYPE_TEXT */
	"ksql_stmt_str", /* FTYPE_PASSWORD */
	NULL, /* FTYPE_STRUCT */
};

/*
 * Functions binding an argument to a statement.
 */
static	const char *const bindtypes[FTYPE__MAX] = {
	"ksql_bind_int", /* FTYPE_INT */
	"ksql_bind_double", /* FTYPE_REAL */
	"ksql_bind_str", /* FTYPE_TEXT */
	"ksql_bind_str", /* FTYPE_PASSWORD */
	NULL, /* FTYPE_STRUCT */
};

/*
 * Put a structure name in capitals.
 * Used for preprocessor definitions.
 * FIXME: this should only happen once, or maybe be stored along with
 * the structure itself.
 */
static char *
gen_strct_caps(const char *v)
{
	char 	*cp, *caps;

	if (NULL == (caps = strdup(v)))
		err(EXIT_FAILURE, NULL);
	for (cp = caps; '\0' != *cp; cp++)
		*cp = toupper((int)*cp);

	return(caps);
}

/*
 * Free ("unfill") an individual field that was filled from the
 * database (see gen_strct_fill_field()).
 */
static void
gen_strct_unfill_field(const struct field *f)
{

	switch(f->type) {
	case (FTYPE_STRUCT):
		printf("\tdb_%s_unfill(&p->%s);\n",
			f->ref->tstrct,
			f->name);
		break;
	case (FTYPE_PASSWORD):
		/* FALLTHROUGH */
	case (FTYPE_TEXT):
		printf("\tfree(p->%s);\n", f->name);
		break;
	default:
		break;
	}
}

/*
 * Fill an individual field from the database.
 * See gen_strct_unfill_field().
 */
static void
gen_strct_fill_field(const struct field *f)
{

	if (FTYPE_STRUCT == f->type)
		return;

	if (FIELD_NULL & f->flags)
		printf("\tp->has_%s = "
			"! ksql_stmt_isnull(stmt, *pos);\n"
		       "\tif (p->has_%s)\n"
		       "\t\tp->%s = ", f->name, f->name, f->name);
	else 
		printf("\tp->%s = ", f->name);

	/* Some sequences need surrounding allocation. */

	if (FTYPE_TEXT == f->type || 
	    FTYPE_PASSWORD == f->type)
		printf("strdup(%s(stmt, (*pos)++));\n", 
			coltypes[f->type]);
	else
		printf("%s(stmt, (*pos)++);\n", 
			coltypes[f->type]);

	if (FIELD_NULL & f->flags) {
		puts("\telse\n"
		     "\t\t(*pos)++;");
		if (FTYPE_TEXT == f->type || FTYPE_PASSWORD == f->type)
			printf("\tif (p->has_%s && NULL == p->%s) {\n"
			       "\t\tperror(NULL);\n"
			       "\t\texit(EXIT_FAILURE);\n"
			       "\t}\n", f->name, f->name);
	} else if (FTYPE_TEXT == f->type || FTYPE_PASSWORD == f->type) 
		printf("\tif (NULL == p->%s) {\n"
		       "\t\tperror(NULL);\n"
		       "\t\texit(EXIT_FAILURE);\n"
		       "\t}\n", f->name);
}

/*
 * Print out a search function for an STYPE_ITERATE.
 * This calls a function pointer with the retrieved data.
 */
static void
gen_strct_func_iter(const struct search *s, const char *caps, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_ITERATE == s->type);

	print_func_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s p;\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, caps, num, caps, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		assert(FTYPE_STRUCT != sr->field->type);
		if (FTYPE_PASSWORD != sr->field->type)
			printf("\t%s(stmt, %zu, v%zu);\n", 
				bindtypes[sr->field->type],
				pos - 1, pos);
		pos++;
	}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tdb_%s_fill(&p, stmt, NULL);\n",
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
		       "\t\t\tdb_%s_unfill(&p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	printf("\t\t(*cb)(&p, arg);\n"
	       "\t\tdb_%s_unfill(&p);\n"
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
gen_strct_func_list(const struct search *s, const char *caps, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_LIST == s->type);

	print_func_search(s, 0);
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
	       s->parent->name, caps, num, caps, num);

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, don't insert into the tailq.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		assert(FTYPE_STRUCT != sr->field->type);
		if (FTYPE_PASSWORD != sr->field->type)
			printf("\t%s(stmt, %zu, v%zu);\n", 
				bindtypes[sr->field->type],
				pos - 1, pos);
		pos++;
	}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill(p, stmt, NULL);\n",
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

	print_func_open(0);
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

	print_func_close(0);
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
gen_strct_func_srch(const struct search *s, const char *caps, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos;

	assert(STYPE_SEARCH == s->type);

	print_func_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s *p = NULL;\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, caps, num, caps, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		assert(FTYPE_STRUCT != sr->field->type);
		if (FTYPE_PASSWORD != sr->field->type)
			printf("\t%s(stmt, %zu, v%zu);\n", 
				bindtypes[sr->field->type],
				pos - 1, pos);
		pos++;
	}

	printf("\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill(p, stmt, NULL);\n",
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

	print_func_freeq(p, 0);
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
gen_func_insert(const struct strct *p, const char *caps)
{
	const struct field *f;
	size_t	 pos, npos;

	print_func_insert(p, 0);
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
	       caps, caps);

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
			continue;
		}
		printf("\t%s(stmt, %zu, %sv%zu);\n", 
			bindtypes[f->type], npos - 1, 
			FIELD_NULL & f->flags ? "*" : "", npos);
		npos++;
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

	print_func_free(p, 0);
	printf("\n"
	       "{\n"
	       "\tdb_%s_unfill(p);\n"
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

	print_func_unfill(p, 0);
	puts("\n"
	     "{\n"
	     "\tif (NULL == p)\n"
	     "\t\treturn;");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_unfill_field(f);
	printf("}\n"
	       "\n");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill(const struct strct *p)
{
	const struct field *f;

	print_func_fill(p, 0);
	puts("\n"
	     "{\n"
	     "\tsize_t i = 0;\n"
	     "\n"
	     "\tif (NULL == pos)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		printf("\tdb_%s_fill(&p->%s, stmt, pos);\n",
			f->name, f->ref->target->parent->name);
	}
	printf("}\n"
	       "\n");
}

/*
 * Generate an "update" function.
 */
static void
gen_func_update(const struct update *up, 
	const char *caps, size_t num)
{
	const struct uref *ref;
	size_t	 pos, npos;

	print_func_update(up, 0);
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

	printf("\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_UPDATE_%zu],\n"
	       "\t\tSTMT_%s_UPDATE_%zu);\n",
	       caps, num, caps, num);

	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		assert(FTYPE_STRUCT != ref->field->type);
		if (FIELD_NULL & ref->field->flags)
			printf("\tif (NULL == v%zu)\n"
			       "\t\tksql_bind_null(stmt, %zu);\n"
			       "\telse\n"
			       "\t", npos, npos - 1);
		if (FTYPE_PASSWORD == ref->field->type) {
			printf("\t%s(stmt, %zu, hash%zu);\n",
			       bindtypes[ref->field->type],
			       npos - 1, pos++);
		} else {
			printf("\t%s(stmt, %zu, %sv%zu);\n", 
				bindtypes[ref->field->type],
				npos - 1, 
				FIELD_NULL & ref->field->flags ? 
				"*" : "", npos);
		}
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

/*
 * Generate all of the functions we've defined in our header for the
 * given structure "s".
 */
static void
gen_funcs(const struct strct *p)
{
	const struct search *s;
	const struct update *u;
	char	*caps;
	size_t	 pos;

	caps = gen_strct_caps(p->name);

	gen_func_fill(p);
	gen_func_unfill(p);
	gen_func_free(p);
	gen_func_freeq(p);
	gen_func_insert(p, caps);

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_SEARCH == s->type)
			gen_strct_func_srch(s, caps, pos++);
		else if (STYPE_LIST == s->type)
			gen_strct_func_list(s, caps, pos++);
		else
			gen_strct_func_iter(s, caps, pos++);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_func_update(u, caps, pos++);

	free(caps);
}

/*
 * Generate a set of statements that will be used for this structure.
 */
static void
gen_enum(const struct strct *p)
{
	const struct search *s;
	const struct update *u;
	char	*caps;
	size_t	 pos;

	caps = gen_strct_caps(p->name);

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		printf("\tSTMT_%s_BY_SEARCH_%zu,\n", caps, pos++);

	printf("\tSTMT_%s_INSERT,\n", caps);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		printf("\tSTMT_%s_UPDATE_%zu,\n", caps, pos++);

	free(caps);
}

/*
 * Recursively generate a series of SCHEMA_xxx statements for getting
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
	char	*caps, *name = NULL;

	/* Uppercase schema. */

	caps = gen_strct_caps(p->name);

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
		printf("\",\" SCHEMA_%s(%s) ", caps, a->alias);
	} else
		printf("\" SCHEMA_%s(%s) ", caps, p->name);

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

	free(caps);
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
	char	*caps;

	caps = gen_strct_caps(p->name);

	/* 
	 * Print custom search queries.
	 * This also uses the recursive selection.
	 */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		printf("\t/* STMT_%s_BY_SEARCH_%zu */\n"
		       "\t\"SELECT ",
			caps, pos++);
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
	       "\t\"INSERT INTO %s (", caps, p->name);
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
		       caps, pos++, p->name);
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

	free(caps);
}

/*
 * Keep our select statements tidy by pre-defining the columns in all of
 * our tables in a handy CPP statement.
 */
static void
gen_schema(const struct strct *p)
{
	const struct field *f;
	char	*caps;

	caps = gen_strct_caps(p->name);

	/* TODO: chop at 72 characters. */

	printf("#define SCHEMA_%s(_x) ", caps);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		printf("STR(_x) \".%s\"", f->name);
		if (TAILQ_NEXT(f, entries))
			printf(" \",\" ");
	}
	puts("");
	free(caps);
}

void
gen_source(const struct strctq *q, const char *header)
{
	const struct strct *p;

	print_commentt(0, COMMENT_C, 
		"WARNING: automatically generated by "
		"kwebapp " VERSION ".\n"
		"DO NOT EDIT!");

	/* Start with all headers we'll need. */

	printf("#include <sys/queue.h>\n"
	       "\n"
	       "#include <stdio.h>\n"
	       "#include <stdlib.h>\n"
	       "#include <string.h>\n"
	       "#include <unistd.h>\n"
	       "\n"
	       "#include <ksql.h>\n"
	       "\n"
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

	/* A set of CPP defines for per-table schema. */

	print_commentt(0, COMMENT_C,
		"Define our table columns.\n"
		"Do this as a series of macros because our\n"
		"statements will use the \"AS\" feature when\n"
		"generating its queries.");
	puts("#define STR(_name) #_name");
	TAILQ_FOREACH(p, q, entries)
		gen_schema(p);
	puts("");

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
		gen_funcs(p);
}
