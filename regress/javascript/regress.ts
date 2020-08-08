const contents: Buffer =
	fs.readFileSync(fname);
const dom: JSDOM =
	new JSDOM(contents.toString());

(<any>global).document = dom.window.document;
return runTest(dom.window.document).body.innerHTML;
