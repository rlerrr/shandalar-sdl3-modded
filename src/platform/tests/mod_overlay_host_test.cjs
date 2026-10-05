// Run the generated Wasm test with host configuration supplied before startup.
// This Deflate ZIP fixture was produced by Python's zipfile, independently of
// the miniz writer used by the C tests.
const fs = require('node:fs');
const path = require('node:path');
const program = path.resolve(process.argv[2]);
const mode = process.argv[3];
const fixture = Buffer.from(
  'UEsDBBQAAAAIAHRLRV1ZGQBNCwAAAAkAAAAVAAAAb3ZlcmxheS10ZXN0LWJhc2UuYmluy81PUUiqLEktBgBQSwECFAAUAAAACAB0S0VdWRkATQsAAAAJAAAAFQAAAAAAAAAAAAAAgAEAAAAAb3ZlcmxheS10ZXN0LWJhc2UuYmluUEsFBgAAAAABAAEAQwAAAD4AAAAAAA==',
  'base64');
const config = { arguments: ['--startup-test'] };
let prefix = '';
switch (mode) {
  case 'bytes': config.modZip = new Uint8Array(fixture); break;
  case 'blob': config.modZip = new Blob([fixture]); break;
  case 'url': config.modZip = 'data:application/zip;base64,' + fixture.toString('base64'); break;
  case 'env':
    // Define this callback inside the generated script's lexical scope to
    // emulate an Emscripten --pre-js host using ENV and MEMFS directly.
    prefix = `Module.preRun = [function() {
      FS.writeFile('/env-mod.zip', new Uint8Array(${JSON.stringify([...fixture])}));
      ENV.SHANDALAR_MOD_ZIP = '/env-mod.zip';
    }];\n`;
    break;
  default: throw new Error('Unknown host test mode: ' + mode);
}
const run = new Function('require', '__dirname', '__filename', 'module', 'Module',
                         prefix + fs.readFileSync(program, 'utf8'));
run(require, path.dirname(program), program, { exports: {} }, config);
