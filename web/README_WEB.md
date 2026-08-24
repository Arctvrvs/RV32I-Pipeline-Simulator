# RV32 Web Simulator

The browser frontend drives the same C++ pipeline core as the native applications through an Emscripten-generated WebAssembly module.

## Prerequisites

- Emscripten SDK 3.1.60 or newer, activated in the current shell
- CMake 3.15 or newer
- Python 3 to serve the files locally

## Build and serve

From the repository root:

```bash
./web/build_wasm.sh
```

On Windows PowerShell:

```powershell
.\web\build_wasm.ps1
```

Both scripts configure a release build, generate `web/sim.js`, and start an HTTP server at `http://localhost:8000`.

The CMake target uses Emscripten's `SINGLE_FILE` option, so the WebAssembly payload is embedded in `sim.js`; a separate `sim.wasm` file is not required.

## Manual build

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --parallel
cd web
python3 -m http.server 8000
```

The page must be served over HTTP because it loads `sim.js` as an ES module.

## Programs and reset behavior

- **Load example** selects one of the bundled `.hex` programs.
- **Load file** accepts a local `.hex` or little-endian ELF32 file.
- **Reset** clears CPU state and restores the program and memory image captured when the file was loaded.

Uploaded files are written to Emscripten's in-memory filesystem and passed to the shared C++ `load_program()` loader.

## GitHub Pages

The repository's `.github/workflows/pages.yml` workflow builds `sim.js` and deploys the complete `web/` directory after a push to `main`. Enable **Settings → Pages → Source → GitHub Actions** after creating the GitHub repository.
