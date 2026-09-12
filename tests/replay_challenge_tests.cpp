#include "game.hpp"
#include "ghost.hpp"
#include "test_driver.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
using namespace GravelByte;
static void Check(bool Passed, const char *Message) {
  if (!Passed) {
    std::fprintf(stderr, "FAIL: %s\n", Message);
    std::exit(1);
  }
}
int main() {
  Game Race;
  Race.SelectedCar = 0;
  Race.Restart();
  Race.CurrentMode = GameMode::Racing;
  for (int Frame = 0; Race.SplitCount < 2 && Frame < 10000; ++Frame)
    Race.Update(.02f, TestDriver(Race));
  const auto Snapshot = Race.LastSplit;
  for (int Frame = 0; Frame < 120; ++Frame)
    Race.Update(.02f, TestDriver(Race));
  Race.RetrySplit();
  Check(Race.Practice && Race.CurrentMode == GameMode::Countdown &&
            Race.Elapsed == Snapshot.Elapsed &&
            Race.CarPosition.CoordinateX == Snapshot.CarPosition.CoordinateX &&
            Race.Velocity.CoordinateZ == Snapshot.Velocity.CoordinateZ &&
            Race.Yaw == Snapshot.Yaw && Race.SteeringInput == Snapshot.SteeringInput &&
            Race.SplitCount == 2,
        "split retry restores full measured physics snapshot");
  for (int Frame = 0; Frame < 15000 && Race.CurrentMode != GameMode::Finished; ++Frame)
    Race.Update(.02f, TestDriver(Race));
  Check(Race.CurrentMode == GameMode::Finished && !Race.NewRecord && !Race.GhostSaveRequested &&
            Race.Records[Race.RecordIndex()].Splits.back() == 0,
        "practice finish awards no personal best or ghost");
  Race.Restart();
  Race.CurrentMode = GameMode::Racing;
  Check(!Race.Practice && Race.ReplayOriginTime == 0, "full restart restores eligibility");
  Race.Recover();
  for (int Frame = 0; Frame < 15000 && Race.CurrentMode != GameMode::Finished; ++Frame)
    Race.Update(.02f, TestDriver(Race));
  Check(Race.NewRecord && Race.GhostSaveRequested &&
            std::abs(Race.ReplayDuration - Race.Elapsed) < .002f,
        "ghost clock includes recovery penalty and full official elapsed time");
  auto Transfer = std::make_unique<GhostTransfer>();
  FillGhostTransfer(Race, *Transfer);
  Check(AttachGhost(Race, *Transfer), "completed best replay validates and attaches");
  const auto Last =
      Race.SampleReplay(Transfer->Poses.data(), Transfer->Header.Count, Transfer->Header.Interval,
                        Transfer->Header.Duration, Transfer->Header.Duration);
  Check(std::abs(Last.Position.CoordinateX - Race.CarPosition.CoordinateX) < .02f,
        "decimated ghost preserves exact last pose");
  for (std::size_t Byte = 0; Byte < sizeof(GhostHeader); ++Byte) {
    auto *Bytes = reinterpret_cast<uint8_t *>(&Transfer->Header);
    Bytes[Byte] ^= 1;
    Check(!ValidGhost(Transfer->Header, Transfer->Poses.data(), Race),
          "every header corruption rejected");
    Bytes[Byte] ^= 1;
  }
  for (std::size_t Byte = 0; Byte < Transfer->Header.Count * sizeof(ReplayPose); ++Byte) {
    auto *Bytes = reinterpret_cast<uint8_t *>(Transfer->Poses.data());
    Bytes[Byte] ^= 1;
    Check(!ValidGhost(Transfer->Header, Transfer->Poses.data(), Race),
          "every payload corruption rejected");
    Bytes[Byte] ^= 1;
  }
  std::array<uint8_t, 256> Page{};
  for (int Offset = GhostHeaderBytes; Offset < int(sizeof(GhostTransfer)); Offset += Page.size()) {
    GhostPage(Race, Transfer->Header, Offset, Page.data(), Page.size());
    for (int Index = 0;
         Index < int(Page.size()) &&
         Offset + Index < GhostHeaderBytes + int(Transfer->Header.Count * sizeof(ReplayPose));
         ++Index)
      Check(Page[Index] == reinterpret_cast<uint8_t *>(Transfer.get())[Offset + Index],
            "streamed flash pages match portable ghost payload");
  }
  auto Staging = std::make_unique<std::array<uint8_t, GhostSlotBytes>>();
  auto Published = std::make_unique<std::array<uint8_t, GhostSlotBytes>>();
  for (int Offset = 0; Offset < GhostSlotBytes; Offset += 256)
    GhostPage(Race, Transfer->Header, Offset, Staging->data() + Offset, 256);
  Published->fill(0xff);
  const auto &InterruptedHeader = *reinterpret_cast<const GhostHeader *>(Published->data());
  const auto *InterruptedPoses =
      reinterpret_cast<const ReplayPose *>(Published->data() + GhostHeaderBytes);
  const int PayloadBytes =
      ((GhostHeaderBytes + Transfer->Header.Count * sizeof(ReplayPose) + 255) / 256) * 256 -
      GhostHeaderBytes;
  // Model loss after every programmed byte: payload first, committed header last.
  for (int Written = 0; Written < PayloadBytes + GhostHeaderBytes; ++Written) {
    const int Address =
        Written < PayloadBytes ? GhostHeaderBytes + Written : Written - PayloadBytes;
    (*Published)[Address] = (*Staging)[Address];
    if (ValidGhost(InterruptedHeader, InterruptedPoses, Race))
      Check(std::memcmp(Published->data(), Staging->data(), sizeof(GhostHeader)) == 0 &&
                std::memcmp(InterruptedPoses, Transfer->Poses.data(),
                            Transfer->Header.Count * sizeof(ReplayPose)) == 0,
            "interrupted publication never accepts a partial payload or header");
    Check(ValidGhost(Transfer->Header, Transfer->Poses.data(), Race),
          "previous slot remains valid throughout interrupted replacement");
  }
  Check(ValidGhost(InterruptedHeader, InterruptedPoses, Race),
        "fully published streamed replay validates");
  // A ghost is visible geometry with a stippled material, not a new shadow mask.
  auto Scene = std::make_unique<Renderer>();
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels{};
  const auto AtTime = Race.SampleReplay(Transfer->Poses.data(), Transfer->Header.Count,
                                        Transfer->Header.Interval, Transfer->Header.Duration, 30.f);
  Race.CurrentMode = GameMode::Racing;
  Race.Elapsed = 30;
  Race.Segment = AtTime.Segment;
  Race.Yaw = Race.CameraYaw = AtTime.Yaw;
  Race.CarPosition =
      AtTime.Position - Vector3{std::sin(AtTime.Yaw) * 10, 0, std::cos(AtTime.Yaw) * 10};
  Race.Locate();
  Race.CameraHeight = Race.CarPosition.CoordinateY = Race.GroundY;
  Scene->Render(Race, Pixels.data());
  int GhostFaces = 0;
  for (int Index = 0; Index < Scene->FaceCount; ++Index)
    if (Scene->Faces[Index].Shadow & 0xc000) {
      ++GhostFaces;
      Check((Scene->Faces[Index].Shadow & 0x3fff) == 0,
            "ghost triangles never refer to player shadow masks");
    }
  Check(GhostFaces > 0 && Scene->Dropped == 0, "ghost ahead draws without geometry overflow");
  Race.CarPosition = AtTime.Position;
  Scene->Render(Race, Pixels.data());
  for (int Index = 0; Index < Scene->FaceCount; ++Index)
    Check(!(Scene->Faces[Index].Shadow & 0xc000), "overlapping ghost fades away completely");
  Race.SteeringAssist = true;
  Check(!ValidGhost(Transfer->Header, Transfer->Poses.data(), Race),
        "ghost from other assist category rejected");
  Race.SteeringAssist = false;
  Race.Restart();
  Check(Race.ReplayCount == 1 && Race.GhostPoses == Transfer->Poses.data(),
        "live replay resets independently of loaded ghost");
  Race.CurrentMode = GameMode::Racing;
  Race.Elapsed = 10;
  VehiclePose Ghost;
  Check(Race.GhostPose(Ghost), "ghost samples at official race time");
  Race.GhostVisible = false;
  Check(!Race.GhostPose(Ghost), "ghost option disables rendering");
  // Variant geometry preserves the original stage while reversing feature order.
  Game Original, Variant;
  Original.SelectCarAndTrack(0, 0);
  Variant.SelectCarAndTrack(0, 0);
  Variant.SelectVariant(2);
  for (int Index = 1; Index <= 297; ++Index)
    Check(Variant.Road[Index].Position.CoordinateX == -Original.Road[Index].Position.CoordinateX &&
              Variant.Road[Index].Position.CoordinateZ == Original.Road[Index].Position.CoordinateZ,
          "mirroring reflects the actual authored geometry");
  Variant.SelectVariant(1);
  Check(Variant.SectorEnds[0] == 298 - Original.SectorEnds[3] &&
            Variant.Layout().BridgeStart == 298 - Original.Layout().BridgeEnd,
        "reverse stage reverses checkpoint and bridge ordering");
  for (int Route = 0; Route < VariantCount; ++Route)
    for (int Track = 0; Track < TrackCount; ++Track)
      for (int Car = 0; Car < CarCount; ++Car)
        for (bool Assisted : {false, true}) {
          Game RouteRun;
          RouteRun.SteeringAssist = Assisted;
          RouteRun.SelectCarAndTrack(Car, Track);
          RouteRun.SelectVariant(Route);
          RouteRun.CurrentMode = GameMode::Racing;
          for (int Frame = 0; Frame < 15000 && RouteRun.CurrentMode != GameMode::Finished; ++Frame)
            RouteRun.Update(.02f, TestDriver(RouteRun));
          Check(RouteRun.CurrentMode == GameMode::Finished && RouteRun.Recoveries == 0 &&
                    RouteRun.MedalForTime(RouteRun.Elapsed) >= 1,
                "every variant and assist category is driveable through public controls");
        }
  // Render the feature entries/exits as well as driving their physics.
  for (int Route = 0; Route < VariantCount; ++Route)
    for (int Track = 0; Track < TrackCount; ++Track) {
      Game View;
      View.SelectCarAndTrack(2, Track);
      View.SelectVariant(Route);
      View.CurrentMode = GameMode::Racing;
      for (int Node : {40, 51, 76, 113, 124, 133, 164, 178, 196, 219, 247, 283}) {
        View.Segment = Node;
        View.CarPosition = View.Road[Node].Position;
        View.Yaw = View.CameraYaw = View.Road[Node].Heading;
        View.Locate();
        View.CameraHeight = View.CarPosition.CoordinateY;
        Scene->Render(View, Pixels.data());
        Check(Scene->Dropped == 0, "variant landmarks render within the geometry budget");
      }
    }
  for (uint32_t Seed = 0; Seed < 256; ++Seed)
    for (int Car = 0; Car < CarCount; ++Car) {
      Game ChallengeRun;
      ChallengeRun.SelectedCar = Car;
      ChallengeRun.SetChallengeSeed(Seed);
      Game Copy;
      Copy.SelectedCar = (Car + 1) % CarCount;
      Copy.SteeringAssist = true;
      Copy.SetChallengeSeed(Seed);
      for (int Index = 1; Index < NodeCount; ++Index) {
        Check(ChallengeRun.Road[Index].Position.CoordinateX ==
                      Copy.Road[Index].Position.CoordinateX &&
                  ChallengeRun.Road[Index].Position.CoordinateZ ==
                      Copy.Road[Index].Position.CoordinateZ,
              "seed geometry is independent of car and assistance");
        Check(ChallengeRun.Road[Index].Position.CoordinateZ >
                  ChallengeRun.Road[Index - 1].Position.CoordinateZ,
              "random route stays forward and cannot cross itself");
      }
      ChallengeRun.CurrentMode = GameMode::Racing;
      if (Seed < 16 && Car == 0)
        for (int Node : {35, 79, 120, 177, 205, 277}) {
          Game View = ChallengeRun;
          View.Segment = Node;
          View.CarPosition = View.Road[Node].Position;
          View.Yaw = View.CameraYaw = View.Road[Node].Heading;
          View.Locate();
          View.CameraHeight = View.CarPosition.CoordinateY;
          Scene->Render(View, Pixels.data());
          Check(Scene->Dropped == 0, "seeded stage landmarks render without overflow");
        }
      for (int Frame = 0; Frame < 15000 && ChallengeRun.CurrentMode != GameMode::Finished; ++Frame)
        ChallengeRun.Update(.02f, TestDriver(ChallengeRun));
      Check(ChallengeRun.CurrentMode == GameMode::Finished && ChallengeRun.Recoveries == 0,
            "all cars finish the deterministic seed corpus without recovery");
      Check(ChallengeRun.MedalForTime(ChallengeRun.Elapsed) == 0 &&
                !ChallengeRun.GhostSaveRequested &&
                ChallengeRun.Records[ChallengeRun.RecordIndex()].Splits.back() == 0,
            "challenge results never award campaign records, ghosts or medals");
      const float SessionBest = ChallengeRun.Best;
      ChallengeRun.SelectCarAndTrack((Car + 1) % CarCount, ChallengeRun.SelectedTrack);
      Check(ChallengeRun.Best == 0, "challenge car categories isolated");
      ChallengeRun.SelectCarAndTrack(Car, ChallengeRun.SelectedTrack);
      Check(ChallengeRun.Best == SessionBest, "changing car retains session challenge best");
      ChallengeRun.SetChallengeSeed(Seed + 1);
      Check(ChallengeRun.Best == 0, "new seed starts a new session competition");
    }
}
