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
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "ort.h"
#include "extern.h"
#include "comments.h"

static	const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"get", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const utypes[UP__MAX] = {
	"update", /* UP_MODIFY */
	"delete", /* UP_DELETE */
};

static	const char *const modtypes[MODTYPE__MAX] = {
	"cat", /* MODTYPE_CONCAT */
	"dec", /* MODTYPE_DEC */
	"inc", /* MODTYPE_INC */
	"set", /* MODTYPE_SET */
	"strset", /* MODTYPE_STRSET */
};

static	const char *const optypes[OPTYPE__MAX] = {
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

static	const char *const ftypes[FTYPE__MAX] = {
	"number", /* FTYPE_BIT */
	"number", /* FTYPE_DATE */
	"number", /* FTYPE_EPOCH */
	"number", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
	"ArrayBuffer", /* FTYPE_BLOB */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"number", /* FTYPE_BITFIELD */
};

/*
 * Print a variable vNN where NN is position "pos" (from one) with the
 * appropriate type in a method signature.
 * This will start with a comma if not the first variable.
 * Returns the number of columns on the current line.
 */
static size_t
print_var(size_t pos, size_t col, const struct field *f)
{
	int	rc;

	if (pos > 1) {
		putchar(',');
		col++;
	}

	if (col >= 72) {
		fputs("\n\t\t", stdout);
		col = 16;
	} else if (pos > 1) {
		putchar(' ');
		col++;
	}

	col += (rc = printf("v%zu: ", pos)) > 0 ? rc : 0;

	rc = f->type == FTYPE_ENUM ?
		printf("ortns.%s", f->enm->name) :
		printf("%s", ftypes[f->type]);

	col += rc > 0 ? rc : 0;
	return col;
}

static size_t
print_name_update(const struct update *u)
{
	const struct uref *ur;
	size_t	 	   col = 0;
	int		   rc;

	rc = printf("db_%s_%s", u->parent->name, utypes[u->type]);
	col = rc > 0 ? rc : 0;

	if (u->name == NULL && u->type == UP_MODIFY) {
		if (!(u->flags & UPDATE_ALL))
			TAILQ_FOREACH(ur, &u->mrq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					modtypes[ur->mod]);
				col += rc > 0 ? rc : 0;
			}
		if (!TAILQ_EMPTY(&u->crq)) {
			col += (rc = printf("_by")) > 0 ? rc : 0;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					optypes[ur->op]);
				col += rc > 0 ? rc : 0;
			}
		}
	} else if (u->name == NULL) {
		if (!TAILQ_EMPTY(&u->crq)) {
			col += (rc = printf("_by")) > 0 ? rc : 0;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					optypes[ur->op]);
				col += rc > 0 ? rc : 0;
			}
		}
	} else 
		col += (rc = printf("_%s", u->name)) > 0 ? rc : 0;

	return col;
}

/*
 * Print the method name for an insertion.
 * Returns the number of columns printed.
 */
static size_t
print_name_insert(const struct strct *p)
{
	int	 rc;

	return (rc = printf("db_%s_insert", p->name)) > 0 ? rc : 0;
}

/*
 * Print the method name for a query.
 * Returns the number of columns printed.
 */
static size_t
print_name_search(const struct search *s)
{
	const struct sent	*sent;
	size_t			 sz = 0;
	int			 rc;

	rc = printf("db_%s_%s", s->parent->name, stypes[s->type]);
	sz += rc > 0 ? rc : 0;

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		sz += (rc = printf("_by")) > 0 ? rc : 0;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			rc = printf("_%s_%s", 
				sent->uname, optypes[sent->op]);
			sz += rc > 0 ? rc : 0;
		}
	} else if (s->name != NULL)
		sz += (rc = printf("_%s", s->name)) > 0 ? rc : 0;

	return sz;
}

/*
 * Generate the insertion function.
 */
