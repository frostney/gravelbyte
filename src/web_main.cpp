#include "game.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <emscripten/emscripten.h>
using namespace rally;
static Game game;
static Renderer renderer;
static std::array<uint16_t, W * H> pixels;
static SaveData transfer;
static unsigned previous = 0;
extern "C" {
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
EMSCRIPTEN_KEEPALIVE int gb_dirty() { return game.save_requested; }
EMSCRIPTEN_KEEPALIVE void gb_saved() { game.save_requested = false; }
EMSCRIPTEN_KEEPALIVE int gb_save_size() { return sizeof(transfer); }
EMSCRIPTEN_KEEPALIVE SaveData *gb_save() {
  transfer = encode_save(game);
  return &transfer;
}
EMSCRIPTEN_KEEPALIVE int gb_load() { return load_save(game, transfer); }
}
