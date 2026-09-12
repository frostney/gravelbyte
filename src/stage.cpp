#include "stage.hpp"
#include "tuning.hpp"
#include <algorithm>
#include <cmath>

namespace GravelByte {
// Curvature at quarter, half and three-quarter distances. Each entry has zero
// curvature at its ends; unequal interior values author tightening, opening,
// and double-apex corners without imposing one sine envelope on every turn.
static constexpr AuthoredCorner ForestCorners[] = {
    {10, 25, .012f, .024f, .018f},   {29, 40, -.032f, -.030f, -.015f},
    {54, 71, .014f, .029f, .043f},   {76, 90, -.035f, -.014f, -.037f},
    {95, 103, .028f, .034f, .023f},  {104, 113, -.026f, -.039f, -.024f},
    {120, 139, .034f, .012f, .040f}, {145, 159, -.024f, -.031f, -.036f},
    {172, 188, .024f, .032f, .018f}, {194, 210, -.026f, -.047f, -.058f},
    {220, 235, .042f, .031f, .016f}, {242, 257, -.033f, -.030f, -.020f},
    {266, 279, .025f, .035f, .030f}, {283, 293, -.025f, -.035f, -.020f}};
static constexpr AuthoredCorner BeachCorners[] = {
    {14, 46, -.012f, -.019f, -.010f},   {55, 89, .014f, .021f, .013f},
    {98, 126, -.019f, -.023f, -.014f},  {134, 143, .030f, .052f, .033f},
    {144, 154, -.035f, -.053f, -.025f}, {170, 205, .015f, .027f, .031f},
    {218, 246, -.025f, -.014f, -.026f}, {261, 291, .018f, .025f, .012f}};
static constexpr AuthoredCorner SnowCorners[] = {
    {10, 25, -.016f, -.030f, -.036f},   {32, 47, .035f, .041f, .025f},
    {55, 68, -.019f, -.041f, -.050f},   {76, 90, .035f, .012f, .036f},
    {96, 106, -.025f, -.036f, -.021f},  {137, 152, .022f, .037f, .049f},
    {160, 173, -.045f, -.034f, -.018f}, {184, 196, .022f, .037f, .048f},
    {204, 219, -.029f, -.011f, -.032f}, {229, 244, .017f, .031f, .036f},
    {253, 266, -.032f, -.038f, -.021f}, {278, 294, .030f, .026f, .011f}};
static constexpr ElevationKey ForestElevation[] = {{0, 0},   {39, 3},   {55, 3},   {81, 7},
                                                   {113, 1}, {144, 18}, {160, 24}, {174, 17},
                                                   {197, 8}, {220, 4},  {258, 11}, {300, 0}};
static constexpr ElevationKey BeachElevation[] = {{0, 2},   {47, 3},  {91, 1},  {127, 2}, {155, 1},
                                                  {180, 4}, {207, 2}, {246, 1}, {270, 3}, {300, 2}};
static constexpr ElevationKey SnowElevation[] = {
    {0, 0},    {30, 8},   {53, 13},  {73, 20},  {93, 16},  {110, 24}, {135, 24},
    {159, 38}, {181, 33}, {201, 28}, {225, 16}, {245, 12}, {275, 4},  {300, 0}};
static constexpr StageLayout Stages[] = {{6.f,
                                          {53, 115, 169, 239, 297},
                                          ForestCorners,
                                          std::size(ForestCorners),
                                          ForestElevation,
                                          std::size(ForestElevation),
                                          42,
                                          52,
                                          -1,
                                          -1,
                                          164,
                                          121,
                                          139,
                                          3.65f},
                                         {4.5f,
                                          {48, 129, 157, 211, 297},
                                          BeachCorners,
                                          std::size(BeachCorners),
                                          BeachElevation,
                                          std::size(BeachElevation),
                                          -1,
                                          -1,
                                          -1,
                                          -1,
                                          -1,
                                          184,
                                          204,
                                          4.15f},
                                         {8.f,
                                          {70, 133, 179, 247, 297},
                                          SnowCorners,
                                          std::size(SnowCorners),
                                          SnowElevation,
                                          std::size(SnowElevation),
                                          -1,
                                          -1,
                                          115,
                                          131,
                                          177,
                                          184,
                                          219,
                                          3.55f}};
const StageLayout &GetStageLayout(int TrackIndex) { return Stages[std::clamp(TrackIndex, 0, 2)]; }
static float Smooth(float Fraction) { return Fraction * Fraction * (3.f - 2.f * Fraction); }
float StageCurvature(const StageLayout &Layout, float Node) {
  for (std::size_t Index = 0; Index < Layout.CornerCount; ++Index) {
    const auto &Corner = Layout.Corners[Index];
    if (Node < Corner.Start || Node >= Corner.End)
      continue;
    const float Along = (Node - Corner.Start) * 4.f / (Corner.End - Corner.Start);
    const int Part = std::min(3, int(Along));
    const float Values[] = {0, Corner.Entry, Corner.Apex, Corner.Exit, 0};
    return Values[Part] + (Values[Part + 1] - Values[Part]) * Smooth(Along - Part);
  }
  return 0;
}
float StageElevation(const StageLayout &Layout, float Node) {
  for (std::size_t Index = 1; Index < Layout.ElevationCount; ++Index) {
    if (Node > Layout.Elevations[Index].Node)
      continue;
    const auto &Earlier = Layout.Elevations[Index - 1], &Later = Layout.Elevations[Index];
    const float Fraction =
        std::clamp((Node - Earlier.Node) / (Later.Node - Earlier.Node), 0.f, 1.f);
    float Height = Earlier.Height + (Later.Height - Earlier.Height) * Smooth(Fraction);
    if (Layout.Crest >= 0) {
      const float Offset = (Node - Layout.Crest) / Tuning::Driving::CrestRadiusNodes;
      Height += Tuning::Driving::CrestHeight * std::exp(-Offset * Offset);
    }
    return Height;
  }
  return Layout.Elevations[Layout.ElevationCount - 1].Height;
}
float StageWidth(const StageLayout &Layout, int Node) {
  constexpr int TransitionNodes = Tuning::Driving::WidthTransitionNodes;
  float Width = Layout.BaseWidth;
  if (Layout.BridgeStart >= 0 && Node >= Layout.BridgeStart - TransitionNodes &&
      Node <= Layout.BridgeEnd + TransitionNodes) {
    const float Entrance = std::clamp(
        (Node - Layout.BridgeStart + TransitionNodes) / float(TransitionNodes), 0.f, 1.f);
    const float Exit =
        std::clamp((Layout.BridgeEnd + TransitionNodes - Node) / float(TransitionNodes), 0.f, 1.f);
    Width -= Tuning::Driving::BridgeNarrowing * std::min(Entrance, Exit);
  }
  if (Layout.TunnelStart >= 0 && Node >= Layout.TunnelStart && Node <= Layout.TunnelEnd)
    Width -= Tuning::Driving::TunnelNarrowing;
  return Width;
}
} // namespace GravelByte