static void
gen_insert(const struct strct *p)
{
	const struct field	*f;
	size_t	 	 	 pos = 1, col;

	puts("");
	print_commentt(1, COMMENT_JS_FRAG_OPEN,
		"Insert a new row into the database. Only "
		"native (and non-rowid) fields may be set.");

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT ||
		    (f->flags & FIELD_ROWID))
			continue;
		print_commentv(1, COMMENT_JS_FRAG,
			"@param v%zu %s", pos++, f->name);
	}
	print_commentt(1, COMMENT_JS_FRAG_CLOSE,
		"@return New row's identifier on success or "
		"<0 otherwise.");

	col = 8;
	putchar('\t');
	if ((col += print_name_insert(p)) >= 72) {
		fputs("\n\t(", stdout);
		col = 9;
	} else {
		putchar('(');
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (!(f->type == FTYPE_STRUCT || 
		      (f->flags & FIELD_ROWID)))
			col = print_var(pos++, col, f);

	fputs("):", stdout);

	if (col + 7 >= 72)
		fputs("\n\t\tnumber", stdout);
	else
		fputs(" number", stdout);

	puts("\n"
	     "\t{\n"
	     "\t\treturn 0;\n"
	     "\t}");
}

/*
 * Generate update/delete function.
 */
static void
gen_modifier(const struct config *cfg, const struct update *up)
{
	const struct uref	*ref;
	enum cmtt		 ct = COMMENT_JS_FRAG_OPEN;
	size_t		 	 pos = 1, col;
	int			 hasunary = 0;

	/* Do we document non-parameterised constraints? */

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (OPTYPE_ISUNARY(ref->op)) {
			hasunary = 1;
			break;
		}

	/* Documentation. */

	puts("");
	if (up->doc != NULL) {
		print_commentt(1, COMMENT_JS_FRAG_OPEN, up->doc);
		ct = COMMENT_JS_FRAG;
	}

	if (hasunary) { 
		print_commentt(1, ct,
			"The following fields are constrained by "
			"unary operations: ");
		TAILQ_FOREACH(ref, &up->crq, entries)
			if (!OPTYPE_ISUNARY(ref->op)) {
				print_commentv(1, COMMENT_JS_FRAG,
					"%s (checked %s null)", 
					ref->field->name, 
					ref->op == OPTYPE_NOTNULL ?
					"not" : "is");
				ct = COMMENT_JS_FRAG;
			}
	}

	if (up->type == UP_MODIFY)
		TAILQ_FOREACH(ref, &up->mrq, entries) {
			if (ref->field->type == FTYPE_PASSWORD) 
				print_commentv(1, ct,
					"@param v%zu update %s "
					"(hashed)", pos++, 
					ref->field->name);
			else
				print_commentv(1, ct,
					"@param v%zu update %s",
					pos++, ref->field->name);
			ct = COMMENT_JS_FRAG;
		}

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (!OPTYPE_ISUNARY(ref->op)) {
			print_commentv(1, ct,
				"@param v%zu %s (%s)", pos++, 
				ref->field->name, 
				optypes[ref->op]);
			ct = COMMENT_JS_FRAG;
		}

	if (ct == COMMENT_JS_FRAG_OPEN)
		ct = COMMENT_JS;
	else
		ct = COMMENT_JS_FRAG_CLOSE;

	print_commentt(1, ct,
		"@return False on constraint violation, "
		"true on success.");

	col = 8;
	putchar('\t');
	if ((col += print_name_update(up)) >= 72) {
		fputs("\n\t(", stdout);
		col = 9;
	} else {
		putchar('(');
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries)
		col = print_var(pos++, col, ref->field);
	TAILQ_FOREACH(ref, &up->crq, entries)
		if (!OPTYPE_ISUNARY(ref->op))
			col = print_var(pos++, col, ref->field);

	fputs("):", stdout);
	if (col + 7 >= 72)
		fputs("\n\t\tboolean", stdout);
	else
		fputs(" boolean", stdout);

	puts("\n"
	     "\t{\n"
	     "\t\treturn false;\n"
	     "\t}");
}

/*
 * Generate the query function.
 */
