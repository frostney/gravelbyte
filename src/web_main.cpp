#include "game.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <emscripten/emscripten.h>
using namespace rally;
static Game game;
static Renderer renderer;
static std::array<uint16_t, W * H> pixels;
static SaveData transfer;
static unsigned previous = 0;
static char status_text[1024];
extern "C" {
EMSCRIPTEN_KEEPALIVE const char *gb_build_id() { return GRAVELBYTE_BUILD_ID; }
EMSCRIPTEN_KEEPALIVE const char *gb_status() {
  switch (game.mode) {
  case Mode::Title:
    std::snprintf(status_text, sizeof(status_text),
                  "Gravelbyte. Press Enter or confirm to choose a car.");
    break;
  case Mode::CarSelect:
    std::snprintf(status_text, sizeof(status_text),
                  "Choose car: %s, %s. Speed %d of 5, acceleration %d of 5, drift %d of 5. Higher "
                  "drift means more sliding. Left and right change car; confirm chooses track.",
                  game.spec().name, game.spec().difficulty, game.spec().speed_stat,
                  game.spec().accel_stat, game.spec().drift_stat);
    break;
  case Mode::TrackSelect:
    if (game.unlocked(game.selected_track))
      std::snprintf(status_text, sizeof(status_text),
                    "Choose track: %s. Unlocked. Target %.2f seconds. Personal best %.2f seconds. "
                    "Confirm to race; back to cars.",
                    TrackNames[game.selected_track], game.default_splits().back(), game.best);
    else
      std::snprintf(status_text, sizeof(status_text),
                    "Choose track: %s. Locked. Beat %s to unlock. Left and right browse tracks; "
                    "back to cars.",
                    TrackNames[game.selected_track], TrackNames[game.selected_track - 1]);
    break;
  case Mode::Countdown:
    std::snprintf(status_text, sizeof(status_text),
                  "Get ready. Racing starts after the countdown.");
    break;
  case Mode::Racing:
    std::snprintf(status_text, sizeof(status_text), "Racing. Checkpoint %d of 5. P pauses.",
                  std::min(5, game.split_count + 1));
    break;
  case Mode::Paused:
    std::snprintf(status_text, sizeof(status_text),
                  "Paused. Resume, retry, or return to car selection.");
    break;
  case Mode::Finished:
    std::snprintf(status_text, sizeof(status_text),
                  "Stage complete. Time %.2f seconds; target %.2f seconds. %s. Confirm retries; "
                  "back chooses a track.",
                  game.elapsed, game.default_splits().back(),
                  game.elapsed < game.default_splits().back() ? "Target beaten"
                                                              : "Target not beaten");
    if (game.show_records) {
      std::size_t used = std::strlen(status_text);
      for (int i = 0; i < SectorCount && used < sizeof(status_text); ++i)
        used += std::snprintf(status_text + used, sizeof(status_text) - used,
                              " Checkpoint %d: %+.2f seconds against target.", i + 1,
                              game.splits[i] - game.default_splits()[i]);
    }
    break;
  }
  return status_text;
}
EMSCRIPTEN_KEEPALIVE int gb_unlocked() { return game.unlocked(game.selected_track); }

EMSCRIPTEN_KEEPALIVE const uint16_t *gb_frame() {
  renderer.render(game, pixels.data());
  return pixels.data();
}
EMSCRIPTEN_KEEPALIVE void gb_step(float dt, unsigned buttons, int controls) {
  game.controls = static_cast<Controls>(std::clamp(controls, 1, 3));
  const unsigned edges = buttons & ~previous;
  previous = buttons;
  Input in;
  in.left = buttons & 1;
  in.right = buttons & 2;
  in.throttle = buttons & 4;
  in.brake = buttons & 8;
  in.handbrake = buttons & 16;
  in.action = edges & 32;
  in.pause = edges & 64;
  in.back = edges & 128;
  in.auxiliary = edges & (16 | 256);
  game.tick(dt, in);
}
EMSCRIPTEN_KEEPALIVE void gb_blur() {
  previous = 0;
  if (game.mode == Mode::Racing || game.mode == Mode::Countdown) {
    game.resume_mode = game.mode;
    game.mode = Mode::Paused;
  }
}
EMSCRIPTEN_KEEPALIVE int gb_car() { return game.selected_car; }
EMSCRIPTEN_KEEPALIVE int gb_track() { return game.selected_track; }
EMSCRIPTEN_KEEPALIVE int gb_mode() { return int(game.mode); }
EMSCRIPTEN_KEEPALIVE float gb_speed() { return game.speed; }
EMSCRIPTEN_KEEPALIVE int gb_muted() { return game.muted; }
EMSCRIPTEN_KEEPALIVE void gb_toggle_audio() { game.toggle_audio(); }
EMSCRIPTEN_KEEPALIVE int gb_dirty() { return game.save_requested; }
EMSCRIPTEN_KEEPALIVE void gb_saved() { game.save_requested = false; }
EMSCRIPTEN_KEEPALIVE int gb_save_size() { return sizeof(transfer); }
EMSCRIPTEN_KEEPALIVE SaveData *gb_save() {
  transfer = encode_save(game);
  return &transfer;
}
EMSCRIPTEN_KEEPALIVE int gb_load() { return load_save(game, transfer); }
}
