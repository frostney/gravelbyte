#include "game.hpp"
#include "test_driver.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace rally;
static Game game;
static Renderer renderer;
static std::array<uint16_t, W * H> framebuffer;
static std::array<uint32_t, W * H> rgba;
static std::string save_path;
static bool muted = false;
static float audio_phase = 0;
static SDL_AudioDeviceID audio_device = 0;
static float sound_frequency = 70, sound_volume = 0;
static void audio(void *, Uint8 *stream, int bytes) {
  auto *samples = reinterpret_cast<int16_t *>(stream);
  for (int i = 0; i < bytes / 2; ++i) {
    audio_phase += sound_frequency / 22050.f;
    if (audio_phase >= 1)
      audio_phase -= 1;
    samples[i] = int16_t((audio_phase < .45f ? 1 : -1) * sound_volume * 1800);
  }
}
static void save() {
  if (!game.save_requested)
    return;
  SaveData record = encode_save(game);
  std::string temporary = save_path + ".tmp";
  if (FILE *file = std::fopen(temporary.c_str(), "wb")) {
    const bool written = std::fwrite(&record, sizeof(record), 1, file) == 1;
    const bool closed = std::fclose(file) == 0;
    if (written && closed && std::rename(temporary.c_str(), save_path.c_str()) == 0)
      game.save_requested = false;
  }
}
static void screenshot(const char *path) {
  FILE *f = std::fopen(path, "wb");
  if (!f) {
    std::perror(path);
    std::exit(2);
  }
  std::fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (uint16_t c : framebuffer) {
    unsigned char rgb[] = {uint8_t((c >> 12) * 17), uint8_t(((c >> 8) & 15) * 17),
                           uint8_t(((c >> 4) & 15) * 17)};
    std::fwrite(rgb, 1, 3, f);
  }
  std::fclose(f);
}
int main(int argc, char **argv) {
  game.controls = Controls::Keyboard;
  if (argc > 1 && std::strcmp(argv[1], "--capture") == 0) {
    int segment = argc > 3 ? std::atoi(argv[3]) : 20;
    segment = std::max(0, std::min(NodeCount - 2, segment));
    if (argc > 6)
      game.select(std::atoi(argv[5]), std::atoi(argv[6]));
    game.segment = segment;
    game.car = game.road[segment].p;
    game.yaw = game.road[segment].heading;
    game.camera_yaw = game.yaw;
    game.ground_y = game.car.y;
    game.camera_height = game.car.y;
    game.pitch = (game.road[segment + 1].p.y - game.road[segment].p.y) / Step;
    game.roll = game.road[segment].bank;
    game.mode = Mode::Racing;
    game.elapsed = 32.45f;
    game.speed = 22.8f;
    if (argc > 4 && std::strcmp(argv[4], "cars") == 0)
      game.mode = Mode::CarSelect;
    if (argc > 4 && std::strcmp(argv[4], "tracks") == 0)
      game.mode = Mode::TrackSelect;
    if (argc > 4 && std::strcmp(argv[4], "title") == 0)
      game.mode = Mode::Title;
    if (argc > 4 && std::strcmp(argv[4], "finish") == 0) {
      game = Game{};
      for (int i = 0; i < 15000 && game.mode != Mode::Finished; ++i)
        game.tick(.02f, test_driver(game));
    }
    if (argc > 4 && std::strcmp(argv[4], "pause") == 0)
      game.mode = Mode::Paused;
    renderer.render(game, framebuffer.data());
    screenshot(argc > 2 ? argv[2] : "frame.ppm");
    std::printf("Captured %d triangles; %d dropped\n", renderer.face_count, renderer.dropped);
    return 0;
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    return 1;
  }
  char *prefs = SDL_GetPrefPath("gravelbyte", "gravelbyte");
  if (!prefs) {
    std::fprintf(stderr, "No writable save directory: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  save_path = std::string(prefs) + "records-v1.best";
  SDL_free(prefs);
  if (FILE *file = std::fopen(save_path.c_str(), "rb")) {
    SaveData record{};
    if (std::fread(&record, sizeof(record), 1, file) == 1)
      load_save(game, record);
    std::fclose(file);
  }
  SDL_Window *window = SDL_CreateWindow("gravelbyte — PicoSystem preview", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 720, 720,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer *screen =
      window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
             : nullptr;
  if (!screen) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(screen, W, H);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Texture *texture =
      SDL_CreateTexture(screen, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, W, H);
  if (!texture) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_AudioSpec spec{};
  spec.freq = 22050;
  spec.format = AUDIO_S16SYS;
  spec.channels = 1;
  spec.samples = 512;
  spec.callback = audio;
  audio_device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
  if (audio_device)
    SDL_PauseAudioDevice(audio_device, 0);
  bool running = true, diagnostics = false;
  uint64_t last = SDL_GetPerformanceCounter();
  while (running) {
    Input input{};
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
          (game.mode == Mode::Racing || game.mode == Mode::Countdown))
        input.pause = true;
      if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_z)
          input.action = true;
        if (e.key.keysym.sym == SDLK_ESCAPE)
          input.back = true;
        if (e.key.keysym.sym == SDLK_p)
          input.pause = true;
        if (e.key.keysym.sym == SDLK_F1)
          diagnostics = !diagnostics;
        if (e.key.keysym.sym == SDLK_m)
          muted = !muted;
      }
    }
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    input.left = keys[SDL_SCANCODE_LEFT];
    input.right = keys[SDL_SCANCODE_RIGHT];
    input.throttle = keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_UP];
    input.brake = keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_DOWN];
    input.handbrake = keys[SDL_SCANCODE_SPACE];
    uint64_t now = SDL_GetPerformanceCounter();
    float dt = float(double(now - last) / double(SDL_GetPerformanceFrequency()));
    last = now;
    game.tick(dt, input);
    save();
    if (audio_device) {
      SDL_LockAudioDevice(audio_device);
      sound_frequency = 65 + game.speed * 8;
      sound_volume = (!muted && game.mode == Mode::Racing) ? (game.impact > 0 ? .7f : .25f) : 0;
      SDL_UnlockAudioDevice(audio_device);
    }
    renderer.render(game, framebuffer.data(), dt > 0 ? int(1 / dt) : 0, diagnostics);
    for (int i = 0; i < W * H; ++i) {
      uint16_t c = framebuffer[i];
      rgba[i] = 0xff000000u | ((c >> 12) * 17u << 16) | (((c >> 8) & 15) * 17u << 8) |
                (((c >> 4) & 15) * 17u);
    }
    SDL_UpdateTexture(texture, nullptr, rgba.data(), W * 4);
    SDL_SetRenderDrawColor(screen, 12, 18, 18, 255);
    SDL_RenderClear(screen);
    SDL_RenderCopy(screen, texture, nullptr, nullptr);
    SDL_RenderPresent(screen);
  }
  if (audio_device)
    SDL_CloseAudioDevice(audio_device);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(screen);
  SDL_DestroyWindow(window);
  SDL_Quit();
}
