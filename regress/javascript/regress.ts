let contents: Buffer;

contents = fs.readFileSync(fname);
doc = new JSDOM(contents.toString()).window.document;
(<any>global).document = doc;
return runTest(doc).body.innerHTML;
