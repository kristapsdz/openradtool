
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
#include "ort-lang-nodejs.h"
#include "lang.h"

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
	"BigInt", /* FTYPE_BIT */
	"BigInt", /* FTYPE_DATE */
	"BigInt", /* FTYPE_EPOCH */
	"BigInt", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
	"Buffer", /* FTYPE_BLOB */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"BigInt", /* FTYPE_BITFIELD */
};

static size_t
xprintf(const char *fmt, ...) 
	__attribute__((format(printf, 1, 2)));

static size_t
xprintf(const char *fmt, ...)
{
	va_list	 ap;
	int	 rc;

	va_start(ap, fmt);
	rc = vprintf(fmt, ap);
	va_end(ap);

	return rc > 0 ? rc : 0;
}

/*
 * Print a variable vNN where NN is position "pos" (from one) with the
 * appropriate type in a method signature.
 * This will start with a comma if not the first variable.
 * Returns the number of columns on the current line.
 */
static size_t
print_var(size_t pos, size_t col, const struct field *f)
{

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

	col += xprintf("v%zu: ", pos);
	col += f->type == FTYPE_ENUM ?
		xprintf("ortns.%s", f->enm->name) :
		xprintf("%s", ftypes[f->type]);
	if ((f->flags & FIELD_NULL) ||
	    (f->type == FTYPE_STRUCT &&
	     (f->ref->source->flags & FIELD_NULL)))
		col += xprintf("|null");

	return col;
}

static void
gen_role(const struct role *r, size_t tabs)
{
	const struct role	*rr;
	size_t			 i;

	if (strcmp(r->name, "all")) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("case '%s\':\n", r->name);
	}
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_role(rr, tabs);
}

/*
 * Recursively generate all roles allowed by this rolemap.
 * Returns non-zero if we wrote something, zero otherwise.
 */
static int
gen_rolemap(const struct rolemap *rm)
{
	const struct rref	*rr;

	if (rm == NULL)
		return 0;

	puts("\t\tswitch (this.#role) {");
	TAILQ_FOREACH(rr, &rm->rq, entries)
		gen_role(rr->role, 2);
	puts("\t\t\tbreak;\n"
	     "\t\tdefault:\n"
	     "\t\t\tprocess.abort();\n"
	     "\t\t}");
	return 1;
}

static void
gen_reffind(const struct strct *p)
{
	const struct field	*f;
	size_t			 col;

	if (!(p->flags & STRCT_HAS_NULLREFS))
		return;

	/* 
	 * Do we have any null-ref fields in this?
	 * (They might be in target references.)
	 */

	TAILQ_FOREACH(f, &p->fq, entries)
		if ((f->type == FTYPE_STRUCT) &&
		    (f->ref->source->flags & FIELD_NULL))
			break;

	puts("");
	putchar('\t');
	col = 8 + xprintf("private db_%s_reffind", p->name);
	if (col >= 72) {
		fputs("\n\t(", stdout);
		col = 9;
	} else {
		putchar('(');
		col++;
	}

	printf("db: ortdb, obj: ortns.%sData): void\n"
	       "\t{\n", p->name);

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT)
			continue;
		if (f->ref->source->flags & FIELD_NULL)
			printf("\t\tif (obj.%s !== null) {\n"
			       "\t\t\tlet cols: any;\n"
			       "\t\t\tconst parms: any[] = [];\n"
			       "\t\t\tconst stmt: Database.Statement =\n"
			       "\t\t\t\tdb.db.prepare(ortstmt.stmtBuilder\n"
			       "\t\t\t\t(ortstmt.ortstmt.STMT_%s_BY_UNIQUE_%s));\n"
			       "\t\t\tstmt.raw(true);\n"
			       "\t\t\tparms.push(obj.%s);\n"
		               "\t\t\tcols = stmt.get(parms);\n"
			       "\t\t\tif (typeof cols === \'undefined\')\n"
			       "\t\t\t\tthrow \'referenced row not found\';\n"
			       "\t\t\tobj.%s = this.db_%s_fill\n"
			       "\t\t\t\t({row: <any[]>cols, pos: 0});\n"
			       "\t\t}\n",
			       f->ref->source->name,
			       f->ref->target->parent->name,
			       f->ref->target->name,
			       f->ref->source->name,
			       f->name,
			       f->ref->target->parent->name);
		if (!(f->ref->target->parent->flags & 
		    STRCT_HAS_NULLREFS))
			continue;
		printf("\t\tthis.db_%s_reffind(db, obj.%s);\n", 
			f->ref->target->parent->name, f->name);
	}
	puts("\t}");
}

