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

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "lang.h"

static int
gen_block(FILE *f, const char *cp)
{
	size_t	 sz;

	sz = strlen(cp);
	if (fputs(cp, f) == EOF)
		return 0;
	return (sz && cp[sz - 1] != '\n') ?
		fputc('\n', f) != EOF : 1;
}

static int
gen_bitem(FILE *f, const struct bitidx *bi, const char *bitf)
{

	if (fprintf(f, ".It Dv BITF_%s_%s, BITI_%s_%s\n", 
	    bitf, bi->name, bitf, bi->name) < 0)
		return 0;
	if (bi->doc != NULL && !gen_block(f, bi->doc))
		return 0;
	return 1;
}

static int
gen_bitfs(FILE *f, const struct config *cfg)
{
	const struct bitidx	*bi;
	const struct bitf	*b;
	char			*name, *cp;

	if (TAILQ_EMPTY(&cfg->bq))
		return 1;

	if (fprintf(f, 
	    "Bitfields available:\n"
	    ".Bl -tag -width Ds\n") < 0)
		return 0;
	TAILQ_FOREACH(b, &cfg->bq, entries) {
		if (fprintf(f, 
		    ".It Vt enum %s\n", b->name) < 0)
			return 0;
		if (b->doc != NULL && !gen_block(f, b->doc))
			return 0;
		if (fprintf(f, 
		    ".Bl -tag -width Ds\n") < 0)
			return 0;
		if ((name = strdup(b->name)) == NULL)
			return 0;
		for (cp = name; *cp != '\0'; cp++)
			*cp = toupper((unsigned char)*cp);
		TAILQ_FOREACH(bi, &b->bq, entries)
			if (!gen_bitem(f, bi, name)) {
				free(name);
				return 0;
		}
		free(name);
		if (fprintf(f, ".El\n") < 0)
			return 0;
	}

	return fprintf(f, ".El\n") >= 0;
}

static int
gen_eitem(FILE *f, const struct eitem *ei, const char *enm)
{

	if (fprintf(f, ".It Dv %s_%s\n", enm, ei->name) < 0)
		return 0;
	if (ei->doc != NULL && !gen_block(f, ei->doc))
		return 0;
	return 1;
}

static int
gen_enums(FILE *f, const struct config *cfg)
{
	const struct eitem	*ei;
	const struct enm	*e;
	char			*name, *cp;

	if (TAILQ_EMPTY(&cfg->eq))
		return 1;

	if (fprintf(f, 
	    "Enumerations available:\n"
	    ".Bl -tag -width Ds\n") < 0)
		return 0;
	TAILQ_FOREACH(e, &cfg->eq, entries) {
		if (fprintf(f, 
		    ".It Vt enum %s\n", e->name) < 0)
			return 0;
		if (e->doc != NULL && !gen_block(f, e->doc))
			return 0;
		if (fprintf(f, 
		    ".Bl -tag -width Ds\n") < 0)
			return 0;
		if ((name = strdup(e->name)) == NULL)
			return 0;
		for (cp = name; *cp != '\0'; cp++)
			*cp = toupper((unsigned char)*cp);
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!gen_eitem(f, ei, name)) {
				free(name);
				return 0;
		}
		free(name);
		if (fprintf(f, ".El\n") < 0)
			return 0;
	}

	return fprintf(f, ".El\n") >= 0;
}

static int
gen_roles(FILE *f, const struct config *cfg)
{
	const struct role	*r;

	if (TAILQ_EMPTY(&cfg->rq))
		return 1;

	if (fprintf(f, 
	    ".Ss Roles\n"
	    "The\n"
	    ".Vt enum ort_role\n"
	    "enumeration defines the following roles:\n"
	    ".Bl -tag -width Ds\n") < 0)
		return 0;
	TAILQ_FOREACH(r, &cfg->arq, allentries)
		if (fprintf(f, 
		    ".It Dv ROLE_%s\n", r->name) < 0)
			return 0;
	return fprintf(f, ".El\n") >= 0;
}

