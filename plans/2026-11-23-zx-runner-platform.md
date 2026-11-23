# ZX Runner: one generic runner for every .zx

Owner, 2026-11-23: a standalone `.zx` runner like LÖVE's, built by Ziran, on
Linux and Android, with real icons. "kryon shouldnt haev a sepcail zx handl",
"clean hiraechy celan coed", and "the runner sholud be generic like executing
a biary file on linux or an appimage so colud be cli or gui ... if it uses
kryon modules thata re in ziran taht is totalyl accetpbal it is menat to be a
modular arcihetcuer".

## Design

- `zx app.zx` loads any bundle, checks its header, translates it for this CPU
  and runs it; console or window makes no difference. The runner knows no
  library: a bundle carries all of its code, Kryon included, and imports only
  host capabilities.
- The runner provides one small, fixed, versioned capability set, as a kernel
  provides syscalls or WASI its imports. A bundle importing a capability this
  device's runner lacks fails with a clear error; nothing else runs it.
  - Base, for every bundle: process and arguments, files, time, network,
    threads.
  - Window, for GUI bundles: a window and drawing surface (GPU or 2D, plus
    text rasterisation where it cannot live in the bundle), input events
    (pointer, keys, text, touch), clipboard and cursor, audio.
- Kryon's backend code that calls SDL, Cairo, Pango or EGL splits in two: the
  runner's host side, in Ziran, and Kryon's side, compiled into the bundle and
  calling only the generic imports. Kryon's players are then deleted.

## Stages

1. Done: the generic runner for console bundles (cmd/zx_runner*.zi), the
   `.zx` file type, desktop entry, default opener and failure dialog
   (`packaging/linux`, `make install-user`), checked by `install_user`.
   Open: generated app and file-type icons, exported to hicolor 16-512 and
   Android adaptive layers.
2. Window capabilities: their versioned interface in Ziran with docs and
   tests, the desktop host behind it linked into `zx`, and Kryon's desktop
   backend ported onto it, so a Kryon example `.zx` runs through plain `zx`
   on a private Xvfb. The surface to cover is what Kryon's player answers
   today (`kryon/src/backend/desktop_zx.zi` `Supported`): raster shapes, text,
   triangles and images, glyph metrics, bitmaps, cursor, window state, input
   and text entry, clipboard, media and video, clock, theme colour.
3. Every Kryon app (Breeze, Werk, Jungle and the rest) builds for that runner;
   Kryon's players (desktop_zx, app_player, window_player, zx_window_*,
   bundle_player*, android_bundle_run) and the player copies
   `mk/ziran-project.mk` installs are deleted; app lanes bump their pins.
4. Android ZX Runner APK on the same capability set: opens `.zx` from VIEW
   and SEND intents (`application/x-ziran-bundle`, `*.zx`) and its own recent
   list; tested on the Moto (armv7) and Pixel 4a (aarch64).
