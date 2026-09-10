# Validation

The table below is the **pre-cinematics baseline**. It does not certify the
current update. See [the update design and defect analysis](cinematics-and-landmarks.md);
new full-device results are being collected before publication.

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
