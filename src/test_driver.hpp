#pragma once
#include "game.hpp"
#include <algorithm>
#include <cmath>

namespace GravelByte {
// Shared by host course tests and the optional hardware benchmark firmware.
// It only supplies public driving inputs; it cannot move the car or finish a run.
inline DrivingInput CalculateDrivingInput(const Game &GameState) {
  // Plan braking from distance to each bend, instead of slowing to the
  // tightest corner speed for the entire next 72 metres.
  float Desired = GameState.GetCarSpecification().MaximumSpeed - 4.f;
  for (int Index = GameState.Segment; Index < std::min(NodeCount, GameState.Segment + 16);
       ++Index) {
    float Curve = std::abs(GameState.Road[Index].Turn);
    if (Curve < .003f)
      continue;
    float CornerSpeed = std::sqrt(6.5f * (GameState.GetCarSpecification().Traction / 8.5f) *
                                  GameState.SurfaceGrip(Index) / Curve);
    float Distance = std::max(
        0.f,
        (Index - GameState.Segment - GameState.SegmentFraction) * GameState.SegmentLength - 12.f);
    Desired = std::min(Desired, std::sqrt(CornerSpeed * CornerSpeed + 2 * 6.f * Distance));
  }
  float Along = GameState.Segment + GameState.SegmentFraction +
                (12 + GameState.Speed * .35f) / GameState.SegmentLength;
  int NodeIndex = std::min(NodeCount - 2, int(Along));
  Vector3 Target = GameState.Road[NodeIndex].Position +
                   (GameState.Road[NodeIndex + 1].Position - GameState.Road[NodeIndex].Position) *
                       Clamp(Along - NodeIndex, 0, 1);
  Vector3 Delta = Target - GameState.CarPosition;
  float Error = AngleDelta(std::atan2(Delta.CoordinateX, Delta.CoordinateZ), GameState.Yaw);
  DrivingInput PlayerInput{};
  PlayerInput.Throttle = GameState.Speed < Desired;
  PlayerInput.Brake = GameState.Speed > Desired + 1;
  PlayerInput.Left = Error < -.025f;
  PlayerInput.Right = Error > .025f;
  return PlayerInput;
}
inline DrivingInput TestDriver(const Game &GameState) {
  if (GameState.CurrentMode == GameMode::Title || GameState.CurrentMode == GameMode::CarSelect ||
      GameState.CurrentMode == GameMode::TrackSelect) {
    DrivingInput Start{};
    Start.Action = true;
    return Start;
  }
  return CalculateDrivingInput(GameState);
}
} // namespace GravelByte
