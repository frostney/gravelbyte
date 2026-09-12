#include "game.hpp"
#include "ghost.hpp"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "picosystem.hpp"
#include "save_journal.hpp"
#include <cmath>
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
#ifdef GRAVELBYTE_BENCHMARK
static uint32_t GhostRenderedFrames = 0;
#endif
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
alignas(4) static uint8_t SavePage[GravelByte::SaveProgramBytes];
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
static constexpr uint32_t GhostOffset = 12 * 1024 * 1024;
static_assert(GhostOffset + GravelByte::RecordCount * 2 * GravelByte::GhostSlotBytes <=
              JournalOffset);
static const GravelByte::GhostHeader &GhostHeaderAt(int Slot) {
  return *reinterpret_cast<const GravelByte::GhostHeader *>(
      XIP_BASE + GhostOffset + (GameState.RecordIndex() * 2 + Slot) * GravelByte::GhostSlotBytes);
}
static const GravelByte::ReplayPose *GhostPosesAt(int Slot) {
  return reinterpret_cast<const GravelByte::ReplayPose *>(
      reinterpret_cast<const uint8_t *>(&GhostHeaderAt(Slot)) + GravelByte::GhostHeaderBytes);
}
static void RefreshGhost() {
  static uint32_t Generation = UINT32_MAX;
  if (Generation == GameState.GhostLoadGeneration)
    return;
  Generation = GameState.GhostLoadGeneration;
  for (int Slot = 0; Slot < 2; ++Slot) {
    const auto &Header = GhostHeaderAt(Slot);
    if (!GravelByte::ValidGhost(Header, GhostPosesAt(Slot), GameState))
      continue;
    GameState.GhostPoses = GhostPosesAt(Slot);
    GameState.GhostCount = Header.Count;
    GameState.GhostInterval = Header.Interval;
    GameState.GhostDuration = Header.Duration;
    break;
  }
}
[[maybe_unused]] static void PersistGhost() {
  if (!GameState.GhostSaveRequested)
    return;
  GameState.GhostSaveRequested = false;
  const auto Header = GravelByte::MakeGhostHeader(GameState);
  if (Header.Count < 2)
    return;
  const int Target = GravelByte::ValidGhost(GhostHeaderAt(0), GhostPosesAt(0),
                                            GameState.RecordIndex(), GameState.PriorSplits)
                         ? 1
                         : 0;
  const uint32_t Offset =
      GhostOffset + (GameState.RecordIndex() * 2 + Target) * GravelByte::GhostSlotBytes;
  GameState.ClearGhost();
  uint32_t Interrupts = save_and_disable_interrupts();
  flash_range_erase(Offset, GravelByte::GhostSlotBytes);
  restore_interrupts(Interrupts);
  const int Bytes = GravelByte::GhostHeaderBytes + Header.Count * sizeof(GravelByte::ReplayPose);
  for (int Page = GravelByte::GhostHeaderBytes; Page < Bytes; Page += FLASH_PAGE_SIZE) {
    GravelByte::GhostPage(GameState, Header, Page, SavePage, FLASH_PAGE_SIZE);
    Interrupts = save_and_disable_interrupts();
    flash_range_program(Offset + Page, SavePage, FLASH_PAGE_SIZE);
    restore_interrupts(Interrupts);
  }
  // Publish the checked header last; an interrupted payload has no valid header.
  GravelByte::GhostPage(GameState, Header, 0, SavePage, FLASH_PAGE_SIZE);
  Interrupts = save_and_disable_interrupts();
  flash_range_program(Offset, SavePage, FLASH_PAGE_SIZE);
  restore_interrupts(Interrupts);
  if (!GravelByte::ValidGhost(GhostHeaderAt(Target), GhostPosesAt(Target), GameState))
    std::printf("GHOST_SAVE_FAILED: personal best retained without replay\n");
#ifdef GRAVELBYTE_SMOKE
  else
    std::printf("GHOST_SAVE_OK record=%d slot=%d count=%lu\n", GameState.RecordIndex(), Target,
                (unsigned long)Header.Count);
#endif
  LastUpdateMicroseconds = picosystem::time_us();
}
[[maybe_unused]] static void PersistBest() {
  static uint32_t RetryAt = 0;
  const uint32_t Now = picosystem::time_us();
  if (GameState.CurrentMode == GravelByte::GameMode::Racing || !GameState.SaveRequested ||
      (RetryAt && int32_t(Now - RetryAt) < 0))
    return;
  // Keep the expanded journal off the PicoSystem's small interrupt stack.
  static GravelByte::SaveSlot PendingSave;
  GravelByte::EncodeSave(GameState, PendingSave.Data);
  const auto Result =
      GravelByte::StorePreparedSave(PendingSave, SaveSlot(0), SaveSlot(1), WriteSlot);
  if (Result != GravelByte::SaveResult::Failed) {
    GameState.SaveRequested = false;
    RetryAt = 0;
    if (Result == GravelByte::SaveResult::Saved) {
      ++VerifiedSaves;
#ifdef GRAVELBYTE_SMOKE
      const int Current = GravelByte::NewestSlot(SaveSlot(0), SaveSlot(1));
      std::printf("SAVE_OK count=%u version=%lu muted=%d slot=%d sequence=%lu\n", VerifiedSaves,
                  (unsigned long)PendingSave.Data.Version, GameState.Muted, Current,
                  (unsigned long)SaveSlot(Current).Sequence);
#endif
    }
  } else {
    RetryAt = Now + 2000000;
    std::printf("SAVE_FAILED: previous record retained\n");
  }
}
#ifdef GRAVELBYTE_BENCHMARK
static int BenchmarkIndex = GRAVELBYTE_BENCHMARK_START;
#ifdef GRAVELBYTE_BENCHMARK_EXTENDED
static constexpr int BenchmarkCount = 6;
#else
static constexpr int BenchmarkCount = 9;
#endif
static void BeginBenchmark() {
#ifdef GRAVELBYTE_BENCHMARK_EXTENDED
  GameState.SteeringAssist = false;
  if (BenchmarkIndex < 3) {
    GameState.Challenge = false;
    GameState.SelectCarAndTrack(2, BenchmarkIndex);
    GameState.SelectVariant(BenchmarkIndex + 1);
  } else {
    static constexpr uint32_t Seeds[] = {0u, 0x80000000u, 0xffffffffu};
    GameState.SelectedCar = 2;
    GameState.SetChallengeSeed(Seeds[BenchmarkIndex - 3]);
  }
#else
  GameState.SelectCarAndTrack(BenchmarkIndex % 3, BenchmarkIndex / 3);
#endif
  GameState.CurrentMode = GravelByte::GameMode::Countdown;
}
#endif
void init() {
  stdio_init_all();
  GameState.ChallengeSeed = get_rand_32();
  const int Current = GravelByte::NewestSlot(SaveSlot(0), SaveSlot(1));
  if (Current >= 0) {
    GravelByte::LoadSave(GameState, SaveSlot(Current).Data);
  }
#ifdef GRAVELBYTE_BENCHMARK
  // Measure the normal audio workload independently of the player's saved mute
  // preference. Benchmark firmware never persists this temporary override.
#ifdef GRAVELBYTE_TEST_SILENT
  GameState.Muted = true;
#else
  GameState.Muted = false;
#endif
  BeginBenchmark();
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
  PlayerInput.Up = button(UP);
  PlayerInput.Down = button(DOWN);
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
      Phase >= 22 ? GravelByte::CalculateDrivingInput(GameState) : GravelByte::DrivingInput{};
  if (Phase != PreviousPhase) {
    PlayerInput.Auxiliary = Phase == 2 || Phase == 3;
    PlayerInput.Action = Phase == 4 || Phase == 5 || Phase == 6 || Phase == 14 || Phase == 15 ||
                         Phase == 17 || Phase == 19;
    PlayerInput.Down = Phase == 11 || Phase == 12 || Phase == 13 || Phase == 16 || Phase == 18;
    PlayerInput.Back = Phase == 20;
    PlayerInput.Right = Phase == 5;
    PlayerInput.Pause = Phase == 10 || Phase == 21;
    if (Phase == 26)
      std::printf("SMOKE_DONE mode=%d saves=%u racing=%d\n", int(GameState.CurrentMode),
                  VerifiedSaves, GameState.CurrentMode == GravelByte::GameMode::Racing);
    PreviousPhase = Phase;
  }
  static uint32_t SmokeFinished = 0;
  static bool SmokeRestarted = false;
  if (GameState.CurrentMode == GravelByte::GameMode::Finished && !SmokeRestarted) {
    if (!SmokeFinished)
      SmokeFinished = Now;
    if (Now - SmokeFinished > 2000000) {
      PlayerInput.Action = true;
      SmokeRestarted = true;
    }
  }
  if (SmokeRestarted && GameState.CurrentMode == GravelByte::GameMode::Countdown) {
    static bool Announced = false;
    if (!Announced) {
      std::printf("GHOST_RELOAD count=%d record=%d best_ms=%lu\n", GameState.GhostCount,
                  GameState.RecordIndex(), (unsigned long)(GameState.Best * 1000));
      Announced = true;
    }
  }

#endif
  GameState.Update(DeltaTimeSeconds, PlayerInput);
  if (GameState.RandomRequested)
    GameState.SetChallengeSeed(get_rand_32());
#ifdef GRAVELBYTE_BENCHMARK
  static bool Reported = false;
  static uint32_t FinishedAt = 0;
  if (GameState.CurrentMode == GravelByte::GameMode::Finished && !Reported) {
    std::printf(
        "BENCHMARK_DONE track=%d car=%d frames=%lu mean_us=%lu max_us=%lu below30=%lu "
        "recoveries=%d time_ms=%lu dropped=%lu overflow_frames=%lu rendered_frames=%lu "
        "audio=%d ghost_frames=%lu variant=%d challenge=%d seed=%lu\n",
        GameState.SelectedTrack, GameState.SelectedCar, (unsigned long)Frames,
        (unsigned long)(Frames ? TotalFrame / Frames : 0), (unsigned long)MaximumFrameMicroseconds,
        (unsigned long)SlowFrames, GameState.Recoveries, (unsigned long)(GameState.Elapsed * 1000),
        (unsigned long)RaceGeometry.Dropped, (unsigned long)RaceGeometry.OverflowFrames,
        (unsigned long)RaceGeometry.Frames, !GameState.Muted, (unsigned long)GhostRenderedFrames,
        GameState.SelectedVariant, GameState.Challenge, (unsigned long)GameState.ChallengeSeed);
    Reported = true;
    FinishedAt = Now;
  }
  if (Reported && Now - FinishedAt > 2000000 && BenchmarkIndex + 1 < BenchmarkCount) {
    ++BenchmarkIndex;
    BeginBenchmark();
    Frames = SlowFrames = MaximumFrameMicroseconds = MaximumDrawMicroseconds = 0;
    TotalFrame = 0;
    RaceGeometry = {};
    GhostRenderedFrames = 0;
    Reported = false;
    LastUpdateMicroseconds = picosystem::time_us();
  }
#endif

  RefreshGhost();
#ifndef GRAVELBYTE_BENCHMARK
  PersistGhost();
  const unsigned SavesBefore = VerifiedSaves;
  PersistBest();
  if (VerifiedSaves != SavesBefore)
    LastUpdateMicroseconds = picosystem::time_us();
#endif
#ifndef GRAVELBYTE_TEST_SILENT
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
      play(voice(0, 0, 80, 10, 0, 0, 0, 12, 12), int(GameState.EngineFrequency()), 75, 22);
  }
