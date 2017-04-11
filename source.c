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

	printf("\tp->%s = ", f->name);

	switch(f->type) {
	case (FTYPE_INT):
		puts("ksql_stmt_int(stmt, (*pos)++);");
		break;
	case (FTYPE_TEXT):
		printf("strdup(ksql_stmt_str(stmt, (*pos)++));\n"
		     "\tif (NULL == p->%s) {\n"
		     "\t\tperror(NULL);\n"
		     "\t\texit(EXIT_FAILURE);\n"
		     "\t}\n", f->name);
		break;
	default:
		break;
	}
}

/*
 * Provide the following function definitions:
 *
 *  (1) db_xxx_fill (fill from database)
 *  (2) db_xxx_unfill (clear internal structure memory)
 *  (3) db_xxx_free (public: free object)
 *  (4) db_xxx_get (public: get object)
 *
 * Also provide documentation for each function.
 */
static void
gen_strct(const struct strct *p)
{
	const struct field *f;
	const struct search *s;
	const struct sent *sent;
	const struct sref *sr;
	char	*caps;
	size_t	 pos, searchnum = 0;

	caps = gen_strct_caps(p->name);

	/* Fill from database. */

	printf("void\n"
	       "db_%s_fill(struct %s *p, "
		"struct ksqlstmt *stmt, size_t *pos)\n"
	       "{\n"
	       "\tsize_t i = 0;\n"
	       "\n"
	       "\tif (NULL == pos)\n"
	       "\t\tpos = &i;\n"
	       "\tmemset(p, 0, sizeof(*p));\n",
	       p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			printf("\tdb_%s_fill(&p->%s, stmt, pos);\n",
				f->name,
				f->ref->target->parent->name);
	printf("}\n"
	       "\n");

	/* Free internal resources. */

	printf("void\n"
	       "db_%s_unfill(struct %s *p)\n"
	       "{\n"
	       "\tif (NULL == p)\n"
	       "\t\treturn;\n",
	       p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_unfill_field(f);
	printf("}\n"
	       "\n");

	/* Free object. */

	printf("void\n"
	       "db_%s_free(struct %s *p)\n"
	       "{\n"
	       "\tdb_%s_unfill(p);\n"
	       "\tfree(p);\n"
	       "}\n"
	       "\n",
	       p->name, p->name, p->name);

	/* 
	 * Get object by identifier (external).
	 * This needs to account for filling in foreign key references.
	 */

	if (NULL != p->rowid)
		printf("struct %s *\n"
		       "db_%s_by_rowid(struct ksql *db, int64_t id)\n"
		       "{\n"
		       "\tstruct ksqlstmt *stmt;\n"
		       "\tstruct %s *p = NULL;\n"
		       "\n"
		       "\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_BY_ROWID],\n"
		       "\t\tSTMT_%s_BY_ROWID);\n"
		       "\tksql_bind_int(stmt, 0, id);\n"
		       "\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
		       "\t\tp = malloc(sizeof(struct %s));\n"
		       "\t\tif (NULL == p) {\n"
		       "\t\t\tperror(NULL);\n"
		       "\t\t\texit(EXIT_FAILURE);\n"
		       "\t\t}\n"
		       "\t\tdb_%s_fill(p, stmt, NULL);\n"
		       "\t}\n"
		       "\tksql_stmt_free(stmt);\n"
		       "\treturn(p);\n"
		       "}\n"
		       "\n",
		       p->name, p->name, p->name,
		       caps, caps, p->name, p->name);

	/*
	 * Generate the custom search functions.
	 */

	TAILQ_FOREACH(s, &p->sq, entries) {
		print_func_search(p, s, 0);
		printf("\n"
		       "{\n"
		       "\tstruct ksqlstmt *stmt;\n"
		       "\tstruct %s *p = NULL;\n"
		       "\n"
		       "\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
		       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
		       p->name, caps, searchnum, 
		       caps, searchnum);
		pos = 1;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			if (FTYPE_INT == sr->field->type)
				printf("\tksql_bind_int");
			else
				printf("\tksql_bind_str");
			printf("(stmt, %zu, v%zu);\n", pos - 1, pos);
			pos++;
		}
		printf("\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
		       "\t\tp = malloc(sizeof(struct %s));\n"
		       "\t\tif (NULL == p) {\n"
		       "\t\t\tperror(NULL);\n"
		       "\t\t\texit(EXIT_FAILURE);\n"
		       "\t\t}\n"
		       "\t\tdb_%s_fill(p, stmt, NULL);\n"
		       "\t}\n"
		       "\tksql_stmt_free(stmt);\n"
		       "\treturn(p);\n"
		       "}\n"
		       "\n",
		       p->name, p->name);
		searchnum++;
	}

	free(caps);
}

/*
 * Generate a set of statements that will be used for this structure:
 *
 *  (1) get by rowid (if applicable)
 *  (2) all explicit search terms
 */
static void
gen_stmt_enum(const struct strct *p)
{
	char	*caps;
	const struct search *s;
	size_t	 snum = 0;

	caps = gen_strct_caps(p->name);

	if (NULL != p->rowid)
		printf("\tSTMT_%s_BY_ROWID,\n", caps);

	TAILQ_FOREACH(s, &p->sq, entries)
		printf("\tSTMT_%s_BY_SEARCH_%zu,\n", caps, snum++);

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
 * Fill in the statements noted in gen_stmt_enum().
 */
static void
gen_stmt(const struct strct *p)
{
	const struct search *s;
	const struct sent *sent;
	const struct sref *sr;
	int	 first;

	if (NULL != p->rowid)  {
		printf("\t\"SELECT ");
		gen_stmt_schema(p, p, NULL);
		printf("\" FROM %s", p->name);
		gen_stmt_joins(p, p, NULL);
		printf(" WHERE %s.%s=?\",\n", 
			p->name, p->rowid->name);
	}

	TAILQ_FOREACH(s, &p->sq, entries) {
		printf("\t\"SELECT ");
		gen_stmt_schema(p, p, NULL);
		printf("\" FROM %s", p->name);
		gen_stmt_joins(p, p, NULL);
		printf(" WHERE");
		first = 1;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if ( ! first)
				printf(" AND");
			first = 0;
			sr = TAILQ_LAST(&sent->srq, srefq);
			printf(" %s.%s=?", 
				NULL == sent->alias ?
				p->name : sent->alias->alias,
				sr->name);
		}
		puts("\",");
	}
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

	puts("/*\n"
	     " * DO NOT EDIT.\n"
	     " * Automatically generated by kwebapp " VERSION ".\n"
	     " */");

	/* Start with all headers we'll need. */

	printf("#include <stdio.h>\n"
	       "#include <stdlib.h>\n"
	       "#include <string.h>\n"
	       "\n"
	       "#include <ksql.h>\n"
	       "\n"
	       "#include \"%s\"\n"
	       "\n",
	       header);

	/* Enumeration for statements. */

	puts("/*\n"
	     " * All SQL statements we'll define in \"stmts\".\n"
	     " */\n"
	     "enum\tstmt {");
	TAILQ_FOREACH(p, q, entries)
		gen_stmt_enum(p);
	puts("\tSTMT__MAX\n"
	     "};\n"
	     "");

	/* A set of CPP defines for per-table schema. */

	puts("/*\n"
	     " * Define our table columns.\n"
	     " * Do this as a series of macros because our\n"
	     " * statements will use the \"AS\" feature when\n"
	     " * generating its queries.\n"
	     " */");
	puts("#define STR(_name) #_name");
	TAILQ_FOREACH(p, q, entries)
		gen_schema(p);
	puts("");

	/* Now the array of all statement. */

	puts("/*\n"
	     " * Our full set of SQL statements.\n"
	     " * We define these beforehand because that's how ksql\n"
	     " * handles statement generation.\n"
	     " * Notice the \"AS\" part: this allows for multiple\n"
	     " * inner joins without ambiguity.\n"
	     " */\n"
	     "static\tconst char *const stmts[STMT__MAX] = {");
	TAILQ_FOREACH(p, q, entries)
		gen_stmt(p);
	puts("};");
	puts("");

	/* Define our functions. */

	TAILQ_FOREACH(p, q, entries)
		gen_strct(p);
}
