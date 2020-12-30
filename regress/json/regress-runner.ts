/*
 * Requirements for typescript, file-system, and spawning ort-nodejs.
 */

const fs = require('fs');
const { execFileSync, spawnSync } = require('child_process');

const basedir: string = 'regress';
const files: string[] = fs.readdirSync(basedir);
let i: number;
let res: any;

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

	/* Run ort-json on ort(5) configuration, catch errors. */

	const jsonProc = spawnSync('./ort-json', [ortname]);

	if (jsonProc.status !== null && jsonProc.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (did not execute)');
		console.log(jsonProc.stderr);
		process.exit(1);
	}

	try {
		res = JSON.parse(jsonProc.stdout.toString());
	} catch (er) {
		console.log('ts-node: ' + ortname + 
			'... fail (did not execute)');
		console.log(er);
		process.exit(1);
	}

	const obj: ortJson.ortJsonConfig = 
		<ortJson.ortJsonConfig>res;
	const fmt: ortJson.ortJsonConfigFormat =
		new ortJson.ortJsonConfigFormat(obj);

	console.log(fmt.toString());

	const diffProc = spawnSync('./ort-diff', [ortname], {
		'input': fmt.toString()
	});

	if (diffProc.status !== null && diffProc.status !== 0) {
		console.log('ts-node: ' + ortname + 
			'... fail (did not compare)');
		console.log(diffProc.stderr.toString());
		console.log(diffProc.stdout.toString());
		process.exit(1);
	}

	/* ...else print out our errors and exit. */

	console.log('ts-node: ' + ortname + '... pass');
}