#endif
  if (GameState.CurrentMode == GravelByte::GameMode::Countdown) {
    const uint8_t Amber =
        uint8_t(GravelByte::Tuning::Feedback::CountdownBase +
                GravelByte::Tuning::Feedback::CountdownAmplitude *
                    (1.f + std::sin(GameState.Countdown *
                                    GravelByte::Tuning::Feedback::CountdownPulseRate)));
    led(Amber, Amber / 2, 0);
  } else if (GameState.CurrentMode == GravelByte::GameMode::Finished &&
             GameState.CinematicTime < GravelByte::Tuning::Feedback::FinishSeconds) {
    led(GravelByte::Tuning::Feedback::Brightness, GravelByte::Tuning::Feedback::Brightness,
        GravelByte::Tuning::Feedback::Brightness);
  } else if (GameState.CurrentMode == GravelByte::GameMode::Racing &&
             (!GameState.Challenge || GameState.ReferenceSplits.back() > 0) &&
             GameState.SplitMessage > GravelByte::Tuning::Physics::MessageSeconds -
                                          GravelByte::Tuning::Feedback::SplitSeconds) {
    led(GameState.SplitDelta > 0 ? GravelByte::Tuning::Feedback::Brightness : 0,
        GameState.SplitDelta <= 0 ? GravelByte::Tuning::Feedback::Brightness : 0, 0);
  } else {
    led(0, 0, 0);
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
#ifdef GRAVELBYTE_SMOKE
    if (GameState.CurrentMode != GravelByte::GameMode::Racing && GameState.GhostCount > 1)
      std::printf("GHOST_LOADED record=%d count=%d best_ms=%lu\n", GameState.RecordIndex(),
                  GameState.GhostCount, (unsigned long)(GameState.Best * 1000));
#endif
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
#ifdef GRAVELBYTE_BENCHMARK
  if (GameState.CurrentMode == GravelByte::GameMode::Racing)
    for (int Index = 0; Index < SceneRenderer.FaceCount; ++Index)
      if (SceneRenderer.Faces[Index].Shadow & 0xc000) {
        ++GhostRenderedFrames;
        break;
      }
#endif
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
