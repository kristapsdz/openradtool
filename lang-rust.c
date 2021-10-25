/*	$Id$ */
/*
 * Copyright (c) 2021 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-rust.h"
#include "ort-version.h"
#include "lang.h"

static	const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"get", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const ftypes[FTYPE__MAX] = {
	"i64", /* FTYPE_BIT */
	"i64", /* FTYPE_DATE */
	"i64", /* FTYPE_EPOCH */
	"i64", /* FTYPE_INT */
	"f64", /* FTYPE_REAL */
	"Vec<u8>", /* FTYPE_BLOB */
	"String", /* FTYPE_TEXT */
	"String", /* FTYPE_PASSWORD */
	"String", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"i64", /* FTYPE_BITFIELD */
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

static char *
strdup_ident(const char *s)
{
	char	*cp;

	if (strcasecmp(s, "self") == 0 ||
	    strcasecmp(s, "type") == 0) {
		if (asprintf(&cp, "r#%s", s) == -1)
			return NULL;
	} else
		cp = strdup(s);

	return cp;
}

static char *
strdup_title(const char *s)
{
	char	*cp;
	size_t	 offs = 0;

	if (strcasecmp(s, "self") == 0 ||
	    strcasecmp(s, "type") == 0) {
		if (asprintf(&cp, "r#%s", s) == -1)
			return NULL;
		offs = 2;
	} else {
		if ((cp = strdup(s)) == NULL)
			return NULL;
	}

	cp[offs] = (char)toupper((unsigned char)cp[offs]);
	return cp;
}

/*
 * Return <0 on failure, >0 on success, 0 if nothing printed.
 */
static int
gen_role(FILE *f, const struct role *r, size_t tabs,
	const char *cond, int rc, int super)
{
	const struct role	*rr;
	int			 ret;

	TAILQ_FOREACH(rr, &r->subrq, entries) {
		ret = gen_role(f, rr, tabs, cond, rc, super);
		if (ret < 0)
			return -1;
		if (ret > 0 && rc == 0)
			rc = ret;
	}

	if (strcmp(r->name, "all") == 0)
		return rc;

	return fprintf(f, "%s%s == %sOrtrole::%c%s",
	    rc > 0 ? " || ": "", cond,
	    super ? "super::" : "",
	    toupper((unsigned char)r->name[0]),
	    r->name + 1) < 0 ? -1 : 1;
}

/*
 * Recursively generate all roles allowed by this rolemap.
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_rolemap(FILE *f, const struct rolemap *rm)
{
	const struct rref	*rr;
	int			 rc = 0;

	if (rm == NULL || TAILQ_EMPTY(&rm->rq))
		return 1;
	if (fprintf(f, "%12sif !(", "") < 0)
		return 0;
	TAILQ_FOREACH(rr, &rm->rq, entries) {
		rc = gen_role(f, rr->role, 4, "self.role", rc, 0);
		if (rc < 0)
			return 0;
	}

	return fprintf(f, ") {\n"
		"%16spanic!(\"role violation\");\n"
		"%12s}\n", "", "") >= 0;
}

static int
gen_var(FILE *f, size_t pos, const struct field *fd, int comma)
{

	if (comma && fputs(", ", f) == EOF)
		return -1;
	if (fprintf(f, "v%zu: ", pos) < 0)
		return -1;
	if ((fd->flags & FIELD_NULL) && fputs("Option<", f) == EOF)
		return 0;
	if (fd->type == FTYPE_ENUM) {
		if (fprintf(f, "data::%c%s",
		    toupper((unsigned char)fd->enm->name[0]),
		    fd->enm->name + 1) < 0)
			return 0;
	} else {
		if (strcmp(ftypes[fd->type], "i64") &&
		    strcmp(ftypes[fd->type], "f64"))
			if (fputc('&', f) == EOF)
				return 0;
		if (fputs(ftypes[fd->type], f) == EOF)
			return 0;
	}
	if ((fd->flags & FIELD_NULL) && fputs(">", f) == EOF)
		return 0;
	return 1;
}

static int
gen_field_to_json(const struct field *fd, int first, FILE *f)
{
	const struct rref	*r;
	int	 		 tabs = 16, rc = 0;

	if (fd->flags & FIELD_NOEXPORT)
		return 0;
	if (fd->type == FTYPE_PASSWORD)
		return 0;

	if (fd->rolemap != NULL &&
	    !TAILQ_EMPTY(&fd->rolemap->rq)) {
		if (fprintf(f, "%16sif !(", "") < 0)
			return 0;
		TAILQ_FOREACH(r, &fd->rolemap->rq, entries) {
			rc = gen_role(f, r->role, 5, "role", rc, 1);
			if (rc < 0)
				return 0;
		}
		if (fputs(") {\n", f) == EOF)
			return 0;
		tabs += 4;
	}

	if (fprintf(f, "%*sret += \"%s\\\"%s\\\":\";\n",
	    tabs, "", first ? "" : ",", fd->name) < 0)
		return -1;

	switch (fd->type) {
	case FTYPE_BIT:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
	case FTYPE_REAL:
	case FTYPE_BITFIELD:
		if (fd->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%*sif let Some(x) = self.%s {\n"
			    "%*sret += &format!(\"\\\"{}\\\"\", x);\n"
			    "%*s} else {\n"
			    "%*sret += \"null\";\n"
			    "%*s}\n",
			    tabs, "", fd->name, 
			    tabs + 4, "", 
			    tabs, "", 
			    tabs + 4, "", 
			    tabs, "") < 0)
				return -1;
		} else {
			if (fprintf(f,
			    "%*sret += &format!"
			     "(\"\\\"{}\\\"\", self.%s);\n",
			    tabs, "", fd->name) < 0)
				return -1;
		}
		break;
	case FTYPE_ENUM:
		if (fd->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%*sif let Some(ref x) = &self.%s {\n"
			    "%*sret += &format!(\"\\\"{}\\\"\", "
			     "num_traits::ToPrimitive::to_i64"
			     "(x).unwrap());\n"
			    "%*s} else {\n"
			    "%*sret += \"null\";\n"
			    "%*s}\n",
			    tabs, "", fd->name, 
			    tabs + 4, "", 
			    tabs, "", 
			    tabs + 4, "", 
			    tabs, "") < 0)
				return -1;
		} else {
			if (fprintf(f,
			    "%*sret += &format!(\"\\\"{}\\\"\", "
			     "num_traits::ToPrimitive::to_i64"
			     "(&self.%s).unwrap());\n",
			    tabs, "", fd->name) < 0)
				return -1;
		}
		break;
	case FTYPE_STRUCT:
		if (fd->ref->source->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%*sif let Some(ref x) = &self.%s {\n"
			    "%*sret += \"{\";\n"
			    "%*sret += &x.to_json();\n"
			    "%*sret += \"}\";\n"
			    "%*s} else {\n"
			    "%*sret += \"null\";\n"
			    "%*s}\n",
			    tabs, "", fd->name, 
			    tabs + 4, "", 
			    tabs + 4, "", 
			    tabs + 4, "", 
			    tabs, "", 
			    tabs + 4, "", 
			    tabs, "") < 0)
				return -1;
		} else {
			if (fprintf(f,
			    "%*sret += \"{\";\n"
			    "%*sret += &self.%s.to_json();\n"
			    "%*sret += \"}\";\n",
			    tabs, "", 
			    tabs, "", fd->name, 
			    tabs, "") < 0)
				return -1;
		}
		break;
	case FTYPE_BLOB:
		if (fd->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%*sif let Some(ref x) = &self.%s {\n"
			    "%*sret += \"\\\"\";\n"
			    "%*sret += &base64::encode(x);\n"
			    "%*sret += \"\\\"\";\n"
			    "%*s} else {\n"
			    "%*sret += \"null\";\n"
			    "%*s}\n",
			    tabs, "", fd->name, 
			    tabs + 4, "", 
			    tabs + 4, "", 
			    tabs + 4, "", 
			    tabs, "", 
			    tabs + 4, "", 
			    tabs, "") < 0)
				return -1;
		} else {
			if (fprintf(f,
			    "%*sret += \"\\\"\";\n"
			    "%*sret += &base64::encode(&self.%s);\n"
			    "%*sret += \"\\\"\";\n",
			    tabs, "", 
			    tabs, "", fd->name, 
			    tabs, "") < 0)
				return -1;
		}
		break;
	default:
		if (fd->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%*sif let Some(ref x) = &self.%s {\n"
			    "%*sret += &json_escape(x);\n"
			    "%*s} else {\n"
			    "%*sret += \"null\";\n"
			    "%*s}\n",
			    tabs, "", fd->name, 
			    tabs + 4, "", 
			    tabs, "", 
			    tabs + 4, "", 
			    tabs, "") < 0)
				return -1;
		} else {
			if (fprintf(f,
			    "%*sret += &json_escape(&self.%s);\n",
			    tabs, "", fd->name) < 0)
				return -1;
		}
		break;
	}

	if (fd->rolemap != NULL && fprintf(f, "%16s}\n", "") < 0)
		return 0;

	return 1;
}

/*
 * Generate the public structure and implementation for a struct.  These
 * will contain the actual data and direct functions on it.  Return TRUE
 * on success, FALSE on failure.
 */
