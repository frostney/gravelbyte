# Active: cinematics, progression and track landmarks

Branch: feat/cinematics-and-landmarks, based on origin/main 4ee4205.
User confirmed all grilling decisions, including retaining existing selector,
locked browsing/global unlocks, actual-run replay hidden records, persistent
sound, larger cars/arrows, no taglines, forest bridge/summer beach/winter tunnel.
See docs/cinematics-and-landmarks.md for the approved brief and architecture.

Implementation complete locally; delivery and full device measurement pending.
Confirmed race-entry bug: generic Pico 2MiB flash configuration asserted on save
at the PicoSystem's 16MiB final sector. Board fixed and compile-time protected.
Hardware real-save smoke passed: five verified writes, countdown and racing,
pause/audio/resume. output/hardware-menu-smoke.log. Original firmware reported
pico board; actual device has 16MiB flash, ID E46024C7430C442A.

New verified full backup: output/backups/before-cinematics.uf2
SHA256 371e159d418e3336d22cb6e2acce2222cb088baf2f83ae9779ba27e8a8e8e8d5.
Running diagnostic capture: output/hardware-cinematics-benchmark.log (all nine).
Final benchmark candidate: output/firmware/cinematics-benchmark.uf2
SHA256 483c04768cab36d7b6dfdd7bc9593ab50613880feb92bfc652033b941be26cb3.
Constants cleanup rebuilt byte-identically. Earlier baseline/lod26 logs had missed
frames and must not be counted as final acceptance. Current 24-node view candidate
has passed Mica/Bracken so far; remaining runs in progress. Check current log.

Next: finish all nine, address any frame misses, validate cinematic playback on
device, restore verified NORMAL player firmware, complete CI/PR/publication and
update validation evidence. NEVER end with the automated diagnostic installed.
Native six tests and browser three tests pass; sanitizer rerun is in progress.
Browser local QA uses localhost8173. Save format remains 200 bytes with v1 migration.
Pico BSS including replay ~232KiB; 50fps remains unmet.

---


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
