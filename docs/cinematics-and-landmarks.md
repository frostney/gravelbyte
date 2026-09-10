# Cinematics and landmarks

Approved scope: retain the existing left/right track selector and show every
track, including locked ones. Beat the preceding default target with any car to
unlock the next globally. Existing qualifying records count. Personal bests do
not raise the unlock target.

The title has the game name, platform-specific continue prompt and persistent
sound toggle. It demonstrates driving through all three environments. Car
selection uses arrows and a larger rotating model without counters. Finishing
replays the actual drive, with the records panel hidden initially. X/Space/Records
shows five cumulative gate deltas against the default target and the prior best.
A retries; B returns to the existing track selector. Taglines are removed.

Bracken Ridge has a river bridge and solid rails. Sunmeadow Run follows a sandy beach throughout, with palms, low inland dunes and
water alongside the road, retaining ordinary off-road consequences. Frostpine
Pass has a tunnel bored through a broad, snow-covered rock mountain. Its portal
faces leave the road open, and its slopes stay within the local road corridor. The existing road
centerlines and car tuning are retained; structure collisions are local additions.

## Implementation boundaries

One generated course remains resident. A 24KiB recording stores quantized poses
at 10Hz initially, with interpolation during playback. When full, the buffer
keeps every second sample and doubles its interval. No second game/terrain copy,
heap-backed replay or persistent replay file is required. Records and race results
are frozen during replay; attract driving never earns records or unlocks.

Cameras use a variable basis and select terrain ahead or behind the car according
to viewing direction. Tunnel approaches use a chase shot to keep the camera clear
of the structure. Bridge roadside shots stay above deck height. Nearby trees use
full 3D visible faces; distant trees switch to lighter geometry at 72m. The road
view extends 24 nodes ahead, retaining the 3D road and original simulation.
The tunnel mountain uses four broad longitudinal sections with 300 bytes of
precomputed vertices; its detailed interior follows every original road node.

Reusable tuning values live in `src/tuning.hpp`. Authored bends, geometry vertices,
car profiles and target times remain data rather than unrelated global constants.

Save version 2 uses a spare bit in the existing selection word for mute, retaining
the 200-byte layout. Version 1 saves load directly; the older course-v3 record
continues to migrate to Torr/Bracken only. Unlocking is derived from the saved
best times, so an older qualifying record earns the same progression.

## Race-entry defect

The previous build used `PICO_BOARD=pico`, which defined a 2MiB flash limit.
The save offset was in the last sector of the PicoSystem's 16MiB flash. Pico SDK
2.2.0's `flash_range_erase` and `flash_range_program` hard-assert that writes fit
the configured limit. This explains why real-player selection saves stopped the
game while save-free benchmark builds completed races.

CMake now selects `pimoroni_picosystem` before SDK import. The save offset derives
from the configured size, and a static assertion prevents a generic-Pico build.
A real-save device smoke test verified five writes and progressed through car
selection, countdown, pause, sound toggles and back into racing. The interrupted
flash interval is excluded from the next simulation delta.

## Evidence

Native tests exercise actual-run replay accuracy (within 30cm), all cinematic
camera/course combinations, bounded long recordings, save migration from an
independently encoded v1 fixture, progression, sound persistence, structure
collisions, all nine public-input drives and road corridor clearance. Browser
integration exercises keyboard, multitouch and gamepad paths, including refusing
a locked start while allowing browsing, and persisted mute after reload.

Hardware measurements and final delivery status are recorded in validation.md.
Raw logs, the verified pre-change full-flash backup and firmware hashes remain in
local `output/`. Never use the older baseline results as evidence for this update.
