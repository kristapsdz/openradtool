function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'val': '0',
		'id': 1,
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
