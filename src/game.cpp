#include "game.hpp"
#include "test_driver.hpp"
#include <algorithm>
#include <cmath>

namespace rally {
float clamp(float x, float a, float b) { return std::max(a, std::min(b, x)); }
float angle_delta(float a, float b) {
  float d = a - b;
  while (d > tuning::Pi)
    d -= tuning::Tau;
  while (d < -tuning::Pi)
    d += tuning::Tau;
  return d;
}
const std::array<CarSpec, CarCount> Cars{
    {{"MICA", "EASY", 7.0f, 28.f, 5.4f, 10.f, .30f, .92f, 1.07f, .84f, 2, 2, 1},
     {"TORR", "STANDARD", 10.4f, 34.f, 4.2f, 8.5f, .29f, 1.f, 1.f, 1.f, 3, 3, 3},
     {"KITE", "EXPERT", 13.5f, 40.f, 3.5f, 9.2f, .32f, 1.06f, .80f, 1.13f, 5, 5, 5}}};
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
  build_track();
  restart();
  mode = Mode::Title;
}
void Game::build_track() {
  road = {};

  // Authored corner sequence: flowing opening, ridge, hairpin, forest descent.
  struct Bend {
    int from, to;
    float curvature;
  };
  constexpr Bend bends[] = {{10, 24, .020f},    {27, 40, -.034f},   {44, 56, .045f},
                            {59, 70, -.032f},   {73, 87, .040f},    {92, 104, -.052f},
                            {108, 120, .032f},  {126, 141, .045f},  {143, 151, -.018f},
                            {161, 173, -.044f}, {175, 187, .050f},  {191, 205, -.066f},
                            {211, 223, .056f},  {226, 237, -.038f}, {250, 261, .047f},
                            {264, 277, -.038f}, {281, 293, .034f}};
  constexpr Bend summer[] = {{12, 35, -.022f},   {42, 65, .028f},    {72, 93, -.030f},
                             {102, 124, .034f},  {131, 146, -.029f}, {162, 183, .032f},
                             {190, 209, -.043f}, {215, 236, .033f},  {253, 271, -.028f},
                             {279, 294, .036f}};
  constexpr Bend winter[] = {{10, 25, -.030f},   {30, 45, .042f},    {50, 67, -.040f},
                             {74, 89, .046f},    {94, 109, -.048f},  {115, 131, .039f},
                             {136, 150, -.041f}, {164, 180, .046f},  {185, 201, -.050f},
                             {209, 225, .046f},  {230, 240, -.032f}, {255, 271, .044f},
                             {278, 293, -.040f}};
  const Bend *layout = selected_track == 0 ? bends : selected_track == 1 ? summer : winter;
  const int bend_count = selected_track == 0   ? int(std::size(bends))
                         : selected_track == 1 ? int(std::size(summer))
                                               : int(std::size(winter));
  float heading = 0;
  for (int i = 0; i < NodeCount; ++i) {
    float curve = 0;
    for (int bi = 0; bi < bend_count; ++bi) {
      auto b = layout[bi];
      if (i >= b.from && i < b.to) {
        const float t = float(i - b.from) / float(b.to - b.from);
        curve = b.curvature * std::sin(t * tuning::Pi);
      }
    }
    heading += curve * Step;
    road[i].heading = heading;
    road[i].right = {std::cos(heading), 0, -std::sin(heading)};
    road[i].turn = curve;
    if (i)
      road[i].p = road[i - 1].p + Vec{std::sin(heading) * Step, 0, std::cos(heading) * Step};
    const float s = i * Step;
    road[i].p.y = 3.5f * std::sin(s * .008f) + 1.6f * std::sin(s * .021f) +
                  26.0f * std::exp(-std::pow((s - 850.f) / 230.f, 2.f)) +
                  2.8f * std::exp(-std::pow((s - 930.f) / 12.f, 2.f)) +
                  2.8f * std::exp(-std::pow((s - 1470.f) / 12.f, 2.f));
    if (selected_track == 1)
      road[i].p.y = 2.8f * std::sin(s * .009f) +
                    10.f * std::exp(-std::pow((s - 650.f) / 190.f, 2.f)) +
                    2.8f * std::exp(-std::pow((s - 930.f) / 12.f, 2.f)) +
                    2.8f * std::exp(-std::pow((s - 1470.f) / 12.f, 2.f));
    if (selected_track == 2)
      road[i].p.y = 4.f * std::sin(s * .006f) +
                    35.f * std::exp(-std::pow((s - 1050.f) / 300.f, 2.f)) +
                    2.8f * std::exp(-std::pow((s - 930.f) / 12.f, 2.f)) +
                    2.8f * std::exp(-std::pow((s - 1470.f) / 12.f, 2.f));
    road[i].half_width = selected_track == 1 ? 3.9f : 3.65f;
    for (auto stretch :
         {std::array<int, 2>{85, 121}, std::array<int, 2>{170, 225}, std::array<int, 2>{255, 289}})
      if (i >= stretch[0] && i <= stretch[1])
        road[i].half_width -=
            .70f * std::sin((i - stretch[0]) * tuning::Pi / (stretch[1] - stretch[0]));
    road[i].bank = clamp(-curve * 2.2f, -.085f, .085f);
    road[i].verge_left = 2.5f + 2.0f * std::sin(s * .019f) + (section(i) == 2 ? 5.f : 0.f);
    road[i].verge_right = 1.f + 2.5f * std::sin(s * .013f + 1.f);
  }
  // Limit each hillside to its own corridor. Long offset strips otherwise
  // fold across hairpins and can put a distant mountain through the near road.
  // The bounds lie inside the nearest-road Voronoi bisectors with a margin.
  for (int i = 0; i < NodeCount; ++i)
    for (int sign : {-1, 1}) {
      float limit = 100;
      for (int j = 0; j < NodeCount; ++j) {
        if (std::abs(j - i) <= 3)
          continue;
        Vec d = road[j].p - road[i].p;
        float toward = sign * (d.x * road[i].right.x + d.z * road[i].right.z);
        if (toward > .01f)
          limit = std::min(limit, .38f * (d.x * d.x + d.z * d.z) / toward);
      }
      (sign < 0 ? road[i].far_left : road[i].far_right) = std::max(road[i].half_width + 3.f, limit);
    }
  for (int i = 0; i < NodeCount; ++i) {
    float width = road[i].half_width, verge = width + .9f;
    float left = road[i].far_left, right = road[i].far_right;
    const float sides[] = {-left,
                           -std::min(14.f, verge + (left - verge) * .5f),
                           -verge,
                           -width,
                           -width * .49f,
                           width * .49f,
                           width,
                           verge,
                           std::min(14.f, verge + (right - verge) * .5f),
                           right};
    for (int side = 0; side < 10; ++side)
      terrain[i][side] = roadside(i, sides[side]);
  }
  // A coarse outer massif is static; keep its terrain sampling out of rendering.
  if (selected_track == 2)
    for (int k = 0; k <= tuning::MountainSections; ++k) {
      float t = float(k) / tuning::MountainSections;
      int node = tuning::TunnelStart +
                 k * (tuning::TunnelEnd - tuning::TunnelStart) / tuning::MountainSections;
      auto &ring = mountain[k];
      ring[2] = road[node].p + Vec{0, tuning::MountainPeak - 12.f * std::abs(t * 2.f - 1.f), 0};
      ring[0] = roadside(node, -std::min(tuning::MountainWidth, road[node].far_left));
      ring[4] = roadside(node, std::min(tuning::MountainWidth, road[node].far_right));
      ring[1] = ring[0] + (ring[2] - ring[0]) * .45f;
      ring[3] = ring[4] + (ring[2] - ring[4]) * .45f;
    }
}
void Game::restart() {
  segment = 1;
  furthest = 1;
  car = road[1].p;
  yaw = road[1].heading;
  camera_yaw = yaw;
  ground_y = car.y;
  camera_height = car.y;
  vertical_speed = pitch = roll = 0;
  airborne = false;
  surface_available = true;
  jumps = 0;
  velocity = {};
  speed = steer = lateral = route_t = elapsed = impact = stranded = slip = 0;
  countdown = tuning::CountdownSeconds;
  show_records = false;
  cinematic_time = replay_time = record_clock = replay_duration = 0;
  replay_count = 0;
  replay_interval = tuning::ReplayInterval;
  next_sample = replay_interval;
  prior_splits = best_splits;
  recoveries = 0;
  recovery_message = 0;
  new_record = false;
  splits = {};
  split_count = 0;
  split_message = split_delta = 0;
  select_reference();
  previous_best = reference_splits.back();
  mode = Mode::Countdown;
  record_pose();
}
bool Game::bridge(int node) const {
  return selected_track == 0 && node >= tuning::BridgeStart && node < tuning::BridgeEnd;
}
bool Game::tunnel(int node) const {
  return selected_track == 2 && node >= tuning::TunnelStart && node < tuning::TunnelEnd;
}
bool Game::coast(int node) const {
  return selected_track == 1 && node >= tuning::CoastStart && node <= tuning::CoastEnd;
}
bool Game::has_scenery(int node, int sign) const {
  return !bridge(node) && !bridge(node - 1) && !tunnel(node) && !(coast(node) && sign > 0);
}
bool Game::unlocked(int track) const {
  for (int previous = 0; previous < track; ++previous) {
    bool beaten = false;
    for (int car_index = 0; car_index < CarCount; ++car_index) {
      int pair = previous * CarCount + car_index;
      float time = records[pair].splits.back();
      beaten |= time > 0 && time < Targets[pair].back();
    }
    if (!beaten)
      return false;
  }
  return true;
}
void Game::toggle_audio() {
  muted = !muted;
  save_requested = true;
}
void Game::record_pose(bool final) {
  if (replay_count == int(replay.size())) {
    for (int i = 0; i < replay_count / 2; ++i)
      replay[i] = replay[i * 2];
    replay_count /= 2;
    replay_interval *= 2;
    next_sample = replay_count * replay_interval;
    if (!final && record_clock + .0001f < next_sample)
      return;
  }
  Vec offset = car - road[segment].p;
  auto quantize = [](float value) {
    return int16_t(std::lround(clamp(value * tuning::PoseScale, -32767, 32767)));
  };
  replay[replay_count++] = {uint16_t(segment),
                            quantize(offset.x),
                            quantize(offset.y),
                            quantize(offset.z),
                            int16_t(angle_delta(yaw, 0) * tuning::AngleScale),
                            int8_t(clamp(pitch * 100, -127, 127)),
                            int8_t(clamp(roll * 100, -127, 127))};
  replay_duration = record_clock;
  next_sample = replay_count * replay_interval;
}
void Game::replay_tick(float dt) {
  if (replay_count < 2 || replay_duration <= 0)
    return;
  replay_time = std::fmod(replay_time + dt, replay_duration);
  int index = std::min(replay_count - 2, int(replay_time / replay_interval));
  float start = index * replay_interval;
  float end = index == replay_count - 2 ? replay_duration : start + replay_interval;
  float t = clamp((replay_time - start) / std::max(.001f, end - start), 0, 1);
  const auto &a = replay[index], &b = replay[index + 1];
  auto position = [&](const ReplayPose &p) {
    return road[p.node].p +
           Vec{p.x / tuning::PoseScale, p.y / tuning::PoseScale, p.z / tuning::PoseScale};
  };
  Vec pa = position(a), pb = position(b);
  car = pa + (pb - pa) * t;
  yaw = a.yaw / tuning::AngleScale +
        angle_delta(b.yaw / tuning::AngleScale, a.yaw / tuning::AngleScale) * t;
  pitch = (a.pitch * (1 - t) + b.pitch * t) * .01f;
  roll = (a.roll * (1 - t) + b.roll * t) * .01f;
  segment = a.node;
  locate();
  camera_yaw = yaw;
  camera_height = car.y;
  speed = std::sqrt((pb.x - pa.x) * (pb.x - pa.x) + (pb.z - pa.z) * (pb.z - pa.z)) /
          std::max(.001f, end - start);
}
void Game::demo_tick(float dt) {
  if (!demo_active || demo_time >= tuning::ShowcaseSeconds) {
    if (!demo_active) {
      title_car = selected_car;
      title_track = selected_track;
    }
    int next = demo_active ? (selected_track + 1) % TrackCount : 0;
    demo_active = true;
    select(title_car, next);
    segment = next == 0 ? tuning::BridgeStart - 12 : next == 1 ? 45 : tuning::TunnelStart - 14;
    car = road[segment].p;
    yaw = camera_yaw = road[segment].heading;
    ground_y = camera_height = car.y;
    speed = 18;
    velocity = {std::sin(yaw) * speed, 0, std::cos(yaw) * speed};
    demo_time = 0;
    mode = Mode::Title;
  }
  demo_time += dt;
  float remaining = std::min(dt, tuning::MaxFrameDelta);
  while (remaining > .00001f) {
    float step = std::min(tuning::PhysicsStep, remaining);
    physics(step, driving_input(*this));
    remaining -= step;
  }
}
int Game::section(int node) { return node < 73 ? 0 : node < 151 ? 1 : node < 221 ? 2 : 3; }
const char *Game::section_name(int node) const {
  static const char *names[] = {"BRACKEN WOOD", "HIGH MOOR", "SLATE RIDGE", "FERN VALLEY"};
  static const char *summer[] = {"PALM SHORE", "GOLDEN DUNES", "SUNSTONE", "TIDELINE"};
  static const char *winter[] = {"PINE GATE", "ICE HOLLOW", "SNOW RIDGE", "FROST VALLEY"};
  return (selected_track == 0 ? names : selected_track == 1 ? summer : winter)[section(node)];
}
const std::array<float, SectorCount> &Game::default_splits() const {
  return Targets[selected_track * CarCount + selected_car];
}
bool Game::icy(int node) const {
  return selected_track == 2 && ((node >= 74 && node < 110) || (node >= 185 && node < 226));
}
float Game::surface_grip(int node) const { return icy(node) ? .76f : 1.f; }
void Game::select(int car_index, int track_index) {
  selected_car = std::clamp(car_index, 0, CarCount - 1);
  track_index = std::clamp(track_index, 0, TrackCount - 1);
  if (selected_track != track_index) {
    selected_track = track_index;
    build_track();
  }
  best_splits = records[selected_track * CarCount + selected_car].splits;
  best = best_splits.back();
  restart();
}
void Game::select_reference() {
  reference_splits = (best > 0 && best < default_splits().back() && best_splits[0] > 0)
                         ? best_splits
                         : default_splits();
}
float Game::terrain_height(int i, float side) const {
  i = std::clamp(i, 0, NodeCount - 1);
  const auto &n = road[i];
  float distance = std::abs(side), verge = n.half_width + .9f, edge = side < 0 ? -verge : verge;
  float far = side < 0 ? n.far_left : n.far_right,
        bank = std::min(14.f, verge + (far - verge) * .5f);
  if (distance <= verge)
    return n.p.y + side * n.bank;
  if (bridge(i) || bridge(i - 1))
    return n.p.y - tuning::RiverDrop * clamp((distance - verge) / 2.f, 0, 1);
  if (coast(i) && side > 0)
    return n.p.y -
           tuning::BeachDrop * clamp((distance - verge) / std::max(1.f, bank - verge), 0, 1);
  float rise = side < 0 ? n.verge_left : n.verge_right;
  if (selected_track == 1)
    rise = .6f + .4f * std::sin(i * .19f);
  if (selected_track == 2)
    rise += 5.f;
  rise *= std::min(1.f, (bank - verge) / 6.f);
  if (distance <= bank)
    return n.p.y + edge * n.bank + (rise - edge * n.bank) * (distance - verge) / (bank - verge);
  return n.p.y + rise +
         (distance - bank) * (selected_track == 1 ? .025f : (.12f + (side < 0 ? .08f : -.14f)));
}
Vec Game::roadside(int i, float side) const {
  i = std::clamp(i, 0, NodeCount - 1);
  Vec p = road[i].p + road[i].right * side;
  p.y = terrain_height(i, side);
  return p;
}
Vec Game::scenery(int i, int sign) const {
  return roadside(i, sign * (7.f + float((i * 13 + sign + 5) % 7)));
}
float Game::road_width() const {
  return road[segment].half_width * (1 - route_t) + road[segment + 1].half_width * route_t;
}
// Query the same triangles used by render_road, instead of extrapolating a
// road-relative height across curved banks, rivers and finite terrain edges.
bool Game::surface_height(Vec p, float &height) const {
  bool found = false;
  auto sample = [&](Vec a, Vec b, Vec c) {
    const float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
    if (std::abs(d) < .0001f)
      return;
    const float u = ((b.z - c.z) * (p.x - c.x) + (c.x - b.x) * (p.z - c.z)) / d;
    const float v = ((c.z - a.z) * (p.x - c.x) + (a.x - c.x) * (p.z - c.z)) / d;
    if (u < -.0001f || v < -.0001f || u + v > 1.0001f)
      return;
    const float y = u * a.y + v * b.y + (1 - u - v) * c.y;
    if (!found || y > height)
      height = y;
    found = true;
  };
  for (int i = std::max(0, segment - 8); i < std::min(NodeCount - 1, segment + 9); ++i) {
    const auto &a = terrain[i], &b = terrain[i + 1];
    for (int strip = 0; strip < 9; ++strip) {
      if (p.x < std::min({a[strip].x, b[strip].x, a[strip + 1].x, b[strip + 1].x}) ||
          p.x > std::max({a[strip].x, b[strip].x, a[strip + 1].x, b[strip + 1].x}) ||
          p.z < std::min({a[strip].z, b[strip].z, a[strip + 1].z, b[strip + 1].z}) ||
          p.z > std::max({a[strip].z, b[strip].z, a[strip + 1].z, b[strip + 1].z}))
        continue;
      // The renderer walks right-hand banks from the outside inward, so
      // their quad diagonal is reversed relative to the left-hand banks.
      if (strip >= 6) {
        sample(a[strip + 1], b[strip + 1], b[strip]);
        sample(a[strip + 1], b[strip], a[strip]);
      } else {
        sample(a[strip], b[strip], b[strip + 1]);
        sample(a[strip], b[strip + 1], a[strip + 1]);
      }
    }
  }
  return found;
}
void Game::locate() {
  float closest = 1.e20f, t_best = 0;
  int found = segment;
  for (int i = std::max(0, segment - 8); i <= std::min(NodeCount - 2, segment + 8); ++i) {
    Vec a = road[i].p, d = road[i + 1].p - a, r = car - a;
    float t = clamp((r.x * d.x + r.z * d.z) / (d.x * d.x + d.z * d.z), 0, 1);
    float dx = r.x - t * d.x, dz = r.z - t * d.z, dist = dx * dx + dz * dz;
    if (dist < closest) {
      closest = dist;
      found = i;
      t_best = t;
    }
  }
  segment = found;
  route_t = t_best;
  Vec center = road[found].p + (road[found + 1].p - road[found].p) * t_best;
  const Vec d = road[found + 1].p - road[found].p;
  lateral = ((car.x - center.x) * d.z - (car.z - center.z) * d.x) / Step;
  ground_y =
      terrain_height(found, lateral) * (1 - t_best) + terrain_height(found + 1, lateral) * t_best;
  surface_available = true;
  if (std::abs(lateral) > road_width())
    surface_available = surface_height(car, ground_y);
  if (std::abs(lateral) < road_width() + 3)
    furthest = std::max(furthest, segment);
}
void Game::recover() {
  car = road[segment].p;
  yaw = road[segment].heading;
  camera_yaw = yaw;
  ground_y = car.y;
  camera_height = car.y;
  vertical_speed = pitch = roll = 0;
  airborne = false;
  surface_available = true;
  velocity = {};
  speed = 0;
  steer = 0;
  lateral = 0;
  stranded = 0;
  elapsed += tuning::physics::RecoveryPenalty;
  ++recoveries;
  recovery_message = tuning::physics::MessageSeconds;
  impact = .4f;
}
void Game::physics(float dt, const Input &in) {
  impact = std::max(0.f, impact - dt);
  recovery_message = std::max(0.f, recovery_message - dt);
  const float sn = std::sin(yaw), cs = std::cos(yaw);
  float forward = velocity.x * sn + velocity.z * cs;
  float side = velocity.x * cs - velocity.z * sn;
  float target = float(in.right) - float(in.left);
  steer += (target - steer) * std::min(1.f, dt * tuning::SteeringResponse);
  float accel = in.throttle ? spec().acceleration : 0.f;
  if (in.brake)
    accel = (forward > 1.f) ? -tuning::physics::BrakeDeceleration
                            : -tuning::physics::ReverseAcceleration;
  if (in.handbrake && forward > 0)
    accel -= tuning::physics::HandbrakeDrag;
  const bool off = std::abs(lateral) > road_width();
  float grip =
      in.handbrake
          ? tuning::physics::HandbrakeGrip
          : spec().grip / (1.f + std::max(0.f, forward - tuning::physics::GripFalloffSpeed) *
                                     tuning::physics::GripFalloff);
  if (in.brake && !in.handbrake)
    grip *= tuning::physics::BrakingGrip;
  if (off)
    grip = tuning::physics::OffroadGrip;
  grip *= surface_grip(segment);
  float lateral_accel = -side * grip;
  // A finite traction circle: accelerating/braking and cornering share grip.
  // The old exponential lateral damping allowed unlimited cornering force.
  const float traction =
      (off ? tuning::physics::OffroadTraction : spec().traction) * surface_grip(segment);
  const float requested = accel * accel + lateral_accel * lateral_accel;
  if (requested > traction * traction) {
    const float scale = traction / std::sqrt(requested);
    accel *= scale;
    lateral_accel *= scale;
  }
  if (airborne) {
    accel = 0;
    lateral_accel = 0;
  }
  accel -= forward * std::abs(forward) *
               (off ? tuning::physics::OffroadDrag : tuning::physics::RoadDrag) +
           forward * tuning::physics::RollingDrag;
  if (!airborne)
    accel -= (road[segment + 1].p.y - road[segment].p.y) / Step * tuning::physics::SlopeGravity;
  forward = clamp(forward + accel * dt, tuning::physics::ReverseSpeed, spec().max_speed);
  side += lateral_accel * dt;
  float turn = steer * spec().steering /
               (1.f + std::abs(forward) * tuning::physics::SteeringFalloff) * forward /
               tuning::physics::Wheelbase;
  if (in.handbrake)
    turn *= tuning::physics::HandbrakeTurn;
  if (airborne)
    turn *= tuning::physics::AirborneTurn;
  yaw += turn * dt;
  camera_yaw += angle_delta(yaw, camera_yaw) * std::min(1.f, dt * tuning::CameraResponse);
  // Keep momentum in world space as the chassis turns: this produces real slip.
  velocity = {sn * forward + cs * side, 0, cs * forward - sn * side};
  const Vec previous_car = car;
  car = car + velocity * dt;
  speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
  slip = std::abs(side);
  const float old_ground = ground_y;
  locate();
  if (bridge(segment) || tunnel(segment)) {
    float limit = road_width() + tuning::RailMargin - tuning::CarClearance;
    if (std::abs(lateral) > limit) {
      float correction = lateral - clamp(lateral, -limit, limit);
      car = car - road[segment].right * correction;
      float outward = velocity.x * road[segment].right.x + velocity.z * road[segment].right.z;
      if (outward * lateral > 0) {
        velocity = (velocity - road[segment].right * outward) * .75f;
        impact = .4f;
      }
      locate();
      car.y = std::max(car.y, ground_y);
      vertical_speed = 0;
    }
  }
  if (!surface_available) {
    recover();
    return;
  }
  const bool off_now = std::abs(lateral) > road_width();
  const float support_step =
      tuning::physics::OffroadStep + speed * dt * tuning::physics::MaxBankSlope;
  if ((off || off_now) && ground_y - car.y > support_step) {
    // A steep bank is a collision, not an instantaneous lift onto its top.
    car = previous_car;
    velocity = velocity * .2f;
    speed *= .2f;
    impact = .4f;
    vertical_speed = 0;
    locate();
  }
  const float ground_velocity =
      clamp((ground_y - old_ground) / dt, -tuning::physics::MaxSupportSpeed,
            tuning::physics::MaxSupportSpeed);
  if (!airborne && (off || off_now) && car.y - ground_y > support_step) {
    airborne = true;
    vertical_speed = std::min(0.f, vertical_speed);
  }
  // Convex authored crests launch the car only when road support falls away.
  const bool crest = (segment >= 155 && segment <= 157) || (segment >= 245 && segment <= 247);
  if (!airborne && crest && speed > tuning::physics::JumpSpeed &&
      vertical_speed > tuning::physics::JumpRise &&
      vertical_speed - ground_velocity > tuning::physics::JumpSupportDrop) {
    airborne = true;
    ++jumps;
  }
  if (airborne) {
    vertical_speed -= tuning::Gravity * dt;
    car.y += vertical_speed * dt;
    if (car.y <= ground_y) {
      car.y = ground_y;
      airborne = false;
      vertical_speed = ground_velocity;
      impact = std::max(impact, tuning::physics::LandingImpact);
    }
  } else {
    car.y = ground_y;
    vertical_speed = ground_velocity;
  }
  const Vec direction = road[segment + 1].p - road[segment].p;
  const float facing = std::cos(angle_delta(yaw, road[segment].heading));
  const float desired_pitch = airborne
                                  ? clamp(vertical_speed / std::max(speed, 1.f),
                                          -tuning::physics::PitchLimit, tuning::physics::PitchLimit)
                                  : direction.y / Step * facing;
  pitch += (desired_pitch - pitch) * std::min(1.f, dt * tuning::physics::PitchResponse);
  roll += (road[segment].bank * facing - steer * speed * tuning::physics::RollLean - roll) *
          std::min(1.f, dt * tuning::SteeringResponse);
  camera_height +=
      (ground_y + std::min(tuning::physics::CameraJumpRise, car.y - ground_y) - camera_height) *
      std::min(1.f, dt * tuning::SteeringResponse);
  // The same deterministic roadside trees are used for rendering and collision.
  if (impact <= 0 && std::abs(lateral) > road_width() + 1) {
    for (int i = std::max(0, segment - 2); i < std::min(NodeCount, segment + 3); ++i) {
      if (i % 3)
        continue;
      for (int sign : {-1, 1}) {
        if (!has_scenery(i, sign))
          continue;
        Vec p = scenery(i, sign);
        float dx = car.x - p.x, dz = car.z - p.z;
        if (dx * dx + dz * dz < tuning::physics::TreeCollisionRadiusSquared) {
          float len = std::sqrt(dx * dx + dz * dz);
          if (len < .01f) {
            dx = 1;
            dz = 0;
            len = 1;
          }
          car.x = p.x + dx / len * tuning::physics::TreeClearance;
          car.z = p.z + dz / len * tuning::physics::TreeClearance;
          velocity = velocity * tuning::physics::TreeBounce;
          impact = tuning::physics::TreeImpact;
          locate();
          if (!surface_available) {
            recover();
            return;
          }
          if (!airborne)
            car.y = ground_y;
          vertical_speed = 0;
        }
      }
    }
  }
  if (std::abs(lateral) > tuning::physics::StrandedDistance ||
      (speed < tuning::physics::StrandedSpeed && in.throttle &&
       std::abs(lateral) > road_width() + 1))
    stranded += dt;
  else
    stranded = 0;
  if (stranded > tuning::physics::RecoveryDelay ||
      std::abs(lateral) > tuning::physics::RecoveryDistance)
    recover();
}
void Game::tick(float dt, const Input &in) {
  if (!std::isfinite(dt) || dt <= 0)
    return;
  if (in.mute || (in.auxiliary && (mode == Mode::Title || mode == Mode::Paused)))
    toggle_audio();
  cinematic_time += std::min(dt, tuning::MaxFrameDelta);
  menu_rotation += std::min(dt, .1f) * .65f;
  const bool left_edge = in.left && !menu_left, right_edge = in.right && !menu_right;
  menu_left = in.left;
  menu_right = in.right;
  if (mode == Mode::Title) {
    if (in.action) {
      if (demo_active)
        select(title_car, title_track);
      demo_active = false;
      mode = Mode::CarSelect;
    } else
      demo_tick(dt);
    return;
  }
  if (mode == Mode::CarSelect || mode == Mode::TrackSelect) {
    const Mode selection_mode = mode;
    const int move = int(right_edge) - int(left_edge);
    if (move) {
      select(selection_mode == Mode::CarSelect ? (selected_car + move + CarCount) % CarCount
                                               : selected_car,
             selection_mode == Mode::TrackSelect ? (selected_track + move + TrackCount) % TrackCount
                                                 : selected_track);
      mode = selection_mode;
    }
    if (in.back)
      mode = selection_mode == Mode::CarSelect ? Mode::Title : Mode::CarSelect;
    else if (in.action) {
      if (selection_mode == Mode::CarSelect)
        mode = Mode::TrackSelect;
      else if (unlocked(selected_track)) {
        restart();
        save_requested = true;
      }
    }
    return;
  }
  if (mode == Mode::Finished) {
    if (in.back) {
      select(selected_car, selected_track);
      mode = Mode::TrackSelect;
    } else if (in.action)
      restart();
    else {
      if (in.auxiliary)
        show_records = !show_records;
      replay_tick(dt);
    }
    return;
  }
  if (in.pause) {
    if (mode == Mode::Paused)
      mode = resume_mode;
    else {
      resume_mode = mode;
      mode = Mode::Paused;
    }
    return;
  }
  if (mode == Mode::Paused) {
    if (in.back)
      mode = Mode::CarSelect;
    else if (in.action)
      restart();
    return;
  }
  if (mode == Mode::Countdown) {
    countdown -= dt;
    if (countdown <= 0) {
      mode = Mode::Racing;
      countdown = 0;
    }
    return;
  }
  elapsed += dt;
  split_message = std::max(0.f, split_message - dt);
  // Fixed upper substep makes steering/grip independent of display frame rate.
  float remaining = std::min(dt, tuning::MaxFrameDelta);
  while (remaining > .00001f) {
    float step = std::min(tuning::PhysicsStep, remaining);
    physics(step, in);
    record_clock += step;
    if (record_clock + .0001f >= next_sample)
      record_pose();
    remaining -= step;
  }
  if (split_count < SectorCount && segment + route_t >= SectorEnds[split_count] &&
      furthest >= SectorEnds[split_count] - 1 && std::abs(lateral) < road_width() + 2) {
    splits[split_count] = elapsed;
    split_delta = elapsed - reference_splits[split_count];
    ++split_count;
    split_message = tuning::physics::MessageSeconds;
  }
  if (split_count == SectorCount) {
    if (record_clock > replay_duration + .0001f)
      record_pose(true);
    mode = Mode::Finished;
    cinematic_time = replay_time = 0;
    show_records = false;
    new_record = (best <= 0 || elapsed < best);
    if (new_record) {
      best = elapsed;
      best_splits = splits;
      records[selected_track * CarCount + selected_car].splits = splits;
      save_requested = true;
    }
  }
}

