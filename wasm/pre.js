// Emscripten --pre-js: persistent saves via IDBFS (IndexedDB-backed) at /save.
//
// The game writes save/*.CAR/.HIL/… relative to cwd "/" (the chdir-to-binary
// is skipped on WASM — see main.c), i.e. into /save. We mount IDBFS there and
// load any existing saves from IndexedDB on startup. To PERSIST a save you must
// FS.syncfs(false, …) after writing it — wire that into the save path (a save-
// side hook is the docs/WASM.md §2d follow-up); the mount + initial load below
// are enough to round-trip once that hook lands.
Module['preRun'] = Module['preRun'] || [];

// SWOS.ini lives in the browser's localStorage (a small editable text file).
// Copy it into MEMFS at /SWOS.ini BEFORE main() runs so Settings_Load picks up
// the user's settings. The web settings panel (shell.html) edits the localStorage
// copy and reloads. Synchronous — no run-dependency needed.
Module['preRun'].push(function () {
  try {
    var ini = localStorage.getItem('SWOS.ini');
    if (ini !== null && ini !== '') FS.writeFile('/SWOS.ini', ini);
  } catch (e) { /* no localStorage (e.g. node) — the game falls back to defaults */ }
});

Module['preRun'].push(function () {
  try { FS.mkdir('/save'); } catch (e) { /* already exists on a warm re-run */ }
  // IDBFS needs a working IndexedDB (every browser has it). node ships a
  // present-but-nonfunctional `indexedDB` global, so gate on emscripten's own
  // ENVIRONMENT_IS_NODE and fall back to in-memory /save there (node smoke
  // tests / headless) — startup must not abort; saves just won't persist.
  var isNode = (typeof ENVIRONMENT_IS_NODE !== 'undefined') && ENVIRONMENT_IS_NODE;
  if (!isNode && typeof indexedDB !== 'undefined') {
    FS.mount(IDBFS, {}, '/save');
    FS.syncfs(true, function (err) {
      if (err) console.warn('SWOS: IDBFS load failed:', err);
    });
  }
});
