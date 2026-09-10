# Authored courses and driving

Bracken Ridge is a 1.78km technical forest stage: tightening corners, a double
apex, a linked chicane, a narrow river bridge and an authored crest. Sunmeadow Run
is a 1.33km coastal sprint with long sweeps, a sharp chicane and a sand transition
inside a corner. Frostpine Pass is a 2.37km winter endurance stage with switchbacks,
double apexes, ice, authored hills and a tunnel enclosed by a mountain.

The courses retain 301 cached road nodes to bound handheld memory. Their segment
lengths are 6, 4.5 and 8 metres. Curvature is authored at entry/apex/exit knots;
smooth interpolation between knots allows tightening and opening turns rather
than imposing a single sine envelope. Elevation keys are placed alongside
corners and landmarks. Checkpoint nodes are respectively 53/115/169/239/297,
48/129/157/211/297, and 70/133/179/247/297. Timing, roadside gates, progress dots
and the selection map use those same per-course values.

Bronze unlocks the next course for every car. Silver and gold reward reruns.
Light countersteering is off by default, selectable with X/Space in the car menu,
and responds only to lateral momentum. Assisted records are separate; both
categories count for progression. Pace notes preview one upcoming corner or
feature; grades run from 1 tightest to 6 fastest. Pause options control sound and
pace notes. The first countdown teaches drift, and the attract driver genuinely
uses the handbrake. Engine pitch steps through five gears and has a rev limiter.
PicoSystem LED feedback follows countdown, split colour and finish.

Save version 4/content revision 4 is a hard cut. The journal reserves records for
four route variants and two assist categories; route variants themselves ship in
the following release. Each journal write programs 1,536 bytes within one 4KiB
sector, preserving the other valid slot. The pending save uses static memory;
the expanded records must not create a roughly 3KiB persistence stack frame on
PicoSystem. Current player static allocation is 236,548 bytes. No clock or SDK
loop changes were made.

Validation includes all nine unassisted and nine assisted full-course drives,
medal boundaries, menu/option persistence, absent-feature pace notes, engine
shifts, terrain/shadow regressions and flash corruption/interrupted writes.
Hardware results are recorded separately after the complete device run.
