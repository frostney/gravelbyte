#include "game.hpp"
#include <cmath>
#include <cstdio>
#include <memory>
int main() {
  using namespace rally;
  auto r = std::make_unique<Renderer>();
  std::array<uint16_t, W * H> a{}, b{};
  int maximum = 0, original_faces = 0, culled_faces = 0;
  for (float angle : {0.f, .4f, 1.2f, 2.7f})
    for (float height : {1.f, 4.f, 10.f}) {
      r->camera = {-std::sin(angle) * 8, height, -std::cos(angle) * 8};
      r->camera_x = int(r->camera.x * 64);
      r->camera_y = int(height * 64);
      r->camera_z = int(r->camera.z * 64);
      r->sine = int(std::sin(angle) * 16384);
      r->cosine = int(std::cos(angle) * 16384);
      for (int pass = 0; pass < 2; ++pass) {
        r->face_count = 0;
        r->depth_buffer.fill(0);
        ++r->render_frame;
        r->pixels = pass ? b.data() : a.data();
        std::fill(r->pixels, r->pixels + W * H, 0);
        if (pass)
          r->snow_tree({0, 0, 0}, 7, 3);
        else
          r->tree({0, 0, 0}, 7, 3);
        if (pass)
          culled_faces += r->face_count;
        else
          original_faces += r->face_count;
        for (int i = 0; i < r->face_count; ++i)
          r->raster(r->faces[i]);
      }
      int different = 0, missing = 0;
      for (int i = 0; i < W * H; ++i) {
        different += a[i] != b[i];
        missing += a[i] != 0 && b[i] == 0;
      }
      maximum = std::max(maximum, different);
      std::printf("angle=%.1f height=%.0f different=%d missing=%d\n", angle, height, different,
                  missing);
      if (missing)
        return 1;
    }
  std::printf("max_changed=%d faces=%d -> %d\n", maximum, original_faces, culled_faces);
  if (culled_faces >= original_faces)
    return 1;
}
