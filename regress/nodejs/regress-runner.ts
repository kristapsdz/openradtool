/// <reference path="../../node_modules/@types/node/index.d.ts" />

/*
 * Requirements for typescript, file-system, and spawning ort-nodejs.
 */

const ts = require('typescript');
const fs = require('fs');
const { execFileSync, spawnSync } = require('child_process');
const bcrypt = require('bcrypt');
const Database = require('better-sqlite3');
const validator = require('validator');

const tmpdb: string = '/tmp/regress.db';
const basedir: string = 'regress/nodejs';
let i: number;
let files: string[] = fs.readdirSync(basedir);
let result: boolean;

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
	const tsname: string = basename + '.ts';
	const script: string = fs.readFileSync(tsname).toString();

	const sql = spawnSync('./ort-sql', [ortname]);
	if (sql.status !== null && sql.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (ort-sql did not execute)');
		console.log(Error(sql.stderr));
		process.exit(1);
	}

	spawnSync('rm', ['-f', tmpdb]);

	const sqlite = spawnSync('sqlite3', [tmpdb], {
		'input': sql.stdout.toString()
	});
	if (sqlite.status !== null && sqlite.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (sqlite3 did not execute)');
		console.log(Error(sqlite.stderr));
		process.exit(1);
	}

	/* Run ort-nodejs on ort(5) configuration, catch errors. */

	const nodejs = spawnSync('./ort-nodejs', ['-v', '-e', ortname]);
	if (nodejs.status !== null && nodejs.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (ort-nodejs did not execute)');
		console.log(Error(nodejs.stderr));
		process.exit(1);
	}
	const full: string = nodejs.stdout.toString() + script;

	/* Try to transpile TypeScript output of ort-nodejs. */

	const output = ts.transpileModule(full, {
		compilerOptions: {
			allowsJs: false,
			alwaysStrict: true,
			module: 'es2015',
			noEmitOnError: true,
			noImplicitAny: true,
			noUnusedLocals: true,
			noUnusedParameters: true,
			strict: true,
			target: 'es6',
		},
		reportDiagnostics: true,
	});

	/* If we have diagnostics, fail. */

	if (typeof output.diagnostics !== 'undefined' &&
	    output.diagnostics.length > 0) {
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

	/* ...else try to run the function. */

	try {
		const func: Function = new Function
			('validator', 'bcrypt', 'Database', 'dbfile', 
			 output.outputText);
		result = func(validator, bcrypt, Database, tmpdb);
	} catch (error) {
		console.log('ts-node: ' + ortname + '... fail');
		const cat = spawnSync('cat', ['-n', '-'], {
			'input': output.outputText
		});
		console.log(cat.stdout.toString());
		console.log(error);
		process.exit(1);
	}

	if (!result) {
		console.log('ts-node: ' + ortname + '... test fail');
		process.exit(1);
	}

	console.log('ts-node: ' + ortname + '... pass');
}

spawnSync('rm', ['-f', tmpdb]);
