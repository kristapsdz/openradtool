function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'test': '1152921504606846976',
		'id': 1
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
