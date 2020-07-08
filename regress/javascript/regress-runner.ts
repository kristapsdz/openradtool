/// <reference path="/usr/local/lib/node_modules/@types/node/index.d.ts" />

const ts = require('typescript');
const fs = require('fs');
const jsdom = require('jsdom');
const { JSDOM } = jsdom;
const { execFileSync, spawnSync } = require('child_process');

const basedir: string = 'regress/javascript';
let i: number;
let result: string;
let contents: string;
let tsname: string;
let xmlname: string;
let ortname: string;
let resname: string;
let basename: string;
let func: Function;
let script: string;
const files: string[] = fs.readdirSync(basedir);
let diff;

script = fs.readFileSync(basedir + '/regress.ts').toString();

for (i = 0; i < files.length; i++) {
	if (files[i].substring
	    (files[i].length - 4, files[i].length) !== '.ort')
		continue;
	basename = basedir + '/' + 
		files[i].substring(0, files[i].length - 4);

	tsname = basename + '.ts';
	xmlname = basename + '.xml';
	ortname = basename + '.ort';
	resname = basename + '.result';

	diff = spawnSync('./ort-javascript', ['-S', '.', ortname]);
	contents = diff.stdout.toString();
	contents += script;
	contents += fs.readFileSync(tsname).toString();

	/* Compile it into JavaScript. */

	result = ts.transpile(contents);

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
		console.log('Failing result:');
		console.log(result);

		/* Emit the difference. */

		const diff = spawnSync('diff', ['-w', '-u', resname, '-'], {
			'input': result
		});
		console.log(diff.stdout.toString());
		process.exit(1);
	}
}
