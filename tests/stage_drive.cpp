#include "game.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// A driver using the public controls, never teleporting or modifying race state.
// This validates that the complete authored course can actually be driven.
int main(int ArgumentCount, char **Arguments) {
  using namespace GravelByte;
  for (int Track = 0; Track < TrackCount; ++Track)
    for (int CarIndex = 0; CarIndex < CarCount; ++CarIndex) {
      Game GameState;
      GameState.SelectCarAndTrack(CarIndex, Track);
      GameState.CurrentMode = GameMode::Countdown;
      DrivingInput Start{};
      Start.Action = true;
      GameState.Update(.02f, Start);
      float MaximumLateralDistance = 0, MaximumAirborneFrames = 0, OffroadSeconds = 0,
            MaximumWidthRatio = 0;
      int AirFrames = 0;
      auto *SceneRenderer = new Renderer;
      std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels{};
      int MaximumTriangles = 0, Frames = 0;
      float LastCapture = -10;
      while (Frames++ < 15000 && GameState.CurrentMode != GameMode::Finished) {
        DrivingInput PlayerInput = TestDriver(GameState);
        GameState.Update(.02f, PlayerInput);
        if (std::abs(GameState.Lateral) > GameState.RoadWidth())
          OffroadSeconds += .02f;
        MaximumWidthRatio =
            std::max(MaximumWidthRatio, std::abs(GameState.Lateral) / GameState.RoadWidth());
        if (GameState.Airborne)
          ++AirFrames;
        MaximumAirborneFrames =
            std::max(MaximumAirborneFrames, GameState.CarPosition.CoordinateY - GameState.GroundY);
        MaximumLateralDistance = std::max(MaximumLateralDistance, std::abs(GameState.Lateral));
        if (Frames % 5 == 0) {
          SceneRenderer->Render(GameState, Pixels.data());
          MaximumTriangles = std::max(MaximumTriangles, SceneRenderer->FaceCount);
          if (SceneRenderer->Dropped) {
            std::fprintf(stderr, "Triangle overflow in stage drive\n");
            return 1;
          }
        }
        if (ArgumentCount > 1 && GameState.Elapsed - LastCapture >= 1) {
          SceneRenderer->Render(GameState, Pixels.data());
          char Filename[512];
          std::snprintf(Filename, sizeof(Filename), "%s/frame-%04d.ppm", Arguments[1],
                        int(GameState.Elapsed));
          if (FILE *FileHandle = std::fopen(Filename, "wb")) {
            std::fprintf(FileHandle, "P6\n120 120\n255\n");
            for (uint16_t ColorValue : Pixels) {
              unsigned char RedGreenBlue[] = {uint8_t((ColorValue >> 12) * 17),
                                              uint8_t(((ColorValue >> 8) & 15) * 17),
                                              uint8_t(((ColorValue >> 4) & 15) * 17)};
              std::fwrite(RedGreenBlue, 1, 3, FileHandle);
            }
            std::fclose(FileHandle);
          }
          LastCapture = GameState.Elapsed;
        }
      }
      std::printf("track=%d car=%d mode=%d time=%.2f segment=%d recoveries=%d max_lateral=%.2f "
                  "max_triangles=%d\n",
                  Track, CarIndex, int(GameState.CurrentMode), GameState.Elapsed, GameState.Segment,
                  GameState.Recoveries, MaximumLateralDistance, MaximumTriangles);
      std::printf("splits=%.3f,%.3f,%.3f,%.3f,%.3f jumps=%d air_frames=%d max_air=%.3f\n",
                  GameState.Splits[0], GameState.Splits[1], GameState.Splits[2],
                  GameState.Splits[3], GameState.Splits[4], GameState.Jumps, AirFrames,
                  MaximumAirborneFrames);
      std::printf("offroad_seconds=%.2f max_width_ratio=%.3f\n", OffroadSeconds, MaximumWidthRatio);
      if (GameState.SplitCount != SectorCount ||
          !(GameState.Splits[0] > 0 && GameState.Splits[0] < GameState.Splits[1] &&
            GameState.Splits[1] < GameState.Splits[2]) ||
          GameState.Jumps < 1 || AirFrames < 2 || MaximumAirborneFrames > 2.f || GameState.Airborne)
        return 1;
      for (int Index = 1; Index < SectorCount; ++Index)
        if (GameState.Splits[Index] <= GameState.Splits[Index - 1])
          return 1;
      Game Restored;
      if (GameState.Elapsed >= GameState.GetDefaultSplits().back()) {
        std::fprintf(stderr,
                     "Reference driver cannot beat target: track=%d car=%d time=%.3f target=%.3f\n",
                     Track, CarIndex, GameState.Elapsed, GameState.GetDefaultSplits().back());
        return 1;
      }
      // Satisfy earlier stages with actual completed runs, then verify that this
      // run's persisted result unlocks the next stage for every car.
      for (int Earlier = 0; Earlier < Track; ++Earlier) {
        Game Prior;
        Prior.SelectCarAndTrack(CarIndex, Earlier);
        Prior.CurrentMode = GameMode::Racing;
        for (int Frame = 0; Frame < 15000 && Prior.CurrentMode != GameMode::Finished; ++Frame)
          Prior.Update(.02f, TestDriver(Prior));
        if (Prior.CurrentMode != GameMode::Finished ||
            Prior.Elapsed >= Prior.GetDefaultSplits().back())
          return 1;
        GameState.Records[Earlier * CarCount + CarIndex] =
            Prior.Records[Earlier * CarCount + CarIndex];
      }
      if (!LoadSave(Restored, EncodeSave(GameState)))
        return 1;
      if (Track + 1 < TrackCount && !Restored.Unlocked(Track + 1))
        return 1;
      Restored.Restart();
      if (std::abs(Restored.Best - GameState.Elapsed) > .002f ||
          std::abs(Restored.BestSplits[1] - GameState.Splits[1]) > .002f)
        return 1;
      delete SceneRenderer;
      if (GameState.CurrentMode != GameMode::Finished || GameState.Recoveries != 0 ||
          GameState.Elapsed > 150 || MaximumLateralDistance > RoadHalfWidth + 1)
        return 1;
    }
}
