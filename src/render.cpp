#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef GRAVELBYTE_PROFILE
#include "pico/time.h"
#endif

namespace rally {
static uint32_t profile_time() {
#ifdef GRAVELBYTE_PROFILE
  return time_us_32();
#else
  return 0;
#endif
}
Renderer::Renderer() {
  for (int i = 0; i < int(ridge_heights.size()); ++i)
    ridge_heights[i] = uint8_t(29 + int(3 * std::sin(i * tuning::Tau * 6 / 512) +
                                        2 * std::sin(i * tuning::Tau * 14 / 512)));
}
uint16_t color(int r, int g, int b) { return uint16_t((r << 12) | (g << 8) | (b << 4) | 15); }
static uint16_t shade(uint16_t c, int amount) {
  return color(std::clamp((c >> 12) + amount, 0, 15), std::clamp(((c >> 8) & 15) + amount, 0, 15),
               std::clamp(((c >> 4) & 15) + amount, 0, 15));
}
static uint16_t fog(uint16_t c, int depth) {
  int t = std::clamp((depth - 35 * 64) * 3 / 2560, 0, 12);
  int r = ((c >> 12) * (16 - t) + 9 * t) >> 4;
  int g = (((c >> 8) & 15) * (16 - t) + 10 * t) >> 4;
  int b = (((c >> 4) & 15) * (16 - t) + 10 * t) >> 4;
  return color(r, g, b);
}
void Renderer::rect(int x, int y, int w, int h, uint16_t c) {
  for (int py = std::max(0, y); py < std::min(H, y + h); ++py)
    for (int px = std::max(0, x); px < std::min(W, x + w); ++px)
      pixels[py * W + px] = c;
}
// Three by five uppercase font. Bits run left-to-right, top-to-bottom.
static uint16_t glyph(char c) {
  static constexpr const char *letters[] = {
      "010101111101101", "110101110101110", "011100100100011", "110101101101110", "111100110100111",
      "111100110100100", "011100101101011", "101101111101101", "111010010010111", "001001001101010",
      "101101110101101", "100100100100111", "101111111101101", "101111111111101", "010101101101010",
      "110101110100100", "010101101111011", "110101110101101", "011100010001110", "111010010010010",
      "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
      "111001010100111"};
  static constexpr const char *digits[] = {"111101101101111", "010110010010111", "110001111100111",
                                           "110001010001110", "101101111001001", "111100110001110",
                                           "011100111101111", "111001010010010", "111101111101111",
                                           "111101111001110"};
  if (c == 's')
    return 0x071e;
  const char *pattern = nullptr;
  if (c >= 'A' && c <= 'Z')
    pattern = letters[c - 'A'];
  if (c >= 'a' && c <= 'z')
    pattern = letters[c - 'a'];
  if (c >= '0' && c <= '9')
    pattern = digits[c - '0'];
  if (pattern) {
    uint16_t bits = 0;
    for (int i = 0; i < 15; ++i)
      bits = uint16_t((bits << 1) | (pattern[i] == '1'));
    return bits;
  }
  if (c == '<')
    return 0x1511;
  if (c == '>')
    return 0x4454;
  if (c == ':')
    return 0x0410;
  if (c == '.')
    return 0x0002;
  if (c == '-')
    return 0x01c0;
  if (c == '+')
    return 0x05d0;
  if (c == '/')
    return 0x12a4;
  if (c == '!')
    return 0x2492;
  return 0;
}
void Renderer::text(int x, int y, const char *value, uint16_t c, int scale) {
  for (; *value; ++value, x += 4 * scale) {
    uint16_t bits = glyph(*value);
    for (int row = 0; row < 5; ++row)
      for (int col = 0; col < 3; ++col)
        if (bits & (1u << (14 - row * 3 - col)))
          rect(x + col * scale, y + row * scale, scale, scale, c);
  }
}
void Renderer::centered(int y, const char *v, uint16_t c, int scale) {
  text((W - int(std::strlen(v)) * 4 * scale + scale) / 2, y, v, c, scale);
}
void Renderer::project(CameraVertex &v) {
  if (v.z < tuning::NearPlane)
    return;
  v.sx =
      int16_t(std::clamp<int32_t>(tuning::CenterX + tuning::FocalLength * v.x / v.z, -2000, 2000));
  v.sy = int16_t(std::clamp<int32_t>(projection_y - tuning::FocalLength * v.y / v.z, -2000, 2000));
  v.inverse_z = uint16_t(tuning::DepthNumerator / v.z);
}
Renderer::CameraVertex Renderer::transform(Vec world) {
  // Q6 world positions and Q14 camera basis. The visible radius is <300m,
  // keeping the products comfortably inside signed 32-bit range.
  const int32_t wx = int32_t(world.x * tuning::WorldScale),
                wy = int32_t(world.y * tuning::WorldScale),
                wz = int32_t(world.z * tuning::WorldScale);
  uint32_t hash = uint32_t(wx) * 73856093u ^ uint32_t(wy) * 19349663u ^ uint32_t(wz) * 83492791u;
  auto &cached = vertex_cache[(hash ^ (hash >> 16)) & (tuning::VertexCacheSize - 1)];
  if (cached.frame == render_frame && cached.x == wx && cached.y == wy && cached.z == wz)
    return cached.transformed;
  int32_t x = wx - camera_x, y = wy - camera_y, z = wz - camera_z;
  const int32_t forward = (x * sine + z * cosine) >> tuning::BasisShift;
  CameraVertex v;
  v.x = (x * cosine - z * sine) >> tuning::BasisShift;
  v.y = (y * pitch_cosine + forward * pitch_sine) >> tuning::BasisShift;
  v.z = (forward * pitch_cosine - y * pitch_sine) >> tuning::BasisShift;
  project(v);
  cached = {wx, wy, wz, render_frame, v};
  return v;
}
void Renderer::triangle(Vec a, Vec b, Vec c, uint16_t col) {
  CameraVertex input[] = {transform(a), transform(b), transform(c)}, clipped[5];
  if (input[0].z > tuning::FarPlane && input[1].z > tuning::FarPlane &&
      input[2].z > tuning::FarPlane)
    return;
  // Clip against near plane before perspective divide (no road popping at camera).
  int count = 0;
  for (int i = 0; i < 3; ++i) {
    CameraVertex p = input[i], q = input[(i + 1) % 3];
    bool p_in = p.z >= tuning::NearPlane, q_in = q.z >= tuning::NearPlane;
    if (p_in)
      clipped[count++] = p;
    if (p_in != q_in) {
      CameraVertex edge;
      edge.x = p.x + (q.x - p.x) * (tuning::NearPlane - p.z) / (q.z - p.z);
      edge.y = p.y + (q.y - p.y) * (tuning::NearPlane - p.z) / (q.z - p.z);
      edge.z = tuning::NearPlane;
      project(edge);
      clipped[count++] = edge;
    }
  }
  for (int i = 1; i < count - 1; ++i) {
    if (face_count == int(faces.size())) {
      ++dropped;
      return;
    }
    CameraVertex v[] = {clipped[0], clipped[i], clipped[i + 1]};
    Triangle t{};
    t.color = fog(col, (v[0].z + v[1].z + v[2].z) / 3);
    for (int j = 0; j < 3; ++j) {
      t.x[j] = v[j].sx;
      t.y[j] = v[j].sy;
      t.inverse_z[j] = v[j].inverse_z;
    }
    if (std::max({t.x[0], t.x[1], t.x[2]}) < 0 || std::min({t.x[0], t.x[1], t.x[2]}) >= W ||
        std::max({t.y[0], t.y[1], t.y[2]}) < 0 || std::min({t.y[0], t.y[1], t.y[2]}) >= H)
      continue;
    faces[face_count++] = t;
  }
}
// Clip a material mask to the actual ground triangle. Rasterize the original
// triangle once, so no overlapping surface or new geometry seam can flicker.
void Renderer::ground_triangle(Vec a, Vec b, Vec c, uint16_t col) {
  const int start = face_count;
  triangle(a, b, c, col);
  if (start == face_count || !shadow_enabled || std::max({a.x, b.x, c.x}) < shadow_min_x ||
      std::min({a.x, b.x, c.x}) > shadow_max_x || std::max({a.z, b.z, c.z}) < shadow_min_z ||
      std::min({a.z, b.z, c.z}) > shadow_max_z)
    return;
  struct Vertex {
    Vec world;
    float x, z;
  };
  auto local = [&](Vec p) {
    Vec d = p - shadow_center;
    return Vertex{p, d.x * shadow_cos - d.z * shadow_sin, d.x * shadow_sin + d.z * shadow_cos};
  };
  Vertex polygon[8] = {local(a), local(b), local(c)};
  int count = 3;
  auto distance = [&](const Vertex &p, int plane) {
    return plane == 0   ? shadow_width - p.x
           : plane == 1 ? shadow_width + p.x
           : plane == 2 ? shadow_length - p.z
                        : shadow_length + p.z;
  };
  for (int plane = 0; plane < 4 && count >= 3; ++plane) {
    Vertex inside[8];
    int ni = 0;
    for (int i = 0; i < count; ++i) {
      const auto p = polygon[i], q = polygon[(i + 1) % count];
      float dp = distance(p, plane), dq = distance(q, plane);
      if (dp >= 0)
        inside[ni++] = p;
      if ((dp >= 0) != (dq >= 0)) {
        float t = dp / (dp - dq);
        inside[ni++] = {p.world + (q.world - p.world) * t, p.x + (q.x - p.x) * t,
                        p.z + (q.z - p.z) * t};
      }
    }
    count = ni;
    for (int i = 0; i < count; ++i)
      polygon[i] = inside[i];
  }
  if (count < 3)
    return;
  if (shadow_count == int(shadow_polygons.size())) {
    ++dropped;
    return;
  }
  ShadowPolygon mask;
  mask.count = count;
  mask.left = mask.top = 2000;
  mask.right = mask.bottom = -2000;
  for (int i = 0; i < count; ++i) {
    auto v = transform(polygon[i].world);
    if (v.z < tuning::NearPlane)
      return;
    mask.x[i] = v.sx;
    mask.y[i] = v.sy;
    mask.left = std::min(mask.left, int(v.sx));
    mask.right = std::max(mask.right, int(v.sx));
    mask.top = std::min(mask.top, int(v.sy));
    mask.bottom = std::max(mask.bottom, int(v.sy));
  }
  shadow_polygons[shadow_count++] = mask;
  for (int i = start; i < face_count; ++i)
    faces[i].shadow = uint16_t(shadow_count);
}
void Renderer::ground_quad(Vec a, Vec b, Vec c, Vec d, uint16_t col) {
  ground_triangle(a, b, c, col);
  ground_triangle(a, c, d, col);
}
void Renderer::quad(Vec a, Vec b, Vec c, Vec d, uint16_t col) {
  triangle(a, b, c, col);
  triangle(a, c, d, col);
}
void Renderer::box(Vec p, Vec size, float yaw, uint16_t col) {
  Vec v[8];
  float sn = yaw == 0 ? 0 : std::sin(yaw), cs = yaw == 0 ? 1 : std::cos(yaw);
  for (int i = 0; i < 8; ++i) {
    float x = (i & 1 ? 1 : -1) * size.x * .5f, z = (i & 2 ? 1 : -1) * size.z * .5f;
    v[i] = p + Vec{x * cs + z * sn, (i & 4) ? size.y : 0, z * cs - x * sn};
  }
  const Vec relative = camera - p;
  const float local_x = relative.x * cs - relative.z * sn,
              local_z = relative.x * sn + relative.z * cs;
  if (local_z < -size.z * .5f)
    quad(v[0], v[1], v[5], v[4], shade(col, -2));
  if (local_z > size.z * .5f)
    quad(v[2], v[3], v[7], v[6], col);
  if (local_x < -size.x * .5f)
    quad(v[0], v[2], v[6], v[4], shade(col, -1));
  if (local_x > size.x * .5f)
    quad(v[1], v[3], v[7], v[5], col);
  if (relative.y > size.y)
    quad(v[4], v[5], v[7], v[6], shade(col, 1));
}
void Renderer::tree(Vec p, float height, int seed) {
  box(p, {.36f, height * .45f, .36f}, 0, color(4, 4, 3));
  for (int tier = 0; tier < 2; ++tier) {
    float y = height * (tier ? .38f : .16f), r = height * (tier ? .24f : .33f);
    Vec tip = p + Vec{0, height * (tier ? 1.f : .77f), 0};
    Vec a = p + Vec{-r, y, -r}, b = p + Vec{r, y, -r}, c = p + Vec{r, y, r}, d = p + Vec{-r, y, r};
    uint16_t col = color(2 + seed % 2, 4 + seed % 3, 3 + seed % 2);
    triangle(a, b, tip, shade(col, -1));
    triangle(b, c, tip, col);
    triangle(c, d, tip, shade(col, 1));
    triangle(d, a, tip, col);
  }
}
void Renderer::snow_tree(Vec p, float height, int seed) {
  box(p, {.36f, height * .45f, .36f}, 0, color(4, 4, 3));
  for (int tier = 0; tier < 2; ++tier) {
    float y = height * (tier ? .38f : .16f), r = height * (tier ? .24f : .33f);
    Vec tip = p + Vec{0, height * (tier ? 1.f : .77f), 0};
    Vec a = p + Vec{-r, y, -r}, b = p + Vec{r, y, -r}, c = p + Vec{r, y, r}, d = p + Vec{-r, y, r};
    uint16_t col = color(2 + seed % 2, 4 + seed % 3, 3 + seed % 2);
    // Close the underside when viewed from below a branch tier. The original
    // double-sided open pyramid used its back faces to cover this silhouette.
    if (camera.y < p.y + y)
      quad(a, b, c, d, shade(col, -2));
    // Exact pyramid face normals: retain every camera-facing face, including
    // all four when looking down from above. Hidden faces cannot affect colour.
    const float rise = tip.y - (p.y + y), threshold = (tip.y - camera.y) * r;
    const float x = (camera.x - p.x) * rise, z = (camera.z - p.z) * rise;
    if (-z >= threshold)
      triangle(a, b, tip, shade(col, -1));
    if (x >= threshold)
      triangle(b, c, tip, col);
    if (z >= threshold)
      triangle(c, d, tip, shade(col, 1));
    if (-x >= threshold)
      triangle(d, a, tip, col);
  }
}
// Beyond 84m the two branch tiers occupy only a handful of pixels. Keep that
// silhouette and the snow cap with camera-facing world geometry, avoiding the
// hidden volume work. Nearby conifers retain the full 3D model.
void Renderer::distant_snow_tree(Vec p, float height, int seed, bool snow) {
  const Vec right = {cam_cos, 0, -cam_sin};
  const float breadth = std::abs(cam_cos) + std::abs(cam_sin);
  const uint16_t leaves = color(2 + seed % 2, 4 + seed % 3, 3 + seed % 2);
  quad(p - right * .18f, p + right * .18f, p + right * .18f + Vec{0, height * .45f, 0},
       p - right * .18f + Vec{0, height * .45f, 0}, color(4, 4, 3));
  triangle(p - right * (height * .33f * breadth) + Vec{0, height * .16f, 0},
           p + right * (height * .33f * breadth) + Vec{0, height * .16f, 0},
           p + Vec{0, height * .77f, 0}, leaves);
  Vec left = p - right * (height * .24f * breadth) + Vec{0, height * .38f, 0},
      edge = p + right * (height * .24f * breadth) + Vec{0, height * .38f, 0},
      tip = p + Vec{0, height, 0};
  Vec snow_left = left + (tip - left) * .7f, snow_right = edge + (tip - edge) * .7f;
  quad(left, edge, snow_right, snow_left, leaves);
  triangle(snow_left, snow_right, tip, snow ? color(14, 15, 15) : leaves);
}
void Renderer::raster(const Triangle &t) {
  const ShadowPolygon *shadow = t.shadow ? &shadow_polygons[t.shadow - 1] : nullptr;
  const uint16_t shadow_color = shadow ? shade(t.color, -4) : t.color;
  // Walk the two edges incrementally: divisions happen once per edge, rather
  // than on every scanline. Screen x uses Q16; reciprocal depth uses Q8.
  int order[] = {0, 1, 2};
  if (t.y[order[0]] > t.y[order[1]])
    std::swap(order[0], order[1]);
  if (t.y[order[1]] > t.y[order[2]])
    std::swap(order[1], order[2]);
  if (t.y[order[0]] > t.y[order[1]])
    std::swap(order[0], order[1]);
  const int top = order[0], middle = order[1], bottom = order[2];
  if (t.y[top] == t.y[bottom])
    return;
  const int long_height = t.y[bottom] - t.y[top];
  const int long_dx = (int(t.x[bottom]) - t.x[top]) * 65536 / long_height;
  const int long_dz = (int(t.inverse_z[bottom]) - t.inverse_z[top]) * 256 / long_height;
  for (int half = 0; half < 2; ++half) {
    int a = half ? middle : top, b = half ? bottom : middle;
    int height = t.y[b] - t.y[a];
    if (height == 0)
      continue;
    int first = std::max(0, int(t.y[a])), last = std::min(H, int(t.y[b]));
    if (first >= last)
      continue;
    int dx = (int(t.x[b]) - t.x[a]) * 65536 / height;
    int dz = (int(t.inverse_z[b]) - t.inverse_z[a]) * 256 / height;
    int x1 = int(t.x[a]) * 65536 + (first - t.y[a]) * dx;
    int z1 = int(t.inverse_z[a]) * 256 + (first - t.y[a]) * dz;
    int x2 = int(t.x[top]) * 65536 + (first - t.y[top]) * long_dx;
    int z2 = int(t.inverse_z[top]) * 256 + (first - t.y[top]) * long_dz;
    for (int y = first; y < last; ++y, x1 += dx, z1 += dz, x2 += long_dx, z2 += long_dz) {
      int lo = x1 >> 16, hi = x2 >> 16, zlo = z1, zhi = z2;
      if (lo > hi) {
        std::swap(lo, hi);
        std::swap(zlo, zhi);
      }
      int left = std::max(0, lo), right = std::min(W - 1, hi);
      if (left > right)
        continue;
      int step = hi > lo ? (zhi - zlo) / (hi - lo) : 0, z = zlo + (left - lo) * step;
      if (!shadow || y < shadow->top || y > shadow->bottom) {
        for (int x = left; x <= right; ++x, z += step) {
          const int offset = y * W + x;
          if ((z >> 8) >= depth_buffer[offset]) {
            depth_buffer[offset] = uint16_t(z >> 8);
            pixels[offset] = t.color;
          }
        }
      } else {
        // Intersect the convex material mask once per scanline, not per pixel.
        int mask_left = W, mask_right = -1;
        for (int i = 0, j = shadow->count - 1; i < shadow->count; j = i++) {
          const int y0 = shadow->y[j], y1 = shadow->y[i], x0 = shadow->x[j], x1 = shadow->x[i];
          if (y < std::min(y0, y1) || y > std::max(y0, y1))
            continue;
          if (y0 == y1) {
            mask_left = std::min(mask_left, std::min(x0, x1));
            mask_right = std::max(mask_right, std::max(x0, x1));
          } else {
            int x = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            mask_left = std::min(mask_left, x);
            mask_right = std::max(mask_right, x);
          }
        }
        for (int x = left; x <= right; ++x, z += step) {
          const int offset = y * W + x;
          if ((z >> 8) >= depth_buffer[offset]) {
            depth_buffer[offset] = uint16_t(z >> 8);
            pixels[offset] = (x >= mask_left && x <= mask_right) ? shadow_color : t.color;
          }
        }
      }
    }
  }
}
static void time_text(char *out, size_t size, float seconds) {
  int cs = int(seconds * 100);
  std::snprintf(out, size, "%02d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
}
void Renderer::render(const Game &g, uint16_t *target, int fps, bool diagnostics) {
  const uint32_t geometry_start = profile_time();
  pixels = target;
  face_count = 0;
  shadow_count = 0;
  dropped = 0;
  depth_buffer.fill(0);
  const bool showroom = g.mode == Mode::CarSelect;
  projection_y = showroom ? tuning::ShowroomCenterY : tuning::CenterY;
  const bool cinematic = g.mode == Mode::Title || g.mode == Mode::Finished;
  float view_yaw = g.camera_yaw;
  cam_sin = std::sin(view_yaw);
  cam_cos = std::cos(view_yaw);
  float camera_distance = showroom ? tuning::ShowroomDistance : tuning::ChaseDistance;
  float height = showroom ? tuning::ShowroomHeight : tuning::ChaseHeight;
  camera = g.car + Vec{-cam_sin * camera_distance, height, -cam_cos * camera_distance};
  camera.y = g.camera_height + height;
  pitch_sine = tuning::ChasePitchSine;
  pitch_cosine = tuning::ChasePitchCosine;
  if (cinematic) {
    // Planned road-relative shots keep the camera clear of tunnel roofs/walls.
    bool portal = g.selected_track == 2 &&
                  g.segment >= tuning::TunnelStart - tuning::PortalCameraMargin &&
                  g.segment <= tuning::TunnelEnd + tuning::PortalCameraMargin;
    int shot = portal ? 0 : int(g.cinematic_time / tuning::ShotSeconds) % tuning::CameraShotCount;
    if (shot == 1) {
      int node = std::clamp(g.segment + tuning::RoadsideLookAhead, 0, NodeCount - 1);
      const int next = std::min(NodeCount - 1, node + 1);
      Vec start = g.roadside(node, -(g.road[node].half_width + tuning::RoadsideOffset));
      Vec end = g.roadside(next, -(g.road[next].half_width + tuning::RoadsideOffset));
      camera = start + (end - start) * g.route_t + Vec{0, tuning::RoadsideHeight, 0};
      camera.y = std::max(camera.y, g.car.y + tuning::RoadsideMinimumHeight);
    } else if (shot == 2) {
      camera = g.car + Vec{-cam_sin * tuning::HighShotBack + cam_cos * tuning::HighShotOffset,
                           tuning::HighShotHeight,
                           -cam_cos * tuning::HighShotBack - cam_sin * tuning::HighShotOffset};
    }
    Vec aim = g.car + Vec{0, tuning::CarAimHeight, 0} - camera;
    view_yaw = std::atan2(aim.x, aim.z);
    float horizontal = std::sqrt(aim.x * aim.x + aim.z * aim.z);
    float pitch_angle = std::atan2(-aim.y, horizontal);
    pitch_sine = int32_t(std::sin(pitch_angle) * tuning::BasisScale);
    pitch_cosine = int32_t(std::cos(pitch_angle) * tuning::BasisScale);
    cam_sin = std::sin(view_yaw);
    cam_cos = std::cos(view_yaw);
  }
  camera_x = int32_t(camera.x * tuning::WorldScale);
  camera_y = int32_t(camera.y * tuning::WorldScale);
  camera_z = int32_t(camera.z * tuning::WorldScale);
  sine = int32_t(cam_sin * tuning::BasisScale);
  cosine = int32_t(cam_cos * tuning::BasisScale);
  if (++render_frame == 0) {
    for (auto &entry : vertex_cache)
      entry.frame = 0;
    render_frame = 1;
  }
  shadow_center = g.car;
  shadow_sin = std::sin(showroom ? g.menu_rotation : g.yaw);
  shadow_cos = std::cos(showroom ? g.menu_rotation : g.yaw);
  shadow_width = 1.05f * g.spec().width;
  shadow_length = 1.8f * g.spec().length;
  const float extent_x = std::abs(shadow_cos) * shadow_width + std::abs(shadow_sin) * shadow_length;
  const float extent_z = std::abs(shadow_sin) * shadow_width + std::abs(shadow_cos) * shadow_length;
  shadow_min_x = g.car.x - extent_x;
  shadow_max_x = g.car.x + extent_x;
  shadow_min_z = g.car.z - extent_z;
  shadow_max_z = g.car.z + extent_z;

  // Fog-coloured sky, distant wooded ridge; foreground is real world geometry.
  rect(0, 0, W, H, color(9, 10, 10));
  rect(0, 0, W, 24, color(10, 11, 12));
  for (int x = 0; x < W; ++x) {
    int phase = x + int(view_yaw * 28);
    int ridge = ridge_heights[unsigned(phase) & (tuning::RidgeSamples - 1)];
    rect(x, ridge - 5, 1, 29, color(8, 9, 10));
    rect(x,
         ridge + 4 + (ridge_heights[(unsigned(phase) + 143) & (tuning::RidgeSamples - 1)] - 29) * 2,
         1, 28, color(7, 8, 8));
  }
  rect(0, 52, W, H - 52, color(6, 7, 5));
  if (g.selected_track == 1) {
    rect(0, 0, W, 25, color(9, 12, 13));
    rect(0, 25, W, 27, color(8, 12, 14));
    rect(0, 43, W, 9, color(3, 9, 12));
    rect(0, 52, W, H - 52, color(12, 11, 7));
  }
  if (g.selected_track == 2) {
    rect(0, 0, W, 25, color(11, 12, 14));
    for (int x = 0; x < W; ++x) {
      int phase = (x + int(view_yaw * 38)) % 48;
      if (phase < 0)
        phase += 48;
      int peak = 15 + std::abs(phase - 24);
      rect(x, peak, 1, 53 - peak, color(7, 9, 12));
      rect(x, peak, 1, std::max(1, (39 - peak) / 3), color(14, 15, 15));
    }
    rect(0, 52, W, H - 52, color(12, 13, 14));
  }
  if (showroom) {
    rect(0, 0, W, H, color(1, 2, 3));
    rect(0, 74, W, 46, color(2, 3, 4));
  }
  bool reverse_view = std::cos(view_yaw - g.road[g.segment].heading) < 0;
  const int behind = reverse_view ? tuning::RoadAhead : tuning::RoadBehind;
  const int ahead = reverse_view ? tuning::RoadBehind : tuning::RoadAhead;
  const int first = std::max(0, g.segment - behind),
            last = std::min(NodeCount - 1, g.segment + ahead);
  for (int i = first; !showroom && i < last; ++i) {
    shadow_enabled = std::abs(i - g.segment) <= 2;
    auto at = [&](int j, float side, float rise = 0.f) {
      return g.roadside(j, side) + Vec{0, rise, 0};
    };
    const int zone = Game::section(i);
    const uint16_t grasses[] = {color(4, 6, 3), color(7, 7, 4), color(6, 6, 5), color(3, 6, 4)};
    const uint16_t gravels[] = {color(9, 9, 7), color(10, 9, 7), color(8, 8, 8), color(8, 8, 6)};
    uint16_t grass = grasses[zone], roadcol = gravels[zone];
    if (g.selected_track == 1) {
      grass = color(9, 9 - zone % 2, 4);
      roadcol = color(12, 10, 7);
    }
    if (g.bridge(i))
      roadcol = color(8, 7, 5);
    if (g.selected_track == 2) {
      grass = color(13, 14, 15);
      roadcol = g.icy(i) ? color(7, 11, 13) : color(11, 12, 13);
    }
    if (g.tunnel(i))
      roadcol = color(6, 7, 8);
    const auto &a = g.terrain[i];
    const auto &b = g.terrain[i + 1];
    for (int side = 0; side < 2; ++side) {
      const int far = side ? 9 : 0, bank = side ? 8 : 1, verge = side ? 7 : 2, edge = side ? 6 : 3;
      // Authored world positions are built once, avoiding repeated software
      // floating-point terrain sampling for every face on the RP2040.
      const bool water = g.bridge(i) || (g.coast(i) && side == 1);
      uint16_t outer = water ? color(3, 8, 11) : grass;
      uint16_t bank_color = g.coast(i) && side == 1 ? color(14, 12, 8) : grass;
      ground_triangle(a[far], b[far], b[bank], shade(outer, -1));
      ground_triangle(a[far], b[bank], a[bank], outer);
      ground_triangle(a[bank], b[bank], b[verge], shade(bank_color, i % 3 == 0 ? 1 : 0));
      ground_triangle(a[bank], b[verge], a[verge], shade(bank_color, -1));
      ground_quad(a[verge], b[verge], b[edge], a[edge], shade(grass, 1));
    }
    // Material strips partition the surface: no coplanar wheel-track overlays.
    for (int strip = 3; strip < 6; ++strip)
      ground_quad(a[strip], b[strip], b[strip + 1], a[strip + 1],
                  shade(roadcol, strip == 4 ? -1 : 0));
    if (i % 3 == 0)
      for (int sign : {-1, 1}) {
        if (!g.has_scenery(i, sign))
          continue;
        Vec p = g.scenery(i, sign);
        if (g.selected_track == 2 ||
            (g.selected_track == 0 && (zone == 0 || zone == 3 || (zone == 1 && i % 12 == 0))) ||
            (g.selected_track == 1 && i % 12 == 0)) {
          const float height = 5.f + float((i * 7) % 4);
          bool distant = false;
          {
            const auto center = transform(p + Vec{0, height * .5f, 0});
            const int radius = int(height * .75f * tuning::WorldScale);
            // Conservative sphere/plane rejection keeps complete silhouettes
            // while avoiding geometry work for trees outside the camera view.
            if (center.z + radius < tuning::NearPlane || center.z - radius > tuning::FarPlane ||
                std::abs(center.x) * tuning::FocalLength >
                    center.z * tuning::CenterX + radius * 105 ||
                center.y * tuning::FocalLength > center.z * projection_y + radius * 101 ||
                -center.y * tuning::FocalLength > center.z * (H - projection_y) + radius * 109)
              continue;
            distant = center.z > tuning::SceneryDetailDistance * tuning::WorldScale;
          }
          if (distant)
            distant_snow_tree(p, height, i, g.selected_track == 2);
          else
            snow_tree(p, height, i);
          if (g.selected_track == 2 && !distant) {
            Vec top = p + Vec{0, 5.2f + float((i * 7) % 4), 0};
            triangle(top, p + Vec{-1.2f, 3.8f, 0}, p + Vec{1.2f, 3.8f, 0}, color(14, 15, 15));
          }
        } else {
          // Replace trees with heather/rock outcrops on exposed sections.
          float r = zone == 2 ? 1.4f : .8f, h = zone == 2 ? 2.f : .6f;
          Vec a = p + Vec{-r, 0, -r}, b = p + Vec{r, 0, -r}, c = p + Vec{r, 0, r},
              d = p + Vec{-r, 0, r};
          Vec top = p + Vec{-.3f, h, .2f};
          uint16_t rock = zone == 2 ? color(7, 8, 8) : color(6, 5, 5);
          triangle(a, b, top, shade(rock, -2));
          triangle(b, c, top, rock);
          triangle(c, d, top, shade(rock, 1));
          triangle(d, a, top, shade(rock, -1));
        }
      }
    if (g.bridge(i) || g.tunnel(i)) {
      const bool tunnel = g.tunnel(i);
      for (int sign : {-1, 1}) {
        Vec a = at(i, sign * (g.road[i].half_width + tuning::RailMargin));
        Vec b = at(i + 1, sign * (g.road[i + 1].half_width + tuning::RailMargin));
        float wall_height = tunnel ? tuning::TunnelHeight : 1.f;
        uint16_t wall = tunnel ? color(5, 6, 7) : color(8, 7, 5);
        quad(a, b, b + Vec{0, wall_height, 0}, a + Vec{0, wall_height, 0}, wall);
        if (!tunnel) {
          box(a, {.25f, 1.25f, .25f}, g.road[i].heading, color(12, 11, 8));
          quad(a, b, b + Vec{0, -1.2f, 0}, a + Vec{0, -1.2f, 0}, color(5, 5, 4));
          if (i % 3 == 0)
            box(a + Vec{0, -tuning::RiverDrop, 0}, {.7f, tuning::RiverDrop, .7f}, g.road[i].heading,
                color(6, 6, 5));
        }
      }
      if (tunnel) {
        Vec a = at(i, -g.road[i].half_width - tuning::RailMargin, tuning::TunnelHeight);
        Vec b = at(i, g.road[i].half_width + tuning::RailMargin, tuning::TunnelHeight);
        Vec c = at(i + 1, g.road[i + 1].half_width + tuning::RailMargin, tuning::TunnelHeight);
        Vec d = at(i + 1, -g.road[i + 1].half_width - tuning::RailMargin, tuning::TunnelHeight);
        quad(a, b, c, d, color(4, 5, 6));
        // Roof and snow-capped shoulders leave the full road corridor open.
        Vec ridge = g.road[i].p + Vec{0, tuning::TunnelHeight + 4.f, 0};
        Vec next_ridge = g.road[i + 1].p + Vec{0, tuning::TunnelHeight + 4.f, 0};
        quad(a, d, next_ridge, ridge, color(11, 12, 14));
        quad(b, c, next_ridge, ridge, color(14, 15, 15));
        if (i % 3 == 0) {
          Vec lamp = at(i, 0, tuning::TunnelHeight - .12f);
          box(lamp, {1.2f, .06f, .5f}, g.road[i].heading, color(15, 14, 9));
        }
      }
    }
    // Sector gates make the timing lines visible before reaching them.
    if (std::find(SectorEnds.begin(), SectorEnds.end(), i) != SectorEnds.end()) {
      for (int sign : {-1, 1}) {
        Vec p = at(i, sign * (g.road[i].half_width + .55f));
        box(p, {.24f, 3.2f, .24f}, g.road[i].heading, color(14, 12, 3));
        Vec right = g.road[i].right * .8f;
        quad(p + Vec{0, 3.2f, 0}, p + right + Vec{0, 3.2f, 0}, p + right + Vec{0, 2.1f, 0},
             p + Vec{0, 2.1f, 0}, color(3, 4, 10));
      }
    }
    if (i % 6 == 0)
      for (int sign : {-1, 1}) {
        Vec p = at(i, sign * (g.road[i].half_width + .6f));
        const auto marker = transform(p + Vec{0, .45f, 0});
        if (marker.z < -64 || std::abs(marker.x) > marker.z + 128)
          continue;
        box(p, {.16f, .85f, .16f}, 0, color(13, 13, 11));
        box(p + Vec{0, .6f, 0}, {.2f, .24f, .2f}, 0, color(12, 3, 2));
      }
    if (i == NodeCount - 4 || i == 2) {
      Vec p = at(i, 0, .035f);
      for (int k = 0; k < 10; ++k) {
        float width = g.road[i].half_width;
        float l = -width + k * width / 5, r = l + width / 5;
        Vec a = at(i, l, .035f), b = at(i, r, .035f);
        Vec d = {std::sin(g.road[i].heading) * .7f, 0, std::cos(g.road[i].heading) * .7f};
        quad(a, b, b + d, a + d, k % 2 ? color(2, 3, 3) : color(14, 14, 12));
      }
      (void)p;
    }
  }
  const float car_sin = shadow_sin, car_cos = shadow_cos;
  const float ps = std::sin(showroom ? 0 : g.pitch), pc = std::cos(showroom ? 0 : g.pitch),
              rs = std::sin(showroom ? 0 : g.roll), rc = std::cos(showroom ? 0 : g.roll);
  auto carpoint = [&](float x, float y, float z) {
    x *= g.spec().width;
    y *= g.spec().height;
    z *= g.spec().length;
    float ry = y * rc + x * rs, rx = x * rc - y * rs;
    float py = ry * pc + z * ps, pz = z * pc - ry * ps;
    return g.car + Vec{rx * car_cos + pz * car_sin, py, pz * car_cos - rx * car_sin};
  };
  auto panel = [&](Vec a, Vec b, Vec c, Vec d, uint16_t col) {
    quad(carpoint(a.x, a.y, a.z), carpoint(b.x, b.y, b.z), carpoint(c.x, c.y, c.z),
         carpoint(d.x, d.y, d.z), col);
  };
  auto carbox = [&](Vec p, Vec size, uint16_t col) {
    Vec v[8];
    for (int i = 0; i < 8; ++i)
      v[i] = carpoint(p.x + (i & 1 ? 1 : -1) * size.x * .5f, p.y + ((i & 4) ? size.y : 0),
                      p.z + (i & 2 ? 1 : -1) * size.z * .5f);
    Vec relative = camera - g.car;
    float x = relative.x * car_cos - relative.z * car_sin,
          z = relative.x * car_sin + relative.z * car_cos;
    if (z < p.z)
      quad(v[0], v[1], v[5], v[4], shade(col, -1));
    else
      quad(v[2], v[3], v[7], v[6], col);
    if (x < p.x)
      quad(v[0], v[2], v[6], v[4], shade(col, -1));
    else
      quad(v[1], v[3], v[7], v[5], col);
    quad(v[4], v[5], v[7], v[6], shade(col, 1));
  };
  for (float x : {-.91f, .91f})
    for (float z : {-1.05f, 1.05f})
      carbox({x, .1f, z}, {.32f, .58f, .72f}, color(2, 2, 2));
  const uint16_t blue = g.selected_car == 0   ? color(14, 5, 3)
                        : g.selected_car == 1 ? color(2, 4, 12)
                                              : color(14, 13, 10);
  const uint16_t gold = g.selected_car == 0   ? color(15, 12, 8)
                        : g.selected_car == 1 ? color(15, 13, 3)
                                              : color(12, 3, 3),
                 glass = color(3, 5, 6);
  // A watertight painted shell: roof, glazing and stripes ARE the faces.
  // There is no underlying cabin box to fight their depth values.
  for (int sign : {-1, 1}) {
    float x = sign * .89f;
    panel({x, .36f, -1.62f}, {x, .36f, 1.62f}, {x, .57f, 1.62f}, {x, .57f, -1.62f},
          shade(blue, -2));
    panel({x, .57f, -1.62f}, {x, .57f, 1.62f}, {x, .78f, 1.62f}, {x, .78f, -1.62f}, gold);
    panel({x, .78f, -1.62f}, {x, .78f, 1.62f}, {x, .97f, 1.52f}, {x, .97f, -1.52f}, blue);
    float xb = sign * .82f, xt = sign * .62f;
    panel({xb, .97f, -1.12f}, {xb, .97f, -.32f}, {xt, 1.55f, -.24f}, {xt, 1.55f, -.70f},
          shade(glass, -1));
    panel({xb, .97f, -.32f}, {xb, .97f, -.20f}, {xt, 1.55f, -.12f}, {xt, 1.55f, -.24f}, blue);
    panel({xb, .97f, -.20f}, {xb, .97f, .92f}, {xt, 1.55f, .32f}, {xt, 1.55f, -.12f}, glass);
    panel({x, .97f, -1.52f}, {x, .97f, 1.52f}, {xb, .97f, .92f}, {xb, .97f, -1.12f}, blue);
  }
  panel({-.62f, 1.55f, -.70f}, {.62f, 1.55f, -.70f}, {.62f, 1.55f, .32f}, {-.62f, 1.55f, .32f},
        gold);
  panel({-.82f, .97f, -1.12f}, {.82f, .97f, -1.12f}, {.62f, 1.55f, -.70f}, {-.62f, 1.55f, -.70f},
        shade(glass, -1));
  panel({-.82f, .97f, .92f}, {.82f, .97f, .92f}, {.62f, 1.55f, .32f}, {-.62f, 1.55f, .32f},
        shade(glass, 1));
  panel({-.89f, .97f, -1.52f}, {.89f, .97f, -1.52f}, {.82f, .97f, -1.12f}, {-.82f, .97f, -1.12f},
        blue);
  panel({-.89f, .97f, 1.52f}, {.89f, .97f, 1.52f}, {.82f, .97f, .92f}, {-.82f, .97f, .92f}, blue);
  for (int sign : {-1, 1}) {
    float z = sign * 1.62f, zt = sign * 1.52f;
    panel({-.89f, .36f, z}, {.89f, .36f, z}, {.89f, .52f, z}, {-.89f, .52f, z}, color(2, 3, 4));
    panel({-.89f, .52f, z}, {.89f, .52f, z}, {.89f, .73f, z}, {-.89f, .73f, z}, shade(blue, -1));
    const float cuts[] = {-.89f, -.44f, .44f, .89f};
    for (int k = 0; k < 3; ++k)
      panel({cuts[k], .73f, z}, {cuts[k + 1], .73f, z}, {cuts[k + 1], .97f, zt},
            {cuts[k], .97f, zt}, k == 1 ? blue : (sign < 0 ? color(14, 3, 2) : color(15, 15, 11)));
  }
  if (g.selected_car != 0)
    carbox({0, 1.04f, -1.38f}, {1.86f, .12f, .32f}, shade(blue, -1));
  // Depth test preserves visibility through tight bends, terrain and car faces.
  geometry_us = profile_time() - geometry_start;
  const uint32_t raster_start = profile_time();
  for (int i = 0; i < face_count; ++i)
    raster(faces[i]);
  raster_us = profile_time() - raster_start;
  const uint16_t white = color(15, 15, 13), yellow = color(15, 13, 3), dark = color(1, 2, 2);
  char buffer[40];
  const char *confirm = g.controls == Controls::Pico       ? "A"
                        : g.controls == Controls::Keyboard ? "ENTER"
                        : g.controls == Controls::Gamepad  ? "A"
                                                           : "GO";
  const char *back = g.controls == Controls::Pico       ? "B"
                     : g.controls == Controls::Keyboard ? "ESC"
                     : g.controls == Controls::Gamepad  ? "B"
                                                        : "BACK";
  const char *pause = g.controls == Controls::Pico       ? "Y"
                      : g.controls == Controls::Keyboard ? "P"
                      : g.controls == Controls::Gamepad  ? "START"
                                                         : "PAUSE";
  const char *aux = g.controls == Controls::Keyboard ? "SPACE"
                    : g.controls == Controls::Touch  ? "TAP"
                                                     : "X";
  const char *audio = g.controls == Controls::Keyboard ? "M"
                      : g.controls == Controls::Touch  ? "TAP"
                                                       : "X";
  if (g.mode == Mode::Title) {
    rect(0, 0, W, 25, dark);
    centered(8, "GRAVELBYTE", white, 2);
    std::snprintf(buffer, sizeof(buffer), "PRESS %s TO CONTINUE", confirm);
    centered(99, buffer, white);
    std::snprintf(buffer, sizeof(buffer), "%s SOUND %s", audio, g.muted ? "OFF" : "ON");
    centered(111, buffer, yellow);
    return;
  }
  if (g.mode == Mode::Finished) {
    if (g.show_records) {
      rect(2, 7, 116, 94, dark);
      centered(12, "STAGE COMPLETE", yellow);
      time_text(buffer, sizeof(buffer), g.elapsed);
      centered(23, buffer, white, 2);
      centered(38, "GATE TARGET  BEST", yellow);
      for (int i = 0; i < SectorCount; ++i) {
        if (g.prior_splits[i] > 0)
          std::snprintf(buffer, sizeof(buffer), "%d %+.2f %+.2f", i + 1,
                        g.splits[i] - g.default_splits()[i], g.splits[i] - g.prior_splits[i]);
        else
          std::snprintf(buffer, sizeof(buffer), "%d %+.2f  --", i + 1,
                        g.splits[i] - g.default_splits()[i]);
        centered(48 + i * 9, buffer,
                 g.splits[i] < g.default_splits()[i] ? color(6, 15, 7) : color(15, 6, 4));
      }
      centered(94, "SECONDS / CUMULATIVE", white);
    }
    std::snprintf(buffer, sizeof(buffer), "%s RECORDS", aux);
    centered(104, buffer, yellow);
    std::snprintf(buffer, sizeof(buffer), "%s RETRY  %s SELECT", confirm, back);
    centered(113, buffer, white);
    return;
  }
  if (g.mode == Mode::CarSelect) {
    centered(6, g.spec().name, white, 2);
    centered(21, g.spec().difficulty, yellow);
    text(5, 52, "<", white, 2);
    text(107, 52, ">", white, 2);
    const char *labels[] = {"SPEED", "ACCEL", "DRIFT"};
    const int values[] = {g.spec().speed_stat, g.spec().accel_stat, g.spec().drift_stat};
    for (int row = 0; row < 3; ++row) {
      text(15, tuning::StatsTop + row * tuning::StatsRow, labels[row], white);
      for (int k = 0; k < tuning::StatBars; ++k)
        rect(53 + k * 10, tuning::StatsTop + row * tuning::StatsRow, 8, 4,
             k < values[row] ? yellow : color(4, 5, 5));
    }
    std::snprintf(buffer, sizeof(buffer), "%s NEXT  %s BACK", confirm, back);
    centered(112, buffer, white);
    return;
  }
  if (g.mode == Mode::TrackSelect) {
    rect(2, 3, 116, 24, dark);
    centered(8, TrackNames[g.selected_track], white);
    centered(19,
             g.selected_track == 0   ? "FOREST GRAVEL"
             : g.selected_track == 1 ? "DRY GRAVEL"
                                     : "SNOW AND ICE",
             yellow);
    text(5, 52, "<", white, 2);
    text(107, 52, ">", white, 2);
    rect(4, 88, 112, 29, dark);
    time_text(buffer, sizeof(buffer), g.default_splits().back());
    char line[40];
    std::snprintf(line, sizeof(line), "TO BEAT %s", buffer);
    if (g.unlocked(g.selected_track))
      centered(92, line, yellow);
    if (!g.unlocked(g.selected_track)) {
      std::snprintf(line, sizeof(line), "BEAT %s", TrackNames[g.selected_track - 1]);
      centered(92, "LOCKED", yellow);
      centered(101, line, white);
    } else
      centered(101, "1.8 KM  FIVE SPLITS", white);
    std::snprintf(buffer, sizeof(buffer),
                  g.unlocked(g.selected_track) ? "%s RACE  %s BACK" : "%s LOCKED  %s BACK", confirm,
                  back);
    centered(110, buffer, white);
    return;
  }
  rect(2, 2, 36, 9, dark);
  time_text(buffer, sizeof(buffer), g.elapsed);
  text(4, 4, buffer, white);
  rect(77, 2, 41, 17, dark);
  text(79, 4, "TO BEAT", yellow);
  time_text(buffer, sizeof(buffer), g.reference_splits.back());
  text(79, 12, buffer, white);
  rect(2, 22, 4 * int(std::strlen(g.section_name(g.segment))) + 4, 9, dark);
  text(4, 24, g.section_name(g.segment), white);
  std::snprintf(buffer, sizeof(buffer), "%d", std::min(SectorCount, g.split_count + 1));
  rect(2, 105, 7, 9, dark);
  text(4, 107, buffer, yellow);
  if (g.split_message > 0 && g.mode == Mode::Racing) {
    std::snprintf(buffer, sizeof(buffer), "%+.2fs", g.split_delta);
    centered(34, buffer, g.split_delta <= 0 ? color(6, 15, 7) : color(15, 6, 4));
  }
  // Pick the strongest curve in the next 90m, giving useful notice at racing speed.
  float upcoming = 0;
  int distance = 0;
  for (int i = g.segment + 3; i < std::min(NodeCount, g.segment + 16); ++i)
    if (std::abs(g.road[i].turn) > std::abs(upcoming)) {
      upcoming = g.road[i].turn;
      distance = (i - g.segment) * int(Step);
    }
  rect(49, 2, 22, 18, dark);
  const int sign = upcoming < 0 ? -1 : 1;
  if (std::abs(upcoming) < .007f) {
    rect(59, 5, 2, 8, yellow);
    rect(57, 5, 6, 2, yellow);
    rect(58, 4, 4, 1, yellow);
  } else {
    rect(59, 7, 2, 6, yellow);
    rect(sign > 0 ? 59 : 54, 6, 7, 2, yellow);
    rect(sign > 0 ? 64 : 54, 4, 2, 6, yellow);
    rect(sign > 0 ? 66 : 52, 5, 1, 4, yellow);
  }
  std::snprintf(buffer, sizeof(buffer), "%d", distance);
  centered(14, buffer, white);
  rect(93, 99, 25, 17, dark);
  std::snprintf(buffer, sizeof(buffer), "%03d", int(g.speed * 3.6f));
  text(95, 101, buffer, white, 2);
  text(100, 112, "KMH", yellow);
  rect(3, 116, 114, 2, dark);
  rect(3, 116, int(114 * g.progress()), 2, yellow);
  for (int i = 0; i < SectorCount; ++i) {
    int x = 3 + int(113.f * (SectorEnds[i] - 1) / (NodeCount - 5));
    const uint16_t checkpoint_color =
        g.splits[i] <= g.reference_splits[i] ? color(6, 15, 7) : color(15, 6, 4);
    rect(x - 1, 114, 3, 3, i < g.split_count ? checkpoint_color : white);
    if (i >= g.split_count)
      rect(x, 115, 1, 1, dark);
  }
  if (g.recovery_message > 0) {
    rect(12, 72, 96, 10, dark);
    centered(75, "RECOVERED +3 SEC", yellow);
  }
  if (g.impact > .4f) {
    rect(0, 0, W, 1, color(15, 5, 2));
    rect(0, 0, 1, H, color(15, 5, 2));
  }
  if (g.mode == Mode::Countdown) {
    rect(45, 40, 30, 30, dark);
    std::snprintf(buffer, sizeof(buffer), "%d", int(std::ceil(g.countdown)));
    centered(45, buffer, yellow, 4);
  }
  if (g.mode == Mode::Racing && g.elapsed < .8f) {
    rect(42, 42, 36, 19, dark);
    centered(46, "GO", yellow, 2);
  }
  if (g.mode == Mode::Paused) {
    rect(5, 36, 110, 61, dark);
    centered(42, "PAUSED", yellow, 2);
    std::snprintf(buffer, sizeof(buffer), "%s RESUME", pause);
    centered(57, buffer, white);
    std::snprintf(buffer, sizeof(buffer), "%s RETRY", confirm);
    centered(67, buffer, white);
    std::snprintf(buffer, sizeof(buffer), "%s SELECT", back);
    centered(77, buffer, white);
    std::snprintf(buffer, sizeof(buffer), "%s SOUND %s", audio, g.muted ? "OFF" : "ON");
    centered(88, buffer, yellow);
  }
  if (diagnostics) {
    rect(0, 22, 46, 15, dark);
    std::snprintf(buffer, sizeof(buffer), "%d FPS", fps);
    text(2, 24, buffer, white);
    std::snprintf(buffer, sizeof(buffer), "%d TRI", face_count);
    text(2, 31, buffer, yellow);
  }
}
} // namespace rally
