function runTest(dom: HTMLDocument): HTMLDocument
{
	const data: ort.fooData[] = [
		{ 
			'foo': 'foobarbaz',
			'bar': 1.0,
			'id': 1,
		}, { 
			'foo': 'foobarbaz2',
			'bar': 2.0,
			'id': 2,
		}
	];
	const obj: ort.foo = new ort.foo(data);

	obj.fillArray(dom.getElementById('foo'));

	return dom;
}
