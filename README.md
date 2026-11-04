# Shandalar SDL3
A port of the 1997 video game [Magic: the Gathering](https://en.wikipedia.org/wiki/Magic:_The_Gathering_(1997_video_game)) by MicroProse (Version 1.3, English). It aims to be as accurate as possible, starting from a (somewhat) byte-matched decompilation of the original binaries.

This repository does **not** contain any game assets. An existing copy of the game is required.

On this branch, Shandalar uses the modded deckdll, cardartlib, and drawcardlib.
Builds prefer the sibling `../assets_modded` folder (`C:/dev/assets_modded`), falling back to
`C:/dev/shandalar` when it is absent. The browser mounts that data at
`/assets_modded`, matching deck-modded. `SHANDALAR_MODDED_ASSET_DIR` selects
the native default and the data packaged by the browser build; a command-line
asset root overrides the native runtime default. Keep game assets outside the
repository.

The Shandalar browser bundle includes deck-modded's startup renderer frames,
opening-menu resources, duel icons, fonts, and database/prompt tables.
Adventure artwork, music, portraits, resolution-specific sprites, and individual
images in `CardArtManalink` load on demand through the existing asset fetcher.

## ZIP mods

Shandalar, Deck Builder, and Facemaker accept one optional mod ZIP at startup:

```powershell
.\shandalar.exe C:/dev/assets_modded --mod=C:/mods/art-pack.zip
# Alternatively, set the environment variable before launching any app:
$env:SHANDALAR_MOD_ZIP = 'C:/mods/art-pack.zip'
```

`--mod path.zip` also works. The command-line choice overrides the environment
variable; specifying `--mod` more than once or an invalid ZIP fails startup.
Relative ZIP paths are resolved before the app changes its working directory.

ZIP entries should mirror paths **inside** the asset root, for example
`CardArtManalink/MyCard.jpg`, `Sound/MySound.wav`, or `Statwin_SDL/MyImage.png`.
Do not include an enclosing asset-root folder. Matching ignores ASCII case,
accepts either slash style, and collapses `.` and repeated separators. Absolute
paths, `..` components, encrypted/unsupported entries, and duplicate normalized
paths are rejected. Stored and Deflate-compressed ZIPs are supported by the
pinned [miniz library](https://github.com/richgel999/miniz/tree/3.1.2).

The archive stays in memory. Read-only opens through
`shandalar_open_file_io()` use ZIP entries first and fall back to the normal
filesystem and browser asset fetcher for absent entries. Each open decompresses
its entry into an independent, seekable SDL stream and frees those bytes on
close. Writes and update modes (`r+`, etc.) use the normal filesystem.
Corrupt overridden entries fail to open instead of falling back to base data.
Asset readers use the overlay for deck/configuration/text files, card databases
and catalogs, sprites, fonts, proprietary PIC/PCX images, and the SDL image,
audio, and video loaders. Artwork and Facemaker file discovery merge ZIP and
disk entries, matching wildcards and removing duplicates case-insensitively.
Save files, registry storage, exports, and logs still use the normal filesystem.
The ZIP does not rename assets or convert formats: legacy `Statwin/` resources
still need the port's `Statwin_SDL/` layout, and companion PNGs are not
automatically substituted for game-requested PIC filenames.

For WebAssembly, set `Module.modZip` **before** loading the generated app script:

```html
<script>
  var Module = {
    modZip: './mods/art-pack.zip', // URL, File/Blob, or Uint8Array
    userDataRoot: '/userdata/mods/art-pack', // optional browser save namespace
    arguments: ['/assets_modded']
  };
</script>
<script src="shandalar.js"></script>
```

The startup hook downloads/reads the ZIP into Emscripten's memory filesystem,
waits for it to finish, then passes it to the same C overlay used on native.
Download failures stop startup. A host with an existing MEMFS/preload setup can
instead pass `--mod=/mods/art-pack.zip` through `Module.arguments`, or set
`ENV.SHANDALAR_MOD_ZIP` in its `preRun` hook. Use one selection mechanism per
launch. Native environment variables on the build machine are not carried into
the browser.

Browser hosts can set `Module.userDataRoot` to `/userdata/mods/<id>` before
startup to isolate that campaign's saves, portraits, and registry settings in a
separate IDBFS database. IDs use lowercase letters, digits, and single hyphens.
Omitting it preserves the existing `/userdata` database. This setting applies
to browser persistence; native save paths retain their existing behavior.

`tools/package_mods.py SOURCE ASSET_DIR/mods` converts the known campaign RARs
with 7-Zip, copies the existing ZIP, verifies archive contents, and writes
`mods.json` with stable IDs and descriptions. It keeps the source archives and
refuses to overwrite an existing package. The web host's mod picker is in the
sibling `shandalar-dot-dev` repository.

The engine card table is reconstructed from the October 2022 `Magic.exe`;
see [the importer instructions](tools/README.md). Use the matching `Cards.dat`
and supporting resources from the same release.

A lot has been decompiled with AI assistance.  The entire GDI32/Win32 to SDL shim layer is also written almost entirely by AI, as well as the asset streaming code for the Emscripten build.

## Building
Two batch files are available for building the currently tested toolchains.  They expect their respective toolchains to be installed at their default locations.

* `clang.bat` Builds a monolithic 32bit .exe using LLVM and MSVC's STL.
* `clang64.bat` Builds a monolithic x86-64 .exe using LLVM and MSVC's STL.
* `msvc64.bat` Builds a monolithic x86-64 .exe using Visual Studio 2026.
* `emscripten.bat` Builds a 32bit WebAssembly binary.  This is batteries-not-included, so a harness is needed to also serve assets properly.  Currently requires `emsdk_env.bat` to be ran in advance.
* `build-appimage.sh` Builds a batteries included x86-64 AppImage using LLVM.  Requires assets to be provided from a separate directory.

### Local Linux AppImage

Run `./build-appimage.sh` from any directory to build the current checkout and package it as an AppImage in the repository root. It uses game assets from the sibling `shandalar-assets` folder by default; set `SHANDALAR_ASSET_DIR` to use another directory. The script requires CMake 3.24+, Ninja, Clang, Python 3 with Pillow, and curl. It downloads linuxdeploy and its AppImage plugin into `tools-appimage/` on the first run. CMake also fetches source dependencies during its first configure, so network access is needed initially.

The AppImage stores saves, portraits, and settings in `${XDG_DATA_HOME:-$HOME/.local/share}/shandalar/userdata`. Set `SHANDALAR_USER_DATA_DIR` when launching to use another writable location.

## Changes for SDL3/Emscripten
To support compiling with Emscripten several technical changes were necessary:
* Shim GDI32/Win32 to adapt to SDL3.
    * Uses [Dear ImGui](https://github.com/ocornut/imgui) to implement standard Windows controls.
    * Convert `.rc` files to vanilla `.c` code.
    * Uses a `registry.ini` file to emulate Windows registry.
* Avoid using threads, relying instead on Asyncify.  This requires adding explicit yields in several spots.
* Fixes implicit animations (town load zoom, world/lair/dungeon reward "static", deck shuffle effect, etc) via explicit yields.
* Switch dynamic linking/LoadLibrary to static linking.
    * Overrides some shared functions/globals that were originally duplicated.
    * Re-implements "exports"/`GetProcAddress` for deckdll.
    * Explicitly inits dlls since `DllMain` no longer exists.
* Rework Facemaker to avoid subprocess with shared memory via `CreateFileMappingA`.
* Fix text file parsing to be line-ending agnostic.
* Tiered asset loading via `shandalar.data`, `shandalar-assets.data`, and `emscripten_fetch` to provide a fast startup.
* Convert statwin assets to more convenient formats (`.bmp` -> `.png`, `.avi` -> `.webp`+`.wav`).

## Changes for mobile
To support touchscreens (no right click, no keyboard) several UI changes were made:
* Adding buttons to open right click context menus.  Needed in the main adventure UI, deckbuilder, and duel UI.
* Add Yes/No buttons where only Y/N keys are accepted.
* Rework text menus to avoid closing from errant clicks, and increase spacing between items.
* Tweak long press/double click in several spots to act as context menus.
* Switch several actions to trigger on mouse/touch Up rather than Down.
* Add drop target in deckbuilder to show sell value and allow selling cards.

## License

Original contributions to Shandalar SDL3 are licensed under the [MIT License](LICENSE), to the extent the contributors hold rights in them. This grant does not cover the original game's assets or trademarks; their rights remain with their respective owners. Third-party code retains its own licenses.
