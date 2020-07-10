function runTest(dom: HTMLDocument): HTMLDocument
{
	const custom: ort.DataCallbacks = {
		'foo-val': ort.somebits.format
	};
	const data: ort.fooData = { 
		'id': 1,
		'val': null
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body, custom);

	return dom;
}
