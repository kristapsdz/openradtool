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
	"BigInt", /* FTYPE_BIT */
	"BigInt", /* FTYPE_DATE */
	"BigInt", /* FTYPE_EPOCH */
	"BigInt", /* FTYPE_INT */
	"BigInt", /* FTYPE_REAL */
	"ArrayBuffer", /* FTYPE_BLOB */
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
			       "\t\t\tparms.push(obj.%s);\n"
		               "\t\t\tcols = stmt.get(parms);\n"
			       "\t\t\tif (typeof cols === \'undefined\')\n"
			       "\t\t\t\tprocess.abort();\n"
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

	TAILQ_FOREACH(f, &p->fq, entries) {
		switch (f->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			printf("\t\t\t'%s': BigInt(data.row[data.pos++]),\n",
				f->name);
			break;
		case FTYPE_REAL:
			printf("\t\t\t'%s': <number%s>"
				"data.row[data.pos++],\n",
				f->name, (f->flags & FIELD_NULL) ? 
				"|null" : "");
			break;
		case FTYPE_BLOB:
			/* TODO. */
			break;
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
		case FTYPE_EMAIL:
			printf("\t\t\t'%s': <string%s>"
				"data.row[data.pos++],\n",
				f->name, (f->flags & FIELD_NULL) ? 
				"|null" : "");
			break;
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
			printf("\t\t\t'%s': <ortns.%s>data.row[data.pos++],\n",
				f->name, f->enm->name);
			break;
		default:
			abort();
		}
	}

	puts("\t\t};");

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

	print_commentt(1, ct,
		"@return False on constraint violation, "
		"true on success.");

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
		fputs("\n\t\tboolean", stdout);
	else
		fputs(" boolean", stdout);

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

	puts("\n"
	     "\t\ttry {\n"
	     "\t\t\tinfo = stmt.run(parms);\n"
	     "\t\t} catch (er) {\n"
	     "\t\t\treturn false;\n"
	     "\t\t}\n"
	     "\n"
	     "\t\treturn true;\n"
	     "\t}");
}

/*
 * Generate a query function.
 */
static void
gen_query(const struct config *cfg,
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

	puts("");
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

	putchar('\t');
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
		puts("number");

	puts("\t{");

	/* Now generate the method body. */

	if (s->type == STYPE_SEARCH)
		printf("\t\tlet cols: any;\n"
		       "\t\tlet obj: ortns.%sData;\n",
		       rs->name);

	printf("\t\tconst parms: any[] = [];\n"
	       "\t\tconst stmt: Database.Statement =\n"
	       "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	       "\t\t\t(ortstmt.ortstmt.STMT_%s_BY_SEARCH_%zu));\n"
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

	if (s->type == STYPE_SEARCH) {
		printf("\n"
		       "\t\tcols = stmt.get(parms);\n"
		       "\t\tif (typeof cols === 'undefined')\n"
		       "\t\t\treturn null;\n"
		       "\t\tobj = this.db_%s_fill"
			"({row: <any[]>cols, pos: 0});\n",
		       rs->name);
		if (rs->flags & STRCT_HAS_NULLREFS)
		       printf("\t\tthis.db_%s_reffind"
				"(this.#o, obj);\n", rs->name);
		printf("\t\treturn new ortns.%s(this.#role, obj)\n",
			rs->name);
	}

	if (s->type == STYPE_LIST)
		puts("\t\treturn [];");
	if (s->type == STYPE_COUNT)
		puts("\t\treturn 0;");

	puts("\t}");
}

