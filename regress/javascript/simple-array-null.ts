function runTest(dom: HTMLDocument): HTMLDocument
{
	const obj: ort.foo = new ort.foo(null);

	obj.fillArray(dom.getElementById('foo'));

	return dom;
}
