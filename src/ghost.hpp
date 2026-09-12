#pragma once
#include "game.hpp"
#include <cmath>
#include <cstring>
namespace GravelByte {
constexpr uint32_t GhostMagic = 0x47524734, GhostVersion = 1;
constexpr int GhostCapacity = 1025, GhostHeaderBytes = 256, GhostSlotBytes = 16384;
struct GhostHeader {
  uint32_t Magic = GhostMagic, Version = GhostVersion, Course = CourseVersion;
  uint32_t Record = 0, Milliseconds = 0, Identity = 0, Count = 0;
  float Interval = 0, Duration = 0;
  uint32_t Checksum = 0;
};
static_assert(sizeof(GhostHeader) <= GhostHeaderBytes);
static_assert(GhostHeaderBytes + sizeof(ReplayPose) * GhostCapacity <= GhostSlotBytes);
inline uint32_t HashGhostBytes(uint32_t Hash, const void *Data, std::size_t Size) {
  const auto *Bytes = static_cast<const uint8_t *>(Data);
  for (std::size_t Index = 0; Index < Size; ++Index)
    Hash = (Hash ^ Bytes[Index]) * Tuning::HashPrime;
  return Hash;
}
inline uint32_t RecordIdentity(const std::array<float, SectorCount> &Splits) {
  uint32_t Hash = Tuning::HashOffsetBasis;
  for (float Time : Splits) {
    const uint32_t Milliseconds = uint32_t(Time * Tuning::Milliseconds + .5f);
    Hash = HashGhostBytes(Hash, &Milliseconds, sizeof(Milliseconds));
  }
  return Hash;
}
inline int GhostStride(const Game &GameState) {
  int Stride = 1;
  while ((GameState.ReplayCount - 2) / Stride + 2 > GhostCapacity)
    Stride *= 2;
  return Stride;
}
inline ReplayPose GhostPoseAt(const Game &GameState, int Index, int Count) {
  return GameState
      .Replay[Index == Count - 1 ? GameState.ReplayCount - 1 : Index * GhostStride(GameState)];
}
inline GhostHeader MakeGhostHeader(const Game &GameState) {
  GhostHeader Header;
  if (!GameState.NewRecord || GameState.Challenge || GameState.Practice ||
      GameState.ReplayCount < 2 || GameState.CurrentMode != GameMode::Finished)
    return Header;
  Header.Record = uint32_t(GameState.RecordIndex());
  Header.Milliseconds = uint32_t(GameState.Elapsed * Tuning::Milliseconds + .5f);
  Header.Identity = RecordIdentity(GameState.Splits);
  Header.Count = (GameState.ReplayCount - 2) / GhostStride(GameState) + 2;
  Header.Interval = GameState.ReplayInterval * GhostStride(GameState);
  Header.Duration = GameState.ReplayDuration;
  Header.Checksum =
      HashGhostBytes(Tuning::HashOffsetBasis, &Header, offsetof(GhostHeader, Checksum));
  for (uint32_t Index = 0; Index < Header.Count; ++Index) {
    const auto Pose = GhostPoseAt(GameState, int(Index), int(Header.Count));
    Header.Checksum = HashGhostBytes(Header.Checksum, &Pose, sizeof(Pose));
  }
  return Header;
}
inline bool ValidGhost(const GhostHeader &Header, const ReplayPose *Poses, int ExpectedRecord,
                       const std::array<float, SectorCount> &ExpectedSplits) {
  if (Header.Magic != GhostMagic || Header.Version != GhostVersion ||
      Header.Course != CourseVersion || Header.Record != uint32_t(ExpectedRecord) ||
      Header.Count < 2 || Header.Count > GhostCapacity ||
      Header.Milliseconds != uint32_t(ExpectedSplits.back() * Tuning::Milliseconds + .5f) ||
      Header.Identity != RecordIdentity(ExpectedSplits) || !Poses ||
      !std::isfinite(Header.Interval) || !std::isfinite(Header.Duration) ||
      Header.Interval < .01f || Header.Duration <= 0 ||
      std::abs(Header.Duration * Tuning::Milliseconds - Header.Milliseconds) > 1.f ||
      (Header.Count - 2) * Header.Interval >= Header.Duration + .001f ||
      (Header.Count - 1) * Header.Interval < Header.Duration - .001f)
    return false;
  auto Hash = HashGhostBytes(Tuning::HashOffsetBasis, &Header, offsetof(GhostHeader, Checksum));
  for (uint32_t Index = 0; Index < Header.Count; ++Index) {
    if (Poses[Index].NodeIndex >= NodeCount)
      return false;
    Hash = HashGhostBytes(Hash, &Poses[Index], sizeof(ReplayPose));
  }
  return Hash == Header.Checksum;
}
inline bool ValidGhost(const GhostHeader &Header, const ReplayPose *Poses, const Game &GameState) {
  return !GameState.Challenge &&
         ValidGhost(Header, Poses, GameState.RecordIndex(), GameState.BestSplits);
}
// Serialize one page without ever allocating a second replay on the handheld.
inline void GhostPage(const Game &GameState, const GhostHeader &Header, int Offset, uint8_t *Bytes,
                      int Count) {
  std::memset(Bytes, 0xff, Count);
  for (int Index = 0; Index < Count; ++Index) {
    const int Position = Offset + Index;
    if (Position < int(sizeof(Header)))
      Bytes[Index] = reinterpret_cast<const uint8_t *>(&Header)[Position];
    else if (Position >= GhostHeaderBytes &&
             Position < GhostHeaderBytes + int(Header.Count * sizeof(ReplayPose))) {
      const int PoseOffset = Position - GhostHeaderBytes;
      const auto Pose = GhostPoseAt(GameState, PoseOffset / sizeof(ReplayPose), Header.Count);
      Bytes[Index] = reinterpret_cast<const uint8_t *>(&Pose)[PoseOffset % sizeof(ReplayPose)];
    }
  }
}
struct GhostTransfer {
  GhostHeader Header;
  std::array<uint8_t, GhostHeaderBytes - sizeof(GhostHeader)> Padding{};
  std::array<ReplayPose, GhostCapacity> Poses{};
};
inline void FillGhostTransfer(const Game &GameState, GhostTransfer &Transfer) {
  Transfer.Header = MakeGhostHeader(GameState);
  for (uint32_t Index = 0; Index < Transfer.Header.Count; ++Index)
    Transfer.Poses[Index] = GhostPoseAt(GameState, Index, Transfer.Header.Count);
}
inline bool AttachGhost(Game &GameState, const GhostTransfer &Transfer) {
  if (!ValidGhost(Transfer.Header, Transfer.Poses.data(), GameState))
    return false;
  GameState.GhostPoses = Transfer.Poses.data();
  GameState.GhostCount = Transfer.Header.Count;
  GameState.GhostInterval = Transfer.Header.Interval;
  GameState.GhostDuration = Transfer.Header.Duration;
  return true;
}
} // namespace GravelByte
