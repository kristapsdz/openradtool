/// <reference path="../../node_modules/@types/node/index.d.ts" />

/*
 * Requires for typescript, file-system, JSDOM, and ort-javascript(1)
 */

const ts = require('typescript');
const fs = require('fs');
const jsdom = require('jsdom');
const { JSDOM } = jsdom;
const { execFileSync, spawnSync } = require('child_process');

const basedir: string = 'regress/javascript';

let result: string;
let files: string[] = fs.readdirSync(basedir);

/*
 * This scriptlet just creates a DOM tree from the given file and runs
 * the test itself.
 */

const script: string = 
	fs.readFileSync(basedir + '/regress.ts').toString();

for (let i = 0; i < files.length; i++) {
	if (files[i].substring
	    (files[i].length - 4, files[i].length) !== '.ort')
		continue;

	/* 
	 * Examine individual ort(5) configuration.
	 * Then create our expected filenames.
	 */

	const basename: string = basedir + '/' + 
		files[i].substring(0, files[i].length - 4);

	const tsname: string = basename + '.ts';
	const xmlname: string = basename + '.xml';
	const ortname: string = basename + '.ort';
	const resname: string = basename + '.result';

	/* Run ort-javascript on ort(5), catch errors. */

	const out = spawnSync('./ort-javascript', ['-S', '.', ortname]);

	if (out.status !== null && out.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (did not execute)');
		console.log(Error(out.stderr));
		process.exit(1);
	}

	const contents: string = 
		out.stdout.toString() +
		fs.readFileSync(tsname).toString() +
		script;

	/* Try to transpile TypeScript of concatenation. */

	const output = ts.transpileModule(contents, {
		compilerOptions: {
			allowsJs: false,
			alwaysStrict: true,
			noEmitOnError: true,
			noImplicitAny: true,
			noUnusedLocals: true,
			noUnusedParameters: true,
			strict: true
		},
		reportDiagnostics: true,
	});

	/* If we have diagnostics, fail. */

	if (typeof output.diagnostics !== 'undefined' &&
	    output.diagnostics.length) {
		console.log('ts-node: ' + ortname + 
			    '... fail (transpile)');
		console.log(ts.formatDiagnosticsWithColorAndContext
			(output.diagnostics, {
				getCurrentDirectory: () => '.',
				getCanonicalFileName: f => '<stdin>',
				getNewLine: () => '\n'
			})
		);
		process.exit(1);
	}

	/* 
	 * Create and invoke a callable function that accepts the
	 * file-system and jsdom requirement along with the input
	 * filename.
	 */

	try {
		const func: Function = new Function
			('fs', 'JSDOM', 'fname', output.outputText);
		result = func(fs, JSDOM, xmlname);
	} catch (error) {
		console.log('ts-node: ' + ortname + '... fail');
		const cat = spawnSync('cat', ['-n', '-'], {
			'input': output.outputText
		});
		console.log(cat.stdout.toString());
		console.log(error);
		process.exit(1);
	}

	/*
	 * Now see if the result is different.
	 * Use `diff` instead of rolling our own.
	 */

	try {
		process.stdout.write('ts-node: ' + ortname + '... ');
		execFileSync('diff', ['-w', '-u', resname, '-'], {
			'input': result
		});
		console.log('pass');
	} catch (error) {
		console.log('fail');

		/* Emit the difference. */

		const diff = spawnSync('diff', ['-w', '-u', resname, '-'], {
			'input': result
		});
		console.log(diff.stdout.toString());
		process.exit(1);
	}
}