static void
gen_api(const struct config *cfg, const struct strct *p)
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
		gen_query(cfg, s, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_modifier(cfg, u, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_modifier(cfg, u, pos++);
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
			printf("ortns.%sData", 
				f->ref->target->parent->name);
		else if (f->type == FTYPE_ENUM)
			printf("ortns.%s", f->enm->name);
		else
			printf("%s", ftypes[f->type]);

		if (f->flags & FIELD_NULL ||
		    (f->type == FTYPE_STRUCT &&
		     (f->ref->source->flags & FIELD_NULL)))
			printf("|null");

		puts(";");
	}
	puts("\t}\n");

	print_commentv(1, COMMENT_JS,
		"Class instance of {@link ortns.%sData}.",
		p->name);

	printf("\texport class %s {\n"
	       "\t\t#role: string;\n"
	       "\t\treadonly obj: ortns.%sData;\n"
	       "\n"
	       "\t\tconstructor(role: string, obj: ortns.%sData)\n"
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
	       "\tdb: Database.Database;\n"
	       "\treadonly version: string = \'%s\';\n"
	       "\treadonly vstamp: number = %lld;\n"
	       "\n",
	       VERSION, (long long)VSTAMP);
	print_commentt(1, COMMENT_JS,
		"@param dbname The file-name of the database "
		"relative to the running application.");
	puts("\tconstructor(dbname: string) {\n"
	     "\t\tthis.#dbname = dbname;\n"
	     "\t\tthis.db = new Database(dbname);\n"
	     "\t\tthis.db.defaultSafeIntegers(true);\n"
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
	       "\tfunction ort_schema_%s(v: string): string\n"
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
 * For any given role, emit all of the possible transitions from the
 * current role into all possible roles, then all of the transitions
 * from the roles "beneath" the current role.
 */
static void
gen_ortctx_dbrole_role(const struct role *r)
{
	const struct role	*rr;

	printf("\t\tcase '%s':\n"
	       "\t\t\tswitch(newrole) {\n",
	       r->name);
	gen_role(r, 3);
	puts("\t\t\t\tthis.#role = newrole;\n"
	     "\t\t\t\treturn;\n"
	     "\t\t\tdefault:\n"
	     "\t\t\t\tbreak;\n"
	     "\t\t\t}\n"
	     "\t\t\tbreak;");
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_ortctx_dbrole_role(rr);
}

/*
 * Generate the dbRole role transition function.
 */
static void
gen_ortctx_dbrole(const struct config *cfg)
{
	const struct role	*r, *rr;

	if (TAILQ_EMPTY(&cfg->rq))
		return;

	puts("\n"
	     "\tdbRole(newrole: string): void\n"
	     "\t{\n"
	     "\t\tif (this.#role === 'none')\n"
	     "\t\t\tprocess.abort();\n"
	     "\t\tif (newrole === 'all')\n"
	     "\t\t\tprocess.abort();\n");

	/* All possible descents from current into encompassed role. */

	puts("\t\tswitch (this.#role) {\n"
	     "\t\tcase 'default':\n"
	     "\t\t\tthis.#role = newrole;\n"
	     "\t\t\treturn;");

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (strcmp(r->name, "all") == 0)
			break;

	assert(r != NULL);
	TAILQ_FOREACH(rr, &r->subrq, entries) 
		gen_ortctx_dbrole_role(rr);

	puts("\t\tdefault:\n"
	     "\t\t\tbreak;\n"
	     "\t\t}\n"
	     "\n"
	     "\t\tprocess.abort();\n"
	     "\t}");
}

/*
 * Output the data access portion of the data model entirely within a
 * single class.
 */
static void
gen_ortctx(const struct config *cfg)
{
	const struct strct	*p;

	puts("\n"
	     "namespace ortstmt {\n"
	     "\texport enum ortstmt {");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		print_sql_enums(2, p, LANG_JS);
	puts("\t}\n"
	     "\n"
	     "\texport function stmtBuilder(idx: ortstmt): string\n"
	     "\t{\n"
	     "\t\treturn ortstmts[idx];\n"
	     "\t}\n"
	     "\n"
	     "\tconst ortstmts: readonly string[] = [");
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
	puts("export class ortctx {");
	if (!TAILQ_EMPTY(&cfg->rq))
		puts("\t#role: string = 'default';");
	puts("\treadonly #o: ortdb;\n"
	     "\n"
	     "\tconstructor(o: ortdb) {\n"
	     "\t\tthis.#o = o;\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransImmediate(id: number): void\n"
	     "\t{\n"
	     "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION IMMEDIATE\');\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransDeferred(id: number): void\n"
	     "\t{\n"
	     "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION DEFERRED\');\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransExclusive(id: number): void\n"
	     "\t{\n"
	     "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION EXCLUSIVE\');\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransRollback(id: number): void\n"
	     "\t{\n"
	     "\t\tthis.#o.db.exec(\'ROLLBACK TRANSACTION\');\n"
	     "\t}\n"
	     "\n"
	     "\tdbTransCommit(id: number): void\n"
	     "\t{\n"
	     "\t\tthis.#o.db.exec(\'COMMIT TRANSACTION\');\n"
	     "\t}");
	gen_ortctx_dbrole(cfg);
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

	puts("\n"
	     "import bcrypt from 'bcrypt';\n"
	     "import Database from 'better-sqlite3';");

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
