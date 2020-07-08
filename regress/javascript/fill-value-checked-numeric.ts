function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'foo': 23,
		'id': 1,
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
