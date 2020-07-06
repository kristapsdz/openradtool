/// <reference path="/usr/local/lib/node_modules/@types/node/index.d.ts" />

const ts = require('typescript');
const fs = require('fs');
const jsdom = require('jsdom');
const { JSDOM } = jsdom;
const { execFileSync } = require('child_process');

const basedir: string = 'regress/javascript';
let i: number;
let result: string;
let contents: Buffer;
let tsname: string;
let xmlname: string;
let resname: string;
let basename: string;
let func: Function;
const files: string[] = fs.readdirSync(basedir);

for (i = 0; i < files.length; i++) {
	if (files[i].substring
	    (files[i].length - 9, files[i].length) !== '.final.ts')
		continue;
	basename = basedir + '/' + 
		files[i].substring(0, files[i].length - 9);
	tsname = basename + '.final.ts';
	xmlname = basename + '.xml';
	resname = basename + '.result';

	/* Read the file and compile it into JavaScript. */

	contents = fs.readFileSync(tsname);
	result = ts.transpile(contents.toString());

	/* 
	 * Create and invoke a callable function that accepts the
	 * file-system and jsdom requirement along with the input
	 * filename.
	 */

	func = new Function('fs', 'JSDOM', 'fname', result);
	result = func(fs, JSDOM, xmlname);

	/*
	 * Now see if the result is different.
	 * Use `diff` instead of rolling our own.
	 */

	try {
		process.stdout.write(tsname + '... ');
		execFileSync('diff', ['-w', '-u', resname, '-'], {
			'input': result
		});
		console.log('pass');
	} catch(error) {
		console.log('fail');
		process.exit(1);
	}
}