namespace GravelByte {
uint32_t StageRandom(uint32_t &State) {
  State += 0x9e3779b9u;
  uint32_t Value = State;
  Value = (Value ^ (Value >> 16)) * 0x21f0aaadu;
  Value = (Value ^ (Value >> 15)) * 0x735a2d97u;
  return Value ^ (Value >> 15);
}
StageLayout RandomStageLayout(const RandomStage &Stage) {
  auto Layout = GetStageLayout(Stage.Biome);
  Layout.Corners = Stage.Corners.data();
  Layout.CornerCount = Stage.CornerCount;
  Layout.Elevations = Stage.Elevations.data();
  Layout.ElevationCount = Stage.ElevationCount;
  Layout.Checkpoints = Stage.Checkpoints;
  Layout.SegmentLength = Stage.SegmentLength;
  Layout.BaseWidth = 4.1f;
  return Layout;
}
void GenerateRandomStage(uint32_t Seed, RandomStage &Stage) {
  Stage = {};
  uint32_t State = Seed;
  Stage.Biome = StageRandom(State) % 3;
  Stage.SegmentLength = 5.f + float(StageRandom(State) % 21) * .1f;
  const auto &Base = GetStageLayout(Stage.Biome);
  Stage.ElevationCount = Base.ElevationCount;
  for (std::size_t Index = 0; Index < Base.ElevationCount; ++Index) {
    Stage.Elevations[Index] = Base.Elevations[Index];
    Stage.Elevations[Index].Height *= .75f;
  }
  for (int Index = 0; Index < StageCheckpointCount - 1; ++Index)
    Stage.Checkpoints[Index] = 50 + Index * 58 + int(StageRandom(State) % 21);
  Stage.Checkpoints.back() = 297;
  // Alternate bounded heading targets. A forward heading under one radian
  // keeps the route monotonic in Z and prevents crossing itself. Four or more
  // straight nodes separate corners; bridge/tunnel/crest landings remain clear.
  float Heading = 0;
  int Sign = StageRandom(State) & 1 ? 1 : -1;
  for (int Start = 12; Start < 276 && Stage.CornerCount < Stage.Corners.size();) {
    const int Length = 10 + StageRandom(State) % 11;
    const int End = std::min(293, Start + Length);
    const auto Overlaps = [&](int First, int Last) {
      return First >= 0 && Start < Last + 4 && End > First - 4;
    };
    if (!Overlaps(Base.BridgeStart, Base.BridgeEnd) &&
        !Overlaps(Base.TunnelStart, Base.TunnelEnd) && !Overlaps(Base.Crest - 2, Base.Crest + 2)) {
      const float Target = Sign * (.35f + float(StageRandom(State) % 46) * .01f);
      const float Turn = Target - Heading;
      const int Shape = StageRandom(State) % 3;
      float Entry = Shape == 0 ? .5f : 1.f, Apex = Shape == 1 ? .35f : 1.f,
            Exit = Shape == 2 ? .5f : 1.f;
      const float Scale =
          Turn * 4.f / ((Entry + Apex + Exit) * (End - Start) * Stage.SegmentLength);
      const float LimitedScale = std::clamp(Scale, -.048f, .048f);
      Stage.Corners[Stage.CornerCount++] = {Start, End, Entry * LimitedScale, Apex * LimitedScale,
                                            Exit * LimitedScale};
      Heading += LimitedScale * (Entry + Apex + Exit) * (End - Start) * Stage.SegmentLength / 4.f;
      Sign = -Sign;
    }
    Start = End + 4 + StageRandom(State) % 5;
  }
}
} // namespace GravelByte
