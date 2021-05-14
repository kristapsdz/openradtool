/*	$Id$ */
/*
 * Copyright (c) 2017--2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-javascript.h"
#include "paths.h"
#include "lang.h"

/*
 * Used for DCbnumber, Dcstring, etc. callback types.
 */
static	const char *cbtypes[FTYPE__MAX] = {
	"integer", /* FTYPE_BIT */
	"integer", /* FTYPE_DATE */
	"integer", /* FTYPE_EPOCH */
	"integer", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
	"string", /* FTYPE_BLOB (base64) */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"integer", /* FTYPE_ENUM */
	"integer", /* FTYPE_BITFIELD */
};

/*
 * Used for the field value type.
 * This will be augmented with "null" possibility and optionalised in
 * case a role is defined on it.
 */
static	const char *types[FTYPE__MAX] = {
	"string|number", /* FTYPE_BIT */
	"string|number", /* FTYPE_DATE */
	"string|number", /* FTYPE_EPOCH */
	"string|number", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
	"string", /* FTYPE_BLOB (base64) */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"string|number", /* FTYPE_ENUM */
	"string|number", /* FTYPE_BITFIELD */
};

/*
 * Escape JavaScript string literal.
 * This escapes backslashes and single apostrophes.
 */
static int
gen_label_text(FILE *f, const char *cp)
{
	unsigned char	 c;

	while ((c = *cp++) != '\0')
		switch (c) {
		case '\'':
			if (fputs("\\\'", f) == EOF)
				return 0;
			break;
		default:
			if (fputc(c, f) == EOF)
				return 0;
			break;
		}

	return 1;
}

/*
 * Print out a characteristic array of language labels.
 * This looks like {_default: 'xxx', en: 'yyy'} and so on.
 * Each language is represented; if not found, an empty string is used.
 * The default language is called "_default".
 */
static int
gen_labels(FILE *f, const struct config *cfg, const struct labelq *q)
{
	const struct label *l;
	size_t		 i;
	const char	*def = NULL;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0) {
			def = l->label;
			break;
		}

	if (fputc('{', f) == EOF)
		return 0;

	for (i = 0; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries) 
			if (l->lang == i)
				break;
		if (l != NULL) {
			if (fprintf(f, "%s: \'", i == 0 ? 
			    "_default" : cfg->langs[i]) < 0)
				return 0;
			if (!gen_label_text(f, l->label))
				return 0;
			if (fputc('\'', f) == EOF)
				return 0;
		} else if (i > 0 && def != NULL) {
			if (fprintf(f, "%s: \'", i == 0 ? 
			    "_default" : cfg->langs[i]) < 0)
				return 0;
			if (!gen_label_text(f, def))
				return 0;
			if (fputc('\'', f) == EOF)
				return 0;
		} else
			if (fprintf(f, "%s: \'\'", i == 0 ?
			    "_default" : cfg->langs[i]) < 0)
				return 0;

		if (i < cfg->langsz - 1 && fputs(", ", f) == EOF)
			return 0;
	}

	return fputc('}', f) != EOF;
}

/*
 * Generate the documentation for each operation we support in
 * _fill() (e.g., _fillField()).
 * Don't generate documentation for fields we ever export.
 */
