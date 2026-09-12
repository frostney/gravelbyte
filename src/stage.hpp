#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace GravelByte {
constexpr int StageCheckpointCount = 5;
struct AuthoredCorner {
  int Start, End;
  float Entry, Apex, Exit;
};
struct ElevationKey {
  int Node;
  float Height;
};
struct StageLayout {
  float SegmentLength;
  std::array<int, StageCheckpointCount> Checkpoints;
  const AuthoredCorner *Corners;
  std::size_t CornerCount;
  const ElevationKey *Elevations;
  std::size_t ElevationCount;
  int BridgeStart, BridgeEnd, TunnelStart, TunnelEnd, Crest;
  int SurfaceStart, SurfaceEnd;
  float BaseWidth;
};
const StageLayout &GetStageLayout(int TrackIndex);
float StageCurvature(const StageLayout &Layout, float Node);
float StageElevation(const StageLayout &Layout, float Node);
float StageWidth(const StageLayout &Layout, int Node);
} // namespace GravelByte

namespace GravelByte {
struct RandomStage {
  std::array<AuthoredCorner, 18> Corners{};
  std::array<ElevationKey, 14> Elevations{};
  std::array<int, StageCheckpointCount> Checkpoints{};
  std::size_t CornerCount = 0, ElevationCount = 0;
  int Biome = 0;
  float SegmentLength = 6;
};
uint32_t StageRandom(uint32_t &State);
void GenerateRandomStage(uint32_t Seed, RandomStage &Stage);
StageLayout RandomStageLayout(const RandomStage &Stage);
} // namespace GravelByte
