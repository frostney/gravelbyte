> Current acceptance: around 40fps at the unchanged default clock, with a 30fps
> minimum. Historical 50fps comparisons below record the former aspiration.

# Validation

For the car-name migration and latest shadow fix, see
[naming and shadow validation](naming-and-shadows.md). Earlier captures below
retain the car labels and implementation details of their measured revisions.

## Audit, failure paths and accessible page update

Seven native and seven ASan/UBSan suites pass, including 180 public-input off-road
excursions, reversed terrain-diagonal coverage and interrupted save writes.
Sixteen Chromium checks cover controls, accessible menus, missing/mixed releases,
storage failure, compact layouts and JavaScript-disabled fallback. See the
[audit remediation](audit-remediation.md), [human playtest record](playtesting.md)
and [search metadata checks](seo.md) for details and explicit limits.

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

The sections below retain historical evidence for previous updates.

## Palm beach and mountain tunnel update

The final renderer passed six native tests, six ASan/UBSan tests and three Chromium
integration tests. Native drives cover all nine car/course combinations. Visual
checks cover palm silhouettes, beach racing and cinematic views, and the tunnel
approach, opening, interior and exit. Formatting checks pass.

Three beach and three winter device races cover **24,104 frames**, averaging
**39.499fps**, with a worst frame of **25.336ms**. All six have zero
sub-30fps racing frames, recoveries and dropped geometry. The 50fps target is
still unmet. [Measurements and exact firmware hashes](biomes-device-results.json)
separate the beach source from the later winter-only mesh optimization. The
initial dense mountain missed refreshes and is excluded from acceptance.

The final mountain caches 300 bytes of geometry; player BSS is 232,436 bytes.
A fresh full 16MiB backup was made and flash-verified before diagnostics.


## Cinematics and landmarks update

The expanded update passes six native contracts, ASan/UBSan and three Chromium
integration tests. These cover progression, old-save migration, persistent mute,
recorded-run replay, camera movement, physical landmarks and full stage drives.
Visual checks include the larger car menu, locked-track preview, each landmark,
and replay with the records panel hidden and visible.

The generic-Pico flash configuration caused the real-player race-entry failure.
The corrected PicoSystem board configuration passed a hardware smoke test with
five verified flash writes and selection/countdown/pause/audio/resume transitions.
See [the design and defect analysis](cinematics-and-landmarks.md).

All nine combinations completed on-device at source `b431636`, with **36,867
racing frames**, **39.496fps weighted mean**, **25.348ms worst frame** and no
sub-30fps frames, recoveries or dropped geometry. Earlier expanded-renderer
candidates missed refreshes and are retained as superseded diagnostic logs.
This meets the 30fps racing minimum; it does not meet the 50fps target.

| Course | Car | Race time | Frames | Mean fps | Worst frame | Below 30fps |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Bracken Ridge | Mica | 106.116s | 4191 | 39.50 | 25.345ms | 0 |
| Bracken Ridge | Torr | 110.370s | 4359 | 39.50 | 25.339ms | 0 |
| Bracken Ridge | Kite | 106.881s | 4221 | 39.50 | 25.341ms | 0 |
| Sunmeadow Run | Mica | 92.294s | 3645 | 39.50 | 25.341ms | 0 |
| Sunmeadow Run | Torr | 95.820s | 3784 | 39.50 | 25.343ms | 0 |
| Sunmeadow Run | Kite | 92.912s | 3668 | 39.49 | 25.348ms | 0 |
| Frostpine Pass | Mica | 106.617s | 4210 | 39.49 | 25.346ms | 0 |
| Frostpine Pass | Torr | 112.817s | 4455 | 39.50 | 25.341ms | 0 |
| Frostpine Pass | Kite | 109.751s | 4334 | 39.50 | 25.339ms | 0 |

[Update measurements and firmware hash](cinematics-device-results.json) identify
the exact nine-run build. The later `a7195be` change only smooths the cinematic
camera and clarifies its touch hint; it does not alter racing physics or the
normal chase camera. The final-source Kite/Frostpine run completed in 109.753s over 4,333 frames, again with zero sub-30fps frames, recoveries or dropped geometry. Post-finish playback samples showed changing recorded positions at a maximum sampled tick of 25.333ms; this is sampled playback evidence, not a count of every replay frame.

## Update delivery

