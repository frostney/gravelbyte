#include "game.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
using namespace GravelByte;

static void Check(bool Passed, const char *Message) {
  if (!Passed) {
    std::fprintf(stderr, "FAIL: %s\n", Message);
    std::exit(1);
  }
}

int main() {
  auto SceneRenderer = std::make_unique<Renderer>();
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels{};
  int InteriorSamples = 0;
  int FrameCount = 0;
  // A coplanar surface must receive the same uninterrupted shadow regardless
  // of how road/grass materials subdivide it. Two banks also exercise a crease.
  for (float BankSlope : {-.12f, 0.f, .12f})
    for (float Heading : {0.f, .4f, 1.2f, 2.7f})
      for (bool Creased : {false, true})
        for (int FrameIndex = 0; FrameIndex < 80; ++FrameIndex) {
          auto RenderSurface = [&](bool Subdivided) {
            Pixels.fill(0);
            SceneRenderer->Pixels = Pixels.data();
            SceneRenderer->DepthBuffer.fill(0);
            SceneRenderer->FaceCount = SceneRenderer->ShadowCount = SceneRenderer->Dropped = 0;
            ++SceneRenderer->RenderFrame;
            SceneRenderer->CameraY = 256;
            SceneRenderer->CameraZ = -320;
            SceneRenderer->ShadowEnabled = true;
            SceneRenderer->ShadowCenter = {-.5f + FrameIndex * .025f, 0, 5};
            SceneRenderer->ShadowSine = std::sin(Heading);
            SceneRenderer->ShadowCosine = std::cos(Heading);
            SceneRenderer->ShadowWidth = 1;
            SceneRenderer->ShadowLength = 1.8f;
            SceneRenderer->ShadowMinimumX = -4;
            SceneRenderer->ShadowMaximumX = 4;
            SceneRenderer->ShadowMinimumZ = 1;
            SceneRenderer->ShadowMaximumZ = 9;
            const int StripCount = Subdivided ? 12 : Creased ? 2 : 1;
            for (int StripIndex = 0; StripIndex < StripCount; ++StripIndex) {
              const float Left = -6 + 12.f * StripIndex / StripCount;
              const float Right = -6 + 12.f * (StripIndex + 1) / StripCount;
              const float LeftHeight = (Creased ? std::abs(Left) : Left) * BankSlope;
              const float RightHeight = (Creased ? std::abs(Right) : Right) * BankSlope;
              const uint16_t SurfaceColor =
                  Subdivided && StripIndex % 2 ? MakeColor(9, 10, 8) : MakeColor(10, 10, 10);
              SceneRenderer->DrawGroundQuadrilateral({Left, LeftHeight, 1}, {Right, RightHeight, 1},
                                                     {Right, RightHeight, 20},
                                                     {Left, LeftHeight, 20}, SurfaceColor);
            }
            for (int FaceIndex = 0; FaceIndex < SceneRenderer->FaceCount; ++FaceIndex)
              SceneRenderer->RasterizeTriangle(SceneRenderer->Faces[FaceIndex]);
            Check(SceneRenderer->Dropped == 0, "surface transitions fit the geometry buffers");
            return Pixels;
          };
          const auto Reference = RenderSurface(false);
          const auto Subdivided = RenderSurface(true);
          ++FrameCount;
          for (int ScreenY = 2; ScreenY < FramebufferHeight - 2; ++ScreenY)
            for (int ScreenX = 2; ScreenX < FramebufferWidth - 2; ++ScreenX) {
              bool Interior = true;
              // Ignore the actual outer silhouette's one-pixel sampling edge.
              for (int VerticalOffset = -1; VerticalOffset <= 1; ++VerticalOffset)
                for (int HorizontalOffset = -1; HorizontalOffset <= 1; ++HorizontalOffset)
                  Interior &= Reference[(ScreenY + VerticalOffset) * FramebufferWidth + ScreenX +
                                        HorizontalOffset] == MakeColor(6, 6, 6);
              if (Interior) {
                ++InteriorSamples;
                const auto Pixel = Subdivided[ScreenY * FramebufferWidth + ScreenX];
                Check(Pixel == MakeColor(6, 6, 6) || Pixel == MakeColor(5, 6, 4),
                      "moving shadow has no bright seams across receiver/material boundaries");
              }
            }
        }
  Check(InteriorSamples > 10000, "the sweep covers visible shadow interiors");

  // Part of the shadow lies behind the near plane, but its visible portion
  // must remain. The former all-or-nothing mask rejection fails this scene.
  Pixels.fill(0);
  SceneRenderer->DepthBuffer.fill(0);
  SceneRenderer->FaceCount = SceneRenderer->ShadowCount = 0;
  ++SceneRenderer->RenderFrame;
  SceneRenderer->CameraY = 32;
  SceneRenderer->CameraZ = 0;
  SceneRenderer->ShadowCenter = {0, 0, 1};
  SceneRenderer->ShadowSine = 0;
  SceneRenderer->ShadowCosine = 1;
  SceneRenderer->ShadowMinimumZ = -1;
  SceneRenderer->DrawGroundQuadrilateral({-6, 0, -.5f}, {6, 0, -.5f}, {6, 0, 8}, {-6, 0, 8},
                                         MakeColor(10, 10, 10));
  Check(SceneRenderer->ShadowCount > 0, "near-plane crossing keeps visible shadow masks");
  for (int FaceIndex = 0; FaceIndex < SceneRenderer->FaceCount; ++FaceIndex)
    SceneRenderer->RasterizeTriangle(SceneRenderer->Faces[FaceIndex]);
  int ShadowPixels = 0;
  for (auto Pixel : Pixels)
    ShadowPixels += Pixel == MakeColor(6, 6, 6);
  Check(ShadowPixels > 10, "near-plane crossing retains visible shading");
  std::printf("PASS: %d moving shadow frames, %d interior samples, near-plane clipping\n",
              FrameCount, InteriorSamples);
}
