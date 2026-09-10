#include "game.hpp"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "picosystem.hpp"
#include "save_journal.hpp"
#include <cstdio>
#include <cstring>
#if defined(GRAVELBYTE_BENCHMARK) || defined(GRAVELBYTE_SMOKE)
#include "test_driver.hpp"
#endif

static GravelByte::Game GameState;
static GravelByte::Renderer SceneRenderer;
static uint32_t LastUpdateMicroseconds = 0, LastReport = 0, LastSoundMicroseconds = 0;
static uint32_t Frames = 0, SlowFrames = 0, MaximumFrameMicroseconds = 0,
                MaximumDrawMicroseconds = 0;
static uint64_t TotalFrame = 0;
static bool Diagnostics = false;
static GravelByte::GeometryTelemetry RaceGeometry;
[[maybe_unused]] static unsigned VerifiedSaves = 0;
alignas(4) static uint16_t
    SecondFrame[GravelByte::FramebufferWidth * GravelByte::FramebufferHeight];
static uint16_t *BackFrame = SecondFrame;
static uint32_t LastRenderMicroseconds = 0;
static void RenderBackFrame();
// Both journal sectors are beyond the SDK's 12MiB application limit.
static_assert(PICO_FLASH_SIZE_BYTES == 16 * 1024 * 1024,
              "Build for pimoroni_picosystem: saves require its 16 MiB flash");
