function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData = { 
		'foo': 'foobarbaz',
		'bar': 1.0,
		'id': 1,
	};
	const obj: ort.foo = new ort.foo(data);

	obj.fillInner(dom.getElementById('foo'));

	return dom;
}
