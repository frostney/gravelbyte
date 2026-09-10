#pragma once
#include "game.hpp"
#include <algorithm>
#include <cmath>

namespace rally {
// Shared by host course tests and the optional hardware benchmark firmware.
// It only supplies public driving inputs; it cannot move the car or finish a run.
inline Input driving_input(const Game &game) {
  // Plan braking from distance to each bend, instead of slowing to the
  // tightest corner speed for the entire next 72 metres.
  float desired = game.spec().max_speed - 4.f;
  for (int i = game.segment; i < std::min(NodeCount, game.segment + 16); ++i) {
    float curve = std::abs(game.road[i].turn);
    if (curve < .003f)
      continue;
    float corner_speed =
        std::sqrt(6.5f * (game.spec().traction / 8.5f) * game.surface_grip(i) / curve);
    float distance = std::max(0.f, (i - game.segment - game.route_t) * Step - 12.f);
    desired = std::min(desired, std::sqrt(corner_speed * corner_speed + 2 * 6.f * distance));
  }
  float along = game.segment + game.route_t + (12 + game.speed * .35f) / Step;
  int node = std::min(NodeCount - 2, int(along));
  Vec target =
      game.road[node].p + (game.road[node + 1].p - game.road[node].p) * clamp(along - node, 0, 1);
  Vec delta = target - game.car;
  float error = angle_delta(std::atan2(delta.x, delta.z), game.yaw);
  Input input{};
  input.throttle = game.speed < desired;
  input.brake = game.speed > desired + 1;
  input.left = error < -.025f;
  input.right = error > .025f;
  return input;
}
inline Input test_driver(const Game &game) {
  if (game.mode == Mode::Title || game.mode == Mode::CarSelect || game.mode == Mode::TrackSelect) {
    Input start{};
    start.action = true;
    return start;
  }
  return driving_input(game);
}
} // namespace rally