static int
gen_data_strct(const struct strct *s, FILE *f)
{
	const struct field	*fd;
	char			*cp = NULL, *name = NULL;
	int			 ret = 0, isnull, first, rc;

	if ((name = strdup_title(s->name)) == NULL)
		goto out;
	if (fprintf(f, "%8spub struct %s {\n", "", name) < 0)
		goto out;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		free(cp);
		if ((cp = strdup_ident(fd->name)) == NULL)
			goto out;
		if (fprintf(f, "%12spub %s: ", "", cp) < 0)
			goto out;

		isnull = fd->flags & FIELD_NULL;
		if (fd->type == FTYPE_STRUCT &&
		    (fd->ref->source->flags & FIELD_NULL))
			isnull = 1;

		if (isnull && fputs("Option<", f) == EOF)
			goto out;

		if (fd->type == FTYPE_STRUCT) {
			free(cp);
			cp = strdup_title(fd->ref->target->parent->name);
			if (cp == NULL)
				goto out;
			if (fputs(cp, f) == EOF)
				goto out;
		} else if (fd->type == FTYPE_ENUM) {
			free(cp);
			cp = strdup_title(fd->enm->name);
			if (cp == NULL)
				goto out;
			if (fprintf(f, "%s", cp) < 0)
				goto out;
		} else {
			if (fputs(ftypes[fd->type], f) == EOF)
				goto out;
		}

		if (isnull && fputc('>', f) == EOF)
			goto out;
		if (fputs(",\n", f) == EOF)
			goto out;
	}

	if (fprintf(f, "%8s}\n", "") < 0)
		goto out;

	if (fprintf(f,
	    "%8simpl %s {\n"
	    "%12spub(super) fn to_json(&self%s) -> String {\n"
	    "%16slet mut ret = String::new();\n",
	    "", name, "", TAILQ_EMPTY(&s->cfg->arq) ? 
	    "" : ", role: super::Ortrole", "") < 0)
		goto out;

	first = 1;
	TAILQ_FOREACH(fd, &s->fq, entries)
		if ((rc = gen_field_to_json(fd, first, f)) < 0)
			goto out;
		else if (rc > 0)
			first = 0;

	ret = fprintf(f, "%16sret\n%12s}\n%8s}\n", "", "", "") >= 0;
out:
	free(name);
	free(cp);
	return ret;
}

static int
gen_types_bitf(const struct bitf *b, FILE *f)
{
	const struct bitidx	*bi;
	char			*name = NULL, *cp = NULL;
	int			 rc = 0;

	if ((name = strdup_title(b->name)) == NULL)
		goto out;
	if (fprintf(f, "%8spub enum %s {\n", "", name) < 0)
		goto out;

	TAILQ_FOREACH(bi, &b->bq, entries) {
		free(cp);
		if ((cp = strdup_title(bi->name)) == NULL)
			goto out;
		if (fprintf(f,
		    "%12s%s = %" PRId64 ",\n", "", cp, bi->value) < 0)
			goto out;
	}

	rc = fprintf(f, "%8s}\n", "") >= 0;
out:
	free(name);
	free(cp);
	return rc;
}

