	/**
	 * Facilities to manipulate 64-bit numbers encoded as strings.
	 * The JavaScript implementation of openradtool uses strings for
	 * numbers because of the 64-bit problem: internally,
	 * openradtool uses 64-bit integer numbers, but JavaScript has
	 * only 53 bits of precision for integers.
	 * This class is a modified version of long.js fixed to base 10.
	 */
	export class Long {
		private readonly __isLong__: boolean = true;
		private readonly low: number;
		private readonly high: number;
		private readonly unsigned: boolean;

		private constructor(low: number,
			high?: number, unsigned?: boolean)
		{
			this.low = low | 0;
			this.high = 
				(typeof high === 'undefined') ?
				0 : (high | 0);
			this.unsigned = 
				(typeof unsigned === 'undefined') ?
				false: unsigned;
		}

		static readonly ZERO: Long = 
			new Long(0, 0);
		static readonly ONE: Long = 
			new Long(1, 0);
		static readonly UZERO: Long = 
			new Long(0, 0, true);
		static readonly TEN_TO_EIGHT: Long =
			new Long(100000000, 0);
		static readonly MIN_VALUE: Long = 
			new Long(0, 0x80000000|0, false);
		static readonly MAX_UNSIGNED_VALUE: Long = 
			new Long(0xFFFFFFFF|0, 0xFFFFFFFF|0, true);
		static readonly  MAX_VALUE: Long = 
			new Long(0xFFFFFFFF|0, 0x7FFFFFFF|0, false);

		/* Constant numbers used in the code. */

		private static readonly TWO_PWR_16_DBL: number = 
			(1 << 16);
		private static readonly TWO_PWR_32_DBL: number = 
			Long.TWO_PWR_16_DBL *
			Long.TWO_PWR_16_DBL;
		private static readonly TWO_PWR_64_DBL: number =
			Long.TWO_PWR_32_DBL *
			Long.TWO_PWR_32_DBL;
		private static readonly TWO_PWR_63_DBL: number = 
			Long.TWO_PWR_64_DBL / 2;

		/**
		 * @return Whether this is a ortns.Long object.
		 */
		static isLong(obj: any): boolean 
		{
			/* 
			 * Complex to silence a typescript warning about
			 * __isLong__ being an unused local otherwise.
			 */

			return typeof obj === 'object' &&
		 	       obj !== null && 
			       '__isLong__' in obj &&
			       (<Long>obj).__isLong__ === true;
		}

		/**
		 * Convert to a JavaScript number, losing precision and
		 * possibly truncating with larger or smaller numbers.
		 * @return The Long best-effort converted to a number.
		 */
		toNumber(): number
		{
			if (this.unsigned)
				return ((this.high >>> 0) * 
					Long.TWO_PWR_32_DBL) + 
				       (this.low >>> 0);
			return this.high * Long.TWO_PWR_32_DBL + 
				(this.low >>> 0);
		};

		/**
		 * Assume this is a 32-bit integer and return that.
		 */
		toInt(): number
		{
			return this.unsigned ? this.low >>> 0 : this.low;
		}

		/**
		 * @return Whether the value is strictly < 0.  This only applies
		 * to signed numbers, else this is always false.
		 */
		isNegative(): boolean
		{
			return !this.unsigned && this.high < 0;
		}

		/**
		 * @return Whether the value is zero, regardless of sign.
		 */
		isZero(): boolean
		{
			return this.high === 0 && this.low === 0;
		}

		/**
		 * @return Whether the value is an odd number.
		 */
		isOdd(): boolean
		{
			return (this.low & 1) === 1;
		}

		/**
		 * @return Whether the value equals the given value.  This is
		 * according to the numerical value, not the bit pattern, so a
		 * negative signed value and a positive signed value will be
		 * the same even if having the same bit pattern.  The exception
		 * here is zero, which is the same whether signed or not.
		 */
		eq(other: Long|number): boolean
		{
			const v: Long = !Long.isLong(other) ?
				new Long(<number>other) : <Long>other;

			if (this.unsigned !== v.unsigned && 
			    (this.high >>> 31) === 1 && 
				    (v.high >>> 31) === 1)
				return false;
			return this.high === v.high && 
				this.low === v.low;
		}

		/**
		 * @return The negative of the value.
		 */
		neg(): Long
		{
			if (!this.unsigned && this.eq(Long.MIN_VALUE))
				return Long.MIN_VALUE;
			return this.not().add(Long.ONE);
		}

		/**
		 * @return The bit-wise NOT of the value.
		 */
		not(): Long
		{
			return new Long(~this.low, 
				~this.high, this.unsigned);
		}

		/**
		 * @return The bit-wise AND of the value with the given value.
		 * Sign is inherited from the source value.  If the
		 * parameterised value can't be parsed, returns zero.
		 */
		and(other: Long|number|string|null): Long
		{
			const v: Long|null = !Long.isLong(other) ?
				Long.fromValue(other) : <Long>other;
			if (v === null)
				return Long.ZERO;
			return new Long(this.low & v.low, 
					this.high & v.high, 
					this.unsigned);
		}

		/**
		 * @return The bit-wise OR of the value with the given value.
		 * Sign is inherited from the source value.  If the
		 * parameterised value can't be parsed, returns zero.
		 */
		or(other: Long|number|string|null): Long 
		{
			    if (!Long.isLong(other))
			            other = Long.fromValue(other);
			    if (other === null)
				    return Long.ZERO;
			    return new Long(this.low | other.low, 
					this.high | other.high, 
					this.unsigned);
		}

		/**
		 * @return The value left-shifted by the given number of bits.
		 * Sign is inherited from the source value.
		 */
		shl(numBits: Long|number): Long
		{
			let v: number = Long.isLong(numBits) ?
				(<Long>numBits).toInt() : 
				<number>numBits;

			if ((v &= 63) === 0)
				return this;
			else if (v < 32)
				return new Long
					(this.low << v, 
					 (this.high << v) | 
					 (this.low >>> (32 - v)), 
					 this.unsigned);
			else
				return new Long
					(0, this.low << (v - 32), 
					 this.unsigned);
		}

		/**
		 * @return The value multiplied by the given value.
		 */
		mul(tomul: Long|number): Long
		{
			const v: Long = !Long.isLong(tomul) ?
				Long.fromNumber(<number>tomul) : 
				<Long>tomul;

			if (this.isZero() || v.isZero())
				return Long.ZERO;
			if (this.eq(Long.MIN_VALUE))
				return v.isOdd() ? Long.MIN_VALUE :
				Long.ZERO;
			if (v.eq(Long.MIN_VALUE))
				return this.isOdd() ? Long.MIN_VALUE :
				Long.ZERO;

			if (this.isNegative()) {
				if (v.isNegative())
					return this.neg().mul(v.neg());
				else
					return this.neg().mul(v).neg();
			} else if (v.isNegative())
				return this.mul(v.neg()).neg();

			// Divide each long into 4 chunks of 16 bits,
			// and then add up 4x4 products.  We can skip
			// products that would overflow.

			const a48: number = this.high >>> 16;
			const a32: number = this.high & 0xFFFF;
			const a16: number = this.low >>> 16;
			const a00: number = this.low & 0xFFFF;

			const b48: number = v.high >>> 16;
			const b32: number = v.high & 0xFFFF;
			const b16: number = v.low >>> 16;
			const b00: number = v.low & 0xFFFF;

			let c48: number = 0;
			let c32: number = 0;
			let c16: number = 0;
			let c00: number = 0;

			c00 += a00 * b00;
			c16 += c00 >>> 16;
			c00 &= 0xFFFF;
			c16 += a16 * b00;
			c32 += c16 >>> 16;
			c16 &= 0xFFFF;
			c16 += a00 * b16;
			c32 += c16 >>> 16;
			c16 &= 0xFFFF;
			c32 += a32 * b00;
			c48 += c32 >>> 16;
			c32 &= 0xFFFF;
			c32 += a16 * b16;
			c48 += c32 >>> 16;
			c32 &= 0xFFFF;
			c32 += a00 * b32;
			c48 += c32 >>> 16;
			c32 &= 0xFFFF;
			c48 += a48 * b00 + a32 * b16 + a16 * b32 + a00 * b48;
			c48 &= 0xFFFF;

			return new Long
				((c16 << 16) | c00, (c48 << 16) | c32,
				 this.unsigned);
		}

		/**
		 * @return The sum of the value and the given value.
		 */
		add(toadd: Long|number): Long
		{
			const v: Long = !Long.isLong(toadd) ?
				Long.fromNumber(<number>toadd) : 
				<Long>toadd;

			// Divide each number into 4 chunks of 16 bits,
			// and then sum the chunks.

			const a48: number = this.high >>> 16;
			const a32: number = this.high & 0xFFFF;
			const a16: number = this.low >>> 16;
			const a00: number = this.low & 0xFFFF;

			const b48: number = v.high >>> 16;
			const b32: number = v.high & 0xFFFF;
			const b16: number = v.low >>> 16;
			const b00: number = v.low & 0xFFFF;

			let c48: number = 0;
			let c32: number = 0;
			let c16: number = 0;
			let c00: number = 0;

			c00 += a00 + b00;
			c16 += c00 >>> 16;
			c00 &= 0xFFFF;
			c16 += a16 + b16;
			c32 += c16 >>> 16;
			c16 &= 0xFFFF;
			c32 += a32 + b32;
			c48 += c32 >>> 16;
			c32 &= 0xFFFF;
			c48 += a48 + b48;
			c48 &= 0xFFFF;

			return new Long
				((c16 << 16) | c00, (c48 << 16) | c32,
				 this.unsigned);
		}

		/**
		 * Attempt to convert from a Long, number, or string.
		 * @return The Long representation of the value, or null on
		 * conversion failure including undefined-ness.
		 */
		static fromValue(val: any, unsigned?: boolean): Long|null
		{
			if (typeof val === 'undefined')
				return null;
			if (typeof val === 'number')
				return Long.fromNumber
					(<number>val, unsigned);
			if (typeof val === 'string')
				return Long.fromString
					(<string>val, unsigned);
			if (Long.isLong(val))
				return <Long>val;
			return null;
		}

		/**
		 * Attempt to convert from a Long, number, or string.
		 * @return The Long representation of the value, null
		 * on conversion failure, or undefined it it was passed
		 * undefined.
		 */
		static fromValueUndef(val: any, unsigned?: boolean): Long|null|undefined
		{
			if (typeof val === 'undefined')
				return undefined;
			return Long.fromValue(val);
		}

		/**
		 * @return The Long representation of the value.  If
		 * NaN, returns zero or unsigned zero as applicable.
		 * If negative and wanting unsigned, bound at zero.
		 * Otherwise bound to a 64-bit integer size.
		 */
		static fromNumber(value: number, unsigned?: boolean): Long
		{
			const usgn: boolean =
				(typeof unsigned === 'undefined') ?
				false : unsigned;

			if (isNaN(value))
				return usgn ? Long.UZERO : Long.ZERO;

			if (usgn) {
				if (value < 0)
					return Long.UZERO;
				if (value >= Long.TWO_PWR_64_DBL)
					return Long.MAX_UNSIGNED_VALUE;
			} else {
				if (value <= -Long.TWO_PWR_63_DBL)
					return Long.MIN_VALUE;
				if (value + 1 >= Long.TWO_PWR_63_DBL)
					return Long.MAX_VALUE;
			}

			if (value < 0)
				return Long.fromNumber
					(-value, unsigned).neg();

			return new Long
				((value % Long.TWO_PWR_32_DBL) | 0, 
				 (value / Long.TWO_PWR_32_DBL) | 0, 
				 unsigned);
		}

		/**
		 * @return The Long representation of the string or zero
		 * if conversion failed.  This should only be used for
		 * constant-value transformation that is *guaranteed*
		 * will not fail.
		 */
		static fromStringZero(str: string): Long
		{
			const val: Long|null = Long.fromString(str);
			return val === null ? Long.ZERO : val;
		}

		/**
		 * @return The Long representation of the string, which
		 * may be an optional leading "-" followed by digits.
		 * It does not accept NaN, Infinity, or other symbols.
		 */
		static fromString(str: string, unsigned?: boolean): Long | null
		{
			const usgn: boolean =
				(typeof unsigned === 'undefined') ?
				false : unsigned;
			const hyph: number = str.indexOf('-');
			const radixToPower: Long = Long.TEN_TO_EIGHT;
			let result: Long = Long.ZERO;

			if (str.length === 0 || hyph > 0 ||
			    str === 'NaN' || str === 'Infinity' || 
			    str === '+Infinity' || str === '-Infinity')
				return null;

			if (hyph === 0) {
				const nresult: Long|null = 
					Long.fromString
					(str.substring(1), usgn);
				if (nresult === null)
					return null;
				return nresult.neg();
			}

			if (!str.match(/^[0-9]+$/))
				return null;

			// Do several (8) digits each time through the
			// loop, so as to  minimize the calls to the
			// very expensive emulated div.

			for (let i: number = 0; i < str.length; i += 8) {
				const size: number = 
					Math.min(8, str.length - i);
				const value: number = 
					parseInt(str.substring(i, i + size), 10);
				if (isNaN(value))
					return null;
				if (size < 8) {
					const power: Long = 
						Long.fromNumber
						(Math.pow(10, size));
					result = result.mul(power).add
						(Long.fromNumber(value));
				} else {
					result = result.mul
						(radixToPower);
					result = result.add
						(Long.fromNumber(value));
				}
			}
			return new Long(result.low, result.high, usgn);
		}
	}

	/**
	 * Labels ("jslabel" in ort(5)) may have multiple languages.
	 * This maps a language name to a translated string.
	 */
	interface langmap { [lang: string]: string };

	/**
	 * Resolve a set of translated strings into a single one
	 * depending upon the current language.
	 * @param vals All translations of a given word.
	 * @return The word in the current language (or the default)
	 * or an empty string on failure.
	 * @internal
	 */
	function _strlang(vals: langmap): string
	{
		const lang: string|null = 
			document.documentElement.lang;

		if (lang !== null && lang in vals)
			return vals[lang];
		else if ('_default' in vals)
			return vals['_default'];
		else
			return '';
	}

	/**
	 * Language replacement conditional upon the label (**jslabel**
	 * in the configuration).  Like {@link _replcl} with inclusion
	 * set to false.
	 * @param e The root of the DOM tree in which we query for 
	 * elements to fill into.
	 * @param name The class name we search for within the root (not 
	 * inclusive).
	 * @param vals All possible translations.
	 * @internal
	 */
	function _replcllang(e: HTMLElement, name:string,
		vals: langmap): void
	{
		_replcl(e, name, _strlang(vals), false);
	}

	/**
	 * Language replacement conditional upon the label (**jslabel**
	 * in the configuration). Like {@link _repl}.
	 * @param e The root of the DOM tree in which we query for 
	 * elements to fill into.
	 * @param vals All possible translations.
	 * @internal
	 */
	function _repllang(e: HTMLElement, vals: langmap): void
	{
		_repl(e, _strlang(vals));
	}

	/**
	 * Set the attribute of an element.
	 * @param e The element whose attribute to set.
	 * @param attr The attribute name.
	 * @param text The attribute value.
	 * @internal
	 */
	function _attr(e: HTMLElement, attr: string, text: string): void
	{
		e.setAttribute(attr, text);
	}

	/**
	 * Remove the attribute of an element.
	 * @param e The element whose attribute to set.
	 * @param attr The attribute name.
	 * @param text The attribute value.
	 * @internal
	 */
	function _rattr(e: HTMLElement, attr: string): void
	{
		e.removeAttribute(attr);
	}

	/**
	 * Set attributes for all elements matching a class.
	 * @internal
	 */
	function _attrcl(e: HTMLElement, attr: string,
		name: string, text: string, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] =
			_elemList(e, name, inc);

		for (i = 0; i < list.length; i++)
			_attr(list[i], attr, text);
	}

	/**
	 * Get all elements beneath (possibly including) a root matching
	 * the given class.
	 * @internal
	 */
	function _elemList(e: HTMLElement|null,
		cls: string, inc: boolean): HTMLElement[]
	{
		let list: HTMLCollectionOf<Element>;
		let i: number;
		const a: HTMLElement[] = [];

		if (e === null)
			return a;
		list = e.getElementsByClassName(cls);
		for (i = 0; i < list.length; i++)
			a.push(<HTMLElement>list[i]);
		if (inc && e.classList.contains(cls))
			a.push(e);
		return a;
	}

	/**
	 * Replace all children of an element with text.
	 * @internal
	 */
	function _repl(e: HTMLElement, text: string): void
	{
		while (e.firstChild)
			e.removeChild(e.firstChild);
		e.appendChild(document.createTextNode(text));
	}

	/**
	 * Replace children of elements matching class with text.
	 * @internal
	 */
	function _replcl(e: HTMLElement, name: string,
		text: string, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] = _elemList(e, name, inc);

		for (i = 0; i < list.length; i++)
			_repl(list[i], text);
	}

	/**
	 * Add class to class list if it doesn't exist.
	 * @internal
	 */
	function _classadd(e: HTMLElement, name: string): HTMLElement
	{
		if (!e.classList.contains(name))
			e.classList.add(name);
		return(e);
	}

	/**
	 * Add class if doesn't exist to all elements with class name.
	 * @internal
	 */
	function _classaddcl(e: HTMLElement, name: string,
		cls: string, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] = _elemList(e, name, inc);

		for (i = 0; i < list.length; i++)
			_classadd(list[i], cls);
	}

	/**
	 * "Hide" element by adding *hide* class.
	 * @internal
	 */
	function _hide(e: HTMLElement): HTMLElement
	{
		if (!e.classList.contains('hide'))
			e.classList.add('hide');
		return e;
	}

	/**
	 * "Hide" all elements matching class by adding *hide* class.
	 * @internal
	 */
	function _hidecl(e: HTMLElement, name: string, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] = _elemList(e, name, inc);

		for (i = 0; i < list.length; i++)
			_hide(list[i]);
	}

	/**
	 * "Show" element by removing *hide* class.
	 * @internal
	 */
	function _show(e: HTMLElement): HTMLElement
	{
		if (e.classList.contains('hide'))
			e.classList.remove('hide');
		return e;
	}

	/**
	 * "Show" all elements matching class by removing *hide* class.
	 * @internal
	 */
	function _showcl(e: HTMLElement, name: string, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] = _elemList(e, name, inc);

		for (i = 0; i < list.length; i++)
			_show(list[i]);
	}

	/**
	 * Check input elements (that is, set the attribute `checked` to
	 * the value `checked`) for all elements of class
	 * `fname-value-checked` whose value matches the given.  The
	 * checked status is removed for each item scanned.
	 * A null value never matches.
	 * @param e Root of tree scanned for elements.
	 * @param fname Structure name, '-', field name.
	 * @param val The value to test for.
	 * @param inc Include root in scanning for elements.
	 * @internal
	 */
	function _fillValueChecked(e: HTMLElement, fname: string,
		val: number|string|null, inc: boolean): void
	{
		let i: number;
		const list: HTMLElement[] = _elemList
			(e, fname + '-value-checked', inc);

		for (i = 0; i < list.length; i++) {
			const attrval: string|null = 
				(<HTMLInputElement>list[i]).value;
			_rattr(list[i], 'checked');
			if (val === null || attrval === null)
				continue;
			if (val.toString() === attrval)
				_attr(list[i], 'checked', 'checked');
		}
	}

	/**
	 * Take all `<option>` elements under the root (non-inclusive)
	 * and sets or unsets the `selected` attribute depending upon
	 * whether it matches the object's value.
	 * A null value never matches.
	 * @param e Root of tree scanned for elements.
	 * @param val The value to test for.
	 * @internal
	 */
	function _fillValueSelect(e: HTMLElement,
		val: number|string|null): void
	{
		let i: number;
		const list: HTMLCollectionOf<HTMLElement> = 
			e.getElementsByTagName('option');

		for (i = 0; i < list.length; i++) {
			const attrval: string|null = 
				(<HTMLOptionElement>list[i]).value;
			_rattr(list[i], 'selected');
			if (val === null || attrval === null)
				continue;
			if (val.toString() === attrval)
				_attr(list[i], 'selected', 'selected');
		}
	}

	/**
	 * Fill in ISO-8601 dates.
	 * Does nothing for null or unexported date.
	 * @param e Root of tree scanned for elements.
	 * @param fname Structure name, '-', field name.
	 * @param val Epoch date itself.
	 * @param inc Include root in scanning for elements.
	 * @internal
	 */
	function _fillDateValue(e: HTMLElement, fname: string,
		val: string|number|null|undefined, inc: boolean): void
	{
		const v: Long|null = Long.fromValue(val);
		const d: Date = new Date();

		if (v === null)
			return;

		d.setTime(v.toNumber() * 1000);

		/* Make sure to zero-pad the digits. */

		const year: number = d.getFullYear();
		const mo: number = d.getMonth() + 1;
		const day: number = d.getDate();
		const full: string = year + '-' +
			(mo < 10 ? '0' : '') + mo + '-' +
			(day < 10 ? '0' : '') + day;

		_attrcl(e, 'value', fname + '-date-value', full, inc);
		_replcl(e, fname + '-date-text', full, inc);
	}

	/**
	 * Check input elements (that is, set the attribute `checked` to
	 * the value `checked`) for elements with class 
	 * `fname-value-checked` whose non-null, numeric value as a bit index
	 * is set in the bit-field given as input.
	 * A null value never matches.
	 * @param e Root of tree scanned for elements.
	 * @param fname Structure name, '-', field name.
	 * @param val Bit-field to test for.
	 * @param inc Include root in scanning for elements.
	 * @internal
	 */
	function _fillBitsChecked(e: HTMLElement, fname: string,
		 val: string|number|null|undefined, inc: boolean): void
	{
		let i: number;
		let v: number;
		const lval: Long|null|undefined = 
			Long.fromValueUndef(val);
		const list: HTMLElement[] = _elemList
			(e, fname + '-bits-checked', inc);

		if (typeof lval === 'undefined')
			return;

		for (i = 0; i < list.length; i++) {
			const attrval: string|null = 
				(<HTMLInputElement>list[i]).value;
			_rattr(list[i], 'checked');
			if (lval === null || attrval === null)
				continue;

			/*
			 * This would be better served with
			 * Number.isInteger(), but we don't want to
			 * assume ES6.
			 */

			v = Number(attrval);
			if (isNaN(v))
				continue;
			if (!(isFinite(v) && Math.floor(v) === v))
				continue;
			if (v < 0 || v > 64)
				continue;

			if ((v === 0 && lval.isZero()) ||
			    !Long.ONE.shl(v - 1).and(lval).isZero())
				_attr(list[i], 'checked', 'checked');
		}
	}

	/**
	 * Fill a structure field.  This first does the has/no class
	 * setting for null values, then optionally returns if null
	 * (running the custom fields first), otherwise the generic
	 * text/value/etc fields, then finally the custom fields.
	 * @param e Root of the DOM tree filled into.
	 * @param strct Name of the structure filling in.
	 * @param name The name of the field.
	 * @param custom Custom callback functions.
	 * @param obj The data itself.
	 * @param inc Whether to include the root element in looking
	 * for elements to fill. Nested structures are always filled 
	 * non-inclusively.
	 * @param cannull Whether the data may be null.
	 * @param sub If the data object is a nested structure
	 * interface, the allocated class of that interface.
	 * @internal
	 */
	function _fillField(e: HTMLElement, strct: string, name: string,
		custom: DataCallbacks|null, obj: any, inc: boolean,
		cannull: boolean, sub: any): void
	{
		let i: number;
		const fname: string = strct + '-' + name;

		/* Don't do anything if we're not defined. */

		if (typeof obj === 'undefined')
			return;

		/* First handle our has/no null situation. */

		if (cannull) {
			if (obj === null) {
				_hidecl(e, strct + '-has-' + name, inc);
				_showcl(e, strct + '-no-' + name, inc);
			} else {
				_showcl(e, strct + '-has-' + name, inc);
				_hidecl(e, strct + '-no-' + name, inc);
			}
		}

		/* Don't process null values that can be null. */

		if (cannull && obj === null) {
			if (custom !== null && fname in custom) {
				if (custom[fname] instanceof Array) {
					for (i = 0; i < custom[fname].length; i++)
						custom[fname][i](e, fname, null);
				} else
					custom[fname](e, fname, null);
			}
			return;
		}

		/* 
		 * Non-null non-structs.
		 * Note that "sub" is never undefined because that would
		 * otherwise be caught in the "typeof obj" guard above.
		 */

		if (sub !== null) {
			const list: HTMLElement[] = 
				_elemList(e, fname + '-obj', inc);
			for (i = 0; i < list.length; i++)
				sub.fillInner(list[i], custom);
		} else {
			const list: HTMLElement[] = 
				_elemList(e, fname + '-enum-select', inc);
			for (i = 0; i < list.length; i++)
				_fillValueSelect(list[i], obj);
			_replcl(e, fname + '-text', obj, inc);
			_attrcl(e, 'value', fname + '-value', obj, inc);
			_fillValueChecked(e, fname, obj, inc);
		}

		/* Lastly, handle the custom callback. */

		if (custom !== null &&
		    typeof custom[fname] !== 'undefined') {
			if (custom[fname] instanceof Array) {
				for (i = 0; i < custom[fname].length; i++)
					custom[fname][i](e, fname, obj);
			} else
				custom[fname](e, fname, obj);
		}
	}
