#pragma once
#include <array>
#include <cstdint>

namespace rally {
constexpr int W = 120, H = 120, NodeCount = 301;
constexpr float Step = 6.0f, RoadHalf = 4.1f;
constexpr int SectorCount = 5;
constexpr std::array<int, SectorCount> SectorEnds{60, 120, 180, 240, NodeCount - 4};
// Cumulative times from one braking-aware drive, with a 2.4% margin.
constexpr std::array<float, SectorCount> DefaultSplits{22.02f, 45.32f, 67.22f, 92.21f, 113.f};
constexpr uint32_t CourseVersion = 3;
struct Vec {
  float x = 0, y = 0, z = 0;
};
inline Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec operator*(Vec a, float b) { return {a.x * b, a.y * b, a.z * b}; }
float clamp(float x, float a, float b);
float angle_delta(float a, float b);
struct Node {
  Vec p;
  float heading = 0;
  float turn = 0;
  Vec right{};
  float bank = 0, verge_left = 0, verge_right = 0;
  float half_width = RoadHalf, far_left = 100, far_right = 100;
};
struct Input {
  bool left = false, right = false, throttle = false, brake = false, handbrake = false,
       action = false, pause = false, back = false;
};
enum class Mode { Title, CarSelect, TrackSelect, Countdown, Racing, Paused, Finished };
enum class Controls { Pico, Keyboard, Gamepad, Touch };
constexpr int CarCount = 3, TrackCount = 3;
struct CarSpec {
  const char *name;
  const char *difficulty;
  float acceleration, max_speed, grip, traction, steering;
  float width, height, length;
  int speed_stat, accel_stat, drift_stat;
};
extern const std::array<CarSpec, CarCount> Cars;
extern const std::array<const char *, TrackCount> TrackNames;
struct Record {
  std::array<float, SectorCount> splits{};
};

struct Game {
  std::array<Node, NodeCount> road{};
  std::array<std::array<Vec, 10>, NodeCount> terrain{};
  Vec car{}, velocity{};
  float yaw = 0, camera_yaw = 0, speed = 0, steer = 0, lateral = 0, route_t = 0;
  float elapsed = 0, countdown = 3, best = 0, previous_best = 0;
  float impact = 0, stranded = 0, recovery_message = 0, slip = 0;
  float ground_y = 0, vertical_speed = 0, pitch = 0, roll = 0, camera_height = 0;
  float split_message = 0, split_delta = 0;
  std::array<float, SectorCount> splits{}, best_splits{}, reference_splits = DefaultSplits;
  int split_count = 0, jumps = 0;
  bool airborne = false;
  int segment = 0, furthest = 0, recoveries = 0;
  Mode mode = Mode::Title, resume_mode = Mode::Racing;
  bool new_record = false, save_requested = false;
  int selected_car = 1, selected_track = 0;
  Controls controls = Controls::Pico;
  float menu_rotation = 0;
  bool menu_left = false, menu_right = false;
  std::array<Record, CarCount * TrackCount> records{};
  const CarSpec &spec() const { return Cars[selected_car]; }
  void build_track();
  void select(int car_index, int track_index);
  float surface_grip(int node) const;
  bool icy(int node) const;
  const std::array<float, SectorCount> &default_splits() const;
  Game();
  void restart();
  void tick(float dt, const Input &in);
  void physics(float dt, const Input &in);
  void locate();
  void recover();
  Vec roadside(int i, float side) const;
  float terrain_height(int i, float side) const;
  float road_width() const;
  Vec scenery(int i, int sign) const;
  static int section(int node);
  const char *section_name(int node) const;
  void select_reference();
  float progress() const;
};
struct SaveData {
  uint32_t magic = 0, version = 0, car = 1, track = 0;
  std::array<std::array<uint32_t, SectorCount>, CarCount * TrackCount> times{};
  uint32_t checksum = 0;
};
SaveData encode_save(const Game &game);
bool load_save(Game &game, const SaveData &save);
struct SaveRecord {
  uint32_t magic, version, milliseconds, checksum;
  uint32_t course = CourseVersion;
  std::array<uint32_t, SectorCount - 1> splits{};
};
SaveRecord encode_best(float seconds, const std::array<float, SectorCount> &splits);
float decode_best(const SaveRecord &record);
void load_best(Game &game, const SaveRecord &record);

struct Renderer {
  struct Triangle {
    int16_t x[3], y[3];
    uint16_t inverse_z[3], color;
    uint16_t shadow = 0;
  };
  struct CameraVertex {
    int32_t x = 0, y = 0, z = 0;
    int16_t sx = 0, sy = 0;
    uint16_t inverse_z = 0;
  };
  struct CachedVertex {
    int32_t x = 0, y = 0, z = 0;
    uint32_t frame = 0;
    CameraVertex transformed{};
  };
  std::array<CachedVertex, 512> vertex_cache{};
  uint32_t render_frame = 0;
  int32_t camera_x = 0, camera_y = 0, camera_z = 0, sine = 0, cosine = 16384;
  struct ShadowPolygon {
    int16_t x[8]{}, y[8]{};
    int count = 0, left = 0, right = 0, top = 0, bottom = 0;
  };
  std::array<ShadowPolygon, 64> shadow_polygons{};
  int shadow_count = 0;
  std::array<Triangle, 1800> faces{};
  std::array<uint16_t, W * H> depth_buffer{};
  std::array<uint8_t, 512> ridge_heights{};
  uint32_t geometry_us = 0, raster_us = 0;
  Renderer();
  int face_count = 0, dropped = 0;
  uint16_t *pixels = nullptr;
  Vec camera{};
  float cam_sin = 0, cam_cos = 1;
  void render(const Game &game, uint16_t *target, int fps = 0, bool diagnostics = false);
  Vec shadow_center{};
  float shadow_sin = 0, shadow_cos = 1, shadow_width = 1.05f, shadow_length = 1.8f;
  bool shadow_enabled = false;
  float shadow_min_x = 0, shadow_max_x = 0, shadow_min_z = 0, shadow_max_z = 0;
  void ground_triangle(Vec a, Vec b, Vec c, uint16_t color);
  void ground_quad(Vec a, Vec b, Vec c, Vec d, uint16_t color);
  void triangle(Vec a, Vec b, Vec c, uint16_t color);
  CameraVertex transform(Vec vertex);
  void project(CameraVertex &vertex);
  void quad(Vec a, Vec b, Vec c, Vec d, uint16_t color);
  void box(Vec p, Vec size, float yaw, uint16_t color);
  void tree(Vec p, float height, int seed);
  void snow_tree(Vec p, float height, int seed);
  void distant_snow_tree(Vec p, float height, int seed);
  void raster(const Triangle &triangle);
  void rect(int x, int y, int w, int h, uint16_t color);
  void text(int x, int y, const char *value, uint16_t color, int scale = 1);
  void centered(int y, const char *value, uint16_t color, int scale = 1);
};
uint16_t color(int r, int g, int b);
} // namespace rally
