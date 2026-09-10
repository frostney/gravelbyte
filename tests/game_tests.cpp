#include "game.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace Rally;
static void Check(bool Condition, const char *Message) {
  if (!Condition) {
    std::fprintf(stderr, "FAIL: %s\n", Message);
    std::exit(1);
  }
}
static Game Running() {
  Game GameState;
  GameState.Restart();
  GameState.Update(3.1f, {});
  return GameState;
}
int main() {
  Game GameState;
  Check(GameState.CurrentMode == GameMode::Title, "boot waits on title");
  GameState.Update(10, {});
  Check(GameState.Elapsed == 0, "title does not start timer");
  DrivingInput AccelerateInput{};
  AccelerateInput.Action = true;
  AccelerateInput.Throttle = true;
  GameState.Update(.02f, AccelerateInput);
  Check(GameState.CurrentMode == GameMode::CarSelect, "A opens car selection");
  GameState.Update(.02f, AccelerateInput);
  Check(GameState.CurrentMode == GameMode::TrackSelect, "confirm chooses car");
  GameState.Update(.02f, AccelerateInput);
  Check(GameState.CurrentMode == GameMode::Countdown, "confirm starts countdown");
  GameState.Update(2, {});
  Check(GameState.CurrentMode == GameMode::Countdown && GameState.Elapsed == 0,
        "countdown excludes stage time");
  GameState.Update(1.1f, {});
  Check(GameState.CurrentMode == GameMode::Racing, "countdown enters race");
  for (int Index = 0; Index < 100; ++Index)
    GameState.Update(.02f, AccelerateInput);
  Check(GameState.Speed > 10, "throttle builds speed");
  DrivingInput PauseInput{};
  PauseInput.Pause = true;
  GameState.Update(.02f, PauseInput);
  float Elapsed = GameState.Elapsed;
  Vector3 Position = GameState.CarPosition;
  DrivingInput Held{};
  Held.Throttle = true;
  GameState.Update(5, Held);
  Check(GameState.Elapsed == Elapsed && GameState.CarPosition.CoordinateZ == Position.CoordinateZ,
        "pause freezes timer and physics");
  GameState.Update(.02f, PauseInput);
  Check(GameState.CurrentMode == GameMode::Racing, "resume returns to racing");
  DrivingInput Brake{};
  Brake.Brake = true;
  float Before = GameState.Speed;
  for (int Index = 0; Index < 20; ++Index)
    GameState.Update(.02f, Brake);
  Check(GameState.Speed < Before, "braking reduces speed");
  for (int Index = 0; Index < 150; ++Index)
    GameState.Update(.02f, Brake);
  Check(GameState.Velocity.CoordinateX * std::sin(GameState.Yaw) +
                GameState.Velocity.CoordinateZ * std::cos(GameState.Yaw) <
            0,
        "brake reverses after stopping");
  Game Fast = Running(), Slow = Running();
  for (int Index = 0; Index < 200; ++Index)
    Fast.Update(.01f, AccelerateInput);
  for (int Index = 0; Index < 50; ++Index)
    Slow.Update(.04f, AccelerateInput);
  Check(std::abs(Fast.CarPosition.CoordinateZ - Slow.CarPosition.CoordinateZ) < .02f,
        "physics consistent at 25 and 100 updates per second");
  Check(std::abs(Fast.Elapsed - Slow.Elapsed) < .001f, "timing independent of update rate");
  Game Drift = Running();
  for (int Index = 0; Index < 100; ++Index)
    Drift.Update(.02f, AccelerateInput);
  DrivingInput Slide = AccelerateInput;
  Slide.Left = true;
  Slide.Handbrake = true;
  for (int Index = 0; Index < 25; ++Index)
    Drift.Update(.02f, Slide);
  Check(Drift.Slip > 1, "handbrake corner creates lateral slip");
  Game Traction = Running();
  Traction.Velocity = {20.f, 0, 0};
  Traction.SimulatePhysics(.01f, {});
  Check(Traction.Velocity.CoordinateX >= 20.f - 8.5f * .01f - .0001f,
        "sideways tyre force cannot exceed gravel traction limit");
  Game OutsideRoad = Running();
  OutsideRoad.CarPosition = OutsideRoad.Roadside(1, 50);
  OutsideRoad.Locate();
  OutsideRoad.Update(.02f, AccelerateInput);
  Check(OutsideRoad.Recoveries == 1 && OutsideRoad.Elapsed >= 3, "stranded recovery adds penalty");
  Check(std::abs(OutsideRoad.Lateral) < .1f, "recovery puts car on road");
  SaveRecord SavedRecord = EncodeBest(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  Check(std::abs(DecodeBest(SavedRecord) - 92.345f) < .001f, "best time record round trip");
  SavedRecord.Milliseconds ^= 1;
  Check(DecodeBest(SavedRecord) == 0, "damaged save is rejected");
  SaveRecord Blank{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
  Check(DecodeBest(Blank) == 0, "erased flash means no record");
  Game Loaded;
  LoadBest(Loaded, EncodeBest(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f}));
  Loaded.Restart();
  Check(std::abs(Loaded.ReferenceSplits[1] - 36) < .001f,
        "saved fastest run supplies sector benchmark");
  Game Slower;
  LoadBest(Slower, EncodeBest(120.f, {24.f, 48.f, 72.f, 96.f, 120.f}));
  Slower.Restart();
  Check(Slower.ReferenceSplits == DefaultSplits,
        "slower personal best does not replace built-in target");
  SavedRecord = EncodeBest(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  SavedRecord.Splits[0] ^= 1;
  Check(DecodeBest(SavedRecord) == 0, "corrupt split rejected");
  SavedRecord = EncodeBest(92.345f, {18.f, 36.f, 35.f, 74.f, 92.345f});
  Check(DecodeBest(SavedRecord) == 0, "non-monotonic split rejected");
  SavedRecord = EncodeBest(92.345f, {18.f, 36.f, 54.f, 74.f, 92.345f});
  SavedRecord.Course = 1;
  Check(DecodeBest(SavedRecord) == 0, "old course times are not comparable");
  Game Sector = Running();
  Sector.Segment = SectorEnds[0];
  Sector.Furthest = Sector.Segment;
  Sector.CarPosition =
      Sector.Road[Sector.Segment].Position +
      (Sector.Road[Sector.Segment + 1].Position - Sector.Road[Sector.Segment].Position) * .2f;
  Sector.Elapsed = 31;
  Sector.Update(.02f, {});
  Check(Sector.SplitCount == 1 && Sector.SplitMessage > 0, "sector crossing records a split");
  float FirstSplit = Sector.Splits[0];
  Sector.Recover();
  Sector.Update(.02f, {});
  Check(Sector.SplitCount == 1 && Sector.Splits[0] == FirstSplit,
        "recovery does not duplicate split");
  Check(Sector.Elapsed > 34, "recovery penalty remains in total time");
  Game Finish = Running();
  Finish.SplitCount = SectorCount - 1;
  Finish.Splits = {18.f, 36.f, 54.f, 74.f, 0.f};
  Finish.Segment = NodeCount - 3;
  Finish.Furthest = NodeCount - 4;
  Finish.CarPosition = Finish.Road[NodeCount - 3].Position;
  Finish.Yaw = Finish.Road[NodeCount - 3].Heading;
  Finish.Elapsed = 90;
  Finish.Update(.02f, {});
  Check(Finish.CurrentMode == GameMode::Finished && Finish.SaveRequested,
        "finish requests best-time save");
  Elapsed = Finish.Elapsed;
  Finish.Update(2, {});
  Check(Finish.Elapsed == Elapsed, "finish time stops");
  Check(Finish.NewRecord && Finish.Best > 90, "first completed stage sets best");
  Finish.Restart();
  Check(Finish.PreviousBest > 90 && Finish.Elapsed == 0, "retry retains record and resets clock");
  auto *SceneRenderer = new Renderer;
  std::array<uint16_t, FramebufferWidth * FramebufferHeight + 2> Pixels{};
  Pixels.front() = 0xa5a5;
  Pixels.back() = 0x5a5a;
  int MaximumFaces = 0;
  for (int NodeIndex = 0; NodeIndex < NodeCount - 1; NodeIndex += 3)
    for (float Angle : {0.f, 1.4f, 3.14159f}) {
      Game View;
      View.Segment = NodeIndex;
      View.CarPosition = View.Road[NodeIndex].Position;
      View.Yaw = View.Road[NodeIndex].Heading + Angle;
      View.CameraYaw = View.Yaw;
      View.CameraHeight = View.GroundY = View.CarPosition.CoordinateY;
      View.CurrentMode = GameMode::Racing;
      SceneRenderer->Render(View, Pixels.data() + 1);
      MaximumFaces = std::max(MaximumFaces, SceneRenderer->FaceCount);
      Check(SceneRenderer->Dropped == 0,
            "renderer stays inside triangle budget around full course");
      Check(Pixels.front() == 0xa5a5 && Pixels.back() == 0x5a5a,
            "renderer respects framebuffer boundaries");
    }
  delete SceneRenderer;
  std::printf("PASS: lifecycle, driving, timestep, recovery, persistence, finish; 300 camera "
              "renders, max %d triangles\n",
              MaximumFaces);
}
