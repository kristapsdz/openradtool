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
		parent: string;
		name: string;
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
		name: string;
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
		parent: string;
		name: string;
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
		name: string;
		pos: posObj;
		doc: string|null;
		labelsNull: labelSet|null;
		labelsUnset: labelSet|null;
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
	 * Manage serialising an ortJsonConfig object into an HTML DOM
	 * tree.
	 */
	export class ortJsonConfigFormat {
		private readonly obj: ortJson.configObj;

		constructor(obj: ortJson.ortJsonConfig) {
			this.obj = obj.config;
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
			str += this.commentToString(bit.doc);
			if (bit.labels !== null)
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
				str += this.bitfObjToString(set[keys[i]]);
			return str;
		}

		private enumItemObjToString(name: string, eitem: enumItemObj): string
		{
			let str: string = ' item ' + name;
			if (eitem.value !== null)
				str += ' ' + eitem.value.toString();
			str += this.commentToString(eitem.doc);
			if (eitem.labels !== null)
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
		       		str += this.updateObjToString
					(null, strct.uq.anon[i]);	
			str += this.updateSetToString(strct.uq.named);

			for (let i: number = 0; i < strct.dq.anon.length; i++) 
		       		str += this.updateObjToString
					(null, strct.dq.anon[i]);	
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

		private rattrcl(root: string|HTMLElement, 
			name: string, attr: string): void 
		{
			const list: HTMLElement[] = 
				this.list(root, name);
			for (let i: number = 0; i < list.length; i++)
				list[i].removeAttribute(attr);
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
		 * - fillStrctSet()
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
			this.fillStrctSet(pn);
			this.fillEnumSet(pn);
			this.fillBitfSet(pn);
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
		 * Per all bitfields specified in a configuration:
		 *
		 * - *config-bitfs-{has,none}*: shown or hidden
		 *   depending on whether there are bitfields
		 * - *config-bitfs*: the first child of these is cloned
		 *   and filled in with fillBitfObj() for each bitfield
		 *   unless there are no bitfields, in which case the
		 *   elements are hidden
		 */
		fillBitfSet(e: HTMLElement): void
		{
			const keys: string[] = Object.keys(this.obj.bq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-bitfs-has');
				this.showcl(e, 'config-bitfs-none');
				return;
			}
			this.showcl(e, 'config-bitfs-has');
			this.hidecl(e, 'config-bitfs-none');
			const list: HTMLElement[] = 
				this.list(e, 'config-bitfs');
			for (let i: number = 0; i < list.length; i++) {
				if (list[i].children.length === 0)
					continue;
				const tmpl: HTMLElement =
					<HTMLElement>list[i].children[0];
				this.show(list[i]);
				this.clr(list[i]);
				for (const name in this.obj.bq) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.fillBitfObj(cln, this.obj.bq[name]);
				}
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * Per all enumerations specified in a configuration:
		 *
		 * - *config-enums-{has,none}*: shown or hidden
		 *   depending on whether there are enumerations
		 * - *config-enums*: the first child of these is cloned
		 *   and filled in with fillEnumObj() for each enumeration
		 *   unless there are no enumerations, in which case the
		 *   elements are hidden
		 */
		fillEnumSet(e: HTMLElement): void
		{
			const keys: string[] = Object.keys(this.obj.eq);
			if (keys.length === 0) {
				this.hidecl(e, 'config-enums-has');
				this.showcl(e, 'config-enums-none');
				return;
			}
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
				for (const name in this.obj.eq) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.fillEnumObj(cln, this.obj.eq[name]);
				}
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * For a bitfield:
		 *
		 * - *config-bitf-doc*: filled in with bitfield
		 *   documentation
		 * - *config-bitf-name*: filled in with the bitfield
		 *   name
		 * - *config-bitf-name-value*: value set to the bitfield
		 *   name
		 */
		fillBitfObj(e: HTMLElement, bitf: ortJson.bitfObj): void
		{
			this.fillComment(e, 'bitf', bitf.doc);
			this.replcl(e, 'config-bitf-name', bitf.name);
			this.attrcl(e, 'config-bitf-name-value', 
				'value', bitf.name);
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * For an enumeration:
		 *
		 * - *config-enum-doc*: filled in with enum
		 *   documentation
		 * - *config-enum-name*: filled in with the enum name
		 * - *config-enum-name-value*: value set to the enum
		 *   name
		 */
		fillEnumObj(e: HTMLElement, enm: ortJson.enumObj): void
		{
			this.fillComment(e, 'enum', enm.doc);
			this.replcl(e, 'config-enum-name', enm.name);
			this.attrcl(e, 'config-enum-name-value', 
				'value', enm.name);
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * For an enumeration item:
		 *
		 * - *config-enumitem-doc*: filled in with bitfield item
		 *   documentation
		 * - *config-enumitem-name*: filled in with the bitfield
		 *   item name
		 * - *config-enumitem-name-value*: value set to the
		 *   bitfield item name
		 * - *config-eitem-value-{has,none}*: shown or hidden
		 *   depending on whether there's a value for the
		 *   enumerateion item
		 * - *config-enumitem-value*: filled in with the
		 *   bitfield item name, if applicable, or an empty
		 *   string of unset
		 * - *config-enumitem-value-value*: value set to the
		 *   bitfield item value, if applicable, or an empty
		 *   string if unset
		 */
		fillEnumItemObj(e: HTMLElement, eitem: ortJson.enumItemObj): void
		{
			this.fillComment(e, 'enumitem', eitem.doc);
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
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * For a bitfield item:
		 *
		 * - *config-bitindex-doc*: filled in with bitfield item
		 *   documentation
		 * - *config-bitindex-name*: filled in with the bitfield
		 *   item name
		 * - *config-bitindex-name-value*: value set to the
		 *   bitfield item name
		 * - *config-bitindex-value*: filled in with the bitfield
		 *   item name
		 * - *config-bitindex-value-value*: value set to the
		 *   bitfield item value
		 */
		fillBitIndexObj(e: HTMLElement, biti: ortJson.bitIndexObj): void
		{
			this.fillComment(e, 'bitindex', biti.doc);
			this.replcl(e, 'config-bitindex-name', biti.name);
			this.replcl(e, 'config-bitindex-value', 
				biti.value.toString());
			this.attrcl(e, 'config-bitindex-name-value', 
				'value', biti.name);
			this.attrcl(e, 'config-bitindex-value-value', 
				'value', biti.value.toString());
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
		 *   and filled in with fillRole() for each role unless
		 *   there are no roles, in which case the elements are
		 *   hidden
		 *
		 * Accepts an optional array of role names not to show.
		 */
		fillRoles(e: HTMLElement, exclude?: string[]): void
		{
			const newrq: roleObj[] = [];
			if (this.obj.rq !== null)
				for (const name in this.obj.rq)
					if (typeof(exclude) === 'undefined' ||
					    exclude.indexOf(name) === -1)
						newrq.push(this.obj.rq[name]);

			if (newrq.length === 0) {
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
				for (let j: number = 0; j < newrq.length; j++) {
					const cln: HTMLElement = 
						<HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.fillRole(cln, newrq[j]);
				}
			}
		}

		/**
		 * See fill() for generic documentation.
		 *
		 * For each role:
		 *
		 * - *config-role-doc*: filled in with role
		 *   documentation
		 * - *config-role-name*: filled in with the role name
		 * - *config-role-name-value*: value set to the role
		 *   name
		 * - *config-role-parent-{has,none}*: shown or hidden
		 *   depending upon whether the parent is set
		 * - *config-role-parent*: filled in with the parent
		 *   name or an empty string if the parent is not set
		 */
		fillRole(e: HTMLElement, role: ortJson.roleObj): void
		{
			this.fillComment(e, 'role', role.doc);
			this.replcl(e, 'config-role-name', role.name);
			if (role.parent === null) {
				this.hidecl(e, 'config-role-parent-has');
				this.showcl(e, 'config-role-parent-none');
				this.replcl(e, 'config-role-parent', '');
			} else {
				this.showcl(e, 'config-role-parent-has');
				this.hidecl(e, 'config-role-parent-none');
				this.replcl(e, 'config-role-parent', role.parent);
			}
			this.attrcl(e, 'config-role-name-value', 
				'value', role.name);
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
		 *   fillStrctObj()) unless there are no strcts, in which
		 *   case the elements are hidden
		 */
		fillStrctSet(e: HTMLElement)
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
					this.fillStrctObj(cln, this.obj.sq[keys[j]]);
				}
				this.show(list[i]);
			}
		}

		/**
		 * See fill() for generic documentation.  See fillFieldSet() for
		 * what's run over for fields, fillSearchClassObj() for queries.
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
		 *   comma-separated role names if the rolemap is
		 *   non-empty
		 * - *config-insert-rolemap*: first child of this is
		 *   cloned and filled in with the name if matching
		 *   *config-insert-rolemap-role* and the value set if
		 *   matching *config-insert-rolemap-role-value* unless
		 *   the rolemap is empty, in which case the element is
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
		 */
		fillStrctObj(e: HTMLElement, strct: ortJson.strctObj): void
		{
			let list: HTMLElement[];

			/* fq (fields) */

			this.fillFieldSet(e, strct);

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
					this.fillSearchClassObj
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
				this.fillRolemap(e, 'insert', null);
			}

			/* doc */

			this.fillComment(e, 'strct', strct.doc);

			/* name */

			this.replcl(e, 'config-strct-name', strct.name);
			this.attrcl(e, 'config-strct-name-value', 'value', strct.name);
		}

		private fillRolemap(e: HTMLElement, 
			name: string, map: string[]|null): void
		{
			const cls: string = 
				'config-' + name + '-rolemap';

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

			if (up.rolemap.length === 0) {
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

		/**
		 * Runs fillSearchObj() for each named or anonymous query.
		 */
		private fillSearchClassObj(e: HTMLElement, 
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
				this.fillSearchObj(cln, strct.sq.named[keys[i]]);
			}
			for (let i = 0; i < strct.sq.anon.length; i++) {
				const cln: HTMLElement = <HTMLElement>
					tmpl.cloneNode(true);
				e.appendChild(cln);
				this.fillSearchObj(cln, strct.sq.anon[i]);
			}
		}

		/**
		 * Per query:
		 *
		 * - *config-query-name-{has,none}*: shown or hidden
		 *   depending on whether the query is anonymous
		 * - *config-query-name*: filled in with the query name
		 *   if a name is defined, empty string otherwise
		 * - *config-query-name-value*: value filled in with the
		 *   query name if a name is defined, empty string 
		 *   otherwise
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
		 * - *config-query-limit-{has,none}*: whether a non-zero
		 *   limit is defined
		 * - *config-query-offset-{has,none}*: whether a non-zero
		 *   offset is defined
		 * - *config-query-offset*: filled in with the offset
		 * - *config-query-offset-value*: value filled in with
		 *   the offset
		 * - *config-query-limit*: filled in with the limit
		 * - *config-query-limit-value*: value filled in with
		 *   the limit
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
		fillSearchObj(e: HTMLElement, query: ortJson.searchObj): void
		{
			/* doc */

			this.fillComment(e, 'query', query.doc);

			/* name */

			if (query.name === null) {
				this.hidecl(e, 'config-query-name-has');
				this.showcl(e, 'config-query-name-none');
				this.replcl(e, 'config-query-name', '');
				this.attrcl(e, 'config-query-name-value', 
					'value', '');
			} else {
				this.hidecl(e, 'config-query-name-none');
				this.showcl(e, 'config-query-name-has');
				this.replcl(e, 'config-query-name', query.name);
				this.attrcl(e, 'config-query-name-value', 
					'value', query.name);
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

			/* limit/offset */
			
			this.replcl(e, 'config-query-limit', query.limit);
			this.attrcl(e, 'config-query-limit-value', 
				'value', query.limit);
			this.replcl(e, 'config-query-offset', query.offset);
			this.attrcl(e, 'config-query-offset-value', 
				'value', query.offset);

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

			if (query.rolemap.length === 0) {
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
				this.replcl(cln, 'config-ord-fname', 
					query.ordq[i].fname);
				this.attrcl(cln, 'config-ord-fname-value', 
					'value', query.ordq[i].fname);
				this.replcl(cln, 'config-ord-op',
					query.ordq[i].op);
				this.attrcl(cln, 'config-ord-op-value',
					'value', query.ordq[i].op);
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
		 * - *config-fields-{has,none}*: shown or hidden depending on
		 *   whether there are fields in the structure
		 * - *config-fields*: the first child of these is cloned and
		 *   filled in with fillFieldObj() for each field unless there
		 *   are no fields, in which case the elements are hidden
		 */
		fillFieldSet(e: HTMLElement, strct: ortJson.strctObj,
			exclude?: string[]): void
		{
			const keys: string[] = []
			for (const name in strct.fq)
				if (typeof(exclude) === 'undefined' ||
			  	    exclude.indexOf(name) === -1)
					keys.push(name);
			if (keys.length === 0) {
				this.hidecl(e, 'config-fields-has');
				this.showcl(e, 'config-fields-none');
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
				this.show(list[i]);
				this.clr(list[i]);
				for (let j: number = 0; j < keys.length; j++) {
					const cln: HTMLElement = <HTMLElement>
						tmpl.cloneNode(true);
					list[i].appendChild(cln);
					this.fillFieldObj
						(cln, strct.fq[keys[j]]);
				}
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
		 * - *config-field-name*: filled in with name
		 * - *config-field-name-value*: value filled in with the
		 *   field name
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
		 * - *config-field-rolemap-join*: filled in with the
		 *   comma-separated role names if the rolemap is
		 *   non-empty
		 * - *config-field-rolemap*: first child of this is
		 *   cloned and filled in with the name if matching
		 *   *config-field-rolemap-role* and the value set if
		 *   matching *config-field-rolemap-role-value* unless
		 *   the rolemap is empty, in which case the element is
		 *   hidden
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
		 * - *config-field-flag-FLAG-set*: set as "checked" if
		 *   the given FLAG is set, otherwise the "checked"
		 *   attribute is removed
		 * - *config-field-flags-{has,none}*: shown or hidden
		 *   depending on whether the field has flags
		 * - *config-field-flags-join*: filled in with
		 *   comma-separated flags or empty string if having
		 *   none
		 */
		fillFieldObj(e: HTMLElement, field: ortJson.fieldObj): void
		{
			const flags: fieldObjFlags[] = [
				'rowid', 'null', 'unique', 'noexport'
			];

			/* doc */

			this.fillComment(e, 'field', field.doc);

			/* name */

			this.replcl(e, 'config-field-name', field.name);
			this.attrcl(e, 'config-field-name-value', 
				'value', field.name);

			/* flags */

			for (let i: number = 0; i < flags.length; i++)
				if (field.flags.indexOf(flags[i]) < 0)
					this.rattrcl(e, 'config-field-flag-' +
						flags[i] + '-set', 
						'checked');
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

			this.fillRolemap(e, 'field', field.rolemap);

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