static void
gen_query(const struct config *cfg, const struct search *s)
{
	const struct sent	*sent;
	const struct strct	*rs;
	size_t			 pos, col, sz;
	int		 	 hasunary = 0, rc;

	/*
	 * The "real struct" we'll return is either ourselves or the one
	 * we reference with a distinct clause.
	 */

	rs = s->dst != NULL ? s->dst->strct : s->parent;

	/* Do we document non-parameterised constraints? */

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISUNARY(sent->op)) {
			hasunary = 1;
			break;
		}

	puts("");

	/* Print per-query-type method documentation. */

	if (s->doc != NULL)
		print_commentt(1, COMMENT_JS_FRAG_OPEN, s->doc);
	else if (s->type == STYPE_SEARCH)
		print_commentv(1, COMMENT_JS_FRAG_OPEN,
			"Search for a specific {@link ortns.%s}.", 
			rs->name);
	else if (s->type == STYPE_LIST)
		print_commentv(1, COMMENT_JS_FRAG_OPEN,
			"Search for a set of {@link ortns.%s}.", 
			rs->name);
	else if (s->type == STYPE_COUNT)
		print_commentv(1, COMMENT_JS_FRAG_OPEN,
			"Search result count of {@link ortns.%s}.", 
			rs->name);
	else
		print_commentv(1, COMMENT_JS_FRAG_OPEN,
			"Iterate results in {@link ortns.%s}.", 
			rs->name);

	if (s->dst != NULL) {
		print_commentv(1, COMMENT_JS_FRAG,
			"This %s distinct query results.",
			s->type == STYPE_ITERATE ? "iterates over" : 
			s->type == STYPE_COUNT ? "counts" : "returns");
		if (s->dst->strct != s->parent) 
			print_commentv(1, COMMENT_JS_FRAG,
				"The results are limited to "
				"{@link ortns.%s.%s}.", 
				s->parent->name, s->dst->fname);
	}

	if (s->type == STYPE_ITERATE)
		print_commentt(1, COMMENT_JS_FRAG,
			"This callback function is called during an "
			"implicit transaction: thus, it should not "
			"invoke any database modifications or risk "
			"deadlock.");
	if (rs->flags & STRCT_HAS_NULLREFS)
		print_commentt(1, COMMENT_JS_FRAG,
			"This search involves nested null structure "
			"linking, which involves multiple database "
			"calls per invocation. Use this sparingly!");

	if (hasunary) { 
		print_commentt(1, COMMENT_JS_FRAG,
			"The following fields are constrained by "
			"unary operations: ");
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if (!OPTYPE_ISUNARY(sent->op))
				continue;
			print_commentv(1, COMMENT_JS_FRAG,
				"%s (checked %s null)", sent->fname, 
				sent->op == OPTYPE_NOTNULL ?
				"not" : "is");
		}
	}

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type == FTYPE_PASSWORD)
			print_commentv(1, COMMENT_JS_FRAG,
				"@param v%zu %s (hashed password)", 
				pos++, sent->fname);
		else
			print_commentv(1, COMMENT_JS_FRAG,
				"@param v%zu %s", pos++, sent->fname);
	}

	if (s->type == STYPE_ITERATE)
		print_commentt(1, COMMENT_JS_FRAG_CLOSE,
			"@param cb Callback with retrieved data.");

	if (s->type == STYPE_SEARCH)
		print_commentt(1, COMMENT_JS_FRAG_CLOSE,
			"@return Result or null on fail.");
	else if (s->type == STYPE_LIST)
		print_commentt(1, COMMENT_JS_FRAG_CLOSE,
			"@return Array of results which may be empty.");
	else if (s->type == STYPE_COUNT)
		print_commentt(1, COMMENT_JS_FRAG_CLOSE,
			"@return Count of results.");

	/* Now generate the method body. */

	col = 8;
	putchar('\t');
	if ((col += print_name_search(s)) >= 72) {
		fputs("\n\t(", stdout);
		col = 9;
	} else {
		putchar('(');
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (!OPTYPE_ISUNARY(sent->op))
			col = print_var(pos++, col, sent->field);

	if (s->type == STYPE_ITERATE) {
		sz = strlen(rs->name) + 25;
		if (col + sz >= 72) {
			fputs(",\n\t\t", stdout);
			col = 16;
		} else  {
			fputs(", ", stdout);
			col += 2;
		}
		rc = printf("cb: (res: ortns.%s) => void", rs->name);
		col += rc > 0 ? rc : 0;
	}

	fputs("): ", stdout);

	if (s->type == STYPE_SEARCH)
		sz = strlen(rs->name) + 11;
	else if (s->type == STYPE_LIST)
		sz = strlen(rs->name) + 8;
	else if (s->type == STYPE_ITERATE)
		sz = 4;
	else
		sz = 6;

	if (col + sz >= 72)
		fputs("\n\t\t", stdout);

	if (s->type == STYPE_SEARCH)
		printf("ortns.%s|null\n", rs->name);
	else if (s->type == STYPE_LIST)
		printf("ortns.%s[]\n", rs->name);
	else if (s->type == STYPE_ITERATE)
		puts("void");
	else
		puts("number");

	puts("\t{");

	if (s->type == STYPE_SEARCH)
		puts("\t\treturn null;");
	else if (s->type == STYPE_LIST)
		puts("\t\treturn [];");
	else if (s->type == STYPE_COUNT)
		puts("\t\treturn 0;");

	puts("\t}");
}