static int
gen_field(FILE *f, const struct field *fd)
{
	int	 c = 0;

	if (fprintf(f, ".It Va ") < 0)
		return 0;

	switch (fd->type) {
	case FTYPE_STRUCT:
		c = fprintf(f, "struct %s %s\n", 
			fd->ref->target->parent->name, fd->name);
		break;
	case FTYPE_REAL:
		c = fprintf(f, "double %s\n", fd->name);
		break;
	case FTYPE_BLOB:
		if (fprintf(f, "void *%s\n", fd->name) < 0)
			return 0;
		c = fprintf(f, ".It Va size_t %s_sz\n", fd->name);
		break;
	case FTYPE_DATE:
	case FTYPE_EPOCH:
		c = fprintf(f, "time_t %s\n", fd->name);
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_INT:
		c = fprintf(f, "int64_t %s\n", fd->name);
		break;
	case FTYPE_TEXT:
	case FTYPE_EMAIL:
	case FTYPE_PASSWORD:
		c = fprintf(f, "char *%s\n", fd->name);
		break;
	case FTYPE_ENUM:
		c = fprintf(f, "enum %s %s\n", 
			fd->enm->name, fd->name);
		break;
	default:
		break;
	}

	if (c < 0)
		return 0;
	if (fd->doc != NULL && !gen_block(f, fd->doc))
		return 0;
	return 1;
}

static int
gen_fields(FILE *f, const struct strct *s)
{
	const struct field	*fd;

	if (fprintf(f, ".Pp\n"
	    ".Bl -compact -tag -width Ds\n") < 0)
		return 0;
	TAILQ_FOREACH(fd, &s->fq, entries)
		if (!gen_field(f, fd))
			return 0;
	return fprintf(f, ".El\n") >= 0;
}

static int
gen_strcts(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 1;

	if (fprintf(f, 
	    ".Ss Structures\n"
	    ".Bl -tag -width Ds\n") < 0)
		return 0;
	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (fprintf(f, ".It Vt struct %s\n", s->name) < 0)
			return 0;
		if (s->doc != NULL && !gen_block(f, s->doc))
			return 0;
		if (!gen_fields(f, s))
			return 0;
	}
	return fprintf(f, ".El\n") >= 0;
}

