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
#include <sys/param.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
static void
gen_label_text(const char *cp)
{
	unsigned char	 c;

	while ((c = *cp++) != '\0')
		switch (c) {
		case '\'':
			printf("\\\'");
			break;
		default:
			putchar(c);
			break;
		}
}

/*
 * Print out a characteristic array of language labels.
 * This looks like {_default: 'xxx', en: 'yyy'} and so on.
 * Each language is represented; if not found, an empty string is used.
 * The default language is called "_default".
 */
static void
gen_labels(const struct config *cfg, const struct labelq *q)
{
	const struct label *l;
	size_t		 i;
	const char	*def = NULL;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0) {
			def = l->label;
			break;
		}

	putchar('{');
	for (i = 0; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries) 
			if (l->lang == i)
				break;
		if (l != NULL) {
			printf("%s: \'", i == 0 ? 
				"_default" : cfg->langs[i]);
			gen_label_text(l->label);
			putchar('\'');
		} else if (i > 0 && def != NULL) {
			printf("%s: \'", i == 0 ? 
				"_default" : cfg->langs[i]);
			gen_label_text(def);
			putchar('\'');
		} else
			printf("%s: \'\'", i == 0 ?
				"_default" : cfg->langs[i]);

		if (i < cfg->langsz - 1)
			printf(", ");
	}
	putchar('}');
}

static void
warn_label(const struct config *cfg, const struct labelq *q, 
	const struct pos *p, const char *name, const char *sub, 
	const char *type)
{
	size_t	 	 i;
	int		 hasdef;
	const struct label *l;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	if (!(hasdef = (l != NULL)))
		fprintf(stderr, "%s:%zu: %s%s%s: "
			"%s jslabel not defined\n",
			p->fname, p->line, name,
			sub != NULL ? "." : "", 
			sub != NULL ? sub : "", type);

	for (i = 1; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries)
			if (l->lang == i)
				break;
		if (l != NULL)
			continue;
		fprintf(stderr, "%s:%zu: %s%s%s: %s "
			"jslabel.%s not defined: %s\n",
			p->fname, p->line, name,
			sub != NULL ? "." : "", 
			sub != NULL ? sub : "",
			type, cfg->langs[i],
			hasdef ? "using default" :
			"using empty string");
	}
}

/*
 * Generate the documentation for each operation we support in
 * _fill() (e.g., _fillField()).
 * Don't generate documentation for fields we ever export.
 */
