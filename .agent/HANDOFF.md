# Gravelbyte handoff

## Delivered

C++ Gravelbyte is published at https://github.com/frostney/gravelbyte and playable
at https://frostney.github.io/gravelbyte/. Matching UF2 is linked on that page.
The verified published player firmware is installed on the connected PicoSystem;
USB telemetry confirms Title mode, no diagnostic auto-driving remains.

Grilling Q1-Q8 completed. Three cars Mica/Torr/Kite; Bracken Ridge, Sunmeadow Run,
Frostpine Pass; five splits, default targets and separate records for all nine
pairs. Minimal car selection has unnumbered speed/acceleration/drift bars.
Touch/keyboard/gamepad browser hints follow the active input. Browser records and
selections persist locally. Native/device legacy course3 records migrate to Torr.
C++ clang-format23.1.0 and web Prettier3.9.6 are pinned and enforced in CI.

## Validation / measured limits

Native Release and ASan/UBSan tests pass: all nine public-input complete drives,
terrain clearance, record corruption/migration/isolation, shadow coverage and
nearby winter tree visibility. Browser CI covers keyboard, gamepad, multitouch,
pause/resume, reload and denied storage. Hosted keyboard play and desktop/mobile
layout checked manually; no physical phone or external gamepad testing claimed.

Device evidence: 36,877 racing frames over all nine car/course combinations,
39.50fps weighted mean, worst25.342ms, zero sub30frames/recoveries/dropped geometry
in accepted runs. Six dry-course runs at d031f4b; three winter runs repeated at
0e08624 after changes confined to winter vegetation. Full detail and hashes in
docs/validation.md and docs/device-results.json. 30fps minimum met; 50fps remains
an open target. Do not claim hardware50fps from host or screenshot measurements.

The shadow is a clipped material mask shaded per scanline on original ground
triangles, avoiding both coplanar flicker and projected T-junction cracks.
Winter uses full nearby conifers, conservative offscreen rejection and light
camera-facing two-tier geometry beyond84m. Roads and terrain remain 3D.

## Build / local evidence

CI pins Arm GNU15.3.Rel1/GCC15.3.1, Emscripten4.0.23 and both Pico SDK revisions.
Implementation CI/deployment: actions run34455644523, commit0e08624. Later evidence
and documentation commits contain no further game changes.
Local SDKs/ARM tools: ~/Library/Caches/picorally.
Emscripten: ~/Library/Caches/gravelbyte/emsdk (source emsdk_env.sh in each shell).
Local benchmark build caches GRAVELBYTE_BENCHMARK_START=6 for winter reruns;
set0 to run all nine. Normal build uses GRAVELBYTE_BENCHMARK=OFF.

Verified full pre-flash backup: output/backups/before-gravelbyte.uf2
SHA256 a05beb5195f6715e3195aaf72a72241e09b9b82a9c53d30d3060978f751b8ed5.
Keep backups and all raw logs. Dry evidence: output/hardware-dry-courses.log
(track<2 final; winter entries there are obsolete). Final winter evidence:
output/hardware-winter.log. Player receipt: output/player-restored.log and
output/player-flash.log. Published UF2/manifest: output/release/.
Source ../picorally remains unchanged. Pascal reference remains in FemtoPascal PR36.