static int
gen_types_enum(const struct enm *e, FILE *f)
{
	const struct eitem	*ei;
	char			*name = NULL, *cp = NULL;
	int			 rc = 0;

	if ((name = strdup_title(e->name)) == NULL)
		goto out;
	if (fprintf(f,
	    "%8s#[derive(FromPrimitive,ToPrimitive,PartialEq,Debug)]\n"
	    "%8spub enum %s {\n", "", "", name) < 0)
		goto out;

	TAILQ_FOREACH(ei, &e->eq, entries) {
		free(cp);
		if ((cp = strdup_title(ei->name)) == NULL)
			goto out;
		if (fprintf(f,
		    "%12s%s = %" PRId64 ",\n", "", cp, ei->value) < 0)
			goto out;
	}

	rc = fprintf(f, "%8s}\n", "") >= 0;
out:
	free(name);
	free(cp);
	return rc;
}

static int
gen_objs_strct(const struct strct *s, FILE *f)
{
	char	*name = NULL;
	int	 rc = 0;

	if ((name = strdup_title(s->name)) == NULL)
		goto out;
	if (fprintf(f,
	    "%8spub struct %s {\n"
	    "%12spub data: super::data::%s,\n",
	    "", name, "", name) < 0)
		goto out;

	if (!TAILQ_EMPTY(&s->cfg->arq) &&
	    fprintf(f, "%12spub(super) role: super::Ortrole,\n", "") < 0)
		goto out;

	if (fprintf(f,
	    "%8s}\n"
	    "%8simpl %s {\n"
	    "%12spub fn export(&self) -> String {\n"
	    "%16slet mut ret = String::new();\n"
	    "%16sret += \"{\";\n"
	    "%16sret += &self.data.to_json(%s);\n"
	    "%16sret += \"}\";\n"
	    "%16sret\n"
	    "%12s}\n"
	    "%8s}\n", 
	    "", "", name, "", "", "", "", 
	    TAILQ_EMPTY(&s->cfg->arq) ? "" : "self.role",
	    "", "", "", "") < 0)
		goto out;
	rc = 1;
out:
	free(name);
	return rc;
}

static int
gen_data(const struct config *cfg, FILE *f)
{
	const struct strct	*s;
	const struct enm	*e;
	const struct bitf	*b;

	if (fprintf(f, "\n%4spub mod data {\n", "") < 0)
		return 0;
	if (fprintf(f, "%8suse base64;\n", "") < 0)
		return 0;

	if (fprintf(f,
	    "%8sfn json_escape(src: &str) -> String {\n"
	    "%12slet mut escaped = "
	     "String::with_capacity(src.len() + 2);\n"
	    "%12sescaped.push('\"');\n"
	    "%12sfor c in src.chars() {\n"
	    "%16smatch c {\n"
	    "%20s'\\x08' => escaped += \"\\\\b\",\n"
	    "%20s'\\x0c' => escaped += \"\\\\f\",\n"
	    "%20s'\\n'   => escaped += \"\\\\n\",\n"
	    "%20s'\\r'   => escaped += \"\\\\r\",\n"
	    "%20s'\\t'   => escaped += \"\\\\t\",\n"
	    "%20s'\"'    => escaped += \"\\\\\\\"\",\n"
	    "%20s'\\\\'   => escaped += \"\\\\\",\n"
	    "%20sc      => escaped.push(c),\n"
	    "%16s}\n"
	    "%12s}\n"
	    "%12sescaped.push('\"');\n"
	    "%12sescaped\n"
	    "%8s}\n",
	    "", "", "", "", "", "", "", "", "",
	    "", "", "", "", "", "", "", "", "") < 0)
		return 0;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (!gen_types_enum(e, f))
			return 0;
	TAILQ_FOREACH(b, &cfg->bq, entries)
		if (!gen_types_bitf(b, f))
			return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_data_strct(s, f))
			return 0;

	return fprintf(f, "%4s}\n", "") != EOF;
}

static int
gen_objs(const struct config *cfg, FILE *f)
{
	const struct strct	*s;

	if (fprintf(f, "\n%4spub mod objs {\n", "") < 0)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_objs_strct(s, f))
			return 0;

	return fprintf(f, "%4s}\n", "") != EOF;
}

static int
gen_roles(const struct config *cfg, FILE *f)
{
	const struct role	*r;
	char			*cp;
	int			 rc;

	if (TAILQ_EMPTY(&cfg->arq))
		return 1;

	if (fprintf(f, "\n"
	    "%4s#[derive(PartialEq,Clone,Copy)]\n"
	    "%4spub enum Ortrole {\n", "", "") < 0)
		return 0;

	TAILQ_FOREACH(r, &cfg->arq, allentries) {
		if (strcmp(r->name, "all") == 0)
			continue;
		if ((cp = strdup_title(r->name)) == NULL)
			return 0;
		rc = fprintf(f, "%8s%s,\n", "", cp);
		free(cp);
		if (rc < 0)
			return 0;
	}

	return fprintf(f, "%4s}\n", "") >= 0;
}

static int
gen_aliases(const struct config *cfg, FILE *f)
{
	const struct field	*fd, *last;
	const struct strct	*s;

	if (fprintf(f, "%8s#[allow(dead_code)]\n"
	    "%8spub enum Ortstmt {\n", "", "") < 0)
		return 0;
	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_sql_enums(f, 3, s, LANG_RUST))
			return 0;
	if (fprintf(f, "%8s}\n", "") < 0)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		last = NULL;
		TAILQ_FOREACH(fd, &s->fq, entries)
			if (fd->type != FTYPE_STRUCT)
				last = fd;
		assert(last != NULL);

		if (fprintf(f, "\n"
		    "%8sfn stmt_%s(v: &str) -> String {\n"
		    "%12slet mut s = String::new();\n",
		    "", s->name, "") < 0)
			return 0;

		TAILQ_FOREACH(fd, &s->fq, entries) {
			if (fd->type == FTYPE_STRUCT)
				continue;
			if (fprintf(f,
			    "%12ss += &format!(\"{}.%s%s\", v);\n",
			    "", fd->name, last != fd ? ", " : "") < 0)
				return 0;
		}

		if (fprintf(f, "%12ss\n%8s}\n", "", "") < 0)
			return 0;
	}

	if (fprintf(f, "\n"
	    "%8spub fn stmt_fmt(v: Ortstmt) -> String {\n"
	    "%12slet s;\n"
	    "%12smatch v {\n", "", "", "") < 0)
		return 0;
	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_sql_stmts(f, 4, s, LANG_RUST))
			return 0;
	return fprintf(f, "%12s}\n%12ss\n%8s}\n", "", "", "") >= 0;
}

