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

	interface strctObj {
		pos: posObj;
		doc: string|null;
		/**
		 * Never an empty set.
		 */
		fields: fieldSet;
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

	interface ortConfig {
		config: config;
	}
}
