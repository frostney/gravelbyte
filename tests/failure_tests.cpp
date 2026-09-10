#include "game.hpp"
#include "save_journal.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace rally;
static void check(bool ok, const char *what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::exit(1);
  }
}
alignas(4) static std::array<std::array<uint8_t, 4096>, 2> flash;
static int erase_bytes = 4096, program_bytes = 256, written_slot = -1;
static const SaveSlot &slot(int index) {
  return *reinterpret_cast<const SaveSlot *>(flash[index].data());
}
static void fake_flash_write(int target, const SaveSlot &value) {
  written_slot = target;
  std::fill_n(flash[target].begin(), erase_bytes, 0xff);
  if (erase_bytes < 4096)
    return;
  std::array<uint8_t, 256> page;
  page.fill(0xff);
  std::memcpy(page.data(), &value, sizeof(value));
  for (int i = 0; i < program_bytes; ++i)
    flash[target][i] &= page[i];
}
int main() {
  Game g;
  const auto old = make_slot(encode_save(g), 42);
  g.muted = true;
  const auto fresh = make_slot(encode_save(g), 43);
  for (std::size_t count = 0; count <= sizeof(SaveSlot); ++count) {
    SaveSlot cut;
    std::memset(&cut, 0xff, sizeof(cut));
    std::memcpy(&cut, &fresh, count);
    int chosen = newest_slot(old, cut);
    check(chosen == 0 || (chosen == 1 && std::memcmp(&cut, &fresh, sizeof(cut)) == 0),
          "every interrupted page program retains old or complete new save");
    check(load_save(g, chosen == 0 ? old.data : cut.data), "chosen interrupted save loads");
  }
  for (std::size_t count = 0; count <= 4096; ++count) {
    std::array<uint8_t, 4096> sector;
    std::memcpy(sector.data(), &old, sizeof(old));
    std::fill(sector.begin(), sector.begin() + count, 0xff);
    SaveSlot erased;
    std::memcpy(&erased, sector.data(), sizeof(erased));
    check(newest_slot(erased, fresh) == 1,
          "partial erase of inactive sector preserves current save");
  }
  for (std::size_t i = 0; i < sizeof(fresh); ++i) {
    auto corrupt = fresh;
    reinterpret_cast<uint8_t *>(&corrupt)[i] ^= 1;
    check(newest_slot(old, corrupt) == 0, "corrupt newest page falls back to intact record");
  }
  check(newest_slot(make_slot(old.data, 0xffffffffu), make_slot(fresh.data, 0)) == 1,
        "journal sequence wraps");
  SaveSlot empty;
  std::memset(&empty, 0xff, sizeof(empty));
  check(newest_slot(empty, empty) == -1, "blank flash has no journal");
  // Exercise the exact writer-selection/readback algorithm used on hardware.
  for (int current : {0, 1})
    for (int cut = 0; cut <= 4096 + 256; ++cut) {
      for (auto &sector : flash)
        sector.fill(0xff);
      std::memcpy(flash[current].data(), &old, sizeof(old));
      erase_bytes = std::min(cut, 4096);
      program_bytes = std::max(0, cut - 4096);
      const auto result = store_save(fresh.data, slot(0), slot(1), fake_flash_write);
      check(written_slot != current, "writer never erases the selected save sector");
      const int selected = newest_slot(slot(0), slot(1));
      check(selected >= 0, "power interruption always leaves a loadable save");
      check(load_save(g, slot(selected).data), "power-cut result restores game state");
      check(result != SaveResult::Saved || g.muted, "success requires new setting readback");
    }
  for (int cut = 0; cut <= 4096 + 256; ++cut) {
    for (auto &sector : flash)
      sector.fill(0xff);
    std::memcpy(flash[1].data(), &old.data, sizeof(old.data));
    const auto legacy = flash[1];
    erase_bytes = std::min(cut, 4096);
    program_bytes = std::max(0, cut - 4096);
    store_save(fresh.data, slot(0), slot(1), fake_flash_write);
    check(written_slot == 0 && flash[1] == legacy,
          "first migration preserves legacy sector across power loss");
  }
  erase_bytes = 4096;
  program_bytes = 256;
  store_save(fresh.data, slot(0), slot(1), fake_flash_write);
  written_slot = -1;
  check(store_save(fresh.data, slot(0), slot(1), fake_flash_write) == SaveResult::Unchanged &&
            written_slot == -1,
        "unchanged saves do not erase flash");
  GeometryTelemetry telemetry;
  telemetry.observe(7);
  telemetry.observe(0);
  telemetry.observe(2);
  telemetry.observe(0);
  check(telemetry.frames == 4 && telemetry.dropped == 9 && telemetry.overflow_frames == 2,
        "overflow on earlier frames remains in final telemetry");

  Renderer renderer;
  std::array<uint16_t, W * H> pixels{};
  // A non-planar right bank exposes a reversed diagonal: interpolating the
  // other diagonal yields 4m at this centroid instead of the drawn 2m.
  g.segment = 150;
  for (int row = 0; row < NodeCount; ++row)
    for (int strip = 0; strip < 10; ++strip)
      g.terrain[row][strip] = {float((strip - 7) * 6), 0, float((row - 150) * 6)};
  g.terrain[151][8].y = 6;
  float bank_height = 0;
  check(g.surface_height({4, 0, 4}, bank_height) && std::abs(bank_height - 2.f) < .0001f,
        "right bank uses the rendered outside-in triangle diagonal");
  int scenarios = 0;
  float max_air = 0;
  for (int track = 0; track < TrackCount; ++track)
    for (int car = 0; car < CarCount; ++car)
      for (int node : {20, 45, 95, 120, 145, 156, 170, 200, 246, 280})
        for (int sign : {-1, 1}) {
          g.select(car, track);
          g.mode = Mode::Racing;
          bool begun = false;
          int perturbation = 0;
          for (int frame = 0; frame < 10000 && g.mode != Mode::Finished; ++frame) {
            auto in = driving_input(g);
            if (g.segment >= node)
              begun = true;
            if (begun && perturbation++ < 120) {
              in = {};
              in.throttle = true;
              in.left = sign < 0;
              in.right = sign > 0;
            }
            g.tick(.02f, in);
            check(std::isfinite(g.car.y) && std::isfinite(g.speed),
                  "off-road state remains finite");
            check(std::abs(g.vertical_speed) <= 12.001f,
                  "banks cannot inject extreme launch speed");
            max_air = std::max(max_air, g.car.y - g.ground_y);
            if (!g.airborne && std::abs(g.lateral) > g.road_width()) {
              float surface = 0;
              check(g.surface_height(g.car, surface), "off-road car has rendered ground support");
              check(std::abs(g.car.y - surface) < .13f, "grounded car follows visible terrain");
            }
            if (begun && frame % 20 == 0) {
              renderer.render(g, pixels.data());
              check(renderer.dropped == 0, "off-road views stay within geometry capacity");
            }
            if (begun && perturbation > 620)
              break;
          }
          check(begun, "off-road perturbation reaches requested course region");
          ++scenarios;
        }
  check(max_air < 3.f, "ordinary off-road excursions do not fly high above the course");
  g.select(1, 2);
  g.mode = Mode::Racing;
  const auto before = g.car;
  g.tick(NAN, {});
  g.tick(-1, {});
  g.tick(0, {});
  check(g.car.x == before.x && g.car.y == before.y && g.car.z == before.z,
        "invalid frame deltas ignored");
  std::printf("PASS: journal power-loss/corruption, cumulative telemetry, %d public-input off-road "
              "excursions; max air %.3fm\n",
              scenarios, max_air);
}
