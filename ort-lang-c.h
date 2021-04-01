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
#ifndef ORT_LANG_C_H
#define ORT_LANG_C_H

#define ORT_LANG_C_CORE	 	 0x01
#define	ORT_LANG_C_JSON_KCGI	 0x02
#define	ORT_LANG_C_JSON_JSMN	 0x04
#define	ORT_LANG_C_VALID_KCGI	 0x08
#define ORT_LANG_C_DB_SQLBOX	 0x10

struct	ort_lang_c {
	const char		*guard;
	const char		*header;
	unsigned int		 flags;
	unsigned int		 includes;
	const char		*ext_b64_ntop;
	const char		*ext_jsmn;
	const char		*ext_gensalt;
};

int	ort_lang_c_header(const struct ort_lang_c *,
		const struct config *, FILE *);
int	ort_lang_c_source(const struct ort_lang_c *, 
		const struct config *, FILE *f);
int	ort_lang_c_manpage(const struct ort_lang_c *,
		const struct config *, FILE *);

#endif /* !ORT_LANG_C_H */
