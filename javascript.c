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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static void
gen_jsdoc_field(const struct field *f)
{

	if (FIELD_NOEXPORT & f->flags || FTYPE_BLOB == f->type)
		return;

	if (FIELD_NULL & f->flags) {
		if (FTYPE_STRUCT == f->type) {
			print_commentv(2, COMMENT_JS_FRAG,
				"%s-%s-obj: invoke %s.fill() method "
				"with %s data (if non-null)",
				f->parent->name, f->name, 
				f->ref->tstrct, f->name);
		} else {
			print_commentv(2, COMMENT_JS_FRAG,
				"%s-%s-text: replace contents "
				"with %s data (if non-null)",
				f->parent->name, f->name, f->name);
		}
		print_commentv(2, COMMENT_JS_FRAG,
			"%s-has-%s: \"hide\" class "
			"removed if %s not null, otherwise "
			"\"hide\" class is added",
			f->parent->name, f->name, f->name);
		print_commentv(2, COMMENT_JS_FRAG,
			"%s-no-%s: \"hide\" class "
			"added if %s not null, otherwise "
			"\"hide\" class is removed",
			f->parent->name, f->name, f->name);
	} else {
		if (FTYPE_STRUCT == f->type) {
			print_commentv(2, COMMENT_JS_FRAG,
				"%s-%s-obj: invoke %s.fill() method "
				"with %s data",
				f->parent->name, f->name, 
				f->ref->tstrct, f->name);
		} else {
			print_commentv(2, COMMENT_JS_FRAG,
				"%s-%s-text: replace contents "
				"with %s data",
				f->parent->name, f->name, f->name);
		}
	}
}

static void
gen_js_field(const struct field *f)
{
	size_t	 indent;

	if (FIELD_NOEXPORT & f->flags || FTYPE_BLOB == f->type)
		return;

	if (FIELD_NULL & f->flags) {
		indent = 4;
		printf("\t\t\tif (null === this.obj.%s) {\n"
		       "\t\t\t\t_hidecl(e, '%s-has-%s');\n"
		       "\t\t\t\t_showcl(e, '%s-no-%s');\n"
		       "\t\t\t} else {\n"
		       "\t\t\t\t_showcl(e, '%s-has-%s');\n"
		       "\t\t\t\t_hidecl(e, '%s-no-%s');\n",
		       f->name, 
		       f->parent->name, f->name,
		       f->parent->name, f->name,
		       f->parent->name, f->name,
		       f->parent->name, f->name);
	} else
		indent = 3;

	if (FTYPE_STRUCT == f->type) 
		print_src(indent,
			"list = e.getElementsByClassName"
			     "('%s-%s-obj');\n"
		        "strct = new %s(this.obj.%s);\n"
		        "for (i = 0; i < list.length; i++) {\n"
		        "strct.fill(list[i]);\n"
		        "}",
		        f->parent->name, f->name, 
		        f->ref->tstrct, f->name);
	else
		print_src(indent,
			"_replcl(e, '%s-%s-text', this.obj.%s);",
			f->parent->name, f->name, f->name);

	if (FIELD_NULL & f->flags)
		puts("\t\t\t}");
}

void
gen_javascript(const struct strctq *sq)
{
	const struct strct *s;
	const struct field *f;

	puts("(function(root) {\n"
	     "\t'use strict';\n"
	     "\n"
	     "\tfunction _repl(e, text)\n"
	     "\t{\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\twhile (e.firstChild)\n"
	     "\t\t\te.removeChild(e.firstChild);\n"
	     "\t\te.appendChild(document.createTextNode(text));\n"
	     "\t}\n"
	     "\n"
	     "\tfunction _replcl(e, name, text)\n"
	     "\t{\n"
	     "\t\tvar list, i;\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = e.getElementsByClassName(name);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_repl(list[i], text);\n"
	     "\t}\n"
	     "\n"
	     "\tfunction _hide(e)\n"
	     "\t{\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn(null);\n"
	     "\t\tif ( ! e.classList.contains('hide'))\n"
	     "\t\t\te.classList.add('hide');\n"
	     "\t\treturn(e);\n"
	     "\t}\n"
	     "\t\n"
	     "\tfunction _hidecl(e, name)\n"
	     "\t{\n"
	     "\t\tvar list, i;\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = e.getElementsByClassName(name);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_hide(list[i]);\n"
	     "\t}\n"
	     "\n"
	     "\tfunction _show(e)\n"
	     "\t{\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn(null);\n"
	     "\t\tif (e.classList.contains('hide'))\n"
	     "\t\t\te.classList.remove('hide');\n"
	     "\t\treturn(e);\n"
	     "\t}\n"
	     "\t\n"
	     "\tfunction _showcl(e, name)\n"
	     "\t{\n"
	     "\t\tvar list, i;\n"
	     "\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = e.getElementsByClassName(name);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_show(list[i]);\n"
	     "\t}\n"
	     "");

	TAILQ_FOREACH(s, sq, entries) {
		print_commentv(1, COMMENT_JS,
			"Represent a \"%s\" object for filling "
			"into a DOM tree.\n"
			"@constructor\n"
			"@param {Object} obj - The %s object.",
			s->name, s->name);
		printf("\tfunction %s(obj)\n"
		       "\t{\n"
		       "\t\tthis.obj = obj;\n",
		       s->name);
		print_commentv(2, COMMENT_JS_FRAG_OPEN,
			"Fill in a \"%s\" object at the given "
			"element in the DOM tree.\n"
			"Elements having the following classes "
			"are manipulated as follows:",
			s->name);
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_jsdoc_field(f);
		print_commentt(2, COMMENT_JS_FRAG_CLOSE,
			"@param {Object} e - The DOM element.");
		puts("\t\tthis.fill = function(e){");
		TAILQ_FOREACH(f, &s->fq, entries)
			if ( ! (FIELD_NOEXPORT & f->flags) &&
			    FTYPE_STRUCT == f->type) {
				puts("\t\t\tvar list, strct, i;");
				break;
			}
		puts("\t\t\tif (null === this.obj || null === e)\n"
		     "\t\t\t\treturn;");
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_js_field(f);
		printf("\t\t};\n"
		       "\t}\n"
		       "\n");
	}

	TAILQ_FOREACH(s, sq, entries)
		printf("\troot.%s = %s;\n", s->name, s->name);

	puts("})(this);");

}
