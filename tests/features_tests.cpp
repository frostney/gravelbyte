#include "game.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
using namespace rally;
static void check(bool ok, const char *message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}
int main() {
  Game g;
  auto original = g.road;
  g.select(1, 1);
  check(std::abs(g.road[100].p.x - original[100].p.x) > 20, "summer is a distinct course");
  g.select(1, 2);
  check(std::abs(g.road[100].p.x - original[100].p.x) > 20, "winter is a distinct course");
  check(g.icy(85) && g.surface_grip(85) < g.surface_grip(55), "ice has local traction changes");
  g.select(1, 0);
  for (int i = 0; i < NodeCount; ++i)
    check(g.road[i].p.x == original[i].p.x && g.road[i].p.y == original[i].p.y,
          "returning to Bracken preserves original course");
  std::array<float, 3> speeds{};
  for (int car = 0; car < CarCount; ++car) {
    g.select(car, 0);
    Input gas;
    gas.throttle = true;
    for (int i = 0; i < 100; ++i)
      g.physics(.01f, gas);
    speeds[car] = g.speed;
  }
  check(speeds[0] < speeds[1] && speeds[1] < speeds[2],
        "acceleration bars match measured acceleration");
  for (int i = 0; i < CarCount * TrackCount; ++i)
    for (int split = 0; split < SectorCount; ++split)
      g.records[i].splits[split] = float(20 * split + 10 + i);
  g.select(2, 2);
  SaveData save = encode_save(g);
  Game restored;
  check(load_save(restored, save), "nine records load");
  check(restored.selected_car == 2 && restored.selected_track == 2, "selections persist");
  for (int track = 0; track < TrackCount; ++track)
    for (int car = 0; car < CarCount; ++car) {
      restored.select(car, track);
      check(std::abs(restored.best - float(90 + track * 3 + car)) < .001f,
            "records isolated by car and track");
    }
  for (size_t byte = 0; byte < sizeof(save); ++byte) {
    auto damaged = save;
    reinterpret_cast<unsigned char *>(&damaged)[byte] ^= 1;
    check(!load_save(restored, damaged), "every corrupted byte is rejected");
  }
  Game legacy;
  load_best(legacy, encode_best(92.f, {18, 36, 54, 74, 92}));
  legacy.select(0, 0);
  check(legacy.best == 0, "legacy record is not assigned to easy car");
  legacy.select(1, 0);
  check(std::abs(legacy.best - 92.f) < .001f, "legacy record stays with Standard Bracken");
  g.mode = Mode::CarSelect;
  g.selected_car = 0;
  Input right;
  right.right = true;
  g.tick(.02f, right);
  g.tick(.02f, right);
  check(g.selected_car == 1, "holding select does not skip cars");
  g.tick(.02f, {});
  g.tick(.02f, right);
  check(g.selected_car == 2, "released select can advance again");
  auto renderer = std::make_unique<Renderer>();
  std::array<uint16_t, W * H> pixels{};
  // Partitioned shadow must still cover the ground and obey foreground depth.
  for (float yaw : {0.f, .4f, 1.2f, 2.7f})
    for (float bank : {-.12f, 0.f, .12f}) {
      renderer->pixels = pixels.data();
      pixels.fill(0);
      renderer->depth_buffer.fill(0);
      renderer->face_count = 0;
      renderer->dropped = 0;
      ++renderer->render_frame;
      renderer->camera_x = 0;
      renderer->camera_y = 256;
      renderer->camera_z = -320;
      renderer->sine = 0;
      renderer->cosine = 16384;
      renderer->shadow_enabled = true;
      renderer->shadow_center = {0, 0, 5};
      renderer->shadow_sin = std::sin(yaw);
      renderer->shadow_cos = std::cos(yaw);
      renderer->shadow_width = 1;
      renderer->shadow_length = 1.8f;
      renderer->shadow_min_x = -3;
      renderer->shadow_max_x = 3;
      renderer->shadow_min_z = 2;
      renderer->shadow_max_z = 8;
      renderer->shadow_enabled = false;
      renderer->ground_quad({-6, -6 * bank, 1}, {6, 6 * bank, 1}, {6, 6 * bank, 20},
                            {-6, -6 * bank, 20}, color(10, 10, 10));
      for (int i = 0; i < renderer->face_count; ++i)
        renderer->raster(renderer->faces[i]);
      const auto unshadowed = pixels;
      pixels.fill(0);
      renderer->face_count = 0;
      renderer->depth_buffer.fill(0);
      renderer->shadow_enabled = true;
      renderer->ground_quad({-6, -6 * bank, 1}, {6, 6 * bank, 1}, {6, 6 * bank, 20},
                            {-6, -6 * bank, 20}, color(10, 10, 10));
      for (int i = 0; i < renderer->face_count; ++i)
        renderer->raster(renderer->faces[i]);
      int dark = 0;
      for (auto p : pixels)
        if (p && ((p >> 12) < 8))
          ++dark;
      check(dark > 15, "ground shadow remains visible on slopes at different car headings");
      for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x) {
          bool interior = true;
          for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
              interior &= unshadowed[(y + dy) * W + x + dx] != 0;
          if (interior)
            check(pixels[y * W + x] != 0, "shadow material introduces no ground holes");
        }
      // A close foreground face must cover the shadow just as it covers the road.
      Renderer::Triangle front{{0, 119, 60}, {119, 119, 0}, {50000, 50000, 50000}, color(15, 2, 2)};
      renderer->raster(front);
      check(pixels[80 * W + 60] == front.color, "foreground geometry occludes shadow");
    }
  std::puts("PASS: track identity, acceleration, nine records, corruption, migration, selection "
            "edges, ground shadow");
}
