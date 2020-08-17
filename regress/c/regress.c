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
#include <sys/types.h>

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <kcgi.h>
#include <kcgiregress.h>

#include "regress.h"

#define	URL "http://localhost:17123/index.json"

struct	ptrs {
	int		(*client)(long, const char *, size_t);
	int		(*server)(const char *);
	const char	 *fname;
};

static size_t
local_parse(void *dat, size_t sz, size_t nm, void *arg)
{

	return kcgi_buf_write(dat, nm * sz, arg) != KCGI_OK ?
		0 : nm * sz;
}

static int
local_client(void *arg)
{
	const struct ptrs	*p = arg;
	CURL			*curl;
	int			 rc;
	struct kcgi_buf	 	 buf;
	long			 http;

	memset(&buf, 0, sizeof(struct kcgi_buf));

	if ((curl = curl_easy_init()) == NULL)
		return 0;
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, local_parse);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

	if (curl_easy_perform(curl) != CURLE_OK) {
		free(buf.buf);
		return 0;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
	rc = (*p->client)(http, buf.buf, buf.sz);

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	free(buf.buf);
	return rc;
}

static int
local_server(void *arg)
{
	const struct ptrs	*p = arg;

	return (*p->server)(p->fname);
}

int
regress(int (*client)(long, const char *, size_t), 
	int (*server)(const char *), int argc, char *argv[])
{
	struct ptrs	 ptrs;

	if (argc != 2)
		return 1;

	ptrs.client = client;
	ptrs.server = server;
	ptrs.fname = argv[1];

	return kcgi_regress_cgi
		(local_client, &ptrs, local_server, &ptrs) ? 0 : 1;
}
