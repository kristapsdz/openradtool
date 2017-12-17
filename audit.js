(function(root) {
	'use strict';

	function clr(root)
	{
		var e;
		if (null !== (e = find(root))) 
			while (e.firstChild)
				e.removeChild(e.firstChild);
		return(e);
	}

	function repl(root, text)
	{
		var e;
		if (null !== (e = clr(root))) 
			e.appendChild(document.createTextNode(text));
		return(e);
	}

	function showcl(root, name)
	{
		var list, i, sz, e;
		if (null === (e = find(root)))
			return;
		list = e.getElementsByClassName(name);
		for (i = 0, sz = list.length; i < sz; i++)
			show(list[i]);
	}

	function hidecl(root, name)
	{
		var list, i, sz, e;
		if (null === (e = find(root)))
			return;
		list = e.getElementsByClassName(name);
		for (i = 0, sz = list.length; i < sz; i++)
			hide(list[i]);
	}

	function replcl(root, name, text)
	{
		var list, i, sz, e;
		if (null === (e = find(root)))
			return;
		list = e.getElementsByClassName(name);
		for (i = 0, sz = list.length; i < sz; i++)
			repl(list[i], text);
	}

	function find(root)
	{
		return((typeof root !== 'string') ? 
			root : document.getElementById(root));
	}

	function show(root)
	{
		var e;
		if (null === (e = find(root)))
			return(null);
		if (e.classList.contains('hide'))
			e.classList.remove('hide');
		return(e);
	}

	function hide(root)
	{
		var e;
		if (null === (e = find(root)))
			return(null);
		if ( ! e.classList.contains('hide'))
			e.classList.add('hide');
		return(e);
	}

	function attr(root, key, val)
	{
		var e;
		if (null !== (e = find(root)))
			e.setAttribute(key, val);
		return(e);
	}

	function attrcl(root, name, key, val)
	{
		var list, i, sz, e;
		if (null === (e = find(root)))
			return;
		list = e.getElementsByClassName(name);
		for (i = 0, sz = list.length; i < sz; i++)
			attr(list[i], key, val);
	}

	function auditAccessfromFill(data, root)
	{
		var i, str = '';

		replcl(root, 'audit-data-accessfrom-function', data.function);
		if (0 === data.path.length) {
			showcl(root, 'audit-data-accessfrom-nopath');
			hidecl(root, 'audit-data-accessfrom-path');
			return;
		}

		for (i = 0; i < data.path.length; i++)
			str += (i > 0 ? '.' : '') + data.path[i];

		hidecl(root, 'audit-data-accessfrom-nopath');
		showcl(root, 'audit-data-accessfrom-path');
		replcl(root, 'audit-data-accessfrom-path', str);
	}


	function auditDataFill(data, root)
	{
		replcl(root, 'audit-data-field-name', data.field);
		if (data.export) {
			hidecl(root, 'audit-data-field-noexport');
			showcl(root, 'audit-data-field-export');
		} else {
			showcl(root, 'audit-data-field-noexport');
			hidecl(root, 'audit-data-field-export');
		}
	}

	function fillUpdate(root, obj)
	{
		replcl(root, 'audit-update-function', obj);
	}

	function fillInsert(root, obj)
	{
		replcl(root, 'audit-insert-function', obj.function);
	}

	function fillDelete(root, obj)
	{
		replcl(root, 'audit-delete-function', obj);
	}

	function fillSearch(root, obj)
	{
		replcl(root, 'audit-search-function', obj);
	}

	function fillList(root, obj)
	{
		replcl(root, 'audit-list-function', obj);
	}

	function fillIterate(root, obj)
	{
		replcl(root, 'audit-iterate-function', obj);
	}

	function fill(root, vec, name, func)
	{
		var list, i, j, sub, clone;

		vec.sort(function(a, b) {
			return(a.localeCompare(b));
		});

		if (vec.length > 0) {
			hidecl(root, 'audit-no' + name);
			showcl(root, 'audit-' + name + '-list');
			list = root.getElementsByClassName
				('audit-' + name + '-list');
			for (i = 0; i < list.length; i++) {
				sub = list[i].children[0];
				clr(list[i]);
				for (j = 0; j < vec.length; j++) {
					clone = sub.cloneNode(true);
					list[i].appendChild(clone);
					func(clone, vec[j]);
				}
			}
		} else {
			hidecl(root, 'audit-' + name + '-list');
			showcl(root, 'audit-no' + name);
		}
	}

	function fillVec(vec, name, func)
	{
		var e, sub, i, clone;

		vec.sort(function(a, b) {
			return(a.localeCompare(b));
		});

		e = find('audit-' + name + '-list');
		sub = e.children[0];
		clr(e);
		for (i = 0; i < vec.length; i++) {
			clone = sub.cloneNode(true);
			e.appendChild(clone);
			func(clone, vec[i]);
		}

		if (0 === vec.length) {
			hide('audit-' + name + '-list');
			show('audit-no' + name);
		} else {
			show('audit-' + name + '-list');
			hide('audit-no' + name);
		}
	}

	function auditFill(audit, root)
	{
		var e, sub, i, j, clone, list, vec;

		replcl(root, 'audit-name', audit.name);
		attrcl(root, 'audit-name', 'for', audit.name);
		attrcl(root, 'audit-view', 'id', audit.name);

		/* If found, fill in data field and access members. */

		if ('data' in audit.access) {
			hidecl(root, 'audit-nodata');
			showcl(root, 'audit-data');
			showcl(root, 'audit-accessfrom');

			audit.access.data.sort(function(a, b) {
				return(a.field.localeCompare(b.field));
			});
			audit.access.accessfrom.sort(function(a, b) {
				return(a.function.localeCompare(b.function));
			});

			vec = audit.access.data;
			list = root.getElementsByClassName
				('audit-data-list');
			for (i = 0; i < list.length; i++) {
				sub = list[i].children[0];
				clr(list[i]);
				for (j = 0; j < vec.length; j++) {
					clone = sub.cloneNode(true);
					list[i].appendChild(clone);
					auditDataFill(vec[j], clone);
				}
			}

			vec = audit.access.accessfrom;
			list = root.getElementsByClassName
				('audit-accessfrom-list');
			for (i = 0; i < list.length; i++) {
				sub = list[i].children[0];
				clr(list[i]);
				for (j = 0; j < vec.length; j++) {
					clone = sub.cloneNode(true);
					list[i].appendChild(clone);
					auditAccessfromFill(vec[j], clone);
				}
			}
		} else {
			hidecl(root, 'audit-data');
			hidecl(root, 'audit-accessfrom');
			showcl(root, 'audit-nodata');
		}

		/* Fill insert function (if applicable). */

		if (null !== audit.access.insert) {
			showcl(root, 'audit-insert');
			hidecl(root, 'audit-noinsert');
			fillInsert(root, audit.access.insert);
		} else {
			hidecl(root, 'audit-insert');
			showcl(root, 'audit-noinsert');
		}

		/* Fill other functions. */

		fill(root, audit.access.searches, 'searches', fillSearch);
		fill(root, audit.access.iterates, 'iterates', fillIterate);
		fill(root, audit.access.lists, 'lists', fillList);
		fill(root, audit.access.updates, 'updates', fillUpdate);
		fill(root, audit.access.deletes, 'deletes', fillDelete);
	}

	function init() 
	{
		var e, sub, i, j, clone, vec;

		hide('parsing');

		if (null === audit) {
			show('parseerr');
			return;
		}

		show('parsed');
		repl('audit-role', audit.role);

		audit.access.sort(function(a, b) {
			return(a.name.localeCompare(b.name));
		});

		e = find('audit-access-list');
		sub = e.children[0];
		clr(e);
		for (i = 0; i < audit.access.length; i++) {
			clone = sub.cloneNode(true);
			e.appendChild(clone);
			auditFill(audit.access[i], clone);
		}

		/* Fill insert functions. */

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			if (null !== audit.access[i].access.insert)
				vec.push(audit.access[i].access.insert);

		fillVec(vec, 'inserts', fillInsert);

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			for (j = 0; j < audit.access[i].access.deletes.length; j++)
				vec.push(audit.access[i].access.deletes[j]);

		fillVec(vec, 'deletes', fillDelete);

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			for (j = 0; j < audit.access[i].access.updates.length; j++)
				vec.push(audit.access[i].access.updates[j]);

		fillVec(vec, 'updates', fillUpdate);

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			for (j = 0; j < audit.access[i].access.searches.length; j++)
				vec.push(audit.access[i].access.searches[j]);

		fillVec(vec, 'searches', fillSearch);

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			for (j = 0; j < audit.access[i].access.iterates.length; j++)
				vec.push(audit.access[i].access.iterates[j]);

		fillVec(vec, 'iterates', fillIterate);

		vec = [];
		for (i = 0; i < audit.access.length; i++)
			for (j = 0; j < audit.access[i].access.lists.length; j++)
				vec.push(audit.access[i].access.lists[j]);

		fillVec(vec, 'lists', fillList);
	}

	root.init = init;
})(this);

window.addEventListener('load', init);
