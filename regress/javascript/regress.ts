const contents = fs.readFileSync(fname);
const dom = new JSDOM(contents.toString());

/* Fixup to pretend this is a browser. */

(<any>global).document = dom.window.document;
(<any>global).HTMLElement = dom.window.HTMLElement;

return runTest(dom.window.document).body.innerHTML;
