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
#ifndef LANG_C_H
#define LANG_C_H

/*
 * Determines whether we should generate allocation functions used by
 * queries: if we have no queries, don't generate these functions.
 */
struct	filldep {
	const struct strct	*p; /* needs allocation functions */
	unsigned int		 need; /* do we need extras? */
#define	FILLDEP_FILL_R		 0x01 /* generate fill_r */
#define	FILLDEP_REFFIND		 0x02 /* ...reffind (XXX: unused) */
	TAILQ_ENTRY(filldep)	 entries;
};

TAILQ_HEAD(filldepq, filldep);

int	gen_func_db_close(FILE *, int);
int	gen_func_db_free(FILE *, const struct strct *, int);
int	gen_func_db_freeq(FILE *, const struct strct *, int);
int	gen_func_db_insert(FILE *, const struct strct *, int);
int	gen_func_db_open(FILE *, int);
int	gen_func_db_open_logging(FILE *, int);
int	gen_func_db_role(FILE *, int);
int	gen_func_db_role_current(FILE *, int);
int	gen_func_db_role_stored(FILE *, int);
int	gen_func_db_search(FILE *, const struct search *, int);
int	gen_func_db_set_logging(FILE *, int);
int	gen_func_db_trans_commit(FILE *, int);
int	gen_func_db_trans_open(FILE *, int);
int	gen_func_db_trans_rollback(FILE *, int);
int	gen_func_db_update(FILE *, const struct update *, int);
int	gen_func_json_array(FILE *, const struct strct *, int);
int	gen_func_json_clear(FILE *, const struct strct *, int);
int	gen_func_json_data(FILE *, const struct strct *, int);
int	gen_func_json_free_array(FILE *, const struct strct *, int);
int	gen_func_json_iterate(FILE *, const struct strct *, int);
int	gen_func_json_obj(FILE *, const struct strct *, int);
int	gen_func_json_parse(FILE *, const struct strct *, int);
int	gen_func_json_parse_array(FILE *, const struct strct *, int);
int	gen_func_valid(FILE *, const struct field *, int);

int	gen_filldep(struct filldepq *, const struct strct *, unsigned int);
const struct filldep *
	get_filldep(const struct filldepq *, const struct strct *);

const char	*get_optype_str(enum optype);
const char	*get_modtype_str(enum modtype);
const char	*get_stype_str(enum stype);
const char	*get_ftype_str(enum ftype);

#endif /* !LANG_C_H */