static void
gen_api(const struct config *cfg, const struct strct *p)
{
	const struct search	*s;
	const struct update	*u;

	if (p->ins != NULL)
		gen_insert(p);

	TAILQ_FOREACH(s, &p->sq, entries)
		gen_query(cfg, s);
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_modifier(cfg, u);
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_modifier(cfg, u);
}

static void
gen_enm(const struct enm *p, size_t pos)
{
	const struct eitem	*ei;

	if (pos)
		puts("");
	if (p->doc != NULL)
		print_commentt(1, COMMENT_JS, p->doc);

	printf("\texport enum %s {\n", p->name);
	TAILQ_FOREACH(ei, &p->eq, entries) {
		if (ei->doc != NULL)
			print_commentt(2, COMMENT_JS, ei->doc);
		printf("\t\t%s = %" PRId64, 
			ei->name, ei->value);
		if (TAILQ_NEXT(ei, entries) != NULL)
			putchar(',');
		puts("");
	}
	puts("\t}");
}

static void
gen_strct(const struct strct *p, size_t pos)
{
	const struct field	*f;

	if (pos)
		puts("");
	if (p->doc != NULL)
		print_commentt(1, COMMENT_JS, p->doc);

	printf("\texport interface %sData {\n", p->name);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->doc != NULL)
			print_commentt(2, COMMENT_JS, f->doc);
		printf("\t\t%s: ", f->name);
		if (f->type == FTYPE_STRUCT)
			printf("ortns.%s", 
				f->ref->target->parent->name);
		else if (f->type == FTYPE_ENUM)
			printf("ortns.%s", f->enm->name);
		else
			printf("%s", ftypes[f->type]);

		if (f->flags & FIELD_NULL)
			printf("|null");
		puts(";");
	}
	puts("\t}\n");

	print_commentv(1, COMMENT_JS,
		"Class instance of {@link ortns.%sData}.",
		p->name);

	printf("\texport class %s {\n"
	       "\t\t#role: number;\n"
	       "\t\treadonly obj: ortns.%sData;\n"
	       "\n"
	       "\t\tconstructor(role: number, obj: ortns.%sData)\n"
	       "\t\t{\n"
	       "\t\t\tthis.#role = role;\n"
	       "\t\t\tthis.obj = obj;\n"
	       "\t\t}\n"
	       "\t}\n",
	       p->name, p->name, p->name);
}

/*
 * This outputs the data structure part of the data model under the
 * "ortns" namespace.
 * It's divided primarily into data within interfaces and classes that
 * encapsulate that data and contain role information.
 */
static void
gen_ortns(const struct config *cfg)
{
	const struct strct	*p;
	const struct enm	*e;
	size_t			 i = 0;

	puts("");
	print_commentt(0, COMMENT_JS,
		"Namespace for data interfaces and representative "
		"classes.  The interfaces are for the data itself, "
		"while the classes manage roles and metadata.");
	puts("export namespace ortns {");
	TAILQ_FOREACH(e, &cfg->eq, entries)
		gen_enm(e, i++);
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_strct(p, i++);
	puts("}");
}

/*
 * Output the class for managing a single connection.
 * This is otherwise defined as a single sequence of role transitions.
 */
