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

Native Release, ASan/UBSan, browser visual QA and CI browser integration pass.
Site is live at https://frostney.github.io/gravelbyte/. CI builds browser and
UF2 together using pinned Arm15.3.Rel1, Emscripten4.0.23, Pico SDK2.2.0 and
PicoSystem revision9a26b2a. C++ clang-format23.1.0 and web Prettier3.9.6 checked.

Final shadow shades original ground triangles with a clipped material mask;
scanline mask intervals avoid per-pixel polygon tests and overlapping surfaces.
First six dry-course hardware runs passed: ~39.5fps, no sub30frames/recoveries.
output/hardware-dry-courses.log retains these six and an obsolete winter baseline;
only track<2 entries are final for that file. Initial winter path missed refresh.
Winter now uses full nearby trees, conservative offscreen rejection, and light
camera-facing two-tier models beyond84m. New winter3-car runs are in
output/hardware-winter.log. Do not mix superseded baseline winter results in.

Verified full pre-flash backup: output/backups/before-gravelbyte.uf2
SHA256 a05beb5195f6715e3195aaf72a72241e09b9b82a9c53d30d3060978f751b8ed5.
Keep backup/receipts. Device currently runs diagnostic firmware; restore player
firmware after measurements. Telemetry port /dev/cu.usbmodem1101.
SDKs and ARM15.3.1 toolchain under ~/Library/Caches/picorally.
Emscripten4.0.23 under ~/Library/Caches/gravelbyte/emsdk; source env EACH shell.
Local benchmark build is configured GRAVELBYTE_BENCHMARK_START=6 for winter only.

Local web server8173 serves build-web/site. Browser bindings browser/tab(local)
and live(hosted). Mobile viewport reset after QA. Browser selection persistence,
keyboard, pause/resume checked manually; CI covers touch contacts and gamepads.

Next: finish winter device checks, latest CI/Pages verification, restore and verify
player UF2 on hardware, update validation evidence and this handoff. No 50fps claim;
the SDK's display cadence is ~39.5fps. Original ../picorally remains unchanged.