/*
 * Generate db_xxx_delete or db_xxx_update method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_update(const struct update *up, size_t num, FILE *f)
{
	const struct uref	*ref;
	size_t		 	 pos = 1, hash;
	int			 rc;

	/* Method signature. */

	if (fprintf(f, "%8spub fn db_%s_%s",
	    "", up->parent->name, utypes[up->type]) < 0)
		return 0;

	if (up->name == NULL && up->type == UP_MODIFY) {
		if (!(up->flags & UPDATE_ALL))
			TAILQ_FOREACH(ref, &up->mrq, entries) {
				rc = fprintf(f, "_%s_%s",
					ref->field->name,
					modtypes[ref->mod]);
				if (rc < 0)
					return 0;
			}
		if (!TAILQ_EMPTY(&up->crq)) {
			if (fprintf(f, "_by") < 0)
				return 0;
			TAILQ_FOREACH(ref, &up->crq, entries) {
				rc = fprintf(f, "_%s_%s",
					ref->field->name,
					optypes[ref->op]);
				if (rc < 0)
					return 0;
			}
		}
	} else if (up->name == NULL) {
		if (!TAILQ_EMPTY(&up->crq)) {
			if (fprintf(f, "_by") < 0)
				return 0;
			TAILQ_FOREACH(ref, &up->crq, entries) {
				rc = fprintf(f, "_%s_%s",
					ref->field->name,
					optypes[ref->op]);
				if (rc < 0)
					return 0;
			}
		}
	} else {
		if (fprintf(f, "_%s", up->name) < 0)
			return 0;
	}

	if (fputs("(&self, ", f) == EOF)
		return 0;

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (gen_var(f, pos, ref->field, pos > 1) < 0)
			return 0;
		pos++;
	}
	TAILQ_FOREACH(ref, &up->crq, entries) {
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		if (gen_var(f, pos, ref->field, pos > 1) < 0)
			return 0;
		pos++;
	}

	if (fprintf(f, ") -> Result<%s> {\n",
	    up->type == UP_MODIFY ? "bool" : "()") < 0)
		return 0;
	if (!gen_rolemap(f, up->rolemap))
		return 0;
	if (fprintf(f, "%12slet sql = stmt::stmt_fmt(stmt::", "") < 0)
		return 0;
	if (up->type == UP_MODIFY &&
	    gen_enum_update(f, 1, up->parent, num, LANG_RUST) < 0)
		return 0;
	if (up->type == UP_DELETE &&
	    gen_enum_delete(f, 1, up->parent, num, LANG_RUST) < 0)
		return 0;
	if (fputs(");\n", f) == EOF)
		return 0;

	pos = hash = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (ref->field->type != FTYPE_PASSWORD ||
		    ref->mod == MODTYPE_STRSET) {
			pos++;
			continue;
		}
		if (ref->field->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%12slet hash%zu = match v%zu {\n"
			    "%16sSome(i) => Some"
			    "(hash(i, self.args.bcrypt_cost).unwrap()),\n"
			    "%16s_ => None,\n%12s};\n",
			    "", hash, pos, "", "", "") < 0)
				return 0;
		} else {
			if (fprintf(f,
			    "%12slet hash%zu = "
			    "hash(v%zu, self.args.bcrypt_cost).unwrap();\n",
			    "", hash, pos) < 0)
				return 0;
		}
		hash++;
		pos++;
	}

	if (fprintf(f,
	    "%12slet mut stmt = self.conn.prepare(&sql)?;\n"
	    "%12smatch stmt.execute(params![\n", "", "") < 0)
		return 0;

	pos = hash = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET) {
			if (fprintf(f, "%16shash%zu,\n", "", hash) < 0)
				return 0;
			hash++;
		} else {
			if (fprintf(f, "%16sv%zu,\n", "", pos) < 0)
				return 0;
		}
		pos++;
	}

	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		if (fprintf(f, "%16sv%zu,\n", "", pos) < 0)
			return 0;
		pos++;
	}

	if (up->type == UP_DELETE)
		return fprintf(f,
		    "%12s]) {\n"
		    "%16sOk(_) => Ok(()),\n"
		    "%16sErr(e) => Err(e),\n"
		    "%12s}\n%8s}\n",
		    "", "", "", "", "") >= 0;

	return fprintf(f,
	    "%12s]) {\n"
	    "%16sOk(_) => Ok(true),\n"
	    "%16sErr(e) => match e {\n"
	    "%20srusqlite::Error::SqliteFailure(err, ref _desc) => "
	     "match err.code {\n"
	    "%24slibsqlite3_sys::ErrorCode::ConstraintViolation => "
	     "Ok(false),\n"
	    "%24s_ => Err(e),\n"
	    "%20s},\n"
	    "%20s_ => Err(e),\n"
	    "%16s},\n"
	    "%12s}\n%8s}\n",
	    "", "", "", "", "", "", "", "", "", "", "") >= 0;
}

static int
gen_checkpass(FILE *f, size_t pos, const char *name,
	enum optype type, const struct field *fd)
{

	if ((fd->flags & FIELD_NULL) && fprintf(f,
	    "v%zu.is_none() || obj.%s.is_none() ||", pos, name) < 0)
		return 0;
	if (type == OPTYPE_EQUAL && fputc('!', f) == EOF)
		return 0;
	if (fprintf(f, "verify(v%zu", pos) < 0)
		return 0;
	if ((fd->flags & FIELD_NULL) &&
	    fputs(".unwrap(), ", f) == EOF)
		return 0;
	if (!(fd->flags & FIELD_NULL) && fputs(", &", f) == EOF)
		return 0;
	if (fprintf(f, "obj.%s", name) < 0)
		return 0;
	if ((fd->flags & FIELD_NULL) &&
	    fputs(".as_ref().unwrap()", f) == EOF)
		return 0;
	if (fputs(").unwrap() {\n", f) == EOF)
		return 0;
	return 1;
}

