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
		val: number|null|undefined, inc: boolean): void
	{
		let year: number;
		let mo: number;
		let day: number;
		let full: string;
		const d: Date = new Date();

		if (val === null || typeof val === 'undefined')
			return;
		d.setTime(val * 1000);
		year = d.getFullYear();
		mo = d.getMonth() + 1;
		day = d.getDate();

		/* Make sure to zero-pad the digits. */

		full = year + '-' +
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
		 val: number|null|undefined, inc: boolean): void
	{
		let i: number;
		let v: number;
		const list: HTMLElement[] = _elemList
			(e, fname + '-bits-checked', inc);

		if (typeof val === 'undefined')
			return;

		for (i = 0; i < list.length; i++) {
			const attrval: string|null = 
				(<HTMLInputElement>list[i]).value;
			_rattr(list[i], 'checked');
			if (val === null || attrval === null)
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
			if (v < 0)
				continue;
			if ((v === 0 && val === 0) || 
			    (v > 0 && ((1 << (v - 1)) & val)))
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
