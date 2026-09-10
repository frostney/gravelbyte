# Minimal GitHub Pages — 2026-09-10

Current change: feat/minimal-pages, based on main 61d27bb. The page now contains
only the game and toolbar, active-input instructions, GitHub link, PicoSystem
download/setup and device controls. Removed duplicate branding, marketing copy,
course cards, footer, external fonts and the visible build badge/fetch.
Fullscreen preserves the square image with letterboxing on wide screens;
verified visually at 844x390 with touch controls.

Reviewed source diff and visually inspected desktop, phone and landscape output.
Eight Chromium checks pass: existing keyboard/touch/gamepad flows and five
above-fold viewport checks (1366x768, 1280x720, 390x844, 375x667, 844x390).
Smaller viewports retain scrolling rather than clipping instructions or reducing
the game below its usable size. Physical phone/gamepad testing was not repeated.
Native six tests, C++/web formatting and native/WASM/PicoSystem builds pass.
Game source and firmware behavior unchanged; no device flash is needed.

Publish with create-pr and exact-head CI, then the main Pages workflow under
existing session authorization. Consult GitHub for final PR/deployment state.
The original checkout retains its audit handoff; this isolated worktree preserves
that record below. Audit remediation still awaits a selected batch.

# Codebase audit — 2026-09-10

Audited main 61d27bb2066aaa3b2bccbe49386f2bfce66da305 using upstream
codebase-audit at known-good-route 4bb09419189430000711893b7ed10ad7d22c6211.
Audit-only; no remediation, publication, flashing or subagents. This handoff
entry is the sole tracked change, required by the workspace handoff protocol.
Disposable copy, native probes, mutations and local browser server were removed.
Original tracked bytes were verified unchanged before this handoff update.

Findings (final explanation in the task response):
- CA-1 IMPORTANT OPERATIONS: single-sector Pico save erases the only good record
  before programming its replacement; interrupted-write format probes lose unlocks.
- CA-2 IMPORTANT OPERATIONS: unversioned cached assets and separate version.json
  allow old WASM to run while the page claims the new build. Reproduced with real
  old/current artifacts in an isolated browser site; not observed on live deployment.
- CA-3 IMPORTANT BEHAVIOR: missing gravelbyte.js aborts static import before the
  load-error handler; browser remains at Loading game. Missing WASM is handled.
- CA-4 IMPORTANT QUALITY: halved beach targets survive all six native tests;
  stage_drive checks completion under 150 seconds, not beating the actual target.
- CA-5 IMPORTANT OPERATIONS: BENCHMARK_DONE reads the latest renderer.dropped,
  not a race aggregate. An injected overflow resets to zero on the next render.
  Existing hardware logs cannot prove zero dropped geometry across every frame.
- CA-6 IMPROVEMENT ARCHITECTURE_RISK: Renderer::render mixes camera, course/car
  mesh, raster scheduling and all UI. 90-day file history: five touches including
  creation, 1085 additions/117 deletions; all nine repository commits on one day.
  This is a bounded early risk signal, not a long-term churn trend.
- CA-7 IMPROVEMENT BEHAVIOR: canvas menus expose no selected car/track/lock/record
  text through the accessibility tree; data attributes alone do not announce it.

Suggested independent batches: safe saves (CA-1); browser release/loading
(CA-2/3); acceptance checks and telemetry (CA-4/5); renderer separation (CA-6);
accessible menu/status text (CA-7). Await a selected batch before implementing.

Executed: six native tests, six ASan/UBSan tests, three Chromium integration
checks, C++/web formatting, two reversible test mutations, interrupted-save
format probes, render overflow reset, missing JS/WASM and mixed-release browser
flows. Checksum-bypass mutation correctly failed; target mutation did not.
2,547 non-tunnel road samples had no mountain overlap. Live Pages and exact-head
CI verified; origin robots.txt allows crawling. No physical power-cut test, new
hardware benchmark, screen-reader session, mobile device or real gamepad test.

## Prior implementation handoff

# Gravelbyte: palm beach and mountain tunnel

Current update: feat/beach-palms-and-mountain-tunnel, game code 6dab658.
Sunmeadow now has sand, palms, low inland dunes and seaside throughout; names
Palm Shore, Golden Dunes, Sunstone, Tideline. Frostpine tunnel sits within a
broad snowy rock massif, with open portal faces. Road centerlines, handling,
targets, saves and Bracken unchanged. Fixed mountain cache adds 300 bytes.

