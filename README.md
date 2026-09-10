# Gravelbyte

A pocket rally game with software 3D roads, hills and loose-surface handling.
Choose a car, pick a course and beat five split times across a 1.8km stage.

[Play in your browser](https://frostney.github.io/gravelbyte/) ·
[Download PicoSystem firmware](https://frostney.github.io/gravelbyte/gravelbyte.uf2)

| Car | Difficulty | Character |
| --- | --- | --- |
| Mica | Easy | Compact, lower top speed, stable grip |
| Torr | Standard | Original demanding rally hatch handling |
| Kite | Expert | Faster, lower coupe with more lateral slip |

Bracken Ridge keeps the original forest course. Sunmeadow Run adds open summer
countryside and fast crests. Frostpine Pass adds snow, mountain bends and visible
blue ice with reduced traction. Every car/course pair has its own default target
and personal record. Higher DRIFT means the car slides more readily.

The car selector shows a rotating model, name, difficulty and three stat bars.
No selection counters or numeric stat fractions. Passed progress markers use the
same green/red result colour as the unobtrusive signed split popup.

## Play

The browser supports keyboard, standard gamepads and simultaneous touch controls.
Hints follow the active input; touch controls appear only for touch play.

| Action | Keyboard | Standard gamepad | PicoSystem |
| --- | --- | --- | --- |
| Steer / select | Left / Right | D-pad / left stick | D-pad |
| Gas | Up / Z | RT / A | A |
| Brake / reverse | Down / X | LT / B | B |
| Drift | Space | X | X |
| Confirm / retry | Enter / Z | A | A |
| Back / change selection | Esc | B | B |
| Pause / resume | P | Start | Y |

Browser input is captured while the game has focus. Leaving the tab or window
pauses racing; resume explicitly. Start play to enable audio. Browser records
and the last chosen car/course are saved locally; unavailable storage leaves
session-only records. Device records are independent. Existing course-v3 C++
Bracken Ridge records migrate to Torr only.

To install on PicoSystem, connect USB, hold **X** while switching it on, then copy
`gravelbyte.uf2` onto **RPI-RP2**. The device restarts automatically.

## Build

C++17 and CMake are shared across native SDL2, Emscripten and PicoSystem builds.

```sh
# Native preview and contracts (install SDL2 first)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

# Owned C++ source formatting
python3 -m pip install clang-format==23.1.0
cmake -S . -B build
cmake --build build --target format
cmake --build build --target format-check

# Browser (activate Emscripten 4.0.23 in this shell first)
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
python3 -m http.server 8173 --directory build-web/site

# Web formatting and browser integration tests
npm ci
npm run format:web:check
# Use npm run format:web to apply the pinned Prettier style.
npx playwright install chromium
npm run test:web

# PicoSystem (install arm-none-eabi GCC first)
export PICO_SDK_PATH=/path/to/pico-sdk
export PICOSYSTEM_DIR=/path/to/picosystem
cmake -S . -B build-pico -DGRAVELBYTE_DEVICE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-pico -j
```

CI pins Arm GNU 15.3.Rel1 (GCC 15.3.1), matching the device measurements.
Pinned SDK revisions and build steps live in `.github/workflows/build.yml`.
Run CMake again after editing static site files to copy them into the web build.
Native macOS output is `build/gravelbyte.app`; Linux output is `build/gravelbyte`.
PicoSystem output is `build-pico/gravelbyte.uf2`.

CI checks formatting, native contracts, all nine complete drives, terrain
clearance and browser integration. Successful main builds publish GitHub Pages;
the playable WASM, downloadable UF2, version and checksums come from one commit.
Pull requests build/test without deployment.

## Design and validation

The renderer uses a 120×120 RGBA4444 framebuffer and a depth buffer. Only the
selected course's terrain is generated. Car shadows change the material of the
existing ground triangles, so they follow slopes without a coplanar overlay.

The PicoSystem target is 50fps with a 30fps minimum during normal racing. Host
tests cannot establish device performance. See [validation](docs/validation.md)
for measured results and [target calibration](docs/targets.md) for reference runs.

`-DGRAVELBYTE_BENCHMARK=ON` builds an automated nine-race device diagnostic that
never writes records. `-DGRAVELBYTE_BENCHMARK_START=6` starts at the winter
course for a focused rerun. Restore the player build after benchmarking. Never flash
until the intended device is identified and a full flash backup is verified.

[Reference art](assets/reference/car-track-reference.png) and its exact
[ImageGen prompt](assets/reference/car-track-prompt.md) are retained as design
references. The game renders its own geometry; the reference is not a screenshot.
The Pascal experiment belongs to [FemtoPascal](https://github.com/frostney/FemtoPascal).
