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

/*
 * Use standard (but unsafe) Linux tools for salting.
 * Not secure.
 */

static const char *
_gensalt(void)
{
	size_t		  i;
	static char 	  salt[] = "$1$........";
	const char *const seedchars =
		"./0123456789ABCDEFGHIJKLMNOPQRST"
		"UVWXYZabcdefghijklmnopqrstuvwxyz";

	/* According to crypt(3), salt is at most 8 bytes. */

	for (i = 0; i < 8; i++)
		salt[i + 3] = seedchars[random() % 64];

	return salt;
}