static void
gen_fill(const struct strct *p)
{
	size_t	 		 col;
	const struct field	*f;

	puts("");
	putchar('\t');
	col = 8 + xprintf("private db_%s_fill", p->name);
	if (col >= 72) {
		fputs("\n\t(", stdout);
		col = 9;
	} else {
		putchar('(');
		col++;
	}

	col += xprintf("data: {row: any[], pos: number}):");
	if (col + strlen(p->name) + 13 >= 72)
		fputs("\n\t\t", stdout);
	else
		putchar(' ');

	printf("ortns.%sData\n", p->name);

	printf("\t{\n"
	       "\t\tconst obj: ortns.%sData = {\n", p->name);

	col = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		switch (f->type) {
		case FTYPE_STRUCT:
			puts("\t\t\t/* A dummy value for now. */");
			if (f->ref->source->flags & FIELD_NULL) {
				printf("\t\t\t'%s': null,\n", f->name);
				break;
			}
			printf("\t\t\t'%s': <ortns.%sData>{},\n",
				f->name, f->ref->target->parent->name);
			break;
		case FTYPE_ENUM:
			/*
			 * Convert these to a string because our
			 * internal representation is 64-bit but numeric
			 * enumerations are constraint to 53.
			 */
			printf("\t\t\t'%s': <ortns.%s%s>",
				f->name, f->enm->name,
				(f->flags & FIELD_NULL) ? 
				"|null" : "");
			if (f->flags & FIELD_NULL)
				printf("(data.row[data.pos + %zu] === "
					"null ?\n\t\t\t\tnull : "
					"data.row[data.pos + %zu]."
					"toString()),\n", col, col);
			else
				printf("data.row[data.pos + %zu]."
					"toString(),\n", col);
			break;
		default:
			assert(ftypes[f->type] != NULL);
			printf("\t\t\t'%s': <%s%s>"
				"data.row[data.pos + %zu],\n",
				f->name, ftypes[f->type],
				(f->flags & FIELD_NULL) ? 
				"|null" : "", col);
			break;
		}
		if (f->type != FTYPE_STRUCT)
			col++;
	}

	printf("\t\t};\n"
	       "\t\tdata.pos += %zu;\n", col);

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type == FTYPE_STRUCT &&
		    !(f->ref->source->flags & FIELD_NULL))
			printf("\t\tobj.%s = this.db_%s_fill(data);\n",
				f->name, f->ref->target->parent->name);

	puts("\t\treturn obj;\n"
	     "\t}");
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

	putchar('\t');
	col = 8 + xprintf("db_%s_insert", p->name);
	if (col >= 72) {
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
		fputs("\n\t\tBigInt", stdout);
	else
		fputs(" BigInt", stdout);

	printf("\n"
	       "\t{\n"
	       "\t\tconst parms: any[] = [];\n"
	       "\t\tlet info: Database.RunResult;\n"
	       "\t\tconst stmt: Database.Statement =\n"
	       "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	       "\t\t\t(ortstmt.ortstmt.STMT_%s_INSERT));\n"
	       "\n", p->name);

	if (gen_rolemap(p->ins->rolemap))
		puts("");

	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT ||
		    (f->flags & FIELD_ROWID))
			continue;

		if (f->type != FTYPE_PASSWORD) {
			printf("\t\tparms.push(v%zu);\n", pos++);
			continue;
		}

		if (f->flags & FIELD_NULL)
			printf("\t\tif (v%zu === null)\n"
			       "\t\t\tparms.push(null);\n"
			       "\t\telse\n"
			       "\t\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos, pos);
		else
			printf("\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos);
		pos++;
	}

	puts("\n"
	     "\t\ttry {\n"
	     "\t\t\tinfo = stmt.run(parms);\n"
	     "\t\t} catch (er) {\n"
	     "\t\t\treturn BigInt(-1);\n"
	     "\t\t}\n"
	     "\n"
	     "\t\treturn BigInt(info.lastInsertRowid);\n"
	     "\t}");
}

/*
 * Generate update/delete function.
 */