static int
gen_jsdoc_field(FILE *f, const struct field *fd)
{
	const char	*ifexp, *ifnull;

	ifexp = fd->rolemap != NULL ? " (if exported)" : "";
	ifnull = (fd->flags & FIELD_NULL) ? " (if non-null)" : "";

	if (fd->type == FTYPE_PASSWORD || 
	    (fd->flags & FIELD_NOEXPORT))
		return 1;

	if (fd->flags & FIELD_NULL) {
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-has-%s`: *hide* class removed if "
		    "value is not null, otherwise it is added%s",
		    fd->parent->name, fd->name, ifexp))
			return 0;
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-no-%s`: *hide* class added if "
		    "value is not null, otherwise it is removed%s",
		    fd->parent->name, fd->name, ifexp))
			return 0;
	} 

	if (fd->type == FTYPE_STRUCT) {
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-obj`: invoke {@link "
		    "%s#fillInner} with **%s** data%s%s",
		    fd->parent->name, fd->name, 
		    fd->ref->target->parent->name, fd->name,
		    ifnull, ifexp))
			return 0;
	} else {
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-enum-select`: sets or unsets the "
		    "`selected` attribute for non-inclusive "
		    "descendent `<option>` elements depending on "
		    "whether the value matches%s%s%s",
		    fd->parent->name, fd->name, ifnull, ifexp,
		    fd->type == FTYPE_BLOB ?
		    " (the base64 encoded value)" : ""))
			return 0;
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-value-checked`: sets or unsets "
		    "the `checked` attribute depending on whether "
		    "the value matches%s%s%s",
		    fd->parent->name, fd->name, ifnull, ifexp,
		    fd->type == FTYPE_BLOB ?
		    " (the base64 encoded value)" : ""))
			return 0;
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-text`: replace contents "
		    "with **%s** data%s%s%s",
		    fd->parent->name, fd->name, fd->name,
		    ifnull, ifexp, fd->type == FTYPE_BLOB ?
		    " (the base64 encoded value)" : ""))
			return 0;
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-value`: replace `value` "
		    "attribute with **%s** data%s%s%s",
		    fd->parent->name, fd->name, fd->name,
		    ifnull, ifexp, fd->type == FTYPE_BLOB ?
		    " (the base64 encoded value)" : ""))
			return 0;
	}

	if (fd->type == FTYPE_DATE ||
	    fd->type == FTYPE_EPOCH) {
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-date-value`: set the element's "
		    "`value` to the ISO-8601 date format of the "
		    "data%s%s", fd->parent->name, fd->name, 
		    ifexp, ifnull))
			return 0;
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-date-text`: like "
		    "`%s-%s-date-value`, but replacing textual "
		    "content%s%s", fd->parent->name, fd->name, 
		    fd->parent->name, fd->name, ifexp, ifnull))
			return 0;
	}

	if (fd->type == FTYPE_BIT ||
	    fd->type == FTYPE_BITFIELD)
		if (!gen_commentv(f, 2, COMMENT_JS_FRAG,
		    "- `%s-%s-bits-checked`: set the `checked` "
		    "attribute when the bit index of the "
		    "element's `value` is set in the data as "
		    "a bit-field%s%s",
		    fd->parent->name, fd->name, ifexp, ifnull))
			return 0;

	return 1;
}

/*
 * Generate calls to _fillField, _fillDateValue, and _fillBitsChecked
 * with properly wrapping lines.
 * Don't generate calls if we don't export the value.
 */
static int
gen_js_field(FILE *f, const struct field *fd)
{
	char	*buf = NULL;
	int	 rc;
	size_t	 col;

	if (fd->type == FTYPE_PASSWORD ||
	    (fd->flags & FIELD_NOEXPORT))
		return 1;

	if (fd->type == FTYPE_STRUCT) {
		if (fd->rolemap != NULL)
			rc = (fd->ref->source->flags & FIELD_NULL) ?
				asprintf(&buf, 
					"\t\t\t\ttypeof o.%s === "
					  "\'undefined\' ? undefined :\n"
					"\t\t\t\to.%s === null ? null :\n"
					"\t\t\t\tnew %s(o.%s)", 
					fd->name, fd->name,
					fd->ref->target->parent->name, 
					fd->name) :
				asprintf(&buf, 
					"\t\t\t\ttypeof o.%s === "
					  "\'undefined\' ?\n"
					"\t\t\t\tundefined : new %s(o.%s)", 
					fd->name,
					fd->ref->target->parent->name, 
					fd->name);
		else
			rc = (fd->ref->source->flags & FIELD_NULL) ?
				asprintf(&buf, 
					"\t\t\t\to.%s === null ? null :\n"
					"\t\t\t\tnew %s(o.%s)", 
					fd->name,
					fd->ref->target->parent->name, 
					fd->name) :
				asprintf(&buf, "\t\t\t\tnew %s(o.%s)", 
					fd->ref->target->parent->name, 
					fd->name);
		if (rc == -1)
			return 0;
	}

	col = 37;
	if (fputs("\t\t\t_fillField(e,", f) == EOF)
		return 0;

	/* Structure name. */

	if (col + strlen(fd->parent->name) + 4 >= 72) {
		if (fputs("\n\t\t\t\t", f) == EOF)
			return 0;
		col = 32;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	if ((rc = fprintf(f, "\'%s\',", fd->parent->name)) < 0)
		return 0;
	col += rc;

	/* Field name. */

	if (col + strlen(fd->name) + 4 >= 72) {
		if (fputs("\n\t\t\t\t", f) == EOF)
			return 0;
		col = 32;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	if ((rc = fprintf(f, "\'%s\',", fd->name)) < 0)
		return 0;
	col += rc;

	/* "Custom." */

	if (col + 7 >= 72) {
		if (fputs("\n\t\t\t\t", f) == EOF)
			return 0;
		col = 32;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	if ((rc = fprintf(f, "custom,")) < 0)
		return 0;
	col += rc;

	/* Field in interface and "inc". */

	if (col + 7 + strlen(fd->name) >= 72) {
		if (fputs("\n\t\t\t\t", f) == EOF)
			return 0;
		col = 32;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	if ((rc = fprintf(f, "o.%s, inc,", fd->name)) < 0)
		return 0;
	col += rc;

	/* True or false. */

	if (col + 6 >= 72) {
		if (fputs("\n\t\t\t\t", f) == EOF)
			return 0;
		col = 32;
	} else {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	rc = fprintf(f, "%s,",
		(fd->flags & FIELD_NULL) ||
		(fd->type == FTYPE_STRUCT && 
		 (fd->ref->source->flags & FIELD_NULL)) ?
		"true" : "false");
	if (rc < 0)
		return 0;
	col += rc;

	/* Nested object or null. */

	if (buf != NULL) {
		if (fprintf(f, "\n%s);\n", buf) < 0)
			return 0;
	} else  {
		if (fputs("null);\n", f) == EOF)
			return 0;
	}

	free(buf);

	if (fd->type == FTYPE_BIT || fd->type == FTYPE_BITFIELD)
		if (fprintf(f, "\t\t\t_fillBitsChecked"
		    "(e, '%s-%s', o.%s, inc);\n",
		    fd->parent->name, fd->name, fd->name) < 0)
			return 0;
	if (fd->type == FTYPE_DATE || fd->type == FTYPE_EPOCH)
		if (fprintf(f, "\t\t\t_fillDateValue"
		    "(e, '%s-%s', o.%s, inc);\n",
		    fd->parent->name, fd->name, fd->name) < 0)
			return 0;

	return 1;
}

/*
 * Generate a class-level method prototype.
 * If "priv" > 1, it is marked as static, if simply > 0, it is private.
 * Otherwise it is neither and assumed public.
 */
static int
gen_class_proto(FILE *f, int priv,
	const char *ret, const char *func, ...)
{
	va_list	 	 ap;
	int		 first = 1, rc;
	const char	*name, *type;
	size_t		 sz, col = 8;

	if (fputs("\t\t", f) == EOF)
		return 0;

	if (priv > 1) {
		if (fputs("static ", f) == EOF)
			return 0;
		col += 7;
	} else if (priv > 0) {
		if (fputs("private ", f) == EOF)
			return 0;
		col += 8;
	}

	if ((rc = fprintf(f, "%s(", func)) < 0)
		return 0;
	col += rc;

	va_start(ap, func);
	while ((name = va_arg(ap, char *)) != NULL) {
		if (first == 0) {
			if (fputs(", ", f) == EOF) {
				va_end(ap);
				return 0;
			}
			col += 2;
		}
		sz = strlen(name);
		type = va_arg(ap, char *);
		assert(type != NULL);
		if (sz + 2 + strlen(type) + col >= 72) {
			if (fputs("\n\t\t\t", f) == EOF) {
				va_end(ap);
				return 0;
			}
			col = 24;
		}
		if ((rc = fprintf(f, "%.*s", (int)sz, name)) < 0) {
			va_end(ap);
			return 0;
		}
		col += rc;
		if ((rc = fprintf(f, ": %s", type)) < 0) {
			va_end(ap);
			return 0;
		}
		col += rc;
		first = 0;
	}
	va_end(ap);
	if (fputs("): ", f) == EOF)
		return 0;
	col += 3;
	if (col + strlen(ret) >= 72 && fputs("\n\t\t\t", f) == EOF)
		return 0;
	return fprintf(f, "%s\n", ret) > 0;
}

int
ort_lang_javascript(const struct config *cfg,
	const struct ort_lang_js *args, FILE *f)
{
	const struct strct	*s;
	const struct field	*fd;
	const struct bitf	*bf;
	const struct bitidx	*bi;
	const struct enm	*e;
	const struct eitem	*ei;
	char			*obj, *objarray;
	int64_t			 maxvalue;
	struct ort_lang_js	 tmp;

	if (args == NULL) {
		memset(&tmp, 0, sizeof(struct ort_lang_js));
		args = &tmp;
	}

	if (fprintf(f, "%snamespace ort {\n",
	    (args->flags & ORT_LANG_JS_EXPORT) ? "export " : "") < 0)
		return 0;
	if (args->ext_privMethods != NULL &&
	    fputs(args->ext_privMethods, f) == EOF)
		return 0;

	if (fputs("\n"
	    "\texport type DCbstring = (e: HTMLElement,\n"
	    "\t\tname: string, val: string) => void;\n"
	    "\texport type DCbstringNull = (e: HTMLElement,\n"
	    "\t\tname: string, val: string|null) => void;\n"
	    "\texport type DCbinteger = (e: HTMLElement,\n"
	    "\t\tname: string, val: string|number) => void;\n"
	    "\texport type DCbintegerNull = (e: HTMLElement,\n"
	    "\t\tname: string, val: string|number|null) => void;\n"
	    "\texport type DCbnumber = (e: HTMLElement,\n"
	    "\t\tname: string, val: number) => void;\n"
	    "\texport type DCbnumberNull = (e: HTMLElement,\n"
	    "\t\tname: string, val: number|null) => void;\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (fprintf(f, "\texport type DCbStruct%s = "
		    "(e: HTMLElement,\n"
		    "\t\tname: string, val: %sData|null) "
		    "=> void;\n", s->name, s->name) < 0)
			return 0;

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 1, COMMENT_JS,
	    "All possible custom callbacks for this "
	    "ort configuration."))
		return 0;
	if (fputs("\texport interface DataCallbacks\n"
	    "\t{\n"
	    "\t\t[key: string]: any;\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (fprintf(f, 
		    "\t\t'%s'?: DCbStruct%s|DCbStruct%s[];\n", 
		    s->name, s->name, s->name) < 0)
			return 0;
		TAILQ_FOREACH(fd, &s->fq, entries) {
			if (fd->type == FTYPE_PASSWORD ||
			    (fd->flags & FIELD_NOEXPORT))
				continue;
			if (fd->type == FTYPE_STRUCT) {
				if (fprintf(f, "\t\t\'%s-%s\'?: "
				    "DCbStruct%s|DCbStruct%s[];\n",
				    s->name, fd->name, 
				    fd->ref->target->parent->name, 
				    fd->ref->target->parent->name) < 0)
					return 0;
			} else {
				if (fprintf(f, "\t\t'%s-%s'?: DCb%s%s|"
				    "DCb%s%s[];\n", s->name, 
				    fd->name, cbtypes[fd->type], 
				    (fd->flags & FIELD_NULL) ? 
				    "Null" : "", 
				    cbtypes[fd->type],
				    (fd->flags & FIELD_NULL) ? 
				    "Null" : "") < 0)
					return 0;
			}
		}
	}
	if (fputs("\t}\n\n", f) == EOF)
		return 0;

	/*
	 * If we have a TypeScript file, then define each of the JSON
	 * objects as interfaces.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (!gen_comment(f, 1, COMMENT_JS, s->doc))
			return 0;
		if (fprintf(f, "\texport interface %sData\n"
		    "\t{\n", s->name) < 0)
			return 0;
		TAILQ_FOREACH(fd, &s->fq, entries) {
			if (fd->type == FTYPE_PASSWORD ||
			    (fd->flags & FIELD_NOEXPORT))
				continue;
			if ((fd->type == FTYPE_STRUCT ||
			     types[fd->type] != NULL) &&
			    !gen_comment(f, 2, COMMENT_JS, fd->doc))
				return 0;
			if (fd->type == FTYPE_STRUCT) {
				if (fprintf(f, "\t\t%s%s: %sData;\n", 
				    fd->name,
				    fd->rolemap != NULL ? "?" : "",
				    fd->ref->target->parent->name) < 0)
					return 0;
			} else {
				if (fprintf(f, "\t\t%s%s: %s;\n", 
				    fd->name, 
				    fd->rolemap != NULL ? "?" : "",
				    types[fd->type]) < 0)
					return 0;
			}
		}
		if (fputs("\t}\n\n", f) == EOF)
			return 0;
	}

	/* Generate classes for each structure. */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (asprintf(&obj, 
		    "%sData|%sData[]", s->name, s->name) == -1)
			return 0;
		if (asprintf(&objarray, "%sData[]", s->name) == -1)
			return 0;

		if (!gen_commentv(f, 1, COMMENT_JS,
		    "Writes {@link %sData} into a DOM tree.", s->name))
			return 0;
		if (fprintf(f, "\texport class %s {\n"
		    "\t\treadonly obj: %sData|%sData[];\n",
		    s->name, s->name, s->name) < 0)
			return 0;

		/* Constructor. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "@param obj The object(s) to write."))
			return 0;
		if (fprintf(f, 
		    "\t\tconstructor(o: %sData|%sData[])\n"
		    "\t\t{\n"
		    "\t\t\tthis.obj = o;\n"
		    "\t\t}\n\n", s->name, s->name) < 0)
			return 0;

		/* fill() method. */

		if (!gen_commentv(f, 2, COMMENT_JS_FRAG_OPEN,
		    "Writes {@link %sData} into the given "
		    "element. If constructed with an array, the "
		    "first element is used.  Elements within (and "
		    "including) the element having the following "
		    "classes are manipulated as follows:", s->name))
			return 0;
		if (!gen_comment(f, 2, COMMENT_JS_FRAG, ""))
			return 0;
		TAILQ_FOREACH(fd, &s->fq, entries)
			if (!gen_jsdoc_field(f, fd))
				return 0;
		if (!gen_comment(f, 2, COMMENT_JS_FRAG, ""))
			return 0;
		if (!gen_comment(f, 2, COMMENT_JS_FRAG_CLOSE,
		    "@param e The DOM element.\n"
		    "@param custom The dictionary of functions "
		    "keyed by structure and field name (e.g., "
		    "*foo** structure, **bar** field would be "
		    "`foo-bar`). The value is a function for "
		    "custom handling that accepts the \'e\' value, "
		    "the name of the structure-field, and the "
		    "value of the structure and field. "
		    "You may also specify an array of functions "
		    "instead of a singleton. "
		    "These callbacks are invoked *after* "
		    "the generic classes are filled."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fill",
		    "e", "HTMLElement|null", 
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tif (e !== null)\n"
		    "\t\t\t\tthis._fill(e, this.obj, true, custom);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillInner() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fill} but not including the "
		    "passed-in element.\n"
		    "@param e The DOM element.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill} for details)."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillInner",
		    "e", "HTMLElement|null", 
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tif (e !== null)\n"
		    "\t\t\t\tthis._fill(e, this.obj, false, custom);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillByClass() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fill} but instead of accepting a "
		    "single element to fill, filling into all "
		    "elements (inclusive) matching the given "
		    "class name beneath (inclusive) the element.\n"
		    "@param e The DOM element.\n"
		    "@param name Name of the class to fill.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill} for details)."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillByClass",
		    "e", "HTMLElement|null", 
		    "name", "string",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tif (e !== null)\n"
		    "\t\t\t\tthis._fillByClass(e, name, true, custom);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillInnerByClass() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fillByClass} but not inclusive "
		    "the root element and class matches.\n"
		    "@param e The DOM element.\n"
		    "@param name Name of the class to fill.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill} for details)."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillInnerByClass",
		    "e", "HTMLElement|null", 
		    "name", "string",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tif (e !== null)\n"
		    "\t\t\t\tthis._fillByClass(e, name, false, custom);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* _fill() private method. */

		if (!gen_class_proto(f, 1, "void", "_fill",
		    "e", "HTMLElement", 
		    "obj", obj,
		    "inc", "boolean",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fprintf(f, "\t\t{\n"
		    "\t\t\tif (obj instanceof Array && "
		    "obj.length === 0)\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\tconst o: %sData =\n"
		    "\t\t\t\t(obj instanceof Array) ? obj[0] : obj;\n"
		    "\t\t\tif (typeof custom === 'undefined')\n"
		    "\t\t\t\tcustom = null;\n", s->name) < 0)
			return 0;
		TAILQ_FOREACH(fd, &s->fq, entries)
			if (!gen_js_field(f, fd))
				return 0;
		if (fprintf(f, "\t\t\tif (custom !== null &&\n"
		    "\t\t\t    typeof custom[\'%s\'] !== "
		    "\'undefined\') {\n"
		    "\t\t\t\tif (custom['%s'] instanceof Array) {\n"
		    "\t\t\t\t\tlet i: number;\n"
		    "\t\t\t\t\tfor (i = 0; "
		    "i < custom['%s'].length; i++)\n"
		    "\t\t\t\t\t\tcustom['%s'][i](e, '%s', o);\n"
		    "\t\t\t\t} else\n"
		    "\t\t\t\t\tcustom['%s'](e, '%s', o);\n"
		    "\t\t\t}\n"
		    "\t\t}\n"
		    "\n", s->name, s->name, s->name, 
		    s->name, s->name, s->name, s->name) < 0)
			return 0;

		/* _fillByClass() private method. */

		if (!gen_class_proto(f, 1, "void", "_fillByClass",
		    "e", "HTMLElement", 
		    "name", "string",
		    "inc", "boolean",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet i: number;\n"
		    "\t\t\tconst list: HTMLElement[] = \n"
		    "\t\t\t\t_elemList(e, name, inc);\n"
	     	    "\t\t\tfor (i = 0; i < list.length; i++)\n"
		    "\t\t\t\tthis._fill(list[i], this.obj, "
		    "inc, custom);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillArrayOrHide() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fillArray}, but hiding an "
		    "element if the array is empty or null.\n"
		    "@param e The DOM element.\n"
		    "@param tohide DOM element to hide.\n"
		    "@param o The array (or object) to fill.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill})."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillArrayOrHide", 
		    "e", "HTMLElement|null",
		    "tohide", "HTMLElement|null",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet len: number;\n"
		    "\t\t\tif (null === this.obj)\n"
		    "\t\t\t\tlen = 0;\n"
		    "\t\t\telse if (this.obj instanceof Array)\n"
		    "\t\t\t\tlen = this.obj.length;\n"
		    "\t\t\telse\n"
		    "\t\t\t\tlen = 1;\n"
		    "\t\t\tif (null !== e)\n"
		    "\t\t\t\t_hide(e);\n"
		    "\t\t\tif (null !== tohide)\n"
		    "\t\t\t\t_show(tohide);\n"
		    "\t\t\tthis.fillArray(e, custom);\n"
		    "\t\t\tif (null !== tohide && 0 === len)\n"
		    "\t\t\t\t_hide(tohide);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillArrayOrShow() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fillArray}, but showing an "
		    "element if the array is empty or null.\n"
		    "@param e The DOM element.\n"
		    "@param toshow The DOM element to show.\n"
		    "@param o The array or object to fill.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill})."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillArrayOrShow", 
		    "e", "HTMLElement|null",
		    "toshow", "HTMLElement|null",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet len: number;\n"
		    "\t\t\tif (null === this.obj)\n"
		    "\t\t\t\tlen = 0;\n"
		    "\t\t\telse if (this.obj instanceof Array)\n"
		    "\t\t\t\tlen = this.obj.length;\n"
		    "\t\t\telse\n"
		    "\t\t\t\tlen = 1;\n"
		    "\t\t\tif (null !== e)\n"
		    "\t\t\t\t_hide(e);\n"
		    "\t\t\tif (null !== toshow)\n"
		    "\t\t\t\t_hide(toshow);\n"
		    "\t\t\tthis.fillArray(e, custom);\n"
		    "\t\t\tif (null !== toshow && 0 === len)\n"
		    "\t\t\t\t_show(toshow);\n"
		    "\t\t}\n\n", f) == EOF)
			return 0;

		/* fillArray() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fill} but for an array. If the "
		    "data is not an array, it is remapped as an "
		    "array of one. This will save the first "
		    "element within \'e\', remove all children of "
		    "\'e\', then repeatedly clone the saved "
		    "element and re-append it, filling in the "
		    "cloned subtree with the array (inclusive "
		    "of the subtree root). If the input array is "
		    "empty or null, \'e\' is hidden by using the "
		    "*hide* class. Otherwise, the *hide* class is "
		    "removed.\n"
		    "@param e The DOM element.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill})."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillArray", 
		    "e", "HTMLElement|null",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fprintf(f, "\t\t{\n"
		    "\t\t\tlet i: number;\n"
		    "\t\t\tconst o: %sData[] =\n"
		    "\t\t\t\t(this.obj instanceof Array) ?\n"
		    "\t\t\t\t this.obj : [this.obj];\n"
		    "\n"
		    "\t\t\tif (e === null || e.children.length === 0)\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t_hide(e);\n"
		    "\t\t\tif (o.length === 0 || this.obj === null)\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t_show(e);\n"
		    "\n"
		    "\t\t\tconst row: HTMLElement =\n"
		    "\t\t\t\t<HTMLElement>e.children[0];\n"
		    "\t\t\twhile (e.firstChild !== null)\n"
		    "\t\t\t\te.removeChild(e.firstChild)\n"
		    "\t\t\tfor (i = 0; i < o.length; i++) {\n"
		    "\t\t\t\tconst cln: HTMLElement =\n"
		    "\t\t\t\t\t<HTMLElement>row.cloneNode(true);\n"
		    "\t\t\t\te.appendChild(cln);\n"
		    "\t\t\t\tthis._fill(cln, o[i], true, custom);\n"
		    "\t\t\t}\n"
		    "\t\t}\n\n", s->name) < 0)
		    	return 0;

		/* fillArrayByClass() method. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Like {@link fillArray} but instead of "
		    "accepting a single element to fill, filling "
		    "all elements by class name beneath the "
		    "given root (non-inclusive).\n"
		    "@param e The DOM element.\n"
		    "@param name Name of the class to fill.\n"
		    "@param custom Custom handler dictionary (see "
		    "{@link fill} for details)."))
			return 0;
		if (!gen_class_proto(f, 0, "void", "fillArrayByClass", 
		    "e", "HTMLElement|null",
		    "name", "string",
		    "custom?", "DataCallbacks|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet i: number;\n"
		    "\t\t\tconst list: HTMLElement[] =\n"
		    "\t\t\t\t_elemList(e, name, false);\n"
	     	    "\t\t\tfor (i = 0; i < list.length; i++)\n"
		    "\t\t\t\tthis.fillArray(list[i], custom);\n"
		    "\t\t}\n\n"
		    "\t}\n\n", f) == EOF)
			return 0;

		free(obj);
		free(objarray);
	}

	TAILQ_FOREACH(bf, &cfg->bq, entries) {
		if (!gen_comment(f, 1, COMMENT_JS, bf->doc))
			return 0;
		if (fprintf(f, "\texport class %s {\n", bf->name) < 0)
			return 0;
		maxvalue = -INT64_MAX;
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			if (!gen_comment(f, 2, COMMENT_JS, bi->doc))
				return 0;
			if (fprintf(f, "\t\tstatic readonly "
			    "BITF_%s: Long = Long.fromStringZero"
			    "(\'%" PRIu64 "\');\n"
			    "\t\tstatic readonly "
			    "BITI_%s: number = %" PRId64 ";\n",
			    bi->name, 
			    (uint64_t)1 << (uint64_t)bi->value,
			    bi->name, bi->value) < 0)
				return 0;
			if (bi->value > maxvalue)
				maxvalue = bi->value;
		}

		/* Now the maximum enumeration value. */

		if (!gen_comment(f, 2, COMMENT_JS,
		    "One larger than the largest bit index."))
			return 0;
		if (fprintf(f, "\t\tstatic readonly "
		    "BITI__MAX: number = %" PRId64 ";\n", 
		    maxvalue + 1) < 0)
			return 0;

		if (fputc('\n', f) == EOF)
			return 0;

		if (!gen_comment(f, 2, COMMENT_JS,
		    "For each bit-field item with its bit index "
		    "set in the value, use the item's **jslabel** "
		    "to format a custom label. Any bit-field item "
		    "without a **jslabel** is ignored.  If no "
		    "item is found (or no **jslabel** were found) "
		    "use an empty string. Multiple labels, if "
		    "found, are separated by a comma. This will "
		    "act on *xxx-yyy-label* classes, where *xxx* "
		    "is the structure name and *yyy* is the "
		    "field name.\n"
		    "A null value is represented by the "
		    "**isnull** labels (the `ort-null` class is "
		    "also appended in this case) and for no bits "
		    "by the " "**unset** label (the `ort-unset` "
		    "class is added in this case).\n"
		    "@param e The DOM element.\n"
		    "@param name If non-null, data is written to "
		    "elements under the root with the given class "
		    "name. Otherwise, data is written directly "
		    "into the DOM element.\n"
		    "@param v The bitfield."))
		    	return 0;
		if (!gen_class_proto(f, 2, "void", "format",
		    "e", "HTMLElement", 
		    "name", "string|null",
		    "v", "string|number|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet i: number = 0;\n"
		    "\t\t\tlet s: string = '';\n"
		    "\t\t\tconst vlong: Long|null = "
		    "Long.fromValue(v);\n"
		    "\n"
		    "\t\t\tif (name !== null)\n"
		    "\t\t\t\tname += '-label';\n"
		    "\n"
		    "\t\t\tif (vlong === null && name !== null) {\n"
		    "\t\t\t\t_classaddcl(e, name, "
		    "\'ort-null\', false);\n"
		    "\t\t\t\t_replcllang(e, name, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &bf->labels_null))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t} else if (vlong === null) {\n"
		    "\t\t\t\t_classadd(e, \'ort-null\');\n"
		    "\t\t\t\t_repllang(e, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &bf->labels_null))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t} else if (vlong.isZero() && name !== null) {\n"
		    "\t\t\t\t_classaddcl(e, name, "
		    "\'ort-unset\', false);\n"
		    "\t\t\t\t_replcllang(e, name, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &bf->labels_unset))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t} else if (vlong.isZero()) {\n"
		    "\t\t\t\t_classadd(e, \'ort-unset\');\n"
		    "\t\t\t\t_repllang(e, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &bf->labels_unset))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t}\n\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			if (fprintf(f, 
			    "\t\t\tif (!vlong.and"
			    "(%s.BITF_%s).isZero()) {\n"
			    "\t\t\t\tconst res: string = _strlang(", 
			    bf->name, bi->name) < 0)
				return 0;
			if (!gen_labels(f, cfg, &bi->labels))
				return 0;
			if (fputs(");\n"
			    "\t\t\t\tif (res.length)\n"
		       	    "\t\t\t\t\ts += "
			    "(i++ > 0 ? ', ' : '') + res;\n"
			    "\t\t\t}\n", f) == EOF)
				return 0;
		}
		if (fputs("\n"
		    "\t\t\tif (name !== null)\n"
		    "\t\t\t\t_replcl(e, name, s, false);\n"
		    "\t\t\telse\n"
		    "\t\t\t\t_repl(e, s);\n"
		    "\t\t}\n"
		    "\t}\n\n", f) == EOF)
			return 0;
	}

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		if (!gen_comment(f, 1, COMMENT_JS, e->doc))
			return 0;
		if (fprintf(f, "\texport class %s {\n", e->name) < 0)
			return 0;
		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (!gen_comment(f, 2, COMMENT_JS, ei->doc))
				return 0;
			if (fprintf(f, 
			    "\t\tstatic readonly %s: string = "
			    "\'%" PRId64 "\';\n", ei->name, 
			    ei->value) < 0)
				return 0;
		}

		if (!gen_comment(f, 2, COMMENT_JS,
		    "Uses the enumeration item's **jslabel** " 
		    "(or an empty string if no **jslabel** is " 
		    "defined or there is no matching item "
		    "for the value) to format a custom label. "
		    "This will act on *xxx-yyy-label* classes, "
		    "where *xxx* is the structure name and "
		    "*yyy* is the field name.\n"
		    "A null value is represented by the "
		    "**isnull** labels (the `ort-null` class is "
		    "also appended in this case)\n"
		    "@param e The DOM element.\n"
		    "@param name If non-null, data is written "
		    "to elements under the root with the given "
		    "class name. If null, data is written "
		    "directly into the DOM element.\n"
		    "@param v The enumeration value."))
			return 0;
		if (!gen_class_proto(f, 2, "void", "format",
		    "e", "HTMLElement", 
		    "name", "string|null",
		    "v", "string|number|null", NULL))
			return 0;
		if (fputs("\t\t{\n"
		    "\t\t\tlet s: string;\n"
		    "\t\t\tif (name !== null)\n"
		    "\t\t\t\tname += '-label';\n"
		    "\t\t\tif (v === null && name !== null) {\n"
		    "\t\t\t\t_classaddcl(e, name, "
		    "\'ort-null\', false);\n"
		    "\t\t\t\t_replcllang(e, name, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &e->labels_null))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t} else if (v === null) {\n"
		    "\t\t\t\t_classadd(e, \'ort-null\');\n"
		    "\t\t\t\t_repllang(e, ", f) == EOF)
			return 0;
		if (!gen_labels(f, cfg, &e->labels_null))
			return 0;
		if (fputs(");\n"
		    "\t\t\t\treturn;\n"
		    "\t\t\t}\n"
		    "\t\t\tswitch(v.toString()) {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (fprintf(f, "\t\t\tcase %s.%s:\n"
			    "\t\t\t\ts = _strlang(",
			    e->name, ei->name) < 0)
				return 0;
			if (!gen_labels(f, cfg, &ei->labels))
				return 0;
			if (fputs(");\n"
			    "\t\t\t\tbreak;\n", f) == EOF)
				return 0;
		}
		if (fputs("\t\t\tdefault:\n"
		    "\t\t\t\ts = '';\n"
		    "\t\t\t\tbreak;\n"
		    "\t\t\t}\n"
		    "\t\t\tif (name !== null)\n"
		    "\t\t\t\t_replcl(e, name, s, false);\n"
		    "\t\t\telse\n"
		    "\t\t\t\t_repl(e, s);\n"
		    "\t\t}\n"
		    "\t}\n\n", f) == EOF)
			return 0;
	}

	return fputs("}\n", f) != EOF;
}
