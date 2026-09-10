#include "game.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <cmath>

namespace GravelByte {
float Clamp(float Value, float Minimum, float Maximum) {
  return std::max(Minimum, std::min(Maximum, Value));
}
float AngleDelta(float TargetAngle, float CurrentAngle) {
  float Difference = TargetAngle - CurrentAngle;
  while (Difference > Tuning::HalfTurnRadians)
    Difference -= Tuning::FullTurnRadians;
  while (Difference < -Tuning::HalfTurnRadians)
    Difference += Tuning::FullTurnRadians;
  return Difference;
}
const std::array<CarSpecification, CarCount> Cars{
    {{"FINCH 1300", "EASY", 7.0f, 28.f, 5.4f, 10.f, .30f, .92f, 1.07f, .84f, 2, 2, 1},
     {"KESTREL GT", "STANDARD", 10.4f, 34.f, 4.2f, 8.5f, .29f, 1.f, 1.f, 1.f, 3, 3, 3},
     {"GOSHAWK TURBO", "EXPERT", 13.5f, 40.f, 3.5f, 9.2f, .32f, 1.06f, .80f, 1.13f, 5, 5, 5}}};
const std::array<const char *, TrackCount> TrackNames{"BRACKEN RIDGE", "SUNMEADOW RUN",
                                                      "FROSTPINE PASS"};
// Calibrated from completed deterministic public-input drives; see docs/targets.md.
static constexpr std::array<std::array<float, SectorCount>, 9> Targets{
    {{26.04f, 53.04f, 78.20f, 107.88f, 132.00f},
     {22.02f, 45.32f, 67.22f, 92.21f, 113.00f},
     {21.12f, 43.53f, 64.60f, 88.59f, 108.52f},
     {23.78f, 46.68f, 69.12f, 93.10f, 114.67f},
     {19.89f, 39.80f, 58.97f, 79.81f, 98.16f},
     {19.09f, 38.21f, 56.64f, 76.72f, 94.31f},
     {26.57f, 54.86f, 81.01f, 109.25f, 132.17f},
     {22.64f, 47.81f, 70.48f, 95.70f, 115.59f},
     {21.75f, 46.07f, 67.89f, 92.25f, 111.39f}}};
Game::Game() {
  BuildTrack();
  Restart();
  CurrentMode = GameMode::Title;
}
void Game::BuildTrack() {
  Road = {};

  // Authored corner sequence: flowing opening, ridge, hairpin, forest descent.
  struct Bend {
    int From, To;
    float Curvature;
  };
  constexpr Bend Bends[] = {{10, 24, .020f},    {27, 40, -.034f},   {44, 56, .045f},
                            {59, 70, -.032f},   {73, 87, .040f},    {92, 104, -.052f},
                            {108, 120, .032f},  {126, 141, .045f},  {143, 151, -.018f},
                            {161, 173, -.044f}, {175, 187, .050f},  {191, 205, -.066f},
                            {211, 223, .056f},  {226, 237, -.038f}, {250, 261, .047f},
                            {264, 277, -.038f}, {281, 293, .034f}};
  constexpr Bend Summer[] = {{12, 35, -.022f},   {42, 65, .028f},    {72, 93, -.030f},
                             {102, 124, .034f},  {131, 146, -.029f}, {162, 183, .032f},
                             {190, 209, -.043f}, {215, 236, .033f},  {253, 271, -.028f},
                             {279, 294, .036f}};
  constexpr Bend Winter[] = {{10, 25, -.030f},   {30, 45, .042f},    {50, 67, -.040f},
                             {74, 89, .046f},    {94, 109, -.048f},  {115, 131, .039f},
                             {136, 150, -.041f}, {164, 180, .046f},  {185, 201, -.050f},
                             {209, 225, .046f},  {230, 240, -.032f}, {255, 271, .044f},
                             {278, 293, -.040f}};
  const Bend *Layout = SelectedTrack == 0 ? Bends : SelectedTrack == 1 ? Summer : Winter;
  const int BendCount = SelectedTrack == 0   ? int(std::size(Bends))
                        : SelectedTrack == 1 ? int(std::size(Summer))
                                             : int(std::size(Winter));
  float Heading = 0;
  for (int Index = 0; Index < NodeCount; ++Index) {
    float Curve = 0;
    for (int BendIndex = 0; BendIndex < BendCount; ++BendIndex) {
      auto CurrentBend = Layout[BendIndex];
      if (Index >= CurrentBend.From && Index < CurrentBend.To) {
        const float Fraction =
            float(Index - CurrentBend.From) / float(CurrentBend.To - CurrentBend.From);
        Curve = CurrentBend.Curvature * std::sin(Fraction * Tuning::HalfTurnRadians);
      }
    }
    Heading += Curve * TrackSegmentLength;
    Road[Index].Heading = Heading;
    Road[Index].Right = {std::cos(Heading), 0, -std::sin(Heading)};
    Road[Index].Turn = Curve;
    if (Index)
      Road[Index].Position =
          Road[Index - 1].Position + Vector3{std::sin(Heading) * TrackSegmentLength, 0,
                                             std::cos(Heading) * TrackSegmentLength};
    const float DistanceAlongTrack = Index * TrackSegmentLength;
    Road[Index].Position.CoordinateY =
        3.5f * std::sin(DistanceAlongTrack * .008f) + 1.6f * std::sin(DistanceAlongTrack * .021f) +
        26.0f * std::exp(-std::pow((DistanceAlongTrack - 850.f) / 230.f, 2.f)) +
        2.8f * std::exp(-std::pow((DistanceAlongTrack - 930.f) / 12.f, 2.f)) +
        2.8f * std::exp(-std::pow((DistanceAlongTrack - 1470.f) / 12.f, 2.f));
    if (SelectedTrack == 1)
      Road[Index].Position.CoordinateY =
          2.8f * std::sin(DistanceAlongTrack * .009f) +
          10.f * std::exp(-std::pow((DistanceAlongTrack - 650.f) / 190.f, 2.f)) +
          2.8f * std::exp(-std::pow((DistanceAlongTrack - 930.f) / 12.f, 2.f)) +
          2.8f * std::exp(-std::pow((DistanceAlongTrack - 1470.f) / 12.f, 2.f));
    if (SelectedTrack == 2)
      Road[Index].Position.CoordinateY =
          4.f * std::sin(DistanceAlongTrack * .006f) +
          35.f * std::exp(-std::pow((DistanceAlongTrack - 1050.f) / 300.f, 2.f)) +
          2.8f * std::exp(-std::pow((DistanceAlongTrack - 930.f) / 12.f, 2.f)) +
          2.8f * std::exp(-std::pow((DistanceAlongTrack - 1470.f) / 12.f, 2.f));
    Road[Index].HalfWidth = SelectedTrack == 1 ? 3.9f : 3.65f;
    for (auto Stretch :
         {std::array<int, 2>{85, 121}, std::array<int, 2>{170, 225}, std::array<int, 2>{255, 289}})
      if (Index >= Stretch[0] && Index <= Stretch[1])
        Road[Index].HalfWidth -= .70f * std::sin((Index - Stretch[0]) * Tuning::HalfTurnRadians /
                                                 (Stretch[1] - Stretch[0]));
    Road[Index].Bank = Clamp(-Curve * 2.2f, -.085f, .085f);
    Road[Index].VergeLeft = 2.5f + 2.0f * std::sin(DistanceAlongTrack * .019f) +
                            (GetSectionIndex(Index) == 2 ? 5.f : 0.f);
    Road[Index].VergeRight = 1.f + 2.5f * std::sin(DistanceAlongTrack * .013f + 1.f);
  }
  // Limit each hillside to its own corridor. Long offset strips otherwise
  // fold across hairpins and can put a distant mountain through the near road.
  // The bounds lie inside the nearest-road Voronoi bisectors with a margin.
  for (int Index = 0; Index < NodeCount; ++Index)
    for (int Sign : {-1, 1}) {
      float Limit = 100;
      for (int OtherIndex = 0; OtherIndex < NodeCount; ++OtherIndex) {
        if (std::abs(OtherIndex - Index) <= 3)
          continue;
        Vector3 FourthVertex = Road[OtherIndex].Position - Road[Index].Position;
        float Toward = Sign * (FourthVertex.CoordinateX * Road[Index].Right.CoordinateX +
                               FourthVertex.CoordinateZ * Road[Index].Right.CoordinateZ);
        if (Toward > .01f)
          Limit = std::min(Limit, .38f *
                                      (FourthVertex.CoordinateX * FourthVertex.CoordinateX +
                                       FourthVertex.CoordinateZ * FourthVertex.CoordinateZ) /
                                      Toward);
      }
      (Sign < 0 ? Road[Index].FarLeft : Road[Index].FarRight) =
          std::max(Road[Index].HalfWidth + 3.f, Limit);
    }
  for (int Index = 0; Index < NodeCount; ++Index) {
    float Width = Road[Index].HalfWidth, Verge = Width + .9f;
    float Left = Road[Index].FarLeft, Right = Road[Index].FarRight;
    const float Sides[] = {-Left,
                           -std::min(14.f, Verge + (Left - Verge) * .5f),
                           -Verge,
                           -Width,
                           -Width * .49f,
                           Width * .49f,
                           Width,
                           Verge,
                           std::min(14.f, Verge + (Right - Verge) * .5f),
                           Right};
    for (int Side = 0; Side < 10; ++Side)
      Terrain[Index][Side] = Roadside(Index, Sides[Side]);
  }
  // A coarse outer massif is static; keep its terrain sampling out of rendering.
  if (SelectedTrack == 2)
    for (int SampleIndex = 0; SampleIndex <= Tuning::MountainSections; ++SampleIndex) {
      float Fraction = float(SampleIndex) / Tuning::MountainSections;
      int NodeIndex = Tuning::TunnelStart + SampleIndex *
                                                (Tuning::TunnelEnd - Tuning::TunnelStart) /
                                                Tuning::MountainSections;
      auto &Ring = Mountain[SampleIndex];
      Ring[2] = Road[NodeIndex].Position +
                Vector3{0, Tuning::MountainPeak - 12.f * std::abs(Fraction * 2.f - 1.f), 0};
      Ring[0] = Roadside(NodeIndex, -std::min(Tuning::MountainWidth, Road[NodeIndex].FarLeft));
      Ring[4] = Roadside(NodeIndex, std::min(Tuning::MountainWidth, Road[NodeIndex].FarRight));
      Ring[1] = Ring[0] + (Ring[2] - Ring[0]) * .45f;
      Ring[3] = Ring[4] + (Ring[2] - Ring[4]) * .45f;
    }
}
void Game::Restart() {
  Segment = 1;
  Furthest = 1;
  CarPosition = Road[1].Position;
  Yaw = Road[1].Heading;
  CameraYaw = Yaw;
  GroundY = CarPosition.CoordinateY;
  CameraHeight = CarPosition.CoordinateY;
  VerticalSpeed = Pitch = Roll = 0;
  Airborne = false;
  SurfaceAvailable = true;
  StartHeld = true;
  Jumps = 0;
  Velocity = {};
  Speed = SteeringInput = Lateral = SegmentFraction = Elapsed = Impact = Stranded = Slip = 0;
  Countdown = Tuning::CountdownSeconds;
  ShowRecords = false;
  CinematicTime = ReplayTime = RecordClock = ReplayDuration = 0;
  ReplayCount = 0;
  ReplayInterval = Tuning::ReplayInterval;
  NextSample = ReplayInterval;
  PriorSplits = BestSplits;
  Recoveries = 0;
  RecoveryMessage = 0;
  NewRecord = false;
  Splits = {};
  SplitCount = 0;
  SplitMessage = SplitDelta = 0;
  SelectReference();
  PreviousBest = ReferenceSplits.back();
  CurrentMode = GameMode::Countdown;
  RecordPose();
}
bool Game::Bridge(int NodeIndex) const {
  return SelectedTrack == 0 && NodeIndex >= Tuning::BridgeStart && NodeIndex < Tuning::BridgeEnd;
}
bool Game::Tunnel(int NodeIndex) const {
  return SelectedTrack == 2 && NodeIndex >= Tuning::TunnelStart && NodeIndex < Tuning::TunnelEnd;
}
bool Game::Coast(int NodeIndex) const {
  return SelectedTrack == 1 && NodeIndex >= Tuning::CoastStart && NodeIndex <= Tuning::CoastEnd;
}
bool Game::HasScenery(int NodeIndex, int Sign) const {
  return !Bridge(NodeIndex) && !Bridge(NodeIndex - 1) && !Tunnel(NodeIndex) &&
         !(Coast(NodeIndex) && Sign > 0);
}
bool Game::Unlocked(int Track) const {
  for (int Previous = 0; Previous < Track; ++Previous) {
    bool Beaten = false;
    for (int CarIndex = 0; CarIndex < CarCount; ++CarIndex) {
      int Pair = Previous * CarCount + CarIndex;
      float TimeValue = Records[Pair].Splits.back();
      Beaten |= TimeValue > 0 && TimeValue < Targets[Pair].back();
    }
    if (!Beaten)
      return false;
  }
  return true;
}
void Game::ToggleAudio() {
  Muted = !Muted;
  SaveRequested = true;
}
void Game::RecordPose(bool Final) {
  if (ReplayCount == int(Replay.size())) {
    for (int Index = 0; Index < ReplayCount / 2; ++Index)
      Replay[Index] = Replay[Index * 2];
    ReplayCount /= 2;
    ReplayInterval *= 2;
    NextSample = ReplayCount * ReplayInterval;
    if (!Final && RecordClock + .0001f < NextSample)
      return;
  }
  Vector3 Offset = CarPosition - Road[Segment].Position;
  auto Quantize = [](float Value) {
    return int16_t(std::lround(Clamp(Value * Tuning::PoseScale, -32767, 32767)));
  };
  Replay[ReplayCount++] = {uint16_t(Segment),
                           Quantize(Offset.CoordinateX),
                           Quantize(Offset.CoordinateY),
                           Quantize(Offset.CoordinateZ),
                           int16_t(AngleDelta(Yaw, 0) * Tuning::AngleScale),
                           int8_t(Clamp(Pitch * 100, -127, 127)),
                           int8_t(Clamp(Roll * 100, -127, 127))};
  ReplayDuration = RecordClock;
  NextSample = ReplayCount * ReplayInterval;
}
void Game::ReplayTick(float DeltaTimeSeconds) {
  if (ReplayCount < 2 || ReplayDuration <= 0)
    return;
  ReplayTime = std::fmod(ReplayTime + DeltaTimeSeconds, ReplayDuration);
  int Index = std::min(ReplayCount - 2, int(ReplayTime / ReplayInterval));
  float Start = Index * ReplayInterval;
  float End = Index == ReplayCount - 2 ? ReplayDuration : Start + ReplayInterval;
  float Fraction = Clamp((ReplayTime - Start) / std::max(.001f, End - Start), 0, 1);
  const auto &EarlierPose = Replay[Index], &LaterPose = Replay[Index + 1];
  auto Position = [&](const ReplayPose &Position) {
    return Road[Position.NodeIndex].Position + Vector3{Position.CoordinateX / Tuning::PoseScale,
                                                       Position.CoordinateY / Tuning::PoseScale,
                                                       Position.CoordinateZ / Tuning::PoseScale};
  };
  Vector3 FirstPoint = Position(EarlierPose), SecondPoint = Position(LaterPose);
  CarPosition = FirstPoint + (SecondPoint - FirstPoint) * Fraction;
  Yaw = EarlierPose.Yaw / Tuning::AngleScale +
        AngleDelta(LaterPose.Yaw / Tuning::AngleScale, EarlierPose.Yaw / Tuning::AngleScale) *
            Fraction;
  Pitch = (EarlierPose.Pitch * (1 - Fraction) + LaterPose.Pitch * Fraction) * .01f;
  Roll = (EarlierPose.Roll * (1 - Fraction) + LaterPose.Roll * Fraction) * .01f;
  Segment = EarlierPose.NodeIndex;
  Locate();
  CameraYaw = Yaw;
  CameraHeight = CarPosition.CoordinateY;
  Speed = std::sqrt((SecondPoint.CoordinateX - FirstPoint.CoordinateX) *
                        (SecondPoint.CoordinateX - FirstPoint.CoordinateX) +
                    (SecondPoint.CoordinateZ - FirstPoint.CoordinateZ) *
                        (SecondPoint.CoordinateZ - FirstPoint.CoordinateZ)) /
          std::max(.001f, End - Start);
}
void Game::DemoTick(float DeltaTimeSeconds) {
  if (!DemoActive || DemoTime >= Tuning::ShowcaseSeconds) {
    if (!DemoActive) {
      TitleCar = SelectedCar;
      TitleTrack = SelectedTrack;
    }
    int Next = DemoActive ? (SelectedTrack + 1) % TrackCount : 0;
    DemoActive = true;
    SelectCarAndTrack(TitleCar, Next);
    Segment = Next == 0 ? Tuning::BridgeStart - 12 : Next == 1 ? 45 : Tuning::TunnelStart - 14;
    CarPosition = Road[Segment].Position;
    Yaw = CameraYaw = Road[Segment].Heading;
    GroundY = CameraHeight = CarPosition.CoordinateY;
    Speed = 18;
    Velocity = {std::sin(Yaw) * Speed, 0, std::cos(Yaw) * Speed};
    DemoTime = 0;
    CurrentMode = GameMode::Title;
  }
  DemoTime += DeltaTimeSeconds;
  float Remaining = std::min(DeltaTimeSeconds, Tuning::MaximumFrameDeltaSeconds);
  while (Remaining > .00001f) {
    float Step = std::min(Tuning::PhysicsStep, Remaining);
    SimulatePhysics(Step, CalculateDrivingInput(*this));
    Remaining -= Step;
  }
}
int Game::GetSectionIndex(int NodeIndex) {
  return NodeIndex < 73 ? 0 : NodeIndex < 151 ? 1 : NodeIndex < 221 ? 2 : 3;
}
const char *Game::GetSectionName(int NodeIndex) const {
  static const char *Names[] = {"BRACKEN WOOD", "HIGH MOOR", "SLATE RIDGE", "FERN VALLEY"};
  static const char *Summer[] = {"PALM SHORE", "GOLDEN DUNES", "SUNSTONE", "TIDELINE"};
  static const char *Winter[] = {"PINE GATE", "ICE HOLLOW", "SNOW RIDGE", "FROST VALLEY"};
  return (SelectedTrack == 0   ? Names
          : SelectedTrack == 1 ? Summer
                               : Winter)[GetSectionIndex(NodeIndex)];
}
const std::array<float, SectorCount> &Game::GetDefaultSplits() const {
  return Targets[SelectedTrack * CarCount + SelectedCar];
}
bool Game::Icy(int NodeIndex) const {
  return SelectedTrack == 2 &&
         ((NodeIndex >= 74 && NodeIndex < 110) || (NodeIndex >= 185 && NodeIndex < 226));
}
float Game::SurfaceGrip(int NodeIndex) const { return Icy(NodeIndex) ? .76f : 1.f; }
void Game::SelectCarAndTrack(int CarIndex, int TrackIndex) {
  SelectedCar = std::clamp(CarIndex, 0, CarCount - 1);
  TrackIndex = std::clamp(TrackIndex, 0, TrackCount - 1);
  if (SelectedTrack != TrackIndex) {
    SelectedTrack = TrackIndex;
    BuildTrack();
  }
  BestSplits = Records[SelectedTrack * CarCount + SelectedCar].Splits;
  Best = BestSplits.back();
  Restart();
}
void Game::SelectReference() {
  ReferenceSplits = (Best > 0 && Best < GetDefaultSplits().back() && BestSplits[0] > 0)
                        ? BestSplits
                        : GetDefaultSplits();
}
float Game::TerrainHeight(int Index, float Side) const {
  Index = std::clamp(Index, 0, NodeCount - 1);
  const auto &Count = Road[Index];
  float Distance = std::abs(Side), Verge = Count.HalfWidth + .9f, Edge = Side < 0 ? -Verge : Verge;
  float Far = Side < 0 ? Count.FarLeft : Count.FarRight,
        Bank = std::min(14.f, Verge + (Far - Verge) * .5f);
  if (Distance <= Verge)
    return Count.Position.CoordinateY + Side * Count.Bank;
  if (Bridge(Index) || Bridge(Index - 1))
    return Count.Position.CoordinateY - Tuning::RiverDrop * Clamp((Distance - Verge) / 2.f, 0, 1);
  if (Coast(Index) && Side > 0)
    return Count.Position.CoordinateY -
           Tuning::BeachDrop * Clamp((Distance - Verge) / std::max(1.f, Bank - Verge), 0, 1);
  float Rise = Side < 0 ? Count.VergeLeft : Count.VergeRight;
  if (SelectedTrack == 1)
    Rise = .6f + .4f * std::sin(Index * .19f);
  if (SelectedTrack == 2)
    Rise += 5.f;
  Rise *= std::min(1.f, (Bank - Verge) / 6.f);
  if (Distance <= Bank)
    return Count.Position.CoordinateY + Edge * Count.Bank +
           (Rise - Edge * Count.Bank) * (Distance - Verge) / (Bank - Verge);
  return Count.Position.CoordinateY + Rise +
         (Distance - Bank) * (SelectedTrack == 1 ? .025f : (.12f + (Side < 0 ? .08f : -.14f)));
}
Vector3 Game::Roadside(int Index, float Side) const {
  Index = std::clamp(Index, 0, NodeCount - 1);
  Vector3 Position = Road[Index].Position + Road[Index].Right * Side;
  Position.CoordinateY = TerrainHeight(Index, Side);
  return Position;
}
Vector3 Game::Scenery(int Index, int Sign) const {
  return Roadside(Index, Sign * (7.f + float((Index * 13 + Sign + 5) % 7)));
}
float Game::RoadWidth() const {
  return Road[Segment].HalfWidth * (1 - SegmentFraction) +
         Road[Segment + 1].HalfWidth * SegmentFraction;
}
// Query the same triangles used by render_road, instead of extrapolating a
// road-relative height across curved banks, rivers and finite terrain edges.
bool Game::SurfaceHeight(Vector3 Position, float &Height) const {
  bool Found = false;
  auto Sample = [&](Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex) {
    const float Difference = (SecondVertex.CoordinateZ - ThirdVertex.CoordinateZ) *
                                 (FirstVertex.CoordinateX - ThirdVertex.CoordinateX) +
                             (ThirdVertex.CoordinateX - SecondVertex.CoordinateX) *
                                 (FirstVertex.CoordinateZ - ThirdVertex.CoordinateZ);
    if (std::abs(Difference) < .0001f)
      return;
    const float HorizontalFraction = ((SecondVertex.CoordinateZ - ThirdVertex.CoordinateZ) *
                                          (Position.CoordinateX - ThirdVertex.CoordinateX) +
                                      (ThirdVertex.CoordinateX - SecondVertex.CoordinateX) *
                                          (Position.CoordinateZ - ThirdVertex.CoordinateZ)) /
                                     Difference;
    const float Value = ((ThirdVertex.CoordinateZ - FirstVertex.CoordinateZ) *
                             (Position.CoordinateX - ThirdVertex.CoordinateX) +
                         (FirstVertex.CoordinateX - ThirdVertex.CoordinateX) *
                             (Position.CoordinateZ - ThirdVertex.CoordinateZ)) /
                        Difference;
    if (HorizontalFraction < -.0001f || Value < -.0001f || HorizontalFraction + Value > 1.0001f)
      return;
    const float CoordinateY = HorizontalFraction * FirstVertex.CoordinateY +
                              Value * SecondVertex.CoordinateY +
                              (1 - HorizontalFraction - Value) * ThirdVertex.CoordinateY;
    if (!Found || CoordinateY > Height)
      Height = CoordinateY;
    Found = true;
  };
  for (int Index = std::max(0, Segment - 8); Index < std::min(NodeCount - 1, Segment + 9);
       ++Index) {
    const auto &CurrentRing = Terrain[Index], &NextRing = Terrain[Index + 1];
    for (int Strip = 0; Strip < 9; ++Strip) {
      if (Position.CoordinateX <
              std::min({CurrentRing[Strip].CoordinateX, NextRing[Strip].CoordinateX,
                        CurrentRing[Strip + 1].CoordinateX, NextRing[Strip + 1].CoordinateX}) ||
          Position.CoordinateX >
              std::max({CurrentRing[Strip].CoordinateX, NextRing[Strip].CoordinateX,
                        CurrentRing[Strip + 1].CoordinateX, NextRing[Strip + 1].CoordinateX}) ||
          Position.CoordinateZ <
              std::min({CurrentRing[Strip].CoordinateZ, NextRing[Strip].CoordinateZ,
                        CurrentRing[Strip + 1].CoordinateZ, NextRing[Strip + 1].CoordinateZ}) ||
          Position.CoordinateZ >
              std::max({CurrentRing[Strip].CoordinateZ, NextRing[Strip].CoordinateZ,
                        CurrentRing[Strip + 1].CoordinateZ, NextRing[Strip + 1].CoordinateZ}))
        continue;
      // The renderer walks right-hand banks from the outside inward, so
      // their quad diagonal is reversed relative to the left-hand banks.
      if (Strip >= 6) {
        Sample(CurrentRing[Strip + 1], NextRing[Strip + 1], NextRing[Strip]);
        Sample(CurrentRing[Strip + 1], NextRing[Strip], CurrentRing[Strip]);
      } else {
        Sample(CurrentRing[Strip], NextRing[Strip], NextRing[Strip + 1]);
        Sample(CurrentRing[Strip], NextRing[Strip + 1], CurrentRing[Strip + 1]);
      }
    }
  }
  return Found;
}
void Game::Locate() {
  float Closest = 1.e20f, BestFraction = 0;
  int Found = Segment;
  for (int Index = std::max(0, Segment - 8); Index <= std::min(NodeCount - 2, Segment + 8);
       ++Index) {
    Vector3 FirstVertex = Road[Index].Position,
            FourthVertex = Road[Index + 1].Position - FirstVertex,
            RedComponent = CarPosition - FirstVertex;
    float Fraction = Clamp((RedComponent.CoordinateX * FourthVertex.CoordinateX +
                            RedComponent.CoordinateZ * FourthVertex.CoordinateZ) /
                               (FourthVertex.CoordinateX * FourthVertex.CoordinateX +
                                FourthVertex.CoordinateZ * FourthVertex.CoordinateZ),
                           0, 1);
    float HorizontalStep = RedComponent.CoordinateX - Fraction * FourthVertex.CoordinateX,
          DepthStep = RedComponent.CoordinateZ - Fraction * FourthVertex.CoordinateZ,
          Dist = HorizontalStep * HorizontalStep + DepthStep * DepthStep;
    if (Dist < Closest) {
      Closest = Dist;
      Found = Index;
      BestFraction = Fraction;
    }
  }
  Segment = Found;
  SegmentFraction = BestFraction;
  Vector3 Center =
      Road[Found].Position + (Road[Found + 1].Position - Road[Found].Position) * BestFraction;
  const Vector3 FourthVertex = Road[Found + 1].Position - Road[Found].Position;
  Lateral = ((CarPosition.CoordinateX - Center.CoordinateX) * FourthVertex.CoordinateZ -
             (CarPosition.CoordinateZ - Center.CoordinateZ) * FourthVertex.CoordinateX) /
            TrackSegmentLength;
  GroundY = TerrainHeight(Found, Lateral) * (1 - BestFraction) +
            TerrainHeight(Found + 1, Lateral) * BestFraction;
  SurfaceAvailable = true;
  if (std::abs(Lateral) > RoadWidth())
    SurfaceAvailable = SurfaceHeight(CarPosition, GroundY);
  if (std::abs(Lateral) < RoadWidth() + 3)
    Furthest = std::max(Furthest, Segment);
}
void Game::Recover() {
  CarPosition = Road[Segment].Position;
  Yaw = Road[Segment].Heading;
  CameraYaw = Yaw;
  GroundY = CarPosition.CoordinateY;
  CameraHeight = CarPosition.CoordinateY;
  VerticalSpeed = Pitch = Roll = 0;
  Airborne = false;
  SurfaceAvailable = true;
  Velocity = {};
  Speed = 0;
  SteeringInput = 0;
  Lateral = 0;
  Stranded = 0;
  Elapsed += Tuning::Physics::RecoveryPenalty;
  ++Recoveries;
  RecoveryMessage = Tuning::Physics::MessageSeconds;
  Impact = .4f;
}
void Game::SimulatePhysics(float DeltaTimeSeconds, const DrivingInput &PlayerInput) {
  if (StartHeld) {
    if (PlayerInput.Throttle || PlayerInput.Brake || Velocity.CoordinateX != 0 ||
        Velocity.CoordinateZ != 0 || Segment != 1 ||
        CarPosition.CoordinateX != Road[1].Position.CoordinateX ||
        CarPosition.CoordinateZ != Road[1].Position.CoordinateZ)
      StartHeld = false;
    else
      return;
  }
  Impact = std::max(0.f, Impact - DeltaTimeSeconds);
  RecoveryMessage = std::max(0.f, RecoveryMessage - DeltaTimeSeconds);
  const float RotationSine = std::sin(Yaw), RotationCosine = std::cos(Yaw);
  float Forward = Velocity.CoordinateX * RotationSine + Velocity.CoordinateZ * RotationCosine;
  float Side = Velocity.CoordinateX * RotationCosine - Velocity.CoordinateZ * RotationSine;
  float Target = float(PlayerInput.Right) - float(PlayerInput.Left);
  SteeringInput +=
      (Target - SteeringInput) * std::min(1.f, DeltaTimeSeconds * Tuning::SteeringResponse);
  float Acceleration = PlayerInput.Throttle ? GetCarSpecification().Acceleration : 0.f;
  if (PlayerInput.Brake)
    Acceleration = (Forward > 1.f) ? -Tuning::Physics::BrakeDeceleration
                                   : -Tuning::Physics::ReverseAcceleration;
  if (PlayerInput.Handbrake && Forward > 0)
    Acceleration -= Tuning::Physics::HandbrakeDrag;
  const bool OutsideRoad = std::abs(Lateral) > RoadWidth();
  float Grip = PlayerInput.Handbrake
                   ? Tuning::Physics::HandbrakeGrip
                   : GetCarSpecification().Grip /
                         (1.f + std::max(0.f, Forward - Tuning::Physics::GripFalloffSpeed) *
                                    Tuning::Physics::GripFalloff);
  if (PlayerInput.Brake && !PlayerInput.Handbrake)
    Grip *= Tuning::Physics::BrakingGrip;
  if (OutsideRoad)
    Grip = Tuning::Physics::OffroadGrip;
  Grip *= SurfaceGrip(Segment);
  float LateralAccel = -Side * Grip;
  // A finite traction circle: accelerating/braking and cornering share grip.
  // The old exponential lateral damping allowed unlimited cornering force.
  const float Traction =
      (OutsideRoad ? Tuning::Physics::OffroadTraction : GetCarSpecification().Traction) *
      SurfaceGrip(Segment);
  const float Requested = Acceleration * Acceleration + LateralAccel * LateralAccel;
  if (Requested > Traction * Traction) {
    const float Scale = Traction / std::sqrt(Requested);
    Acceleration *= Scale;
    LateralAccel *= Scale;
  }
  if (Airborne) {
    Acceleration = 0;
    LateralAccel = 0;
  }
  Acceleration -= Forward * std::abs(Forward) *
                      (OutsideRoad ? Tuning::Physics::OffroadDrag : Tuning::Physics::RoadDrag) +
                  Forward * Tuning::Physics::RollingDrag;
  if (!Airborne)
    Acceleration -= (Road[Segment + 1].Position.CoordinateY - Road[Segment].Position.CoordinateY) /
                    TrackSegmentLength * Tuning::Physics::SlopeGravity;
  Forward = Clamp(Forward + Acceleration * DeltaTimeSeconds, Tuning::Physics::ReverseSpeed,
                  GetCarSpecification().MaximumSpeed);
  Side += LateralAccel * DeltaTimeSeconds;
  float Turn = SteeringInput * GetCarSpecification().Steering /
               (1.f + std::abs(Forward) * Tuning::Physics::SteeringFalloff) * Forward /
               Tuning::Physics::Wheelbase;
  if (PlayerInput.Handbrake)
    Turn *= Tuning::Physics::HandbrakeTurn;
  if (Airborne)
    Turn *= Tuning::Physics::AirborneTurn;
  Yaw += Turn * DeltaTimeSeconds;
  CameraYaw +=
      AngleDelta(Yaw, CameraYaw) * std::min(1.f, DeltaTimeSeconds * Tuning::CameraResponse);
  // Keep momentum in world space as the chassis turns: this produces real slip.
  Velocity = {RotationSine * Forward + RotationCosine * Side, 0,
              RotationCosine * Forward - RotationSine * Side};
  const Vector3 PreviousCar = CarPosition;
  CarPosition = CarPosition + Velocity * DeltaTimeSeconds;
  Speed = std::sqrt(Velocity.CoordinateX * Velocity.CoordinateX +
                    Velocity.CoordinateZ * Velocity.CoordinateZ);
  Slip = std::abs(Side);
  const float OldGround = GroundY;
  Locate();
  if (Bridge(Segment) || Tunnel(Segment)) {
    float Limit = RoadWidth() + Tuning::RailMargin - Tuning::CarClearance;
    if (std::abs(Lateral) > Limit) {
      float Correction = Lateral - Clamp(Lateral, -Limit, Limit);
      CarPosition = CarPosition - Road[Segment].Right * Correction;
      float Outward = Velocity.CoordinateX * Road[Segment].Right.CoordinateX +
                      Velocity.CoordinateZ * Road[Segment].Right.CoordinateZ;
      if (Outward * Lateral > 0) {
        Velocity = (Velocity - Road[Segment].Right * Outward) * .75f;
        Impact = .4f;
      }
      Locate();
      CarPosition.CoordinateY = std::max(CarPosition.CoordinateY, GroundY);
      VerticalSpeed = 0;
    }
  }
  if (!SurfaceAvailable) {
    Recover();
    return;
  }
  const bool OutsideRoadNow = std::abs(Lateral) > RoadWidth();
  const float SupportStep =
      Tuning::Physics::OffroadStep + Speed * DeltaTimeSeconds * Tuning::Physics::MaximumBankSlope;
  if ((OutsideRoad || OutsideRoadNow) && GroundY - CarPosition.CoordinateY > SupportStep) {
    // A steep bank is a collision, not an instantaneous lift onto its top.
    CarPosition = PreviousCar;
    Velocity = Velocity * .2f;
    Speed *= .2f;
    Impact = .4f;
    VerticalSpeed = 0;
    Locate();
  }
  const float GroundVelocity =
      Clamp((GroundY - OldGround) / DeltaTimeSeconds, -Tuning::Physics::MaximumSupportSpeed,
            Tuning::Physics::MaximumSupportSpeed);
  if (!Airborne && (OutsideRoad || OutsideRoadNow) &&
      CarPosition.CoordinateY - GroundY > SupportStep) {
    Airborne = true;
    VerticalSpeed = std::min(0.f, VerticalSpeed);
  }
  // Convex authored crests launch the car only when road support falls away.
  const bool Crest = (Segment >= 155 && Segment <= 157) || (Segment >= 245 && Segment <= 247);
  if (!Airborne && Crest && Speed > Tuning::Physics::JumpSpeed &&
      VerticalSpeed > Tuning::Physics::JumpRise &&
      VerticalSpeed - GroundVelocity > Tuning::Physics::JumpSupportDrop) {
    Airborne = true;
    ++Jumps;
  }
  if (Airborne) {
    VerticalSpeed -= Tuning::Gravity * DeltaTimeSeconds;
    CarPosition.CoordinateY += VerticalSpeed * DeltaTimeSeconds;
    if (CarPosition.CoordinateY <= GroundY) {
      CarPosition.CoordinateY = GroundY;
      Airborne = false;
      VerticalSpeed = GroundVelocity;
      Impact = std::max(Impact, Tuning::Physics::LandingImpact);
    }
  } else {
    CarPosition.CoordinateY = GroundY;
    VerticalSpeed = GroundVelocity;
  }
  const Vector3 Direction = Road[Segment + 1].Position - Road[Segment].Position;
  const float Facing = std::cos(AngleDelta(Yaw, Road[Segment].Heading));
  const float DesiredPitch = Airborne
                                 ? Clamp(VerticalSpeed / std::max(Speed, 1.f),
                                         -Tuning::Physics::PitchLimit, Tuning::Physics::PitchLimit)
                                 : Direction.CoordinateY / TrackSegmentLength * Facing;
  Pitch +=
      (DesiredPitch - Pitch) * std::min(1.f, DeltaTimeSeconds * Tuning::Physics::PitchResponse);
  Roll += (Road[Segment].Bank * Facing - SteeringInput * Speed * Tuning::Physics::RollLean - Roll) *
          std::min(1.f, DeltaTimeSeconds * Tuning::SteeringResponse);
  CameraHeight +=
      (GroundY + std::min(Tuning::Physics::CameraJumpRise, CarPosition.CoordinateY - GroundY) -
       CameraHeight) *
      std::min(1.f, DeltaTimeSeconds * Tuning::SteeringResponse);
  // The same deterministic roadside trees are used for rendering and collision.
  if (Impact <= 0 && std::abs(Lateral) > RoadWidth() + 1) {
    for (int Index = std::max(0, Segment - 2); Index < std::min(NodeCount, Segment + 3); ++Index) {
      if (Index % 3)
        continue;
      for (int Sign : {-1, 1}) {
        if (!HasScenery(Index, Sign))
          continue;
        Vector3 Position = Scenery(Index, Sign);
        float HorizontalStep = CarPosition.CoordinateX - Position.CoordinateX,
              DepthStep = CarPosition.CoordinateZ - Position.CoordinateZ;
        if (HorizontalStep * HorizontalStep + DepthStep * DepthStep <
            Tuning::Physics::TreeCollisionRadiusSquared) {
          float Length = std::sqrt(HorizontalStep * HorizontalStep + DepthStep * DepthStep);
          if (Length < .01f) {
            HorizontalStep = 1;
            DepthStep = 0;
            Length = 1;
          }
          CarPosition.CoordinateX =
              Position.CoordinateX + HorizontalStep / Length * Tuning::Physics::TreeClearance;
          CarPosition.CoordinateZ =
              Position.CoordinateZ + DepthStep / Length * Tuning::Physics::TreeClearance;
          Velocity = Velocity * Tuning::Physics::TreeBounce;
          Impact = Tuning::Physics::TreeImpact;
          Locate();
          if (!SurfaceAvailable) {
            Recover();
            return;
          }
          if (!Airborne)
            CarPosition.CoordinateY = GroundY;
          VerticalSpeed = 0;
        }
      }
    }
  }
  if (std::abs(Lateral) > Tuning::Physics::StrandedDistance ||
      (Speed < Tuning::Physics::StrandedSpeed && PlayerInput.Throttle &&
       std::abs(Lateral) > RoadWidth() + 1))
    Stranded += DeltaTimeSeconds;
  else
    Stranded = 0;
  if (Stranded > Tuning::Physics::RecoveryDelay ||
      std::abs(Lateral) > Tuning::Physics::RecoveryDistance)
    Recover();
}
void Game::Update(float DeltaTimeSeconds, const DrivingInput &PlayerInput) {
  if (!std::isfinite(DeltaTimeSeconds) || DeltaTimeSeconds <= 0)
    return;
  if (PlayerInput.Mute || (PlayerInput.Auxiliary &&
                           (CurrentMode == GameMode::Title || CurrentMode == GameMode::Paused)))
    ToggleAudio();
  CinematicTime += std::min(DeltaTimeSeconds, Tuning::MaximumFrameDeltaSeconds);
  MenuRotation += std::min(DeltaTimeSeconds, .1f) * .65f;
  const bool LeftEdge = PlayerInput.Left && !MenuLeft, RightEdge = PlayerInput.Right && !MenuRight;
  MenuLeft = PlayerInput.Left;
  MenuRight = PlayerInput.Right;
  if (CurrentMode == GameMode::Title) {
    if (PlayerInput.Action) {
      if (DemoActive)
        SelectCarAndTrack(TitleCar, TitleTrack);
      DemoActive = false;
      CurrentMode = GameMode::CarSelect;
    } else
      DemoTick(DeltaTimeSeconds);
    return;
  }
  if (CurrentMode == GameMode::CarSelect || CurrentMode == GameMode::TrackSelect) {
    const GameMode SelectionMode = CurrentMode;
    const int Move = int(RightEdge) - int(LeftEdge);
    if (Move) {
      SelectCarAndTrack(
          SelectionMode == GameMode::CarSelect ? (SelectedCar + Move + CarCount) % CarCount
                                               : SelectedCar,
          SelectionMode == GameMode::TrackSelect ? (SelectedTrack + Move + TrackCount) % TrackCount
                                                 : SelectedTrack);
      CurrentMode = SelectionMode;
    }
    if (PlayerInput.Back)
      CurrentMode = SelectionMode == GameMode::CarSelect ? GameMode::Title : GameMode::CarSelect;
    else if (PlayerInput.Action) {
      if (SelectionMode == GameMode::CarSelect)
        CurrentMode = GameMode::TrackSelect;
      else if (Unlocked(SelectedTrack)) {
        Restart();
        SaveRequested = true;
      }
    }
    return;
  }
  if (CurrentMode == GameMode::Finished) {
    if (PlayerInput.Back) {
      SelectCarAndTrack(SelectedCar, SelectedTrack);
      CurrentMode = GameMode::TrackSelect;
    } else if (PlayerInput.Action)
      Restart();
    else {
      if (PlayerInput.Auxiliary)
        ShowRecords = !ShowRecords;
      ReplayTick(DeltaTimeSeconds);
    }
    return;
  }
  if (PlayerInput.Pause) {
    if (CurrentMode == GameMode::Paused)
      CurrentMode = ResumeMode;
    else {
      ResumeMode = CurrentMode;
      CurrentMode = GameMode::Paused;
    }
    return;
  }
  if (CurrentMode == GameMode::Paused) {
    if (PlayerInput.Back)
      CurrentMode = GameMode::CarSelect;
    else if (PlayerInput.Action)
      Restart();
    return;
  }
  if (CurrentMode == GameMode::Countdown) {
    Countdown -= DeltaTimeSeconds;
    if (Countdown <= 0) {
      CurrentMode = GameMode::Racing;
      Countdown = 0;
    }
    return;
  }
  Elapsed += DeltaTimeSeconds;
  SplitMessage = std::max(0.f, SplitMessage - DeltaTimeSeconds);
  // Fixed upper substep makes steering/grip independent of display frame rate.
  float Remaining = std::min(DeltaTimeSeconds, Tuning::MaximumFrameDeltaSeconds);
  while (Remaining > .00001f) {
    float Step = std::min(Tuning::PhysicsStep, Remaining);
    SimulatePhysics(Step, PlayerInput);
    RecordClock += Step;
    if (RecordClock + .0001f >= NextSample)
      RecordPose();
    Remaining -= Step;
  }
  if (SplitCount < SectorCount && Segment + SegmentFraction >= SectorEnds[SplitCount] &&
      Furthest >= SectorEnds[SplitCount] - 1 && std::abs(Lateral) < RoadWidth() + 2) {
    Splits[SplitCount] = Elapsed;
    SplitDelta = Elapsed - ReferenceSplits[SplitCount];
    ++SplitCount;
    SplitMessage = Tuning::Physics::MessageSeconds;
  }
  if (SplitCount == SectorCount) {
    if (RecordClock > ReplayDuration + .0001f)
      RecordPose(true);
    CurrentMode = GameMode::Finished;
    CinematicTime = ReplayTime = 0;
    ShowRecords = false;
    NewRecord = (Best <= 0 || Elapsed < Best);
    if (NewRecord) {
      Best = Elapsed;
      BestSplits = Splits;
      Records[SelectedTrack * CarCount + SelectedCar].Splits = Splits;
      SaveRequested = true;
    }
  }
}

float Game::Progress() const {
  return Clamp((Segment + SegmentFraction - 1.f) / (NodeCount - 5.f), 0, 1);
}
static uint32_t Checksum(const SaveRecord &SavedRecord) {
  uint32_t Hash = Tuning::HashOffsetBasis;
  for (uint32_t Word : {SavedRecord.Magic, SavedRecord.Version, SavedRecord.Milliseconds,
                        SavedRecord.Course, SavedRecord.Splits[0], SavedRecord.Splits[1],
                        SavedRecord.Splits[2], SavedRecord.Splits[3]}) {
    for (int Index = 0; Index < 4; ++Index) {
      Hash = (Hash ^ uint8_t(Word)) * Tuning::HashPrime;
      Word >>= 8;
    }
  }
  return Hash;
}
SaveRecord EncodeBest(float Seconds, const std::array<float, SectorCount> &Splits) {
  SaveRecord SavedRecord{
      0x52414c59, 2,
      uint32_t(Clamp(Seconds * Tuning::Milliseconds, 0, float(Tuning::MaximumRecordMilliseconds)) +
               .5f),
      0};
  for (int Index = 0; Index < SectorCount - 1; ++Index)
    SavedRecord.Splits[Index] = uint32_t(
        Clamp(Splits[Index] * Tuning::Milliseconds, 0, float(Tuning::MaximumRecordMilliseconds)) +
        .5f);
  SavedRecord.Checksum = Checksum(SavedRecord);
  return SavedRecord;
}
float DecodeBest(const SaveRecord &SavedRecord) {
  if (SavedRecord.Magic != 0x52414c59 || SavedRecord.Version != 2 ||
      SavedRecord.Course != CourseVersion || SavedRecord.Milliseconds < 1000 ||
      SavedRecord.Milliseconds > Tuning::MaximumRecordMilliseconds ||
      SavedRecord.Checksum != Checksum(SavedRecord) || SavedRecord.Splits[0] == 0 ||
      SavedRecord.Splits.back() >= SavedRecord.Milliseconds)
    return 0;
  for (int Index = 1; Index < SectorCount - 1; ++Index)
    if (SavedRecord.Splits[Index] <= SavedRecord.Splits[Index - 1])
      return 0;
  return SavedRecord.Milliseconds * .001f;
}
void LoadBest(Game &GameState, const SaveRecord &SavedRecord) {
  GameState.Best = DecodeBest(SavedRecord);
  GameState.BestSplits = {};
  if (GameState.Best > 0) {
    for (int Index = 0; Index < SectorCount - 1; ++Index)
      GameState.BestSplits[Index] = SavedRecord.Splits[Index] * .001f;
    GameState.BestSplits.back() = GameState.Best;
  }
  GameState.Records[1].Splits = GameState.BestSplits;
  GameState.SelectReference();
}
static uint32_t SaveChecksum(const SaveData &Save) {
  uint32_t Hash = Tuning::HashOffsetBasis;
  auto Word = [&](uint32_t Value) {
    for (int Index = 0; Index < 4; ++Index) {
      Hash = (Hash ^ uint8_t(Value)) * Tuning::HashPrime;
      Value >>= 8;
    }
  };
  Word(Save.Magic);
  Word(Save.Version);
  Word(Save.CarSelectionAndFlags);
  Word(Save.Track);
  for (auto &Times : Save.Times)
    for (auto TimeValue : Times)
      Word(TimeValue);
  return Hash;
}
SaveData EncodeSave(const Game &GameState) {
  SaveData Save;
  Save.Magic = Tuning::SaveMagic;
  Save.Version = Tuning::SaveVersion;
  Save.CarSelectionAndFlags = (GameState.DemoActive ? GameState.TitleCar : GameState.SelectedCar) |
                              (GameState.Muted ? Tuning::MutedFlag : 0);
  Save.Track = GameState.DemoActive ? GameState.TitleTrack : GameState.SelectedTrack;
  for (int Index = 0; Index < CarCount * TrackCount; ++Index)
    for (int OtherIndex = 0; OtherIndex < SectorCount; ++OtherIndex)
      Save.Times[Index][OtherIndex] =
          uint32_t(Clamp(GameState.Records[Index].Splits[OtherIndex] * Tuning::Milliseconds, 0,
                         float(Tuning::MaximumRecordMilliseconds)) +
                   .5f);
  Save.Checksum = SaveChecksum(Save);
  return Save;
}
bool ValidSave(const SaveData &Save) {
  const unsigned CarIndex = Save.CarSelectionAndFlags & Tuning::SelectionMask;
  if (Save.Magic != Tuning::SaveMagic ||
      (Save.Version != 1 && Save.Version != Tuning::SaveVersion) ||
      (Save.CarSelectionAndFlags &
       ~(Tuning::SelectionMask | (Save.Version == 2 ? Tuning::MutedFlag : 0u))) ||
      CarIndex >= CarCount || Save.Track >= TrackCount || Save.Checksum != SaveChecksum(Save))
    return false;
  for (auto &Times : Save.Times) {
    if (Times.back() == 0) {
      for (auto Fraction : Times)
        if (Fraction)
          return false;
    } else {
      if (Times.front() == 0 || Times.back() > Tuning::MaximumRecordMilliseconds)
        return false;
      for (int Index = 1; Index < SectorCount; ++Index)
        if (Times[Index] <= Times[Index - 1])
          return false;
    }
  }
  return true;
}
bool LoadSave(Game &GameState, const SaveData &Save) {
  if (!ValidSave(Save))
    return false;
  const unsigned CarIndex = Save.CarSelectionAndFlags & Tuning::SelectionMask;
  for (int Index = 0; Index < CarCount * TrackCount; ++Index)
    for (int OtherIndex = 0; OtherIndex < SectorCount; ++OtherIndex)
      GameState.Records[Index].Splits[OtherIndex] = Save.Times[Index][OtherIndex] * .001f;
  GameState.Muted =
      Save.Version == Tuning::SaveVersion && (Save.CarSelectionAndFlags & Tuning::MutedFlag);
  GameState.SelectCarAndTrack(int(CarIndex), int(Save.Track));
  GameState.CurrentMode = GameMode::Title;
  return true;
}
} // namespace GravelByte
