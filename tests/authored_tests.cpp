#include "game.hpp"
#include "test_driver.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace GravelByte;
static void Check(bool Passed, const char *Message) {
  if (!Passed) {
    std::fprintf(stderr, "FAIL: %s\n", Message);
    std::exit(1);
  }
}
static void Press(Game &GameState, DrivingInput Input) {
  GameState.Update(.02f, {});
  GameState.Update(.02f, Input);
}
int main() {
  Game Preview;
  Preview.SelectCarAndTrack(0, 0);
  Preview.CurrentMode = GameMode::TrackSelect;
  Preview.Update(.02f, {});
  const auto PreviewPosition = Preview.CarPosition;
  for (int Frame = 0; Frame < 200; ++Frame)
    Preview.Update(.02f, {});
  Check(Preview.CarPosition.CoordinateX != PreviewPosition.CoordinateX ||
            Preview.CarPosition.CoordinateZ != PreviewPosition.CoordinateZ,
        "selected course preview moves through the actual scenery");
  Check(Preview.Elapsed == 0 && Preview.SplitCount == 0 && Preview.Best == 0,
        "preview does not run the timer or award records");
  DrivingInput ConfirmPreview;
  ConfirmPreview.Action = true;
  Preview.Update(.02f, ConfirmPreview);
  Check(Preview.CurrentMode == GameMode::Countdown && Preview.Segment == 1 &&
            Preview.CarPosition.CoordinateX == Preview.Road[1].Position.CoordinateX &&
            Preview.Speed == 0,
        "confirm leaves the preview and starts from a stationary grid");
  Game CountdownSave;
  CountdownSave.Restart();
  CountdownSave.SaveRequested = false;
  CountdownSave.Update(.02f, {});
  Check(CountdownSave.CurrentMode == GameMode::Countdown && CountdownSave.DriftHintSeen &&
            CountdownSave.ShowDriftHint && CountdownSave.SaveRequested,
        "first drift hint is persisted during countdown while its display remains visible");
  CountdownSave.SaveRequested = false;
  while (CountdownSave.CurrentMode == GameMode::Countdown)
    CountdownSave.Update(.02f, {});
  Check(!CountdownSave.SaveRequested, "entering racing does not request a flash write");
  Game GameState;
  const auto &Forest = GetStageLayout(0), &Beach = GetStageLayout(1), &Snow = GetStageLayout(2);
  Check(Beach.SegmentLength < Forest.SegmentLength && Forest.SegmentLength < Snow.SegmentLength,
        "coast sprint and winter endurance have different lengths");
  Check(Forest.Checkpoints != Beach.Checkpoints && Forest.Checkpoints != Snow.Checkpoints &&
            Beach.Checkpoints != Snow.Checkpoints,
        "each course has its own five-split rhythm");
  Check(StageWidth(Forest, 47) < StageWidth(Forest, 20), "bridge narrows the usable road");
  Check(StageCurvature(Beach, 139) > 0 && StageCurvature(Beach, 149) < 0,
        "coastal chicane changes direction between adjacent corners");
  Check(std::abs(StageCurvature(Forest, 66.75f)) > std::abs(StageCurvature(Forest, 58.25f)) * 2,
        "forest corner tightens through its apex");
  Check(std::abs(StageCurvature(Snow, 83)) < std::abs(StageCurvature(Snow, 79.5f)) * .5f,
        "winter double apex releases curvature between peaks");
  Check(StageElevation(Forest, Forest.Crest) > StageElevation(Forest, Forest.Crest + 2),
        "authored forest crest has a descending exit");
  GameState.SelectCarAndTrack(0, 1);
  Check(GameState.SurfaceGrip(190) < GameState.SurfaceGrip(180),
        "sand changes grip within the coastal sweeper");
  char Note[64];
  GameState.Segment = 1;
  GameState.PaceNote(Note, sizeof(Note));
  Check(std::strstr(Note, "BRIDGE") == nullptr && std::strstr(Note, "TUNNEL") == nullptr,
        "absent features never produce pace notes");
  GameState.Segment = 133;
  GameState.Speed = 25;
  GameState.PaceNote(Note, sizeof(Note));
  Check(Note[0] == 'R', "pace note chooses first chicane corner, not a later sharper bend");
  GameState.PaceNotes = false;
  GameState.PaceNote(Note, sizeof(Note));
  Check(!Note[0], "disabled pace notes produce no instruction");
  for (int Track = 0; Track < TrackCount; ++Track)
    for (int Car = 0; Car < CarCount; ++Car) {
      GameState.SelectCarAndTrack(Car, Track);
      Check(GameState.MedalTarget(3) < GameState.MedalTarget(2) &&
                GameState.MedalTarget(2) < GameState.MedalTarget(1),
            "medal targets ordered");
      for (int Medal = 1; Medal <= 3; ++Medal)
        Check(GameState.MedalForTime(GameState.MedalTarget(Medal)) == Medal,
              "exact target earns the medal");
      Check(GameState.MedalForTime(GameState.MedalTarget(1) + .1f) == 0 &&
                GameState.MedalForTime(0) == 0,
            "slow and absent runs earn no medal");
      GameState.SteeringAssist = true;
      GameState.SelectCarAndTrack(Car, Track);
      GameState.CurrentMode = GameMode::Racing;
      for (int Frame = 0; Frame < 15000 && GameState.CurrentMode != GameMode::Finished; ++Frame)
        GameState.Update(.02f, TestDriver(GameState));
      Check(GameState.CurrentMode == GameMode::Finished && GameState.Recoveries == 0 &&
                GameState.SplitCount == SectorCount,
            "assisted car finishes every stage without recovery");
      Check(GameState.MedalForTime(GameState.Elapsed) >= 1, "assisted clean drive earns bronze");
      const float AssistedBest = GameState.Best;
      GameState.ToggleAssist();
      Check(GameState.Best == 0, "assisted result never replaces unassisted personal best");
      GameState.ToggleAssist();
      Check(GameState.Best == AssistedBest, "returning to assistance restores its own best");
      std::printf("assisted track=%d car=%d time=%.3f recoveries=%d\n", Track, Car,
                  GameState.Records[GameState.RecordIndex()].Splits.back(), GameState.Recoveries);
    }
  GameState.SelectCarAndTrack(0, 0);
  GameState.StartHeld = false;
  GameState.Velocity = {0, 0, 20};
  GameState.Yaw = 0;
  GameState.SteeringInput = 0;
  GameState.SimulatePhysics(.01f, {});
  Check(GameState.SteeringInput == 0, "assist does not follow road without lateral slip");
  GameState.Velocity = {5, 0, 20};
  GameState.SimulatePhysics(.01f, {});
  Check(GameState.SteeringInput > 0 && GameState.SteeringInput <= .22f,
        "assist applies bounded steering toward lateral momentum");
  GameState.CurrentMode = GameMode::Racing;
  DrivingInput Pause, Down, Confirm, Back;
  Pause.Pause = true;
  Down.Down = true;
  Confirm.Action = true;
  Back.Back = true;
  Press(GameState, Pause);
  Press(GameState, Down);
  Press(GameState, Down);
  Press(GameState, Confirm);
  Check(GameState.OptionsOpen, "pause menu opens options");
  Press(GameState, Down);
  const bool PreviousNotes = GameState.PaceNotes;
  Press(GameState, Confirm);
  Check(GameState.PaceNotes != PreviousNotes && GameState.SaveRequested,
        "pace note option toggles and requests persistence");
  Press(GameState, Back);
  Check(!GameState.OptionsOpen && GameState.CurrentMode == GameMode::Paused,
        "back from options returns to pause");
  Press(GameState, Back);
  Check(GameState.CurrentMode == GameMode::Racing, "back resumes without restarting");
  Game Restored;
  Check(LoadSave(Restored, EncodeSave(GameState)) && Restored.PaceNotes == GameState.PaceNotes &&
            Restored.SteeringAssist == GameState.SteeringAssist,
        "driving options persist");
  GameState.Speed = GameState.GetCarSpecification().MaximumSpeed * .199f;
  const float BeforeShift = GameState.EngineFrequency();
  GameState.Speed = GameState.GetCarSpecification().MaximumSpeed * .201f;
  Check(GameState.EngineFrequency() < BeforeShift - 100, "upshift audibly drops engine pitch");
  GameState.Speed = GameState.GetCarSpecification().MaximumSpeed;
  GameState.CinematicTime = 0;
  const float LimiterFirst = GameState.EngineFrequency();
  GameState.CinematicTime = 1.f / 35.f;
  Check(GameState.EngineFrequency() != LimiterFirst, "rev limiter alternates pitch at top speed");
}
