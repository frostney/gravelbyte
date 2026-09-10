#include "game.hpp"
#include <cmath>
#include <cstdio>
#include <memory>
int main() {
  using namespace GravelByte;
  auto SceneRenderer = std::make_unique<Renderer>();
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> CurrentRing{}, NextRing{};
  int Maximum = 0, OriginalFaces = 0, CulledFaces = 0;
  for (float Angle : {0.f, .4f, 1.2f, 2.7f})
    for (float Height : {1.f, 4.f, 10.f}) {
      SceneRenderer->Camera = {-std::sin(Angle) * 8, Height, -std::cos(Angle) * 8};
      SceneRenderer->CameraX = int(SceneRenderer->Camera.CoordinateX * 64);
      SceneRenderer->CameraY = int(Height * 64);
      SceneRenderer->CameraZ = int(SceneRenderer->Camera.CoordinateZ * 64);
      SceneRenderer->CameraSineFixed = int(std::sin(Angle) * 16384);
      SceneRenderer->CameraCosineFixed = int(std::cos(Angle) * 16384);
      for (int Pass = 0; Pass < 2; ++Pass) {
        SceneRenderer->FaceCount = 0;
        SceneRenderer->DepthBuffer.fill(0);
        ++SceneRenderer->RenderFrame;
        SceneRenderer->Pixels = Pass ? NextRing.data() : CurrentRing.data();
        std::fill(SceneRenderer->Pixels,
                  SceneRenderer->Pixels + FramebufferWidth * FramebufferHeight, 0);
        if (Pass)
          SceneRenderer->DrawSnowTree({0, 0, 0}, 7, 3);
        else
          SceneRenderer->DrawTree({0, 0, 0}, 7, 3);
        if (Pass)
          CulledFaces += SceneRenderer->FaceCount;
        else
          OriginalFaces += SceneRenderer->FaceCount;
        for (int Index = 0; Index < SceneRenderer->FaceCount; ++Index)
          SceneRenderer->RasterizeTriangle(SceneRenderer->Faces[Index]);
      }
      int Different = 0, Missing = 0;
      for (int Index = 0; Index < FramebufferWidth * FramebufferHeight; ++Index) {
        Different += CurrentRing[Index] != NextRing[Index];
        Missing += CurrentRing[Index] != 0 && NextRing[Index] == 0;
      }
      Maximum = std::max(Maximum, Different);
      std::printf("angle=%.1f height=%.0f different=%d missing=%d\n", Angle, Height, Different,
                  Missing);
      if (Missing)
        return 1;
    }
  std::printf("max_changed=%d faces=%d -> %d\n", Maximum, OriginalFaces, CulledFaces);
  if (CulledFaces >= OriginalFaces)
    return 1;
}
