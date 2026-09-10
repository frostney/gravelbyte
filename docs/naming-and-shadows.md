# Descriptive naming and continuous ground shadows

The cars are Finch 1300 (Easy), Kestrel GT (Standard) and Goshawk Turbo (Expert).
Their numeric save identities, handling and target times are unchanged. The
racing HUD uses the five progress markers without a separate lower-left sector
number; accessible status announcements still describe checkpoint progress.

Owned C++ and browser JavaScript identifiers now use descriptive PascalCase.
This includes native/PicoSystem entry-point internals, tests and the WASM bridge.
Required SDK, browser and library names remain unchanged. The renamed WASM
exports and their JavaScript consumer ship together in the same versioned asset
bundle. clang-tidy enforces C++ naming and minimum identifier lengths in CI.

## Shadow defect and correction

Each receiving ground triangle previously clipped a separate shadow mask to its
own edges. The mask and receiver were projected and rounded independently,
leaving bright seams inside the shadow as it crossed road or terrain triangles.
The mask could also disappear entirely when one corner crossed the near plane.

Each receiver now projects the full shadow rectangle onto its own surface plane.
The receiver triangle provides the coverage and depth test; the mask only changes
its material colour. This keeps the road/grass boundary from becoming a second
rounded shadow edge and adds no overlapping ground geometry. The mask is clipped
against the near plane rather than discarded wholesale.

## Validation

- The original moving-surface reproduction exposed 283 bright interior samples;
  the fixed renderer exposes zero in that same sweep.
- The expanded test passes 1,920 moving frames and 231,519 shadow-interior samples
  across four headings, three bank slopes, creases and alternating road/grass
  materials. It also checks partial near-plane visibility. Reintroducing the
  all-or-nothing near-plane rejection makes that regression test fail.
- Eight native and eight AddressSanitizer/UndefinedBehaviorSanitizer suites pass.
  Existing foreground-occlusion tests, 180 off-road excursions, save corruption
  and interrupted-write cases remain covered.
- Seventeen Chromium tests pass, including keyboard/touch/gamepad controls,
  blocked storage, load errors, mismatched builds and audio initialization.
- All 36 sampled racing frames are byte-identical before and after identifier
  migration (both samples include the shadow correction and HUD change).
- A compiler reference comparison found no changed bindings in migrated existing
  code. Ignoring identifier spelling and comments, game logic, headers, save
  journal, driver and tuning retain identical tokens except the three new car
  strings. The PicoSystem's actual 200-byte saved record round-trips byte-for-byte.
- Visual browser checks confirm all three names fit, the racing sector number
  is absent, and checkpoint progress remains visible.

All nine audio-enabled PicoSystem races passed: 36,872 frames, weighted mean
39.49fps, worst frame 25.352ms, zero frames below 30fps, recoveries or dropped
geometry. The 50fps aspiration remains unmet. The normal player firmware was
flash-verified and restored at its title screen; both original save sectors
were read back and are byte-identical. [Device results and artifact/source
hashes](naming-shadow-device-results.json) identify the measured build.

The local verified full-flash backup and raw measurement logs are retained under
`output/naming/`. Earlier telemetry from a failed first flashing attempt only
observed the old title screen and is excluded from the measured race results.
