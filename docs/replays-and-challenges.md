# Replays, practice and challenges

Each authored course has Original, Reverse, Mirror and Reverse + Mirror variants.
Bronze on its original route unlocks the other three for all cars. The road,
checkpoint order, bridge, tunnel, surfaces and pace notes follow the chosen route.
Reverse split targets reverse the original target intervals; total medal targets
stay the same. Records are separate for all 72 course/car/variant/assist pairs.

A new personal best stores a replay. During the next eligible authored run, a
pale stippled ghost samples that replay at the official elapsed time, including
recovery penalties. It uses a simpler body/roof/wheel mesh so small trim does not consume the
handheld frame budget. It has no collision or shadow and fades within six metres,
becoming invisible within three metres. Pause > Options controls ghost visibility.
The live finish replay remains independent, with its existing cinematic cameras.

The handheld retains one 24KiB recording buffer. Stored ghosts are decimated to
at most 1,025 twelve-byte poses, preserving the last pose and exact duration.
Their immutable flash data is sampled directly; there is no second replay RAM
buffer. Two 16KiB slots per record occupy 2.25MiB starting at flash offset 12MiB,
below the two existing journal sectors. Checksummed headers identify format,
course revision, record category, split identity, timing and pose count. Payload
pages are written first and the header last. A new save preserves the slot that
matches the prior best until replacement is validated. A failed replay can leave
a valid new time without a ghost; mismatched ghosts are never used.

Browsers use IndexedDB transactions, copy replay bytes before asynchronous work,
and reject stale load generations. Storage failure keeps gameplay available.
Desktop previews use two files and atomic rename. Host interruption models test
publication after every programmed byte; they do not simulate every electrical
failure mode. The test fixtures are generated through normal driving controls.

Pause > Retry last split restores the actual car position, velocity, heading,
steering, suspension/contact state, elapsed time and split history captured at
the checkpoint. A countdown precedes the retry. The run is visibly marked as
practice and cannot award records, medals or a saved ghost. Restart starts a full
eligible run. Before the first checkpoint, retry returns to the starting grid.

Challenge is a fourth entry in the shared course selector. X on PicoSystem,
Space on keyboard or New on touch generates a random stage. Left/right opens an
eight-digit hexadecimal seed editor: left/right select a digit, up/down change
it, A/Enter/Go confirms and B/Esc/Back cancels. Zero and the full unsigned 32-bit
range are valid. The same seed generates the same route independently of car or
assist. The current seed survives retry, car changes and save/reload; the browser
also includes it in the URL for sharing. There is no calendar or daily-stage mode.

Generated routes choose a biome, 5–7 metre node spacing, a checkpoint layout and
compound-corner profiles. Bounded alternating headings and straight clearance
around landmark/crest approaches constrain the generator. Tests cover 256 seeds
with all three cars; this finite corpus is not a proof of every possible seed.
Challenge bests last for the session and are separated by car and assist. They
do not unlock campaign content, award medals or replace authored ghosts.

Host validation: 72 full variant/car/assist drives and 768 seeded drives finish
without automatic recovery; 240 additional landmark views stay within the
geometry budget. Storage corruption, byte-by-byte interrupted publication,
visible/overlapping ghost rendering, practice eligibility and replay clock tests
pass in ten native and ten AddressSanitizer/UndefinedBehaviorSanitizer suites.
The 26 browser tests cover an actual full driven best saved through IndexedDB
and loaded after reload, corrupt storage, seed editing, unsigned URL boundaries
and practice restart. The full-race browser check sends public keyboard inputs
with a controlled animation clock; it does not edit the car, timer or records. Native/WASM comparison covers
2,107 road nodes across seven seeds, with a maximum position difference of
0.000306 metres (0.02-metre acceptance tolerance).

The silent PicoSystem ghost workload completed all nine car/course combinations:
35,576 racing frames at 39.475fps weighted mean, worst frame 25.347ms, with zero
sub-30fps frames, automatic recoveries or dropped geometry. A synthetic workload
moves saved poses two nodes ahead so proximity fading cannot remove the second
car's cost: 35,565 frames contained ghost geometry. These are performance fixtures,
not player records. An earlier full-detail ghost failed with 28 slow frames on
Finch/forest; that rejected result prompted the simplified ghost mesh.

The highest reported render time was 23.955ms; sampled CPU idle fell to 5%.
The SDK display interval is about 25.3ms at its unchanged 250MHz clock, so these
results do not justify reducing the clock. Tests ran silently at the user's
request; this update does not claim audio-enabled hardware performance.

Six further silent device races cover Goshawk on reverse forest, mirrored beach,
reverse/mirrored winter, and random seeds `00000000`, `80000000`, `FFFFFFFF`
(one per biome). Their 21,124 frames average 39.475fps, worst 25.350ms, with zero
sub-30fps frames, recoveries or dropped geometry. These representative device
runs complement the wider native corpus; they do not measure every route/seed.

A real PicoSystem run verified eight alternating record/settings writes, saved a
514-pose personal-best ghost, and loaded it on both in-game restart and firmware
reboot. Flash readback independently validated its checksum and 102.596-second
record identity. The original reserved 4MiB, including player records and mute
preference, was then restored byte-for-byte. The normal player was flash-verified
and observed at its title screen. Its static allocation is 237,816 bytes.
[Hardware measurements and artifact identities](replay-device-results.json)
retain accepted results and the rejected full-detail-ghost result separately. Automated driving establishes
completion and regression coverage; human difficulty tuning still needs player
feedback.
