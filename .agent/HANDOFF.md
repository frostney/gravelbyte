# Gravelbyte implementation handoff

## Scope / decisions

Grilling Q1-Q8 complete. User approved three cars, distinct summer/winter tracks,
formatter, WASM, touch/keyboard/gamepad, local records and successful-main Pages
auto-deploy. Minimal car selector: no 1/3 or 3/5 counters. New courses are
Sunmeadow Run and Frostpine Pass. Shadow flicker is an acceptance issue.

Original ../picorally remains read-only and untracked. Only C++ src/tests and
one telemetry tool imported into this newly initialized empty frostney/gravelbyte
repository. Pascal reference remains in FemtoPascal draft PR36.

## Implemented

Three cars (Mica/Torr/Kite), three active-generated courses, 9 record/target sets,
legacy migration, menu flow, platform hints, minimal stat bars, seasonal terrain,
localized ice. Emscripten bridge, responsive static site, browser localStorage,
multi-pointer touch and gamepad. Pinned formatter and CI/Pages workflow.
ImageGen reference and exact prompt retained.

## Current work

Native tests / browser visual QA / nine-race PicoSystem benchmark in progress.
Initial world-mesh shadow split caused ~50ms frames and T-junction cracks. Replaced
with per-ground-triangle material masks; original ground geometry/depth remains.
Final shadow path needs fresh hardware matrix after flash.

Device E46024C7430C442A is connected. Full flash backup made and verified:
output/backups/before-gravelbyte.uf2
SHA256 a05beb5195f6715e3195aaf72a72241e09b9b82a9c53d30d3060978f751b8ed5.
Keep backup and receipts. Current device runs diagnostic firmware; restore player
firmware after measurements. Telemetry port /dev/cu.usbmodem1101.
SDKs and ARM15.3.1 toolchain under ~/Library/Caches/picorally.
Emscripten4.0.23 installed at ~/Library/Caches/gravelbyte/emsdk; source its env
for EVERY shell invocation (system Python otherwise too old for emcc).

Local web server port8173 serves build-web/site. Node browser binding browser,
tab in current REPL. Browser selection restores Kite/Frostpine after reload;
keyboard, pause/resume and screenshots checked. Read browser skill before QA.

Next: finish native shadow tests, flash material-mask benchmark, run CI browser
suite, publish tested Pages with matching UF2, full device results, restore player,
update docs/handoff. Do not claim 50fps or complete delivery from host tests.
