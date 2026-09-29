# Ziran website

The homepage editor runs the repository's actual module loader, checker,
portable linker, and VM in a Web Worker. Its runtime loads only when someone
presses Run. It has the portable subset's language coverage and no installed
host capabilities. Workers can be stopped and are recreated for each run.

## Build and preview

Activate Emscripten SDK 6.0.6, then run from the repository root:

```sh
python3 scripts/build_playground.py
python3 -m http.server 8765 --bind 127.0.0.1 --directory site
```

Open `http://127.0.0.1:8765/`. A web server is needed for the worker and
WebAssembly assets; opening `index.html` as a file still shows the examples.

The runtime assets are generated and ignored by Git. The website workflow
builds them before deploying, including when the compiler or standard library
changes. Local build objects and the Emscripten cache live under `build/`.

`tests/playground_runtime.cjs` checks execution and diagnostics against the
built runtime. Run it with `node tests/playground_runtime.cjs`.

With Chromium and Xvfb installed, `node tests/playground_browser.mjs` checks
editing, execution, cancellation, diagrams, and responsive layouts. It starts
its own local server and a private display, without using the desktop display.
