#include "game.hpp"
#include "save_journal.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace GravelByte;
static void Check(bool Passed, const char *Description) {
  if (!Passed) {
    std::fprintf(stderr, "FAIL: %s\n", Description);
    std::exit(1);
  }
}
alignas(4) static std::array<std::array<uint8_t, 4096>, 2> Flash;
static int EraseBytes = 4096, ProgramBytes = SaveProgramBytes, WrittenSlot = -1;
static const SaveSlot &Slot(int Index) {
  return *reinterpret_cast<const SaveSlot *>(Flash[Index].data());
}
static void FakeFlashWrite(int Target, const SaveSlot &Value) {
  WrittenSlot = Target;
  std::fill_n(Flash[Target].begin(), EraseBytes, 0xff);
  if (EraseBytes < 4096)
    return;
  std::array<uint8_t, SaveProgramBytes> Page;
  Page.fill(0xff);
  std::memcpy(Page.data(), &Value, sizeof(Value));
  for (int Index = 0; Index < ProgramBytes; ++Index)
    Flash[Target][Index] &= Page[Index];
}
int main() {
  Game GameState;
  const auto Old = MakeSlot(EncodeSave(GameState), 42);
  GameState.Muted = true;
  const auto Fresh = MakeSlot(EncodeSave(GameState), 43);
  for (std::size_t Count = 0; Count <= sizeof(SaveSlot); ++Count) {
    SaveSlot Cut;
    std::memset(&Cut, 0xff, sizeof(Cut));
    std::memcpy(&Cut, &Fresh, Count);
    int Chosen = NewestSlot(Old, Cut);
    Check(Chosen == 0 || (Chosen == 1 && std::memcmp(&Cut, &Fresh, sizeof(Cut)) == 0),
          "every interrupted page program retains old or complete new save");
    Check(LoadSave(GameState, Chosen == 0 ? Old.Data : Cut.Data), "chosen interrupted save loads");
  }
  for (std::size_t Count = 0; Count <= 4096; ++Count) {
    std::array<uint8_t, 4096> Sector;
    std::memcpy(Sector.data(), &Old, sizeof(Old));
    std::fill(Sector.begin(), Sector.begin() + Count, 0xff);
    SaveSlot Erased;
    std::memcpy(&Erased, Sector.data(), sizeof(Erased));
    Check(NewestSlot(Erased, Fresh) == 1,
          "partial erase of inactive sector preserves current save");
  }
  for (std::size_t Index = 0; Index < sizeof(Fresh); ++Index) {
    auto Corrupt = Fresh;
    reinterpret_cast<uint8_t *>(&Corrupt)[Index] ^= 1;
    Check(NewestSlot(Old, Corrupt) == 0, "corrupt newest page falls back to intact record");
  }
  Check(NewestSlot(MakeSlot(Old.Data, 0xffffffffu), MakeSlot(Fresh.Data, 0)) == 1,
        "journal sequence wraps");
  SaveSlot Empty;
  std::memset(&Empty, 0xff, sizeof(Empty));
  Check(NewestSlot(Empty, Empty) == -1, "blank flash has no journal");
  // Exercise the exact writer-selection/readback algorithm used on hardware.
  for (int Current : {0, 1})
    for (int Cut = 0; Cut <= 4096 + SaveProgramBytes; ++Cut) {
      for (auto &Sector : Flash)
        Sector.fill(0xff);
      std::memcpy(Flash[Current].data(), &Old, sizeof(Old));
      EraseBytes = std::min(Cut, 4096);
      ProgramBytes = std::max(0, Cut - 4096);
      const auto Result = StoreSave(Fresh.Data, Slot(0), Slot(1), FakeFlashWrite);
      Check(WrittenSlot != Current, "writer never erases the selected save sector");
      const int Selected = NewestSlot(Slot(0), Slot(1));
      Check(Selected >= 0, "power interruption always leaves a loadable save");
      Check(LoadSave(GameState, Slot(Selected).Data), "power-cut result restores game state");
      Check(Result != SaveResult::Saved || GameState.Muted,
            "success requires new setting readback");
    }
  for (int Cut = 0; Cut <= 4096 + SaveProgramBytes; ++Cut) {
    for (auto &Sector : Flash)
      Sector.fill(0xff);
    std::memcpy(Flash[1].data(), &Old.Data, sizeof(Old.Data));
    const auto Legacy = Flash[1];
    EraseBytes = std::min(Cut, 4096);
    ProgramBytes = std::max(0, Cut - 4096);
    StoreSave(Fresh.Data, Slot(0), Slot(1), FakeFlashWrite);
    Check(WrittenSlot == 0 && Flash[1] == Legacy,
          "initial save preserves the other sector across power loss");
  }
  EraseBytes = 4096;
  ProgramBytes = SaveProgramBytes;
  StoreSave(Fresh.Data, Slot(0), Slot(1), FakeFlashWrite);
  WrittenSlot = -1;
  Check(StoreSave(Fresh.Data, Slot(0), Slot(1), FakeFlashWrite) == SaveResult::Unchanged &&
            WrittenSlot == -1,
        "unchanged saves do not erase flash");
  GeometryTelemetry Telemetry;
  Telemetry.Observe(7);
  Telemetry.Observe(0);
  Telemetry.Observe(2);
  Telemetry.Observe(0);
  Check(Telemetry.Frames == 4 && Telemetry.Dropped == 9 && Telemetry.OverflowFrames == 2,
        "overflow on earlier frames remains in final telemetry");

  Renderer SceneRenderer;
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels{};
  // A non-planar right bank exposes a reversed diagonal: interpolating the
  // other diagonal yields 4m at this centroid instead of the drawn 2m.
  GameState.Segment = 150;
  for (int Row = 0; Row < NodeCount; ++Row)
    for (int Strip = 0; Strip < 10; ++Strip)
      GameState.Terrain[Row][Strip] = {float((Strip - 7) * 6), 0, float((Row - 150) * 6)};
  GameState.Terrain[151][8].CoordinateY = 6;
  float BankHeight = 0;
  Check(GameState.SurfaceHeight({4, 0, 4}, BankHeight) && std::abs(BankHeight - 2.f) < .0001f,
        "right bank uses the rendered outside-in triangle diagonal");
  int Scenarios = 0;
  float MaximumAirborneFrames = 0;
  for (int Track = 0; Track < TrackCount; ++Track)
    for (int CarIndex = 0; CarIndex < CarCount; ++CarIndex)
      for (int NodeIndex : {20, 45, 95, 120, 145, 156, 170, 200, 246, 280})
        for (int Sign : {-1, 1}) {
          GameState.SelectCarAndTrack(CarIndex, Track);
          GameState.CurrentMode = GameMode::Racing;
          bool Begun = false;
          int Perturbation = 0;
          for (int Frame = 0; Frame < 10000 && GameState.CurrentMode != GameMode::Finished;
               ++Frame) {
            auto PlayerInput = CalculateDrivingInput(GameState);
            if (GameState.Segment >= NodeIndex)
              Begun = true;
            if (Begun && Perturbation++ < 120) {
              PlayerInput = {};
              PlayerInput.Throttle = true;
              PlayerInput.Left = Sign < 0;
              PlayerInput.Right = Sign > 0;
            }
            GameState.Update(.02f, PlayerInput);
            Check(std::isfinite(GameState.CarPosition.CoordinateY) &&
                      std::isfinite(GameState.Speed),
                  "off-road state remains finite");
            Check(std::abs(GameState.VerticalSpeed) <= 12.001f,
                  "banks cannot inject extreme launch speed");
            MaximumAirborneFrames = std::max(MaximumAirborneFrames,
                                             GameState.CarPosition.CoordinateY - GameState.GroundY);
            if (!GameState.Airborne && std::abs(GameState.Lateral) > GameState.RoadWidth()) {
              float Surface = 0;
              Check(GameState.SurfaceHeight(GameState.CarPosition, Surface),
                    "off-road car has rendered ground support");
              Check(std::abs(GameState.CarPosition.CoordinateY - Surface) < .13f,
                    "grounded car follows visible terrain");
            }
            if (Begun && Frame % 20 == 0) {
              SceneRenderer.Render(GameState, Pixels.data());
              Check(SceneRenderer.Dropped == 0, "off-road views stay within geometry capacity");
            }
            if (Begun && Perturbation > 620)
              break;
          }
          Check(Begun, "off-road perturbation reaches requested course region");
          ++Scenarios;
        }
  Check(MaximumAirborneFrames < 3.f,
        "ordinary off-road excursions do not fly high above the course");
  GameState.SelectCarAndTrack(1, 2);
  GameState.CurrentMode = GameMode::Racing;
  const auto Before = GameState.CarPosition;
  GameState.Update(NAN, {});
  GameState.Update(-1, {});
  GameState.Update(0, {});
  Check(GameState.CarPosition.CoordinateX == Before.CoordinateX &&
            GameState.CarPosition.CoordinateY == Before.CoordinateY &&
            GameState.CarPosition.CoordinateZ == Before.CoordinateZ,
        "invalid frame deltas ignored");
  std::printf("PASS: journal power-loss/corruption, cumulative telemetry, %d public-input off-road "
              "excursions; max air %.3fm\n",
              Scenarios, MaximumAirborneFrames);
}
