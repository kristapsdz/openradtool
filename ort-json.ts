namespace ortJson {
	/**
	 * Same as "struct pos" in ort(3).
	 */
	export interface posObj {
		fname: string;
		column: number;
		line: number;
	}

	/**
	 * Same as "struct label" in ort(3).
	 */
	export interface labelObj {
		value: string;
		pos: posObj;
	}

	/**
	 * Label dictionary.
	 * Key is language.
	 */
	export interface labelSet {
		[lang: string]: labelObj;
	}

	/**
	 * Same as "struct eitem" in ort(3).
	 */
	export interface enumItemObj {
		pos: posObj;
		doc: string|null;
		value: string|number|null;
		labels: labelSet|null;
	}

	export interface enumItemSet {
		[name: string]: enumItemObj;
	}

	/**
	 * Same as "struct enm" in ort(3).
	 */
	export interface enumObj {
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet|null;
		eq: enumItemSet;
	}

	export interface enumSet {
		[name: string]: enumObj;
	}

	/**
	 * Same as "struct bitidx" in ort(3).
	 */
	export interface bitIndexObj {
		pos: posObj;
		doc: string|null;
		value: string|number|null;
		labels: labelSet|null;
	}

	export interface bitIndexSet {
		[name: string]: bitIndexObj;
	}

	/**
	 * Same as "struct bitf" in ort(3).
	 */
	export interface bitfObj {
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet|null;
		labelsUnset: labelSet|null;
		bq: bitIndexSet;
	}

	export interface bitfSet {
		[name: string]: bitfObj;
	}

	export interface roleObj {
		pos: posObj;
		doc: string|null;
		children: roleSet|null;
	}

	export interface roleSet {
		[name: string]: roleObj;
	}

	/**
	 * Same as "struct fvalid" in ort(3).
	 */
	export interface validObj {
		type: 'eq'|'le'|'gt'|'lt'|'ge';
		/*
		 * Depends on field type.
		 */
		limit: string;
	}

	/**
	 * The names required to uniquely identify a field.
	 */
	export interface fieldPtrObj {
		strct: string;
		field: string;
	};

	/**
	 * Same as "struct ref" in ort(3).
	 */
	export interface refObj {
		target: fieldPtrObj;
		source: fieldPtrObj;
	}

	/**
	 * Same as "struct field" in ort(3).
	 */
	export interface fieldObj {
		pos: posObj;
		doc: string|null;
		type: 'bit'|'date'|'epoch'|'int'|'real'|'blob'|'text'|'password'|'email'|'struct'|'enum'|'bitfield';
		actup: 'none'|'restrict'|'nullify'|'cascade'|'default';
		actdel: 'none'|'restrict'|'nullify'|'cascade'|'default';
		rolemap: string[]|null;
		/**
		 * Only set if we're an enum.
		 */
		enm?: string;
		/**
		 * Only set if we're a bitfield.
		 */
		bitf?: string;
		/**
		 * Only if we're a "struct" or with a reference.
		 */
		ref?: refObj;
		/**
		 * The interpretation of this depends upon the type.
		 */
		def: string|null;
		fvq: validObj[];
	}

	export interface fieldSet {
		[name: string]: fieldObj;
	}

	/**
	 * Same as "struct insert" in ort(3).
	 */
	export interface insertObj {
		pos: posObj;
		rolemap: string[]|null;
	}

	/**
	 * Same as "struct sent" in ort(3).
	 */
	export interface sentObj {
		pos: posObj;
		fname: string;
		op: 'eq'|'ge'|'gt'|'le'|'lt'|'neq'|'like'|'and'|'or'|'streq'|'strneq'|'isnull'|'notnull';
	}

	/**
	 * Same as "struct order" in ort(3).
	 */
	export interface orderObj {
		pos: posObj;
		fname: string;
		op: 'desc'|'asc';
	}

	/**
	 * Same as "struct aggr" in ort(3).
	 */
	export interface aggrObj {
		pos: posObj;
		fname: string;
		op: 'minrow'|'maxrow';
	}

	/**
	 * Same as "struct group" in ort(3).
	 */
	export interface groupObj {
		pos: posObj;
		fname: string;
	}

	/**
	 * Same as "struct dstct" in ort(3).
	 */
	export interface dstnctObj {
		pos: posObj;
		fname: string;
	}

	/**
	 * Same as "struct search" in ort(3).
	 */
	export interface searchObj {
		pos: posObj;
		doc: string|null;
		rolemap: string[]|null;
		/**
		 * Numeric string. 
		 */
		limit: string;
		/**
		 * Numeric string. 
		 */
		offset: string;
		/**
		 * Order is significant because it dictates the parameter
		 * order in the API.
		 */
		snq: sentObj[];
		/**
		 * Order is significant because it dictates the order in
		 * which SQL provides its filters.
		 */
		ordq: orderObj[];
		aggr: aggrObj|null;
		group: groupObj|null;
		dst: dstnctObj|null;
		type: 'search'|'iterate'|'list'|'count';
	}

	export interface searchSet {
		[name: string]: searchObj;
	}

	export interface searchClassObj {
		/**
		 * Have a user-provided name.
		 */
		named: searchSet;
		/**
		 * Defined by search parameters: not named.
		 */
		anon: searchObj[];
	}

	export interface urefObj {
		pos: posObj;
		field: string;
		op: 'eq'|'ge'|'gt'|'le'|'lt'|'neq'|'like'|'and'|'or'|'streq'|'strneq'|'isnull'|'notnull';
		mod: 'concat'|'dec'|'inc'|'set'|'strset';
	}

	/**
	 * Same as "struct update" in ort(3).
	 */
	export interface updateObj {
		pos: posObj;
		doc: string|null;
		rolemap: string[]|null;
		/**
		 * Can contain "all" to represent UPDATE_ALL.
		 */
		flags: string[];
		mrq: urefObj[];
		crq: urefObj[];
	}

	export interface updateSet {
		[name: string]: updateObj;
	}

	export interface updateClassObj {
		/**
		 * Have a user-provided name.
		 */
		named: updateSet;
		/**
		 * Defined by update parameters: not named.
		 */
		anon: updateObj[];
	}

	/**
	 * Same as "strct unique" in ort(3).
	 */
	export interface uniqueObj {
		pos: posObj;
		nq: string[];
	}

	/**
	 * Same as "struct strct" in ort(3).
	 */
	export interface strctObj {
		pos: posObj;
		doc: string|null;
		/**
		 * Never an empty set.
		 */
		fq: fieldSet;
		insert: insertObj|null;
		/**
		 * Unlike "strct" in ort(3), which has all searches
		 * under a common "sq", we split between named and
		 * anonymous searches beneath this object.
		 */
		sq: searchClassObj;
		uq: updateClassObj;
		dq: updateClassObj;
		dn: uniqueObj[];
	}

	export interface strctSet {
		[name: string]: strctObj;
	}

	/**
	 * Same as "struct config" in ort(3).
	 */
	export interface configObj {
		eq: enumSet|null;
		bq: bitfSet|null;
		/**
		 * This is an emtpy set for RBAC w/o explicit roles.
		 */
		roles: roleSet|null;
		/**
		 * Never an empty set.
		 */
		sq: strctSet;
	}

	export interface ortJsonConfig {
		config: configObj;
	}
}