[Final game-code CI](https://github.com/frostney/gravelbyte/actions/runs/34475290302)
passed at `a7195be`. The CI-built player was installed with flash verification
after restoring the original saved settings sector from the verified pre-change
backup. USB telemetry confirms normal title mode, with attract driving and a
zero race timer. Both real-save smoke runs verified five writes and successful
selection/countdown/pause/audio/resume transitions.

CI player SHA256: `7a6b2cc05bc85827a15602d9ce7551cdcefad6973a2533c7e8650563fd32ad63`.
The main-branch workflow publishes matching browser and UF2 artifacts after its
checks pass. Local SDK builds have different build-path metadata from CI; keep
the individual firmware hashes with their measurement records.

## Previous baseline

Everything below records the earlier delivery, before cinematics and landmarks.
It is retained as historical evidence, not as certification of this update.

This file records acceptance evidence and outstanding checks. Native checks are
not a substitute for PicoSystem telemetry or physical touchscreen/gamepad testing.

- All nine car/course combinations complete through public controls, with five
  ordered splits, no recovery, bounded airborne height and no triangle overflow.
- Bracken Ridge / Torr retains the original physics and course geometry.
- Terrain clearance covers 17,940 positions across the three road corridors.
- Save tests round-trip all nine records and selections, reject corruption at
  every byte, and check legacy migration and record isolation.
- Shadow tests cover slopes and rotated car headings, ground coverage and
  foreground occlusion. Shadows use the original ground triangles' depth.
- Browser integration covers keyboard, emulated touch contacts, standard-gamepad
  input, active-input hints, pause/resume, selection persistence and denied storage.
- Native Release and AddressSanitizer/UndefinedBehaviorSanitizer checks pass.
- CI and Pages deployment passed for the browser/input/persistence implementation.
- All nine car/course combinations completed on PicoSystem, totalling 36,877
  racing frames at 39.50fps, with no sub-30fps frames, recoveries or reported
  triangle overflow in the accepted runs. Maximum frame time: 25.342ms.
- The 30fps minimum is met in these runs; the 50fps target remains unmet.
- Winter trees use conservative visibility rejection, camera-facing distant
  branch tiers beyond 84m and full nearby models. Visibility tests retain the
  complete nearby silhouettes from low, normal and elevated viewpoints.

## Shadow investigation

The original shadow applied road slope in car-local coordinates and used a
floating plane above the road. That plane intersected slopes during yaw changes.
An initial mesh-partition fix removed the overlap but introduced extra geometry
work and one-pixel cracks at projected T-junctions. The final approach clips a
material mask in world space and shades the original ground triangle once.
It creates neither a depth overlap nor new ground geometry edges.

Raw hardware logs and verified full flash backups are local under `output/`;
they are excluded from Git. The public result summary is updated after completion.

## Device results

| Course | Car | Race time | Frames | Mean fps | Worst frame | Below 30fps |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Bracken Ridge | Mica | 106.210s | 4195 | 39.50 | 25.341ms | 0 |
| Bracken Ridge | Torr | 110.353s | 4358 | 39.50 | 25.342ms | 0 |
| Bracken Ridge | Kite | 106.870s | 4222 | 39.51 | 25.333ms | 0 |
| Sunmeadow Run | Mica | 92.273s | 3645 | 39.51 | 25.333ms | 0 |
| Sunmeadow Run | Torr | 95.835s | 3785 | 39.50 | 25.339ms | 0 |
| Sunmeadow Run | Kite | 92.925s | 3670 | 39.50 | 25.335ms | 0 |
| Frostpine Pass | Mica | 106.605s | 4210 | 39.50 | 25.341ms | 0 |
| Frostpine Pass | Torr | 112.861s | 4457 | 39.50 | 25.341ms | 0 |
| Frostpine Pass | Kite | 109.777s | 4335 | 39.50 | 25.339ms | 0 |

The six dry runs used the final scanline shadow implementation at `d031f4b`.
The three winter runs were repeated at `0e08624` after changes confined to winter
vegetation rendering. Dry vegetation and racing physics were unchanged by that
optimization. These measurements include the public-input automated driver's
CPU cost, physics, rendering, display synchronization and racing audio.

[Machine-readable results and firmware hashes](device-results.json) distinguish
the two measured builds. Superseded winter runs with missed refreshes remain in
local diagnostic logs and are excluded from this final table. This is device
benchmark evidence; physical mobile touchscreens and external gamepads were not
used. Their browser inputs were exercised through Chromium integration tests.

The published player UF2 is built by CI from the same source as the web build;
its checked-in workflow uses the same Arm compiler release as these measurements.

## Delivery

[Implementation CI and Pages deployment](https://github.com/frostney/gravelbyte/actions/runs/34455644523)
passed on `0e08624`. The hosted WASM was loaded and played, with no browser console
warnings or errors observed. The published UF2 was downloaded, its SHA256 checked
against the same deployment's manifest, and loaded onto the identified PicoSystem
with flash verification. USB telemetry then confirmed `mode=0` at the title screen,
so the device is running player firmware rather than the automated diagnostic.

Player UF2 SHA256: `3b7f2498d49f6d1c2157f7656564d05d7f16f49331afb3c0d778a764792bea2b`.
The full pre-flash backup remains verified and retained locally.
