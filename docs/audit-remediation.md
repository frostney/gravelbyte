# Audit remediation and failure-path validation

Scope: the seven findings from the September 10 codebase audit, plus the reported
off-road flight, page controls, human difficulty feedback and search metadata.
The handoff is local/ignored and no longer tracked. See playtesting.md and seo.md
for the external validation boundaries.

| Finding | Remedy | Evidence |
| --- | --- | --- |
| CA-1 save durability | Two flash-sector slots; validate payload/sequence/checksum, write only the inactive slot, verify readback, preserve the legacy sector during first migration | Shared writer exercised at 13,059 erase/program interruption points; corruption, sequence wrap, legacy migration and unchanged-write tests |
| CA-2 release consistency | HTML, app, CSS, WASM and firmware use one commit-specific asset path; loader compares the executable's embedded build ID | Mismatched metadata and an actual previous-release WASM are rejected before Play is enabled; package paths/download/checksums verified over HTTP |
| CA-3 loading failures | Catch the dynamic engine import; report main-script and WASM failures | Missing app.js, gravelbyte.js and gravelbyte.wasm all produce an actionable error |
| CA-4 acceptance value | Each public-input reference run must beat its actual target; real preceding runs establish persistent progression | All nine combinations complete; a target-halving mutation fails with the actual/target times |
| CA-5 telemetry | Accumulate dropped geometry and overflowing frames across every racing render | An earlier overflow survives clean later frames in the shared accumulator; BENCHMARK_DONE reports total drops, overflow frames and rendered frames |
| CA-6 renderer boundaries | Separate camera/shadow setup, background, road, scenery, track objects, car and UI; retain fixed buffers and face order | 60 sampled frames are byte-identical before/after the render-only refactor |
| CA-7 accessible selection | Announce car statistics, track locks/targets and results; native menu controls support keyboard/assistive navigation | Native-button flow from title through car/track selection, locked-stage refusal and race entry; status/labels checked in the accessibility tree |

## Off-road bug

The old road-relative ground formula diverged from the drawn triangles on curved
banks and finite terrain edges. Public-control excursions produced support-speed
spikes above 375m/s. Off-road support now samples the actual terrain triangles,
steep uphill steps collide instead of lifting the car, unsupported terrain
recovers the car, downward steps fall under gravity, and collision corrections
resample support. The query also follows the right bank's reversed triangle
diagonal; a non-planar-bank regression and mutation check cover this distinction.
Support speed is bounded; authored road jumps are retained.

180 public-input perturbation runs cover all cars, both steering directions and
ten regions on each track, including hills, bridge/tunnel areas and crests.
Checks cover finite state, visible terrain support, bounded vertical motion and
geometry capacity in sampled off-road views. Max airborne clearance in these
runs is 1.245m. Restoring the old support lookup fails the regression. These
perturbations validate failure recovery behavior, not human driving skill.

## Validation and limits

Seven native and seven ASan/UBSan suites pass. Sixteen Chromium checks cover
keyboard, simulated multitouch/gamepad, focus loss, denied/corrupt storage,
missing/mismatched assets, native accessible menus, five above-fold sizes and
static metadata/download without JavaScript. C++ and web formatting pass.
Native, WASM, player and diagnostic PicoSystem builds pass.

Audio-enabled device coverage includes all nine car/course combinations:
36,870 racing frames, 39.497fps weighted mean, and a worst frame of
25.348ms. Every accepted run has zero sub-30fps frames, recoveries,
aggregate dropped triangles and overflowing frames. The 30fps minimum is met in
these runs; the 50fps target remains unmet. Six dry runs are retained from the
full audio benchmark; the three snow races were repeated after a snow-only view
distance reduction from 144m to 120m. Forty dry captures remain byte-identical.
[Exact results, scoped source hashes and artifact identities](audit-device-results.json)
include the rejected earlier snow timings separately.

A real-device smoke run verified five alternating journal writes and successful
selection/countdown/pause/audio/resume transitions. Both slot checksums and
sequences were checked from flash. Original save sectors were restored and
verified, and the normal player was reinstated at its title screen. Full flash
backups and raw telemetry remain local under output/. The host interruption
models do not substitute for physical power-cut testing.
Physical interruption during erase/program, a real screen-reader session and
additional human playtests are not simulated as completed evidence. Human
acceptance remains provisional; the player's inability to beat a stage motivated
a larger Mica allowance. Torr/Standard's Bracken target and road physics remain
compatible with existing course-v3 records.
