/*	$Id$ */
/*
 * Copyright (c) 2017, 2018, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef EXTERN_H
#define EXTERN_H

TAILQ_HEAD(resolveq, resolve);

enum	resolvet {
	RESOLVE_FIELD_BITS,
	RESOLVE_FIELD_ENUM,
	RESOLVE_FIELD_FOREIGN,
	RESOLVE_FIELD_STRUCT,
	RESOLVE_AGGR,
	RESOLVE_DISTINCT,
	RESOLVE_GROUPROW,
	RESOLVE_ORDER,
	RESOLVE_ROLE,
	RESOLVE_SENT,
	RESOLVE_UNIQUE,
	RESOLVE_UP_CONSTRAINT,
	RESOLVE_UP_MODIFIER
};

/*
 * A name that points to something within the configuration.
 * Since there's no order imposed on configuration objects (e.g., having
 * "struct" fields before the actual foreign key), we need to do this
 * after parsing.
 */
struct	resolve {
	enum resolvet	 	type;
	union {
		struct field_foreign {
				struct ref	*result;
				char		*tstrct;
				char		*tfield;
		} field_foreign; /* field ->foo:bar.x int<- */
		struct field_struct {
				struct ref	*result;
				char		*sfield;
		} field_struct; /* field foo struct ->bar<- */
		struct field_bits {
				struct field	*result;
				char		*name;
		} field_bits; /* field foo bits ->bar<- */
		struct field_enum {
				struct field	*result;
				char		*name;
		} field_enum; /* field foo enum ->bar<- */
		struct struct_up_const {
				struct uref	*result;
				char		*name;
		} struct_up_const; /* delete ->bar<-, ... */
		struct struct_up_mod {
				struct uref	*result;
				char		*name;
		} struct_up_mod; /* update ->bar<-: ... */
		struct struct_aggr {
				struct aggr	 *result;
				char		**names;
				size_t		  namesz;
		} struct_aggr; /* ...maxrow ->bar<- */
		struct struct_distinct {
				struct dstnct	 *result;
				char		**names;
				size_t		  namesz;
		} struct_distinct; /* ...distinct ->bar<- */
		struct struct_grouprow {
				struct group	 *result;
				char		**names;
				size_t		  namesz;
		} struct_grouprow; /* ...grouprow ->bar<- */
		struct struct_order {
				struct ord	 *result;
				char		**names;
				size_t		  namesz;
		} struct_order; /* ...order ->bar<- */
		struct struct_role {
				struct roleset	*result;
				char		*name;
		} struct_role; /* ...roles ->bar<- */
		struct struct_sent {
				struct sent	 *result;
				char		**names;
				size_t		  namesz;
		} struct_sent; /* update ->bar<-: ... */
		struct struct_unique {
				struct nref	*result;
				char		*name;
		} struct_unique; /* unique ->bar<-... */
	};
	TAILQ_ENTRY(resolve)	entries;
};

/*
 * Private information used only within the parsing and linking phase,
 * and not exported to the final configuration.
 */
struct	config_private {
	struct resolveq		 rq; /* resolution requests */
};

void	 ort_msgv(struct config *, enum msgtype, 
		const char *, int, const struct pos *, 
		const char *, va_list);
void	 ort_msg(struct config *, enum msgtype, 
		const char *, int, const struct pos *, 
		const char *, ...);

#endif /* !EXTERN_H */
