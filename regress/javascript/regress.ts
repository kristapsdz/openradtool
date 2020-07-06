const fs = require('fs');
const jsdom = require('jsdom');
const { JSDOM } = jsdom;

fs.readFile('@FILE@', 'utf8', function(err, data) {
	let doc: HTMLDocument;
	if (err !== null) {
		process.stderr.write(err);
		process.exit(1);
	}
	doc = new JSDOM(data).window.document;
	(<any>global).document = doc;
	process.stdout.write(runTest(doc).body.innerHTML);
});