static int
gen_query_checkpass(FILE *f, const struct search *s, int ret)
{
	size_t		 	 pos = 1;
	const struct sent	*sent;

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		if (fprintf(f, "%16sif ", "") < 0)
			return 0;
		if (!gen_checkpass(f, pos, sent->fname,
		    sent->op, sent->field))
			return 0;
		if (ret && fprintf(f,
		    "%20sreturn Ok(None);\n%16s}\n", "", "") < 0)
			return 0;
		if (!ret && fprintf(f,
		    "%20scontinue;\n%16s}\n", "", "") < 0)
			return 0;
		pos++;
	}
	return 1;
}

/*
 * Generate db_xxx_reffind method (if applicable).
 * Return zero on failure, non-zero on success or non-applicable.
 */
static int
gen_reffind(const struct strct *s, FILE *f)
{
	const struct field	*fd;

	if (!(s->flags & STRCT_HAS_NULLREFS))
		return 1;

	if (fprintf(f,
	    "%8sfn db_%s_reffind(&self, obj: &mut data::%c%s) -> "
	    "Result<()> {\n", "", s->name,
	    toupper((unsigned char)s->name[0]), s->name + 1) < 0)
		return 0;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		if (fd->ref->source->flags & FIELD_NULL) {
			if (fprintf(f,
			    "%12sif let Some(nobj) = obj.%s {\n"
	    		    "%16slet sql = stmt::stmt_fmt(stmt::",
			    "", fd->ref->source->name, "") < 0)
				return 0;
			if (gen_enum_unique
			    (f, 1, fd->ref->target, LANG_RUST) < 0)
				return 0;
			if (fputs(");\n", f) == EOF)
				return 0;
			if (fprintf(f,
			    "%16slet mut stmt = "
			     "self.conn.prepare(&sql)?;\n"
			    "%16slet mut rows = "
			     "stmt.query(params![\n"
			    "%20snobj,\n"
			    "%16s])?;\n"
			    "%16sif let Some(row) = rows.next()? {\n"
			    "%20slet mut i = 0;\n"
			    "%20sobj.%s = Some"
			     "(self.db_%s_fill(&row, &mut i)?);\n"
			    "%16s} else {\n"
			    "%20sreturn Err(rusqlite::"
			     "Error::QueryReturnedNoRows);\n"
			    "%16s}\n"
			    "%12s}\n",
			    "", "", "", "", "", "", "", fd->name,
			    fd->ref->target->parent->name,
			    "", "", "", "") < 0)
				return 0;
		}
		if (!(fd->ref->target->parent->flags &
		      STRCT_HAS_NULLREFS))
			continue;
		if (fprintf(f,
		    "%12sif let Some(mut nobj) = obj.%s.as_mut() {\n"
		    "%16sself.db_%s_reffind(&mut nobj)?;\n"
		    "%12s}\n", "", fd->name, "",
		    fd->ref->target->parent->name, "") < 0)
			return 0;
	}

	return fprintf(f, "%12sOk(())\n%8s}\n", "", "") >= 0;
}

static int
gen_fill(const struct strct *s, FILE *f)
{
	const struct field	*fd;
	size_t	 		 scol, col, cols = 0;

	TAILQ_FOREACH(fd, &s->fq, entries)
		cols += fd->type != FTYPE_STRUCT;

	if (fprintf(f,
	    "%8sfn db_%s_fill(&self, row: &Row, i: &mut usize) -> "
	     "Result<data::%c%s> {\n"
	    "%12slet ncol: usize = *i;\n"
	    "%12s*i += %zu;\n",
	    "", s->name, toupper((unsigned char)s->name[0]),
	    s->name + 1, "", "", cols) < 0)
		return 0;

	col = scol = 0;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT &&
		    !(fd->ref->source->flags & FIELD_NULL)) {
			if (fprintf(f,
			    "%12slet obj%zu = self.db_%s_fill(row, i)?;\n",
			    "", scol++, fd->ref->target->parent->name) < 0)
				return 0;
		} else if (fd->type == FTYPE_ENUM) {
			if (fprintf(f,
			    "%12slet obj%zu = row.get(ncol + %zu)?;\n",
			    "", scol++, col++) < 0)
				return 0;
		} else if (fd->type != FTYPE_STRUCT)
			col++;
	}

	if (fprintf(f, "%12sOk(data::%c%s {\n",
	    "", toupper((unsigned char)s->name[0]), s->name + 1) < 0)
		return 0;

	col = scol = 0;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		switch (fd->type) {
		case FTYPE_STRUCT:
			if (fd->ref->source->flags & FIELD_NULL) {
				if (fprintf(f, "%16s%s: "
				    "None,\n", "", fd->name) < 0)
					return 0;
			} else {
				if (fprintf(f, "%16s%s: obj%zu,\n",
				    "", fd->name, scol++) < 0)
					return 0;
			}
			break;
		case FTYPE_ENUM:
			if (fprintf(f, "%16s%s: ", "", fd->name) < 0)
				return 0;
			if (fd->flags & FIELD_NULL) {
				if (fprintf(f,
				    "match obj%zu {\n"
				    "%20sSome(x) => Some(FromPrimitive"
				     "::from_i64(x).ok_or(rusqlite"
				     "::Error::IntegralValueOutOfRange"
				     "(ncol + %zu, x))?),\n"
				    "%20sNone => None,\n"
				    "%16s},\n",
				    scol, "", col, "", "") < 0)
					return 0;
			} else {
				if (fprintf(f,
				    "FromPrimitive::from_i64(obj%zu)."
				     "ok_or(rusqlite::Error::"
				     "IntegralValueOutOfRange(ncol + "
				     "%zu, obj%zu))?,\n",
				    scol, col, scol) < 0)
					return 0;
			}
			col++;
			scol++;
			break;
		default:
			if (fprintf(f, "%16s%s: row.get(ncol + %zu)?,"
			    "\n", "", fd->name, col++) < 0)
				return 0;
			break;
		}
	}

	return fprintf(f, "%12s})\n%8s}\n", "", "") >= 0;
}

/*
 * Generate db_xxxx_insert method.
 * Return zero on failure, non-zero on success.
 */