static void
gen_modifier(const struct config *cfg,
	const struct update *up, size_t num)
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

	if (up->type == UP_MODIFY)
		print_commentt(1, ct,
			"@return False on constraint violation, "
			"true on success.");
	else
		print_commentt(1, ct, "");

	/* Method signature. */

	putchar('\t');
	col = 8 + xprintf("db_%s_%s",
		up->parent->name, utypes[up->type]);

	if (up->name == NULL && up->type == UP_MODIFY) {
		if (!(up->flags & UPDATE_ALL))
			TAILQ_FOREACH(ref, &up->mrq, entries)
				col += xprintf("_%s_%s", 
					ref->field->name, 
					modtypes[ref->mod]);
		if (!TAILQ_EMPTY(&up->crq)) {
			col += xprintf("_by");
			TAILQ_FOREACH(ref, &up->crq, entries)
				col += xprintf("_%s_%s", 
					ref->field->name, 
					optypes[ref->op]);
		}
	} else if (up->name == NULL) {
		if (!TAILQ_EMPTY(&up->crq)) {
			col += xprintf("_by");
			TAILQ_FOREACH(ref, &up->crq, entries)
				col += xprintf("_%s_%s", 
					ref->field->name, 
					optypes[ref->op]);
		}
	} else 
		col += xprintf("_%s", up->name);

	if (col >= 72) {
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
		fputs("\n\t\t", stdout);
	else
		putchar(' ');

	fputs(up->type == UP_MODIFY ? "boolean" : "void", stdout);

	/* Method body. */

	printf("\n"
	       "\t{\n"
	       "\t\tconst parms: any[] = [];\n"
	       "\t\tlet info: Database.RunResult;\n"
	       "\t\tconst stmt: Database.Statement =\n"
	       "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	       "\t\t\t(ortstmt.ortstmt.STMT_%s_%s_%zu));\n"
	       "\n", 
	       up->parent->name,
	       up->type == UP_MODIFY ? "UPDATE" : "DELETE",
	       num);
	if (gen_rolemap(up->rolemap))
		puts("");

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (ref->field->type != FTYPE_PASSWORD ||
		    ref->mod == MODTYPE_STRSET) {
			printf("\t\tparms.push(v%zu);\n", pos++);
			continue;
		}
		if (ref->field->flags & FIELD_NULL)
			printf("\t\tif (v%zu === null)\n"
			       "\t\t\tparms.push(null);\n"
			       "\t\telse\n"
			       "\t\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos, pos);
		else
			printf("\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos);
		pos++;
	}

	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		printf("\t\tparms.push(v%zu);\n", pos++);
	}

	if (up->type == UP_MODIFY)
		puts("\n"
		     "\t\ttry {\n"
		     "\t\t\tinfo = stmt.run(parms);\n"
		     "\t\t} catch (er) {\n"
		     "\t\t\treturn false;\n"
		     "\t\t}\n"
		     "\n"
		     "\t\treturn true;");
	else
		puts("\n"
		     "\t\tstmt.run(parms);");

	puts("\t}");
}

/*
 * Generate a query function.
 */