Final six native and sanitizer tests and three browser tests pass. Visual QA:
output/qa-biomes, including final mountain approach/interior/exit. Review:
output/review-biomes.md. Native public controls finish all nine combinations.
Hardware: three beach runs at b0ab924 and three winter runs at 6dab658 pass:
24,104 frames, 39.499fps, worst25.336ms, zero sub30/recoveries/dropped geometry.
50fps remains unmet. Initial dense mountain missed refreshes; excluded from
acceptance. See docs/biomes-device-results.json for exact hashes and source.
Bracken was not remeasured on hardware in this update.

New full verified backup: output/backups/before-beach-mountain.uf2,
SHA256 334677443d2988dddfcb554a4584897077b0a4d0e1678c678cd8552081e5d3bb.
Settings sector extracted and range-validated as player-settings-before-biomes.uf2.
Normal player restored with flash verification; title-mode telemetry:
output/biomes-player-restored.log. Settings match pre-update backup:
output/biomes-settings-verify.log. CI/Pages publication follows existing flow;
consult live PR/Actions for current state. Do not leave benchmark firmware.

## Previous update context

# Gravelbyte: cinematics and landmark update

Implementation and hardware validation complete on feat/cinematics-and-landmarks.
Final game-code commit: a7195bedd2752575699619769bd224f7258052e8. Subsequent changes
record delivery evidence only. GitHub publication uses the reviewed branch and
the successful-main Pages workflow; consult current PR/Actions state for status.

## Agreed behavior

Keep existing left/right track selector; all tracks browsable, locked tracks
show requirements. Beat previous default target with any car for permanent global
unlock; existing qualifying records count. Bigger car previews, arrows, no counters
or taglines. Title says Press A to continue on PicoSystem, with platform-equivalent
browser prompts, and cycles driving demonstrations across all three environments.

Actual-run cinematic replay after finishing; records hidden by default. X/Space/
Records toggles five gate comparisons against default target and prior best.
A retries; B returns to tracks. Persistent audio toggle: title/pause X, desktop M,
or browser mute. Bridge/solid rails in Bracken, beach/water in Sunmeadow, snowy
mountains/tunnel with solid walls in Frostpine. Roads and handling remain 3D.
See docs/cinematics-and-landmarks.md for implementation constraints and design.

## Verified

Race-entry defect was PICO_BOARD=pico (2MiB limit) asserting on writes to the
PicoSystem's last 16MiB flash sector. Board is now pimoroni_picosystem, protected
by static assertion. Final real-save smoke: five read-back-verified writes,
changed car, countdown, racing, pause/audio/resume. Original saved flash sector
restored after diagnostic writes, then normal CI-built player installed/verified.

Nine hardware races at b431636: 36,867 frames, 39.496fps weighted, worst25.348ms,
zero sub30frames/recoveries/dropped geometry. Final a7195be camera interpolation
change also passed a complete Kite/Frostpine race and sampled playback. 50fps is
still unmet. docs/cinematics-device-results.json distinguishes exact sources and
hashes; do not combine superseded baseline/lod26 logs into accepted evidence.

Six native and ASan/UBSan tests pass, including all nine public-input drives,
old-save fixture migration, progression, replay accuracy and smoothness, all
camera/course views and collision/terrain checks. Three Chromium integration
tests pass for keyboard, simulated multitouch/gamepad, locks and persistent mute.
Physical phones/external gamepads were not used. CI34475290302 passed final code.

Installed CI player SHA256:
7a6b2cc05bc85827a15602d9ce7551cdcefad6973a2533c7e8650563fd32ad63.
Receipts: output/cinematics-player-flash.log, output/cinematics-player-restored.log,
output/settings-restored.log. Normal title has attract driving by design; mode=0
and elapsed=0 distinguish it from a racing benchmark. Never leave diagnostics.

## Local tooling / evidence

Original ../picorally stays untouched. Pascal stays in FemtoPascal PR36.
SDKs and Arm15.3.Rel1: ~/Library/Caches/picorally. Emscripten4.0.23:
~/Library/Caches/gravelbyte/emsdk (source emsdk_env.sh in each shell).
build-pico is normal, build-pico-smoke exercises real saves, build-pico-benchmark
starts all9, build-pico-replay starts diagnostic pair8. Never mix smoke/benchmark.

New full verified backup: output/backups/before-cinematics.uf2
SHA256 371e159d418e3336d22cb6e2acce2222cb088baf2f83ae9779ba27e8a8e8e8d5.
Original settings extracted/range-checked from that backup:
output/backups/original-player-settings.uf2. Preserve all backups and raw logs.

Final nine-run log: output/hardware-cinematics-benchmark.log. Final-code replay:
output/hardware-final-replay.log. Final menu/save: output/hardware-final-menu.log.
Normal CI artifact: output/ci-a7195be/build-pico/gravelbyte.uf2.
Local visual QA: output/qa-update. Review record: output/review-cinematics.md.
