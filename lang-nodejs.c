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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-nodejs.h"
#include "ort-version.h"
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

static const char *const vtypes[VALIDATE__MAX] = {
	">=", /* VALIDATE_GE */
	"<=", /* VALIDATE_LE */
	">", /* VALIDATE_GT */
	"<", /* VALIDATE_LT */
	"===", /* VALIDATE_EQ */
};

/*
 * Generate variable vNN where NN is position "pos" (from one) with the
 * appropriate type in a method signature.
 * This will start with a comma if not the first variable.
 * Return <0 on fail, >0 for columns printed.
 */
static int
gen_var(FILE *f, size_t pos, size_t col, const struct field *fd)
{
	int	 rc;

	if (pos > 1) {
		if (fputc(',', f) == EOF)
			return -1;
		col++;
	}

	if (col >= 72) {
		if (fputs("\n\t\t", f) == EOF)
			return -1;
		col = 16;
	} else if (pos > 1) {
		if (fputc(' ', f) == EOF)
			return -1;
		col++;
	}

	if ((rc = fprintf(f, "v%zu: ", pos)) < 0)
		return -1;
	col += rc;

	rc = fd->type == FTYPE_ENUM ?
		fprintf(f, "ortns.%s", fd->enm->name) :
		fprintf(f, "%s", ftypes[fd->type]);
	if (rc < 0)
		return -1;
	col += rc;

	if ((fd->flags & FIELD_NULL) ||
	    (fd->type == FTYPE_STRUCT &&
	     (fd->ref->source->flags & FIELD_NULL))) {
		if ((rc = fprintf(f, "|null")) < 0)
			return 0;
		col += rc;
	}

	assert(col > 0 && col < INT_MAX);
	return (int)col;
}

/*
 * Generate role name (if not all) and recursively descend.
 * Return zero on failure, non-zero on success.
 */
static int
gen_role(FILE *f, const struct role *r, size_t tabs)
{
	const struct role	*rr;
	size_t			 i;

	if (strcmp(r->name, "all")) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "case '%s\':\n", r->name) < 0)
			return 0;
	}

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_role(f, rr, tabs))
			return 0;
	return 1;
}

/*
 * Recursively generate all roles allowed by this rolemap.
 * Return <0 on failure, >0 if we wrote something, 0 otherwise.
 */
static int
gen_rolemap(FILE *f, const struct rolemap *rm)
{
	const struct rref	*rr;

	if (rm == NULL)
		return 0;

	if (fputs("\t\tswitch (this.#role) {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(rr, &rm->rq, entries)
		if (!gen_role(f, rr->role, 2))
			return 0;

	return fputs("\t\t\tbreak;\n"
	     "\t\tdefault:\n"
	     "\t\t\tprocess.abort();\n"
	     "\t\t}\n", f) != EOF;
}

/*
 * Generate db_xxx_reffind method (if applicable).
 * Return zero on failure, non-zero on success or non-applicable.
 */
static int
gen_reffind(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	size_t			 col;
	int			 rc;

	if (!(p->flags & STRCT_HAS_NULLREFS))
		return 1;

	/* 
	 * Do we have any null-ref fields in this?
	 * (They might be in target references.)
	 */

	TAILQ_FOREACH(fd, &p->fq, entries)
		if ((fd->type == FTYPE_STRUCT) &&
		    (fd->ref->source->flags & FIELD_NULL))
			break;

	if (fputs("\n\t", f) == EOF)
		return 0;
	if ((rc = fprintf(f, "private db_%s_reffind", p->name)) < 0)
		return 0;
	col = 8 + rc;

	if (col >= 72) {
		if (fputs("\n\t(", f) == EOF)
			return 0;
		col = 9;
	} else {
		if (fputc('(', f) == EOF)
			return 0;
		col++;
	}

	if (fprintf(f, "db: ortdb, "
	    "obj: ortns.%sData): void\n\t{\n", p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		if (fd->ref->source->flags & FIELD_NULL) {
			if (fprintf(f, "\t\tif (obj.%s !== null) {\n"
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
			    fd->ref->source->name,
			    fd->ref->target->parent->name,
			    fd->ref->target->name,
			    fd->ref->source->name,
			    fd->name,
			    fd->ref->target->parent->name) < 0)
				return 0;
		}
		if (!(fd->ref->target->parent->flags & 
		    STRCT_HAS_NULLREFS))
			continue;
		if (fprintf(f, "\t\tthis.db_%s_reffind(db, obj.%s);\n",
		    fd->ref->target->parent->name, fd->name) < 0)
			return 0;
	}

	return fputs("\t}\n", f) != EOF;
}

/*
 * Generate db_xxx_fill method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_fill(FILE *f, const struct strct *p)
{
	size_t	 		 col;
	const struct field	*fd;
	int			 rc;

	if (fputs("\n\t", f) == EOF)
		return 0;
	if ((rc = fprintf(f, "private db_%s_fill", p->name)) < 0)
		return 0;
	col = 8 + rc;

	if (col >= 72) {
		if (fputs("\n\t(", f) == EOF)
			return 0;
		col = 9;
	} else {
		if (fputc('(', f) == EOF)
			return 0;
		col++;
	}

	if ((rc = fprintf(f, 
	    "data: {row: any[], pos: number}):")) < 0)
		return 0;
	col += rc;

	if (col + strlen(p->name) + 13 >= 72) {
		if (fputs("\n\t\t", f) == EOF)
			return 0;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
	}

	if (fprintf(f, "ortns.%sData\n\t{\n"
	    "\t\tconst obj: ortns.%sData = {\n", 
	    p->name, p->name) < 0)
		return 0;

	col = 0;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		switch (fd->type) {
		case FTYPE_STRUCT:
			if (fputs("\t\t\t/* A dummy value "
			    "for now. */\n", f) == EOF)
				return 0;
			if (fd->ref->source->flags & FIELD_NULL) {
				if (fprintf(f, "\t\t\t'%s': "
				    "null,\n", fd->name) < 0)
					return 0;
				break;
			}
			if (fprintf(f, "\t\t\t'%s': "
			    "<ortns.%sData>{},\n", fd->name, 
			    fd->ref->target->parent->name) < 0)
				return 0;
			break;
		case FTYPE_ENUM:
			/*
			 * Convert these to a string because our
			 * internal representation is 64-bit but numeric
			 * enumerations are constraint to 53.
			 */
			if (fprintf(f, "\t\t\t'%s': <ortns.%s%s>",
			    fd->name, fd->enm->name,
			    (fd->flags & FIELD_NULL) ? 
			    "|null" : "") < 0)
				return 0;
			if (fd->flags & FIELD_NULL) {
				if (fprintf(f, 
				    "(data.row[data.pos + %zu] === "
				    "null ?\n\t\t\t\tnull : "
				    "data.row[data.pos + %zu]."
				    "toString()),\n", col, col) < 0)
					return 0;
			} else {
				if (fprintf(f, 
				    "data.row[data.pos + %zu]."
				    "toString(),\n", col) < 0)
					return 0;
			}
			break;
		default:
			assert(ftypes[fd->type] != NULL);
			if (fprintf(f, "\t\t\t'%s': <%s%s>"
			    "data.row[data.pos + %zu],\n",
			    fd->name, ftypes[fd->type],
			    (fd->flags & FIELD_NULL) ? 
			    "|null" : "", col) < 0)
				return 0;
			break;
		}
		if (fd->type != FTYPE_STRUCT)
			col++;
	}

	if (fprintf(f, "\t\t};\n"
	    "\t\tdata.pos += %zu;\n", col) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type == FTYPE_STRUCT &&
		    !(fd->ref->source->flags & FIELD_NULL))
			if (fprintf(f, "\t\tobj.%s = "
			    "this.db_%s_fill(data);\n", fd->name, 
			    fd->ref->target->parent->name) < 0)
				return 0;

	return fputs("\t\treturn obj;\n\t}\n", f) != EOF;
}