static int
gen_insert(const struct strct *s, FILE *f)
{
	const struct field	*fd;
	size_t	 	 	 pos, hash;

	if (fprintf(f, "%8spub fn db_%s_insert"
	    "(&self, ", "", s->name) < 0)
		return 0;

	pos = 1;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;
		if (gen_var(f, pos, fd, pos > 1) < 0)
			return 0;
		pos++;
	}

	if (fputs(") -> Result<i64> {\n", f) == EOF)
		return 0;
	if (!gen_rolemap(f, s->ins->rolemap))
		return 0;

	if (fprintf(f,
	    "%12slet sql = stmt::stmt_fmt(stmt::", "") < 0)
		return 0;
	if (gen_enum_insert(f, 1, s, LANG_RUST) < 0)
		return 0;
	if (fputs(");\n", f) == EOF)
		return 0;

	/*
	 * Hash passwords.  Use the bcrypt crate, and make sure to
	 * account for NULL password fields.
	 */

	pos = hash = 1;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;
		if (fd->type == FTYPE_PASSWORD &&
		    (fd->flags & FIELD_NULL)) {
			if (fprintf(f,
			    "%12slet hash%zu = match v%zu {\n"
			    "%16sSome(i) => Some(hash(i, "
			     "self.args.bcrypt_cost).unwrap()),\n"
			    "%16s_ => None,\n%12s};\n",
			    "", hash, pos, "", "", "") < 0)
				return 0;
		} else if (fd->type == FTYPE_PASSWORD) {
			if (fprintf(f,
			    "%12slet hash%zu = hash(v%zu, "
			     "self.args.bcrypt_cost).unwrap();\n",
			    "", hash, pos) < 0)
				return 0;
		}
		hash += fd->type == FTYPE_PASSWORD;
		pos++;
	}

	if (fprintf(f,
	    "%12slet mut stmt = self.conn.prepare(&sql)?;\n"
	    "%12smatch stmt.insert(params![\n", "", "") < 0)
		return 0;

	pos = hash = 1;
	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;
		switch (fd->type) {
		case FTYPE_PASSWORD:
			if (fprintf(f,
			    "%16shash%zu,\n", "", hash) < 0)
				return 0;
			break;
		case FTYPE_ENUM:
			if (fd->flags & FIELD_NULL) {
				if (fprintf(f,
				    "%16smatch v%zu {\n"
				    "%20sSome(x) => Some"
				     "(ToPrimitive::to_i64"
				      "(&x).unwrap()),\n"
				    "%20sNone => None,\n"
				    "%16s},\n",
				    "", pos, "", "", "") < 0)
					return 0;
			} else {
				if (fprintf(f,
				    "%16sToPrimitive::to_i64"
				     "(&v%zu).unwrap(),\n",
				    "", pos) < 0)
					return 0;
			}
			break;
		default:
			if (fprintf(f,
			    "%16sv%zu,\n", "", pos) < 0)
				return 0;
			break;
		}

		hash += fd->type == FTYPE_PASSWORD;
		pos++;
	}

	if (fprintf(f,
	    "%12s]) {\n"
	    "%16sOk(i) => Ok(i),\n"
	    "%16sErr(e) => match e {\n"
	    "%20srusqlite::Error::SqliteFailure(err, ref _desc) => "
	     "match err.code {\n"
	    "%24slibsqlite3_sys::ErrorCode::ConstraintViolation => "
	     "Ok(-1),\n"
	    "%24s_ => Err(e),\n"
	    "%20s},\n"
	    "%20s_ => Err(e),\n"
	    "%16s},\n"
	    "%12s}\n%8s}\n",
	    "", "", "", "", "", "", "", "", "", "", "") < 0)
		return 0;
	return 1;
}

