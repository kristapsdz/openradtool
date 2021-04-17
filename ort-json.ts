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
		lang: string;
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
		parent: string;
		name: string;
		pos: posObj;
		doc: string|null;
		value: string|number|null;
		labels: labelSet;
	}

	export interface enumItemSet {
		[name: string]: enumItemObj;
	}

	/**
	 * Same as "struct enm" in ort(3).
	 */
	export interface enumObj {
		name: string;
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet;
		eq: enumItemSet;
	}

	export interface enumSet {
		[name: string]: enumObj;
	}

	/**
	 * Same as "struct bitidx" in ort(3).
	 */
	export interface bitIndexObj {
		parent: string;
		name: string;
		pos: posObj;
		doc: string|null;
		value: string|number;
		labels: labelSet;
	}

	export interface bitIndexSet {
		[name: string]: bitIndexObj;
	}

	/**
	 * Same as "struct bitf" in ort(3).
	 */
	export interface bitfObj {
		name: string;
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet;
		labelsUnset: labelSet;
		bq: bitIndexSet;
	}

	export interface bitfSet {
		[name: string]: bitfObj;
	}

	/**
	 * Same as "struct role" in ort(3).
	 */
	export interface roleObj {
		name: string;
		pos: posObj;
		doc: string|null;
		parent: string|null;
		subrq: string[];
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

	export type fieldObjFlags = 'rowid'|'null'|'unique'|'noexport';
	export type fieldObjActions =  'none'|'restrict'|'nullify'|'cascade'|'default';

	/**
	 * Same as "struct field" in ort(3).
	 */
	export interface fieldObj {
		parent: string;
		name: string;
		pos: posObj;
		doc: string|null;
		type: 'bit'|'date'|'epoch'|'int'|'real'|'blob'|'text'|'password'|'email'|'struct'|'enum'|'bitfield';
		actup: fieldObjActions;
		actdel: fieldObjActions;
		flags: fieldObjFlags[];
		rolemap: string[];
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
		rolemap: string[];
	}

	export type sentObjOp = 'eq'|'ge'|'gt'|'le'|'lt'|'neq'|'like'|'and'|
		'or'|'streq'|'strneq'|'isnull'|'notnull';
	export type urefObjOp = 'eq'|'ge'|'gt'|'le'|'lt'|'neq'|'like'|'and'|
		'or'|'streq'|'strneq'|'isnull'|'notnull';

	/**
	 * Same as "struct sent" in ort(3).
	 */
	export interface sentObj {
		pos: posObj;
		fname: string;
		op: sentObjOp;
	}

	export type orderObjOp = 'desc'|'asc';

	/**
	 * Same as "struct order" in ort(3).
	 */
	export interface orderObj {
		pos: posObj;
		fname: string;
		op: orderObjOp;
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
		parent: string;
		name: string|null;
		pos: posObj;
		doc: string|null;
		rolemap: string[];
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

	export type urefObjMod = 'concat'|'dec'|'inc'|'set'|'strset';

	export interface urefObj {
		pos: posObj;
		field: string;
		op: urefObjOp;
		mod: urefObjMod;
	}

	/**
	 * Same as "struct update" in ort(3).
	 */
	export interface updateObj {
		name: string|null;
		parent: string;
		pos: posObj;
		doc: string|null;
		type: 'update'|'delete';
		rolemap: string[];
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

	export type rolemapObjType = 'all'|'count'|'delete'|'insert'|
		'iterate'|'list'|'search'|'update'|'noexport';

	/**
	 * Similar to "struct rolemap" in ort(3).
	 */
	export interface rolemapObj {
		type: rolemapObjType;
		/**
		 * If not null (it's only null if type is "all" or
		 * "insert", or "noexport" for all fields), is the named
		 * field/search/update.
		 */
		name: string|null;
		/**
		 * If empty, no roles and should be ignored.
		 */
		rq: string[];
	}

	/**
	 * Same as "struct strct" in ort(3).
	 */
	export interface strctObj {
		name: string;
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
		eq: enumSet;
		bq: bitfSet;
		/**
		 * Unlike in struct config, the "rq" here contains all
		 * roles at the top-level.  This means one can look for
		 * roles by name by checking whether they're defined
		 * properties of the object.
		 */
		rq: roleSet|null;
		sq: strctSet;
	}

	export interface ortJsonConfig {
		config: configObj;
	}

	/**
	 * Prefixes to put before the constellation of "name-id" and "name-link"
	 * classes when filling in configurations.
	 * These may be blank (empty strings), but if non-empty, should be
	 * suffixed by a forward slash ('/') as names will be directly appended
	 * to the prefixes.
	 */
	export interface ortJsonConfigPrefixes {
		strcts: string;
		roles: string;
		enums: string;
		bitfs: string;
	}

	/**
	 * These callbacks are adding custom manipulation of the DOM for
	 * specific types of objects.  These callbacks are invoked after all
	 * other processing for the object (e.g., the field) has run.
	 */
	export interface ortJsonConfigCallbacks {
		strct?: (e: HTMLElement, strct: ortJson.strctObj,
			arg?: any) => void;
		field?: (e: HTMLElement, strct: ortJson.fieldObj,
			arg?: any) => void;
		enm?: (e: HTMLElement, strct: ortJson.enumObj,
			arg?: any) => void;
		eitem?: (e: HTMLElement, strct: ortJson.enumItemObj,
			arg?: any) => void;
		bitf?: (e: HTMLElement, strct: ortJson.bitfObj,
			arg?: any) => void;
		bitindex?: (e: HTMLElement, strct: ortJson.bitIndexObj,
			arg?: any) => void;
		role?: (e: HTMLElement, strct: ortJson.roleObj,
			arg?: any) => void;
	}

	/**
	 * Manage serialising an ortJsonConfig object into an HTML DOM
	 * tree.
	 */
	export class ortJsonConfigFormat {
		private readonly obj: ortJson.configObj;
		private readonly prefixes: ortJson.ortJsonConfigPrefixes;

		constructor(obj: ortJson.ortJsonConfig,
			prefixes?: ortJson.ortJsonConfigPrefixes) 
		{
			this.obj = obj.config;
			if (typeof(prefixes) === 'undefined') {
				this.prefixes = {
					'strcts': '',
					'roles': '',
					'enums': '',
					'bitfs': '',
				};
			} else
				this.prefixes = prefixes;
		}

		private commentToString(doc: string|null): string
		{
			if (doc === null)
				return '';
			return ' comment \"' + doc.replace(/"/g, '\\\"') + '\"';
		}

		private roleObjToString(r: roleObj): string
		{
			let str: string = ' role ' + r.name;
			str += this.commentToString(r.doc);
			if (r.subrq.length)
				str += ' {' + this.roleSubrqToString(r) + '}';
			return str + ';';
		}

		private roleSubrqToString(r: roleObj): string
		{
			let str: string = '';
			for (let i: number = 0; i < r.subrq.length; i++) {
				if (this.obj.rq === null)
					continue;
				const nr: roleObj|undefined =
					this.obj.rq[r.subrq[i]];
				if (nr === undefined)
					continue;
				str += this.roleObjToString(nr);
			}
			return str;
		}

		private roleSetToString(): string 
		{
			if (this.obj.rq === null)
				return '';
			const r: roleObj|undefined = this.obj.rq['all'];
			if (r === undefined)
				return '';
			return ' roles {' + this.roleSubrqToString(r) + '};';
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

		private labelSetToString(set: labelSet, name?: string|null,
			semi?: boolean): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			if (keys.length === 0)
				return '';
			if (typeof name !== 'undefined' && name !== null)
				str += ' ' + name;
			for (let i: number = 0; i < keys.length; i++) {
				const label: string =
					set[keys[i]].value.replace
						(/"/g, '\\\"');
				str += ' jslabel' + 
					(keys[i] === '_default' ? '' :
					 ('.' + keys[i])) +
					' \"' + label + '\"';
			}
			if (typeof semi !== 'undefined' && semi)
				str += ';';
			return str;
		}

		private bitIndexObjToString(name: string, bit: bitIndexObj): string
		{
			let str: string = ' item ' + 
				name + ' ' + bit.value.toString();
			str += this.commentToString(bit.doc);
			str += this.labelSetToString(bit.labels);
			return str + ' ;';
		}

		private bitfObjToString(bitf: bitfObj): string
		{
			let str: string = ' bitfield ' + bitf.name + ' {';
			const keys: string[] = Object.keys(bitf.bq);
			for (let i: number = 0; i < keys.length; i++)
				str += this.bitIndexObjToString(keys[i],
					bitf.bq[keys[i]]);
			str += this.commentToString(bitf.doc);
			if (bitf.doc !== null)
				str += ';';
			str += this.labelSetToString
				(bitf.labelsNull, 'isnull', true);
			str += this.labelSetToString
				(bitf.labelsUnset, 'isunset', true);
			return str + ' };';
		}

		private bitfSetToString(set: bitfSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.bitfObjToString(set[keys[i]]);
			return str;
		}

		private enumItemObjToString(name: string, eitem: enumItemObj): string
		{
			let str: string = ' item ' + name;
			if (eitem.value !== null)
				str += ' ' + eitem.value.toString();
			str += this.commentToString(eitem.doc);
			str += this.labelSetToString(eitem.labels);
			return str + ' ;';
		}

		private enumObjToString(enm: enumObj): string
		{
			let str: string = ' enum ' + enm.name + ' {';
			const keys: string[] = Object.keys(enm.eq);
			for (let i: number = 0; i < keys.length; i++)
				str += this.enumItemObjToString(keys[i],
					enm.eq[keys[i]]);
			str += this.commentToString(enm.doc);
			if (enm.doc !== null)
				str += ';';
			str += this.labelSetToString
				(enm.labelsNull, 'isnull', true);
			return str + ' };';
		}

		private enumSetToString(set: enumSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.enumObjToString(set[keys[i]]);
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

		private updateObjToString(up: updateObj): string
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
			str += this.commentToString(up.doc);
			if (up.name !== null)
				str += ' name ' + up.name;
			return str + ';';
		}

		private updateSetToString(set: updateSet): string
		{
			let str: string = '';
			const keys: string[] = Object.keys(set);
			for (let i: number = 0; i < keys.length; i++)
				str += this.updateObjToString(set[keys[i]]);
			return str;
		}

		private searchObjToString(search: searchObj): string
		{
			let str: string = ' ' + search.type;
			for (let i: number = 0; i < search.sntq.length; i++) {
				if (i > 0)
					str += ',';
				str += ' ' + search.sntq[i].fname + 
					' ' + search.sntq[i].op;
			}
			str += ':';
			str += this.commentToString(search.doc);
			if (search.limit !== '0' || search.offset !== '0') {
				str += ' limit ' + search.limit;
				if (search.offset !== '0')
					str += ', ' + search.offset;
			}
			if (search.name !== null)
				str += ' name ' + search.name;
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
				str += this.searchObjToString(set[keys[i]]);
			return str;
		}

		private rolemapObjToString(map: rolemapObj): string
		{
			if (map.rq.length === 0)
				return '';
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

		private strctObjToString(strct: strctObj): string
		{
			let str: string = ' struct ' + strct.name + ' {';
			str += this.fieldSetToString(strct.fq);

			for (let i: number = 0; i < strct.sq.anon.length; i++) 
		       		str += this.searchObjToString(strct.sq.anon[i]);	
			str += this.searchSetToString(strct.sq.named);

			for (let i: number = 0; i < strct.uq.anon.length; i++) 
		       		str += this.updateObjToString(strct.uq.anon[i]);	
			str += this.updateSetToString(strct.uq.named);

			for (let i: number = 0; i < strct.dq.anon.length; i++) 
		       		str += this.updateObjToString(strct.dq.anon[i]);	
			str += this.updateSetToString(strct.dq.named);
			if (strct.insert !== null)
				str += ' insert;';
			str += this.commentToString(strct.doc);
			if (strct.doc !== null) 
				str += ';';
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
				str += this.strctObjToString(set[keys[i]]);
			return str;
		}

		/**
		 * Convert the configuration to an ort(5) document.
		 * It does not validation of the document.
		 *
		 * Throughout this, the "pos" elements of the current
		 * configuration are all ignored.
		 */
		toString(): string
		{
			let str: string = '';

			if (this.obj.rq !== null)
				str += this.roleSetToString();
			str += this.bitfSetToString(this.obj.bq);
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

		private list(e: HTMLElement, name: string): HTMLElement[]
		{
			const ret: HTMLElement[] = [];
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz: number = list.length;
			for (let i: number = 0; i < sz; i++)
				ret.push(<HTMLElement>list[i]);
			if (e.classList.contains(name))
				ret.push(e);
			return ret;
		}

		private rattrcl(e: HTMLElement, name: string, at: string): void
		{
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz: number = list.length;
			for (let i: number = 0; i < sz; i++)
				list[i].removeAttribute(at);
			if (e.classList.contains(name))
				e.removeAttribute(at);
		}

		private attrcl(e: HTMLElement, name: string, at: string, 
			val: string): void 
		{
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz: number = list.length;
			for (let i: number = 0; i < sz; i++)
				list[i].setAttribute(at, val);
			if (e.classList.contains(name))
				e.setAttribute(at, val);
		}

		private replcl(e: HTMLElement, name: string, txt: string): void
		{
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz: number = list.length;
			for (let i: number = 0; i < sz; i++)
				list[i].textContent = txt;
			if (e.classList.contains(name))
				e.textContent = txt;
		}

		private clr(e: HTMLElement): HTMLElement
		{
			while (e.firstChild)
				e.removeChild(e.firstChild);
			return e;
		}

		private show(e: HTMLElement): HTMLElement
		{
			if (e.classList.contains('hide'))
				e.classList.remove('hide');
			return e;
		}

		private hide(e: HTMLElement): HTMLElement
		{
			if (!e.classList.contains('hide'))
				e.classList.add('hide');
			return e;
		}

		private hidecl(e: HTMLElement, name: string): void 
		{
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz = list.length;
			for (let i: number = 0; i < sz; i++)
				list[i].classList.add('hide');
			if (e.classList.contains(name))
				e.classList.add('hide');
		}

		private showcl(e: HTMLElement, name: string): void 
		{
			const list: HTMLCollectionOf<Element> =
				e.getElementsByClassName(name);
			const sz = list.length;
			for (let i: number = 0; i < sz; i++)
				list[i].classList.remove('hide');
			if (e.classList.contains(name))
				e.classList.remove('hide');
		}

		/**
		 * Invokes fill() for each element with the given class
		 * (inclusive) under the root.
		 */
		fillByClass(root: HTMLElement|string, name: string,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			const e: HTMLElement|null = this.find(root);
			if (e === null)
				return;
			const list: HTMLElement[] = this.list(e, name);
			for (let i: number = 0; i < list.length; i++)
				this.fill(list[i], cb, arg);
		}

		/**
		 * Fills the configuration the given element (inclusive),
		 * starting with structures, then drilling down.  Elements are
		 * manipulated by whether the contain the following classes.  To
		 * "hide" an element means to append the *hide* class to its
		 * class list, while to "show" an element is to remove this.
		 * The caller will need to make sure that these classes contain
		 * the appropriate `display: none` or similar style definitions.
		 *
		 * - fillStrctSet() (for structs)
		 * - fillRoleSet() (for roles)
		 * - fillEnumSet() (for enums)
		 * - fillBitfSet() (for bitfields)
		 *
		 * - *config-strcts-prefix-link*: sets 'href' to the strcts
		 *   prefix fragment, even if empty
		 * - *config-roles-prefix-link*: sets 'href' to the roles
		 *   prefix fragment, even if empty
		 * - *config-enums-prefix-link*: sets 'href' to the enums
		 *   prefix fragment, even if empty
		 * - *config-bitfs-prefix-link*: sets 'href' to the bitfs
		 *   prefix fragment, even if empty
		 * - *config-strcts-prefix-id*: sets 'id' to the strcts prefix,
		 *   even if empty
		 * - *config-roles-prefix-id*: sets 'id' to the roles prefix,
		 *   even if empty
		 * - *config-enums-prefix-id*: sets 'id' to the enums prefix,
		 *   even if empty
		 * - *config-bitfs-prefix-id*: sets 'id' to the bitfs prefix,
		 *   even if empty
		 */
		fill(e: string|HTMLElement|null,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			if (e === null)
				return;
			const pn: HTMLElement|null = this.find(e);
			if (pn === null)
				return;
			this.attrcl(pn, 'config-strcts-prefix-id',
				'id', this.prefixes.strcts);
			this.attrcl(pn, 'config-roles-prefix-id',
				'id', this.prefixes.roles);
			this.attrcl(pn, 'config-enums-prefix-id',
				'id', this.prefixes.enums);
			this.attrcl(pn, 'config-bitfs-prefix-id',
				'id', this.prefixes.bitfs);
			this.attrcl(pn, 'config-strcts-prefix-link',
				'href', '#' + this.prefixes.strcts);
			this.attrcl(pn, 'config-roles-prefix-link',
				'href', '#' + this.prefixes.roles);
			this.attrcl(pn, 'config-enums-prefix-link',
				'href', '#' + this.prefixes.enums);
			this.attrcl(pn, 'config-bitfs-prefix-link',
				'href', '#' + this.prefixes.bitfs);
			this.fillRoleSet(pn, undefined, cb, arg);
			this.fillStrctSet(pn, cb, arg);
			this.fillEnumSet(pn, cb, arg);
			this.fillBitfSet(pn, cb, arg);
			this.show(pn);
		}

		/**
		 * For a possibly-null comment string "doc" of the given name
		 * "type" (e.g., "strct" or "field"):
		 *
		 * - *config-type-doc-{none,has}*: shown or hidden if "doc" is
		 *   non-null
		 * - *config-type-doc*: filled with "doc" or emptied if null
		 * - *config-type-doc-value*: set *value* to "doc" or empty if
		 *   null
		 */
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
		 * For all bitfields in a configuration:
		 *
		 * - *config-bitfs-{has,none}*: shown or hidden depending on
		 *   whether there are bitfields
		 * - *config-bitfs*: the first child of these is cloned and
		 *   filled in with fillBitfObj() for each bitfield unless there
		 *   are no bitfields, in which case the elements are hidden
		 * - *config-bitfs-name-link*: set *href* to 'bitfs' within
		 *   the *config-bitfs-prefix-link* prefix
		 * - *config-bitfs-name-id*: set *id* to 'bitfs' within the
		 *   *config-bitfs-prefix-id* prefix
		 */
		fillBitfSet(e: HTMLElement,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-bitfs-name-link', 
				'href', '#' + this.prefixes.bitfs + 'bitfs');
			this.attrcl(e, 'config-bitfs-name-id', 
				'id', this.prefixes.bitfs + 'bitfs');
			const keys: string[] = Object.keys(this.obj.bq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-bitfs-has');
				this.showcl(e, 'config-bitfs-none');
				this.hidecl(e, 'config-bitfs');
				return;
			}
			keys.sort();
			this.showcl(e, 'config-bitfs-has');
			this.hidecl(e, 'config-bitfs-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-bitfs');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (const name in this.obj.bq) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					this.fillBitfObj(cln, 
						this.obj.bq[name], cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For all enumerations in a configuration:
		 *
		 * - *config-enums-{has,none}*: shown or hidden depending on
		 *   whether there are enumerations
		 * - *config-enums*: the first child of these is cloned and
		 *   filled in with fillEnumObj() for each enumeration unless
		 *   there are no enumerations, in which case the elements are
		 *   hidden
		 * - *config-enums-name-link*: set *href* to 'enums' within
		 *   the *config-enums-prefix-link* prefix
		 * - *config-enums-name-id*: set *id* to 'enums' within the
		 *   *config-enums-prefix-id* prefix
		 */
		fillEnumSet(e: HTMLElement,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-enums-name-link', 
				'href', '#' + this.prefixes.enums + 'enums');
			this.attrcl(e, 'config-enums-name-id', 
				'id', this.prefixes.enums + 'enums');
			const keys: string[] = Object.keys(this.obj.eq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-enums-has');
				this.showcl(e, 'config-enums-none');
				this.hidecl(e, 'config-enums');
				return;
			}
			keys.sort();
			this.showcl(e, 'config-enums-has');
			this.hidecl(e, 'config-enums-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-enums');
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
					this.fillEnumObj(cln, 
						this.obj.eq[keys[j]], cb, arg);
					list[i].appendChild(cln);
				}
			}
		}

		/**
		 * For a bitfield:
		 *
		 * - *config-bitf-name*: filled in with the bitfield name
		 * - *config-bitf-name-data*: *data-name* set to the bitfield
		 *   name
		 * - *config-bitf-name-value*: value set to the bitfield name
		 * - *config-bitf-name-id*: *id* set as the name within the
		 *   *config-bitfs-name-id* prefix
		 * - *config-bitf-name-link*: *href* set as the name within the
		 *   *config-bitfs-name-link* prefix
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * For bitfield items:
		 * - see fillBitIndexSet()
		 *
		 * For labels:
		 * - see fillLabelSet() ("bitf-null" and "bitf-unset")
		 */
		fillBitfObj(e: HTMLElement, bitf: ortJson.bitfObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillComment(e, 'bitf', bitf.doc);
			this.replcl(e, 'config-bitf-name', bitf.name);
			this.attrcl(e, 'config-bitf-name-value', 
				'value', bitf.name);
			this.attrcl(e, 'config-bitf-name-data', 
				'data-name', bitf.name);
			this.attrcl(e, 'config-bitf-name-id', 'id', 
				this.prefixes.bitfs + 'bitfs/' + bitf.name);
			this.attrcl(e, 'config-bitf-name-link', 'href', 
				'#' + this.prefixes.bitfs + 'bitfs/' + 
				bitf.name);
			this.fillBitIndexSet(e, bitf, cb, arg);
			this.fillLabelSet(e, 'bitf-null', bitf.labelsNull);
			this.fillLabelSet(e, 'bitf-unset', bitf.labelsUnset);
			if (typeof cb !== 'undefined' &&
			    typeof cb.bitf !== 'undefined')
				cb.bitf(e, bitf, arg);
		}

		/**
		 * For an enumeration:
		 *
		 * - *config-enum-name*: filled in with the enum name
		 * - *config-enum-name-id*: *id* set as the name within the
		 *   *config-enums-name-id* prefix
		 * - *config-enum-name-link*: *href* set as the name within the
		 *   *config-enums-name-link* prefix
		 * - *config-enum-name-value*: value set to the enum name
		 * - *config-enum-name-data*: *data-name* attribute set to the
		 *   enum name
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * For enumeration items:
		 * - see fillEnumItemSet() 
		  
		 * For labels:
		 * - see fillLabelSet() (name "enum-null")
		 */
		fillEnumObj(e: HTMLElement, enm: ortJson.enumObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillComment(e, 'enum', enm.doc);
			this.replcl(e, 'config-enum-name', enm.name);
			this.attrcl(e, 'config-enum-name-value', 
				'value', enm.name);
			this.attrcl(e, 'config-enum-name-data', 
				'data-name', enm.name);
			this.attrcl(e, 'config-enum-name-id', 'id', 
				this.prefixes.enums + 'enums/' + enm.name);
			this.attrcl(e, 'config-enum-name-link', 'href', 
				'#' + this.prefixes.enums + 'enums/' + 
				enm.name);
			this.fillEnumItemSet(e, enm, cb, arg);
			this.fillLabelSet(e, 'enum-null', enm.labelsNull);
			if (typeof cb !== 'undefined' &&
			    typeof cb.enm !== 'undefined')
				cb.enm(e, enm, arg);
		}

		/**
		 * For a set of enumeration items:
		 *
		 * - *config-eitems-name-link*: set *href* to 'eitems' within
		 *   the *config-enum-name-link* prefix
		 * - *config-eitems-name-id*: set *id* to 'eitems' within the
		 *   *config-enum-name-id* prefix
		 * - *config-eitems-{has,none}*: shown or hidden depending on
		 *   whether there are enumeration items
		 * - *config-eitems*: first child of the element is cloned and
		 *   filled in with fillEnumItemObj() for each item unless there
		 *   are no items, in which case the element is hidden
		 */
		private fillEnumItemSet(e: HTMLElement, enm: ortJson.enumObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-eitems-name-link', 'href', 
				'#' + this.prefixes.enums + 'enums/' + 
				enm.name + '/eitems');
			this.attrcl(e, 'config-eitems-name-id', 'id', 
				this.prefixes.enums + 'enums/' + 
				enm.name + '/eitems');
			const keys: string[] = Object.keys(enm.eq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-eitems-has');
				this.showcl(e, 'config-eitems-none');
				this.hidecl(e, 'config-eitems');
				return;
			}
			keys.sort();
			this.showcl(e, 'config-eitems-has');
			this.hidecl(e, 'config-eitems-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-eitems');
			for (let i: number = 0; i < list.length; i++) {
				const tmpl: HTMLElement = 
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillEnumItemObj(cln, 
						enm.eq[keys[j]], cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For an enumeration item:
		 *
		 * - *config-enumitem-name*: set text to item name
		 * - *config-enumitem-name-value*: set *value* to item name
		 * - *config-enumitem-name-id*: *id* set as the name within the
		 *   *config-enums-name-id* prefix
		 * - *config-enumitem-name-link*: *href* set as the name within the
		 *   *config-enums-name-link* prefix
		 * - *config-enumitem-value-{has,none}*: shown or hidden depending
		 *   on whether there's a value for the enumerateion item
		 * - *config-enumitem-value*: filled in with the bitfield item
		 *   name, if applicable, or an empty string of unset
		 * - *config-enumitem-value-value*: *value* set to item value,
		 *   if applicable, or an empty string if unset
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * For labels:
		 * - see fillLabelSet() (name "enumitem")
		 */
		fillEnumItemObj(e: HTMLElement, eitem: ortJson.enumItemObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillComment(e, 'enumitem', eitem.doc);
			this.attrcl(e, 'config-enumitem-name-id', 'id', 
				this.prefixes.enums + 'enums/' + 
				eitem.parent + '/eitems/' + eitem.name);
			this.attrcl(e, 'config-enumitem-name-link', 'href', 
				'#' + this.prefixes.enums + 'enums/' +
				eitem.parent + '/eitems/' + eitem.name);
			this.replcl(e, 'config-enumitem-name', eitem.name);
			this.attrcl(e, 'config-enumitem-name-value', 
				'value', eitem.name);
			if (eitem.value !== null) {
				this.showcl(e, 'config-enumitem-value-has');
				this.hidecl(e, 'config-enumitem-value-none');
				this.replcl(e, 'config-enumitem-value', 
					eitem.value.toString());
				this.attrcl(e, 'config-enumitem-value-value', 
					'value', eitem.value.toString());
			} else {
				this.hidecl(e, 'config-enumitem-value-has');
				this.showcl(e, 'config-enumitem-value-none');
				this.replcl(e, 'config-enumitem-value', '');
				this.attrcl(e, 'config-enumitem-value-value', 
					'value', '');
			}
			this.fillLabelSet(e, 'enumitem', eitem.labels);
			if (typeof cb !== 'undefined' &&
			    typeof cb.eitem !== 'undefined')
				cb.eitem(e, eitem, arg);
		}

		/**
		 * For a set of labels:
		 *
		 * - *config-NAME-labels-{has,none}*: shown or hidden depending
		 *   on whether the set is empty
		 * - *config-NAME-labels*: first element cloned, others cleared,
		 *   filled with fillLabelObj()
		 */
		private fillLabelSet(e: HTMLElement, name: string, 
			labels: ortJson.labelSet): void
		{
			const keys: string[] = Object.keys(labels);
			if (keys.length === 0) {
				this.hidecl(e, 'config-' + name + '-labels-has');
				this.showcl(e, 'config-' + name + '-labels-none');
				this.hidecl(e, 'config-' + name + '-labels');
				return;
			}
			keys.sort();
			this.showcl(e, 'config-' + name + '-labels-has');
			this.hidecl(e, 'config-' + name + '-labels-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-' + name + '-labels');
			for (let i: number = 0; i < list.length; i++) {
				const tmpl: HTMLElement = 
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillLabelObj(cln, labels[keys[j]]);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For a single label:
		 *
		 * - *config-label-lang*: set to the label language
		 * - *config-label-value*: set to the label value
		 */
		private fillLabelObj(e: HTMLElement, label: ortJson.labelObj): void
		{
			this.replcl(e, 'config-label-lang', label.lang);
			this.replcl(e, 'config-label-value', label.value);
		}

		/**
		 * For a set of bitfield items:
		 *
		 * - *config-bitems-name-link*: set *href* to 'bitems' within
		 *   the *config-bitf-name-link* prefix
		 * - *config-bitems-name-id*: set *id* to 'bitems' within the
		 *   *config-bitf-name-id* prefix
		 * - *config-bitems-{has,none}*: shown or hidden depending on
		 *   whether there are bitfield items
		 * - *config-bitems*: first child of the element is cloned and
		 *   filled in with fillBitIndexObj() for each item unless there
		 *   are no items, in which case the element is hidden
		 */
		private fillBitIndexSet(e: HTMLElement, bitf: ortJson.bitfObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-bitems-name-link', 'href', 
				'#' + this.prefixes.bitfs + 'bitfs/' + 
				bitf.name + '/bitems');
			this.attrcl(e, 'config-bitems-name-id', 'id', 
				this.prefixes.bitfs + 'bitfs/' + 
				bitf.name + '/bitems');
			const keys: string[] = Object.keys(bitf.bq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-bitems-has');
				this.showcl(e, 'config-bitems-none');
				this.hidecl(e, 'config-bitems');
			}
			keys.sort();
			this.showcl(e, 'config-bitems-has');
			this.hidecl(e, 'config-bitems-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-bitems');
			for (let i: number = 0; i < list.length; i++) {
				const tmpl: HTMLElement = 
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillBitIndexObj(cln,
						bitf.bq[keys[j]], cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For a bitfield item:
		 *
		 * - *config-bitindex-name-id*: *id* set as the name within the
		 *   *config-bitfs-name-id* prefix
		 * - *config-bitindex-name-link*: *href* set as the name within the
		 *   *config-bitfs-name-link* prefix
		 * - *config-bitindex-name*: set text to item name
		 * - *config-bitindex-name-value*: *value* set to name
		 * - *config-bitindex-value*: set text to item value
		 * - *config-bitindex-value-value*: *value* set to item value
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * For labels:
		 * - see fillLabelSet() (name "bitindex")
		 */
		fillBitIndexObj(e: HTMLElement, biti: ortJson.bitIndexObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillComment(e, 'bitindex', biti.doc);
			this.attrcl(e, 'config-bitindex-name-id', 'id', 
				this.prefixes.bitfs + 'bitfs/' + 
				biti.parent + '/bitems/' + biti.name);
			this.attrcl(e, 'config-bitindex-name-link', 'href', 
				'#' + this.prefixes.bitfs + 'bitfs/' +
				biti.parent + '/bitems/' + biti.name);
			this.replcl(e, 'config-bitindex-name', biti.name);
			this.replcl(e, 'config-bitindex-value', 
				biti.value.toString());
			this.attrcl(e, 'config-bitindex-name-value', 
				'value', biti.name);
			this.attrcl(e, 'config-bitindex-value-value', 
				'value', biti.value.toString());
			this.fillLabelSet(e, 'bitindex', biti.labels);
			if (typeof cb !== 'undefined' &&
			    typeof cb.bitindex !== 'undefined')
				cb.bitindex(e, biti, arg);
		}

		/**
		 * For all roles in a configuration:
		 *
		 * - *config-roles-{has,none}*: shown or hidden depending on
		 *   whether there are roles at all (even if not having
		 *   user-defined roles)
		 * - *config-roles*: the first child of these is cloned and
		 *   filled in with fillRoleObj() for each role unless there
		 *   are no roles, in which case the elements are hidden
		 * - *config-roles-name-link*: set *href* to 'roles' within
		 *   the *config-roles-prefix-link* prefix
		 * - *config-roles-name-id*: set *id* to 'roles' within the
		 *   *config-roles-prefix-id* prefix
		 *
		 * Accepts an optional array of role names not to show.
		 */
		fillRoleSet(e: HTMLElement, exclude?: string[],
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-roles-name-link', 
				'href', '#' + this.prefixes.roles + 'roles');
			this.attrcl(e, 'config-roles-name-id', 
				'id', this.prefixes.roles + 'roles');
			const newrq: roleObj[] = [];
			if (this.obj.rq !== null)
				for (const name in this.obj.rq)
					if (typeof(exclude) === 'undefined' ||
					    exclude.indexOf(name) === -1)
						newrq.push(this.obj.rq[name]);

			if (newrq.length === 0) {
				this.hidecl(e, 'config-roles-has');
				this.showcl(e, 'config-roles-none');
				this.hidecl(e, 'config-roles');
				return;
			}
			newrq.sort(function(a: roleObj, b: roleObj) {
				return a.name.localeCompare(b.name);
			});
			this.showcl(e, 'config-roles-has');
			this.hidecl(e, 'config-roles-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-roles');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j: number = 0; j < newrq.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					this.fillRoleObj(cln, newrq[j], cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For each role:
		 *
		 * - *config-role-name*: filled in with the role name
		 * - *config-role-name-value*: value set to the role name
		 * - *config-role-name-data*: 'data-name' attribute set with
		 *   name
		 * - *config-role-name-id*: *id* set as the name within the
		 *   *config-roles-name-id* prefix
		 * - *config-role-name-link*: *href* set as the name within the
		 *   *config-roles-name-link* prefix
		 * - *config-role-parent-{has,none}*: shown or hidden depending
		 *   upon whether the parent is set
		 * - *config-role-parent*: filled in with the parent name or an
		 *   empty string if the parent is not set
		 * - *config-role-subrq-{has,none}*: shown or hidden depending
		 *   upon whether the role has children
		 * - *config-role-subrq-join*: filled in with comma-separated
		 *   children or an empty string
		 * - *config-role-subrq*: clone each element beneath for each
		 *   subrole and fill in the *config-role-subrq-role-name* and
		 *   *config-role-subrq-role-link* (or hide if none)
		 *
		 * For documentation:
		 * - see fillComment()
		 */
		fillRoleObj(e: HTMLElement, role: ortJson.roleObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillComment(e, 'role', role.doc);
			this.replcl(e, 'config-role-name', role.name);
			this.attrcl(e, 'config-role-name-data', 
				'data-name', role.name);
			this.attrcl(e, 'config-role-name-id', 'id', 
				this.prefixes.roles + 'roles/' + role.name);
			this.attrcl(e, 'config-role-name-link', 'href', 
				'#' + this.prefixes.roles + 'roles/' + 
				role.name);
			if (role.parent === null) {
				this.hidecl(e, 'config-role-parent-has');
				this.showcl(e, 'config-role-parent-none');
				this.replcl(e, 'config-role-parent', '');
			} else {
				this.showcl(e, 'config-role-parent-has');
				this.hidecl(e, 'config-role-parent-none');
				this.replcl(e, 'config-role-parent', role.parent);
				this.attrcl(e, 'config-role-parent-link', 
					'href', '#' + this.prefixes.roles + 
					'roles/' + role.parent);
			}
			this.attrcl(e, 'config-role-name-value', 
				'value', role.name);
			if (role.subrq.length === 0) {
				this.showcl(e, 'config-role-subrq-none');
				this.hidecl(e, 'config-role-subrq-has');
				this.replcl(e, 'config-role-subrq-join', '');
				this.hidecl(e, 'config-role-subrq');
			} else { 
				this.hidecl(e, 'config-role-subrq-none');
				this.showcl(e, 'config-role-subrq-has');
				this.replcl(e, 'config-role-subrq-join', 
					role.subrq.join(', '));
				const list: HTMLElement[] = 
					this.list(e, 'config-role-subrq');
				for (let i: number = 0; i < list.length; i++) {
					if (list[i].children.length === 0)
						continue;
					const tmpl: HTMLElement =
						<HTMLElement>list[i].children[0];
					this.clr(list[i]);
					for (let j: number = 0; 
					     j < role.subrq.length; j++) {
						const cln: HTMLElement = 
							<HTMLElement>
							tmpl.cloneNode(true);
						this.replcl(cln, 
							'config-role-subrq-role-name',
							role.subrq[j]);
						this.attrcl(cln, 
							'config-role-subrq-role-link', 
							'href', '#' + 
							this.prefixes.roles + 
							'roles/' + role.subrq[j]);
						list[i].appendChild(cln);
					}
					this.show(list[i]);
				}
			}
			if (typeof cb !== 'undefined' &&
			    typeof cb.role !== 'undefined')
				cb.role(e, role, arg);
		}

		/**
		 * For a set of strcts:
		 *
		 * - *config-strcts-{has,none}*: shown or hidden depending on
		 *   whether there are strcts
		 * - *config-strcts*: the first child of these is cloned and
		 *   filled in with data for each strct (see fillStrctObj())
		 *   unless there are no strcts, in which case the elements are
		 *   hidden
		 * - *config-strcts-name-link*: set *href* to 'strcts' within
		 *   the *config-strcts-prefix-link* prefix
		 * - *config-strcts-name-id*: set *id* to 'strcts' within the
		 *   *config-strcts-prefix-id* prefix
		 */
		fillStrctSet(e: HTMLElement,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.attrcl(e, 'config-strcts-name-link', 
				'href', '#' + this.prefixes.strcts + 'strcts');
			this.attrcl(e, 'config-strcts-name-id', 
				'id', this.prefixes.strcts + 'strcts');
			const keys: string[] = Object.keys(this.obj.sq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-strcts-has');
				this.hidecl(e, 'config-strcts');
				this.showcl(e, 'config-strcts-none');
				return;
			} 
			keys.sort();
			this.showcl(e, 'config-strcts-has');
			this.hidecl(e, 'config-strcts-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-strcts');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j: number = 0; j < keys.length; j++) {
					const strct: ortJson.strctObj =
						this.obj.sq[keys[j]];
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					this.fillStrctObj(cln, strct, cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * For a strct:
		 *
		 * - *config-strct-name*: filled in with name
		 * - *config-strct-name-id*: *id* set as the name within the
		 *   *config-strcts-name-id* prefix
		 * - *config-strct-name-link*: *href* set as the name within the
		 *   *config-strcts-name-link* prefix
		 * - *config-strct-name-value*: value set with name
		 * - *config-strct-name-data*: *data-name* attribute set with
		 *   name
		 * - *config-fields*: the first child of this is cloned and
		 *   filled in with data for each field (see "Per field")
		 * - *config-insert-{has,none}*: shown or hidden depending on
		 *   whether an insert is defined (see "Per insert" if there is
		 *   an insert defined)
		 * - *config-uniques-{has,none}*: shown or hidden depending upon
		 *   whether there are any unique tuples
		 * - *config-uniques*: the first child of this is cloned and
		 *   filled in with data for each unique tuple (see "Per
		 *   unique") unless there are no tuples, in which case the
		 *   element is hidden
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * For any insert:
		 * - see fillRolemap()
		 *
		 * For any unique:
		 * - see fillUniques()
		 *
		 * For updates and deletes:
		 * - see fillUpdateClassObj()
		 *
		 * For any fields:
		 * - see fillFieldSet()
		 *
		 * For queries:
		 * - see fillSearchClassObj() 
		 */
		fillStrctObj(e: HTMLElement, strct: ortJson.strctObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			this.fillFieldSet(e, strct, undefined, cb, arg);

			if (strct.nq.length) {
				this.showcl(e, 'config-uniques-has');
				this.hidecl(e, 'config-uniques-none');
				const list: HTMLElement[] = 
					this.list(e, 'config-uniques');
				for (let i: number = 0; i < list.length; i++)
					this.fillUniques(list[i], strct.nq);
			} else {
				this.hidecl(e, 'config-uniques-has');
				this.showcl(e, 'config-uniques-none');
			}

			this.fillUpdateClassObj(e, 'deletes', strct, strct.dq);
			this.fillUpdateClassObj(e, 'updates', strct, strct.uq);
			this.fillSearchClassObj(e, strct);

			if (strct.insert !== null) {
				this.showcl(e, 'config-insert-has');
				this.hidecl(e, 'config-insert-none');
				this.fillRolemap(e, 'insert', 
					strct.insert.rolemap);
			} else {
				this.hidecl(e, 'config-insert-has');
				this.showcl(e, 'config-insert-none');
				this.fillRolemap(e, 'insert', null);
			}

			this.fillComment(e, 'strct', strct.doc);
			this.replcl(e, 'config-strct-name', strct.name);
			this.attrcl(e, 'config-strct-name-value', 
				'value', strct.name);
			this.attrcl(e, 'config-strct-name-data', 
				'data-name', strct.name);
			this.attrcl(e, 'config-strct-name-id', 'id', 
				this.prefixes.strcts + 'strcts/' + strct.name);
			this.attrcl(e, 'config-strct-name-link', 'href', 
				'#' + this.prefixes.strcts + 'strcts/' + 
				strct.name);
			if (typeof cb !== 'undefined' &&
			    typeof cb.strct !== 'undefined')
				cb.strct(e, strct, arg);
		}

		/**
		 * Per rolemap with the given NAME:
		 *
		 * - *config-NAME-rolemap-{has,none}*: shown or hidden depending
		 *   on whether there's a non-empty rolemap
		 * - *config-NAME-rolemap-join*: set to the comma-separated role
		 *   names if the rolemap is non-empty
		 * - *config-NAME-rolemap*: first child of this is
		 *   cloned and filled in with the name if matching
		 *   *config-NAME-rolemap-role*, the *value* set if
		 *   matching *config-NAME-rolemap-role-value*, the *href* set
		 *   to the role's id if *config-NAME-rolemap-role-link* unless
		 *   the rolemap is empty, in which case the element is hidden
		 */
		private fillRolemap(e: HTMLElement, 
			name: string, map: string[]|null): void
		{
			const cls: string = 'config-' + name + '-rolemap';
			if (map === null || map.length === 0) {
				this.replcl(e, cls + '-join', '');
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
				this.clr(list[i]);
				for (let j: number = 0; j < map.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					this.replcl(cln, cls + '-role', map[j]);
					this.attrcl(cln, cls + '-role-value', 
						'value', map[j]);
					this.attrcl(cln, cls + '-role-link', 
						'href', '#' + this.prefixes.roles + 
						'roles/' + map[j]);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
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
				this.replcl(cln, 'config-unique', 
					  nq[i].nq.join(', '));
				e.appendChild(cln);
			}
		}

		/**
		 * For the updates/deletes in a struct, depending upon whether
		 * the 'deletes' or 'updates' is given as the type
		 *
		 * - *config-_TYPE_-name-link*: set *href* to type within
		 *   the *config-strct-name-link* prefix
		 * - *config-_TYPE_-name-id*: set *id* to the type within the
		 *   *config-strct-name-id* prefix
		 * - *config-_TYPE_-{has,none}*: shown or hidden depending upon
		 *   whether there are any updates/deletes
		 * - *config-_TYPE_*: the first child of this is cloned and
		 *   filled in with data for each update (see fillUpdateObj())
		 *   unless there are no updates/deletes, in which case the
		 *   element is hidden
		 */
		private fillUpdateClassObj(e: HTMLElement, 
			type: 'updates'|'deletes', strct: ortJson.strctObj,
			cls: ortJson.updateClassObj): void
		{
			this.attrcl(e, 'config-' + type + '-name-link', 'href', 
				'#' + this.prefixes.strcts + 'strcts/' + 
				strct.name + '/' + type);
			this.attrcl(e, 'config-' + type + '-name-id', 'id', 
				this.prefixes.strcts + 'strcts/' + 
				strct.name + '/' + type);
			const keys: string[] = Object.keys(cls.named);
			if (keys.length + cls.anon.length === 0) {
				this.hidecl(e, 'config-' + type + '-has');
				this.showcl(e, 'config-' + type + '-none');
				this.hidecl(e, 'config-' + type);
				return;
			}
			keys.sort();
			this.showcl(e, 'config-' + type + '-has');
			this.hidecl(e, 'config-' + type + '-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-' + type);
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillUpdateObj(cln, type,
						cls.named[keys[j]], j);
					list[i].appendChild(cln);
				}
				for (let j = 0; j < cls.anon.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillUpdateObj(cln, type,
						cls.anon[j], j);
					list[i].appendChild(cln);
				}
			}
		}

		/**
		 * Per update (or delete):
		 *
		 * - see fillRolemap()
		 * - *config-update-name-{has,none}*: shown or hidden depending
		 *   on whether the update is anonymous
		 * - *config-update-name*: filled in with the update name if a
		 *   name is defined or empty
		 * - *config-update-name-value*: value filled in with the update
		 *   name if a name is defined or empty
		 * - *config-update-name-id*: *id* set as the name within the
		 *   *config-_type_-name-id* prefix (if anonymous, then _up and
		 *   a consistent number)
		 * - *config-update-name-link*: *href* set as the name within
		 *   the *config-_type_-name-link* prefix (if anonymous, then
		 *   _up and a consistent number)
		 * - *config-update-fields-{has,none}*: shown or hidden
		 *   depending on whether the update has non-zero mrq or crqs
		 * - *config-update-crq-{has,none}*: shown or hidden depending
		 *   on whether the update has non-zero crq
		 * - *config-update-mrq-{has,none}*: shown or hidden depending
		 *   on whether the update has non-zero mrq
		 * - *config-update-mrq*: the first child of this is cloned and
		 *   filled in with data for each reference (see "Per update
		 *   reference") unless there are no references, in which case
		 *   the element is hidden
		 * - *config-update-crq*: the first child of this is cloned and
		 *   filled in with data for each reference (see "Per update
		 *   reference") unless there are no references, in which case
		 *   the element is hidden
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * Per update reference:
		 * - *config-uref-field*: the field name in the current
		 *   structure
		 * - *config-uref-field-value*: value set to the field name in
		 *   the current structure
		 * - *config-uref-op*: the update constraint operator
		 * - *config-uref-op-value*: value set to the update constraint
		 *   operator
		 * - *config-uref-mod*: the update modifier
		 * - *config-uref-mod-value*: value set to the update modifier
		 */
		fillUpdateObj(e: HTMLElement, type: 'updates'|'deletes',
			up: ortJson.updateObj, pos: number): void
		{
			this.fillComment(e, 'update', up.doc);
			if (up.name === null) {
				this.hidecl(e, 'config-update-name-has');
				this.showcl(e, 'config-update-name-none');
				this.replcl(e, 'config-update-name', '');
				this.attrcl(e, 'config-update-name-value', 
					'value', '');
				this.attrcl(e, 'config-update-name-id', 'id', 
					this.prefixes.strcts + 'strcts/' + 
					up.parent + '/' + type + '/_up' + 
					pos);
				this.attrcl(e, 'config-update-name-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + up.parent + '/' + type + 
					'/_up' + pos);
			} else {
				this.hidecl(e, 'config-update-name-none');
				this.showcl(e, 'config-update-name-has');
				this.replcl(e, 'config-update-name', up.name);
				this.attrcl(e, 'config-update-name-value', 
					'value', up.name);
				this.attrcl(e, 'config-update-name-id', 'id', 
					this.prefixes.strcts + 'strcts/' +
					up.parent + '/' + type + '/' + 
					up.name);
				this.attrcl(e, 'config-update-name-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + up.parent + '/' + 
					type + '/' + up.name);
			}
			if (up.crq.length + up.mrq.length === 0) {
				this.showcl(e, 'config-update-fields-none');
				this.hidecl(e, 'config-update-fields-has');
			} else {
				this.hidecl(e, 'config-update-fields-none');
				this.showcl(e, 'config-update-fields-has');
			}
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
					this.fillUrefs(list[i], up.crq);
			}
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
					this.fillUrefs(list[i], up.mrq);
			}
			this.fillRolemap(e, 'update', up.rolemap);
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
				this.replcl(cln, 'config-uref-field', 
					urefs[i].field);
				this.attrcl(cln, 'config-uref-field-value', 
					'value', urefs[i].field);
				this.replcl(cln, 'config-uref-op', 
					urefs[i].op);
				this.attrcl(cln, 'config-uref-op-value', 
					'value', urefs[i].op);
				this.replcl(cln, 'config-uref-mod', 
					urefs[i].mod);
				this.attrcl(cln, 'config-uref-mod-value', 
					'value', urefs[i].mod);
				e.appendChild(cln);
			}
		}

		/**
		 * For the queries in a struct:
		 *
		 * - *config-queries-name-link*: set *href* to 'queries' within
		 *   the *config-strct-name-link* prefix
		 * - *config-queries-name-id*: set *id* to 'queries' within the
		 *   *config-strct-name-id* prefix
		 * - *config-queries-{has,none}*: shown or hidden depending upon
		 *   whether there are any queries
		 * - *config-queries*: the first child of this is cloned and
		 *   filled in with data for each query (see fillSearchObj())
		 *   unless there are no queries, in which case the element is
		 *   hidden
		 */
		private fillSearchClassObj(e: HTMLElement, 
			strct: ortJson.strctObj): void
		{
			this.attrcl(e, 'config-queries-name-link', 'href', 
				'#' + this.prefixes.strcts + 'strcts/' + 
				strct.name + '/queries');
			this.attrcl(e, 'config-queries-name-id', 'id', 
				this.prefixes.strcts + 'strcts/' + 
				strct.name + '/queries');
			const keys: string[] = Object.keys(strct.sq.named);
			if (keys.length + strct.sq.anon.length === 0) {
				this.hidecl(e, 'config-queries-has');
				this.showcl(e, 'config-queries-none');
				this.hidecl(e, 'config-queries');
				return;
			} 
			keys.sort();
			this.showcl(e, 'config-queries-has');
			this.hidecl(e, 'config-queries-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-queries');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillSearchObj(cln, 
						strct.sq.named[keys[j]], j);
					list[i].appendChild(cln);
				}
				for (let j = 0; j < strct.sq.anon.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillSearchObj(cln, strct.sq.anon[j], j);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
			}
		}

		/**
		 * Per query:
		 *
		 * - see fillRolemap()
		 * - *config-query-name-{has,none}*: shown or hidden
		 *   depending on whether the query is anonymous
		 * - *config-query-name*: filled in with the query name
		 *   if a name is defined, empty string otherwise
		 * - *config-query-name-value*: value filled in with the
		 *   query name if a name is defined, empty string 
		 *   otherwise
		 * - *config-query-name-id*: *id* set as the parent name,
		 *   forward slash, then name with the optional prefix as set
		 *   during construction (or _query + unique number)
		 * - *config-query-name-link*: *href* set as the same fragment
		 *   set in *config-query-name-id*
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
		 * - *config-query-limit-{has,none}*: whether a non-zero
		 *   limit is defined
		 * - *config-query-limoffs-{has,none}*: whether a
		 *   non-zero offset and/or limit is defined
		 * - *config-query-offset-{has,none}*: whether a non-zero
		 *   offset is defined
		 * - *config-query-offset*: filled in with the offset
		 * - *config-query-offset-value*: value filled in with
		 *   the offset
		 * - *config-query-limit*: filled in with the limit
		 * - *config-query-limit-value*: value filled in with
		 *   the limit
		 *
		 * For documentation:
		 * - see fillComment()
		 *
		 * Per query search field:
		 * - *config-sent-fname*: filled in with the fname
		 * - *config-sent-fname-value*: value filled in with the
		 *   fname
		 * - *config-sent-op*: filled in with the operation
		 * - *config-sent-op-value*: value filled in with the
		 *   operation
		 *
		 * Per query order field:
		 * - *config-ord-fname*: filled in with the fname
		 * - *config-ord-fname-value*: value filled in with the
		 *   fname
		 * - *config-ord-op*: filled in with the operation
		 * - *config-ord-op-value*: value filled in with the
		 *   operation
		 */
		fillSearchObj(e: HTMLElement, query: ortJson.searchObj,
			pos: number): void
		{
			this.fillComment(e, 'query', query.doc);
			if (query.name === null) {
				this.hidecl(e, 'config-query-name-has');
				this.showcl(e, 'config-query-name-none');
				this.replcl(e, 'config-query-name', '');
				this.attrcl(e, 'config-query-name-value', 
					'value', '');
				this.attrcl(e, 'config-query-name-id', 'id', 
					this.prefixes.strcts + 'strcts/' + 
					query.parent + '/queries/_q' + pos);
				this.attrcl(e, 'config-query-name-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + query.parent + 
					'/queries/_q' + pos);
			} else {
				this.hidecl(e, 'config-query-name-none');
				this.showcl(e, 'config-query-name-has');
				this.replcl(e, 'config-query-name', query.name);
				this.attrcl(e, 'config-query-name-value', 
					'value', query.name);
				this.attrcl(e, 'config-query-name-id', 'id', 
					this.prefixes.strcts + 'strcts/' +
					query.parent + '/queries/' + 
					query.name);
				this.attrcl(e, 'config-query-name-link', 'href', 
					'#' + this.prefixes.strcts + 'strcts/' +
					query.parent + '/queries/' + 
					query.name);
			}
			this.replcl(e, 'config-query-type', query.type);
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
					this.fillSents(list[i], query);
			}
			this.replcl(e, 'config-query-limit', query.limit);
			this.attrcl(e, 'config-query-limit-value', 
				'value', query.limit);
			this.replcl(e, 'config-query-offset', query.offset);
			this.attrcl(e, 'config-query-offset-value', 
				'value', query.offset);
			if (query.limit.toString() === '0' &&
			    query.offset.toString() === '0') {
				this.showcl(e, 'config-query-limoffs-none');
				this.hidecl(e, 'config-query-limoffs-has');
			} else {
				this.hidecl(e, 'config-query-limoffs-none');
				this.showcl(e, 'config-query-limoffs-has');
			}
			if (query.limit.toString() === '0') {
				this.showcl(e, 'config-query-limit-none');
				this.hidecl(e, 'config-query-limit-has');
			} else {
				this.hidecl(e, 'config-query-limit-none');
				this.showcl(e, 'config-query-limit-has');
			}
			if (query.offset.toString() === '0') {
				this.showcl(e, 'config-query-offset-none');
				this.hidecl(e, 'config-query-offset-has');
			} else {
				this.hidecl(e, 'config-query-offset-none');
				this.showcl(e, 'config-query-offset-has');
			}
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
					this.fillOrds(list[i], query);
			}
			if (query.dst !== null) {
				this.showcl(e, 'config-query-dst-has');
				this.hidecl(e, 'config-query-dst-none');
				this.replcl(e, 'config-query-dst-fname', 
					query.dst.fname);
			} else {
				this.hidecl(e, 'config-query-dst-has');
				this.showcl(e, 'config-query-dst-none');
			}
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
			this.fillRolemap(e, 'query', query.rolemap);
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
				this.replcl(cln, 'config-ord-fname', 
					query.ordq[i].fname);
				this.attrcl(cln, 'config-ord-fname-value', 
					'value', query.ordq[i].fname);
				this.replcl(cln, 'config-ord-op',
					query.ordq[i].op);
				this.attrcl(cln, 'config-ord-op-value',
					'value', query.ordq[i].op);
				e.appendChild(cln);
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
				this.replcl(cln, 'config-sent-fname', 
					query.sntq[i].fname);
				this.attrcl(cln, 'config-sent-fname-value', 
					'value', query.sntq[i].fname);
				this.replcl(cln, 'config-sent-op', 
					query.sntq[i].op);
				this.attrcl(cln, 'config-sent-op-value', 
					'value', query.sntq[i].op);
			}
		}

		/**
		 * For all fields in a structure:
		 *
		 * - *config-fields-name-link*: set *href* to 'fields' within
		 *   the *config-strct-name-link* prefix
		 * - *config-fields-name-id*: set *id* to 'fields' within the
		 *   *config-strct-name-id* prefix
		 * - *config-fields-{has,none}*: shown or hidden depending on
		 *   whether there are fields in the structure
		 * - *config-fields*: the first child of these is cloned and
		 *   filled in with fillFieldObj() for each field unless there
		 *   are no fields, in which case the elements are hidden
		 */
		fillFieldSet(e: HTMLElement, strct: ortJson.strctObj,
			exclude?: string[], cb?: ortJson.ortJsonConfigCallbacks,
			arg?: any): void
		{
			this.attrcl(e, 'config-fields-name-link', 'href', 
				'#' + this.prefixes.strcts + 'strcts/' + 
				strct.name + '/fields');
			this.attrcl(e, 'config-fields-name-id', 'id', 
				this.prefixes.strcts + 'strcts/' + 
				strct.name + '/fields');
			const keys: string[] = []
			for (const name in strct.fq)
				if (typeof(exclude) === 'undefined' ||
			  	    exclude.indexOf(name) === -1)
					keys.push(name);
			if (keys.length === 0) {
				this.hidecl(e, 'config-fields-has');
				this.showcl(e, 'config-fields-none');
				this.hidecl(e, 'config-fields');
				return;
			}
			keys.sort();
			this.showcl(e, 'config-fields-has');
			this.hidecl(e, 'config-fields-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-fields');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.clr(list[i]);
				for (let j: number = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					this.fillFieldObj(cln, 
						strct.fq[keys[j]], cb, arg);
					list[i].appendChild(cln);
				}
				this.show(list[i]);
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

		/**
		 * Per field:
		 *
		 * - *config-field-name*: filled in with name
		 * - *config-field-name-value*: value filled in with field name
		 * - *config-field-name-data*: *data-name* set with name
		 * - *config-field-name-id*: *id* set as the name within the
		 *   *config-fields-name-id* prefix
		 * - *config-field-name-link*: *href* set as the name within the
		 *   *config-fields-name-link* prefix
		 * - *config-field-fullname-data*: 'data-fullname' attribute set
		 *   as "parent.name"
		 * - *config-field-type-TYPE*: shown or hidden depending upon
		 *   the field type
		 * - *config-field-type*: filled in with the type name
		 * - *config-field-store-type*: shown or hidden depending upon
		 *   whether a non-struct type
		 * - *config-field-ref-{has,none}*: shown or hidden depending on
		 *   whether there's a ref
		 * - *config-field-fkey-{has,none}*: shown or hidden depending
		 *   on whether there's a non-struct ref
		 * - *config-field-ref-target-{strct,field}*: filled with the
		 *   target names if ref is defined
		 * - *config-field-ref-target-{strct,field}-link*: set *href* to
		 *   the *id* set by target's *config-field-name-id* or
		 *   *config-strct-name-id* if ref is defined
		 * - *config-field-ref-source-{strct,field}*: filled with the
		 *   source names if ref is defined
		 * - *config-field-ref-source-{strct,field}-link*: set *href* to
		 *   the *id* set by source's *config-field-name-id* or
		 *   *config-strct-name-id* if ref is defined
		 * - *config-field-bitf-{has,none}*: shown or hidden depending
		 *   on whether there's a bitf
		 * - *config-field-bitf*: filled in with the bitf name if bitf
		 *   is defined or empty string otherwise
		 * - *config-field-enm-{has,none}*: shown or hidden depending on
		 *   whether there's an enm
		 * - *config-field-enm*: filled in with the enm name if enm is
		 *   defined or empty string otherwise
		 * - *config-field-actdel*: filled in with actdel
		 * - *config-field-actup*: filled in with actup
		 * - *config-field-actup-TYPE*: shown or hidden depending upon
		 *   the actup type
		 * - *config-field-actdel-TYPE*: shown or hidden depending upon
		 *   the actdel type
		 * - *config-field-rolemap-{has,none}*: shown or hidden
		 *   depending on whether there's a non-empty rolemap
		 * - *config-field-rolemap-join*: filled in with the
		 *   comma-separated role names if the rolemap is non-empty
		 * - *config-field-rolemap*: first child of this is cloned and
		 *   filled in with the name if matching
		 *   *config-field-rolemap-role* and the value set if matching
		 *   *config-field-rolemap-role-value* unless the rolemap is
		 *   empty, in which case the element is hidden
		 * - *config-field-def-{has,none}*: shown or hidden depending on
		 *   whether def is defined
		 * - *config-field-def*: set to def, if defined
		 * - *config-field-def-value*: *value* set to def, if defined
		 * - *config-field-limits-{has,none}*: shown or hidden depending
		 *   on whether limits are defined
		 * - *config-field-limit-TYPE*: shown or hidden depending upon
		 *   the actual limit type, which may be one of *number*,
		 *   *real*, *string*, enum* if limits are defined
		 * - *config-field-limit*: filled in with comma-separated limits
		 *   type-value pairs if limits are defined
		 * - *config-field-flag-FLAG-set*: set as "checked" if the given
		 *   FLAG is set, otherwise the "checked" attribute is removed
		 * - *config-field-flags-{has,none}*: shown or hidden depending
		 *   on whether the field has flags
		 * - *config-field-flags-join*: filled in with comma-separated
		 *   flags or empty string if having none
		 *
		 * For documentation:
		 * - see fillComment()
		 */
		fillFieldObj(e: HTMLElement, field: ortJson.fieldObj,
			cb?: ortJson.ortJsonConfigCallbacks, arg?: any): void
		{
			const flags: fieldObjFlags[] = [
				'rowid', 'null', 'unique', 'noexport'
			];
			this.fillComment(e, 'field', field.doc);
			this.replcl(e, 'config-field-name', field.name);
			this.attrcl(e, 'config-field-name-value', 
				'value', field.name);
			this.attrcl(e, 'config-field-name-data', 
				'data-name', field.name);
			this.attrcl(e, 'config-field-fullname-data', 
				'data-fullname', field.parent + '.' + 
				field.name);
			this.attrcl(e, 'config-field-name-id', 'id', 
				this.prefixes.strcts + 'strcts/' + 
				field.parent + '/fields/' + field.name);
			this.attrcl(e, 'config-field-name-link', 'href', 
				'#' + this.prefixes.strcts + 'strcts/' +
				field.parent + '/fields/' + field.name);
			for (let i: number = 0; i < flags.length; i++)
				if (field.flags.indexOf(flags[i]) < 0)
					this.rattrcl(e, 'config-field-flag-' +
						flags[i] + '-set', 'checked');
				else
					this.attrcl(e, 'config-field-flag-' + 
						flags[i] + '-set', 
						'checked', 'checked');
			if (field.flags.length) {
				this.hidecl(e, 'config-field-flags-none');
				this.showcl(e, 'config-field-flags-has');
				this.replcl(e, 'config-field-flags-join', 
					field.flags.join(','));
			} else {
				this.showcl(e, 'config-field-flags-none');
				this.hidecl(e, 'config-field-flags-has');
				this.replcl(e, 'config-field-flags-join', '');
			}
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
			this.fillFieldAction(e, field.actup, 'actup');
			this.fillFieldAction(e, field.actdel, 'actdel');
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
				this.attrcl(e, 
					'config-field-ref-target-strct-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + field.ref.target.strct);
				this.replcl(e, 'config-field-ref-target-field', 
					field.ref.target.field);
				this.attrcl(e, 
					'config-field-ref-target-field-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + field.ref.target.strct + 
					'/fields/' + field.ref.target.field);
				this.replcl(e, 'config-field-ref-source-strct', 
					field.ref.source.strct);
				this.attrcl(e, 
					'config-field-ref-source-strct-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + field.ref.source.strct);
				this.replcl(e, 'config-field-ref-source-field', 
					field.ref.source.field);
				this.attrcl(e, 
					'config-field-ref-source-field-link', 
					'href', '#' + this.prefixes.strcts + 
					'strcts/' + field.ref.source.strct + 
					'/fields/' + field.ref.source.field);
			}
			if (typeof field.bitf === 'undefined') {
				this.showcl(e, 'config-field-bitf-none');
				this.hidecl(e, 'config-field-bitf-has');
				this.replcl(e, 'config-field-bitf', '');
			} else {
				this.hidecl(e, 'config-field-bitf-none');
				this.showcl(e, 'config-field-bitf-has');
				this.replcl(e, 'config-field-bitf', field.bitf);
				this.attrcl(e, 'config-field-bitf-link', 'href',
					'#' + this.prefixes.bitfs + 'bitfs/' + 
					field.bitf);
			}
			if (typeof field.enm === 'undefined') {
				this.showcl(e, 'config-field-enm-none');
				this.hidecl(e, 'config-field-enm-has');
				this.replcl(e, 'config-field-enm', '');
			} else {
				this.hidecl(e, 'config-field-enm-none');
				this.showcl(e, 'config-field-enm-has');
				this.replcl(e, 'config-field-enm', field.enm);
				this.attrcl(e, 'config-field-enm-link', 'href',
					'#' + this.prefixes.enums + 'enums/' + 
					field.enm);
			}
			this.fillRolemap(e, 'field', field.rolemap);
			if (field.def === null) {
				this.showcl(e, 'config-field-def-none');
				this.hidecl(e, 'config-field-def-has');
			} else { 
				this.hidecl(e, 'config-field-def-none');
				this.showcl(e, 'config-field-def-has');
				this.replcl(e, 'config-field-def', field.def);
				this.attrcl(e, 'config-field-def-value', 
					'value', field.def);
			}
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

			if (typeof cb !== 'undefined' &&
			    typeof cb.field !== 'undefined')
				cb.field(e, field, arg);
		}
	}
}