int
ort_lang_c_manpage(const struct ort_lang_c *args,
	const struct config *cfg, FILE *f)
{

	if (fprintf(f, 
	    ".\\\" WARNING: automatically generated by ort-%s.\n"
	    ".\\\" DO NOT EDIT!\n", VERSION) < 0)
		return 0;

	if (fprintf(f,
	    ".Dd $Mdocdate$\n"
	    ".Dt ORT 3\n"
	    ".Os\n"
	    ".Sh NAME\n"
	    ".Nm ort\n"
	    ".Nd functions for your project\n"
	    ".Sh DESCRIPTION\n") < 0)
		return 0;

	if (!gen_roles(f, cfg))
		return 0;

	if (!TAILQ_EMPTY(&cfg->eq) || !TAILQ_EMPTY(&cfg->bq)) {
		if (fprintf(f, 
		    ".Ss User-defined types\n") < 0)
			return 0;
		if (!gen_enums(f, cfg))
			return 0;
		if (!gen_bitfs(f, cfg))
			return 0;
	}

	if (!gen_strcts(f, cfg))
		return 0;

#if 0
	if (args->flags & ORT_LANG_C_CORE) {
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_struct(f, cfg, p))
				return 0;
	}

	if (args->flags & ORT_LANG_C_VALID_KCGI) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "All of the fields we validate.\n"
		    "These are as VALID_XXX_YYY, where XXX is "
		    "the structure and YYY is the field.\n"
		    "Only native types are listed."))
			return 0;
		if (fputs("enum\tvalid_keys {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_valid_enums(f, p))
				return 0;
		if (fputs("\tVALID__MAX\n};\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Validation fields.\n"
		    "Pass this directly into khttp_parse(3) "
		    "to use them as-is.\n"
		    "The functions are \"valid_xxx_yyy\", "
		    "where \"xxx\" is the struct and \"yyy\" "
		    "the field, and can be used standalone.\n"
		    "The form inputs are named \"xxx-yyy\"."))
			return 0;
		if (fputs("extern const struct kvalid "
			   "valid_keys[VALID__MAX];\n\n", f) == EOF)
			return 0;
	}

	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Possible error returns from jsmn_parse(), "
		    "if returning a <0 error code."))
			return 0;
		if (fputs("enum jsmnerr_t {\n"
		          "\tJSMN_ERROR_NOMEM = -1,\n"
		          "\tJSMN_ERROR_INVAL = -2,\n"
		          "\tJSMN_ERROR_PART = -3\n"
		          "};\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C, 
		    "Type of JSON token"))
			return 0;
		if (fputs("typedef enum {\n"
		          "\tJSMN_UNDEFINED = 0,\n"
		          "\tJSMN_OBJECT = 1,\n"
		          "\tJSMN_ARRAY = 2,\n"
		          "\tJSMN_STRING = 3,\n"
		          "\tJSMN_PRIMITIVE = 4\n"
		          "} jsmntype_t;\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "JSON token description."))
			return 0;
		if (fputs("typedef struct {\n"
		          "\tjsmntype_t type;\n"
		          "\tint start;\n"
		          "\tint end;\n"
		          "\tint size;\n"
		          "} jsmntok_t;\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "JSON parser. Contains an array of token "
		    "blocks available. Also stores the string "
		    "being parsed now and current position in "
		    "that string."))
			return 0;
		if (fputs("typedef struct {\n"
		          "\tunsigned int pos;\n"
		          "\tunsigned int toknext;\n"
		          "\tint toksuper;\n"
		          "} jsmn_parser;\n\n", f) == EOF)
			return 0;
	}

	if (fputs("__BEGIN_DECLS\n\n", f) == EOF)
		return 0;

	if (args->flags & ORT_LANG_C_DB_SQLBOX) {
		if (!gen_open(f, cfg))
			return 0;
		if (!gen_transaction(f, cfg))
			return 0;
		if (!gen_close(f, cfg))
			return 0;
		if (!TAILQ_EMPTY(&cfg->rq))
			if (!gen_roles(f, cfg))
				return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_database(f, cfg, p))
				return 0;
	}

	if (args->flags & ORT_LANG_C_JSON_KCGI)
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_json_out(f, cfg, p))
				return 0;
	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Check whether the current token in a "
		    "JSON parse sequence \"tok\" parsed from "
		    "\"json\" is equal to a string.\n"
		    "Usually used when checking for key "
		    "equality.\n"
		    "Returns non-zero on equality, zero "
		    "otherwise."))
			return 0;
		if (fputs("int jsmn_eq(const char *json,\n"
		          "\tconst jsmntok_t *tok, "
			  "const char *s);\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Initialise a JSON parser sequence \"p\"."))
			return 0;
		if (fputs("void jsmn_init"
			  "(jsmn_parser *p);\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Parse a buffer \"buf\" of length \"sz\" "
		    "into tokens \"toks\" of length \"toksz\" "
		    "with parser \"p\".\n"
		    "Returns the number of tokens parsed or "
		    "<0 on failure (possible errors described "
		    "in enum jsmnerr_t).\n"
		    "If passed NULL \"toks\", simply computes "
		    "the number of tokens required."))
			return 0;
		if (fputs("int jsmn_parse(jsmn_parser *p, "
			  "const char *buf,\n"
			  "\tsize_t sz, jsmntok_t *toks, "
		          "unsigned int toksz);\n\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_json_parse(f, cfg, p))
				return 0;
	}
	if (args->flags & ORT_LANG_C_VALID_KCGI)
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_valids(f, cfg, p))
				return 0;

	if (fputs("__END_DECLS\n", f) == EOF)
		return 0;

	if (args->guard != NULL &&
	    fputs("\n#endif\n", f) == EOF)
		return 0;

#endif
	return 1;
}