static int
gen_query(const struct search *s, size_t num, FILE *f)
{
	const struct sent	*sent;
	const struct strct	*rs;
	char			*ret;
	size_t			 pos, hash;

	/*
	 * The "real struct" we'll return is either ourselves or the one
	 * we reference with a distinct clause.
	 */

	rs = s->dst != NULL ? s->dst->strct : s->parent;
	if ((ret = strdup(rs->name)) == NULL)
		return 0;
	ret[0] = toupper((unsigned char)ret[0]);

	if (fprintf(f, "%8spub fn db_%s_%s",
	    "", s->parent->name, stypes[s->type]) < 0)
		return 0;

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		if (fprintf(f, "_by") < 0)
			return 0;
		TAILQ_FOREACH(sent, &s->sntq, entries)
			if (fprintf(f, "_%s_%s",
			    sent->uname, optypes[sent->op]) < 0)
				return 0;
	} else if (s->name != NULL)
		if (fprintf(f, "_%s", s->name) < 0)
			return 0;

	if (fputs("(&self", f) == EOF)
		return 0;

	if (s->type == STYPE_ITERATE &&
	    fprintf(f, ", cb: fn(res: objs::%s)", ret) < 0)
		return 0;

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (gen_var(f, pos, sent->field, 1) < 0)
			return 0;
		pos++;
	}

	if (fputs(") -> ", f) == EOF)
		return 0;

	switch (s->type) {
	case STYPE_SEARCH:
		if (fprintf(f, "Result<Option<objs::%s>>", ret) < 0)
			return 0;
		break;
	case STYPE_LIST:
		if (fprintf(f, "Result<Vec<objs::%s>>", ret) < 0)
			return 0;
		break;
	case STYPE_ITERATE:
		if (fputs("Result<()>", f) == EOF)
			return 0;
		break;
	default:
		if (fputs("Result<i64>", f) == EOF)
			return 0;
		break;
	}

	if (fputs(" {\n", f) == EOF)
		return 0;
	if (!gen_rolemap(f, s->rolemap))
		return 0;

	if (fprintf(f, "%12slet sql = stmt::stmt_fmt(stmt::", "") < 0)
		return 0;
	if (gen_enum_query(f, 1, s->parent, num, LANG_RUST) < 0)
		return 0;
	if (fputs(");\n", f) == EOF)
		return 0;
	if (fprintf(f,
	    "%12slet mut stmt = self.conn.prepare(&sql)?;\n"
	    "%12slet mut rows = stmt.query(params![\n", "", "") < 0)
		return 0;

	pos = hash = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		switch (sent->field->type) {
		case FTYPE_ENUM:
			if (fprintf(f, "%16sToPrimitive::to_i64"
			    "(&v%zu).unwrap(),\n", "", pos) < 0)
				return 0;
			break;
		case FTYPE_PASSWORD:
			if (sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ)
				break;
			/* FALLTHROUGH */
		default:
			if (fprintf(f, "%16sv%zu,\n", "", pos) < 0)
				return 0;
		}
		pos++;
	}

	if (fprintf(f, "%12s])?;\n", "") < 0)
		return 0;
	
	switch (s->type) {
	case STYPE_SEARCH:
		if (fprintf(f,
		    "%12sif let Some(row) = rows.next()? {\n"
		    "%16slet mut i = 0;\n"
		    "%16slet %sobj = self.db_%s_fill(&row, &mut i)?;\n",
		    "", "", "",
		    (rs->flags & STRCT_HAS_NULLREFS) ? "mut " : "",
		    rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS)
		       if (fprintf(f,
			   "%16sself.db_%s_reffind(&mut obj);\n",
			   "", rs->name) < 0)
			       return 0;
		gen_query_checkpass(f, s, 1);
		if (fprintf(f,
		    "%16sreturn Ok(Some(objs::%s {\n"
		    "%20sdata: obj,\n", "", ret, "") < 0)
			return 0;
		if (!TAILQ_EMPTY(&s->parent->cfg->arq) &&
		    fprintf(f, "%20srole: self.role,\n", "") < 0)
			return 0;
		if (fprintf(f,
		    "%16s}));\n"
		    "%12s}\n"
		    "%12sOk(None)\n", "", "", "") < 0)
			return 0;
		break;
	case STYPE_ITERATE:
		if (fprintf(f,
		    "%12swhile let Some(row) = rows.next()? {\n"
		    "%16slet mut i = 0;\n"
		    "%16slet %sobj = self.db_%s_fill(&row, &mut i)?;\n",
		    "", "", "",
		    (rs->flags & STRCT_HAS_NULLREFS) ? "mut " : "",
		    rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS)
		       if (fprintf(f,
			   "%16sself.db_%s_reffind(&mut obj);\n",
			   "", rs->name) < 0)
			       return 0;
		gen_query_checkpass(f, s, 0);
		if (fprintf(f,
		    "%16scb(objs::%s {\n"
		    "%20sdata: obj,\n", "", ret, "") < 0)
			return 0;
		if (!TAILQ_EMPTY(&s->parent->cfg->arq) &&
		    fprintf(f, "%20srole: self.role,\n", "") < 0)
			return 0;
		if (fprintf(f,
		    "%16s});\n"
		    "%12s}\n"
		    "%12sOk(())\n", "", "", "") < 0)
			return 0;
		break;
	case STYPE_LIST:
		if (fprintf(f,
		    "%12slet mut vec = Vec::new();\n"
		    "%12swhile let Some(row) = rows.next()? {\n"
		    "%16slet mut i = 0;\n"
		    "%16slet %sobj = self.db_%s_fill(&row, &mut i)?;\n",
		    "", "", "", "",
		    (rs->flags & STRCT_HAS_NULLREFS) ? "mut " : "",
		    rs->name) < 0)
			return 0;
		if (rs->flags & STRCT_HAS_NULLREFS)
		       if (fprintf(f,
			   "%16sself.db_%s_reffind(&mut obj);\n",
			   "", rs->name) < 0)
			       return 0;
		gen_query_checkpass(f, s, 0);
		if (fprintf(f,
		    "%16svec.push(objs::%s {\n"
		    "%20sdata: obj,\n", "", ret, "") < 0)
			return 0;
		if (!TAILQ_EMPTY(&s->parent->cfg->arq) &&
		    fprintf(f, "%20srole: self.role,\n", "") < 0)
			return 0;
		if (fprintf(f,
		    "%16s});\n"
		    "%12s}\n"
		    "%12sOk(vec)\n", "", "", "") < 0)
			return 0;
		break;
	default:
		if (fprintf(f,
		    "%12sif let Some(row) = rows.next()? {\n"
		    "%16slet count: i64 = row.get(0)?;\n"
		    "%16sreturn Ok(count);\n"
		    "%12s}\n"
		    "%12sErr(rusqlite::Error::QueryReturnedNoRows)\n",
		    "", "", "", "", "") < 0)
			return 0;
		break;
	}

	free(ret);
	return fprintf(f, "%8s}\n", "") >= 0;
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

	if (fprintf(f,
	    "%16sOrtrole::%c%s => {\n"
	    "%20sif ",
	    "", toupper((unsigned char)r->name[0]),
	    r->name + 1, "") < 0)
		return 0;
	if (gen_role(f, r, 3, "newrole", 0, 0) < 0)
		return 0;
	if (fprintf(f, " {\n"
	    "%24sself.role = newrole;\n"
	    "%24sreturn;\n"
	    "%20s}\n"
	    "%16s},\n", 
	    "", "", "", "") < 0)
		return 0;

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_ortctx_dbrole_role(f, rr))
			return 0;

	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_ortctx_dbrole(const struct config *cfg, FILE *f)
{
	const struct role	*r, *rr;

	if (TAILQ_EMPTY(&cfg->rq))
		return 1;

	if (fprintf(f,
	    "%8sfn db_role_current(&self) -> Ortrole {\n"
	    "%12sreturn self.role;\n"
	    "%8s}\n", "", "", "") < 0)
		return 0;

	if (fprintf(f,
	    "%8sfn db_role(&mut self, newrole: Ortrole) {\n"
	    "%12sif self.role == newrole {\n"
	    "%16sreturn;\n"
	    "%12s}\n"
	    "%12sif self.role == Ortrole::None {\n"
	    "%16spanic!(\"role violation\");\n"
	    "%12s}\n",
	    "", "", "", "", "", "", "") < 0)
		return 0;

	if (fprintf(f,
	    "%12smatch self.role {\n"
	    "%16sOrtrole::Default => {\n"
	    "%20sself.role = newrole;\n"
	    "%20sreturn;\n"
	    "%16s},\n",
	    "", "", "", "", "") < 0)
		return 0;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (strcmp(r->name, "all") == 0)
			break;

	assert(r != NULL);
	TAILQ_FOREACH(rr, &r->subrq, entries) 
		if (!gen_ortctx_dbrole_role(f, rr))
			return 0;

	return fprintf(f, "%16s_ => { },\n"
	     "%12s}\n"
	     "%12spanic!(\"role violation\");\n"
	     "%8s}\n", "", "", "", "") >= 0;
}

static int
gen_ortctx(const struct config *cfg, FILE *f)
{
	const struct strct	*s;
	const struct search	*sr;
	const struct update	*u;
	size_t			 pos;

	if (!gen_ortctx_dbrole(cfg, f))
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (!gen_fill(s, f))
			return 0;
		if (!gen_reffind(s, f))
			return 0;
		if (s->ins != NULL && !gen_insert(s, f))
			return 0;
		pos = 0;
		TAILQ_FOREACH(sr, &s->sq, entries)
			if (!gen_query(sr, pos++, f))
				return 0;
		pos = 0;
		TAILQ_FOREACH(u, &s->dq, entries)
			if (!gen_update(u, pos++, f))
				return 0;
		pos = 0;
		TAILQ_FOREACH(u, &s->uq, entries)
			if (!gen_update(u, pos++, f))
				return 0;
	}

	return 1;
}

