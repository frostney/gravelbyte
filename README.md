# Gravelbyte

A pocket rally game with software 3D roads, hills and loose-surface handling.
Choose a car, pick a course and chase five split times across stages of different lengths.

[Play in your browser](https://frostney.github.io/gravelbyte/) ·
[Download PicoSystem firmware](https://frostney.github.io/gravelbyte/gravelbyte.uf2)

| Car | Difficulty | Character |
| --- | --- | --- |
| Finch 1300 | Easy | Compact, lower top speed, stable grip |
| Kestrel GT | Standard | Original demanding rally hatch handling |
| Goshawk Turbo | Expert | Faster, lower coupe with more lateral slip |

Bracken Ridge crosses a river bridge through the forest. Sunmeadow Run follows
a palm-lined sandy beach beside coastal water. Frostpine Pass winds through snowy mountains
and a tunnel, with visible blue ice reducing traction. Every car/course pair has bronze, silver and gold targets, with separate
assisted and unassisted personal records. Higher DRIFT means the car slides more readily.

The car selector shows a rotating model, name, difficulty and three stat bars.
No selection counters or numeric stat fractions. Passed progress markers use the
same green/red result colour as the unobtrusive signed split popup.

Browse all three tracks with Up/Down; the selected course shows a route map and
five checkpoints. Earn bronze with any car, with or without assist, to unlock the
next course for all cars. Personal bests never raise the unlocking requirement.
Finch has the most generous bronze target. X toggles light countersteering in the
car selector (Space on keyboard); it is off by default and never follows the road.
Bracken is a 1.78km technical stage with tightening corners, a double apex and a
narrow bridge. Sunmeadow is a 1.33km coastal sprint with a sharp chicane and sand
mid-corner. Frostpine is a 2.37km endurance stage with switchbacks, ice and a
mountain tunnel. Each has its own five checkpoint positions and authored elevation.

The title cycles through driving demonstrations with tracking, roadside and
raised cameras. Press A on PicoSystem (Enter on keyboard, Go on touch) to continue.
After finishing, the actual run replays with records hidden. X toggles the five
gate comparisons against the default target and the prior personal best; Space
on keyboard or Records on touch does the same. A retries, B returns to tracks.
X toggles sound on the title (M on keyboard). Pause opens Resume, Restart,
Options and Courses; Up/Down selects and A confirms. Options contains sound and
pace notes. The browser also has a mute icon. These settings persist.
Pace notes announce the next corner: 1 is tightest and 6 fastest, with tightening,
double-apex, crest, narrow-bridge and tunnel warnings. The first countdown teaches
drift. Engine pitch steps through gears and buzzes at the limiter. The PicoSystem
LED pulses amber during countdown, flashes green/red at splits, and white at finish.

## Play

The browser supports keyboard, standard gamepads and simultaneous touch controls.
Hints follow the active input; touch controls appear only for touch play.
Keyboard instructions use labelled keycaps. Sound and GitHub use icons; press
F for fullscreen. Car/track choices and lock requirements are announced through
an accessible status region, with native menu buttons available to keyboard and
assistive-technology users. Nonvisual racing itself has not been established.
Start with Finch 1300; brake before tight bends, release as you turn, and drift sparingly.

| Action | Keyboard | Standard gamepad | PicoSystem |
| --- | --- | --- | --- |
| Steer / choose car | Left / Right | D-pad / left stick | Left / Right |
| Choose course / menu option | Up / Down | D-pad / left stick | Up / Down |
| Toggle steering assist (car selector) | Space | X | X |
| Gas | Up / Z | RT / A | A |
| Brake / reverse | Down / X | LT / B | B |
| Drift | Space | X | X |
| Confirm / retry | Enter / Z | A | A |
| Back / change selection | Esc | B | B |
| Pause / resume | P | Start | Y |

Browser input is captured while the game has focus. Leaving the tab or window
pauses racing; resume explicitly. Start play to enable audio. Browser records
and the last chosen car/course are saved locally; unavailable storage leaves
session-only records. Device records are independent. The redesigned courses use
a new save format; earlier times and settings are intentionally discarded.

To install on PicoSystem, connect USB, hold **X** while switching it on, then copy
`gravelbyte.uf2` onto **RPI-RP2**. The device restarts automatically.

## Build

C++17 and CMake are shared across native SDL2, Emscripten and PicoSystem builds.

```sh
# Native preview and contracts (install SDL2 first)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
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
The package-web target copies static files on every web build. Release assets
are grouped beneath assets/<commit>/; the loader checks the executable build ID.
Reconfigure after changing commits so the embedded build ID is refreshed.
Native macOS output is `build/gravelbyte.app`; Linux output is `build/gravelbyte`.
PicoSystem output is `build-pico/gravelbyte.uf2`.

CI checks formatting, native contracts, all nine complete drives, terrain
clearance and browser integration. Successful main builds publish GitHub Pages;
the playable WASM, downloadable UF2, version and checksums come from one commit.
The stable gravelbyte.uf2 download URL remains available. SHA256SUMS lists the
versioned asset paths. Pico saves alternate between two flash sectors, preserving
the previous valid save during an erase or write. Old save formats are rejected.
Pull requests build/test without deployment.

## Design and validation

The renderer uses a 120×120 RGBA4444 framebuffer and a depth buffer. Only the
selected course's terrain is generated. Car shadows change the material of the
existing ground triangles, so they follow slopes without a coplanar overlay.

The PicoSystem runs at the SDK display cadence around 40fps, with a 30fps minimum
during normal racing and the default clock unchanged. Host
tests cannot establish device performance. See [validation](docs/validation.md)
for measured results and [target calibration](docs/targets.md) for reference runs.

`-DGRAVELBYTE_BENCHMARK=ON` builds an automated nine-race device diagnostic that
never writes records or ghosts. It loads matching stored ghosts when present and
reports the number of frames containing ghost geometry. Benchmarks enable audio
regardless of the saved mute preference unless built with
`GRAVELBYTE_TEST_SILENT=ON`; each result identifies its audio workload. `-DGRAVELBYTE_BENCHMARK_START=6` starts at the winter
course for a focused rerun. Restore the player build after benchmarking. Never flash
until the intended device is identified and a full flash backup is verified.
`-DGRAVELBYTE_SMOKE=ON` builds a separate real-save diagnostic: it toggles sound,
changes car, starts a race, changes sound, ghost and pace-note options while paused,
and resumes. It completes a driven run, stores a new best replay if earned,
and restarts to check that the ghost loads. It prints
`SAVE_OK` after record verification, `SMOKE_DONE` after reaching racing,
`GHOST_SAVE_OK` after replay verification, `GHOST_RELOAD` after restarting,
and `GHOST_LOADED` when a saved replay is available after boot.
Do not combine the two diagnostic options. Always restore the normal player.
The board is explicitly `pimoroni_picosystem`: the generic Pico's 2MiB flash
limit would assert when saving in the PicoSystem's final 16MiB flash sector.

Reusable simulation, camera, recording, landmark and layout values are grouped
in `src/tuning.hpp`. Authored road bends, car profiles and targets remain data.
Replays use a fixed 24KiB pose buffer; very long runs progressively reduce the
sampling frequency instead of exhausting device memory. Personal-best replays persist and can appear as ghosts; finish replays remain available during the session. See [replays and challenges](docs/replays-and-challenges.md).

[Reference art](assets/reference/car-track-reference.png) and its exact
[ImageGen prompt](assets/reference/car-track-prompt.md) are retained as design
references. The game renders its own geometry; the reference is not a screenshot.
The Pascal experiment belongs to [FemtoPascal](https://github.com/frostney/FemtoPascal).

## Code style

Use descriptive PascalCase identifiers in owned C++ (`GravelByte` namespace),
and descriptive camelCase bindings and properties in browser JavaScript.
Platform entry points and external API names retain their required spelling.
Run `python3 tools/check_cpp.py`
for naming checks, alongside the existing C++ and web format checks. CI enforces
the C++ naming rules. Keep save layouts and serialized keys compatible when
renaming fields.

## Development controls and checks

Run `npm ci` to install Lefthook. Its pre-commit checks run clang-tidy against
the native compilation database (including all shared headers), clang-format,
and Prettier. Install clang-tidy, pinned clang-format 23.1.0, CMake and SDL2 first.
Platform adapters are additionally checked by their PicoSystem/WASM builds.
C++ uses descriptive PascalCase identifiers in namespace `GravelByte`; JavaScript
uses descriptive camelCase bindings and properties. External API names retain
their required spelling.

Configure `-DGRAVELBYTE_DIAGNOSTICS=ON` for the developer FPS overlay: Up+Y on
PicoSystem or F1 on desktop. Release builds omit these shortcuts.

The pinned PicoSystem SDK waits for panel VSYNC, giving about 39.5 displayed
frames per second. We accept that cadence, keep its default 250MHz clock, and
require at least 30fps during normal racing. Telemetry separates rendering,
SDK update time, buffer-swap time, elapsed display transfer and SDK wait percent.
`flip_elapsed_us` is elapsed transfer time observed after update, not CPU work;
`wait_percent` is busy-wait opportunity, not a battery-life measurement.

## Route variants, practice and challenges

Bronze on an original course unlocks its reverse, mirrored and reversed-mirrored
routes. Left/right selects a variant in the track menu. Each car/route/assist
combination has its own records and personal-best ghost. Pause > Options toggles
sound, ghost and pace notes. Pause > Retry last split starts practice from the
last checkpoint; practice cannot earn records or medals. Restart restores a full
eligible run.

Choose Challenge for a random stage. X/Space/New generates another; left/right
opens the shared eight-digit seed editor. Use left/right to choose a digit and
up/down to change it, then A/Enter/Go to confirm. The same seed works on handheld
and web. Browser URLs carry the seed for sharing. Challenge bests are session
only, with no campaign unlocks or medals.

Use `-DGRAVELBYTE_TEST_SILENT=ON` for silent local PicoSystem/desktop testing,
or `GRAVELBYTE_TEST_SILENT=1 npm run test:web` to mute browser test output.
The track selector previews the selected scenery behind the checkpoint map.

`-DGRAVELBYTE_BENCHMARK_EXTENDED=ON` selects six additional diagnostic runs:
forest reverse, beach mirror, winter reverse + mirror, then challenge seeds
00000000, 80000000 and ffffffff (all with Goshawk Turbo). Combine it with
`GRAVELBYTE_BENCHMARK=ON`; the start index is 0–5 for this set.
