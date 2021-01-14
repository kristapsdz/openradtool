/* vim: set filetype=javascript: */

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
	 * The default language is "_default".
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
		value: string|number;
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

	export type fieldObjFlags = 'rowid'|'null'|'unique';
	export type fieldObjActions =  'none'|'restrict'|'nullify'|'cascade'|'default';

	/**
	 * Same as "struct field" in ort(3).
	 */
	export interface fieldObj {
		pos: posObj;
		doc: string|null;
		type: 'bit'|'date'|'epoch'|'int'|'real'|'blob'|'text'|'password'|'email'|'struct'|'enum'|'bitfield';
		actup: fieldObjActions;
		actdel: fieldObjActions;
		flags: fieldObjFlags[];
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
		sntq: sentObj[];
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
		type: 'update'|'delete';
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
	 * Similar to "struct rolemap" in ort(3).
	 */
	export interface rolemapObj {
		type: 'all'|'count'|'delete'|'insert'|'iterate'|'list'|'search'|'update'|'noexport';
		/**
		 * If not null (it's only null if type is "all" or
		 * "insert", or "noexport" for all fields), is the named
		 * field/search/update.
		 */
		name: string|null;
		rq: string[];
	}

	/**
	 * Same as "struct strct" in ort(3).
	 */
	export interface strctObj {
		pos: posObj;
		doc: string|null;
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
		nq: uniqueObj[];
		/**
		 * This is informational: all of the operations have
		 * their roles therein.  
		 */
		rq: rolemapObj[];
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
		 * Contains all user-defined roles, i.e., those
		 * descending from (but not including) "all".  This is
		 * an empty object for no user-defined roles (i.e., just
		 * the "roles" statement) and null for no roles.
		 */
		rq: roleSet|null;
		/**
		 * List of all non-hierarchical roles names including
		 * system-defined roles.  Empty if having no roles.
		 */
		arq: string[];
		sq: strctSet;
	}

	export interface ortJsonConfig {
		config: configObj;
	}

	/**
	 * Manage serialising an ortJsonConfig object into an HTML DOM
	 * tree.
	 */
	export class ortJsonConfigFormat {
		private readonly obj: ortJson.configObj;

		constructor(obj: ortJson.ortJsonConfig) {
			this.obj = obj.config;
		}

		private commentToString(doc: string): string
		{
			return ' comment \"' + doc.replace(/"/g, '\\\"') + '\"';
		}

		private roleObjToString(name: string, role: roleObj): string
		{
			let str: string = ' role ' + name;
			if (role.doc !== null)
				this.commentToString(role.doc);
			if (role.children !== null)
				str += this.roleSetToString(role.children);
			return str + ';';
		}

		private roleSetToString(set: roleSet): string 
		{
			let str: string = ' {';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.roleObjToString
					(keys[i], set[keys[i]]);
			return str + '}';
		}

		private fieldObjToString(name: string, field: fieldObj): string
		{
			let str: string = ' field';
			if (typeof field.ref !== 'undefined' && 
			    field.type !== 'struct')
				str += ' ' + field.ref.source.field + 
					':' + field.ref.target.strct + 
					'.' + field.ref.target.field;
			else
				str += ' ' + name;
			str += ' ' + field.type;
			if (typeof field.enm !== 'undefined')
				str += ' ' + field.enm;
			if (typeof field.bitf !== 'undefined')
				str += ' ' + field.bitf;
			if (typeof field.ref !== 'undefined' && 
			    field.type === 'struct')
				str += ' ' + field.ref.source.field;
			if (typeof field.ref !== 'undefined' && 
			    field.type !== 'struct') {
				str += ' actdel ' + field.actdel;
				str += ' actup ' + field.actup;
			}
			if (field.doc !== null)
				str += this.commentToString(field.doc);
			if (field.def !== null &&
			    (field.type === 'email' ||
			     field.type === 'text'))
				str += ' default \"' + 
					field.def.replace(/"/g, '\\\"') +
					'\"';
			else if (field.def !== null)
				str += ' default ' + field.def;

			for (let i: number = 0; i < field.fvq.length; i++)
				str += ' limit ' + field.fvq[i].type + 
					' ' + field.fvq[i].limit;
			for (let i: number = 0; i < field.flags.length; i++) 
				str += ' ' + field.flags[i];
			return str + ' ;';
		}

		private labelSetToString(set: labelSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++) {
				const label: string =
					set[keys[i]].value.replace
						(/"/g, '\\\"');
				str += ' jslabel' + 
					(keys[i] === '_default' ? '' :
					 ('.' + keys[i])) +
					' \"' + label + '\"';
			}
			return str;
		}

		private bitIndexObjToString(name: string, bit: bitIndexObj): string
		{
			let str: string = ' item ' + 
				name + ' ' + bit.value.toString();
			if (bit.doc !== null)
				str += this.commentToString(bit.doc);
			if (bit.labels !== null)
				str += this.labelSetToString(bit.labels);
			return str + ' ;';
		}

		private bitfObjToString(name: string, bitf: bitfObj): string
		{
			let str: string = ' bitfield ' + name + ' {';
			const keys: string[] = Object.keys(bitf.bq);
			for (let i: number = 0; i < keys.length; i++)
				str += this.bitIndexObjToString(keys[i],
					bitf.bq[keys[i]]);
			if (bitf.doc !== null)
				str += this.commentToString(bitf.doc) + ';';
			if (bitf.labelsNull !== null)
				str += ' isnull' + this.labelSetToString
					(bitf.labelsNull) + ';';
			if (bitf.labelsUnset !== null)
				str += ' isunset' + this.labelSetToString
					(bitf.labelsUnset) + ';';
			return str + ' };';
		}

		private bitfSetToString(set: bitfSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.bitfObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		private enumItemObjToString(name: string, eitem: enumItemObj): string
		{
			let str: string = ' item ' + name;
			if (eitem.value !== null)
				str += ' ' + eitem.value.toString();
			if (eitem.doc !== null)
				str += this.commentToString(eitem.doc);
			if (eitem.labels !== null)
				str += this.labelSetToString(eitem.labels);
			return str + ' ;';
		}

		private enumObjToString(name: string, enm: enumObj): string
		{
			let str: string = ' enum ' + name + ' {';
			const keys: string[] = Object.keys(enm.eq);
			for (let i: number = 0; i < keys.length; i++)
				str += this.enumItemObjToString(keys[i],
					enm.eq[keys[i]]);
			if (enm.doc !== null)
				str += this.commentToString(enm.doc) + ';';
			if (enm.labelsNull !== null)
				str += ' isnull' + this.labelSetToString
					(enm.labelsNull) + ';';
			return str + ' };';
		}

		private enumSetToString(set: enumSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.enumObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		private fieldSetToString(set: fieldSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.fieldObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		private updateObjToString(name: string|null, up: updateObj): string
		{
			let str: string = ' ' + up.type;
			if (up.type === 'update' && up.flags.length === 0) {
				for (let i: number = 0; i < up.mrq.length; i++) {
					if (i > 0)
						str += ',';
					str += ' ' + up.mrq[i].field + 
						' ' + up.mrq[i].mod;
				}
				str += ':';
			} else if (up.type === 'update')
				str += ':';

			for (let i: number = 0; i < up.crq.length; i++) {
				if (i > 0)
					str += ',';
				str += ' ' + up.crq[i].field + 
					' ' + up.crq[i].op;
			}
			str += ':';
			if (up.doc !== null)
				str += this.commentToString(up.doc);
			if (name !== null)
				str += ' name ' + name;
			return str + ';';
		}

		private updateSetToString(set: updateSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.updateObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		private searchObjToString(name: string|null, search: searchObj): string
		{
			let str: string = ' ' + search.type;
			for (let i: number = 0; i < search.sntq.length; i++) {
				if (i > 0)
					str += ',';
				str += ' ' + search.sntq[i].fname + 
					' ' + search.sntq[i].op;
			}
			str += ':';
			if (search.doc !== null)
				str += this.commentToString(search.doc);
			if (search.limit !== '0') {
				str += ' limit ' + search.limit;
				if (search.offset !== '0')
					str += ', ' + search.offset;
			}
			if (name !== null)
				str += ' name ' + name;
			if (search.dst !== null)
				str += ' distinct ' + search.dst.fname;
			if (search.group !== null)
				str += ' grouprow ' + search.group.fname;
			if (search.aggr !== null)
				str += ' ' + search.aggr.op + 
					' ' + search.aggr.fname;
			if (search.ordq.length > 0) {
				str += ' order';
				for (let i: number = 0; i < search.ordq.length; i++) {
					if (i > 0)
						str += ',';
					str += ' ' + search.ordq[i].fname + 
						' ' + search.ordq[i].op;
				}
			}
			return str + ';';
		}

		private searchSetToString(set: searchSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.searchObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		private rolemapObjToString(map: rolemapObj): string
		{
			let str: string = ' roles';
			for (let i: number = 0; i < map.rq.length; i++) {
				if (i > 0)
					str += ',';
				str += ' ' + map.rq[i];
			}
			str += ' { ' + map.type;
			if (map.name !== null)
				str += ' ' + map.name;
			return str + '; };';
		}

		private strctObjToString(name: string, strct: strctObj): string
		{
			let str: string = ' struct ' + name + ' {';
			str += this.fieldSetToString(strct.fq);

			for (let i: number = 0; i < strct.sq.anon.length; i++) 
		       		str += this.searchObjToString
					(null, strct.sq.anon[i]);	
			str += this.searchSetToString(strct.sq.named);

			for (let i: number = 0; i < strct.uq.anon.length; i++) 
		       		str += this.updateObjToString
					(null, strct.uq.anon[i]);	
			str += this.updateSetToString(strct.uq.named);

			for (let i: number = 0; i < strct.dq.anon.length; i++) 
		       		str += this.updateObjToString
					(null, strct.dq.anon[i]);	
			str += this.updateSetToString(strct.dq.named);
			if (strct.insert !== null)
				str += ' insert;';
			if (strct.doc !== null) 
				str += this.commentToString(strct.doc) + ';';
			for (let i: number = 0; i < strct.nq.length; i++) {
				str += ' unique ';
				for (let j: number = 0; 
				     j < strct.nq[i].nq.length; j++) {
					if (j > 0)
						str += ',';
					str += strct.nq[i].nq[j];
				}
				str += ';';
			}
			for (let i: number = 0; i < strct.rq.length; i++)
				str += this.rolemapObjToString(strct.rq[i]);
			return str + ' };';
		}

		private strctSetToString(set: strctSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.strctObjToString
					(keys[i], set[keys[i]]);
			return str;
		}

		/**
		 * Convert the configuration to an ort(5) document.
		 * It does not validation of the document.
		 */
		toString(): string
		{
			let str: string = '';

			if (this.obj.rq !== null)
				str += 'roles ' + this.roleSetToString
					(this.obj.rq) + ';';
			if (this.obj.bq !== null) 
				str += this.bitfSetToString(this.obj.bq);
			if (this.obj.eq !== null) 
				str += this.enumSetToString(this.obj.eq);

			return str + this.strctSetToString(this.obj.sq);
		}

		private find(root: string|HTMLElement): 
			HTMLElement|null
		{
			if (typeof root !== 'string')
				return root;
			const e: HTMLElement|null =
				document.getElementById(root);
			return e;
		}

		private list(root: string|HTMLElement, name: string): 
			HTMLElement[] 
		{
			const ret: HTMLElement[] = [];
			const e: HTMLElement|null = this.find(root);
			if (e === null)
				return ret;
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			for (let i: number = 0; i < list.length; i++)
				ret.push(<HTMLElement>list[i]);
			if (e.classList.contains(name))
				ret.push(e);
			return ret;
		}

		private attrcl(root: string|HTMLElement, 
			name: string, attr: string, text: string): void 
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++)
				list[i].setAttribute(attr, text);
		}

		private replcl(root: string|HTMLElement, 
			name: string, text: string): void 
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++) {
				this.clr(<HTMLElement>list[i]);
				list[i].appendChild
					(document.createTextNode
					 (text.toString()));
			}
		}

		private clr(root: string|HTMLElement): 
			HTMLElement|null
		{
			const e: HTMLElement|null = this.find(root);
			if (e === null)
				return null;
			while (e.firstChild)
				e.removeChild(e.firstChild);
			return e;
		}

		private show(root: string|HTMLElement): 
			HTMLElement|null
		{
			const e: HTMLElement|null = this.find(root);
			if (e === null)
				return null;
			if (e.classList.contains('hide'))
				e.classList.remove('hide');
			return e;
		}

		private hide(root: string|HTMLElement): 
			HTMLElement|null
		{
			const e: HTMLElement|null = this.find(root);
			if (e === null)
				return null;
			if (!e.classList.contains('hide'))
				e.classList.add('hide');
			return e;
		}

		private hidecl(root: string|HTMLElement, 
			name: string): void 
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++)
				this.hide(<HTMLElement>list[i]);
		}

		private showcl(root: string|HTMLElement, 
			name: string): void 
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++)
				this.show(<HTMLElement>list[i]);
		}

		/**
		 * Invokes fill() for each element with the given class
		 * under the root.
		 */
		fillByClass(root: HTMLElement|string, name: string): void
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++)
				this.fill(<HTMLElement>list[i]);
		}

		/**
		 * Fills the configuration **including** the given element,
		 * starting with structures, then drilling down.
		 * Elements are manipulated by whether the contain the
		 * following classes.  Note that to "hide" an element
		 * means to append the *hide* class to its class list,
		 * while to "show" an element is to remove this.  The
		 * caller will need to make sure that these classes
		 * contain the appropriate `display: none` or similar
		 * style definitions.
		 *
		 * Per configuration:
		 *
		 * - fillStrcts()
		 * - fillRoles()
		 */
		fill(e: string|HTMLElement|null): void
		{
			if (e === null)
				return;
			const pn: HTMLElement|null = this.find(e);
			if (pn === null)
				return;
			this.fillRoles(pn);
			this.fillStrcts(pn);
		}

		private fillComment(e: HTMLElement, 
			type: string, doc: string|null): void
		{
			const str: string = 'config-' + type + '-doc';
			if (doc === null || doc.length === 0) {
				this.showcl(e, str + '-none');
				this.hidecl(e, str + '-has');
				this.replcl(e, str, '');
				this.attrcl(e, str + '-value', 'value', '');
			} else {
				this.hidecl(e, str + '-none');
				this.showcl(e, str + '-has');
				this.replcl(e, str, doc);
				this.attrcl(e, str + '-value', 'value', doc);
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * Per all roles specified in a configuration:
		 *
		 * - *config-roles-{has,none}*: shown or hidden
		 *   depending on whether there are roles at all (even
		 *   if not having user-defined roles)
		 * - *config-roles*: the first child of these is cloned
		 *   and filled in with data for each role unless there
		 *   are no roles, in which case the elements are hidden
		 *
		 * For each role:
		 *
		 * - *config-role*: filled in with the role name
		 * - *config-role-value*: value set to the role name
		 */
		fillRoles(e: HTMLElement): void
		{
			if (this.obj.arq.length === 0) {
				this.hidecl(e, 'config-roles-has');
				this.showcl(e, 'config-roles-none');
				return;
			}
			this.showcl(e, 'config-roles-has');
			this.hidecl(e, 'config-roles-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-roles');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.show(list[i]);
				this.clr(list[i]);
				for (let j: number = 0; 
				     j < this.obj.arq.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.replcl(cln, 'config-role', 
						this.obj.arq[j]);
					this.attrcl(cln, 'config-role-value',
						'value', this.obj.arq[j]);
				}
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * Per set of strcts:
		 *
		 * - *config-strcts-{has,none}*: shown or hidden
		 *   depending on whether there are strcts
		 * - *config-strcts*: the first child of these is cloned
		 *   and filled in with data for each strct (see
		 *   fillStrct()) unless there are no strcts, in which
		 *   case the elements are hidden
		 */
		fillStrcts(e: HTMLElement)
		{
			const keys: string[] = 
				Object.keys(this.obj.sq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-strcts-has');
				this.hidecl(e, 'config-strcts');
				this.showcl(e, 'config-strcts-none');
				return;
			} 
			this.showcl(e, 'config-strcts-has');
			this.hidecl(e, 'config-strcts-none');
			keys.sort();
			const list: HTMLElement[] = 
				this.list(e, 'config-strcts');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.show(list[i]);
				this.clr(list[i]);
				for (let j: number = 0; j < keys.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.fillStrct(cln, keys[j], 
						this.obj.sq[keys[j]]);
				}
				this.show(list[i]);
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * Per strct:
		 *
		 * - *config-strct-name*: filled in with name
		 * - *config-strct-name-value*: value set with name
		 * - *config-strct-doc-{none,has}*: shown or hidden
		 *   depending on whether there's a non-empty
		 *   documentation field
		 * - *config-strct-doc*: filled in with non-empty
		 *   documentation, if found
		 * - *config-strct-doc-value*: value set with non-empty
		 *   documentation, if found
		 * - *config-fields*: the first child of this is cloned
		 *   and filled in with data for each field (see "Per
		 *   field")
		 * - *config-queries-{has,none}*: shown or hidden
		 *   depending upon whether there are any queries
		 * - *config-queries*: the first child of this is cloned
		 *   and filled in with data for each query (see "Per
		 *   query") unless there are no queries, in which case
		 *   the element is hidden
		 * - *config-insert-{has,none}*: shown or hidden
		 *   depending on whether an insert is defined (see "Per
		 *   insert" if there is an insert defined)
		 * - *config-updates-{has,none}*: shown or hidden
		 *   depending upon whether there are any updates
		 * - *config-updates*: the first child of this is cloned
		 *   and filled in with data for each update (see "Per
		 *   update") unless there are no queries, in which case
		 *   the element is hidden
		 * - *config-deletes-{has,none}*: shown or hidden
		 *   depending upon whether there are any deletes
		 * - *config-deletes*: the first child of this is cloned
		 *   and filled in with data for each update (see "Per
		 *   update") unless there are no queries, in which case
		 *   the element is hidden
		 * - *config-uniques-{has,none}*: shown or hidden
		 *   depending upon whether there are any unique tuples
		 * - *config-uniques*: the first child of this is cloned
		 *   and filled in with data for each unique tuple (see
		 *   "Per unique") unless there are no tuples, in
		 *   which case the element is hidden
		 *
		 * Per insert:
		 * - *config-insert-rolemap-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty rolemap
		 * - *config-insert-rolemap-join*: filled in with the
		 *   comma-separated role names if rolemaps are defined
		 * - *config-insert-rolemap*: first child of this is
		 *   cloned and filled in with the name if matching
		 *   *config-insert-rolemap-role* and the value set if
		 *   matching *config-insert-rolemap-role-value* unless
		 *   there are no roles, in which case the element is
		 *   hidden
		 *
		 * Per unique:
		 * - *config-unique*: the comma-separated fields making
		 *   up a unique tuple
		 *
		 * Per update (or delete):
		 * - *config-update-rolemap-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty rolemap
		 * - *config-update-rolemap*: filled in with the
		 *   comma-separated role names if rolemaps are defined
		 * - *config-update-doc-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty
		 *   documentation field
		 * - *config-update-doc*: filled in with non-empty
		 *   documentation if doc is defined*
		 * - *config-update-name-{has,none}*: shown or hidden
		 *   depending on whether the update is anonymous
		 * - *config-update-name*: filled in with the update
		 *   name if a name is defined
		 * - *config-update-fields-{has,none}*: shown or hidden
		 *   depending on whether the update has non-zero mrq
		 *   or crqs
		 * - *config-update-crq-{has,none}*: shown or hidden
		 *   depending on whether the update has non-zero crq
		 * - *config-update-mrq-{has,none}*: shown or hidden
		 *   depending on whether the update has non-zero mrq
		 * - *config-update-mrq*: the first child of this is
		 *   cloned and filled in with data for each reference
		 *   (see "Per update reference") unless there are no
		 *   references, in which case the element is hidden
		 * - *config-update-crq*: the first child of this is
		 *   cloned and filled in with data for each reference
		 *   (see "Per update reference") unless there are no
		 *   references, in which case the element is hidden
		 *
		 * Per update reference:
		 * - *config-uref-field*: the field name in the current
		 *   structure
		 * - *config-uref-op*: the update constraint operator
		 * - *config-uref-mod*: the update modifier
		 *
		 * Per query:
		 * - *config-query-name-{has,none}*: shown or hidden
		 *   depending on whether the query is anonymous
		 * - *config-query-name*: filled in with the query name
		 *   if a name is defined
		 * - *config-query-doc-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty
		 *   documentation field
		 * - *config-query-doc*: filled in with non-empty
		 *   documentation if doc is defined*
		 * - *config-query-type*: filled in with the query type
		 * - *config-query-sntq-{has,none}*: whether there's a
		 *   list of search columns
		 * - *config-query-sntq*: the first child of this is
		 *   cloned and filled in with search columns (see "Per
		 *   query search field") unless the list is empty, in which
		 *   case the element is hidden
		 * - *config-query-ordq-{has,none}*: whether there's a
		 *   list of order columns
		 * - *config-query-ordq*: the first child of this is
		 *   cloned and filled in with order columns (see "Per
		 *   query order field") unless the list is empty, in which
		 *   case the element is hidden
		 * - *config-query-aggr-{has,none}*: whether both an
		 *   aggregate and group are defined
		 * - *config-query-aggr-op*: aggregate operation, if
		 *   defined
		 * - *config-query-aggr-fname*: aggregate query path,
		 *   if defined
		 * - *config-query-group-fname*: group query path,
		 *   if defined
		 * - *config-query-dst-{has,none}*: whether a distinct
		 *   reduction is defined
		 * - *config-query-dst-fname*: distinct query path
		 *
		 * Per query search field:
		 * - *config-sent-fname*: filled in with the fname
		 * - *config-sent-op*: filled in with the operation
		 *
		 * Per query order field:
		 * - *config-ord-fname*: filled in with the fname
		 * - *config-ord-op*: filled in with the operation
		 *
		 * Per field:
		 * - *config-field-name*: filled in with name
		 * - *config-field-doc-{none,has}*: shown or hidden
		 *   depending on whether there's a non-empty
		 *   documentation field
		 * - *config-field-doc*: filled in with non-empty
		 *   documentation if doc is defined
		 * - *config-field-type-TYPE*: shown or hidden depending
		 *   upon the field type
		 * - *config-field-type*: filled in with the type name
		 * - *config-field-store-type*: shown or hidden
		 *   depending upon whether a non-struct type
		 * - *config-field-ref-{has,none}*: shown or hidden
		 *   depending on whether there's a ref
		 * - *config-field-fkey-{has,none}*: shown or hidden
		 *   depending on whether there's a non-struct ref
		 * - *config-field-ref-target-{strct,field}*: filled
		 *   with the target names if ref is defined
		 * - *config-field-ref-source-{strct,field}*: filled
		 *   with the source names if ref is defined
		 * - *config-field-bitf-{has,none}*: shown or hidden
		 *   depending on whether there's a bitf
		 * - *config-field-bitf*: filled in with the bitf name
		 *   if bitf is defined
		 * - *config-field-enm-{has,none}*: shown or hidden
		 *   depending on whether there's an enm
		 * - *config-field-enm*: filled in with the enm name
		 *   if enm is defined
		 * - *config-field-actdel*: filled in with actdel
		 * - *config-field-actup*: filled in with actup
		 * - *config-field-actup-TYPE*: shown or hidden
		 *   depending upon the actup type
		 * - *config-field-actdel-TYPE*: shown or hidden
		 *   depending upon the actdel type
		 * - *config-field-rolemap-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty rolemap
		 * - *config-field-rolemap*: filled in with the
		 *   comma-separated role names if rolemaps are defined
		 * - *config-field-def-{has,none}*: shown or hidden
		 *   depending on whether there's a default value
		 * - *config-field-def*: filled in with the default
		 *   value if a default is defined
		 * - *config-field-limits-{has,none}*: shown or hidden
		 *   depending on whether limits are defined
		 * - *config-field-limit-TYPE*: shown or hidden
		 *   depending upon the actual limit type, which may be
		 *   one of *number*, *real*, *string*, enum* if limits
		 *   are defined
		 * - *config-field-limit*: filled in with
		 *   comma-separated limits type-value pairs if limits
		 *   are defined
		 */
		fillStrct(e: HTMLElement, name: string, 
			  strct: ortJson.strctObj): void
		{
			let list: HTMLElement[];

			/* fq (fields) */

			list = this.list(e, 'config-fields');
			for (let i: number = 0; i < list.length; i++)
				this.fillFields(<HTMLElement>list[i], strct);

			/* dq (deletes) */
			
			if (strct.dq.anon.length +
		  	    Object.keys(strct.dq.named).length === 0) {
				this.hidecl(e, 'config-deletes-has');
				this.showcl(e, 'config-deletes-none');
			} else {
				this.showcl(e, 'config-deletes-has');
				this.hidecl(e, 'config-deletes-none');
				list = this.list(e, 'config-deletes');
				for (let i: number = 0; i < list.length; i++)
					this.fillUpdates
						(<HTMLElement>list[i], 
						 strct.dq);
			}

			/* nq */
			
			if (strct.nq.length) {
				this.showcl(e, 'config-uniques-has');
				this.hidecl(e, 'config-uniques-none');
				list = this.list(e, 'config-uniques');
				for (let i: number = 0; i < list.length; i++)
					this.fillUniques
						(<HTMLElement>list[i], 
						 strct.nq);
			} else {
				this.hidecl(e, 'config-uniques-has');
				this.showcl(e, 'config-uniques-none');
			}

			/* uq (updates) */
			
			if (strct.uq.anon.length +
		  	    Object.keys(strct.uq.named).length === 0) {
				this.hidecl(e, 'config-updates-has');
				this.showcl(e, 'config-updates-none');
			} else {
				this.showcl(e, 'config-updates-has');
				this.hidecl(e, 'config-updates-none');
				list = this.list(e, 'config-updates');
				for (let i = 0; i < list.length; i++)
					this.fillUpdates
						(<HTMLElement>list[i], 
						 strct.uq);
			}

			/* sq (queries) */
			
			if (strct.sq.anon.length +
		  	    Object.keys(strct.sq.named).length === 0) {
				this.hidecl(e, 'config-queries-has');
				this.showcl(e, 'config-queries-none');
			} else {
				this.showcl(e, 'config-queries-has');
				this.hidecl(e, 'config-queries-none');
				list = this.list(e, 'config-queries');
				for (let i = 0; i < list.length; i++)
					this.fillQueries
						(<HTMLElement>list[i], 
						 strct);
			}

			/* insert */

			if (strct.insert !== null) {
				this.showcl(e, 'config-insert-has');
				this.hidecl(e, 'config-insert-none');
				this.fillRolemap(e, 'insert', 
					strct.insert.rolemap);
			} else {
				this.hidecl(e, 'config-insert-has');
				this.showcl(e, 'config-insert-none');
			}

			/* doc */

			this.fillComment(e, 'strct', strct.doc);

			/* name */

			this.replcl(e, 'config-strct-name', name);
			this.attrcl(e, 'config-strct-name-value', 'value', name);
		}

		private fillRolemap(e: HTMLElement, 
			name: string, map: string[]|null): void
		{
			const cls: string = 
				'config-' + name + '-rolemap';

			if (map === null || map.length === 0) {
				this.showcl(e, cls + '-none');
				this.hidecl(e, cls + '-has');
				this.hidecl(e, cls);
				return;
			}

			this.replcl(e, cls + '-join', map.join(', '));
			this.hidecl(e, cls + '-none');
			this.showcl(e, cls + '-has');

			const list: HTMLElement[] = this.list(e, cls);

			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.show(list[i]);
				this.clr(list[i]);
				for (let j: number = 0; j < map.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.replcl(cln, 
						cls + '-role', map[j]);
					this.attrcl(cln, 
						cls + '-role-value', 
						'value', map[j]);
				}
			}
		}

		private fillUniques(e: HTMLElement, 
			nq: ortJson.uniqueObj[]): void
		{
			if (e.children.length === 0)
				return;
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			for (let i = 0; i < nq.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.replcl(cln, 'config-unique', 
					  nq[i].nq.join(', '));
			}
		}

		private fillUpdates(e: HTMLElement, 
			cls: ortJson.updateClassObj): void
		{
			if (e.children.length === 0)
				return;
			const keys: string[] = Object.keys(cls.named);
			if (keys.length + cls.anon.length === 0) {
				this.hide(e);
				return;
			}
			this.show(e);
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			keys.sort();
			for (let i = 0; i < keys.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillUpdate(cln, keys[i], 
					cls.named[keys[i]]);
			}
			for (let i = 0; i < cls.anon.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillUpdate(cln, null, cls.anon[i]);
			}
		}

		private fillUpdate(e: HTMLElement,
			name: string|null, up: ortJson.updateObj): void
		{
			/* doc */

			this.fillComment(e, 'update', up.doc);

			/* name */

			if (name === null) {
				this.hidecl(e, 'config-update-name-has');
				this.showcl(e, 'config-update-name-none');
			} else {
				this.hidecl(e, 'config-update-name-none');
				this.showcl(e, 'config-update-name-has');
				this.replcl(e, 'config-update-name', name);
			}

			if (up.crq.length + up.mrq.length === 0) {
				this.showcl(e, 'config-update-fields-none');
				this.hidecl(e, 'config-update-fields-has');
			} else {
				this.showcl(e, 'config-update-fields-none');
				this.hidecl(e, 'config-update-fields-has');
			}

			/* crq */

			if (up.crq.length === 0) {
				this.showcl(e, 'config-update-crq-none');
				this.hidecl(e, 'config-update-crq-has');
				this.hidecl(e, 'config-update-crq');
			} else {
				this.hidecl(e, 'config-update-crq-none');
				this.showcl(e, 'config-update-crq-has');
				this.showcl(e, 'config-update-crq');
				const list: HTMLElement[] =
					this.list(e, 'config-update-crq');
				for (let i: number = 0; i < list.length; i++)
					this.fillUrefs(<HTMLElement>list[i], up.crq);
			}

			/* mrq */

			if (up.mrq.length === 0) {
				this.showcl(e, 'config-update-mrq-none');
				this.hidecl(e, 'config-update-mrq-has');
				this.hidecl(e, 'config-update-mrq');
			} else {
				this.hidecl(e, 'config-update-mrq-none');
				this.showcl(e, 'config-update-mrq-has');
				this.showcl(e, 'config-update-mrq');
				const list: HTMLElement[] =
					this.list(e, 'config-update-mrq');
				for (let i: number = 0; i < list.length; i++)
					this.fillUrefs(<HTMLElement>list[i], up.mrq);
			}

			/* rolemap */

			if (up.rolemap === null || up.rolemap.length === 0) {
				this.showcl(e, 'config-update-rolemap-none');
				this.hidecl(e, 'config-update-rolemap-has');
			} else {
				this.hidecl(e, 'config-update-rolemap-none');
				this.showcl(e, 'config-update-rolemap-has');
				this.replcl(e, 'config-update-rolemap',
					up.rolemap.join(', '));
			}

		}

		private fillUrefs(e: HTMLElement, 
			urefs: ortJson.urefObj[]): void
		{
			if (e.children.length === 0)
				return;
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			for (let i = 0; i < urefs.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.replcl(cln, 'config-uref-field', 
					urefs[i].field);
				this.replcl(cln, 'config-uref-op', 
					urefs[i].op);
				this.replcl(cln, 'config-uref-mod', 
					urefs[i].mod);
			}
		}

		private fillQueries(e: HTMLElement, 
			strct: ortJson.strctObj): void
		{
			if (e.children.length === 0)
				return;
			const keys: string[] = Object.keys(strct.sq.named);
			if (keys.length + strct.sq.anon.length === 0) {
				this.hide(e);
				return;
			}
			this.show(e);
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			keys.sort();
			for (let i = 0; i < keys.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillQuery(cln, keys[i], 
					strct.sq.named[keys[i]]);
			}
			for (let i = 0; i < strct.sq.anon.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillQuery(cln, null, strct.sq.anon[i]);
			}
		}

		private fillQuery(e: HTMLElement,
			name: string|null, query: ortJson.searchObj): void
		{
			/* doc */

			this.fillComment(e, 'query', query.doc);

			/* name */

			if (name === null) {
				this.hidecl(e, 'config-query-name-has');
				this.showcl(e, 'config-query-name-none');
			} else {
				this.hidecl(e, 'config-query-name-none');
				this.showcl(e, 'config-query-name-has');
				this.replcl(e, 'config-query-name', name);
			}

			/* type */

			this.replcl(e, 'config-query-type', query.type);

			/* sntq */

			if (query.sntq.length === 0) {
				this.showcl(e, 'config-query-sntq-none');
				this.hidecl(e, 'config-query-sntq-has');
				this.hidecl(e, 'config-query-sntq');
			} else {
				this.hidecl(e, 'config-query-sntq-none');
				this.showcl(e, 'config-query-sntq-has');
				this.showcl(e, 'config-query-sntq');
				const list: HTMLElement[] =
					this.list(e, 'config-query-sntq');
				for (let i: number = 0; i < list.length; i++)
					this.fillSents(<HTMLElement>list[i], query);
			}
			
			/* ordq */

			if (query.ordq.length === 0) {
				this.showcl(e, 'config-query-ordq-none');
				this.hidecl(e, 'config-query-ordq-has');
				this.hidecl(e, 'config-query-ordq');
			} else {
				this.hidecl(e, 'config-query-ordq-none');
				this.showcl(e, 'config-query-ordq-has');
				this.showcl(e, 'config-query-ordq');
				const list: HTMLElement[] =
					this.list(e, 'config-query-ordq');
				for (let i: number = 0; i < list.length; i++)
					this.fillOrds(<HTMLElement>list[i], query);
			}

			/* dst */

			if (query.dst !== null) {
				this.showcl(e, 'config-query-dst-has');
				this.hidecl(e, 'config-query-dst-none');
				this.replcl(e, 'config-query-dst-fname', 
					query.dst.fname);
			} else {
				this.hidecl(e, 'config-query-dst-has');
				this.showcl(e, 'config-query-dst-none');
			}

			/* aggr/group */

			if (query.aggr !== null && query.group != null) {
				this.showcl(e, 'config-query-aggr-has');
				this.hidecl(e, 'config-query-aggr-none');
				this.replcl(e, 'config-query-group-fname', 
					query.group.fname);
				this.replcl(e, 'config-query-aggr-fname', 
					query.aggr.fname);
				this.replcl(e, 'config-query-aggr-op', 
					query.aggr.op);
			} else {
				this.hidecl(e, 'config-query-aggr-has');
				this.showcl(e, 'config-query-aggr-none');
			}

			/* rolemap */

			if (query.rolemap === null || query.rolemap.length === 0) {
				this.showcl(e, 'config-query-rolemap-none');
				this.hidecl(e, 'config-query-rolemap-has');
			} else {
				this.hidecl(e, 'config-query-rolemap-none');
				this.showcl(e, 'config-query-rolemap-has');
				this.replcl(e, 'config-query-rolemap',
					query.rolemap.join(', '));
			}

		}

		private fillOrds(e: HTMLElement, 
			query: ortJson.searchObj): void
		{
			if (e.children.length === 0)
				return;
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			for (let i = 0; i < query.ordq.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.replcl(cln, 
					'config-ord-fname', 
					query.ordq[i].fname);
				this.replcl(cln, 
					'config-ord-op', 
					query.ordq[i].op);
			}
		}

		private fillSents(e: HTMLElement, 
			query: ortJson.searchObj): void
		{
			if (e.children.length === 0)
				return;
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			for (let i = 0; i < query.sntq.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.replcl(cln, 
					'config-sent-fname', 
					query.sntq[i].fname);
				this.replcl(cln, 
					'config-sent-op', 
					query.sntq[i].op);
			}
		}

		private fillFields(e: HTMLElement, 
			strct: ortJson.strctObj): void
		{
			if (e.children.length === 0)
				return;
			const tmpl: HTMLElement =
				<HTMLElement>e.children[0];
			this.clr(e);
			const keys: string[] = Object.keys(strct.fq);
			keys.sort();
			for (let i = 0; i < keys.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillField(cln, keys[i], 
					strct.fq[keys[i]]);
			}
		}

		private fillFieldAction(e: HTMLElement, 
			act: string, type: string): void
		{
			this.hidecl(e, 'config-field-' + type + '-none');
			this.hidecl(e, 'config-field-' + type + '-restrict');
			this.hidecl(e, 'config-field-' + type + '-nullify');
			this.hidecl(e, 'config-field-' + type + '-default');
			this.hidecl(e, 'config-field-' + type + '-cascade');
			this.showcl(e, 'config-field-' + type + '-' + act);
			this.replcl(e, 'config-field-' + type, act);
		}

		private fillField(e: HTMLElement,
			name: string, field: ortJson.fieldObj): void
		{
			/* doc */

			this.fillComment(e, 'field', field.doc);

			/* name */

			this.replcl(e, 'config-field-name', name);

			/* type */

			this.hidecl(e, 'config-field-type-bit');
			this.hidecl(e, 'config-field-type-date');
			this.hidecl(e, 'config-field-type-epoch');
			this.hidecl(e, 'config-field-type-int');
			this.hidecl(e, 'config-field-type-real');
			this.hidecl(e, 'config-field-type-blob');
			this.hidecl(e, 'config-field-type-text');
			this.hidecl(e, 'config-field-type-password');
			this.hidecl(e, 'config-field-type-email');
			this.hidecl(e, 'config-field-type-struct');
			this.hidecl(e, 'config-field-type-enum');
			this.hidecl(e, 'config-field-type-bitfield');
			this.showcl(e, 'config-field-type-' + field.type);
			this.replcl(e, 'config-field-type', field.type);
			if (field.type !== 'struct')
				this.showcl(e, 'config-field-store-type');
			else
				this.hidecl(e, 'config-field-store-type');

			/* actdel, actup */

			this.fillFieldAction(e, field.actup, 'actup');
			this.fillFieldAction(e, field.actdel, 'actdel');

			/* reference (foreign key or struct) */

			if (typeof field.ref === 'undefined') {
				this.showcl(e, 'config-field-ref-none');
				this.hidecl(e, 'config-field-ref-has');
				this.hidecl(e, 'config-field-fkey-has');
				this.showcl(e, 'config-field-fkey-none');
			} else {
				this.hidecl(e, 'config-field-ref-none');
				this.showcl(e, 'config-field-ref-has');
				if (field.type !== 'struct') {
					this.showcl(e, 'config-field-fkey-has');
					this.hidecl(e, 'config-field-fkey-none');
				} else {
					this.hidecl(e, 'config-field-fkey-has');
					this.showcl(e, 'config-field-fkey-none');
				}
				this.replcl(e, 'config-field-ref-target-strct', 
					field.ref.target.strct);
				this.replcl(e, 'config-field-ref-target-field', 
					field.ref.target.field);
				this.replcl(e, 'config-field-ref-source-strct', 
					field.ref.source.strct);
				this.replcl(e, 'config-field-ref-source-field', 
					field.ref.source.field);
			}

			/* bitf */

			if (typeof field.bitf === 'undefined') {
				this.showcl(e, 'config-field-bitf-none');
				this.hidecl(e, 'config-field-bitf-has');
			} else {
				this.hidecl(e, 'config-field-bitf-none');
				this.showcl(e, 'config-field-bitf-has');
				this.replcl(e, 'config-field-bitf', field.bitf);
			}

			/* enm */

			if (typeof field.enm === 'undefined') {
				this.showcl(e, 'config-field-enm-none');
				this.hidecl(e, 'config-field-enm-has');
			} else {
				this.hidecl(e, 'config-field-enm-none');
				this.showcl(e, 'config-field-enm-has');
				this.replcl(e, 'config-field-enm', field.enm);
			}

			/* rolemap */

			if (field.rolemap === null || field.rolemap.length === 0) {
				this.showcl(e, 'config-field-rolemap-none');
				this.hidecl(e, 'config-field-rolemap-has');
			} else {
				this.hidecl(e, 'config-field-rolemap-none');
				this.showcl(e, 'config-field-rolemap-has');
				this.replcl(e, 'config-field-rolemap',
					field.rolemap.join(', '));
			}

			/* def */

			if (field.def === null) {
				this.showcl(e, 'config-field-def-none');
				this.hidecl(e, 'config-field-def-has');
			} else { 
				this.hidecl(e, 'config-field-def-none');
				this.showcl(e, 'config-field-def-has');
				this.replcl(e, 'config-field-def', field.def);
			}
			
			/* fvq */

			if (field.fvq.length === 0) {
				this.hidecl(e, 'config-field-limits-has');
				this.showcl(e, 'config-field-limits-none');
			} else {
				this.showcl(e, 'config-field-limits-has');
				this.hidecl(e, 'config-field-limits-none');
				this.hidecl(e, 'config-field-limit-number');
				this.hidecl(e, 'config-field-limit-real');
				this.hidecl(e, 'config-field-limit-string');
				this.hidecl(e, 'config-field-limit-enum');
				if (field.type === 'bit' ||
				    field.type === 'bitfield' ||
				    field.type === 'date' ||
				    field.type === 'epoch' ||
				    field.type === 'int')
					this.showcl(e, 
						'config-field-limit-number');
				else if (field.type === 'real')
					this.showcl(e, 
						'config-field-limit-real');
				else if (field.type === 'email' ||
				    field.type === 'text')
					this.showcl(e, 
						'config-field-limit-string');
				else if (field.type === 'enum')
					this.showcl(e, 
						'config-field-limit-enum');
				let lim: string = '';
				for (let i: number = 0; i < field.fvq.length; i++) {
					lim += field.fvq[i].type + 
						' ' + field.fvq[i].limit;
					if (i < field.fvq.length - 1)
						lim += ', ';
				}
				this.replcl(e, 'config-field-limit', lim);
			}
		}
	}
}
