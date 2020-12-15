namespace ort {
	interface posObj {
		fname: string;
		column: number;
		line: number;
	}

	interface labelObj {
		value: string;
		pos: posObj;
	}

	interface labelSet {
		[lang: string]: labelObj;
	}

	interface enumItemObj {
		pos: posObj;
		doc: string|null;
		value: string|number|null;
		labels: labelSet|null;
	}

	interface enumItemSet {
		[name: string]: enumItemObj;
	}

	interface enumObj {
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet|null;
		items: enumItemSet;
	}

	interface enumSet {
		[name: string]: enumObj;
	}

	interface bitIndexObj {
		pos: posObj;
		doc: string|null;
		value: string|number|null;
		labels: labelSet|null;
	}

	interface bitIndexSet {
		[name: string]: bitIndexObj;
	}

	interface bitfObj {
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet|null;
		labelsUnset: labelSet|null;
		items: bitIndexSet;
	}

	interface bitfSet {
		[name: string]: bitfObj;
	}

	interface roleObj {
		pos: posObj;
		doc: string|null;
		children: roleSet|null;
	}

	interface roleSet {
		[name: string]: roleObj;
	}

	interface validObj {
		type: 'eq'|'le'|'gt'|'lt'|'ge';
		/*
		 * Depends on field type.
		 */
		limit: string;
	}

	interface fieldObj {
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
		 * The interpretation of this depends upon the type.
		 */
		def: string|null;
		/**
		 * Empty for no valids.
		 */
		valids: validObj[];
	}

	interface fieldSet {
		[name: string]: fieldObj;
	}

	interface insertObj {
		pos: posObj;
		rolemap: string[]|null;
	}

	interface sentObj {
		pos: posObj;
		fname: string;
		op: 'eq'|'ge'|'gt'|'le'|'lt'|'neq'|'like'|'and'|'or'|'streq'|'strneq'|'isnull'|'notnull';
	}

	interface orderObj {
		pos: posObj;
		fname: string;
		op: 'desc'|'asc';
	}

	interface aggrObj {
		pos: posObj;
		fname: string;
		op: 'minrow'|'maxrow';
	}

	interface groupObj {
		pos: posObj;
		fname: string;
	}

	interface dstnctObj {
		pos: posObj;
		fname: string;
	}

	interface searchObj {
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
		params: sentObj[];
		order: orderObj[];
		aggr: aggrObj|null;
		group: groupObj|null;
		dst: dstnctObj|null;
		type: 'search'|'iterate'|'list'|'count';
	}

	interface searchSet {
		[name: string]: searchObj;
	}

	interface searchClassObj {
		named: searchSet;
		anon: searchObj[];
	}

	interface strctObj {
		pos: posObj;
		doc: string|null;
		/**
		 * Never an empty set.
		 */
		fields: fieldSet;
		insert: insertObj|null;
		searches: searchClassObj;
	}

	interface strctSet {
		[name: string]: strctObj;
	}

	interface config {
		enums: enumSet|null;
		bitfs: bitfSet|null;
		/**
		 * This is an emtpy set for RBAC w/o explicit roles.
		 */
		roles: roleSet|null;
		/**
		 * Never an empty set.
		 */
		strcts: strctSet;
	}

	export interface ortConfig {
		config: config;
	}
}
