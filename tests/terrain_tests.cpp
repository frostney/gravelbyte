#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

// An XZ barycentric query detects terrain folded over the drivable road,
// independently of the renderer's depth test or its camera projection.
static float triangle_height(rally::Vec p, rally::Vec a, rally::Vec b, rally::Vec c) {
  float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
  if (std::abs(d) < .001f)
    return -std::numeric_limits<float>::infinity();
  float u = ((b.z - c.z) * (p.x - c.x) + (c.x - b.x) * (p.z - c.z)) / d;
  float v = ((c.z - a.z) * (p.x - c.x) + (a.x - c.x) * (p.z - c.z)) / d;
  return u >= 0 && v >= 0 && u + v <= 1 ? u * a.y + v * b.y + (1 - u - v) * c.y
                                        : -std::numeric_limits<float>::infinity();
}
int main() {
  using namespace rally;
  Game g;
  int samples = 0;
  for (int track = 0; track < TrackCount; ++track) {
    g.select(1, track);
    for (int n = 1; n < NodeCount - 1; ++n)
      for (float t : {0.f, .25f, .5f, .75f})
        for (float fraction : {-.95f, -.5f, 0.f, .5f, .95f}) {
          Vec p = g.roadside(n, g.road[n].half_width * fraction) * (1 - t) +
                  g.roadside(n + 1, g.road[n + 1].half_width * fraction) * t;
          ++samples;
          for (int i = std::max(0, n - 5); i < std::min(NodeCount - 1, n + 31); ++i)
            for (int side = 0; side < 2; ++side) {
              int far = side ? 9 : 0, bank = side ? 8 : 1, verge = side ? 7 : 2;
              const auto &a = g.terrain[i];
              const auto &b = g.terrain[i + 1];
              float h = std::max({triangle_height(p, a[far], b[far], b[bank]),
                                  triangle_height(p, a[far], b[bank], a[bank]),
                                  triangle_height(p, a[bank], b[bank], b[verge]),
                                  triangle_height(p, a[bank], b[verge], a[verge])});
              if (h > p.y + .1f) {
                std::fprintf(stderr,
                             "Terrain strip %d intrudes %.2fm above road node %d (t=%.2f, width "
                             "fraction=%.2f)\n",
                             i, h - p.y, n, t, fraction);
                return 1;
              }
            }
        }
  }
  std::printf("PASS: %d road corridor samples clear of overlapping hillsides\n", samples);
}
