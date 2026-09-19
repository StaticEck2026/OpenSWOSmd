// Emscripten --pre-js: persistent saves via IDBFS (IndexedDB-backed) at /save.
//
// The game writes save/*.CAR/.HIL/… relative to cwd "/" (the chdir-to-binary
// is skipped on WASM — see main.c), i.e. into /save. We mount IDBFS there and
// load any existing saves from IndexedDB on startup. The write side is handled
// in the game: rob_save/rob_delete (src/resload.c) call FS.syncfs(false, …)
// after a successful write, so saves made in the browser persist. The mount and
// initial load below are the read half of that round trip.
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

// Netplay: a browser has no UDP, so Emscripten's socket layer sends each datagram
// as a WebSocket message to ws://<server>:<port> -- the matchmaker's WebSocket gate
// (docs/NETPLAY.md §8). A page served over https may only open wss://, so follow
// the page: the prefix form lets Emscripten complete it with the server and port.
if (typeof location !== 'undefined')
  Module['websocket'] = { url: location.protocol === 'https:' ? 'wss://' : 'ws://' };

// Test hooks, from the page's query string -- nothing is read without one, so a
// normal visit is unchanged:
//   ?arg=--srp-resim=/in/x.SRP&arg=...   command-line arguments, in order
//   ?env=SWOS_MM_AUTO=host&env=...        environment variables (getenv)
//   ?file=<url>                           fetched into /in/<name> before main()
// scripts/wasm_netplay_test.sh drives the browser build with these the way the
// native tests drive a binary with its command line and environment.
(function () {
  if (typeof location === 'undefined' || !location.search) return;
  var q = new URLSearchParams(location.search);
  var args = q.getAll('arg');
  if (args.length) Module['arguments'] = (Module['arguments'] || []).concat(args);
  var env = q.getAll('env');
  var files = q.getAll('file');
  Module['preRun'].push(function () {
    env.forEach(function (kv) {
      var i = kv.indexOf('=');
      if (i > 0) ENV[kv.slice(0, i)] = kv.slice(i + 1);
    });
    if (files.length) {
      try { FS.mkdir('/in'); } catch (e) { /* already there */ }
      files.forEach(function (url) {
        var name = url.split('?')[0].split('/').pop();
        FS.createPreloadedFile('/in', name, url, true, false);
      });
    }
  });
})();
