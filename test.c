/*	$Id$ */
/*
 * Copyright (c) 2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

int
main(void)
{
	struct ort	*sql;
	int64_t		 cid, uid, nuid, val = 1;
	struct user	*u, *u2, *u3;
	const char	*buf = "hello there";

	/* 
	 * Open the database.
	 * It must already have been installed with a schema.
	 */

	if ((sql = db_open_logging("db.db", NULL, warnx, NULL)) == NULL)
		errx(EXIT_FAILURE, "db.db: db_open");

	/* Clear out initial users and companies. */

	db_user_delete(sql);
	db_company_delete(sql);
	
	/* Insert our initial company record. */

	if ((cid = db_company_insert(sql, "foo bar", &val)) < 0)
		errx(EXIT_FAILURE, "db.db: db_company_insert");

	/* Now insert our initial user. */

	uid = db_user_insert(sql, cid, SEX_male,
		"password", "foo@foo.com", strlen(buf), 
		(const void **)&buf, "foo bar");
	if (uid < 0) {
		warnx("are you re-running the test?");
		errx(EXIT_FAILURE, "db.db: db_user_insert (duplicate)");
	}

	/*
	 * Try to insert a user with the same e-mail.
	 * This should fail because the e-mail is unique.
	 */

	nuid = db_user_insert(sql, cid, SEX_male,
		"password", "foo@foo.com", 0, NULL, "foo bar");
	if (nuid >= 0)
		errx(EXIT_FAILURE, "db.db: db_user_insert should fail");

	/* Now fetch the entry by its unique id. */

	if ((u = db_user_get_by_uid_eq(sql, uid)) == NULL)
		errx(EXIT_FAILURE, "db.db: db_user_get_by_uid_eq");

	/* Print it... */

	warnx("company name: %s", u->company.name);
	warnx("company id: %" PRId64, u->company.id);
	if (u->company.has_somenum)
		warnx("company somenum: %" PRId64, u->company.somenum);
	if (u->company.has_somenum)
		warnx("company has unset somenum");
	warnx("user cid: %" PRId64, u->cid);
	warnx("user hash: %s", u->hash);
	warnx("user email: %s", u->email);
	warnx("user name: %s", u->name);
	warnx("user image size: %zu B", u->image_sz);
	warnx("user uid: %" PRId64, u->uid);

	/* Look up the same user by e-mail/password. */

	u2 = db_user_get_creds(sql, "foo@foo.com", "password");

	if (u2 == NULL || u2->uid != u->uid)
		errx(EXIT_FAILURE, "db.db: db_user_get_creds");

	/* 
	 * Now try looking them up with the wrong password. 
	 * This should return NULL.
	 */

	u3 = db_user_get_creds(sql, "foo@foo.com", "password2");
	if (u3 != NULL)
		errx(EXIT_FAILURE, "db.db: db_user_get_creds");

	db_user_free(u);
	db_user_free(u2);
	db_user_free(u3);

	/* Change the user's password. */

	if (!db_user_update_hash_set_by_uid_eq(sql, "password2", uid))
		errx(EXIT_FAILURE, "db.db: db_user_update_hash_by_uid_eq");

	/* 
	 * Now do the same dance, checking for the changed password.
	 * (It should have changed.)
	 */

	u2 = db_user_get_creds(sql, "foo@foo.com", "password");
	if (u2 != NULL)
		errx(EXIT_FAILURE, "db_user_get_creds");

	u3 = db_user_get_creds(sql, "foo@foo.com", "password2");
	if (u3 == NULL)
		errx(EXIT_FAILURE, "db_user_get_creds");

	db_user_free(u2);
	db_user_free(u3);

	/* That's it!  Close up shop. */

	db_close(sql);
	return EXIT_SUCCESS;
}
