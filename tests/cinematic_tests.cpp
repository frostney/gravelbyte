#include "game.hpp"
#include "test_driver.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
using namespace GravelByte;
static void Check(bool Passed, const char *FailureReason) {
  if (!Passed) {
    std::fprintf(stderr, "FAIL: %s\n", FailureReason);
    std::exit(1);
  }
}
int main() {
  auto OwnedGame = std::make_unique<Game>();
  Game &GameState = *OwnedGame;
  Check(GameState.Unlocked(0) && !GameState.Unlocked(1) && !GameState.Unlocked(2),
        "fresh progression");
  GameState.SelectCarAndTrack(2, 2);
  GameState.CurrentMode = GameMode::TrackSelect;
  DrivingInput Confirm;
  Confirm.Action = true;
  GameState.Update(.02f, Confirm);
  Check(GameState.CurrentMode == GameMode::TrackSelect,
        "locked track can be browsed but not started");
  GameState.SelectCarAndTrack(0, 0);
  GameState.Records[0].Splits = GameState.GetDefaultSplits();
  Check(!GameState.Unlocked(1), "matching the target is not beating it");
  for (float &TimeValue : GameState.Records[0].Splits)
    TimeValue *= .99f;
  Check(GameState.Unlocked(1) && !GameState.Unlocked(2), "any car unlocks next track globally");
  GameState.SelectCarAndTrack(1, 1);
  GameState.Records[4].Splits = GameState.GetDefaultSplits();
  for (float &TimeValue : GameState.Records[4].Splits)
    TimeValue *= .99f;
  GameState.ToggleAudio();
  SaveData Save = EncodeSave(GameState);
  Game Restored;
  Check(LoadSave(Restored, Save) && Restored.Muted && Restored.Unlocked(2),
        "audio and progression survive reload");
  SaveData Legacy{};
  FILE *Fixture = std::fopen(GRAVELBYTE_V1_FIXTURE, "rb");
  Check(Fixture && std::fread(&Legacy, sizeof(Legacy), 1, Fixture) == 1,
        "read pre-update save fixture");
  std::fclose(Fixture);
  Check(LoadSave(Restored, Legacy) && !Restored.Muted && Restored.SelectedCar == 2 &&
            Restored.SelectedTrack == 1 && Restored.Unlocked(1) &&
            std::abs(Restored.Records[1].Splits.back() - 92.f) < .001f,
        "v1 saves migrate selections, records and earned unlocks");
  GameState.CurrentMode = GameMode::Title;
  bool Seen[TrackCount]{};
  for (int Index = 0; Index < 2400; ++Index) {
    GameState.Update(.02f, {});
    Seen[GameState.SelectedTrack] = true;
  }
  Check(Seen[0] && Seen[1] && Seen[2], "title showcases all tracks");
  Check(GameState.Elapsed == 0, "attract driving does not race");
  auto After = EncodeSave(GameState);
  Check(std::memcmp(&Save, &After, sizeof(Save)) == 0,
        "showcase preserves selected course, settings and records");
  GameState.Update(.02f, Confirm);
  Check(GameState.CurrentMode == GameMode::CarSelect && GameState.SelectedTrack == 1 &&
            GameState.SelectedCar == 1,
        "continue restores player's selections");
  GameState.SelectCarAndTrack(1, 0);
  std::vector<Vector3> Driven;
  while (GameState.CurrentMode != GameMode::Racing)
    GameState.Update(.02f, {});
  Driven.push_back(GameState.CarPosition);
  while (GameState.CurrentMode != GameMode::Finished && Driven.size() < 10000) {
    GameState.Update(.02f, CalculateDrivingInput(GameState));
    Driven.push_back(GameState.CarPosition);
  }
  Check(GameState.CurrentMode == GameMode::Finished && !GameState.ShowRecords,
        "finish starts with unobscured replay");
  const auto Result = GameState.Splits;
  auto ResultSave = EncodeSave(GameState);
  auto SceneRenderer = std::make_unique<Renderer>();
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels;
  for (int Frame = 25; Frame < int(Driven.size()) - 25; Frame += 25) {
    GameState.ReplayTime = Frame * .02f - .001f;
    GameState.ReplayTick(.001f);
    Vector3 Delta = GameState.CarPosition - Driven[Frame];
    Check(std::sqrt(Delta.CoordinateX * Delta.CoordinateX + Delta.CoordinateY * Delta.CoordinateY +
                    Delta.CoordinateZ * Delta.CoordinateZ) < .3f,
          "replay follows actual driving within 30cm");
    for (int Shot = 0; Shot < 3; ++Shot) {
      GameState.CinematicTime = Shot * Tuning::ShotSeconds;
      SceneRenderer->Render(GameState, Pixels.data());
      Check(SceneRenderer->Dropped == 0, "cinematic geometry fits budget");
    }
  }
  GameState.ReplayTime = 0;
  Vector3 PreviousCamera{};
  for (int Frame = 0; Frame < 800; ++Frame) {
    GameState.ReplayTick(.02f);
    GameState.CinematicTime = Tuning::ShotSeconds;
    SceneRenderer->Render(GameState, Pixels.data());
    if (Frame) {
      Vector3 Move = SceneRenderer->Camera - PreviousCamera;
      Check(std::sqrt(Move.CoordinateX * Move.CoordinateX + Move.CoordinateY * Move.CoordinateY +
                      Move.CoordinateZ * Move.CoordinateZ) < 2.f,
            "roadside camera follows smoothly across road nodes");
    }
    PreviousCamera = SceneRenderer->Camera;
  }
  DrivingInput AuxiliaryLabel;
  AuxiliaryLabel.Auxiliary = true;
  GameState.Update(.02f, AuxiliaryLabel);
  Check(GameState.ShowRecords, "records toggle on");
  GameState.Update(.02f, AuxiliaryLabel);
  Check(!GameState.ShowRecords && GameState.Splits == Result,
        "records toggle off without changing results");
  After = EncodeSave(GameState);
  Check(std::memcmp(&ResultSave, &After, sizeof(After)) == 0, "replay never changes saved result");
  DrivingInput Back;
  Back.Back = true;
  GameState.Update(.02f, Back);
  Check(GameState.CurrentMode == GameMode::TrackSelect,
        "finish back returns to existing track selector");
  for (int Track : {0, 2}) {
    GameState.SelectCarAndTrack(1, Track);
    GameState.Segment = Track == 0 ? Tuning::BridgeStart + 3 : Tuning::TunnelStart + 3;
    GameState.CarPosition =
        GameState.Roadside(GameState.Segment, GameState.Road[GameState.Segment].HalfWidth + 1);
    GameState.Locate();
    GameState.SimulatePhysics(.01f, {});
    Check(std::abs(GameState.Lateral) < GameState.RoadWidth(),
          "bridge rails and tunnel walls constrain car");
  }
  for (int Track = 0; Track < TrackCount; ++Track) {
    GameState.SelectCarAndTrack(1, Track);
    GameState.CurrentMode = GameMode::Finished;
    for (int NodeIndex = 5; NodeIndex < NodeCount - 5; NodeIndex += 3) {
      GameState.Segment = NodeIndex;
      GameState.CarPosition = GameState.Road[NodeIndex].Position;
      GameState.CameraYaw = GameState.Yaw = GameState.Road[NodeIndex].Heading;
      GameState.CameraHeight = GameState.CarPosition.CoordinateY;
      for (int Shot = 0; Shot < 3; ++Shot) {
        GameState.CinematicTime = Shot * Tuning::ShotSeconds;
        SceneRenderer->Render(GameState, Pixels.data());
        Check(SceneRenderer->Dropped == 0, "every cinematic shot fits on every course");
        if (GameState.Tunnel(NodeIndex))
          Check(SceneRenderer->Camera.CoordinateY <
                    GameState.CarPosition.CoordinateY + Tuning::TunnelHeight,
                "tunnel camera clears ceiling");
      }
    }
  }
  // A long stationary run exercises replay compaction, including the final partial sample.
  GameState.SelectCarAndTrack(1, 0);
  GameState.CurrentMode = GameMode::Racing;
  for (int Index = 0; Index < 26000; ++Index)
    GameState.Update(.02f, {});
  GameState.RecordPose(true);
  Check(GameState.ReplayCount <= int(Tuning::ReplayCapacity) &&
            GameState.ReplayInterval > Tuning::ReplayInterval,
        "long recordings stay bounded");
  GameState.ReplayTime = GameState.ReplayDuration - .02f;
  GameState.ReplayTick(.01f);
  Check(std::isfinite(GameState.CarPosition.CoordinateX) && GameState.Segment < NodeCount,
        "compacted replay remains valid");
  std::puts("PASS: progression, persistent sound, actual-run replay, cinematic geometry, physical "
            "landmarks");
}
