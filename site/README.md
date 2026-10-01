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

The same runtime exports `BuildBundle(root, search, inputs, entry_module,
entry_function, output)` for editors that provide their own host. Search roots
and input files are newline-separated filesystem paths; named roots use the
normal `name=/directory` form. Clients write source into `FS`, then read the
compiled `.zib` output. A failed compilation removes that output. This path
uses the ordinary module loader, checker, linker, verifier, and bundle writer;
it does not execute the program or install host capabilities.

`scripts/build_playground.py --output-dir DIR --embed-file local@/virtual`
builds the runtime for another site with additional library sources.
`node tests/web_bundle_runtime.cjs` checks this compiler API. Native browser
hosts can keep a bundle instance across frames and bound each call with
`BundleInstanceLimitSteps` (`LimitBundleSteps` in `std/bundle_host`). The generic
Web bridge accepts both wasm32 pointers and lowered memory64 pointers.

With Chromium and Xvfb installed, `node tests/playground_browser.mjs` checks
editing, execution, cancellation, diagrams, and responsive layouts. It starts
its own local server and a private display, without using the desktop display.
