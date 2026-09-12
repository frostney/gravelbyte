# Authored-course target calibration

Course revision 4 deliberately discards earlier times. Host Release measurements
use public digital controls at 20ms per frame; every car finishes every course
with five splits and no recovery. These automated runs know upcoming geometry;
they establish reachability, not human approachability. Human timing feedback is
still needed, especially for a first bronze and the higher medals.

Targets use the initial authored-course reference table, before the final small
crest-launch adjustment (under 0.2 seconds difference in the current runs below).
Each checkpoint receives the same car-specific bronze margin. Exact equality
qualifies. Personal-best targets never make progression harder.

| Car | Bronze multiplier | Silver multiplier | Gold multiplier |
| --- | --- | --- | --- |
| Finch 1300 | 1.40 | 1.15 | 1.02 |
| Kestrel GT | 1.20 | 1.08 | 0.98 |
| Goshawk Turbo | 1.12 | 1.04 | 0.96 |

The generous Finch bronze responds to the player's difficulty winning any stage.
Gold on Standard and Expert asks for a faster line than the conservative reference
driver. Assistance changes neither medals nor unlock thresholds; its personal
records are stored separately. All nine assisted reference drives also finish
without recovery and beat bronze.

Current unassisted runs:

```text
track=0 car=0 mode=6 time=99.52 segment=297 recoveries=0 max_lateral=3.75 max_triangles=934
splits=17.280,38.861,57.421,80.738,99.515 jumps=1 air_frames=11 max_air=0.097
offroad_seconds=0.14 max_width_ratio=1.028
track=0 car=1 mode=6 time=103.87 segment=297 recoveries=0 max_lateral=3.25 max_triangles=942
splits=17.120,40.061,59.501,84.058,103.875 jumps=1 air_frames=14 max_air=0.147
offroad_seconds=0.00 max_width_ratio=0.891
track=0 car=2 mode=6 time=100.74 segment=297 recoveries=0 max_lateral=2.75 max_triangles=943
splits=16.500,38.721,57.681,81.558,100.735 jumps=1 air_frames=29 max_air=0.196
offroad_seconds=0.00 max_width_ratio=0.753
track=1 car=0 mode=6 time=67.94 segment=297 recoveries=0 max_lateral=2.78 max_triangles=700
splits=11.520,28.180,36.361,49.341,67.941 jumps=0 air_frames=0 max_air=0.000
offroad_seconds=0.00 max_width_ratio=0.670
track=1 car=1 mode=6 time=69.96 segment=297 recoveries=0 max_lateral=2.49 max_triangles=706
splits=11.240,28.520,37.201,50.721,69.960 jumps=0 air_frames=0 max_air=0.000
offroad_seconds=0.00 max_width_ratio=0.600
track=1 car=2 mode=6 time=67.68 segment=297 recoveries=0 max_lateral=2.31 max_triangles=706
splits=10.840,27.580,35.961,49.061,67.681 jumps=0 air_frames=0 max_air=0.000
offroad_seconds=0.00 max_width_ratio=0.557
track=2 car=0 mode=6 time=129.27 segment=297 recoveries=0 max_lateral=3.24 max_triangles=760
splits=32.081,58.281,78.619,109.274,129.271 jumps=2 air_frames=27 max_air=0.282
offroad_seconds=0.00 max_width_ratio=0.914
track=2 car=1 mode=6 time=133.23 segment=297 recoveries=0 max_lateral=2.64 max_triangles=761
splits=33.061,58.941,80.159,112.633,133.232 jumps=2 air_frames=28 max_air=0.302
offroad_seconds=0.00 max_width_ratio=0.743
track=2 car=2 mode=6 time=129.15 segment=297 recoveries=0 max_lateral=2.19 max_triangles=761
splits=32.061,57.081,77.719,109.194,129.151 jumps=2 air_frames=29 max_air=0.324
offroad_seconds=0.00 max_width_ratio=0.616
```