int
ort_lang_rust(const struct ort_lang_rust *args,
	const struct config *cfg, FILE *f)
{
	struct ort_lang_rust	 tmp;

	if (args == NULL) {
		memset(&tmp, 0, sizeof(struct ort_lang_rust));
		args = &tmp;
	}

	if (!gen_commentv(f, 0, COMMENT_C,
	    "WARNING: automatically generated by ort %s.\n"
	    "DO NOT EDIT!", ORT_VERSION))
		return 0;

	if (fprintf(f,
	    "#[macro_use]\n"
	    "extern crate num_derive;\n"
	    "\n"
	    "pub mod ort {\n"
	    "%4suse libsqlite3_sys;\n"
	    "%4suse rusqlite::{Connection,Result,params,Row};\n", 
	    "", "") < 0)
		return 0;
	if ((cfg->flags & CONFIG_HAS_PASS) && fprintf(f,
	    "%4suse bcrypt::{hash,verify};\n", "") < 0)
		return 0;
	if (fprintf(f,
	    "%4suse num_traits::{FromPrimitive,ToPrimitive};\n"
	    "\n"
	    "%4spub const VERSION: &str = \"%s\";\n"
	    "%4spub const VSTAMP: i64 = %lld;\n",
	    "", "", ORT_VERSION, "", (long long)ORT_VSTAMP) < 0)
		return 0;

	if (!gen_roles(cfg, f))
		return 0;
	if (!gen_data(cfg, f))
		return 0;
	if (!gen_objs(cfg, f))
		return 0;

	if (fprintf(f, "\n%4spub(self) mod stmt {\n", "") < 0)
		return 0;
	if (!gen_aliases(cfg, f))
		return 0;
	if (fprintf(f, "%4s}\n", "") < 0)
		return 0;

	if (fprintf(f, "\n"
	    "%4spub struct Ortctx {\n"
	    "%8sargs: Ortargs,\n"
	    "%8sconn: Connection,\n", "", "", "") < 0)
		return 0;
	if (!TAILQ_EMPTY(&cfg->arq) &&
	    fprintf(f, "%8srole: Ortrole,\n", "") < 0)
		return 0;
	if (fprintf(f, "%4s}\n", "") < 0)
		return 0;

	if (fprintf(f, "\n%4simpl Ortctx {\n", "") < 0)
		return 0;
	if (!gen_ortctx(cfg, f))
		return 0;

	if (fprintf(f,
	    "%8spub(self) fn new(dbname: &str, args: &Ortargs) -> "
	     "Result<Ortctx> {\n"
	    "%12slet conn = Connection::open(dbname)?;\n"
	    "%12sconn.execute(\"PRAGMA foreign_keys=ON\", [])?;\n"
	    "%12sOk(Ortctx {\n"
	    "%16sargs: *args,\n"
	    "%16sconn,\n", 
	    "", "", "", "", "", "") < 0)
		return 0;
	if (!TAILQ_EMPTY(&cfg->arq) &&
	    fprintf(f, "%16srole: Ortrole::Default,\n", "") < 0)
		return 0;
	if (fprintf(f, "%12s})\n%8s}\n%4s}\n", "", "", "") < 0)
		return 0;

	if (fprintf(f, "\n"
            "%4s#[derive(Copy, Clone)]\n"
	    "%4spub struct Ortargs {\n"
	    "%8spub bcrypt_cost: u32,\n"
	    "%4s}\n", "", "", "", "") < 0)
		return 0;

	if (fprintf(f, "\n"
	    "%4spub struct Ortdb {\n"
	    "%8sdbname: String,\n"
	    "%8sargs: Ortargs,\n"
	    "%4s}\n",
	    "", "", "", "") < 0)
		return 0;

	if (fprintf(f, "\n%4simpl Ortdb {\n", "") < 0)
		return 0;

	if (fprintf(f, 
	    "%8spub fn new(dbname: &str) -> Ortdb {\n"
	    "%12sOrtdb {\n"
	    "%16sdbname: dbname.to_string(),\n"
	    "%16sargs: Ortargs {\n"
	    "%20sbcrypt_cost: bcrypt::DEFAULT_COST,\n"
	    "%16s}\n"
	    "%12s}\n"
	    "%8s}\n",
	    "", "", "", "", "", "", "", "") < 0)
		return 0;
	if (fprintf(f, 
	    "%8spub fn new_with_args(dbname: &str, args: Ortargs) -> "
	     "Ortdb {\n"
	    "%12sOrtdb {\n"
	    "%16sdbname: dbname.to_string(),\n"
	    "%16sargs,\n"
	    "%12s}\n"
	    "%8s}\n",
	    "", "", "", "", "", "") < 0)
		return 0;
	if (fprintf(f, 
	    "%8spub fn connect(&self) -> Result<Ortctx> {\n"
	    "%12sOrtctx::new(&self.dbname, &self.args)\n"
	    "%8s}\n"
	    "%4s}\n"
	    "}\n",
	    "", "", "", "") < 0)
		return 0;

	return 1;
}
