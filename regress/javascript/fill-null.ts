function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'id': 1,
		'foo': 'xyz',
		'bar': null
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
