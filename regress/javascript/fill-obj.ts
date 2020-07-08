function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'id': 1,
		'barid': 2,
		'bar': {
			'foo': 'foobarbaz',
			'bar': 3.0,
			'id': 2,
		}
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
