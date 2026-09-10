# Validation

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
- The six dry-course device races completed at about 39.5fps with zero frames
  below 30fps. Revised winter geometry is undergoing its three complete runs.
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