/*
 * Generate db_xxxx_insert method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_insert(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	size_t	 	 	 pos = 1, col;
	int			 rc;

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS_FRAG_OPEN,
	    "Insert a new row into the database. Only "
	    "native (and non-rowid) fields may be set."))
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;
		if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
		    "@param v%zu %s", pos++, fd->name))
			return 0;
	}
	if (!gen_comment(f, 1, COMMENT_JS_FRAG_CLOSE,
	    "@return New row's identifier on success or "
	    "<0 otherwise."))
		return 0;

	if (fputc('\t', f) == EOF)
		return 0;
	if ((rc = fprintf(f, "db_%s_insert", p->name)) < 0)
		return 0;
	col = 8 + rc;

	if (col >= 72) {
		if (fputs("\n\t(", f) == EOF)
			return 0;
		col = 9;
	} else {
		if (fputc('(', f) == EOF)
			return 0;
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!(fd->type == FTYPE_STRUCT || 
		      (fd->flags & FIELD_ROWID))) {
			if ((rc = gen_var(f, pos++, col, fd)) < 0)
				return 0;
			col = rc;
		}

	if (fputs("):", f) == EOF)
		return 0;

	if (col + 7 >= 72) {
		if (fputs("\n\t\tBigInt", f) == EOF)
			return 0;
	} else {
		if (fputs(" BigInt", f) == EOF)
			return 0;
	}

	if (fprintf(f, "\n"
	    "\t{\n"
	    "\t\tconst parms: any[] = [];\n"
	    "\t\tlet info: Database.RunResult;\n"
	    "\t\tconst stmt: Database.Statement =\n"
	    "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	    "\t\t\t(ortstmt.ortstmt.STMT_%s_INSERT));\n"
	    "\n", p->name) < 0)
		return 0;

	if ((rc = gen_rolemap(f, p->ins->rolemap)) < 0)
		return 0;
	else if (rc > 0 && fputc('\n', f) == EOF)
		return 0;

	pos = 1;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;

		/* 
		 * Passwords are special-cased below the switch and we
		 * need to convert bitfields (individual bits and named
		 * fields) into a signed representation else high bits
		 * will trip range errors.
		 */

		switch (fd->type) {
		case FTYPE_PASSWORD:
			break;
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
			if (fd->flags & FIELD_NULL) {
				if (fprintf(f, "\t\tparms.push"
				    "(v%zu === null ? null : "
				    "BigInt.asIntN(64, v%zu));\n",
				    pos, pos) < 0)
					return 0;
			} else
				if (fprintf(f, 
				    "\t\tparms.push"
				    "(BigInt.asIntN(64, v%zu));\n", 
				    pos) < 0)
					return 0;
			pos++;
			continue;
		default:
			if (fprintf(f, 
			    "\t\tparms.push(v%zu);\n", pos++) < 0)
				return 0;
			continue;
		}

		/* Handle password. */

		if (fd->flags & FIELD_NULL) {
			if (fprintf(f, "\t\tif (v%zu === null)\n"
			    "\t\t\tparms.push(null);\n"
			    "\t\telse\n"
			    "\t\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos, pos) < 0)
				return 0;
		} else {
			if (fprintf(f, "\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos) < 0)
				return 0;
		}
		pos++;
	}

	return fputs("\n"
	     "\t\ttry {\n"
	     "\t\t\tinfo = stmt.run(parms);\n"
	     "\t\t} catch (er) {\n"
	     "\t\t\treturn BigInt(-1);\n"
	     "\t\t}\n"
	     "\n"
	     "\t\treturn BigInt(info.lastInsertRowid);\n"
	     "\t}\n", f) != EOF;
}

/*
 * Generate db_xxx_delete or db_xxx_update method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_update(FILE *f, const struct config *cfg,
	const struct update *up, size_t num)
{
	const struct uref	*ref;
	enum cmtt		 ct = COMMENT_JS_FRAG_OPEN;
	size_t		 	 pos = 1, col;
	int			 hasunary = 0, rc;

	/* Do we document non-parameterised constraints? */

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (OPTYPE_ISUNARY(ref->op)) {
			hasunary = 1;
			break;
		}

	/* Documentation. */

	if (fputc('\n', f) == EOF)
		return 0;
	if (up->doc != NULL) {
		if (!gen_comment(f, 1, COMMENT_JS_FRAG_OPEN, up->doc))
			return 0;
		ct = COMMENT_JS_FRAG;
	}

	if (hasunary) { 
		if (!gen_comment(f, 1, ct,
		    "The following fields are constrained by "
		    "unary operations:"))
			return 0;
		TAILQ_FOREACH(ref, &up->crq, entries) {
			if (OPTYPE_ISUNARY(ref->op))
				continue;
			if (!gen_commentv(f, 1, COMMENT_JS_FRAG,
			    "%s (checked %s null)", ref->field->name, 
			    ref->op == OPTYPE_NOTNULL ? "not" : "is"))
				return 0;
			ct = COMMENT_JS_FRAG;
		}
	}

	if (up->type == UP_MODIFY)
		TAILQ_FOREACH(ref, &up->mrq, entries) {
			if (ref->field->type == FTYPE_PASSWORD) {
				if (!gen_commentv(f, 1, ct,
				    "@param v%zu update %s "
				    "(hashed)", pos++, 
				    ref->field->name))
					return 0;
			} else {
				if (!gen_commentv(f, 1, ct,
				    "@param v%zu update %s",
				    pos++, ref->field->name))
					return 0;
			}
			ct = COMMENT_JS_FRAG;
		}

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (!OPTYPE_ISUNARY(ref->op)) {
			if (!gen_commentv(f, 1, ct,
			    "@param v%zu constraint %s (%s)", pos++, 
			    ref->field->name, optypes[ref->op]))
				return 0;
			ct = COMMENT_JS_FRAG;
		}

	if (ct == COMMENT_JS_FRAG_OPEN)
		ct = COMMENT_JS;
	else
		ct = COMMENT_JS_FRAG_CLOSE;

	if (up->type == UP_MODIFY) {
		if (!gen_comment(f, 1, ct,
		    "@return False on constraint violation, "
		    "true on success."))
			return 0;
	} else {
		if (!gen_comment(f, 1, ct, ""))
			return 0;
	}

	/* Method signature. */

	if (fputc('\t', f) == EOF)
		return 0;
	if ((rc = fprintf(f, "db_%s_%s",
	    up->parent->name, utypes[up->type])) < 0)
		return 0;
	col = 8 + rc;

	if (up->name == NULL && up->type == UP_MODIFY) {
		if (!(up->flags & UPDATE_ALL))
			TAILQ_FOREACH(ref, &up->mrq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ref->field->name, 
					modtypes[ref->mod]);
				if (rc < 0)
					return 0;
				col += rc;
			}
		if (!TAILQ_EMPTY(&up->crq)) {
			if ((rc = fprintf(f, "_by")) < 0)
				return 0;
			col += rc;
			TAILQ_FOREACH(ref, &up->crq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ref->field->name, 
					optypes[ref->op]);
				if (rc < 0)
					return 0;
				col += rc;
			}
		}
	} else if (up->name == NULL) {
		if (!TAILQ_EMPTY(&up->crq)) {
			if ((rc = fprintf(f, "_by")) < 0)
				return 0;
			col += rc;
			TAILQ_FOREACH(ref, &up->crq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ref->field->name, 
					optypes[ref->op]);
				if (rc < 0)
				col += rc;
			}
		}
	} else {
		if ((rc = fprintf(f, "_%s", up->name)) < 0)
			return 0;
		col += rc;
	}

	if (col >= 72) {
		if (fputs("\n\t(", f) == EOF)
			return 0;
		col = 9;
	} else {
		if (fputc('(', f) == EOF)
			return 0;
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if ((rc = gen_var(f, pos++, col, ref->field)) < 0)
			return 0;
		col = rc;
	}
	TAILQ_FOREACH(ref, &up->crq, entries)
		if (!OPTYPE_ISUNARY(ref->op)) {
			if ((rc = gen_var
			    (f, pos++, col, ref->field)) < 0)
				return 0;
			col = rc;
		}

	if (fputs("):", f) == EOF)
		return 0;
	if (col + 7 >= 72) {
		if (fputs("\n\t\t", f) == EOF)
			return 0;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
	}

	if (fputs(up->type == UP_MODIFY ? 
	    "boolean" : "void", f) == EOF)
		return 0;

	/* Method body. */

	if (fprintf(f, "\n"
	    "\t{\n"
	    "\t\tconst parms: any[] = [];\n"
	    "\t\tlet info: Database.RunResult;\n"
	    "\t\tconst stmt: Database.Statement =\n"
	    "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	    "\t\t\t(ortstmt.ortstmt.STMT_%s_%s_%zu));\n"
	    "\n", 
	    up->parent->name,
	    up->type == UP_MODIFY ? "UPDATE" : "DELETE",
	    num) < 0)
		return 0;

	if ((rc = gen_rolemap(f, up->rolemap)) < 0)
		return 0;
	else if (rc > 0 && fputc('\n', f) == EOF)
		return 0;

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		/* 
		 * Passwords are special-cased below the switch (unless
		 * they're string-setting) and we need to convert
		 * bitfields (individual bits and named fields) into a
		 * signed representation: unsigned can exceed range.
		 */

		switch (ref->field->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
			if (ref->field->flags & FIELD_NULL) {
				if (fprintf(f, "\t\tparms.push"
				    "(v%zu === null ? null : "
				    "BigInt.asIntN(64, v%zu));\n",
				    pos, pos) < 0)
					return 0;
			} else
				if (fprintf(f, 
				    "\t\tparms.push"
				    "(BigInt.asIntN(64, v%zu));\n", 
				    pos) < 0)
					return 0;
			pos++;
			continue;
		case FTYPE_PASSWORD:
			if (ref->mod != MODTYPE_STRSET)
				break;
			/* FALLTHROUGH */
		default:
			if (fprintf(f, 
			    "\t\tparms.push(v%zu);\n", pos++) < 0)
				return 0;
			continue;
		}

		if (ref->field->flags & FIELD_NULL) {
			if (fprintf(f, "\t\tif (v%zu === null)\n"
			    "\t\t\tparms.push(null);\n"
			    "\t\telse\n"
			    "\t\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos, pos) < 0)
				return 0;
		} else {
			if (fprintf(f, "\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos) < 0)
				return 0;
		}
		pos++;
	}

	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		switch (ref->field->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
			if (ref->field->flags & FIELD_NULL) {
				if (fprintf(f, "\t\tparms.push"
				    "(v%zu === null ? null : "
				    "BigInt.asIntN(64, v%zu));\n",
				    pos, pos) < 0)
					return 0;
			} else
				if (fprintf(f, 
				    "\t\tparms.push"
				    "(BigInt.asIntN(64, v%zu));\n", 
				    pos) < 0)
					return 0;
			pos++;
			break;
		default:
			if (fprintf(f, 
			    "\t\tparms.push(v%zu);\n", pos++) < 0)
				return 0;
			continue;
		}
	}

	if (up->type == UP_MODIFY) {
		if (fputs("\n"
		    "\t\ttry {\n"
		    "\t\t\tinfo = stmt.run(parms);\n"
		    "\t\t} catch (er) {\n"
		    "\t\t\treturn false;\n"
		    "\t\t}\n"
		    "\n"
		    "\t\treturn true;\n", f) == EOF)
			return 0;
	} else {
		if (fputs("\n"
		    "\t\tstmt.run(parms);\n", f) == EOF)
			return 0;
	}

	return fputs("\t}\n", f) != EOF;
}

/*
 * Generate db_xxx_{get,count,list,iterate} method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_query(FILE *f, const struct config *cfg,
	const struct search *s, size_t num)
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

	if ((rc = fprintf(f, "db_%s_%s", 
	    s->parent->name, stypes[s->type])) < 0)
		return 0;
	col = 8 + rc;

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		if ((rc = fprintf(f, "_by")) < 0)
			return 0;
		col += rc;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if ((rc = fprintf(f, "_%s_%s", 
			    sent->uname, optypes[sent->op])) < 0)
				return 0;
			col += rc;
		}
	} else if (s->name != NULL) {
		if ((rc = fprintf(f, "_%s", s->name)) < 0)
			return 0;
		col += rc;
	}

	if (col >= 72) {
		if (fputs("\n\t(", f) == EOF)
			return 0;
		col = 9;
	} else {
		if (fputc('(', f) == EOF)
			return 0;
		col++;
	}

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (!OPTYPE_ISUNARY(sent->op)) {
			if ((rc = gen_var
			    (f, pos++, col, sent->field)) < 0)
				return 0;
			col = rc;
		}

	if (s->type == STYPE_ITERATE) {
		sz = strlen(rs->name) + 25;
		if (pos > 1 && fputc(',', f) == EOF)
			return 0;
		if (col + sz >= 72) {
			if (fputs("\n\t\t", f) == EOF)
				return 0;
			col = 16;
		} else if (pos > 1) {
			if (fputc(' ', f) == EOF)
				return 0;
			col += 2;
		}
		if ((rc = fprintf(f, "cb: "
		    "(res: ortns.%s) => void", rs->name)) < 0)
			return 0;
		col += rc;
	}

	if (fputs("): ", f) == EOF)
		return 0;

	if (s->type == STYPE_SEARCH)
		sz = strlen(rs->name) + 11;
	else if (s->type == STYPE_LIST)
		sz = strlen(rs->name) + 8;
	else if (s->type == STYPE_ITERATE)
		sz = 4;
	else
		sz = 6;

	if (col + sz >= 72 && fputs("\n\t\t", f) == EOF)
		return 0;

	if (s->type == STYPE_SEARCH) {
		if (fprintf(f, "ortns.%s|null\n", rs->name) < 0)
			return 0;
	} else if (s->type == STYPE_LIST) {
		if (fprintf(f, "ortns.%s[]\n", rs->name) < 0)
			return 0;
	} else if (s->type == STYPE_ITERATE) {
		if (fputs("void\n", f) == EOF)
			return 0;
	} else {
		if (fputs("BigInt\n", f) == EOF)
			return 0;
	}

	if (fputs("\t{\n", f) == EOF)
		return 0;

	/* Now generate the method body. */

	if (fprintf(f, "\t\tconst parms: any[] = [];\n"
	    "\t\tconst stmt: Database.Statement =\n"
	    "\t\t\tthis.#o.db.prepare(ortstmt.stmtBuilder\n"
	    "\t\t\t(ortstmt.ortstmt.STMT_%s_BY_SEARCH_%zu));\n"
	    "\t\tstmt.raw(true);\n"
	    "\n", s->parent->name, num) < 0)
		return 0;
	if ((rc = gen_rolemap(f, s->rolemap)) < 0)
		return 0;
	else if (rc > 0 && fputc('\n', f) == EOF)
		return 0;

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;

		/* 
		 * Passwords are special-cased below the switch (unless
		 * they're streq/strneq) and we need to convert
		 * bitfields (individual bits and named fields) into a
		 * signed representation: unsigned can exceed range.
		 */

		switch (sent->field->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
			if (sent->field->flags & FIELD_NULL) {
				if (fprintf(f, "\t\tparms.push"
				    "(v%zu === null ? null : "
				    "BigInt.asIntN(64, v%zu));\n",
				    pos, pos) < 0)
					return 0;
			} else
				if (fprintf(f, 
				    "\t\tparms.push"
				    "(BigInt.asIntN(64, v%zu));\n", 
				    pos) < 0)
					return 0;
			pos++;
			continue;
		case FTYPE_PASSWORD:
			if (sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ)
				break;
			/* FALLTHROUGH */
		default:
			if (fprintf(f, 
			    "\t\tparms.push(v%zu);\n", pos++) < 0)
				return 0;
			continue;
		}

		if (sent->field->flags & FIELD_NULL) {
			if (fprintf(f, "\t\tif (v%zu === null)\n"
			    "\t\t\tparms.push(null);\n"
			    "\t\telse\n"
			    "\t\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos, pos) < 0)
				return 0;
		} else {
			if (fprintf(f, "\t\tparms.push(bcrypt.hashSync"
			    "(v%zu, bcrypt.genSaltSync()));\n", 
			    pos) < 0)
				return 0;
		}
		pos++;
	}
	
	if (pos > 1 && fputc('\n', f) == EOF)
		return 0;

	switch (s->type) {
	case STYPE_SEARCH:
		if (fprintf(f, "\t\tconst cols: any = stmt.get(parms);\n"
		    "\n"
		    "\t\tif (typeof cols === 'undefined')\n"
		    "\t\t\treturn null;\n"
		    "\t\tconst obj: ortns.%sData = \n"
		    "\t\t\tthis.db_%s_fill"
		    "({row: <any[]>cols, pos: 0});\n",
		    rs->name, rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS) {
		       if (fprintf(f, "\t\tthis.db_%s_reffind"
			   "(this.#o, obj);\n", rs->name) < 0)
			       return 0;
		}
		if (fprintf(f, "\t\treturn new "
	  	    "ortns.%s(this.#role, obj);\n", rs->name) < 0)
			return 0;
		break;
	case STYPE_ITERATE:
		if (fprintf(f, 
		    "\t\tfor (const cols of stmt.iterate(parms)) {\n"
		    "\t\t\tconst obj: ortns.%sData =\n"
		    "\t\t\t\tthis.db_%s_fill"
		    "({row: <any>cols, pos: 0});\n",
		    rs->name, rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS) {
			if (fprintf(f, "\t\t\tthis.db_%s_reffind"
			    "(this.#o, obj);\n", rs->name) < 0)
				return 0;
		}
		if (fprintf(f, 
		    "\t\t\tcb(new ortns.%s(this.#role, obj));\n"
		    "\t\t}\n", rs->name) < 0)
			return 0;
		break;
	case STYPE_LIST:
		if (fprintf(f, 
		    "\t\tconst rows: any[] = stmt.all(parms);\n"
		    "\t\tconst objs: ortns.%s[] = [];\n"
		    "\t\tlet i: number;\n"
		    "\n"
		    "\t\tfor (i = 0; i < rows.length; i++) {\n"
		    "\t\t\tconst obj: ortns.%sData =\n"
		    "\t\t\t\tthis.db_%s_fill"
		    "({row: <any[]>rows[i], pos: 0});\n",
		    rs->name, rs->name, rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS) {
			if (fprintf(f, "\t\t\tthis.db_%s_reffind"
			    "(this.#o, obj);\n", rs->name) < 0)
				return 0;
		}
		if (fprintf(f, 
		    "\t\t\tobjs.push(new ortns.%s(this.#role, obj));\n"
		    "\t\t}\n"
		    "\t\treturn objs;\n", rs->name) < 0)
			return 0;
		break;
	case STYPE_COUNT:
		if (fprintf(f, 
		    "\t\tconst cols: any = stmt.get(parms);\n"
		    "\n"
		    "\t\tif (typeof cols === 'undefined')\n"
		    "\t\t\tthrow \'count returned no result!?\';\n"
		    "\t\treturn BigInt(cols[0]);\n") < 0)
			return 0;
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

	if (!gen_fill(f, p))
		return 0;
	if (!gen_reffind(f, p))
		return 0;

	if (p->ins != NULL && !gen_insert(f, p))
		return 0;

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (!gen_query(f, cfg, s, pos++))
			return 0;

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		if (!gen_update(f, cfg, u, pos++))
			return 0;

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		if (!gen_update(f, cfg, u, pos++))
			return 0;

	return 1;
}

/*
 * Generate an bitfield pseudo-enumeration.
 * Return zero on failure, non-zero on success.
 */
static int
gen_bitf(FILE *f, const struct bitf *p, size_t pos)
{
	const struct bitidx	*bi;

	if (pos > 0 && fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS, p->doc))
		return 0;

	if (fprintf(f, "\texport enum %s {\n", p->name) < 0)
		return 0;

	TAILQ_FOREACH(bi, &p->bq, entries) {
		if (!gen_comment(f, 2, COMMENT_JS, bi->doc))
			return 0;
		if (fprintf(f, 
		    "\t\tBITI_%s = \'%" PRId64 "\',\n"
		    "\t\tBITF_%s = \'%" PRIu64 "\'", 
		    bi->name, bi->value,
		    bi->name, UINT64_C(1) << (uint64_t)bi->value) < 0)
			return 0;
		if (TAILQ_NEXT(bi, entries) != NULL)
			if (fputc(',', f) == EOF)
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	return fputs("\t}\n", f) != EOF;
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
				if (!gen_role(f, r->role, 2))
					return 0;
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

static int
gen_ortns_express_valid(FILE *f, const struct field *fd)
{
	const struct fvalid	*fv;

	if (fputs
	    ("\t\t\tif (typeof v === 'undefined' || v === null)\n"
	     "\t\t\t\treturn null;\n", f) == EOF)
		return 0;

	/* These use the native functions for validation. */

	switch (fd->type) {
	case FTYPE_BLOB:
		if (fputs("\t\t\treturn v;\n", f) == EOF)
			return 0;
		return 1;
	case FTYPE_TEXT:
	case FTYPE_PASSWORD:
		if (fputs
		    ("\t\t\tconst nv: string = "
		     "v.toString();\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(fv, &fd->fvq, entries)
			if (fprintf(f, 
			    "\t\t\tif (!(nv.length %s %zu))\n"
			    "\t\t\t\treturn null;\n", 
			    vtypes[fv->type], fv->d.value.len) < 0)
				return 0;
		if (fputs("\t\t\treturn nv;\n", f) == EOF)
			return 0;
		return 1;
	case FTYPE_EMAIL:
		if (fputs
		    ("\t\t\tif (!validator.isEmail(v.trim()))\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\treturn validator.normalizeEmail"
		     "(v.trim());\n", f) == EOF)
			return 0;
		return 1;
	case FTYPE_REAL:
		if (fputs
		    ("\t\t\tlet nv: number;\n"
		     "\t\t\tif (!validator.isDecimal"
		     "(v.toString().trim(), { locale: 'en-US' }))\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\tnv = parseFloat(v);\n"
		     "\t\t\tif (isNaN(nv))\n"
		     "\t\t\t\treturn null;\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(fv, &fd->fvq, entries)
			if (fprintf(f, 
			    "\t\t\tif (!(nv %s %f))\n"
			    "\t\t\t\treturn null;\n", 
			    vtypes[fv->type], fv->d.value.decimal) < 0)
				return 0;
		if (fputs("\t\t\treturn nv;\n", f) == EOF)
			return 0;
		return 1;
	case FTYPE_DATE:
		if (fputs
		    ("\t\t\tif (!validator.isDate(v.trim(), { "
		     "format: 'YYYY-MM-DD', strictMode: true }))\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\tconst nd: Date|null = "
		     "validator.toDate(v.trim());\n"
		     "\t\t\tif (nd === null)\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\tconst nv: BigInt = "
		     "BigInt(nd.getTime() / 1000);\n",
		     f) == EOF)
			return 0;
		break;
	case FTYPE_ENUM:
		assert(fd->enm != NULL);
		assert(TAILQ_EMPTY(&fd->fvq));
		if (fprintf(f, 
		    "\t\t\tif (!(<any>Object).values(ortns.%s)."
		    "includes(v.toString().trim()))\n"
		    "\t\t\t\treturn null;\n"
		    "\t\t\treturn <ortns.%s>v.toString().trim();\n",
		    fd->enm->name, fd->enm->name) < 0)
			return 0;
		return 1;
	default:
		if (fputs
		    ("\t\t\tif (v.toString().trim().length === 0)\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\tlet nv: BigInt;\n"
		     "\t\t\ttry {\n"
		     "\t\t\t\tnv = BigInt(v);\n"
		     "\t\t\t} catch (er) {\n"
		     "\t\t\t\treturn null;\n"
		     "\t\t\t}\n", f) == EOF)
			return 0;
		
		/*
		 * Bitfields need to be clamped into signed integers,
		 * but can be passed as unsigned.  Integers need to be
		 * checked for boundaries, and bits are 0--63.
		 */

		if (fd->type == FTYPE_BITFIELD) {
		       if (fputs
			   ("\t\t\tif (nv < minInt || nv > maxUint)\n" 
			    "\t\t\t\treturn null;\n"
			    "\t\t\tnv = BigInt.asIntN(64, nv);\n", f) == EOF)
			       return 0;
		} else if (fd->type != FTYPE_BIT) {
		       if (fputs
			   ("\t\t\tif (nv < minInt || nv > maxInt)\n" 
			    "\t\t\t\treturn null;\n", f) == EOF)
			       return 0;
		} else {
		       if (fputs
			   ("\t\t\tif (nv < BigInt(0) || nv > BigInt(64))\n"
			    "\t\t\t\treturn null;\n", f) == EOF)
			       return 0;
		}
		break;
	}

	TAILQ_FOREACH(fv, &fd->fvq, entries)
		if (fprintf(f, 
		    "\t\t\tif (!(nv %s BigInt('%" PRId64 "')))\n"
		    "\t\t\t\treturn null;\n", 
		    vtypes[fv->type], fv->d.value.integer) < 0)
			return 0;

	return fputs("\t\t\treturn nv;\n", f) != EOF;
}

static int
gen_ortns_express_valids(const struct ort_lang_nodejs *args,
	FILE *f, const struct config *cfg)
{
	const struct strct	*st;
	const struct field	*fd;
	int			 c;

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 0, COMMENT_JS, "Namespace for validation."))
		return 0;
	if (!(args->flags & ORT_LANG_NODEJS_NOMODULE) &&
	    fputs("export ", f) == EOF)
		return 0;
	if (fputs
	    ("namespace ortvalid {\n"
	     "\tconst minInt: BigInt = BigInt('-9223372036854775808');\n"
	     "\tconst maxInt: BigInt = BigInt('9223372036854775807');\n"
	     "\tconst maxUint: BigInt = BigInt('18446744073709551615');\n"
	     "\n"
	     "\texport interface ortValidType {\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(st, &cfg->sq, entries)
		TAILQ_FOREACH(fd, &st->fq, entries) {
			if (fd->type == FTYPE_STRUCT)
				continue;
			if (fprintf(f, "\t\t'%s-%s': (v?: any) => ", 
			    st->name, fd->name) < 0)
				return 0;
			switch (fd->type) {
			case FTYPE_BLOB:
				c = fputs("any;\n", f) != EOF;
				break;
			case FTYPE_ENUM:
				c = fprintf(f, "%s|null;\n", 
					fd->enm->name) >= 0;
				break;
			default:
				c = fprintf(f, "%s|null;\n", 
					ftypes[fd->type]) >= 0;
				break;
			}
			if (!c)
				return 0;
		}
	if (fputs("\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Validator routines for each field.\n"
	    "These all test the input and return the validated "
	    "output or null on failure.\n"
	    "Validated output may be different from input, not just "
	    "in terms of type (e.g., the opaque input value being "
	    "returned as a BigInt), but reformatted like an e-mail "
	    "address having white-space stripped."))
		return 0;
	if (fputs
	    ("\texport const ortValids: ortValidType = {\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(st, &cfg->sq, entries) {
		TAILQ_FOREACH(fd, &st->fq, entries) {
			if (fd->type == FTYPE_STRUCT)
				continue;
			if (fprintf(f, "\t\t'%s-%s': (v) => {\n", 
			    st->name, fd->name) < 0)
				return 0;
			if (!gen_ortns_express_valid(f, fd))
				return 0;
			if (fputs("\t\t},\n", f) == EOF)
				return 0;
		}
	}

	return fputs("\t}\n}\n", f) != EOF;
}

/*
 * Generates data structure part of the data model under the "ortns"
 * namespace.
 * It's divided primarily into data within interfaces and classes that
 * encapsulate that data and contain role information.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortns(const struct ort_lang_nodejs *args,
	FILE *f, const struct config *cfg)
{
	const struct strct	*p;
	const struct enm	*e;
	const struct bitf	*b;
	size_t			 i = 0;

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 0, COMMENT_JS,
	    "Namespace for data interfaces and representative "
	    "classes.  The interfaces are for the data itself, "
	    "while the classes manage roles and metadata."))
		return 0;
	if (!(args->flags & ORT_LANG_NODEJS_NOMODULE) &&
	    fputs("export ", f) == EOF)
		return 0;
	if (fputs("namespace ortns {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (!gen_enm(f, e, i++))
			return 0;
	TAILQ_FOREACH(b, &cfg->bq, entries)
		if (!gen_bitf(f, b, i++))
			return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_strct(f, p, i++))
			return 0;

	return fputs("}\n", f) != EOF;
}

/*
 * Generate the class for managing a single connection.
 * This is otherwise defined as a single sequence of role transitions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortdb(const struct ort_lang_nodejs *args, FILE *f)
{

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 0, COMMENT_JS,
	    "Primary database object. "
	    "Only one of these should exist per running node.js "
	    "server."))
		return 0;
	if (!(args->flags & ORT_LANG_NODEJS_NOMODULE) &&
	    fputs("export ", f) == EOF)
		return 0;
	if (fputs("class ortdb {\n"
	    "\tdb: Database.Database;\n", f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "The ort-nodejs version used to produce this file."))
		return 0;
	if (fprintf(f, "\treadonly version: "
	    "string = \'%s\';\n", ORT_VERSION) < 0)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "The numeric (monotonically increasing) ort-nodejs "
	    "version used to produce this file."))
		return 0;
	if (fprintf(f, "\treadonly vstamp: number = %lld;\n"
	    "\n", (long long)ORT_VSTAMP) < 0)
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
	    "Connect to the database.  This should be invoked for "
	    "each request.  In applications not having a request, "
	    "this corresponds to a single operator sequence.  If "
	    "roles are enabled, the connection will begin in the "
	    "\"default\" role."))
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
 * Generate all of the possible transitions from the given role into all
 * possible roles, then all of the transitions from the roles "beneath"
 * the current role.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortctx_dbrole_role(FILE *f, const struct role *r)
{
	const struct role	*rr;

	if (fprintf(f, "\t\tcase '%s':\n"
	    "\t\t\tswitch(newrole) {\n", r->name) < 0)
		return 0;

	if (!gen_role(f, r, 3))
		return 0;
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

	if (fputc('\n', f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "If roles are enabled, get the currently-assigned role.  "
	    "If db_role() hasn't yet been called, this will be "
	    "\"default\"."))
		return 0;
	if (fputs("\tdb_role_current(): string\n"
	    "\t{\n"
	    "\t\treturn this.#role;\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "If roles are enabled, move from the current role to "
	    "\"newrole\".  If the role is the same as the current "
	    "role, this does nothing.  Roles may only transition to "
	    "ancestor roles, not descendant roles or siblings, or "
	    "any other non-ancestor roles.  The only exception is "
	    "when leaving \"default\" or entering \"none\".  This "
	    "does not return failure: on role violation, it invokes "
	    "process.abort()."))
		return 0;
	if (fputs("\tdb_role(newrole: string): void\n"
	    "\t{\n"
	    "\t\tif (this.#role === newrole)\n"
	    "\t\t\treturn;\n"
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
 * Generate the data access portion of the data model entirely within a
 * single class.
 * Return zero on failure, non-zero on success.
 */
static int
gen_ortctx(const struct ort_lang_nodejs *args,
	FILE *f, const struct config *cfg)
{
	const struct strct	*p;

	if (fputs("\n"
	    "namespace ortstmt {\n"
	    "\texport enum ortstmt {\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_sql_enums(f, 2, p, LANG_JS))
			return 0;
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
		if (!gen_sql_stmts(f, 2, p, LANG_JS))
			return 0;
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
	if (!(args->flags & ORT_LANG_NODEJS_NOMODULE) &&
	    fputs("export ", f) == EOF)
		return 0;
	if (fputs("class ortctx {\n"
	    "\t#role: string = 'default';\n"
	    "\treadonly #o: ortdb;\n"
	    "\n"
	    "\tconstructor(o: ortdb) {\n"
	    "\t\tthis.#o = o;\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Open a transaction with a unique identifier \"id\".  "
	    "This is the preferred way of creating database "
	    "transactions.  The transaction immediately enters "
	    "unshared lock mode (single writer, readers allowed).  "
	    "Throws an exception on database error."))
		return 0;
	if (fputs("\tdb_trans_open_immediate(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION IMMEDIATE\');\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Open a transaction with a unique identifier \"id\".  "
	    "The transaction locks the database on first access "
	    "with shared locks (no writes allowed, reads allowed) "
	    "on queries and unshared locks (single writer, reads "
	    "allowed) on modification.  Throws an exception on "
	    "database error."))
		return 0;
	if (fputs("\tdb_trans_open_deferred(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION DEFERRED\');\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Open a transaction with a unique identifier \"id\".  "
	    "The transaction locks exclusively, preventing all "
	    "other access.  Throws an exception on database error."))
		return 0;
	if (fputs("\tdb_trans_open_exclusive(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'BEGIN TRANSACTION EXCLUSIVE\');\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Roll-back a transaction opened by db_trans_open_xxxx() "
	    "with identifier \"id\".  Throws an exception on "
	    "database error."))
		return 0;
	if (fputs("\tdb_trans_rollback(id: number): void\n"
	    "\t{\n"
	    "\t\tthis.#o.db.exec(\'ROLLBACK TRANSACTION\');\n"
	    "\t}\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_JS,
	    "Commit a transaction opened by db_trans_open_xxxx() "
	    "with identifier \"id\".  Throws an exception on "
	    "database error."))
	if (fputs("\tdb_trans_commit(id: number): void\n"
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
ort_lang_nodejs(const struct ort_lang_nodejs *args,
	const struct config *cfg, FILE *f)
{
	struct ort_lang_nodejs	 tmp;

	if (args == NULL) {
		memset(&tmp, 0, sizeof(struct ort_lang_nodejs));
		args = &tmp;
	}

	if (!gen_commentv(f, 0, COMMENT_JS, 
	    "WARNING: automatically generated by ort %s.\n"
	    "DO NOT EDIT!\n"
	    "@packageDocumentation", ORT_VERSION))
		return 0;

	if (args->flags & (ORT_LANG_NODEJS_DB|ORT_LANG_NODEJS_VALID))
		if (fputc('\n', f) == EOF)
			return 0;

	if (!(args->flags & ORT_LANG_NODEJS_NOMODULE)) {
		if ((args->flags & ORT_LANG_NODEJS_DB) &&
		    fputs("import bcrypt from 'bcrypt';\n"
		    "import Database from 'better-sqlite3';\n", f) == EOF)
			return 0;
		if ((args->flags & ORT_LANG_NODEJS_VALID) &&
		    fputs("import validator from 'validator';\n", f) == EOF)
			return 0;
	}

	if ((args->flags & ORT_LANG_NODEJS_CORE) &&
	    !gen_ortns(args, f, cfg))
		return 0;

	if ((args->flags & ORT_LANG_NODEJS_VALID) &&
	    !gen_ortns_express_valids(args, f, cfg))
		return 0;

	if ((args->flags & ORT_LANG_NODEJS_DB)) {
		if (!gen_ortdb(args, f))
			return 0;
		if (!gen_ortctx(args, f, cfg))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_JS,
		    "Instance an application-wide context. "
		    "This should only be called once per server, with "
		    "the {@link ortdb.connect} method used for "
		    "sequences of operations. Throws an exception on "
		    "database error."))
			return 0;
		if (!(args->flags & ORT_LANG_NODEJS_NOMODULE) &&
		    fputs("export ", f) == EOF)
			return 0;
		if (fputs
		    ("function ort(dbname: string): ortdb\n"
		     "{\n"
		     "\treturn new ortdb(dbname);\n"
		     "}\n", f) == EOF)
			return 0;
	}

	return 1;
}
