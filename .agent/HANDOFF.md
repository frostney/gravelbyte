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