float Game::progress() const { return clamp((segment + route_t - 1.f) / (NodeCount - 5.f), 0, 1); }
static uint32_t checksum(const SaveRecord &r) {
  uint32_t hash = tuning::FnvOffset;
  for (uint32_t word : {r.magic, r.version, r.milliseconds, r.course, r.splits[0], r.splits[1],
                        r.splits[2], r.splits[3]}) {
    for (int i = 0; i < 4; ++i) {
      hash = (hash ^ uint8_t(word)) * tuning::FnvPrime;
      word >>= 8;
    }
  }
  return hash;
}
SaveRecord encode_best(float seconds, const std::array<float, SectorCount> &splits) {
  SaveRecord r{0x52414c59, 2,
               uint32_t(clamp(seconds * tuning::Milliseconds, 0, float(tuning::MaxRecordMs)) + .5f),
               0};
  for (int i = 0; i < SectorCount - 1; ++i)
    r.splits[i] =
        uint32_t(clamp(splits[i] * tuning::Milliseconds, 0, float(tuning::MaxRecordMs)) + .5f);
  r.checksum = checksum(r);
  return r;
}
float decode_best(const SaveRecord &r) {
  if (r.magic != 0x52414c59 || r.version != 2 || r.course != CourseVersion ||
      r.milliseconds < 1000 || r.milliseconds > tuning::MaxRecordMs || r.checksum != checksum(r) ||
      r.splits[0] == 0 || r.splits.back() >= r.milliseconds)
    return 0;
  for (int i = 1; i < SectorCount - 1; ++i)
    if (r.splits[i] <= r.splits[i - 1])
      return 0;
  return r.milliseconds * .001f;
}
void load_best(Game &game, const SaveRecord &record) {
  game.best = decode_best(record);
  game.best_splits = {};
  if (game.best > 0) {
    for (int i = 0; i < SectorCount - 1; ++i)
      game.best_splits[i] = record.splits[i] * .001f;
    game.best_splits.back() = game.best;
  }
  game.records[1].splits = game.best_splits;
  game.select_reference();
}
static uint32_t save_checksum(const SaveData &save) {
  uint32_t hash = tuning::FnvOffset;
  auto word = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i) {
      hash = (hash ^ uint8_t(v)) * tuning::FnvPrime;
      v >>= 8;
    }
  };
  word(save.magic);
  word(save.version);
  word(save.car);
  word(save.track);
  for (auto &times : save.times)
    for (auto time : times)
      word(time);
  return hash;
}
SaveData encode_save(const Game &game) {
  SaveData save;
  save.magic = tuning::SaveMagic;
  save.version = tuning::SaveVersion;
  save.car = (game.demo_active ? game.title_car : game.selected_car) |
             (game.muted ? tuning::MutedFlag : 0);
  save.track = game.demo_active ? game.title_track : game.selected_track;
  for (int i = 0; i < CarCount * TrackCount; ++i)
    for (int j = 0; j < SectorCount; ++j)
      save.times[i][j] = uint32_t(
          clamp(game.records[i].splits[j] * tuning::Milliseconds, 0, float(tuning::MaxRecordMs)) +
          .5f);
  save.checksum = save_checksum(save);
  return save;
}
bool valid_save(const SaveData &save) {
  const unsigned car = save.car & tuning::SelectionMask;
  if (save.magic != tuning::SaveMagic ||
      (save.version != 1 && save.version != tuning::SaveVersion) ||
      (save.car & ~(tuning::SelectionMask | (save.version == 2 ? tuning::MutedFlag : 0u))) ||
      car >= CarCount || save.track >= TrackCount || save.checksum != save_checksum(save))
    return false;
  for (auto &times : save.times) {
    if (times.back() == 0) {
      for (auto t : times)
        if (t)
          return false;
    } else {
      if (times.front() == 0 || times.back() > tuning::MaxRecordMs)
        return false;
      for (int i = 1; i < SectorCount; ++i)
        if (times[i] <= times[i - 1])
          return false;
    }
  }
  return true;
}
bool load_save(Game &game, const SaveData &save) {
  if (!valid_save(save))
    return false;
  const unsigned car = save.car & tuning::SelectionMask;
  for (int i = 0; i < CarCount * TrackCount; ++i)
    for (int j = 0; j < SectorCount; ++j)
      game.records[i].splits[j] = save.times[i][j] * .001f;
  game.muted = save.version == tuning::SaveVersion && (save.car & tuning::MutedFlag);
  game.select(int(car), int(save.track));
  game.mode = Mode::Title;
  return true;
}
} // namespace rally
