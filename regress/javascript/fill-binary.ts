function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'blob': 'Zm9vYmFyYmF6Cg==',
		'id': 1
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
