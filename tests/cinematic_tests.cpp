#include "game.hpp"
#include "test_driver.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
using namespace rally;
static void check(bool ok, const char *why) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", why);
    std::exit(1);
  }
}
int main() {
  auto game = std::make_unique<Game>();
  Game &g = *game;
  check(g.unlocked(0) && !g.unlocked(1) && !g.unlocked(2), "fresh progression");
  g.select(2, 2);
  g.mode = Mode::TrackSelect;
  Input confirm;
  confirm.action = true;
  g.tick(.02f, confirm);
  check(g.mode == Mode::TrackSelect, "locked track can be browsed but not started");
  g.select(0, 0);
  g.records[0].splits = g.default_splits();
  check(!g.unlocked(1), "matching the target is not beating it");
  for (float &time : g.records[0].splits)
    time *= .99f;
  check(g.unlocked(1) && !g.unlocked(2), "any car unlocks next track globally");
  g.select(1, 1);
  g.records[4].splits = g.default_splits();
  for (float &time : g.records[4].splits)
    time *= .99f;
  g.toggle_audio();
  SaveData save = encode_save(g);
  Game restored;
  check(load_save(restored, save) && restored.muted && restored.unlocked(2),
        "audio and progression survive reload");
  SaveData legacy{};
  FILE *fixture = std::fopen(GRAVELBYTE_V1_FIXTURE, "rb");
  check(fixture && std::fread(&legacy, sizeof(legacy), 1, fixture) == 1,
        "read pre-update save fixture");
  std::fclose(fixture);
  check(load_save(restored, legacy) && !restored.muted && restored.selected_car == 2 &&
            restored.selected_track == 1 && restored.unlocked(1) &&
            std::abs(restored.records[1].splits.back() - 92.f) < .001f,
        "v1 saves migrate selections, records and earned unlocks");
  g.mode = Mode::Title;
  bool seen[TrackCount]{};
  for (int i = 0; i < 2400; ++i) {
    g.tick(.02f, {});
    seen[g.selected_track] = true;
  }
  check(seen[0] && seen[1] && seen[2], "title showcases all tracks");
  check(g.elapsed == 0, "attract driving does not race");
  auto after = encode_save(g);
  check(std::memcmp(&save, &after, sizeof(save)) == 0,
        "showcase preserves selected course, settings and records");
  g.tick(.02f, confirm);
  check(g.mode == Mode::CarSelect && g.selected_track == 1 && g.selected_car == 1,
        "continue restores player's selections");
  g.select(1, 0);
  std::vector<Vec> driven;
  while (g.mode != Mode::Racing)
    g.tick(.02f, {});
  driven.push_back(g.car);
  while (g.mode != Mode::Finished && driven.size() < 10000) {
    g.tick(.02f, driving_input(g));
    driven.push_back(g.car);
  }
  check(g.mode == Mode::Finished && !g.show_records, "finish starts with unobscured replay");
  const auto result = g.splits;
  auto result_save = encode_save(g);
  auto renderer = std::make_unique<Renderer>();
  std::array<uint16_t, W * H> pixels;
  for (int frame = 25; frame < int(driven.size()) - 25; frame += 25) {
    g.replay_time = frame * .02f - .001f;
    g.replay_tick(.001f);
    Vec delta = g.car - driven[frame];
    check(std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) < .3f,
          "replay follows actual driving within 30cm");
    for (int shot = 0; shot < 3; ++shot) {
      g.cinematic_time = shot * tuning::ShotSeconds;
      renderer->render(g, pixels.data());
      check(renderer->dropped == 0, "cinematic geometry fits budget");
    }
  }
  Input aux;
  aux.auxiliary = true;
  g.tick(.02f, aux);
  check(g.show_records, "records toggle on");
  g.tick(.02f, aux);
  check(!g.show_records && g.splits == result, "records toggle off without changing results");
  after = encode_save(g);
  check(std::memcmp(&result_save, &after, sizeof(after)) == 0, "replay never changes saved result");
  Input back;
  back.back = true;
  g.tick(.02f, back);
  check(g.mode == Mode::TrackSelect, "finish back returns to existing track selector");
  for (int track : {0, 2}) {
    g.select(1, track);
    g.segment = track == 0 ? tuning::BridgeStart + 3 : tuning::TunnelStart + 3;
    g.car = g.roadside(g.segment, g.road[g.segment].half_width + 1);
    g.locate();
    g.physics(.01f, {});
    check(std::abs(g.lateral) < g.road_width(), "bridge rails and tunnel walls constrain car");
  }
  for (int track = 0; track < TrackCount; ++track) {
    g.select(1, track);
    g.mode = Mode::Finished;
    for (int node = 5; node < NodeCount - 5; node += 3) {
      g.segment = node;
      g.car = g.road[node].p;
      g.camera_yaw = g.yaw = g.road[node].heading;
      g.camera_height = g.car.y;
      for (int shot = 0; shot < 3; ++shot) {
        g.cinematic_time = shot * tuning::ShotSeconds;
        renderer->render(g, pixels.data());
        check(renderer->dropped == 0, "every cinematic shot fits on every course");
        if (g.tunnel(node))
          check(renderer->camera.y < g.car.y + tuning::TunnelHeight,
                "tunnel camera clears ceiling");
      }
    }
  }
  // A long stationary run exercises replay compaction, including the final partial sample.
  g.select(1, 0);
  g.mode = Mode::Racing;
  for (int i = 0; i < 26000; ++i)
    g.tick(.02f, {});
  g.record_pose(true);
  check(g.replay_count <= int(tuning::ReplayCapacity) && g.replay_interval > tuning::ReplayInterval,
        "long recordings stay bounded");
  g.replay_time = g.replay_duration - .02f;
  g.replay_tick(.01f);
  check(std::isfinite(g.car.x) && g.segment < NodeCount, "compacted replay remains valid");
  std::puts("PASS: progression, persistent sound, actual-run replay, cinematic geometry, physical "
            "landmarks");
}
