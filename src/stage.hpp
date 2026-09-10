#pragma once
#include <array>
#include <cstddef>

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
