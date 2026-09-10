# Foundation release

The starting car stays at its grid position until throttle or brake is pressed.
Deliberate reverse remains available. JavaScript uses descriptive camelCase;
C++ uses descriptive PascalCase in namespace GravelByte. PicoSystem web help
uses keycaps. Lefthook runs clang-tidy and formatting checks before commits.
Developer overlays require GRAVELBYTE_DIAGNOSTICS and are absent by default.

The pinned PicoSystem SDK calls update, waits for any remaining screen DMA,
swaps the framebuffer in draw, waits for the panel VSYNC, and starts the next
transfer. Its full tick includes those waits; the game renders inside update
while the previous framebuffer is being transmitted. There is no fixed 40Hz
update timer to remove. The current panel cadence is approximately 39.5fps.

Nine audio-enabled device races on the unchanged 250MHz clock produced 36,875
frames at a weighted 39.500fps, with zero sub-30fps frames, recoveries, dropped
triangles or overflow frames. Sampled update time ranged from 12.212 to 23.667ms
(median18.668ms); the render high-water mark was23.662ms. Sampled SDK busy-wait
opportunity ranged from6% to51% (median26%). These samples show real variation
in CPU work beneath a flat display cadence. They are not power measurements.

The accepted target is this display cadence with a30fps minimum. The clock and
panel configuration are unchanged. The SDK's no_overclock option would select
125MHz on this pinned SDK; no lower-clock or higher-refresh experiment was run.

[Device results](foundations-device-results.json) identify the core source
commit and both firmware hashes. Native8, ASan/UBSan8 and Chromium17 suites
passed. Removing the starting hold in a disposable build makes the new regression
fail on its stationary-grid assertion. Browser tests cover keyboard, touch,
gamepad, accessibility, storage failures, failed assets and small viewports.
A screenshot confirms the Pico keycaps fit the existing desktop page layout.
The installed Lefthook checks passed both explicitly and during the commit.

The full original flash was backed up and verified before flashing. Benchmark
firmware did not write records. Both journal sectors were compared before and
after normal-player installation. Raw telemetry, flash logs and backups remain
local under output/foundations/. This release retains current saves; the agreed
clean save-format break belongs to the authored-course release.
