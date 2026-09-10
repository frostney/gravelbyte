#include "game.hpp"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "picosystem.hpp"
#include <cstdio>
#include <cstring>
#if defined(GRAVELBYTE_BENCHMARK) || defined(GRAVELBYTE_SMOKE)
#include "test_driver.hpp"
#endif

static rally::Game game;
static rally::Renderer renderer;
static uint32_t last_us = 0, last_report = 0, last_sound_us = 0;
static uint32_t frames = 0, slow_frames = 0, max_frame = 0, max_draw = 0;
static uint64_t total_frame = 0;
static bool diagnostics = false;
[[maybe_unused]] static unsigned verified_saves = 0;
alignas(4) static uint16_t second_frame[rally::W * rally::H];
static uint16_t *back_frame = second_frame;
static uint32_t last_render_us = 0;
static void render_back_frame();
// PicoSystem's linker caps the application at 12MiB. The final flash sector is
// outside the firmware image and is only touched when a best time improves.
static_assert(PICO_FLASH_SIZE_BYTES == 16 * 1024 * 1024,
              "Build for pimoroni_picosystem: saves require its 16 MiB flash");
static constexpr uint32_t SaveOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
static uint8_t save_page[((sizeof(rally::SaveData) + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE) *
                         FLASH_PAGE_SIZE];
[[maybe_unused]] static void persist_best() {
  if (!game.save_requested)
    return;
  std::memset(save_page, 0xff, sizeof(save_page));
  auto record = rally::encode_save(game);
  const auto *existing = reinterpret_cast<const rally::SaveData *>(XIP_BASE + SaveOffset);
  if (std::memcmp(existing, &record, sizeof(record)) == 0) {
    game.save_requested = false;
    return;
  }

  std::memcpy(save_page, &record, sizeof(record));
  // SDK uses core 0 only. Interrupts (including USB/audio) must not fetch from
  // XIP during erase/program; Pico SDK's flash routines execute from SRAM.
  uint32_t state = save_and_disable_interrupts();
  flash_range_erase(SaveOffset, FLASH_SECTOR_SIZE);
  flash_range_program(SaveOffset, save_page, sizeof(save_page));
  restore_interrupts(state);
  auto *stored = reinterpret_cast<const rally::SaveData *>(XIP_BASE + SaveOffset);
  if (std::memcmp(stored, &record, sizeof(record)) == 0) {
    game.save_requested = false;
    ++verified_saves;
#ifdef GRAVELBYTE_SMOKE
    std::printf("SAVE_OK count=%u version=%lu muted=%d\n", verified_saves,
                (unsigned long)record.version, game.muted);
#endif
  }
}
void init() {
  stdio_init_all();
  auto *saved = reinterpret_cast<const rally::SaveRecord *>(XIP_BASE + SaveOffset);
  if (!rally::load_save(game, *reinterpret_cast<const rally::SaveData *>(saved)))
    rally::load_best(game, *saved);
#ifdef GRAVELBYTE_BENCHMARK
  game.select(GRAVELBYTE_BENCHMARK_START % 3, GRAVELBYTE_BENCHMARK_START / 3);
  game.mode = rally::Mode::Countdown;
#endif
  last_us = picosystem::time_us();
}
void update(uint32_t) {
  using namespace picosystem;
  uint32_t now = time_us();
  float dt = (now - last_us) * rally::tuning::SecondsPerMicrosecond;
  last_us = now;
  rally::Input input{};
  input.left = button(LEFT);
  input.right = button(RIGHT);
  input.throttle = button(A);
  input.brake = button(B);
  input.handbrake = button(X);
  input.action = pressed(A);
  input.pause = pressed(Y);
  input.back = pressed(B);
  input.auxiliary = pressed(X);
  if (button(UP) && pressed(Y)) {
    diagnostics = !diagnostics;
    input.pause = false;
  }
#ifdef GRAVELBYTE_BENCHMARK
  input = rally::test_driver(game);
#endif
#ifdef GRAVELBYTE_SMOKE
  static uint32_t smoke_start = now;
  static int previous_phase = -1;
  const int phase = int((now - smoke_start) / 1000000);
  input = phase >= 14 ? rally::driving_input(game) : rally::Input{};
  if (phase != previous_phase) {
    input.auxiliary = phase == 2 || phase == 3 || phase == 11 || phase == 12;
    input.action = phase == 4 || phase == 5 || phase == 6;
    input.right = phase == 5;
    input.pause = phase == 10 || phase == 13;
    if (phase == 20)
      std::printf("SMOKE_DONE mode=%d saves=%u racing=%d\n", int(game.mode), verified_saves,
                  game.mode == rally::Mode::Racing);
    previous_phase = phase;
  }
#endif
  game.tick(dt, input);
#ifdef GRAVELBYTE_BENCHMARK
  static bool reported = false;
  static uint32_t finished_at = 0;
  if (game.mode == rally::Mode::Finished && !reported) {
    std::printf("BENCHMARK_DONE track=%d car=%d frames=%lu mean_us=%lu max_us=%lu below30=%lu "
                "recoveries=%d time_ms=%lu dropped=%d\n",
                game.selected_track, game.selected_car, (unsigned long)frames,
                (unsigned long)(frames ? total_frame / frames : 0), (unsigned long)max_frame,
                (unsigned long)slow_frames, game.recoveries, (unsigned long)(game.elapsed * 1000),
                renderer.dropped);
    reported = true;
    finished_at = now;
  }
  if (reported && now - finished_at > 2000000 && game.selected_track * 3 + game.selected_car < 8) {
    int next = game.selected_track * 3 + game.selected_car + 1;
    game.select(next % 3, next / 3);
    frames = slow_frames = max_frame = max_draw = 0;
    total_frame = 0;
    reported = false;
    last_us = picosystem::time_us();
  }
#endif

#ifndef GRAVELBYTE_BENCHMARK
  const unsigned saves_before = verified_saves;
  persist_best();
  if (verified_saves != saves_before)
    last_us = picosystem::time_us();
#endif
  if (!game.muted &&
      (game.mode == rally::Mode::Racing || game.mode == rally::Mode::Title ||
       game.mode == rally::Mode::Finished) &&
      now - last_sound_us >= rally::tuning::audio::UpdateUs) {
    last_sound_us = now;
    if (game.impact > .3f)
      play(voice(0, 20, 40, 30, 0, 0, 0, 95, 40), 80, 65, 55);
    else if (game.slip > 2.5f)
      play(voice(0, 0, 80, 10, 0, 0, 0, 70, 10), 400 + int(game.slip * 30), 70, 28);
    else
      play(voice(0, 0, 80, 10, 0, 0, 0, 12, 12),
           int(rally::tuning::audio::BaseFrequency) +
               int(game.speed * rally::tuning::audio::SpeedFrequency),
           75, 22);
  }
  if (stats.tick_us && game.mode == rally::Mode::Racing) {
    ++frames;
    total_frame += stats.tick_us;
    if (stats.tick_us > rally::tuning::telemetry::MinFpsFrameUs)
      ++slow_frames;
    if (stats.tick_us > max_frame)
      max_frame = stats.tick_us;
    if (last_render_us > max_draw)
      max_draw = last_render_us;
  }
  if (now - last_report >= rally::tuning::telemetry::ReportUs && stdio_usb_connected()) {
    last_report = now;
    std::printf("gravelbyte mode=%d segment=%d frames=%lu mean_us=%lu max_us=%lu max_draw_us=%lu "
                "below30=%lu tris=%d dropped=%d geometry_us=%lu raster_us=%lu tick_us=%lu "
                "time_ms=%lu recoveries=%d jumps=%d split1_ms=%lu split2_ms=%lu split3_ms=%lu "
                "split4_ms=%lu split5_ms=%lu\n",
                int(game.mode), game.segment, (unsigned long)frames,
                (unsigned long)(frames ? total_frame / frames : 0), (unsigned long)max_frame,
                (unsigned long)max_draw, (unsigned long)slow_frames, renderer.face_count,
                renderer.dropped, (unsigned long)renderer.geometry_us,
                (unsigned long)renderer.raster_us, (unsigned long)stats.tick_us,
                (unsigned long)(game.elapsed * 1000), game.recoveries, game.jumps,
                (unsigned long)(game.splits[0] * 1000), (unsigned long)(game.splits[1] * 1000),
                (unsigned long)(game.splits[2] * 1000), (unsigned long)(game.splits[3] * 1000),
                (unsigned long)(game.splits[4] * 1000));
  }
  // The SDK is transmitting SCREEN during update(). Render to the other
  // framebuffer so CPU rendering overlaps that DMA transfer without tearing.
  render_back_frame();
}
static void render_back_frame() {
  const uint32_t started = picosystem::time_us();
  auto *pixels = back_frame;
  renderer.render(game, pixels, int(picosystem::stats.fps), diagnostics);
  // Our shared image is RGBA4444. PicoSystem's SPI-friendly packed order is
  // G B A R (see the SDK's rgb()), so convert in-place only after rendering.
  for (int i = 0; i < rally::W * rally::H; ++i) {
    uint16_t c = pixels[i];
    pixels[i] = uint16_t((c >> 12) | 0x00f0 | ((c & 0x00f0) << 4) | ((c & 0x0f00) << 4));
  }
  last_render_us = picosystem::time_us() - started;
}
void draw(uint32_t) {
  // SDK calls draw() only after the previous display DMA completes. Its DMA
  // reads SCREEN->data on each scanline, so this is the safe point to swap.
  uint16_t *previous = picosystem::SCREEN->data;
  picosystem::SCREEN->data = back_frame;
  back_frame = previous;
}
