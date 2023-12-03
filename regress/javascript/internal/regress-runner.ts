/// <reference path="../../../node_modules/@types/node/index.d.ts" />

/*
 * Requires for typescript, file-system, JSDOM, and ort-javascript(1)
 */

const ts = require('typescript');
const fs = require('fs');
const { execFileSync, spawnSync } = require('child_process');

const basedir: string = 'regress/javascript/internal';

let result: boolean;
let files: string[] = fs.readdirSync(basedir);

const script: string = 
	fs.readFileSync('ortPrivate.ts').toString();

for (let i = 0; i < files.length; i++) {
	if (files[i].substring
	    (files[i].length - 3, files[i].length) !== '.ts')
		continue;
	if (files[i] === 'regress-runner.ts')
		continue;

	const tsname: string = basedir + '/' + files[i];

	const contents: string = 
		'namespace ortJson {\n' +
		script + 
		'}\n' +
		fs.readFileSync(tsname).toString();

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
		console.log('ts-node: ' + tsname + 
			    '... fail (transpile)');
		console.log(ts.formatDiagnosticsWithColorAndContext
			(output.diagnostics, {
				getCurrentDirectory: () => '.',
				getCanonicalFileName: (f: string) => '<stdin>',
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
			(output.outputText);
		result = func();
	} catch (error) {
		console.log('ts-node: ' + tsname + 
			    '... fail (runtime)');
		const cat = spawnSync('cat', ['-n', '-'], {
			'input': output.outputText
		});
		console.log(cat.stdout.toString());
		console.log(error);
		process.exit(1);
	}

	if (!result) {
		console.log('ts-node: ' + tsname + '... fail');
		process.exit(1);
	}

	console.log('ts-node: ' + tsname + '... pass');
}