static void
gen_ortdb(void)
{
	puts("");
	print_commentt(0, COMMENT_JS,
		"Primary database object. "
		"Only one of these should exist per running node.js "
		"server.");
	printf("export class ortdb {\n"
	       "\t#dbname: string;\n"
	       "\treadonly version: string = \'%s\';\n"
	       "\treadonly vstamp: number = %lld;\n"
	       "\n",
	       VERSION, (long long)VSTAMP);
	print_commentt(1, COMMENT_JS,
		"@param dbname The file-name of the database "
		"relative to the running application.");
	puts("\tconstructor(dbname: string) {\n"
	     "\t\tthis.#dbname = dbname;\n"
	     "\t}\n");
	print_commentt(1, COMMENT_JS,
		"Create a connection to the database. "
		"This should be called for each sequence "
		"representing a single operator. "
		"In web applications, for example, this should "
		"be called for each request.");
	puts("\tconnect(): ortctx\n"
	     "\t{\n"
	     "\t\treturn new ortctx(this);\n"
	     "\t}\n"
	     "}");
}

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 */
static void
gen_alias_builder(const struct strct *p)
{
	const struct field	*f, *last = NULL;
	int			 first = 1;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type != FTYPE_STRUCT)
			last = f;

	assert(last != NULL);
	printf("\n"
	       "\texport function ort_schema_%s(v: string): string\n"
	       "\t{\n"
	       "\t\treturn ", p->name);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT)
			continue;
		if (!first)
			fputs("\t\t       ", stdout);
		printf("v + \'.%s\'", f->name);
		if (f != last)
			puts(" + \',\' +");
		else
			puts(";");
		first = 0;
	}
	puts("\t}");
}

/*
 * Output the data access portion of the data model entirely within a
 * single class.
 */
static void
gen_ortctx(const struct config *cfg)
{
	const struct strct	*p;

	puts("namespace ortstmt {");
	puts("\tenum ortstmt {");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		print_sql_enums(2, p, LANG_JS);
	puts("\t}");
	puts("\treadonly ortstmts: string[] = [");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		print_sql_stmts(2, p, LANG_JS);
	puts("\t];");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_alias_builder(p);
	puts("}\n");

	puts("");
	print_commentt(0, COMMENT_JS,
		"Manages all access to the database. "
		"This object should be used for the lifetime of a "
		"single \'request\', such as a request for a web "
		"application.");
	puts("export class ortctx {\n"
	     "\t#role: number;\n"
	     "\treadonly #o: ortdb;\n"
	     "\n"
	     "\tconstructor(o: ortdb) {\n"
	     "\t\tthis.#o = o;\n"
	     "\t\tthis.#role = 0;\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransImmediate(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransDeferred(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransExclusive(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransRollback(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransRollback(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransCommit(id: number): void\n"
	     "\t{\n"
	     "\t}\n"
	     "\n"
	     "\tdbRole(newrole: number): void\n"
	     "\t{\n"
	     "\t\tthis.#role = newrole;\n"
	     "\t}");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_api(cfg, p);
	puts("}");
}

/*
 * Top-level generator.
 */
static void
gen_nodejs(const struct config *cfg)
{

	print_commentv(0, COMMENT_JS, 
	       "WARNING: automatically generated by "
	       "%s " VERSION ".\n"
	       "DO NOT EDIT!\n"
	       "@packageDocumentation", getprogname());

	gen_ortns(cfg);
	gen_ortdb();
	gen_ortctx(cfg);

	puts("");
	print_commentt(0, COMMENT_JS,
		"Instance an application-wide context. "
		"This should only be called once per server, with "
		"the {@link ortdb.connect} method used for "
		"sequences of operations.");
	puts("export function ort(dbname: string): ortdb\n"
	     "{\n"
	     "\treturn new ortdb(dbname);\n"
	     "}");
}

int
main(int argc, char *argv[])
{
	struct config	 *cfg = NULL;
	int		  c, rc = 0;
	FILE		**confs = NULL;
	size_t		  i, confsz;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((c = getopt(argc, argv, "")) != -1)
		goto usage;

	argc -= optind;
	argv += optind;
	confsz = (size_t)argc;
	
	/* Read in all of our files now so we can repledge. */

	if (confsz > 0) {
		if ((confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
	}

	/* No more opening files. */

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		goto out;

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		gen_nodejs(cfg);
out:
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s", argv[i]);
	free(confs);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s [config]\n",
		getprogname());
	return EXIT_FAILURE;
}
