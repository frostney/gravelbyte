#include "game.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// A driver using the public controls, never teleporting or modifying race state.
// This validates that the complete authored course can actually be driven.
int main(int argc, char **argv) {
  using namespace rally;
  for (int track = 0; track < TrackCount; ++track)
    for (int car = 0; car < CarCount; ++car) {
      Game game;
      game.select(car, track);
      game.mode = Mode::Countdown;
      Input start{};
      start.action = true;
      game.tick(.02f, start);
      float max_lateral = 0, max_air = 0, offroad_seconds = 0, max_width_ratio = 0;
      int air_frames = 0;
      auto *renderer = new Renderer;
      std::array<uint16_t, W * H> pixels{};
      int max_triangles = 0, frames = 0;
      float last_capture = -10;
      while (frames++ < 15000 && game.mode != Mode::Finished) {
        Input input = test_driver(game);
        game.tick(.02f, input);
        if (std::abs(game.lateral) > game.road_width())
          offroad_seconds += .02f;
        max_width_ratio = std::max(max_width_ratio, std::abs(game.lateral) / game.road_width());
        if (game.airborne)
          ++air_frames;
        max_air = std::max(max_air, game.car.y - game.ground_y);
        max_lateral = std::max(max_lateral, std::abs(game.lateral));
        if (frames % 5 == 0) {
          renderer->render(game, pixels.data());
          max_triangles = std::max(max_triangles, renderer->face_count);
          if (renderer->dropped) {
            std::fprintf(stderr, "Triangle overflow in stage drive\n");
            return 1;
          }
        }
        if (argc > 1 && game.elapsed - last_capture >= 1) {
          renderer->render(game, pixels.data());
          char filename[512];
          std::snprintf(filename, sizeof(filename), "%s/frame-%04d.ppm", argv[1],
                        int(game.elapsed));
          if (FILE *f = std::fopen(filename, "wb")) {
            std::fprintf(f, "P6\n120 120\n255\n");
            for (uint16_t c : pixels) {
              unsigned char rgb[] = {uint8_t((c >> 12) * 17), uint8_t(((c >> 8) & 15) * 17),
                                     uint8_t(((c >> 4) & 15) * 17)};
              std::fwrite(rgb, 1, 3, f);
            }
            std::fclose(f);
          }
          last_capture = game.elapsed;
        }
      }
      std::printf("track=%d car=%d mode=%d time=%.2f segment=%d recoveries=%d max_lateral=%.2f "
                  "max_triangles=%d\n",
                  track, car, int(game.mode), game.elapsed, game.segment, game.recoveries,
                  max_lateral, max_triangles);
      std::printf("splits=%.3f,%.3f,%.3f,%.3f,%.3f jumps=%d air_frames=%d max_air=%.3f\n",
                  game.splits[0], game.splits[1], game.splits[2], game.splits[3], game.splits[4],
                  game.jumps, air_frames, max_air);
      std::printf("offroad_seconds=%.2f max_width_ratio=%.3f\n", offroad_seconds, max_width_ratio);
      if (game.split_count != SectorCount ||
          !(game.splits[0] > 0 && game.splits[0] < game.splits[1] &&
            game.splits[1] < game.splits[2]) ||
          game.jumps < 1 || air_frames < 2 || max_air > 2.f || game.airborne)
        return 1;
      for (int i = 1; i < SectorCount; ++i)
        if (game.splits[i] <= game.splits[i - 1])
          return 1;
      Game restored;
      if (game.elapsed >= game.default_splits().back()) {
        std::fprintf(stderr,
                     "Reference driver cannot beat target: track=%d car=%d time=%.3f target=%.3f\n",
                     track, car, game.elapsed, game.default_splits().back());
        return 1;
      }
      // Satisfy earlier stages with actual completed runs, then verify that this
      // run's persisted result unlocks the next stage for every car.
      for (int earlier = 0; earlier < track; ++earlier) {
        Game prior;
        prior.select(car, earlier);
        prior.mode = Mode::Racing;
        for (int frame = 0; frame < 15000 && prior.mode != Mode::Finished; ++frame)
          prior.tick(.02f, test_driver(prior));
        if (prior.mode != Mode::Finished || prior.elapsed >= prior.default_splits().back())
          return 1;
        game.records[earlier * CarCount + car] = prior.records[earlier * CarCount + car];
      }
      if (!load_save(restored, encode_save(game)))
        return 1;
      if (track + 1 < TrackCount && !restored.unlocked(track + 1))
        return 1;
      restored.restart();
      if (std::abs(restored.best - game.elapsed) > .002f ||
          std::abs(restored.best_splits[1] - game.splits[1]) > .002f)
        return 1;
      delete renderer;
      if (game.mode != Mode::Finished || game.recoveries != 0 || game.elapsed > 150 ||
          max_lateral > RoadHalf + 1)
        return 1;
    }
}
