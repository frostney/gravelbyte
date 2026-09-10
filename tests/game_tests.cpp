#include "game.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace rally;
static void check(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}
static Game running() {
  Game g;
  g.restart();
  g.tick(3.1f, {});
  return g;
}
int main() {
  Game g;
  check(g.mode == Mode::Title, "boot waits on title");
  g.tick(10, {});
  check(g.elapsed == 0, "title does not start timer");
  Input a{};
  a.action = true;
  a.throttle = true;
  g.tick(.02f, a);
  check(g.mode == Mode::CarSelect, "A opens car selection");
  g.tick(.02f, a);
  check(g.mode == Mode::TrackSelect, "confirm chooses car");
  g.tick(.02f, a);
  check(g.mode == Mode::Countdown, "confirm starts countdown");
  g.tick(2, {});
  check(g.mode == Mode::Countdown && g.elapsed == 0, "countdown excludes stage time");
  g.tick(1.1f, {});
  check(g.mode == Mode::Racing, "countdown enters race");
  for (int i = 0; i < 100; ++i)
    g.tick(.02f, a);
  check(g.speed > 10, "throttle builds speed");
  Input p{};
  p.pause = true;
  g.tick(.02f, p);
  float elapsed = g.elapsed;
  Vec position = g.car;
  Input held{};
  held.throttle = true;
  g.tick(5, held);
  check(g.elapsed == elapsed && g.car.z == position.z, "pause freezes timer and physics");
  g.tick(.02f, p);
  check(g.mode == Mode::Racing, "resume returns to racing");
  Input brake{};
  brake.brake = true;
  float before = g.speed;
  for (int i = 0; i < 20; ++i)
    g.tick(.02f, brake);
  check(g.speed < before, "braking reduces speed");
  for (int i = 0; i < 150; ++i)
    g.tick(.02f, brake);
  check(g.velocity.x * std::sin(g.yaw) + g.velocity.z * std::cos(g.yaw) < 0,
        "brake reverses after stopping");
  Game fast = running(), slow = running();
  for (int i = 0; i < 200; ++i)
    fast.tick(.01f, a);
  for (int i = 0; i < 50; ++i)
    slow.tick(.04f, a);
  check(std::abs(fast.car.z - slow.car.z) < .02f,
        "physics consistent at 25 and 100 updates per second");
  check(std::abs(fast.elapsed - slow.elapsed) < .001f, "timing independent of update rate");
  Game drift = running();
  for (int i = 0; i < 100; ++i)
    drift.tick(.02f, a);
  Input slide = a;
  slide.left = true;
  slide.handbrake = true;
  for (int i = 0; i < 25; ++i)
    drift.tick(.02f, slide);
  check(drift.slip > 1, "handbrake corner creates lateral slip");
  Game traction = running();
  traction.velocity = {20.f, 0, 0};
  traction.physics(.01f, {});
  check(traction.velocity.x >= 20.f - 8.5f * .01f - .0001f,
        "sideways tyre force cannot exceed gravel traction limit");
  Game off = running();
  off.car = off.roadside(1, 50);
  off.locate();
  off.tick(.02f, a);
  check(off.recoveries == 1 && off.elapsed >= 3, "stranded recovery adds penalty");
  check(std::abs(off.lateral) < .1f, "recovery puts car on road");
  SaveRecord record = encode_best(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  check(std::abs(decode_best(record) - 92.345f) < .001f, "best time record round trip");
  record.milliseconds ^= 1;
  check(decode_best(record) == 0, "damaged save is rejected");
  SaveRecord blank{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
  check(decode_best(blank) == 0, "erased flash means no record");
  Game loaded;
  load_best(loaded, encode_best(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f}));
  loaded.restart();
  check(std::abs(loaded.reference_splits[1] - 36) < .001f,
        "saved fastest run supplies sector benchmark");
  Game slower;
  load_best(slower, encode_best(120.f, {24.f, 48.f, 72.f, 96.f, 120.f}));
  slower.restart();
  check(slower.reference_splits == DefaultSplits,
        "slower personal best does not replace built-in target");
  record = encode_best(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  record.splits[0] ^= 1;
  check(decode_best(record) == 0, "corrupt split rejected");
  record = encode_best(92.345f, {18.f, 36.f, 35.f, 74.f, 92.345f});
  check(decode_best(record) == 0, "non-monotonic split rejected");
  record = encode_best(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  record.course = 1;
  check(decode_best(record) == 0, "old course times are not comparable");
  Game sector = running();
  sector.segment = SectorEnds[0];
  sector.furthest = sector.segment;
  sector.car = sector.road[sector.segment].p +
               (sector.road[sector.segment + 1].p - sector.road[sector.segment].p) * .2f;
  sector.elapsed = 31;
  sector.tick(.02f, {});
  check(sector.split_count == 1 && sector.split_message > 0, "sector crossing records a split");
  float first_split = sector.splits[0];
  sector.recover();
  sector.tick(.02f, {});
  check(sector.split_count == 1 && sector.splits[0] == first_split,
        "recovery does not duplicate split");
  check(sector.elapsed > 34, "recovery penalty remains in total time");
  Game finish = running();
  finish.split_count = SectorCount - 1;
  finish.splits = {18.f, 36.f, 54.f, 74.f, 0.f};
  finish.segment = NodeCount - 3;
  finish.furthest = NodeCount - 4;
  finish.car = finish.road[NodeCount - 3].p;
  finish.yaw = finish.road[NodeCount - 3].heading;
  finish.elapsed = 90;
  finish.tick(.02f, {});
  check(finish.mode == Mode::Finished && finish.save_requested, "finish requests best-time save");
  elapsed = finish.elapsed;
  finish.tick(2, {});
  check(finish.elapsed == elapsed, "finish time stops");
  check(finish.new_record && finish.best > 90, "first completed stage sets best");
  finish.restart();
  check(finish.previous_best > 90 && finish.elapsed == 0, "retry retains record and resets clock");
  auto *renderer = new Renderer;
  std::array<uint16_t, W * H + 2> pixels{};
  pixels.front() = 0xa5a5;
  pixels.back() = 0x5a5a;
  int maximum_faces = 0;
  for (int node = 0; node < NodeCount - 1; node += 3)
    for (float angle : {0.f, 1.4f, 3.14159f}) {
      Game view;
      view.segment = node;
      view.car = view.road[node].p;
      view.yaw = view.road[node].heading + angle;
      view.camera_yaw = view.yaw;
      view.camera_height = view.ground_y = view.car.y;
      view.mode = Mode::Racing;
      renderer->render(view, pixels.data() + 1);
      maximum_faces = std::max(maximum_faces, renderer->face_count);
      check(renderer->dropped == 0, "renderer stays inside triangle budget around full course");
      check(pixels.front() == 0xa5a5 && pixels.back() == 0x5a5a,
            "renderer respects framebuffer boundaries");
    }
  delete renderer;
  std::printf("PASS: lifecycle, driving, timestep, recovery, persistence, finish; 300 camera "
              "renders, max %d triangles\n",
              maximum_faces);
}
