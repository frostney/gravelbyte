#include "game.hpp"
#include <algorithm>
#include <cmath>

namespace rally {
float clamp(float x, float a, float b) { return std::max(a, std::min(b, x)); }
float angle_delta(float a, float b) {
  float d = a - b;
  while (d > 3.14159265f)
    d -= 6.2831853f;
  while (d < -3.14159265f)
    d += 6.2831853f;
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
    {{22.64f, 46.12f, 68.00f, 93.81f, 114.78f},
     {22.02f, 45.32f, 67.22f, 92.21f, 113.00f},
     {21.12f, 43.53f, 64.60f, 88.59f, 108.52f},
     {20.68f, 40.59f, 60.10f, 80.96f, 99.71f},
     {19.89f, 39.80f, 58.97f, 79.81f, 98.16f},
     {19.09f, 38.21f, 56.64f, 76.72f, 94.31f},
     {23.10f, 47.70f, 70.44f, 95.00f, 114.93f},
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
        curve = b.curvature * std::sin(t * 3.14159265f);
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
            .70f * std::sin((i - stretch[0]) * 3.14159265f / (stretch[1] - stretch[0]));
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
  jumps = 0;
  velocity = {};
  speed = steer = lateral = route_t = elapsed = impact = stranded = slip = 0;
  countdown = 3;
  recoveries = 0;
  recovery_message = 0;
  new_record = false;
  save_requested = false;
  splits = {};
  split_count = 0;
  split_message = split_delta = 0;
  select_reference();
  previous_best = reference_splits.back();
  mode = Mode::Countdown;
}
int Game::section(int node) { return node < 73 ? 0 : node < 151 ? 1 : node < 221 ? 2 : 3; }
const char *Game::section_name(int node) const {
  static const char *names[] = {"BRACKEN WOOD", "HIGH MOOR", "SLATE RIDGE", "FERN VALLEY"};
  static const char *summer[] = {"HAYFIELD", "GOLDEN CREST", "SUNSTONE", "ORCHARD"};
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
  float rise = side < 0 ? n.verge_left : n.verge_right;
  rise *= std::min(1.f, (bank - verge) / 6.f);
  if (distance <= bank)
    return n.p.y + edge * n.bank + (rise - edge * n.bank) * (distance - verge) / (bank - verge);
  return n.p.y + rise + (distance - bank) * (.12f + (side < 0 ? .08f : -.14f));
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
  velocity = {};
  speed = 0;
  steer = 0;
  lateral = 0;
  stranded = 0;
  elapsed += 3;
  ++recoveries;
  recovery_message = 2.5f;
  impact = .4f;
}
void Game::physics(float dt, const Input &in) {
  impact = std::max(0.f, impact - dt);
  recovery_message = std::max(0.f, recovery_message - dt);
  const float sn = std::sin(yaw), cs = std::cos(yaw);
  float forward = velocity.x * sn + velocity.z * cs;
  float side = velocity.x * cs - velocity.z * sn;
  float target = float(in.right) - float(in.left);
  steer += (target - steer) * std::min(1.f, dt * 7.f);
  float accel = in.throttle ? spec().acceleration : 0.f;
  if (in.brake)
    accel = (forward > 1.f) ? -22.f : -5.f;
  if (in.handbrake && forward > 0)
    accel -= 5.f;
  const bool off = std::abs(lateral) > road_width();
  float grip = in.handbrake ? 1.35f : spec().grip / (1.f + std::max(0.f, forward - 18.f) * .03f);
  if (in.brake && !in.handbrake)
    grip *= .9f;
  if (off)
    grip = 3.8f;
  grip *= surface_grip(segment);
  float lateral_accel = -side * grip;
  // A finite traction circle: accelerating/braking and cornering share grip.
  // The old exponential lateral damping allowed unlimited cornering force.
  const float traction = (off ? 5.5f : spec().traction) * surface_grip(segment);
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
  accel -= forward * std::abs(forward) * (off ? .095f : .007f) + forward * .08f;
  if (!airborne)
    accel -= (road[segment + 1].p.y - road[segment].p.y) / Step * 9.f;
  forward = clamp(forward + accel * dt, -4.f, spec().max_speed);
  side += lateral_accel * dt;
  float turn = steer * spec().steering / (1.f + std::abs(forward) * .035f) * forward / 2.6f;
  if (in.handbrake)
    turn *= 1.45f;
  if (airborne)
    turn *= .15f;
  yaw += turn * dt;
  camera_yaw += angle_delta(yaw, camera_yaw) * std::min(1.f, dt * 4.f);
  // Keep momentum in world space as the chassis turns: this produces real slip.
  velocity = {sn * forward + cs * side, 0, cs * forward - sn * side};
  car = car + velocity * dt;
  speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
  slip = std::abs(side);
  const float old_ground = ground_y;
  locate();
  const float ground_velocity = (ground_y - old_ground) / dt;
  // Convex authored crests launch the car only when road support falls away.
  const bool crest = (segment >= 155 && segment <= 157) || (segment >= 245 && segment <= 247);
  if (!airborne && crest && speed > 16 && vertical_speed > .6f &&
      vertical_speed - ground_velocity > .35f) {
    airborne = true;
    ++jumps;
  }
  if (airborne) {
    vertical_speed -= 16.f * dt;
    car.y += vertical_speed * dt;
    if (car.y <= ground_y) {
      car.y = ground_y;
      airborne = false;
      vertical_speed = ground_velocity;
      impact = std::max(impact, .14f);
    }
  } else {
    car.y = ground_y;
    vertical_speed = ground_velocity;
  }
  const Vec direction = road[segment + 1].p - road[segment].p;
  const float facing = std::cos(angle_delta(yaw, road[segment].heading));
  const float desired_pitch = airborne ? clamp(vertical_speed / std::max(speed, 1.f), -.25f, .25f)
                                       : direction.y / Step * facing;
  pitch += (desired_pitch - pitch) * std::min(1.f, dt * 9.f);
  roll += (road[segment].bank * facing - steer * speed * .0008f - roll) * std::min(1.f, dt * 7.f);
  camera_height +=
      (ground_y + std::min(.4f, car.y - ground_y) - camera_height) * std::min(1.f, dt * 7.f);
  // The same deterministic roadside trees are used for rendering and collision.
  if (impact <= 0 && std::abs(lateral) > road_width() + 1) {
    for (int i = std::max(0, segment - 2); i < std::min(NodeCount, segment + 3); ++i) {
      if (i % 3)
        continue;
      for (int sign : {-1, 1}) {
        Vec p = scenery(i, sign);
        float dx = car.x - p.x, dz = car.z - p.z;
        if (dx * dx + dz * dz < 2.0f) {
          float len = std::sqrt(dx * dx + dz * dz);
          if (len < .01f) {
            dx = 1;
            dz = 0;
            len = 1;
          }
          car.x = p.x + dx / len * 1.5f;
          car.z = p.z + dz / len * 1.5f;
          velocity = velocity * -.12f;
          impact = .7f;
        }
      }
    }
  }
  if (std::abs(lateral) > 22 ||
      (speed < 1.2f && in.throttle && std::abs(lateral) > road_width() + 1))
    stranded += dt;
  else
    stranded = 0;
  if (stranded > 2.5f || std::abs(lateral) > 45)
    recover();
}
void Game::tick(float dt, const Input &in) {
  if (!std::isfinite(dt) || dt <= 0)
    return;
  menu_rotation += std::min(dt, .1f) * .65f;
  const bool left_edge = in.left && !menu_left, right_edge = in.right && !menu_right;
  menu_left = in.left;
  menu_right = in.right;
  if (mode == Mode::Title) {
    if (in.action)
      mode = Mode::CarSelect;
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
      else {
        restart();
        save_requested = true;
      }
    }
    return;
  }
  if (mode == Mode::Finished) {
    if (in.back)
      mode = Mode::CarSelect;
    else if (in.action)
      restart();
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
  float remaining = std::min(dt, .25f);
  while (remaining > .00001f) {
    float step = std::min(.01f, remaining);
    physics(step, in);
    remaining -= step;
  }
  if (split_count < SectorCount && segment + route_t >= SectorEnds[split_count] &&
      furthest >= SectorEnds[split_count] - 1 && std::abs(lateral) < road_width() + 2) {
    splits[split_count] = elapsed;
    split_delta = elapsed - reference_splits[split_count];
    ++split_count;
    split_message = 2.5f;
  }
  if (split_count == SectorCount) {
    mode = Mode::Finished;
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
  uint32_t hash = 2166136261u;
  for (uint32_t word : {r.magic, r.version, r.milliseconds, r.course, r.splits[0], r.splits[1],
                        r.splits[2], r.splits[3]}) {
    for (int i = 0; i < 4; ++i) {
      hash = (hash ^ uint8_t(word)) * 16777619u;
      word >>= 8;
    }
  }
  return hash;
}
SaveRecord encode_best(float seconds, const std::array<float, SectorCount> &splits) {
  SaveRecord r{0x52414c59, 2, uint32_t(clamp(seconds * 1000.f, 0, 86400000.f) + .5f), 0};
  for (int i = 0; i < SectorCount - 1; ++i)
    r.splits[i] = uint32_t(clamp(splits[i] * 1000.f, 0, 86400000.f) + .5f);
  r.checksum = checksum(r);
  return r;
}
float decode_best(const SaveRecord &r) {
  if (r.magic != 0x52414c59 || r.version != 2 || r.course != CourseVersion ||
      r.milliseconds < 1000 || r.milliseconds > 86400000 || r.checksum != checksum(r) ||
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
  uint32_t hash = 2166136261u;
  auto word = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i) {
      hash = (hash ^ uint8_t(v)) * 16777619u;
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
  save.magic = 0x4752564c;
  save.version = 1;
  save.car = game.selected_car;
  save.track = game.selected_track;
  for (int i = 0; i < CarCount * TrackCount; ++i)
    for (int j = 0; j < SectorCount; ++j)
      save.times[i][j] = uint32_t(clamp(game.records[i].splits[j] * 1000.f, 0, 86400000.f) + .5f);
  save.checksum = save_checksum(save);
  return save;
}
bool load_save(Game &game, const SaveData &save) {
  if (save.magic != 0x4752564c || save.version != 1 || save.car >= CarCount ||
      save.track >= TrackCount || save.checksum != save_checksum(save))
    return false;
  for (auto &times : save.times) {
    if (times.back() == 0) {
      for (auto t : times)
        if (t)
          return false;
    } else {
      if (times.front() == 0 || times.back() > 86400000)
        return false;
      for (int i = 1; i < SectorCount; ++i)
        if (times[i] <= times[i - 1])
          return false;
    }
  }
  for (int i = 0; i < CarCount * TrackCount; ++i)
    for (int j = 0; j < SectorCount; ++j)
      game.records[i].splits[j] = save.times[i][j] * .001f;
  game.select(int(save.car), int(save.track));
  game.mode = Mode::Title;
  return true;
}
} // namespace rally
