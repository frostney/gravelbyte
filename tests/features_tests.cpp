#include "game.hpp"
#include <array>
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
  Game GameState;
  auto Original = GameState.Road;
  GameState.SelectCarAndTrack(1, 1);
  Check(std::abs(GameState.Road[100].Position.CoordinateX - Original[100].Position.CoordinateX) >
            20,
        "summer is a distinct course");
  GameState.SelectCarAndTrack(1, 2);
  Check(std::abs(GameState.Road[100].Position.CoordinateX - Original[100].Position.CoordinateX) >
            20,
        "winter is a distinct course");
  Check(GameState.Icy(85) && GameState.SurfaceGrip(85) < GameState.SurfaceGrip(55),
        "ice has local traction changes");
  GameState.SelectCarAndTrack(1, 0);
  for (int Index = 0; Index < NodeCount; ++Index)
    Check(GameState.Road[Index].Position.CoordinateX == Original[Index].Position.CoordinateX &&
              GameState.Road[Index].Position.CoordinateY == Original[Index].Position.CoordinateY,
          "returning to Bracken preserves original course");
  std::array<float, 3> Speeds{};
  for (int CarIndex = 0; CarIndex < CarCount; ++CarIndex) {
    GameState.SelectCarAndTrack(CarIndex, 0);
    DrivingInput Gas;
    Gas.Throttle = true;
    for (int Index = 0; Index < 100; ++Index)
      GameState.SimulatePhysics(.01f, Gas);
    Speeds[CarIndex] = GameState.Speed;
  }
  Check(Speeds[0] < Speeds[1] && Speeds[1] < Speeds[2],
        "acceleration bars match measured acceleration");
  for (int Index = 0; Index < RecordCount; ++Index)
    for (int Split = 0; Split < SectorCount; ++Split)
      GameState.Records[Index].Splits[Split] = float(20 * Split + 10 + Index);
  GameState.SelectCarAndTrack(2, 2);
  SaveData Save = EncodeSave(GameState);
  Game Restored;
  Check(LoadSave(Restored, Save), "all route and assist records load");
  Check(Restored.SelectedCar == 2 && Restored.SelectedTrack == 2, "selections persist");
  for (int Track = 0; Track < TrackCount; ++Track)
    for (int CarIndex = 0; CarIndex < CarCount; ++CarIndex) {
      Restored.SelectCarAndTrack(CarIndex, Track);
      Check(std::abs(Restored.Best - float(90 + Game::RecordIndex(Track, CarIndex, 0, false))) <
                .001f,
            "records isolated by car and track");
    }
  for (size_t Byte = 0; Byte < sizeof(Save); ++Byte) {
    auto Damaged = Save;
    reinterpret_cast<unsigned char *>(&Damaged)[Byte] ^= 1;
    Check(!LoadSave(Restored, Damaged), "every corrupted byte is rejected");
  }
  Game Legacy;
  LoadBest(Legacy, EncodeBest(92.f, {18, 36, 54, 74, 92}));
  Legacy.SelectCarAndTrack(0, 0);
  Check(Legacy.Best == 0, "legacy record is not assigned to easy car");
  Legacy.SelectCarAndTrack(1, 0);
  Check(std::abs(Legacy.Best - 92.f) < .001f, "legacy record stays with Standard Bracken");
  GameState.CurrentMode = GameMode::CarSelect;
  GameState.SelectedCar = 0;
  DrivingInput Right;
  Right.Right = true;
  GameState.Update(.02f, Right);
  GameState.Update(.02f, Right);
  Check(GameState.SelectedCar == 1, "holding select does not skip cars");
  GameState.Update(.02f, {});
  GameState.Update(.02f, Right);
  Check(GameState.SelectedCar == 2, "released select can advance again");
  auto SceneRenderer = std::make_unique<Renderer>();
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels{};
  // Partitioned shadow must still cover the ground and obey foreground depth.
  for (float Yaw : {0.f, .4f, 1.2f, 2.7f})
    for (float Bank : {-.12f, 0.f, .12f}) {
      SceneRenderer->Pixels = Pixels.data();
      Pixels.fill(0);
      SceneRenderer->DepthBuffer.fill(0);
      SceneRenderer->FaceCount = 0;
      SceneRenderer->ShadowCount = 0;
      SceneRenderer->Dropped = 0;
      ++SceneRenderer->RenderFrame;
      SceneRenderer->CameraX = 0;
      SceneRenderer->CameraY = 256;
      SceneRenderer->CameraZ = -320;
      SceneRenderer->CameraSineFixed = 0;
      SceneRenderer->CameraCosineFixed = 16384;
      SceneRenderer->ShadowEnabled = true;
      SceneRenderer->ShadowCenter = {0, 0, 5};
      SceneRenderer->ShadowSine = std::sin(Yaw);
      SceneRenderer->ShadowCosine = std::cos(Yaw);
      SceneRenderer->ShadowWidth = 1;
      SceneRenderer->ShadowLength = 1.8f;
      SceneRenderer->ShadowMinimumX = -3;
      SceneRenderer->ShadowMaximumX = 3;
      SceneRenderer->ShadowMinimumZ = 2;
      SceneRenderer->ShadowMaximumZ = 8;
      SceneRenderer->ShadowEnabled = false;
      SceneRenderer->DrawGroundQuadrilateral({-6, -6 * Bank, 1}, {6, 6 * Bank, 1},
                                             {6, 6 * Bank, 20}, {-6, -6 * Bank, 20},
                                             MakeColor(10, 10, 10));
      for (int Index = 0; Index < SceneRenderer->FaceCount; ++Index)
        SceneRenderer->RasterizeTriangle(SceneRenderer->Faces[Index]);
      const auto Unshadowed = Pixels;
      Pixels.fill(0);
      SceneRenderer->FaceCount = 0;
      SceneRenderer->DepthBuffer.fill(0);
      SceneRenderer->ShadowEnabled = true;
      SceneRenderer->DrawGroundQuadrilateral({-6, -6 * Bank, 1}, {6, 6 * Bank, 1},
                                             {6, 6 * Bank, 20}, {-6, -6 * Bank, 20},
                                             MakeColor(10, 10, 10));
      for (int Index = 0; Index < SceneRenderer->FaceCount; ++Index)
        SceneRenderer->RasterizeTriangle(SceneRenderer->Faces[Index]);
      int Dark = 0;
      for (auto Position : Pixels)
        if (Position && ((Position >> 12) < 8))
          ++Dark;
      Check(Dark > 15, "ground shadow remains visible on slopes at different car headings");
      for (int CoordinateY = 2; CoordinateY < FramebufferHeight - 2; ++CoordinateY)
        for (int CoordinateX = 2; CoordinateX < FramebufferWidth - 2; ++CoordinateX) {
          bool Interior = true;
          for (int VerticalStep = -2; VerticalStep <= 2; ++VerticalStep)
            for (int HorizontalStep = -2; HorizontalStep <= 2; ++HorizontalStep)
              Interior &= Unshadowed[(CoordinateY + VerticalStep) * FramebufferWidth + CoordinateX +
                                     HorizontalStep] != 0;
          if (Interior)
            Check(Pixels[CoordinateY * FramebufferWidth + CoordinateX] != 0,
                  "shadow material introduces no ground holes");
        }
      // A close foreground face must cover the shadow just as it covers the road.
      Renderer::Triangle Front{
          {0, 119, 60}, {119, 119, 0}, {50000, 50000, 50000}, MakeColor(15, 2, 2)};
      SceneRenderer->RasterizeTriangle(Front);
      Check(Pixels[80 * FramebufferWidth + 60] == Front.SurfaceColor,
            "foreground geometry occludes shadow");
    }
  std::puts("PASS: track identity, acceleration, nine records, corruption, migration, selection "
            "edges, ground shadow");
}
