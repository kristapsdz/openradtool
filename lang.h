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
#ifndef ORT_LANG_H
#define ORT_LANG_H

/*
 * Type of comment.
 */
enum	cmtt {
	COMMENT_C, /* self-contained C comment */
	COMMENT_C_FRAG, /* C w/o open or close */
	COMMENT_C_FRAG_CLOSE, /* C w/o open */
	COMMENT_C_FRAG_OPEN, /* C w/o close */
	COMMENT_JS, /* self-contained jsdoc */
	COMMENT_JS_FRAG, /* jsdoc w/o open or close */
	COMMENT_JS_FRAG_CLOSE, /* jsdoc w/o open */
	COMMENT_JS_FRAG_OPEN, /* jsdoc w/o close */
	COMMENT_SQL /* self-contained SQL comment */
};

enum	langt {
	LANG_JS,
	LANG_C
};

int	 gen_comment(FILE *, size_t, enum cmtt, const char *);
int	 gen_commentv(FILE *, size_t, enum cmtt, const char *, ...)
		__attribute__((format(printf, 4, 5)));

void	 print_commentt(size_t, enum cmtt, const char *);
void	 print_commentv(size_t, enum cmtt, const char *, ...)
		__attribute__((format(printf, 3, 4)));
void	 print_sql_stmts(size_t, const struct strct *, enum langt);
void	 print_sql_enums(size_t, const struct strct *, enum langt);

#endif /* !ORT_LANG_H */
