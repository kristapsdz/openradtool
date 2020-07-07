function runTest(dom: HTMLDocument): HTMLDocument
{
	const obj: ort.foo = new ort.foo([]);

	obj.fillArray(dom.getElementById('foo'));

	return dom;
}
