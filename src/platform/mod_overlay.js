// Browser hosts can provide a URL, File/Blob, or Uint8Array before loading the
// generated application script. Keep the ZIP in MEMFS; C reads it into the
// overlay before opening any assets.
if (Module['modZip'] !== undefined) {
  var shandalarModSource = Module['modZip'];
  Module['arguments'] = (Module['arguments'] || []).concat(['--mod=/shandalar-mod.zip']);
  Module['preRun'] = Module['preRun'] || [];
  if (typeof Module['preRun'] === 'function') {
    Module['preRun'] = [Module['preRun']];
  }
  Module['preRun'].push(function() {
    addRunDependency('shandalar-mod-zip');
    Promise.resolve().then(async function() {
      var bytes;
      if (typeof shandalarModSource === 'string') {
        var response = await fetch(shandalarModSource);
        if (!response.ok) {
          throw new Error('Mod ZIP download failed: HTTP ' + response.status);
        }
        bytes = new Uint8Array(await response.arrayBuffer());
      } else if (shandalarModSource instanceof Uint8Array) {
        bytes = shandalarModSource;
      } else if (shandalarModSource && typeof shandalarModSource.arrayBuffer === 'function') {
        bytes = new Uint8Array(await shandalarModSource.arrayBuffer());
      } else {
        throw new Error('Module.modZip must be a URL, File/Blob, or Uint8Array');
      }
      FS.writeFile('/shandalar-mod.zip', bytes);
      removeRunDependency('shandalar-mod-zip');
    }).catch(function(error) {
      abort('Could not load mod ZIP: ' + error.message);
    });
  });
}