static int
gen_query(FILE *f, const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct strct	*rs;
	size_t			 pos, col, sz;
	int		 	 hasunary = 0;

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

	/* Print per-query-type method documentation. */

	if (fputc('\n', f) == EOF)
		return 0;

	if (s->doc != NULL) {
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_OPEN, s->doc))
			return 0;
	} else if (s->type == STYPE_SEARCH) {
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG_OPEN,
		    "Search for a specific {@link ortns.%s}.", 
		    rs->name))
			return 0;
	} else if (s->type == STYPE_LIST) {
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG_OPEN,
		    "Search for a set of {@link ortns.%s}.", 
		    rs->name))
			return 0;
	} else if (s->type == STYPE_COUNT) {
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG_OPEN,
		    "Search result count of {@link ortns.%s}.", 
		    rs->name))
			return 0;
	} else
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG_OPEN,
		    "Iterate results in {@link ortns.%s}.", 
		    rs->name))
			return 0;

	if (s->dst != NULL) {
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
		    "This %s distinct query results.",
		    s->type == STYPE_ITERATE ? "iterates over" : 
		    s->type == STYPE_COUNT ? "counts" : "returns"))
			return 0;
		if (s->dst->strct != s->parent) 
			if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
			    "The results are limited to "
			    "{@link ortns.%s.%s}.", 
			    s->parent->name, s->dst->fname))
				return 0;
	}

	if (s->type == STYPE_ITERATE)
		if (!gen_comment(f, 1, COMMENT_JS_FRAG,
		    "This callback function is called during an "
		    "implicit transaction: thus, it should not "
		    "invoke any database modifications or risk "
		    "deadlock."))
			return 0;
	if (rs->flags & STRCT_HAS_NULLREFS)
		if (!gen_comment(f, 1, COMMENT_JS_FRAG,
		    "This search involves nested null structure "
		    "linking, which involves multiple database "
		    "calls per invocation. Use this sparingly!"))
			return 0;

	if (hasunary) { 
		if (!gen_comment(f, 1, COMMENT_JS_FRAG,
		    "The following fields are constrained by "
		    "unary operations: "))
			return 0;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if (!OPTYPE_ISUNARY(sent->op))
				continue;
			if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
			    "%s (checked %s null)", sent->fname, 
			    sent->op == OPTYPE_NOTNULL ? "not" : "is"))
				return 0;
		}
	}

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type == FTYPE_PASSWORD) {
			if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
			    "@param v%zu %s (hashed password)", 
			    pos++, sent->fname))
				return 0;
		} else
			if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
			    "@param v%zu %s", pos++, sent->fname))
				return 0;
	}

	if (s->type == STYPE_ITERATE)
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_CLOSE,
		    "@param cb Callback with retrieved data."))
			return 0;

	if (s->type == STYPE_SEARCH) {
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_CLOSE,
		    "@return Result or null if no results found."))
			return 0;
	} else if (s->type == STYPE_LIST) {
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_CLOSE,
		    "@return Result of null if no results found."))
			return 0;
	} else if (s->type == STYPE_COUNT)
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_CLOSE,
		    "@return Count of results."))
			return 0;

	if (fputc('\t', f) == EOF)
		return 0;

	col = 8 + xprintf("db_%s_%s", 
		s->parent->name, stypes[s->type]);

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		col += xprintf("_by");
		TAILQ_FOREACH(sent, &s->sntq, entries)
			col += xprintf("_%s_%s", 
				sent->uname, optypes[sent->op]);
	} else if (s->name != NULL)
		col += xprintf("_%s", s->name);

	if (col >= 72) {
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
		col += xprintf("cb: (res: ortns.%s) => void", rs->name);
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
		puts("BigInt");

	puts("\t{");

	/* Now generate the method body. */

	printf("\t\tconst parms: any[] = [];\n"
	       "\t\tconst stmt: Database.Statement =\n"
	       "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	       "\t\t\t(ortstmt.ortstmt.STMT_%s_BY_SEARCH_%zu));\n"
	       "\t\tstmt.raw(true);\n"
	       "\n", s->parent->name, num);
	if (gen_rolemap(s->rolemap))
		puts("");

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type != FTYPE_PASSWORD ||
		    (sent->op == OPTYPE_STREQ ||
		     sent->op == OPTYPE_STRNEQ)) {
			printf("\t\tparms.push(v%zu);\n", pos++);
			continue;
		}
		if (sent->field->flags & FIELD_NULL)
			printf("\t\tif (v%zu === null)\n"
			       "\t\t\tparms.push(null);\n"
			       "\t\telse\n"
			       "\t\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos, pos);
		else
			printf("\t\tparms.push(bcrypt.hashSync"
				"(v%zu, bcrypt.genSaltSync()));\n", 
				pos);
		pos++;
	}
	
	puts("");

	switch (s->type) {
	case STYPE_SEARCH:
		printf("\t\tconst cols: any = stmt.get(parms);\n"
		       "\n"
		       "\t\tif (typeof cols === 'undefined')\n"
		       "\t\t\treturn null;\n"
		       "\t\tconst obj: ortns.%sData = \n"
		       "\t\t\tthis.db_%s_fill"
		       	"({row: <any[]>cols, pos: 0});\n",
		       rs->name, rs->name);
		if (rs->flags & STRCT_HAS_NULLREFS)
		       printf("\t\tthis.db_%s_reffind"
				"(this.#o, obj);\n", rs->name);
		printf("\t\treturn new ortns.%s(this.#role, obj);\n",
			rs->name);
		break;
	case STYPE_ITERATE:
		printf("\t\tfor (const cols of stmt.iterate(parms)) {\n"
		       "\t\t\tconst obj: ortns.%sData =\n"
		       "\t\t\t\tthis.db_%s_fill"
		       	"({row: <any>cols, pos: 0});\n",
		       rs->name, rs->name);
		if (rs->flags & STRCT_HAS_NULLREFS)
			printf("\t\t\tthis.db_%s_reffind"
				"(this.#o, obj);\n", rs->name);
		printf("\t\t\tcb(new ortns.%s(this.#role, obj));\n"
		       "\t\t}\n", rs->name);
		break;
	case STYPE_LIST:
		printf("\t\tconst rows: any[] = stmt.all(parms);\n"
		       "\t\tconst objs: ortns.%s[] = [];\n"
		       "\t\tlet i: number;\n"
		       "\n"
		       "\t\tfor (i = 0; i < rows.length; i++) {\n"
		       "\t\t\tconst obj: ortns.%sData =\n"
		       "\t\t\t\tthis.db_%s_fill"
		       	"({row: <any[]>rows[i], pos: 0});\n",
		       rs->name, rs->name, rs->name);
		if (rs->flags & STRCT_HAS_NULLREFS)
			printf("\t\t\tthis.db_%s_reffind"
				"(this.#o, obj);\n", rs->name);
		printf("\t\t\tobjs.push(new ortns.%s(this.#role, obj));\n"
		       "\t\t}\n"
		       "\t\treturn objs;\n", rs->name);
		break;
	case STYPE_COUNT:
		printf("\t\tconst cols: any = stmt.get(parms);\n"
		       "\n"
		       "\t\tif (typeof cols === 'undefined')\n"
		       "\t\t\tthrow \'count returned no result!?\';\n"
		       "\t\treturn BigInt(cols[0]);\n");
		break;
	default:
		break;
	}

	return fputs("\t}\n", f) != EOF;
}

