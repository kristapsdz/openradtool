function formatBinary(e: HTMLElement, name: string, v: string): void
{
	const list: HTMLCollectionOf<Element> = 
		e.getElementsByClassName(name + '-binary');
	let elem: HTMLElement;
	let i: number;

	for (i = 0; i < list.length; i++) {
		elem = <HTMLElement>list[i];
		while (elem.firstChild)
			elem.removeChild(elem.firstChild);
		elem.appendChild(document.createTextNode
			(Buffer.from(v, 'base64').toString()));
	}
}


function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'blob': 'Zm9vYmFyYmF6', /* "foobarbaz" w/o newline */
		'id': 1
	};
	const custom: ort.DataCallbacks = {
		'foo-blob': formatBinary
	};

	const obj: ort.foo = new ort.foo(data, custom);

	obj.fill(dom.body, custom);

	return dom;
}
