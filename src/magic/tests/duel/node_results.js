// Persist GoogleTest XML without exposing host files to card/game code.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  if (typeof process !== 'undefined' && process.env.DUEL_TEST_OUTPUT_DIR) {
    FS.mkdir('/test-output');
    FS.mount(NODEFS, {root: process.env.DUEL_TEST_OUTPUT_DIR}, '/test-output');
  }
});