static void
gen_jsdoc_field(const struct field *f)
{
	const char	*ifexp, *ifnull;

	ifexp = f->rolemap != NULL ? " (if exported)" : "";
	ifnull = (f->flags & FIELD_NULL) ? " (if non-null)" : "";

	if (f->type == FTYPE_PASSWORD || 
	    (f->flags & FIELD_NOEXPORT))
		return;

	if (f->flags & FIELD_NULL) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-has-%s`: *hide* class removed if "
			"value is not null, otherwise it is added%s",
			f->parent->name, f->name, ifexp);
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-no-%s`: *hide* class added if "
			"value is not null, otherwise it is removed%s",
			f->parent->name, f->name, ifexp);
	} 

	if (f->type == FTYPE_STRUCT) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-obj`: invoke {@link "
			"%s#fillInner} with **%s** data%s%s",
			f->parent->name, f->name, 
			f->ref->target->parent->name, f->name,
			ifnull, ifexp);
	} else {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-enum-select`: sets or unsets the "
			"`selected` attribute for non-inclusive "
			"descendent `<option>` elements depending on "
			"whether the value matches%s%s%s",
			f->parent->name, f->name, ifnull, ifexp,
			f->type == FTYPE_BLOB ?
			" (the base64 encoded value)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-value-checked`: sets or unsets "
			"the `checked` attribute depending on whether "
			"the value matches%s%s%s",
			f->parent->name, f->name, ifnull, ifexp,
			f->type == FTYPE_BLOB ?
			" (the base64 encoded value)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-text`: replace contents "
			"with **%s** data%s%s%s",
			f->parent->name, f->name, f->name,
			ifnull, ifexp, f->type == FTYPE_BLOB ?
			" (the base64 encoded value)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-value`: replace `value` "
			"attribute with **%s** data%s%s%s",
			f->parent->name, f->name, f->name,
			ifnull, ifexp, f->type == FTYPE_BLOB ?
			" (the base64 encoded value)" : "");
	}

	if (f->type == FTYPE_DATE ||
	    f->type == FTYPE_EPOCH) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-date-value`: set the element's "
			"`value` to the ISO-8601 date format of the "
			"data%s%s", f->parent->name, f->name, 
			ifexp, ifnull);
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-date-text`: like "
			"`%s-%s-date-value`, but replacing textual "
			"content%s%s", f->parent->name, f->name, 
			f->parent->name, f->name, ifexp, ifnull);
	}

	if (f->type == FTYPE_BIT ||
	    f->type == FTYPE_BITFIELD)
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-bits-checked`: set the `checked` "
			"attribute when the bit index of the "
			"element's `value` is set in the data as "
			"a bit-field%s%s",
			f->parent->name, f->name, ifexp, ifnull);
}

/*
 * Generate calls to _fillField, _fillDateValue, and _fillBitsChecked
 * with properly wrapping lines.
 * Don't generate calls if we don't export the value.
 */
static void
gen_js_field(const struct field *f)
{
	char	*buf = NULL;
	int	 rc;
	size_t	 col;

	if (f->type == FTYPE_PASSWORD ||
	    (f->flags & FIELD_NOEXPORT))
		return;

	if (f->type == FTYPE_STRUCT) {
		if (f->rolemap != NULL)
			rc = (f->ref->source->flags & FIELD_NULL) ?
				asprintf(&buf, 
					"\t\t\t\ttypeof o.%s === "
					  "\'undefined\' ? undefined :\n"
					"\t\t\t\to.%s === null ? null :\n"
					"\t\t\t\tnew %s(o.%s)", 
					f->name, f->name,
					f->ref->target->parent->name, 
					f->name) :
				asprintf(&buf, 
					"\t\t\t\ttypeof o.%s === "
					  "\'undefined\' ?\n"
					"\t\t\t\tundefined : new %s(o.%s)", 
					f->name,
					f->ref->target->parent->name, 
					f->name);
		else
			rc = (f->ref->source->flags & FIELD_NULL) ?
				asprintf(&buf, 
					"\t\t\t\to.%s === null ? null :\n"
					"\t\t\t\tnew %s(o.%s)", 
					f->name,
					f->ref->target->parent->name, 
					f->name) :
				asprintf(&buf, "\t\t\t\tnew %s(o.%s)", 
					f->ref->target->parent->name, 
					f->name);
		if (rc == -1)
			err(EXIT_FAILURE, "asprintf");
	}

	col = 24;
	fputs("\t\t\t", stdout);
	col += (rc = printf("_fillField(e,")) > 0 ? rc : 0;

	/* Structure name. */

	if (col + strlen(f->parent->name) + 4 >= 72) {
		col = 32;
		fputs("\n\t\t\t\t", stdout);
	} else {
		col++;
		putchar(' ');
	}
	col += (rc = printf("\'%s\',", f->parent->name)) > 0 ? rc : 0;

	/* Field name. */

	if (col + strlen(f->name) + 4 >= 72) {
		col = 32;
		fputs("\n\t\t\t\t", stdout);
	} else {
		col++;
		putchar(' ');
	}
	col += (rc = printf("\'%s\',", f->name)) > 0 ? rc : 0;

	/* "Custom." */

	if (col + 7 >= 72) {
		col = 32;
		fputs("\n\t\t\t\t", stdout);
	} else {
		col++;
		putchar(' ');
	}
	col += (rc = printf("custom,")) > 0 ? rc : 0;

	/* Field in interface and "inc". */

	if (col + 7 + strlen(f->name) >= 72) {
		col = 32;
		fputs("\n\t\t\t\t", stdout);
	} else {
		col++;
		putchar(' ');
	}
	col += (rc = printf("o.%s, inc,", f->name)) > 0 ? rc : 0;

	/* True or false. */

	if (col + 6 >= 72) {
		col = 32;
		fputs("\n\t\t\t\t", stdout);
	} else {
		col++;
		putchar(' ');
	}
	rc = printf("%s,",
		(f->flags & FIELD_NULL) ||
		(f->type == FTYPE_STRUCT && 
		 (f->ref->source->flags & FIELD_NULL)) ?
		"true" : "false");

	col += rc > 0 ? rc : 0;

	/* Nested object or null. */

	if (buf != NULL)
		printf("\n%s);\n", buf);
	else 
		puts("null);");

	free(buf);

	if (f->type == FTYPE_BIT || f->type == FTYPE_BITFIELD)
		printf("\t\t\t_fillBitsChecked"
			"(e, '%s-%s', o.%s, inc);\n",
			f->parent->name, f->name, f->name);
	if (f->type == FTYPE_DATE || f->type == FTYPE_EPOCH)
		printf("\t\t\t_fillDateValue"
			"(e, '%s-%s', o.%s, inc);\n",
			f->parent->name, f->name, f->name);
}

/*
 * Generate a class-level method prototype.
 * If "priv" > 1, it is marked as static, if simply > 0, it is private.
 * Otherwise it is neither and assumed public.
 */
static void
gen_class_proto(int priv, const char *ret, const char *func, ...)
{
	va_list	 	 ap;
	int		 first = 1, rc;
	const char	*name, *type;
	size_t		 sz, col = 8;

	fputs("\t\t", stdout);

	if (priv > 1) {
		fputs("static ", stdout);
		col += 7;
	} else if (priv > 0) {
		fputs("private ", stdout);
		col += 8;
	}

	rc = printf("%s(", func);
	col += rc > 0 ? (size_t)rc : 0;

	va_start(ap, func);
	while ((name = va_arg(ap, char *)) != NULL) {
		if (first == 0) {
			rc = printf(", ");
			col += rc > 0 ? (size_t)rc : 0;
		}
		sz = strlen(name);
		type = va_arg(ap, char *);
		assert(type != NULL);
		if (sz + 2 + strlen(type) + col >= 72) {
			fputs("\n\t\t\t", stdout);
			col = 24;
		}
		rc = printf("%.*s", (int)sz, name);
		col += rc > 0 ? (size_t)rc : 0;
		rc =printf(": %s", type);
		col += rc > 0 ? (size_t)rc : 0;
		first = 0;
	}
	va_end(ap);
	rc = printf("): ");
	col += rc > 0 ? (size_t)rc : 0;
	if (col + strlen(ret) >= 72)
		fputs("\n\t\t\t", stdout);
	printf("%s\n", ret);
}

/*
 * Main driver function.
 * This emits the top-level structure.
 */
void
gen_javascript(const struct config *cfg, const char *priv, int privfd)
{
	const struct strct	*s;
	const struct field	*f;
	const struct bitf	*bf;
	const struct bitidx	*bi;
	const struct enm	*e;
	const struct eitem	*ei;
	char			*obj, *objarray;
	char			 buf[BUFSIZ];
	int64_t			 maxvalue;
	ssize_t			 ssz;

	puts("namespace ort {");

	while ((ssz = read(privfd, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, (size_t)ssz, stdout);
	if (ssz == -1)
		err(EXIT_FAILURE, "%s", priv);

	puts("\n"
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
	     "\t\tname: string, val: number|null) => void;");

	TAILQ_FOREACH(s, &cfg->sq, entries)
		printf("\texport type DCbStruct%s = (e: HTMLElement,\n"
		       "\t\tname: string, val: ort.%sData|null) "
			"=> void;\n", s->name, s->name);

	puts("");
	print_commentt(1, COMMENT_JS,
		"All possible custom callbacks for this "
		"ort configuration.");
	puts("\texport interface DataCallbacks\n"
	     "\t{\n"
	     "\t\t[key: string]: any;");
	TAILQ_FOREACH(s, &cfg->sq, entries) {
		printf("\t\t'%s'?: DCbStruct%s|DCbStruct%s[];\n",
			s->name, s->name, s->name);
		TAILQ_FOREACH(f, &s->fq, entries) {
			if (f->type == FTYPE_PASSWORD ||
			    ((f->flags & FIELD_NOEXPORT)))
				continue;
			if (f->type == FTYPE_STRUCT)
				printf("\t\t\'%s-%s\'?: "
					"DCbStruct%s|DCbStruct%s[];\n",
					s->name, f->name, 
					f->ref->target->parent->name, 
					f->ref->target->parent->name);
			else
				printf("\t\t'%s-%s'?: DCb%s%s|"
					"DCb%s%s[];\n", s->name, 
					f->name, cbtypes[f->type], 
					(f->flags & FIELD_NULL) ? 
					"Null" : "", 
					cbtypes[f->type],
					(f->flags & FIELD_NULL) ? 
					"Null" : "");
		}
	}
	puts("\t}\n");

	/*
	 * If we have a TypeScript file, then define each of the JSON
	 * objects as interfaces.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (s->doc != NULL)
			print_commentt(1, COMMENT_JS, s->doc);
		printf("\texport interface %sData\n"
		       "\t{\n", s->name);
		TAILQ_FOREACH(f, &s->fq, entries) {
			if (f->type == FTYPE_PASSWORD ||
			    (f->flags & FIELD_NOEXPORT))
				continue;
			if (f->doc != NULL &&
			    (f->type == FTYPE_STRUCT ||
			     types[f->type] != NULL))
				print_commentt(2, COMMENT_JS, f->doc);
			if (f->type == FTYPE_STRUCT)
				printf("\t\t%s%s: %sData;\n", f->name,
					f->rolemap != NULL ? "?" : "",
					f->ref->target->parent->name);
			else 
				printf("\t\t%s%s: %s;\n", f->name, 
					f->rolemap != NULL ? "?" : "",
					types[f->type]);
		}
		puts("\t}\n");
	}

	/* Generate classes for each structure. */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (asprintf(&obj, 
		    "%sData|%sData[]", s->name, s->name) < 0)
			err(EXIT_FAILURE, NULL);
		if (asprintf(&objarray, "%sData[]", s->name) < 0)
			err(EXIT_FAILURE, NULL);

		print_commentv(1, COMMENT_JS,
			"Writes {@link %sData} into a DOM tree.",
			s->name);
		printf("\texport class %s {\n"
		       "\t\treadonly obj: %sData|%sData[];\n",
		       s->name, s->name, s->name);

		/* Constructor. */

		print_commentt(2, COMMENT_JS,
			"@param obj The object(s) to write.");
		printf("\t\tconstructor(o: %sData|%sData[])\n"
		       "\t\t{\n"
		       "\t\t\tthis.obj = o;\n"
		       "\t\t}\n\n", s->name, s->name);

		/* fill() method. */

		print_commentv(2, COMMENT_JS_FRAG_OPEN,
			"Writes {@link %sData} into the given "
			"element. If constructed with an array, the "
			"first element is used.  Elements within (and "
			"including) the element having the following "
			"classes are manipulated as follows:", s->name);
		print_commentt(2, COMMENT_JS_FRAG, "");
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_jsdoc_field(f);
		print_commentt(2, COMMENT_JS_FRAG, "");
		print_commentt(2, COMMENT_JS_FRAG_CLOSE,
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
			"the generic classes are filled.");
		gen_class_proto(0, "void", "fill",
			"e", "HTMLElement|null", 
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tif (e !== null)\n"
		     "\t\t\t\tthis._fill(e, this.obj, true, custom);\n"
		     "\t\t}\n");

		/* fillInner() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fill} but not including the "
			"passed-in element.\n"
			"@param e The DOM element.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillInner",
			"e", "HTMLElement|null", 
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tif (e !== null)\n"
		     "\t\t\t\tthis._fill(e, this.obj, false, custom);\n"
		     "\t\t}\n");

		/* fillByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fill} but instead of accepting a "
			"single element to fill, filling into all "
			"elements (inclusive) matching the given "
			"class name beneath (inclusive) the element.\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillByClass",
			"e", "HTMLElement|null", 
			"name", "string",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tif (e !== null)\n"
		     "\t\t\t\tthis._fillByClass(e, name, true, custom);\n"
		     "\t\t}\n");

		/* fillInnerByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillByClass} but not inclusive "
			"the root element and class matches.\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillInnerByClass",
			"e", "HTMLElement|null", 
			"name", "string",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tif (e !== null)\n"
		     "\t\t\t\tthis._fillByClass(e, name, false, custom);\n"
		     "\t\t}\n");

		/* _fill() private method. */

		gen_class_proto(1, "void", "_fill",
			"e", "HTMLElement", 
			"obj", obj,
			"inc", "boolean",
			"custom?", "DataCallbacks|null", NULL);
		printf("\t\t{\n"
		       "\t\t\tif (obj instanceof Array && obj.length === 0)\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\tconst o: %sData =\n"
		       "\t\t\t\t(obj instanceof Array) ? obj[0] : obj;\n"
		       "\t\t\tif (typeof custom === 'undefined')\n"
		       "\t\t\t\tcustom = null;\n", s->name);
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_js_field(f);
		printf("\t\t\tif (custom !== null &&\n"
		       "\t\t\t    typeof custom[\'%s\'] !== \'undefined\') {\n"
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
		       s->name, s->name, s->name, s->name);

		/* _fillByClass() private method. */

		gen_class_proto(1, "void", "_fillByClass",
			"e", "HTMLElement", 
			"name", "string",
			"inc", "boolean",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet i: number;\n"
		     "\t\t\tconst list: HTMLElement[] = \n"
		     "\t\t\t\t_elemList(e, name, inc);\n"
	     	     "\t\t\tfor (i = 0; i < list.length; i++)\n"
		     "\t\t\t\tthis._fill(list[i], this.obj, "
		     	"inc, custom);\n"
		     "\t\t}\n");

		/* fillArrayOrHide() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray}, but hiding an "
			"element if the array is empty or null.\n"
			"@param e The DOM element.\n"
			"@param tohide DOM element to hide.\n"
			"@param o The array (or object) to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArrayOrHide", 
			"e", "HTMLElement|null",
			"tohide", "HTMLElement|null",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
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
		     "\t\t}\n");

		/* fillArrayOrShow() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray}, but showing an "
			"element if the array is empty or null.\n"
			"@param e The DOM element.\n"
			"@param toshow The DOM element to show.\n"
			"@param o The array or object to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArrayOrShow", 
			"e", "HTMLElement|null",
			"toshow", "HTMLElement|null",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
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
		     "\t\t}\n");

		/* fillArray() method. */

		print_commentt(2, COMMENT_JS,
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
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArray", 
			"e", "HTMLElement|null",
			"custom?", "DataCallbacks|null", NULL);
		printf("\t\t{\n"
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
		       "\t\t}\n\n", s->name);

		/* fillArrayByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray} but instead of "
			"accepting a single element to fill, filling "
			"all elements by class name beneath the "
			"given root (non-inclusive).\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillArrayByClass", 
			"e", "HTMLElement|null",
			"name", "string",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet i: number;\n"
		     "\t\t\tconst list: HTMLElement[] =\n"
		     "\t\t\t\t_elemList(e, name, false);\n"
	     	     "\t\t\tfor (i = 0; i < list.length; i++)\n"
		     "\t\t\t\tthis.fillArray(list[i], custom);\n"
		     "\t\t}\n\n"
		     "\t}\n");

		free(obj);
		free(objarray);
	}

	TAILQ_FOREACH(bf, &cfg->bq, entries) {
		if (bf->doc != NULL)
			print_commentt(1, COMMENT_JS, bf->doc);
		printf("\texport class %s {\n", bf->name);
		maxvalue = -INT64_MAX;
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			if (bi->doc != NULL)
				print_commentt(2, COMMENT_JS, bi->doc);
			printf("\t\tstatic readonly "
				 "BITF_%s: Long = Long.fromStringZero(\'%" PRIu64 "\');\n"
				"\t\tstatic readonly "
			      	 "BITI_%s: number = %" PRId64 ";\n",
				bi->name, 1LLU << bi->value,
				bi->name, bi->value);
			if (bi->value > maxvalue)
				maxvalue = bi->value;
		}

		/* Now the maximum enumeration value. */

		print_commentt(2, COMMENT_JS,
			"One larger than the largest bit index.");
		printf("\t\tstatic readonly "
			"BITI__MAX: number = %" PRId64 ";\n", 
			maxvalue + 1);

		warn_label(cfg, &bf->labels_unset, &bf->pos,
			bf->name, NULL, "bits isunset");
		warn_label(cfg, &bf->labels_null, &bf->pos,
			bf->name, NULL, "bits isnull");

		puts("");
		print_commentt(2, COMMENT_JS,
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
			"@param v The bitfield.");
		gen_class_proto(2, "void", "format",
			"e", "HTMLElement", 
			"name", "string|null",
			"v", "string|number|null", NULL);
		puts("\t\t{\n"
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
		     	"\'ort-null\', false);");
		printf("\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_null);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (vlong === null) {\n"
		       "\t\t\t\t_classadd(e, \'ort-null\');\n"
		       "\t\t\t\t_repllang(e, ");
		gen_labels(cfg, &bf->labels_null);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (vlong.isZero() && name !== null) {\n"
		       "\t\t\t\t_classaddcl(e, name, "
		     	"\'ort-unset\', false);\n"
		       "\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_unset);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (vlong.isZero()) {\n"
		       "\t\t\t\t_classadd(e, \'ort-unset\');\n"
		       "\t\t\t\t_repllang(e, ");
		gen_labels(cfg, &bf->labels_unset);
		puts(");\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t}\n");
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			warn_label(cfg, &bi->labels, &bi->pos,
				bf->name, bi->name, "item");
			printf("\t\t\tif (!vlong.and(%s.BITF_%s).isZero()) {\n"
			       "\t\t\t\tconst res: string = _strlang(", 
			       bf->name, bi->name);
			gen_labels(cfg, &bi->labels);
			puts(");\n"
			     "\t\t\t\tif (res.length)\n"
		       	     "\t\t\t\t\ts += "
			     "(i++ > 0 ? ', ' : '') + res;\n"
			     "\t\t\t}");
		}
		puts("\n"
		     "\t\t\tif (name !== null)\n"
		     "\t\t\t\t_replcl(e, name, s, false);\n"
		     "\t\t\telse\n"
		     "\t\t\t\t_repl(e, s);\n"
		     "\t\t}\n"
		     "\t}\n");
	}

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		if (e->doc != NULL)
			print_commentt(1, COMMENT_JS, e->doc);
		printf("\texport class %s {\n", e->name);
		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (ei->doc != NULL)
				print_commentt(2, COMMENT_JS, ei->doc);
			printf("\t\tstatic readonly %s: string = "
				"\'%" PRId64 "\';\n", ei->name, 
				ei->value);
		}

		warn_label(cfg, &e->labels_null, &e->pos,
			e->name, NULL, "enum isnull");

		print_commentt(2, COMMENT_JS,
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
			"@param v The enumeration value.");
		gen_class_proto(2, "void", "format",
			"e", "HTMLElement", 
			"name", "string|null",
			"v", "string|number|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet s: string;\n"
		     "\t\t\tif (name !== null)\n"
		     "\t\t\t\tname += '-label';\n"
		     "\t\t\tif (v === null && name !== null) {\n"
		     "\t\t\t\t_classaddcl(e, name, "
		     	"\'ort-null\', false);");
		printf("\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &e->labels_null);
		puts(");\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t} else if (v === null) {\n"
		     "\t\t\t\t_classadd(e, \'ort-null\');");
		printf("\t\t\t\t_repllang(e, ");
		gen_labels(cfg, &e->labels_null);
		puts(");\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t}\n"
		     "\t\t\tswitch(v.toString()) {");
		TAILQ_FOREACH(ei, &e->eq, entries) {
			warn_label(cfg, &ei->labels, &ei->pos,
				e->name, ei->name, "item");
			printf("\t\t\tcase %s.%s:\n"
			       "\t\t\t\ts = _strlang(",
			       e->name, ei->name);
			gen_labels(cfg, &ei->labels);
			puts(");\n"
			     "\t\t\t\tbreak;");
		}
		puts("\t\t\tdefault:\n"
		     "\t\t\t\ts = '';\n"
		     "\t\t\t\tbreak;\n"
		     "\t\t\t}\n"
		     "\t\t\tif (name !== null)\n"
		     "\t\t\t\t_replcl(e, name, s, false);\n"
		     "\t\t\telse\n"
		     "\t\t\t\t_repl(e, s);\n"
		     "\t\t}\n"
		     "\t}\n");
	}

	puts("}");
}