static constexpr uint32_t SaveOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
static constexpr uint32_t JournalOffset = SaveOffset - FLASH_SECTOR_SIZE;
alignas(4) static uint8_t SavePage[FLASH_PAGE_SIZE];
static const GravelByte::SaveSlot &SaveSlot(int Index) {
  return *reinterpret_cast<const GravelByte::SaveSlot *>(XIP_BASE + JournalOffset +
                                                         Index * FLASH_SECTOR_SIZE);
}
static void WriteSlot(int Target, const GravelByte::SaveSlot &Slot) {
  std::memset(SavePage, 0xff, sizeof(SavePage));
  std::memcpy(SavePage, &Slot, sizeof(Slot));
  // Core 0 only; interrupts must not fetch XIP while flash runs from SRAM.
  uint32_t State = save_and_disable_interrupts();
  flash_range_erase(JournalOffset + Target * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
  flash_range_program(JournalOffset + Target * FLASH_SECTOR_SIZE, SavePage, sizeof(SavePage));
  restore_interrupts(State);
}
[[maybe_unused]] static void PersistBest() {
  static uint32_t RetryAt = 0;
  const uint32_t Now = picosystem::time_us();
  if (!GameState.SaveRequested || (RetryAt && int32_t(Now - RetryAt) < 0))
    return;
  const auto SavedRecord = GravelByte::EncodeSave(GameState);
  const auto Result = GravelByte::StoreSave(SavedRecord, SaveSlot(0), SaveSlot(1), WriteSlot);
  if (Result != GravelByte::SaveResult::Failed) {
    GameState.SaveRequested = false;
    RetryAt = 0;
    if (Result == GravelByte::SaveResult::Saved) {
      ++VerifiedSaves;
#ifdef GRAVELBYTE_SMOKE
      const int Current = GravelByte::NewestSlot(SaveSlot(0), SaveSlot(1));
      std::printf("SAVE_OK count=%u version=%lu muted=%d slot=%d sequence=%lu\n", VerifiedSaves,
                  (unsigned long)SavedRecord.Version, GameState.Muted, Current,
                  (unsigned long)SaveSlot(Current).Sequence);
#endif
    }
  } else {
    RetryAt = Now + 2000000;
    std::printf("SAVE_FAILED: previous record retained\n");
  }
}
void init() {
  stdio_init_all();
  const int Current = GravelByte::NewestSlot(SaveSlot(0), SaveSlot(1));
  if (Current >= 0) {
    GravelByte::LoadSave(GameState, SaveSlot(Current).Data);
  } else {
    const auto *Saved = reinterpret_cast<const GravelByte::SaveRecord *>(XIP_BASE + SaveOffset);
    if (!GravelByte::LoadSave(GameState, *reinterpret_cast<const GravelByte::SaveData *>(Saved)))
      GravelByte::LoadBest(GameState, *Saved);
  }
#ifdef GRAVELBYTE_BENCHMARK
  // Measure the normal audio workload independently of the player's saved mute
  // preference. Benchmark firmware never persists this temporary override.
  GameState.Muted = false;
  GameState.SelectCarAndTrack(GRAVELBYTE_BENCHMARK_START % 3, GRAVELBYTE_BENCHMARK_START / 3);
  GameState.CurrentMode = GravelByte::GameMode::Countdown;
#endif
  LastUpdateMicroseconds = picosystem::time_us();
}
void update(uint32_t) {
  using namespace picosystem;
  uint32_t Now = time_us();
  float DeltaTimeSeconds =
      (Now - LastUpdateMicroseconds) * GravelByte::Tuning::SecondsPerMicrosecond;
  LastUpdateMicroseconds = Now;
  GravelByte::DrivingInput PlayerInput{};
  PlayerInput.Left = button(LEFT);
  PlayerInput.Right = button(RIGHT);
  PlayerInput.Throttle = button(A);
  PlayerInput.Brake = button(B);
  PlayerInput.Handbrake = button(X);
  PlayerInput.Action = pressed(A);
  PlayerInput.Pause = pressed(Y);
  PlayerInput.Back = pressed(B);
  PlayerInput.Auxiliary = pressed(X);
#ifdef GRAVELBYTE_DIAGNOSTICS
  if (button(UP) && pressed(Y)) {
    Diagnostics = !Diagnostics;
    PlayerInput.Pause = false;
  }
#endif
#ifdef GRAVELBYTE_BENCHMARK
  PlayerInput = GravelByte::TestDriver(GameState);
#endif
#ifdef GRAVELBYTE_SMOKE
  static uint32_t SmokeStart = Now;
  static int PreviousPhase = -1;
  const int Phase = int((Now - SmokeStart) / 1000000);
  PlayerInput =
      Phase >= 14 ? GravelByte::CalculateDrivingInput(GameState) : GravelByte::DrivingInput{};
  if (Phase != PreviousPhase) {
    PlayerInput.Auxiliary = Phase == 2 || Phase == 3 || Phase == 11 || Phase == 12;
    PlayerInput.Action = Phase == 4 || Phase == 5 || Phase == 6;
    PlayerInput.Right = Phase == 5;
    PlayerInput.Pause = Phase == 10 || Phase == 13;
    if (Phase == 20)
      std::printf("SMOKE_DONE mode=%d saves=%u racing=%d\n", int(GameState.CurrentMode),
                  VerifiedSaves, GameState.CurrentMode == GravelByte::GameMode::Racing);
    PreviousPhase = Phase;
  }
#endif
  GameState.Update(DeltaTimeSeconds, PlayerInput);
#ifdef GRAVELBYTE_BENCHMARK
  static bool Reported = false;
  static uint32_t FinishedAt = 0;
  if (GameState.CurrentMode == GravelByte::GameMode::Finished && !Reported) {
    std::printf("BENCHMARK_DONE track=%d car=%d frames=%lu mean_us=%lu max_us=%lu below30=%lu "
                "recoveries=%d time_ms=%lu dropped=%lu overflow_frames=%lu rendered_frames=%lu "
                "audio=%d\n",
                GameState.SelectedTrack, GameState.SelectedCar, (unsigned long)Frames,
                (unsigned long)(Frames ? TotalFrame / Frames : 0),
                (unsigned long)MaximumFrameMicroseconds, (unsigned long)SlowFrames,
                GameState.Recoveries, (unsigned long)(GameState.Elapsed * 1000),
                (unsigned long)RaceGeometry.Dropped, (unsigned long)RaceGeometry.OverflowFrames,
                (unsigned long)RaceGeometry.Frames, !GameState.Muted);
    Reported = true;
    FinishedAt = Now;
  }
  if (Reported && Now - FinishedAt > 2000000 &&
      GameState.SelectedTrack * 3 + GameState.SelectedCar < 8) {
    int Next = GameState.SelectedTrack * 3 + GameState.SelectedCar + 1;
    GameState.SelectCarAndTrack(Next % 3, Next / 3);
    Frames = SlowFrames = MaximumFrameMicroseconds = MaximumDrawMicroseconds = 0;
    TotalFrame = 0;
    RaceGeometry = {};
    Reported = false;
    LastUpdateMicroseconds = picosystem::time_us();
  }
#endif

#ifndef GRAVELBYTE_BENCHMARK
  const unsigned SavesBefore = VerifiedSaves;
  PersistBest();
  if (VerifiedSaves != SavesBefore)
    LastUpdateMicroseconds = picosystem::time_us();
#endif
  if (!GameState.Muted &&
      (GameState.CurrentMode == GravelByte::GameMode::Racing ||
       GameState.CurrentMode == GravelByte::GameMode::Title ||
       GameState.CurrentMode == GravelByte::GameMode::Finished) &&
      Now - LastSoundMicroseconds >= GravelByte::Tuning::Audio::UpdateIntervalMicroseconds) {
    LastSoundMicroseconds = Now;
    if (GameState.Impact > .3f)
      play(voice(0, 20, 40, 30, 0, 0, 0, 95, 40), 80, 65, 55);
    else if (GameState.Slip > 2.5f)
      play(voice(0, 0, 80, 10, 0, 0, 0, 70, 10), 400 + int(GameState.Slip * 30), 70, 28);
    else
      play(voice(0, 0, 80, 10, 0, 0, 0, 12, 12),
           int(GravelByte::Tuning::Audio::BaseFrequency) +
               int(GameState.Speed * GravelByte::Tuning::Audio::SpeedFrequency),
           75, 22);
  }
  if (stats.tick_us && GameState.CurrentMode == GravelByte::GameMode::Racing) {
    ++Frames;
    TotalFrame += stats.tick_us;
    if (stats.tick_us > GravelByte::Tuning::Telemetry::MinimumFrameRateIntervalMicroseconds)
      ++SlowFrames;
    if (stats.tick_us > MaximumFrameMicroseconds)
      MaximumFrameMicroseconds = stats.tick_us;
    if (LastRenderMicroseconds > MaximumDrawMicroseconds)
      MaximumDrawMicroseconds = LastRenderMicroseconds;
  }
  if (Now - LastReport >= GravelByte::Tuning::Telemetry::ReportIntervalMicroseconds &&
      stdio_usb_connected()) {
    LastReport = Now;
    std::printf(
        "gravelbyte mode=%d segment=%d frames=%lu mean_us=%lu max_us=%lu max_draw_us=%lu "
        "below30=%lu tris=%d dropped=%d geometry_us=%lu raster_us=%lu tick_us=%lu "
        "time_ms=%lu recoveries=%d jumps=%d split1_ms=%lu split2_ms=%lu split3_ms=%lu "
        "split4_ms=%lu split5_ms=%lu update_us=%lu swap_us=%lu flip_elapsed_us=%lu "
        "wait_percent=%lu clock_hz=%lu\n",
        int(GameState.CurrentMode), GameState.Segment, (unsigned long)Frames,
        (unsigned long)(Frames ? TotalFrame / Frames : 0), (unsigned long)MaximumFrameMicroseconds,
        (unsigned long)MaximumDrawMicroseconds, (unsigned long)SlowFrames, SceneRenderer.FaceCount,
        SceneRenderer.Dropped, (unsigned long)SceneRenderer.GeometryMicroseconds,
        (unsigned long)SceneRenderer.RasterMicroseconds, (unsigned long)stats.tick_us,
        (unsigned long)(GameState.Elapsed * 1000), GameState.Recoveries, GameState.Jumps,
        (unsigned long)(GameState.Splits[0] * 1000), (unsigned long)(GameState.Splits[1] * 1000),
        (unsigned long)(GameState.Splits[2] * 1000), (unsigned long)(GameState.Splits[3] * 1000),
        (unsigned long)(GameState.Splits[4] * 1000), (unsigned long)stats.update_us,
        (unsigned long)stats.draw_us, (unsigned long)stats.flip_us, (unsigned long)stats.idle,
        (unsigned long)clock_get_hz(clk_sys));
  }
  // The SDK is transmitting SCREEN during update(). Render to the other
  // framebuffer so CPU rendering overlaps that DMA transfer without tearing.
  RenderBackFrame();
}
static void RenderBackFrame() {
  const uint32_t Started = picosystem::time_us();
  auto *Pixels = BackFrame;
  SceneRenderer.Render(GameState, Pixels, int(picosystem::stats.fps), Diagnostics);
  if (GameState.CurrentMode == GravelByte::GameMode::Racing)
    RaceGeometry.Observe(SceneRenderer.Dropped);
  // Our shared image is RGBA4444. PicoSystem's SPI-friendly packed order is
  // G B A R (see the SDK's rgb()), so convert in-place only after rendering.
  for (int Index = 0; Index < GravelByte::FramebufferWidth * GravelByte::FramebufferHeight;
       ++Index) {
    uint16_t ColorValue = Pixels[Index];
    Pixels[Index] = uint16_t((ColorValue >> 12) | 0x00f0 | ((ColorValue & 0x00f0) << 4) |
                             ((ColorValue & 0x0f00) << 4));
  }
  LastRenderMicroseconds = picosystem::time_us() - Started;
}
void draw(uint32_t) {
  // SDK calls draw() only after the previous display DMA completes. Its DMA
  // reads SCREEN->data on each scanline, so this is the safe point to swap.
  uint16_t *Previous = picosystem::SCREEN->data;
  picosystem::SCREEN->data = BackFrame;
  BackFrame = Previous;
}
