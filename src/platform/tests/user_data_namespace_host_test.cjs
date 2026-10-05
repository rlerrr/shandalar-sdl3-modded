const fs = require('node:fs');
const path = require('node:path');
const program = path.resolve(process.argv[2]);
const root = process.argv[3];
const config = { arguments: [root], userDataRoot: root };
const run = new Function('require', '__dirname', '__filename', 'module', 'Module', fs.readFileSync(program, 'utf8'));
run(require, path.dirname(program), program, { exports: {} }, config);