/*
 * Generate the database functions for a structure.
 * Return zero on failure, non-zero on success.
 */
static int
gen_api(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct search	*s;
	const struct update	*u;
	size_t			 pos;

	gen_fill(p);
	gen_reffind(p);

	if (p->ins != NULL)
		gen_insert(p);
	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (!gen_query(f, cfg, s, pos++))
			return 0;
	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_modifier(cfg, u, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_modifier(cfg, u, pos++);

	return 1;
}

/*
 * Generate an enumeration.
 * Return zero on failure, non-zero on success.
 */
static int
gen_enm(FILE *f, const struct enm *p, size_t pos)
{
	const struct eitem	*ei;

	if (pos > 0 && fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS, p->doc))
		return 0;

	if (fprintf(f, "\texport enum %s {\n", p->name) < 0)
		return 0;

	TAILQ_FOREACH(ei, &p->eq, entries) {
		if (!gen_comment(f, 2, COMMENT_JS, ei->doc))
			return 0;
		if (fprintf(f, "\t\t%s = \'%" PRId64 "\'", 
		    ei->name, ei->value) < 0)
			return 0;
		if (TAILQ_NEXT(ei, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	return fputs("\t}\n", f) != EOF;
}

/*
 * Generate the interface for the structure and its export routines.
 * Return zero on failure, non-zero on success.
 */
static int
gen_strct(FILE *f, const struct strct *p, size_t pos)
{
	const struct field	*fd;
	const struct rref	*r;
	const char		*tab;

	if (pos > 0 && fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS, p->doc))
		return 0;
	if (fprintf(f, "\texport interface %sData {\n", p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (!gen_comment(f, 2, COMMENT_JS, fd->doc))
			return 0;
		if (fprintf(f, "\t\t%s: ", fd->name) < 0)
			return 0;
		if (fd->type == FTYPE_STRUCT) {
			if (fprintf(f, "ortns.%sData", 
			    fd->ref->target->parent->name) < 0)
				return 0;
		} else if (fd->type == FTYPE_ENUM) {
			if (fprintf(f, "ortns.%s", 
			    fd->enm->name) < 0)
				return 0;
		} else {
			if (fprintf(f, "%s", ftypes[fd->type]) < 0)
				return 0;
		}

		if (fd->flags & FIELD_NULL ||
		    (fd->type == FTYPE_STRUCT &&
		     (fd->ref->source->flags & FIELD_NULL)))
			if (fputs("|null", f) == EOF)
				return 0;

		if (fputs(";\n", f) == EOF)
			return 0;
	}

	if (fputs("\t}\n\n", f) == EOF)
		return 0;

	if (fprintf(f, "\tfunction db_export_%s"
	    "(role: string, obj: %sData): any\n"
	    "\t{\n"
	    "\t\tconst res: any = {}\n"
	    "\n", p->name, p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->flags & FIELD_NOEXPORT) {
			if (!gen_commentv(f, 2, COMMENT_JS,
			    "Don't output %s: noexport.", fd->name))
				return 0;
			continue;
		} else if (fd->type == FTYPE_PASSWORD) {
			if (!gen_commentv(f, 2, COMMENT_JS,
			    "Don't output %s: password.", fd->name))
				return 0;
			continue;
		}

		tab = "";
		if (fd->rolemap != NULL) {
			tab = "\t";
			if (fputs("\t\tswitch (role) {\n", f) == EOF)
				return 0;
			TAILQ_FOREACH(r, &fd->rolemap->rq, entries)
				gen_role(r->role, 2);
			if (!gen_comment(f, 3, COMMENT_JS, 
			    "Don't export field to noted roles."))
				return 0;
			if (fputs("\t\t\tbreak;\n"
			    "\t\tdefault:\n", f) == EOF)
				return 0;
		}
		
		/*
		 * If the type is a structure, then we need to convert
		 * that nested structure to an exportable as well.
		 * Otherwise, make sure we write all integers as strings
		 * (to preserve 64-bit-ness).
		 */

		if (fd->type == FTYPE_STRUCT) {
	     		if (fd->ref->source->flags & FIELD_NULL) {
				if (fprintf(f, "%s\t\tres[\'%s\'] = "
				    "(obj[\'%s\'] === null) ?\n"
				    "%s\t\t\tnull : db_export_%s"
				    "(role, obj[\'%s\'])\n",
				    tab, fd->name, fd->name, tab,
				    fd->ref->target->parent->name,
				    fd->name) < 0)
					return 0;
			} else {
				if (fprintf(f, "%s\t\tres[\'%s\'] = "
				    "db_export_%s"
				    "(role, obj[\'%s\'])\n",
				    tab, fd->name, 
				    fd->ref->target->parent->name,
				    fd->name) < 0)
					return 0;
			}
		} else {
			switch (fd->type) {
			case FTYPE_BIT:
			case FTYPE_DATE:
			case FTYPE_EPOCH:
			case FTYPE_INT:
			case FTYPE_REAL:
			case FTYPE_BITFIELD:
				if (!(fd->flags & FIELD_NULL)) {
					if (fprintf(f, 
					    "%s\t\tres[\'%s\'] = "
					    "obj[\'%s\']."
					    "toString();\n", tab, 
					    fd->name, fd->name) < 0)
						return 0;
					break;
				}
				if (fprintf(f, "%s\t\tres[\'%s\'] = "
				    "(obj[\'%s\'] === null) ?\n"
				    "%s\t\t\tnull : "
				    "obj[\'%s\'].toString();\n", 
				    tab, fd->name, fd->name, 
				    tab, fd->name) < 0)
					return 0;
				break;
			case FTYPE_BLOB:
				if (!(fd->flags & FIELD_NULL)) {
					if (fprintf(f, 
					    "%s\t\tres[\'%s\'] = new "
					    "obj[\'%s\'].toString"
					    "(\'base64\');\n", tab, 
					    fd->name, fd->name) < 0)
						return 0;
					break;
				}
				if (fprintf(f, "%s\t\tres[\'%s\'] = "
				    "(obj[\'%s\'] === null) ?\n"
				    "%s\t\t\tnull : "
				    "obj[\'%s\'].toString"
				    "(\'base64\');\n", 
				    tab, fd->name, fd->name, 
				    tab, fd->name) < 0)
					return 0;
				break;
			default:
				if (fprintf(f, "%s\t\tres[\'%s\'] = "
				    "obj[\'%s\'];\n",
				    tab, fd->name, fd->name) < 0)
					return 0;
				break;
			}
		}

		if (fd->rolemap != NULL &&
		    fputs("\t\t\tbreak;\n\t\t}\n", f) == EOF)
			return 0;
	}

	if (fputs("\n"
	    "\t\treturn res;\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_commentv(f, 1, COMMENT_JS,
	    "Class instance of {@link ortns.%sData}.",
	    p->name))
		return 0;
	if (fprintf(f, "\texport class %s {\n"
	    "\t\treadonly #role: string;\n"
	    "\t\treadonly obj: ortns.%sData;\n"
	    "\n", p->name, p->name) < 0)
		return 0;

	if (!gen_commentv(f, 2, COMMENT_JS,
	    "A {@link ortns.%sData} as extracted from the database "
	    "in a particular role.\n"
	    "@param role The role in which this was extracted "
	    "from the database. When exported, this role will be "
	    "checked for permission to export.\n"
	    "@param obj The raw data.", p->name))
		return 0;
	if (fprintf(f, "\t\tconstructor(role: string, obj: ortns.%sData)\n"
	    "\t\t{\n"
	    "\t\t\tthis.#role = role;\n"
	    "\t\t\tthis.obj = obj;\n"
	    "\t\t}\n"
	    "\n", p->name) < 0)
		return 0;

	if (!gen_commentv(f, 2, COMMENT_JS,
	    "Export the contained {@link ortns.%sData} respecting "
	    "fields not exported, roles, etc.  It's safe to call "
	    "`JSON.stringify()` on the returned object to write "
	    "responses.", p->name))
		return 0;

	return fprintf(f, "\t\texport(): any\n"
	       "\t\t{\n"
	       "\t\t\treturn db_export_%s(this.#role, this.obj);\n"
	       "\t\t}\n"
	       "\t}\n", p->name) > 0;
}

/*
 * This outputs the data structure part of the data model under the
 * "ortns" namespace.
 * It's divided primarily into data within interfaces and classes that
 * encapsulate that data and contain role information.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortns(FILE *f, const struct config *cfg)
{
	const struct strct	*p;
	const struct enm	*e;
	size_t			 i = 0;

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 0, COMMENT_JS,
	    "Namespace for data interfaces and representative "
	    "classes.  The interfaces are for the data itself, "
	    "while the classes manage roles and metadata."))
		return 0;
	if (fputs("export namespace ortns {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (!gen_enm(f, e, i++))
			return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_strct(f, p, i++))
			return 0;
	return fputs("}\n", f) != EOF;
}

/*
 * Output the class for managing a single connection.
 * This is otherwise defined as a single sequence of role transitions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortdb(FILE *f)
{

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 0, COMMENT_JS,
	    "Primary database object. "
	    "Only one of these should exist per running node.js "
	    "server."))
		return 0;
	if (fputs("export class ortdb {\n"
	    "\tdb: Database.Database;\n", f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "The ort-nodejs version used to produce this file."))
		return 0;
	if (fprintf(f, "\treadonly version: "
	    "string = \'%s\';\n", VERSION) < 0)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "The numeric (monotonically increasing) ort-nodejs "
	    "version used to produce this file."))
		return 0;
	if (fprintf(f, "\treadonly vstamp: number = %lld;\n"
	    "\n", (long long)VSTAMP) < 0)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "@param dbname The file-name of the database "
	    "relative to the running application."))
		return 0;
	if (fputs("\tconstructor(dbname: string) {\n"
	    "\t\tthis.db = new Database(dbname);\n"
	    "\t\tthis.db.defaultSafeIntegers(true);\n"
	    "\t}\n\n", f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "Create a connection to the database. "
	    "This should be called for each sequence "
	    "representing a single operator. "
	    "In web applications, for example, this should "
	    "be called for each request."))
		return 0;
	return fputs("\tconnect(): ortctx\n"
	     "\t{\n"
	     "\t\treturn new ortctx(this);\n"
	     "\t}\n"
	     "}\n", f) != EOF;
}

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 * Return zero on failure, non-zero on success.
 */
static int
gen_alias_builder(FILE *f, const struct strct *p)
{
	const struct field	*fd, *last = NULL;
	int			 first = 1;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type != FTYPE_STRUCT)
			last = fd;

	assert(last != NULL);
	if (fprintf(f, "\n"
	    "\tfunction ort_schema_%s(v: string): string\n"
	    "\t{\n"
	    "\t\treturn ", p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT)
			continue;
		if (!first &&
		    fputs("\t\t       ", f) == EOF)
			return 0;
		if (fprintf(f, "v + \'.%s\'", fd->name) < 0)
			return 0;
		if (fd != last) {
			if (fputs(" + \',\' +\n", f) == EOF)
				return 0;
		} else {
			if (fputs(";\n", f) == EOF)
				return 0;
		}
		first = 0;
	}

	return fputs("\t}\n", f) != EOF;
}

/*
 * For any given role, emit all of the possible transitions from the
 * current role into all possible roles, then all of the transitions
 * from the roles "beneath" the current role.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortctx_dbrole_role(FILE *f, const struct role *r)
{
	const struct role	*rr;

	if (fprintf(f, "\t\tcase '%s':\n"
	    "\t\t\tswitch(newrole) {\n", r->name) < 0)
		return 0;

	gen_role(r, 3);
	if (fputs("\t\t\t\tthis.#role = newrole;\n"
	    "\t\t\t\treturn;\n"
	    "\t\t\tdefault:\n"
	    "\t\t\t\tbreak;\n"
	    "\t\t\t}\n"
	    "\t\t\tbreak;\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_ortctx_dbrole_role(f, rr))
			return 0;

	return 1;
}

/*
 * Generate the dbRole role transition function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortctx_dbrole(FILE *f, const struct config *cfg)
{
	const struct role	*r, *rr;

	if (TAILQ_EMPTY(&cfg->rq))
		return 1;

	if (fputs("\n"
	    "\tdb_role(newrole: string): void\n"
	    "\t{\n"
	    "\t\tif (this.#role === 'none')\n"
	    "\t\t\tprocess.abort();\n"
	    "\t\tif (newrole === 'all')\n"
	    "\t\t\tprocess.abort();\n\n", f) == EOF)
		return 0;

	/* All possible descents from current into encompassed role. */

	if (fputs("\t\tswitch (this.#role) {\n"
	    "\t\tcase 'default':\n"
	    "\t\t\tthis.#role = newrole;\n"
	    "\t\t\treturn;\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (strcmp(r->name, "all") == 0)
			break;

	assert(r != NULL);
	TAILQ_FOREACH(rr, &r->subrq, entries) 
		if (!gen_ortctx_dbrole_role(f, rr))
			return 0;

	return fputs("\t\tdefault:\n"
	     "\t\t\tbreak;\n"
	     "\t\t}\n"
	     "\n"
	     "\t\tprocess.abort();\n"
	     "\t}\n", f) != EOF;
}

/*
 * Output the data access portion of the data model entirely within a
 * single class.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortctx(FILE *f, const struct config *cfg)
{
	const struct strct	*p;

	if (fputs("\n"
	    "namespace ortstmt {\n"
	    "\texport enum ortstmt {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		print_sql_enums(2, p, LANG_JS);
	if (fputs("\t}\n\n", f) == EOF)
		return 0;

	/* Convert enums to statements. */

	if (fputs("\texport function stmtBuilder(idx: ortstmt): string\n"
	    "\t{\n"
	    "\t\treturn ortstmts[idx];\n"
	    "\t}\n"
	    "\n"
	    "\tconst ortstmts: readonly string[] = [\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		print_sql_stmts(2, p, LANG_JS);
	if (fputs("\t];\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_alias_builder(f, p))
			return 0;
	if (fputs("}\n\n", f) == EOF)
		return 0;

	/* ortctx */

	if (!gen_comment(f, 0, COMMENT_JS,
	    "Manages all access to the database. "
	    "This object should be used for the lifetime of a "
	    "single \'request\', such as a request for a web "
	    "application."))
		return 0;

	if (fputs("export class ortctx {\n"
	    "\t#role: string = 'default';\n"
	    "\treadonly #o: ortdb;\n"
	    "\n"
	    "\tconstructor(o: ortdb) {\n"
	    "\t\tthis.#o = o;\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!TAILQ_EMPTY(&cfg->rq))
		if (fputs("\tdb_role_current(): string\n"
		    "\t{\n"
		    "\t\treturn this.#role;\n"
		    "\t}\n\n", f) == EOF)
			return 0;

	if (fputs("\tdb_trans_open_immediate(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION IMMEDIATE\');\n"
	    "\t}\n\n"
	    "\tdb_trans_open_deferred(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION DEFERRED\');\n"
	    "\t}\n\n"
	    "\tdb_trans_open_exclusive(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION EXCLUSIVE\');\n"
	    "\t}\n\n"
	    "\tdb_trans_rollback(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'ROLLBACK TRANSACTION\');\n"
	    "\t}\n\n"
	    "\tdb_trans_commit(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'COMMIT TRANSACTION\');\n"
	    "\t}\n", f) == EOF)
	    	return 0;

	if (!gen_ortctx_dbrole(f, cfg))
		return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_api(f, cfg, p))
			return 0;
	return fputs("}\n", f) != EOF;
}

int
gen_nodejs(const struct config *cfg, FILE *f)
{

	if (!gen_commentv(f, 0, COMMENT_JS, 
	    "WARNING: automatically generated by ort %s.\n"
	    "DO NOT EDIT!\n"
	    "@packageDocumentation", VERSION))
		return 0;

	if (fputs("\n"
	    "import bcrypt from 'bcrypt';\n"
	    "import Database from 'better-sqlite3';\n", f) == EOF)
		return 0;

	if (!gen_ortns(f, cfg))
		return 0;
	if (!gen_ortdb(f))
		return 0;
	if (!gen_ortctx(f, cfg))
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_JS,
	    "Instance an application-wide context. "
	    "This should only be called once per server, with "
	    "the {@link ortdb.connect} method used for "
	    "sequences of operations."))
		return 0;

	return fputs("export function ort(dbname: string): ortdb\n"
	     "{\n"
	     "\treturn new ortdb(dbname);\n"
	     "}\n", f) != EOF;
}
