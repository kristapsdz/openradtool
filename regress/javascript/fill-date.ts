function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'ctime': 1594223212,
		'id': 1
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fill(dom.body);

	return dom;
}
