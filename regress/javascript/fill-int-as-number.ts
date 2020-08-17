function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'test': 1099511627776,
		'id': 1
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
