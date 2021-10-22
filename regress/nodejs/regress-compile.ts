/// <reference path="../../node_modules/@types/node/index.d.ts" />

/*
 * Requirements for typescript, file-system, and spawning ort-nodejs.
 */

const ts = require('typescript');
const fs = require('fs');
const { execFileSync, spawnSync } = require('child_process');

const basedir: string = 'regress';
let i: number;
let files: string[] = fs.readdirSync(basedir);

/*
 * Loop through all files in the regress directory, which basically
 * covers all features of ort(5).
 * Produce only ".ort" files.
 */

for (i = 0; i < files.length; i++) {
	if (files[i].substring
	    (files[i].length - 4, files[i].length) !== '.ort')
		continue;
	
	/* Examine individual ort(5) configuration. */

	const basename: string = basedir + '/' + 
		files[i].substring(0, files[i].length - 4);
	const ortname: string = basename + '.ort';

	/* Run ort-nodejs on ort(5) configuration, catch errors. */

	const out = spawnSync('./ort-nodejs', [ortname]);

	if (out.status !== null && out.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (did not execute)');
		console.log(Error(out.stderr));
		process.exit(1);
	}

	/* Try to transpile TypeScript output of ort-nodejs. */

	const output = ts.transpileModule(out.stdout.toString(), {
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

	/* If we don't have diagnostics, succeed. */

	if (typeof output.diagnostics === 'undefined' ||
	    output.diagnostics.length === 0) {
		console.log('ts-node: ' + ortname + '... pass');
		continue;
	}

	/* ...else print out our errors and exit. */

	console.log('ts-node: ' + ortname + '... fail');
	console.log(ts.formatDiagnosticsWithColorAndContext
		(output.diagnostics, {
			getCurrentDirectory: () => '.',
			getCanonicalFileName: f => '<stdin>',
			getNewLine: () => '\n'
		})
	);
	process.exit(1);
}
