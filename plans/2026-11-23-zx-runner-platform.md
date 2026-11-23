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
   `.zx` file type, desktop entry, default opener, failure dialog, and
   generated app and file-type icons exported to hicolor 16-512 and Android
   adaptive layers (`packaging/linux`, `packaging/android`, `make install-user`),
   checked by `install_user`.
2. Capabilities through the runner, with no compiler change. A capability is
   a `#foreign host_api` function in a Ziran std module; a bundle lists the
   ones it imports (`module:function`), and `zx` binds each from its own table
   through `std/bundle_host` or names the ones it lacks and stops. Today a
   bundle has only `print`: `std/file_linux` has no portable lowering, so a
   `.zx` cannot even read a file.
   a. Done: `zx` hosts the bundle in-process (`std/bundle_host`, native
      backend) and binds imports from its table (cmd/zx_runner_capabilities.zi).
   b. Done: the base set in `std/guest_assets`, `guest_clock`, `guest_files`
      and `guest_process` (docs/ZX.md "ZX Runner"). Open: `std/file` and its
      neighbours use them when `TARGET == .ZX`, so ordinary code runs in a
      bundle unchanged.
   c. Done: the window set in `std/guest_window` (open, present RGBA,
      events, wait, size, title, cursor, text input, clipboard, close),
      answered by cmd/zx_runner_window.zi on SDL2 when `pkg-config sdl2`
      finds it. The bundle runs its own loop and draws its own pixels.
      Open: display scale, a damage rectangle, cursor pictures, touch and
      audio output.
   d. Done: `install_user` opens a window through the installed `zx` on a
      private Xvfb; a pixel demo with pointer and keys ran the same way.
   e. Kryon's zx backend: its pixmap rasterizer and font decoder compiled
      into the bundle, presenting through the window set, so a Kryon example
      runs through plain `zx`. Kryon's player answers 62 capabilities on
      desktop and 46 on Android today (`kryon/src/backend/desktop_zx.zi` and
      `bundle_player.zi` `Supported`); video and system theme become
      optional capabilities.
3. Every Kryon app (Breeze, Werk, Jungle and the rest) builds for that runner;
   Kryon's players (desktop_zx, app_player, window_player, zx_window_*,
   bundle_player*, android_bundle_run) and the player copies
   `mk/ziran-project.mk` installs are deleted; app lanes bump their pins.
4. Done: Android ZX Runner APK on the same capability set
   (`packaging/android/build.py`). It opens `.zx` from VIEW and
   SEND intents (`application/x-ziran-bundle`, `*.zx`) and from its launcher,
   which picks a program and keeps a recent list. The window set is answered
   by the activity's EGL surface, letterboxed, with touch and key input mapped
   back into the bundle's pixels. Verified on the Moto (armv7, Android 9) and
   a Pixel 3a (arm64, Android 12): the console bundle prints and exits 42, a
   Kryon pixmap bundle renders its button page, and a tap raises the counter.
   Open: soft-keyboard text entry, pointer cursors and a multi-instance
   foreground service for long-running programs; the launcher's recents are
   app-private copies, so a removed file leaves a dead row until long-pressed.
