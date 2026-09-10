#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef GRAVELBYTE_PROFILE
#include "pico/time.h"
#endif

namespace Rally {
static uint32_t ProfileTimeMicroseconds() {
#ifdef GRAVELBYTE_PROFILE
  return time_us_32();
#else
  return 0;
#endif
}
Renderer::Renderer() {
  for (int Index = 0; Index < int(RidgeHeights.size()); ++Index)
    RidgeHeights[Index] =
        uint8_t(29 + int(3 * std::sin(Index * Tuning::FullTurnRadians * 6 / 512) +
                         2 * std::sin(Index * Tuning::FullTurnRadians * 14 / 512)));
}
uint16_t MakeColor(int RedComponent, int GreenComponent, int BlueComponent) {
  return uint16_t((RedComponent << 12) | (GreenComponent << 8) | (BlueComponent << 4) | 15);
}
static uint16_t ShadeColor(uint16_t ColorValue, int Amount) {
  return MakeColor(std::clamp((ColorValue >> 12) + Amount, 0, 15),
                   std::clamp(((ColorValue >> 8) & 15) + Amount, 0, 15),
                   std::clamp(((ColorValue >> 4) & 15) + Amount, 0, 15));
}
static uint16_t ApplyFog(uint16_t ColorValue, int Depth) {
  int Fraction = std::clamp((Depth - 35 * 64) * 3 / 2560, 0, 12);
  int RedComponent = ((ColorValue >> 12) * (16 - Fraction) + 9 * Fraction) >> 4;
  int GreenComponent = (((ColorValue >> 8) & 15) * (16 - Fraction) + 10 * Fraction) >> 4;
  int BlueComponent = (((ColorValue >> 4) & 15) * (16 - Fraction) + 10 * Fraction) >> 4;
  return MakeColor(RedComponent, GreenComponent, BlueComponent);
}
void Renderer::DrawRectangle(int CoordinateX, int CoordinateY, int Width, int Height,
                             uint16_t ColorValue) {
  for (int PositionY = std::max(0, CoordinateY);
       PositionY < std::min(FramebufferHeight, CoordinateY + Height); ++PositionY)
    for (int PositionX = std::max(0, CoordinateX);
         PositionX < std::min(FramebufferWidth, CoordinateX + Width); ++PositionX)
      Pixels[PositionY * FramebufferWidth + PositionX] = ColorValue;
}
// Three by five uppercase font. Bits run left-to-right, top-to-bottom.
static uint16_t Glyph(char Character) {
  static constexpr const char *Letters[] = {
      "010101111101101", "110101110101110", "011100100100011", "110101101101110", "111100110100111",
      "111100110100100", "011100101101011", "101101111101101", "111010010010111", "001001001101010",
      "101101110101101", "100100100100111", "101111111101101", "101111111111101", "010101101101010",
      "110101110100100", "010101101111011", "110101110101101", "011100010001110", "111010010010010",
      "101101101101111", "101101101101010", "101101111111101", "101101010101101", "101101010010010",
      "111001010100111"};
  static constexpr const char *Digits[] = {"111101101101111", "010110010010111", "110001111100111",
                                           "110001010001110", "101101111001001", "111100110001110",
                                           "011100111101111", "111001010010010", "111101111101111",
                                           "111101111001110"};
  if (Character == 's')
    return 0x071e;
  const char *Pattern = nullptr;
  if (Character >= 'A' && Character <= 'Z')
    Pattern = Letters[Character - 'A'];
  if (Character >= 'a' && Character <= 'z')
    Pattern = Letters[Character - 'a'];
  if (Character >= '0' && Character <= '9')
    Pattern = Digits[Character - '0'];
  if (Pattern) {
    uint16_t Bits = 0;
    for (int Index = 0; Index < 15; ++Index)
      Bits = uint16_t((Bits << 1) | (Pattern[Index] == '1'));
    return Bits;
  }
  if (Character == '<')
    return 0x1511;
  if (Character == '>')
    return 0x4454;
  if (Character == ':')
    return 0x0410;
  if (Character == '.')
    return 0x0002;
  if (Character == '-')
    return 0x01c0;
  if (Character == '+')
    return 0x05d0;
  if (Character == '/')
    return 0x12a4;
  if (Character == '!')
    return 0x2492;
  return 0;
}
void Renderer::DrawText(int CoordinateX, int CoordinateY, const char *Value, uint16_t ColorValue,
                        int Scale) {
  for (; *Value; ++Value, CoordinateX += 4 * Scale) {
    uint16_t Bits = Glyph(*Value);
    for (int Row = 0; Row < 5; ++Row)
      for (int ColumnIndex = 0; ColumnIndex < 3; ++ColumnIndex)
        if (Bits & (1u << (14 - Row * 3 - ColumnIndex)))
          DrawRectangle(CoordinateX + ColumnIndex * Scale, CoordinateY + Row * Scale, Scale, Scale,
                        ColorValue);
  }
}
void Renderer::DrawCenteredText(int CoordinateY, const char *TextValue, uint16_t ColorValue,
                                int Scale) {
  DrawText((FramebufferWidth - int(std::strlen(TextValue)) * 4 * Scale + Scale) / 2, CoordinateY,
           TextValue, ColorValue, Scale);
}
void Renderer::Project(CameraVertex &Vertex) {
  if (Vertex.CoordinateZ < Tuning::NearPlane)
    return;
  Vertex.ScreenX = int16_t(std::clamp<int32_t>(
      Tuning::CenterX + Tuning::FocalLength * Vertex.CoordinateX / Vertex.CoordinateZ, -2000,
      2000));
  Vertex.ScreenY = int16_t(std::clamp<int32_t>(
      ProjectionY - Tuning::FocalLength * Vertex.CoordinateY / Vertex.CoordinateZ, -2000, 2000));
  Vertex.InverseDepth = uint16_t(Tuning::DepthNumerator / Vertex.CoordinateZ);
}
Renderer::CameraVertex Renderer::Transform(Vector3 World) {
  // Q6 world positions and Q14 camera basis. The visible radius is <300m,
  // keeping the products comfortably inside signed 32-bit range.
  const int32_t WorldX = int32_t(World.CoordinateX * Tuning::WorldScale),
                WorldY = int32_t(World.CoordinateY * Tuning::WorldScale),
                WorldZ = int32_t(World.CoordinateZ * Tuning::WorldScale);
  uint32_t Hash =
      uint32_t(WorldX) * 73856093u ^ uint32_t(WorldY) * 19349663u ^ uint32_t(WorldZ) * 83492791u;
  auto &Cached = VertexCache[(Hash ^ (Hash >> 16)) & (Tuning::VertexCacheSize - 1)];
  if (Cached.Frame == RenderFrame && Cached.CoordinateX == WorldX && Cached.CoordinateY == WorldY &&
      Cached.CoordinateZ == WorldZ)
    return Cached.Transformed;
  int32_t CoordinateX = WorldX - CameraX, CoordinateY = WorldY - CameraY,
          CoordinateZ = WorldZ - CameraZ;
  const int32_t Forward =
      (CoordinateX * CameraSineFixed + CoordinateZ * CameraCosineFixed) >> Tuning::BasisShift;
  CameraVertex Vertex;
  Vertex.CoordinateX =
      (CoordinateX * CameraCosineFixed - CoordinateZ * CameraSineFixed) >> Tuning::BasisShift;
  Vertex.CoordinateY =
      (CoordinateY * PitchCosineFixed + Forward * PitchSineFixed) >> Tuning::BasisShift;
  Vertex.CoordinateZ =
      (Forward * PitchCosineFixed - CoordinateY * PitchSineFixed) >> Tuning::BasisShift;
  Project(Vertex);
  Cached = {WorldX, WorldY, WorldZ, RenderFrame, Vertex};
  return Vertex;
}
void Renderer::DrawTriangle(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                            uint16_t SurfaceColor) {
  CameraVertex InputVertices[] = {Transform(FirstVertex), Transform(SecondVertex),
                                  Transform(ThirdVertex)},
               Clipped[5];
  if (InputVertices[0].CoordinateZ > Tuning::FarPlane &&
      InputVertices[1].CoordinateZ > Tuning::FarPlane &&
      InputVertices[2].CoordinateZ > Tuning::FarPlane)
    return;
  // Clip against near plane before perspective divide (no road popping at camera).
  int Count = 0;
  for (int Index = 0; Index < 3; ++Index) {
    CameraVertex Position = InputVertices[Index], NextPosition = InputVertices[(Index + 1) % 3];
    bool CurrentInside = Position.CoordinateZ >= Tuning::NearPlane,
         NextInside = NextPosition.CoordinateZ >= Tuning::NearPlane;
    if (CurrentInside)
      Clipped[Count++] = Position;
    if (CurrentInside != NextInside) {
      CameraVertex Edge;
      Edge.CoordinateX =
          Position.CoordinateX + (NextPosition.CoordinateX - Position.CoordinateX) *
                                     (Tuning::NearPlane - Position.CoordinateZ) /
                                     (NextPosition.CoordinateZ - Position.CoordinateZ);
      Edge.CoordinateY =
          Position.CoordinateY + (NextPosition.CoordinateY - Position.CoordinateY) *
                                     (Tuning::NearPlane - Position.CoordinateZ) /
                                     (NextPosition.CoordinateZ - Position.CoordinateZ);
      Edge.CoordinateZ = Tuning::NearPlane;
      Project(Edge);
      Clipped[Count++] = Edge;
    }
  }
  for (int Index = 1; Index < Count - 1; ++Index) {
    if (FaceCount == int(Faces.size())) {
      ++Dropped;
      return;
    }
    CameraVertex Vertex[] = {Clipped[0], Clipped[Index], Clipped[Index + 1]};
    Triangle Face{};
    Face.SurfaceColor = ApplyFog(
        SurfaceColor, (Vertex[0].CoordinateZ + Vertex[1].CoordinateZ + Vertex[2].CoordinateZ) / 3);
    for (int OtherIndex = 0; OtherIndex < 3; ++OtherIndex) {
      Face.CoordinateX[OtherIndex] = Vertex[OtherIndex].ScreenX;
      Face.CoordinateY[OtherIndex] = Vertex[OtherIndex].ScreenY;
      Face.InverseDepth[OtherIndex] = Vertex[OtherIndex].InverseDepth;
    }
    if (std::max({Face.CoordinateX[0], Face.CoordinateX[1], Face.CoordinateX[2]}) < 0 ||
        std::min({Face.CoordinateX[0], Face.CoordinateX[1], Face.CoordinateX[2]}) >=
            FramebufferWidth ||
        std::max({Face.CoordinateY[0], Face.CoordinateY[1], Face.CoordinateY[2]}) < 0 ||
        std::min({Face.CoordinateY[0], Face.CoordinateY[1], Face.CoordinateY[2]}) >=
            FramebufferHeight)
      continue;
    Faces[FaceCount++] = Face;
  }
}
// Project the complete shadow onto this receiver's plane. The ground triangle
// supplies coverage and depth; clipping the mask to its edges would round those
// edges twice and expose bright seams between neighbouring receiver triangles.
void Renderer::DrawGroundTriangle(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                                  uint16_t SurfaceColor) {
  const int Start = FaceCount;
  DrawTriangle(FirstVertex, SecondVertex, ThirdVertex, SurfaceColor);
  if (Start == FaceCount || !ShadowEnabled ||
      std::max({FirstVertex.CoordinateX, SecondVertex.CoordinateX, ThirdVertex.CoordinateX}) <
          ShadowMinimumX ||
      std::min({FirstVertex.CoordinateX, SecondVertex.CoordinateX, ThirdVertex.CoordinateX}) >
          ShadowMaximumX ||
      std::max({FirstVertex.CoordinateZ, SecondVertex.CoordinateZ, ThirdVertex.CoordinateZ}) <
          ShadowMinimumZ ||
      std::min({FirstVertex.CoordinateZ, SecondVertex.CoordinateZ, ThirdVertex.CoordinateZ}) >
          ShadowMaximumZ)
    return;
  const Vector3 FirstEdge = SecondVertex - FirstVertex, SecondEdge = ThirdVertex - FirstVertex;
  const float Determinant = FirstEdge.CoordinateX * SecondEdge.CoordinateZ -
                            FirstEdge.CoordinateZ * SecondEdge.CoordinateX;
  if (std::abs(Determinant) < .00001f)
    return; // A vertical or degenerate face cannot receive a downward shadow.
  const float HeightSlopeX = (FirstEdge.CoordinateY * SecondEdge.CoordinateZ -
                              FirstEdge.CoordinateZ * SecondEdge.CoordinateY) /
                             Determinant;
  const float HeightSlopeZ = (FirstEdge.CoordinateX * SecondEdge.CoordinateY -
                              FirstEdge.CoordinateY * SecondEdge.CoordinateX) /
                             Determinant;
  CameraVertex Corners[4];
  for (int CornerIndex = 0; CornerIndex < 4; ++CornerIndex) {
    const float LocalX = (CornerIndex == 0 || CornerIndex == 3) ? -ShadowWidth : ShadowWidth;
    const float LocalZ = CornerIndex < 2 ? -ShadowLength : ShadowLength;
    Vector3 Position = ShadowCenter + Vector3{LocalX * ShadowCosine + LocalZ * ShadowSine, 0,
                                              LocalZ * ShadowCosine - LocalX * ShadowSine};
    Position.CoordinateY = FirstVertex.CoordinateY +
                           (Position.CoordinateX - FirstVertex.CoordinateX) * HeightSlopeX +
                           (Position.CoordinateZ - FirstVertex.CoordinateZ) * HeightSlopeZ;
    Corners[CornerIndex] = Transform(Position);
  }
  // Preserve visible mask coverage when only part of it crosses the near plane.
  CameraVertex ClippedCorners[8];
  int ClippedCount = 0;
  for (int CornerIndex = 0; CornerIndex < 4; ++CornerIndex) {
    const auto Current = Corners[CornerIndex], Next = Corners[(CornerIndex + 1) % 4];
    const bool CurrentInside = Current.CoordinateZ >= Tuning::NearPlane,
               NextInside = Next.CoordinateZ >= Tuning::NearPlane;
    if (CurrentInside)
      ClippedCorners[ClippedCount++] = Current;
    if (CurrentInside != NextInside) {
      CameraVertex Intersection;
      Intersection.CoordinateX =
          Current.CoordinateX + (Next.CoordinateX - Current.CoordinateX) *
                                    (Tuning::NearPlane - Current.CoordinateZ) /
                                    (Next.CoordinateZ - Current.CoordinateZ);
      Intersection.CoordinateY =
          Current.CoordinateY + (Next.CoordinateY - Current.CoordinateY) *
                                    (Tuning::NearPlane - Current.CoordinateZ) /
                                    (Next.CoordinateZ - Current.CoordinateZ);
      Intersection.CoordinateZ = Tuning::NearPlane;
      Project(Intersection);
      ClippedCorners[ClippedCount++] = Intersection;
    }
  }
  if (ClippedCount < 3)
    return;
  if (ShadowCount == int(ShadowPolygons.size())) {
    ++Dropped;
    return;
  }
  ShadowPolygon Mask;
  Mask.Count = ClippedCount;
  Mask.Left = Mask.Top = 2000;
  Mask.Right = Mask.Bottom = -2000;
  for (int CornerIndex = 0; CornerIndex < ClippedCount; ++CornerIndex) {
    const auto &Corner = ClippedCorners[CornerIndex];
    Mask.CoordinateX[CornerIndex] = Corner.ScreenX;
    Mask.CoordinateY[CornerIndex] = Corner.ScreenY;
    Mask.Left = std::min(Mask.Left, int(Corner.ScreenX));
    Mask.Right = std::max(Mask.Right, int(Corner.ScreenX));
    Mask.Top = std::min(Mask.Top, int(Corner.ScreenY));
    Mask.Bottom = std::max(Mask.Bottom, int(Corner.ScreenY));
  }
  ShadowPolygons[ShadowCount++] = Mask;
  for (int FaceIndex = Start; FaceIndex < FaceCount; ++FaceIndex)
    Faces[FaceIndex].Shadow = uint16_t(ShadowCount);
}
void Renderer::DrawGroundQuadrilateral(Vector3 FirstVertex, Vector3 SecondVertex,
                                       Vector3 ThirdVertex, Vector3 FourthVertex,
                                       uint16_t SurfaceColor) {
  DrawGroundTriangle(FirstVertex, SecondVertex, ThirdVertex, SurfaceColor);
  DrawGroundTriangle(FirstVertex, ThirdVertex, FourthVertex, SurfaceColor);
}
void Renderer::DrawQuadrilateral(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                                 Vector3 FourthVertex, uint16_t SurfaceColor) {
  DrawTriangle(FirstVertex, SecondVertex, ThirdVertex, SurfaceColor);
  DrawTriangle(FirstVertex, ThirdVertex, FourthVertex, SurfaceColor);
}
void Renderer::DrawBox(Vector3 Position, Vector3 Size, float Yaw, uint16_t SurfaceColor) {
  Vector3 Vertex[8];
  float RotationSine = Yaw == 0 ? 0 : std::sin(Yaw), RotationCosine = Yaw == 0 ? 1 : std::cos(Yaw);
  for (int Index = 0; Index < 8; ++Index) {
    float CoordinateX = (Index & 1 ? 1 : -1) * Size.CoordinateX * .5f,
          CoordinateZ = (Index & 2 ? 1 : -1) * Size.CoordinateZ * .5f;
    Vertex[Index] = Position + Vector3{CoordinateX * RotationCosine + CoordinateZ * RotationSine,
                                       (Index & 4) ? Size.CoordinateY : 0,
                                       CoordinateZ * RotationCosine - CoordinateX * RotationSine};
  }
  const Vector3 Relative = Camera - Position;
  const float LocalX = Relative.CoordinateX * RotationCosine - Relative.CoordinateZ * RotationSine,
              LocalZ = Relative.CoordinateX * RotationSine + Relative.CoordinateZ * RotationCosine;
  if (LocalZ < -Size.CoordinateZ * .5f)
    DrawQuadrilateral(Vertex[0], Vertex[1], Vertex[5], Vertex[4], ShadeColor(SurfaceColor, -2));
  if (LocalZ > Size.CoordinateZ * .5f)
    DrawQuadrilateral(Vertex[2], Vertex[3], Vertex[7], Vertex[6], SurfaceColor);
  if (LocalX < -Size.CoordinateX * .5f)
    DrawQuadrilateral(Vertex[0], Vertex[2], Vertex[6], Vertex[4], ShadeColor(SurfaceColor, -1));
  if (LocalX > Size.CoordinateX * .5f)
    DrawQuadrilateral(Vertex[1], Vertex[3], Vertex[7], Vertex[5], SurfaceColor);
  if (Relative.CoordinateY > Size.CoordinateY)
    DrawQuadrilateral(Vertex[4], Vertex[5], Vertex[7], Vertex[6], ShadeColor(SurfaceColor, 1));
}
void Renderer::DrawTree(Vector3 Position, float Height, int Seed) {
  DrawBox(Position, {.36f, Height * .45f, .36f}, 0, MakeColor(4, 4, 3));
  for (int Tier = 0; Tier < 2; ++Tier) {
    float CoordinateY = Height * (Tier ? .38f : .16f), RedComponent = Height * (Tier ? .24f : .33f);
    Vector3 Tip = Position + Vector3{0, Height * (Tier ? 1.f : .77f), 0};
    Vector3 FirstVertex = Position + Vector3{-RedComponent, CoordinateY, -RedComponent},
            SecondVertex = Position + Vector3{RedComponent, CoordinateY, -RedComponent},
            ThirdVertex = Position + Vector3{RedComponent, CoordinateY, RedComponent},
            FourthVertex = Position + Vector3{-RedComponent, CoordinateY, RedComponent};
    uint16_t SurfaceColor = MakeColor(2 + Seed % 2, 4 + Seed % 3, 3 + Seed % 2);
    DrawTriangle(FirstVertex, SecondVertex, Tip, ShadeColor(SurfaceColor, -1));
    DrawTriangle(SecondVertex, ThirdVertex, Tip, SurfaceColor);
    DrawTriangle(ThirdVertex, FourthVertex, Tip, ShadeColor(SurfaceColor, 1));
    DrawTriangle(FourthVertex, FirstVertex, Tip, SurfaceColor);
  }
}
void Renderer::DrawPalmTree(Vector3 Position, float Height, int Seed) {
  // A leaning trunk and broad, drooping fronds give a readable palm silhouette
  // at 120 pixels. Fixed world-space leaves work in every cinematic view.
  const float Lean = (Seed % 2 ? 1.f : -1.f) * Tuning::PalmLean;
  const Vector3 Crown = Position + Vector3{Lean, Height, .35f};
  const Vector3 Trunk = {Tuning::PalmTrunkWidth, 0, 0};
  const Vector3 Depth = {0, 0, Tuning::PalmTrunkWidth};
  DrawQuadrilateral(Position - Trunk, Position + Trunk, Crown + Trunk, Crown - Trunk,
                    MakeColor(8, 6, 3));
  DrawQuadrilateral(Position - Depth, Position + Depth, Crown + Depth, Crown - Depth,
                    MakeColor(6, 4, 2));
  const Vector3 Directions[] = {{1, 0, .3f}, {-.3f, 0, 1}, {-1, 0, -.3f}, {.3f, 0, -1}};
  for (int Leaf = 0; Leaf < 4; ++Leaf) {
    Vector3 Along = Directions[Leaf], Across = {-Along.CoordinateZ, 0, Along.CoordinateX};
    Vector3 Midpoint = Crown + Along * (Tuning::PalmCrownRadius * .5f) + Vector3{0, .35f, 0};
    Vector3 Tip = Crown + Along * Tuning::PalmCrownRadius - Vector3{0, 1.2f, 0};
    Vector3 Left = Midpoint - Across * .65f, Right = Midpoint + Across * .65f;
    DrawTriangle(Crown, Left, Tip, MakeColor(3, 8 + Leaf % 2, 3));
    DrawTriangle(Crown, Tip, Right, MakeColor(2, 6 + Leaf % 2, 2));
  }
}
void Renderer::DrawSnowTree(Vector3 Position, float Height, int Seed) {
  DrawBox(Position, {.36f, Height * .45f, .36f}, 0, MakeColor(4, 4, 3));
  for (int Tier = 0; Tier < 2; ++Tier) {
    float CoordinateY = Height * (Tier ? .38f : .16f), RedComponent = Height * (Tier ? .24f : .33f);
    Vector3 Tip = Position + Vector3{0, Height * (Tier ? 1.f : .77f), 0};
    Vector3 FirstVertex = Position + Vector3{-RedComponent, CoordinateY, -RedComponent},
            SecondVertex = Position + Vector3{RedComponent, CoordinateY, -RedComponent},
            ThirdVertex = Position + Vector3{RedComponent, CoordinateY, RedComponent},
            FourthVertex = Position + Vector3{-RedComponent, CoordinateY, RedComponent};
    uint16_t SurfaceColor = MakeColor(2 + Seed % 2, 4 + Seed % 3, 3 + Seed % 2);
    // Close the underside when viewed from below a branch tier. The original
    // double-sided open pyramid used its back faces to cover this silhouette.
    if (Camera.CoordinateY < Position.CoordinateY + CoordinateY)
      DrawQuadrilateral(FirstVertex, SecondVertex, ThirdVertex, FourthVertex,
                        ShadeColor(SurfaceColor, -2));
    // Exact pyramid face normals: retain every camera-facing face, including
    // all four when looking down from above. Hidden faces cannot affect colour.
    const float Rise = Tip.CoordinateY - (Position.CoordinateY + CoordinateY),
                Threshold = (Tip.CoordinateY - Camera.CoordinateY) * RedComponent;
    const float CoordinateX = (Camera.CoordinateX - Position.CoordinateX) * Rise,
                CoordinateZ = (Camera.CoordinateZ - Position.CoordinateZ) * Rise;
    if (-CoordinateZ >= Threshold)
      DrawTriangle(FirstVertex, SecondVertex, Tip, ShadeColor(SurfaceColor, -1));
    if (CoordinateX >= Threshold)
      DrawTriangle(SecondVertex, ThirdVertex, Tip, SurfaceColor);
    if (CoordinateZ >= Threshold)
      DrawTriangle(ThirdVertex, FourthVertex, Tip, ShadeColor(SurfaceColor, 1));
    if (-CoordinateX >= Threshold)
      DrawTriangle(FourthVertex, FirstVertex, Tip, SurfaceColor);
  }
}
// At distance the two branch tiers occupy only a handful of pixels. Keep that
// silhouette and the snow cap with camera-facing world geometry, avoiding the
// hidden volume work. Nearby conifers retain the full 3D model.
void Renderer::DrawDistantSnowTree(Vector3 Position, float Height, int Seed, bool Snow) {
  const Vector3 Right = {CameraCosine, 0, -CameraSine};
  const float Breadth = std::abs(CameraCosine) + std::abs(CameraSine);
  const uint16_t Leaves = MakeColor(2 + Seed % 2, 4 + Seed % 3, 3 + Seed % 2);
  DrawQuadrilateral(Position - Right * .18f, Position + Right * .18f,
                    Position + Right * .18f + Vector3{0, Height * .45f, 0},
                    Position - Right * .18f + Vector3{0, Height * .45f, 0}, MakeColor(4, 4, 3));
  DrawTriangle(Position - Right * (Height * .33f * Breadth) + Vector3{0, Height * .16f, 0},
               Position + Right * (Height * .33f * Breadth) + Vector3{0, Height * .16f, 0},
               Position + Vector3{0, Height * .77f, 0}, Leaves);
  Vector3 Left = Position - Right * (Height * .24f * Breadth) + Vector3{0, Height * .38f, 0},
          Edge = Position + Right * (Height * .24f * Breadth) + Vector3{0, Height * .38f, 0},
          Tip = Position + Vector3{0, Height, 0};
  Vector3 SnowLeft = Left + (Tip - Left) * .7f, SnowRight = Edge + (Tip - Edge) * .7f;
  DrawQuadrilateral(Left, Edge, SnowRight, SnowLeft, Leaves);
  DrawTriangle(SnowLeft, SnowRight, Tip, Snow ? MakeColor(14, 15, 15) : Leaves);
}
void Renderer::RasterizeTriangle(const Triangle &Face) {
  const ShadowPolygon *Shadow = Face.Shadow ? &ShadowPolygons[Face.Shadow - 1] : nullptr;
  const uint16_t ShadowColor = Shadow ? ShadeColor(Face.SurfaceColor, -4) : Face.SurfaceColor;
  // Walk the two edges incrementally: divisions happen once per edge, rather
  // than on every scanline. Screen x uses Q16; reciprocal depth uses Q8.
  int Order[] = {0, 1, 2};
  if (Face.CoordinateY[Order[0]] > Face.CoordinateY[Order[1]])
    std::swap(Order[0], Order[1]);
  if (Face.CoordinateY[Order[1]] > Face.CoordinateY[Order[2]])
    std::swap(Order[1], Order[2]);
  if (Face.CoordinateY[Order[0]] > Face.CoordinateY[Order[1]])
    std::swap(Order[0], Order[1]);
  const int Top = Order[0], Middle = Order[1], Bottom = Order[2];
  if (Face.CoordinateY[Top] == Face.CoordinateY[Bottom])
    return;
  const int LongHeight = Face.CoordinateY[Bottom] - Face.CoordinateY[Top];
  const int LongEdgeHorizontalStep =
      (int(Face.CoordinateX[Bottom]) - Face.CoordinateX[Top]) * 65536 / LongHeight;
  const int LongEdgeDepthStep =
      (int(Face.InverseDepth[Bottom]) - Face.InverseDepth[Top]) * 256 / LongHeight;
  for (int Half = 0; Half < 2; ++Half) {
    int EdgeStartIndex = Half ? Middle : Top, EdgeEndIndex = Half ? Bottom : Middle;
    int Height = Face.CoordinateY[EdgeEndIndex] - Face.CoordinateY[EdgeStartIndex];
    if (Height == 0)
      continue;
    int First = std::max(0, int(Face.CoordinateY[EdgeStartIndex])),
        Last = std::min(FramebufferHeight, int(Face.CoordinateY[EdgeEndIndex]));
    if (First >= Last)
      continue;
    int HorizontalStep =
        (int(Face.CoordinateX[EdgeEndIndex]) - Face.CoordinateX[EdgeStartIndex]) * 65536 / Height;
    int DepthStep =
        (int(Face.InverseDepth[EdgeEndIndex]) - Face.InverseDepth[EdgeStartIndex]) * 256 / Height;
    int FirstX = int(Face.CoordinateX[EdgeStartIndex]) * 65536 +
                 (First - Face.CoordinateY[EdgeStartIndex]) * HorizontalStep;
    int FirstDepth = int(Face.InverseDepth[EdgeStartIndex]) * 256 +
                     (First - Face.CoordinateY[EdgeStartIndex]) * DepthStep;
    int SecondX = int(Face.CoordinateX[Top]) * 65536 +
                  (First - Face.CoordinateY[Top]) * LongEdgeHorizontalStep;
    int SecondDepth =
        int(Face.InverseDepth[Top]) * 256 + (First - Face.CoordinateY[Top]) * LongEdgeDepthStep;
    for (int CoordinateY = First; CoordinateY < Last; ++CoordinateY, FirstX += HorizontalStep,
             FirstDepth += DepthStep, SecondX += LongEdgeHorizontalStep,
             SecondDepth += LongEdgeDepthStep) {
      int LowerBound = FirstX >> 16, UpperBound = SecondX >> 16, LeftDepth = FirstDepth,
          RightDepth = SecondDepth;
      if (LowerBound > UpperBound) {
        std::swap(LowerBound, UpperBound);
        std::swap(LeftDepth, RightDepth);
      }
      int Left = std::max(0, LowerBound), Right = std::min(FramebufferWidth - 1, UpperBound);
      if (Left > Right)
        continue;
      int Step = UpperBound > LowerBound ? (RightDepth - LeftDepth) / (UpperBound - LowerBound) : 0,
          CoordinateZ = LeftDepth + (Left - LowerBound) * Step;
      if (!Shadow || CoordinateY < Shadow->Top || CoordinateY > Shadow->Bottom) {
        for (int CoordinateX = Left; CoordinateX <= Right; ++CoordinateX, CoordinateZ += Step) {
          const int Offset = CoordinateY * FramebufferWidth + CoordinateX;
          if ((CoordinateZ >> 8) >= DepthBuffer[Offset]) {
            DepthBuffer[Offset] = uint16_t(CoordinateZ >> 8);
            Pixels[Offset] = Face.SurfaceColor;
          }
        }
      } else {
        // Intersect the convex material mask once per scanline, not per pixel.
        int MaskLeft = FramebufferWidth, MaskRight = -1;
        for (int Index = 0, OtherIndex = Shadow->Count - 1; Index < Shadow->Count;
             OtherIndex = Index++) {
          const int StartY = Shadow->CoordinateY[OtherIndex], EndY = Shadow->CoordinateY[Index],
                    StartX = Shadow->CoordinateX[OtherIndex], FirstX = Shadow->CoordinateX[Index];
          if (CoordinateY < std::min(StartY, EndY) || CoordinateY > std::max(StartY, EndY))
            continue;
          if (StartY == EndY) {
            MaskLeft = std::min(MaskLeft, std::min(StartX, FirstX));
            MaskRight = std::max(MaskRight, std::max(StartX, FirstX));
          } else {
            int CoordinateX = StartX + (FirstX - StartX) * (CoordinateY - StartY) / (EndY - StartY);
            MaskLeft = std::min(MaskLeft, CoordinateX);
            MaskRight = std::max(MaskRight, CoordinateX);
          }
        }
        for (int CoordinateX = Left; CoordinateX <= Right; ++CoordinateX, CoordinateZ += Step) {
          const int Offset = CoordinateY * FramebufferWidth + CoordinateX;
          if ((CoordinateZ >> 8) >= DepthBuffer[Offset]) {
            DepthBuffer[Offset] = uint16_t(CoordinateZ >> 8);
            Pixels[Offset] = (CoordinateX >= MaskLeft && CoordinateX <= MaskRight)
                                 ? ShadowColor
                                 : Face.SurfaceColor;
          }
        }
      }
    }
  }
}
static void TimeText(char *Out, size_t Size, float Seconds) {
  int Centiseconds = int(Seconds * 100);
  std::snprintf(Out, Size, "%02d:%02d.%02d", Centiseconds / 6000, (Centiseconds / 100) % 60,
                Centiseconds % 100);
}
void Renderer::Render(const Game &GameState, uint16_t *Target, int FramesPerSecond,
                      bool Diagnostics) {
  const uint32_t GeometryStart = ProfileTimeMicroseconds();
  Pixels = Target;
  FaceCount = ShadowCount = Dropped = 0;
  DepthBuffer.fill(0);
  const float ViewYaw = PrepareCamera(GameState);
  PrepareShadow(GameState);
  RenderBackground(GameState, ViewYaw);
  if (GameState.CurrentMode != GameMode::CarSelect)
    RenderRoad(GameState, ViewYaw);
  RenderCar(GameState);
  GeometryMicroseconds = ProfileTimeMicroseconds() - GeometryStart;
  const uint32_t RasterStart = ProfileTimeMicroseconds();
  for (int Index = 0; Index < FaceCount; ++Index)
    RasterizeTriangle(Faces[Index]);
  RasterMicroseconds = ProfileTimeMicroseconds() - RasterStart;
  RenderInterface(GameState, FramesPerSecond, Diagnostics);
}
float Renderer::PrepareCamera(const Game &GameState) {
  const bool Showroom = GameState.CurrentMode == GameMode::CarSelect;
  ProjectionY = Showroom ? Tuning::ShowroomCenterY : Tuning::CenterY;
  const bool Cinematic =
      GameState.CurrentMode == GameMode::Title || GameState.CurrentMode == GameMode::Finished;
  float ViewYaw = GameState.CameraYaw;
  CameraSine = std::sin(ViewYaw);
  CameraCosine = std::cos(ViewYaw);
  float CameraDistance = Showroom ? Tuning::ShowroomDistance : Tuning::ChaseDistance;
  float Height = Showroom ? Tuning::ShowroomHeight : Tuning::ChaseHeight;
  Camera = GameState.CarPosition +
           Vector3{-CameraSine * CameraDistance, Height, -CameraCosine * CameraDistance};
  Camera.CoordinateY = GameState.CameraHeight + Height;
  PitchSineFixed = Tuning::ChasePitchSine;
  PitchCosineFixed = Tuning::ChasePitchCosine;
  if (Cinematic) {
    // Planned road-relative shots keep the camera clear of tunnel roofs/walls.
    bool Portal = GameState.SelectedTrack == 2 &&
                  GameState.Segment >= Tuning::TunnelStart - Tuning::PortalCameraMargin &&
                  GameState.Segment <= Tuning::TunnelEnd + Tuning::PortalCameraMargin;
    int Shot =
        Portal ? 0 : int(GameState.CinematicTime / Tuning::ShotSeconds) % Tuning::CameraShotCount;
    if (Shot == 1) {
      int NodeIndex = std::clamp(GameState.Segment + Tuning::RoadsideLookAhead, 0, NodeCount - 1);
      const int Next = std::min(NodeCount - 1, NodeIndex + 1);
      Vector3 Start = GameState.Roadside(
          NodeIndex, -(GameState.Road[NodeIndex].HalfWidth + Tuning::RoadsideOffset));
      Vector3 End =
          GameState.Roadside(Next, -(GameState.Road[Next].HalfWidth + Tuning::RoadsideOffset));
      Camera =
          Start + (End - Start) * GameState.SegmentFraction + Vector3{0, Tuning::RoadsideHeight, 0};
      Camera.CoordinateY = std::max(Camera.CoordinateY, GameState.CarPosition.CoordinateY +
                                                            Tuning::RoadsideMinimumHeight);
    } else if (Shot == 2) {
      Camera = GameState.CarPosition +
               Vector3{-CameraSine * Tuning::HighShotBack + CameraCosine * Tuning::HighShotOffset,
                       Tuning::HighShotHeight,
                       -CameraCosine * Tuning::HighShotBack - CameraSine * Tuning::HighShotOffset};
    }
    Vector3 Aim = GameState.CarPosition + Vector3{0, Tuning::CarAimHeight, 0} - Camera;
    ViewYaw = std::atan2(Aim.CoordinateX, Aim.CoordinateZ);
    float Horizontal =
        std::sqrt(Aim.CoordinateX * Aim.CoordinateX + Aim.CoordinateZ * Aim.CoordinateZ);
    float PitchAngle = std::atan2(-Aim.CoordinateY, Horizontal);
    PitchSineFixed = int32_t(std::sin(PitchAngle) * Tuning::BasisScale);
    PitchCosineFixed = int32_t(std::cos(PitchAngle) * Tuning::BasisScale);
    CameraSine = std::sin(ViewYaw);
    CameraCosine = std::cos(ViewYaw);
  }
  CameraX = int32_t(Camera.CoordinateX * Tuning::WorldScale);
  CameraY = int32_t(Camera.CoordinateY * Tuning::WorldScale);
  CameraZ = int32_t(Camera.CoordinateZ * Tuning::WorldScale);
  CameraSineFixed = int32_t(CameraSine * Tuning::BasisScale);
  CameraCosineFixed = int32_t(CameraCosine * Tuning::BasisScale);
  if (++RenderFrame == 0) {
    for (auto &Entry : VertexCache)
      Entry.Frame = 0;
    RenderFrame = 1;
  }
  return ViewYaw;
}
void Renderer::PrepareShadow(const Game &GameState) {
  const bool Showroom = GameState.CurrentMode == GameMode::CarSelect;
  ShadowCenter = GameState.CarPosition;
  ShadowSine = std::sin(Showroom ? GameState.MenuRotation : GameState.Yaw);
  ShadowCosine = std::cos(Showroom ? GameState.MenuRotation : GameState.Yaw);
  ShadowWidth = 1.05f * GameState.GetCarSpecification().Width;
  ShadowLength = 1.8f * GameState.GetCarSpecification().Length;
  const float ExtentX = std::abs(ShadowCosine) * ShadowWidth + std::abs(ShadowSine) * ShadowLength;
  const float ExtentZ = std::abs(ShadowSine) * ShadowWidth + std::abs(ShadowCosine) * ShadowLength;
  ShadowMinimumX = GameState.CarPosition.CoordinateX - ExtentX;
  ShadowMaximumX = GameState.CarPosition.CoordinateX + ExtentX;
  ShadowMinimumZ = GameState.CarPosition.CoordinateZ - ExtentZ;
  ShadowMaximumZ = GameState.CarPosition.CoordinateZ + ExtentZ;
}
void Renderer::RenderBackground(const Game &GameState, float ViewYaw) {
  const bool Showroom = GameState.CurrentMode == GameMode::CarSelect;
  // Fog-coloured sky, distant wooded ridge; foreground is real world geometry.
  DrawRectangle(0, 0, FramebufferWidth, FramebufferHeight, MakeColor(9, 10, 10));
  DrawRectangle(0, 0, FramebufferWidth, 24, MakeColor(10, 11, 12));
  for (int CoordinateX = 0; CoordinateX < FramebufferWidth; ++CoordinateX) {
    int Phase = CoordinateX + int(ViewYaw * 28);
    int Ridge = RidgeHeights[unsigned(Phase) & (Tuning::RidgeSamples - 1)];
    DrawRectangle(CoordinateX, Ridge - 5, 1, 29, MakeColor(8, 9, 10));
    DrawRectangle(CoordinateX,
                  Ridge + 4 +
                      (RidgeHeights[(unsigned(Phase) + 143) & (Tuning::RidgeSamples - 1)] - 29) * 2,
                  1, 28, MakeColor(7, 8, 8));
  }
  DrawRectangle(0, 52, FramebufferWidth, FramebufferHeight - 52, MakeColor(6, 7, 5));
  if (GameState.SelectedTrack == 1) {
    DrawRectangle(0, 0, FramebufferWidth, 25, MakeColor(9, 12, 13));
    DrawRectangle(0, 25, FramebufferWidth, 27, MakeColor(8, 12, 14));
    DrawRectangle(0, 43, FramebufferWidth, 9, MakeColor(3, 9, 12));
    DrawRectangle(0, 52, FramebufferWidth, FramebufferHeight - 52, MakeColor(12, 11, 7));
  }
  if (GameState.SelectedTrack == 2) {
    DrawRectangle(0, 0, FramebufferWidth, 25, MakeColor(11, 12, 14));
    for (int CoordinateX = 0; CoordinateX < FramebufferWidth; ++CoordinateX) {
      int Phase = (CoordinateX + int(ViewYaw * 38)) % 48;
      if (Phase < 0)
        Phase += 48;
      int Peak = 15 + std::abs(Phase - 24);
      DrawRectangle(CoordinateX, Peak, 1, 53 - Peak, MakeColor(7, 9, 12));
      DrawRectangle(CoordinateX, Peak, 1, std::max(1, (39 - Peak) / 3), MakeColor(14, 15, 15));
    }
    DrawRectangle(0, 52, FramebufferWidth, FramebufferHeight - 52, MakeColor(12, 13, 14));
  }
  if (Showroom) {
    DrawRectangle(0, 0, FramebufferWidth, FramebufferHeight, MakeColor(1, 2, 3));
    DrawRectangle(0, 74, FramebufferWidth, 46, MakeColor(2, 3, 4));
  }
}
void Renderer::RenderMountain(const Game &GameState, int First, int Last) {
  if (GameState.SelectedTrack == 2 && Last >= Tuning::TunnelStart && First <= Tuning::TunnelEnd) {
    // Broad, cached rock/snow panels enclose the detailed tunnel interior.
    for (int SampleIndex = 0; SampleIndex < Tuning::MountainSections; ++SampleIndex) {
      const auto &CurrentRing = GameState.Mountain[SampleIndex],
                 &NextRing = GameState.Mountain[SampleIndex + 1];
      DrawQuadrilateral(CurrentRing[0], NextRing[0], NextRing[1], CurrentRing[1],
                        MakeColor(7, 8, 10));
      DrawQuadrilateral(CurrentRing[1], NextRing[1], NextRing[2], CurrentRing[2],
                        MakeColor(11, 13, 14));
      DrawQuadrilateral(CurrentRing[2], NextRing[2], NextRing[3], CurrentRing[3],
                        MakeColor(14, 15, 15));
      DrawQuadrilateral(CurrentRing[3], NextRing[3], NextRing[4], CurrentRing[4],
                        MakeColor(7, 8, 10));
    }
    for (int SampleIndex : {0, Tuning::MountainSections}) {
      int NodeIndex = SampleIndex == 0 ? Tuning::TunnelStart : Tuning::TunnelEnd;
      const auto &Ring = GameState.Mountain[SampleIndex];
      for (int Sign : {-1, 1}) {
        Vector3 Bottom = GameState.Roadside(
            NodeIndex, Sign * (GameState.Road[NodeIndex].HalfWidth + Tuning::RailMargin));
        Vector3 Top = Bottom + Vector3{0, Tuning::TunnelHeight, 0};
        Vector3 Outer = Ring[Sign < 0 ? 0 : 4], Shoulder = Ring[Sign < 0 ? 1 : 3];
        DrawQuadrilateral(Outer, Bottom, Top, Shoulder, MakeColor(7, 8, 9));
        DrawTriangle(Shoulder, Top, Ring[2], MakeColor(10, 11, 12));
        Vector3 Center = GameState.Road[NodeIndex].Position + Vector3{0, Tuning::TunnelHeight, 0};
        DrawTriangle(Top, Center, Ring[2], MakeColor(12, 13, 14));
      }
    }
  }
}
void Renderer::RenderRoad(const Game &GameState, float ViewYaw) {
  bool ReverseView = std::cos(ViewYaw - GameState.Road[GameState.Segment].Heading) < 0;
  const int ViewDistance = GameState.SelectedTrack == 2 ? Tuning::SnowRoadAhead : Tuning::RoadAhead;
  const int Behind = ReverseView ? ViewDistance : Tuning::RoadBehind;
  const int Ahead = ReverseView ? Tuning::RoadBehind : ViewDistance;
  const int First = std::max(0, GameState.Segment - Behind),
            Last = std::min(NodeCount - 1, GameState.Segment + Ahead);
  RenderMountain(GameState, First, Last);
  for (int Index = First; Index < Last; ++Index) {
    ShadowEnabled = std::abs(Index - GameState.Segment) <= 2;
    const int Zone = Game::GetSectionIndex(Index);
    const uint16_t Grasses[] = {MakeColor(4, 6, 3), MakeColor(7, 7, 4), MakeColor(6, 6, 5),
                                MakeColor(3, 6, 4)};
    const uint16_t Gravels[] = {MakeColor(9, 9, 7), MakeColor(10, 9, 7), MakeColor(8, 8, 8),
                                MakeColor(8, 8, 6)};
    uint16_t Grass = Grasses[Zone], RoadColor = Gravels[Zone];
    if (GameState.SelectedTrack == 1) {
      Grass = MakeColor(14, 12, 8);
      RoadColor = MakeColor(12, 10, 6);
    }
    if (GameState.Bridge(Index))
      RoadColor = MakeColor(8, 7, 5);
    if (GameState.SelectedTrack == 2) {
      Grass = MakeColor(13, 14, 15);
      RoadColor = GameState.Icy(Index) ? MakeColor(7, 11, 13) : MakeColor(11, 12, 13);
    }
    if (GameState.Tunnel(Index))
      RoadColor = MakeColor(6, 7, 8);
    const auto &CurrentRing = GameState.Terrain[Index];
    const auto &NextRing = GameState.Terrain[Index + 1];
    for (int Side = 0; Side < 2; ++Side) {
      const int Far = Side ? 9 : 0, Bank = Side ? 8 : 1, Verge = Side ? 7 : 2, Edge = Side ? 6 : 3;
      // Authored world positions are built once, avoiding repeated software
      // floating-point terrain sampling for every face on the RP2040.
      const bool Water = GameState.Bridge(Index) || (GameState.Coast(Index) && Side == 1);
      uint16_t Outer = Water ? MakeColor(3, 8, 11) : Grass;
      uint16_t BankColor = GameState.Coast(Index) && Side == 1 ? MakeColor(14, 12, 8) : Grass;
      DrawGroundTriangle(CurrentRing[Far], NextRing[Far], NextRing[Bank], ShadeColor(Outer, -1));
      DrawGroundTriangle(CurrentRing[Far], NextRing[Bank], CurrentRing[Bank], Outer);
      DrawGroundTriangle(CurrentRing[Bank], NextRing[Bank], NextRing[Verge],
                         ShadeColor(BankColor, Index % 3 == 0 ? 1 : 0));
      DrawGroundTriangle(CurrentRing[Bank], NextRing[Verge], CurrentRing[Verge],
                         ShadeColor(BankColor, -1));
      DrawGroundQuadrilateral(CurrentRing[Verge], NextRing[Verge], NextRing[Edge],
                              CurrentRing[Edge], ShadeColor(Grass, 1));
    }
    // Material strips partition the surface: no coplanar wheel-track overlays.
    for (int Strip = 3; Strip < 6; ++Strip)
      DrawGroundQuadrilateral(CurrentRing[Strip], NextRing[Strip], NextRing[Strip + 1],
                              CurrentRing[Strip + 1], ShadeColor(RoadColor, Strip == 4 ? -1 : 0));
    RenderScenery(GameState, Index);
    RenderTrackObjects(GameState, Index);
  }
}
void Renderer::RenderScenery(const Game &GameState, int Index) {
  const int Zone = Game::GetSectionIndex(Index);
  if (Index % 3 == 0)
    for (int Sign : {-1, 1}) {
      if (!GameState.HasScenery(Index, Sign))
        continue;
      Vector3 Position = GameState.Scenery(Index, Sign);
      if (GameState.SelectedTrack == 2 ||
          (GameState.SelectedTrack == 0 &&
           (Zone == 0 || Zone == 3 || (Zone == 1 && Index % 12 == 0))) ||
          (GameState.SelectedTrack == 1 && Index % 6 == 0)) {
        const float Height = 5.f + float((Index * 7) % 4);
        bool Distant = false;
        {
          const auto Center = Transform(Position + Vector3{0, Height * .5f, 0});
          const int Radius = int(Height * .75f * Tuning::WorldScale);
          // Conservative sphere/plane rejection keeps complete silhouettes
          // while avoiding geometry work for trees outside the camera view.
          if (Center.CoordinateZ + Radius < Tuning::NearPlane ||
              Center.CoordinateZ - Radius > Tuning::FarPlane ||
              std::abs(Center.CoordinateX) * Tuning::FocalLength >
                  Center.CoordinateZ * Tuning::CenterX + Radius * 105 ||
              Center.CoordinateY * Tuning::FocalLength >
                  Center.CoordinateZ * ProjectionY + Radius * 101 ||
              -Center.CoordinateY * Tuning::FocalLength >
                  Center.CoordinateZ * (FramebufferHeight - ProjectionY) + Radius * 109)
            continue;
          // Leave frame-time headroom around the mountain portal by simplifying
          // distant snow trees; road geometry and nearby tree detail are retained.
          const float DetailDistance = GameState.SelectedTrack == 2
                                           ? Tuning::SnowSceneryDetailDistance
                                           : Tuning::SceneryDetailDistance;
          Distant = Center.CoordinateZ > DetailDistance * Tuning::WorldScale;
        }
        if (GameState.SelectedTrack == 1)
          DrawPalmTree(Position, Height, Index);
        else if (Distant)
          DrawDistantSnowTree(Position, Height, Index, GameState.SelectedTrack == 2);
        else
          DrawSnowTree(Position, Height, Index);
        if (GameState.SelectedTrack == 2 && !Distant) {
          Vector3 Top = Position + Vector3{0, 5.2f + float((Index * 7) % 4), 0};
          DrawTriangle(Top, Position + Vector3{-1.2f, 3.8f, 0}, Position + Vector3{1.2f, 3.8f, 0},
                       MakeColor(14, 15, 15));
        }
      } else {
        // Replace trees with heather/rock outcrops on exposed sections.
        float RedComponent = Zone == 2 ? 1.4f : .8f, Height = Zone == 2 ? 2.f : .6f;
        Vector3 FirstVertex = Position + Vector3{-RedComponent, 0, -RedComponent},
                SecondVertex = Position + Vector3{RedComponent, 0, -RedComponent},
                ThirdVertex = Position + Vector3{RedComponent, 0, RedComponent},
                FourthVertex = Position + Vector3{-RedComponent, 0, RedComponent};
        Vector3 Top = Position + Vector3{-.3f, Height, .2f};
        uint16_t Rock = Zone == 2 ? MakeColor(7, 8, 8) : MakeColor(6, 5, 5);
        DrawTriangle(FirstVertex, SecondVertex, Top, ShadeColor(Rock, -2));
        DrawTriangle(SecondVertex, ThirdVertex, Top, Rock);
        DrawTriangle(ThirdVertex, FourthVertex, Top, ShadeColor(Rock, 1));
        DrawTriangle(FourthVertex, FirstVertex, Top, ShadeColor(Rock, -1));
      }
    }
}
void Renderer::RenderTrackObjects(const Game &GameState, int Index) {
  auto CurrentIndex = [&](int OtherIndex, float Side, float Rise = 0.f) {
    return GameState.Roadside(OtherIndex, Side) + Vector3{0, Rise, 0};
  };
  if (GameState.Bridge(Index) || GameState.Tunnel(Index)) {
    const bool Tunnel = GameState.Tunnel(Index);
    for (int Sign : {-1, 1}) {
      Vector3 FirstVertex =
          CurrentIndex(Index, Sign * (GameState.Road[Index].HalfWidth + Tuning::RailMargin));
      Vector3 SecondVertex = CurrentIndex(
          Index + 1, Sign * (GameState.Road[Index + 1].HalfWidth + Tuning::RailMargin));
      float WallHeight = Tunnel ? Tuning::TunnelHeight : 1.f;
      uint16_t Wall = Tunnel ? MakeColor(5, 6, 7) : MakeColor(8, 7, 5);
      DrawQuadrilateral(FirstVertex, SecondVertex, SecondVertex + Vector3{0, WallHeight, 0},
                        FirstVertex + Vector3{0, WallHeight, 0}, Wall);
      if (!Tunnel) {
        DrawBox(FirstVertex, {.25f, 1.25f, .25f}, GameState.Road[Index].Heading,
                MakeColor(12, 11, 8));
        DrawQuadrilateral(FirstVertex, SecondVertex, SecondVertex + Vector3{0, -1.2f, 0},
                          FirstVertex + Vector3{0, -1.2f, 0}, MakeColor(5, 5, 4));
        if (Index % 3 == 0)
          DrawBox(FirstVertex + Vector3{0, -Tuning::RiverDrop, 0}, {.7f, Tuning::RiverDrop, .7f},
                  GameState.Road[Index].Heading, MakeColor(6, 6, 5));
      }
    }
    if (Tunnel) {
      Vector3 FirstVertex = CurrentIndex(
          Index, -GameState.Road[Index].HalfWidth - Tuning::RailMargin, Tuning::TunnelHeight);
      Vector3 SecondVertex = CurrentIndex(
          Index, GameState.Road[Index].HalfWidth + Tuning::RailMargin, Tuning::TunnelHeight);
      Vector3 ThirdVertex =
          CurrentIndex(Index + 1, GameState.Road[Index + 1].HalfWidth + Tuning::RailMargin,
                       Tuning::TunnelHeight);
      Vector3 FourthVertex =
          CurrentIndex(Index + 1, -GameState.Road[Index + 1].HalfWidth - Tuning::RailMargin,
                       Tuning::TunnelHeight);
      DrawQuadrilateral(FirstVertex, SecondVertex, ThirdVertex, FourthVertex, MakeColor(4, 5, 6));
      if (Index % 3 == 0) {
        Vector3 Lamp = CurrentIndex(Index, 0, Tuning::TunnelHeight - .12f);
        DrawBox(Lamp, {1.2f, .06f, .5f}, GameState.Road[Index].Heading, MakeColor(15, 14, 9));
      }
    }
  }
  // Sector gates make the timing lines visible before reaching them.
  if (std::find(SectorEnds.begin(), SectorEnds.end(), Index) != SectorEnds.end()) {
    for (int Sign : {-1, 1}) {
      Vector3 Position = CurrentIndex(Index, Sign * (GameState.Road[Index].HalfWidth + .55f));
      DrawBox(Position, {.24f, 3.2f, .24f}, GameState.Road[Index].Heading, MakeColor(14, 12, 3));
      Vector3 Right = GameState.Road[Index].Right * .8f;
      DrawQuadrilateral(Position + Vector3{0, 3.2f, 0}, Position + Right + Vector3{0, 3.2f, 0},
                        Position + Right + Vector3{0, 2.1f, 0}, Position + Vector3{0, 2.1f, 0},
                        MakeColor(3, 4, 10));
    }
  }
  if (Index % 6 == 0)
    for (int Sign : {-1, 1}) {
      Vector3 Position = CurrentIndex(Index, Sign * (GameState.Road[Index].HalfWidth + .6f));
      const auto Marker = Transform(Position + Vector3{0, .45f, 0});
      if (Marker.CoordinateZ < -64 || std::abs(Marker.CoordinateX) > Marker.CoordinateZ + 128)
        continue;
      DrawBox(Position, {.16f, .85f, .16f}, 0, MakeColor(13, 13, 11));
      DrawBox(Position + Vector3{0, .6f, 0}, {.2f, .24f, .2f}, 0, MakeColor(12, 3, 2));
    }
  if (Index == NodeCount - 4 || Index == 2) {
    Vector3 Position = CurrentIndex(Index, 0, .035f);
    for (int SampleIndex = 0; SampleIndex < 10; ++SampleIndex) {
      float Width = GameState.Road[Index].HalfWidth;
      float LeftValue = -Width + SampleIndex * Width / 5, RedComponent = LeftValue + Width / 5;
      Vector3 FirstVertex = CurrentIndex(Index, LeftValue, .035f),
              SecondVertex = CurrentIndex(Index, RedComponent, .035f);
      Vector3 FourthVertex = {std::sin(GameState.Road[Index].Heading) * .7f, 0,
                              std::cos(GameState.Road[Index].Heading) * .7f};
      DrawQuadrilateral(FirstVertex, SecondVertex, SecondVertex + FourthVertex,
                        FirstVertex + FourthVertex,
                        SampleIndex % 2 ? MakeColor(2, 3, 3) : MakeColor(14, 14, 12));
    }
    (void)Position;
  }
}
void Renderer::RenderCar(const Game &GameState) {
  const bool Showroom = GameState.CurrentMode == GameMode::CarSelect;
  const float CarSine = ShadowSine, CarCosine = ShadowCosine;
  const float PitchSine = std::sin(Showroom ? 0 : GameState.Pitch),
              PitchCosine = std::cos(Showroom ? 0 : GameState.Pitch),
              RoadSine = std::sin(Showroom ? 0 : GameState.Roll),
              RoadCosine = std::cos(Showroom ? 0 : GameState.Roll);
  auto TransformCarPoint = [&](float CoordinateX, float CoordinateY, float CoordinateZ) {
    CoordinateX *= GameState.GetCarSpecification().Width;
    CoordinateY *= GameState.GetCarSpecification().Height;
    CoordinateZ *= GameState.GetCarSpecification().Length;
    float RelativeY = CoordinateY * RoadCosine + CoordinateX * RoadSine,
          RelativeX = CoordinateX * RoadCosine - CoordinateY * RoadSine;
    float PositionY = RelativeY * PitchCosine + CoordinateZ * PitchSine,
          PositionZ = CoordinateZ * PitchCosine - RelativeY * PitchSine;
    return GameState.CarPosition + Vector3{RelativeX * CarCosine + PositionZ * CarSine, PositionY,
                                           PositionZ * CarCosine - RelativeX * CarSine};
  };
  auto Panel = [&](Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                   Vector3 FourthVertex, uint16_t SurfaceColor) {
    DrawQuadrilateral(TransformCarPoint(FirstVertex.CoordinateX, FirstVertex.CoordinateY,
                                        FirstVertex.CoordinateZ),
                      TransformCarPoint(SecondVertex.CoordinateX, SecondVertex.CoordinateY,
                                        SecondVertex.CoordinateZ),
                      TransformCarPoint(ThirdVertex.CoordinateX, ThirdVertex.CoordinateY,
                                        ThirdVertex.CoordinateZ),
                      TransformCarPoint(FourthVertex.CoordinateX, FourthVertex.CoordinateY,
                                        FourthVertex.CoordinateZ),
                      SurfaceColor);
  };
  auto DrawCarBox = [&](Vector3 Position, Vector3 Size, uint16_t SurfaceColor) {
    Vector3 Vertex[8];
    for (int Index = 0; Index < 8; ++Index)
      Vertex[Index] =
          TransformCarPoint(Position.CoordinateX + (Index & 1 ? 1 : -1) * Size.CoordinateX * .5f,
                            Position.CoordinateY + ((Index & 4) ? Size.CoordinateY : 0),
                            Position.CoordinateZ + (Index & 2 ? 1 : -1) * Size.CoordinateZ * .5f);
    Vector3 Relative = Camera - GameState.CarPosition;
    float CoordinateX = Relative.CoordinateX * CarCosine - Relative.CoordinateZ * CarSine,
          CoordinateZ = Relative.CoordinateX * CarSine + Relative.CoordinateZ * CarCosine;
    if (CoordinateZ < Position.CoordinateZ)
      DrawQuadrilateral(Vertex[0], Vertex[1], Vertex[5], Vertex[4], ShadeColor(SurfaceColor, -1));
    else
      DrawQuadrilateral(Vertex[2], Vertex[3], Vertex[7], Vertex[6], SurfaceColor);
    if (CoordinateX < Position.CoordinateX)
      DrawQuadrilateral(Vertex[0], Vertex[2], Vertex[6], Vertex[4], ShadeColor(SurfaceColor, -1));
    else
      DrawQuadrilateral(Vertex[1], Vertex[3], Vertex[7], Vertex[5], SurfaceColor);
    DrawQuadrilateral(Vertex[4], Vertex[5], Vertex[7], Vertex[6], ShadeColor(SurfaceColor, 1));
  };
  for (float CoordinateX : {-.91f, .91f})
    for (float CoordinateZ : {-1.05f, 1.05f})
      DrawCarBox({CoordinateX, .1f, CoordinateZ}, {.32f, .58f, .72f}, MakeColor(2, 2, 2));
  const uint16_t Blue = GameState.SelectedCar == 0   ? MakeColor(14, 5, 3)
                        : GameState.SelectedCar == 1 ? MakeColor(2, 4, 12)
                                                     : MakeColor(14, 13, 10);
  const uint16_t Gold = GameState.SelectedCar == 0   ? MakeColor(15, 12, 8)
                        : GameState.SelectedCar == 1 ? MakeColor(15, 13, 3)
                                                     : MakeColor(12, 3, 3),
                 Glass = MakeColor(3, 5, 6);
  // A watertight painted shell: roof, glazing and stripes ARE the faces.
  // There is no underlying cabin box to fight their depth values.
  for (int Sign : {-1, 1}) {
    float CoordinateX = Sign * .89f;
    Panel({CoordinateX, .36f, -1.62f}, {CoordinateX, .36f, 1.62f}, {CoordinateX, .57f, 1.62f},
          {CoordinateX, .57f, -1.62f}, ShadeColor(Blue, -2));
    Panel({CoordinateX, .57f, -1.62f}, {CoordinateX, .57f, 1.62f}, {CoordinateX, .78f, 1.62f},
          {CoordinateX, .78f, -1.62f}, Gold);
    Panel({CoordinateX, .78f, -1.62f}, {CoordinateX, .78f, 1.62f}, {CoordinateX, .97f, 1.52f},
          {CoordinateX, .97f, -1.52f}, Blue);
    float BottomX = Sign * .82f, TopX = Sign * .62f;
    Panel({BottomX, .97f, -1.12f}, {BottomX, .97f, -.32f}, {TopX, 1.55f, -.24f},
          {TopX, 1.55f, -.70f}, ShadeColor(Glass, -1));
    Panel({BottomX, .97f, -.32f}, {BottomX, .97f, -.20f}, {TopX, 1.55f, -.12f},
          {TopX, 1.55f, -.24f}, Blue);
    Panel({BottomX, .97f, -.20f}, {BottomX, .97f, .92f}, {TopX, 1.55f, .32f}, {TopX, 1.55f, -.12f},
          Glass);
    Panel({CoordinateX, .97f, -1.52f}, {CoordinateX, .97f, 1.52f}, {BottomX, .97f, .92f},
          {BottomX, .97f, -1.12f}, Blue);
  }
  Panel({-.62f, 1.55f, -.70f}, {.62f, 1.55f, -.70f}, {.62f, 1.55f, .32f}, {-.62f, 1.55f, .32f},
        Gold);
  Panel({-.82f, .97f, -1.12f}, {.82f, .97f, -1.12f}, {.62f, 1.55f, -.70f}, {-.62f, 1.55f, -.70f},
        ShadeColor(Glass, -1));
  Panel({-.82f, .97f, .92f}, {.82f, .97f, .92f}, {.62f, 1.55f, .32f}, {-.62f, 1.55f, .32f},
        ShadeColor(Glass, 1));
  Panel({-.89f, .97f, -1.52f}, {.89f, .97f, -1.52f}, {.82f, .97f, -1.12f}, {-.82f, .97f, -1.12f},
        Blue);
  Panel({-.89f, .97f, 1.52f}, {.89f, .97f, 1.52f}, {.82f, .97f, .92f}, {-.82f, .97f, .92f}, Blue);
  for (int Sign : {-1, 1}) {
    float CoordinateZ = Sign * 1.62f, TopDepth = Sign * 1.52f;
    Panel({-.89f, .36f, CoordinateZ}, {.89f, .36f, CoordinateZ}, {.89f, .52f, CoordinateZ},
          {-.89f, .52f, CoordinateZ}, MakeColor(2, 3, 4));
    Panel({-.89f, .52f, CoordinateZ}, {.89f, .52f, CoordinateZ}, {.89f, .73f, CoordinateZ},
          {-.89f, .73f, CoordinateZ}, ShadeColor(Blue, -1));
    const float Cuts[] = {-.89f, -.44f, .44f, .89f};
    for (int SampleIndex = 0; SampleIndex < 3; ++SampleIndex)
      Panel({Cuts[SampleIndex], .73f, CoordinateZ}, {Cuts[SampleIndex + 1], .73f, CoordinateZ},
            {Cuts[SampleIndex + 1], .97f, TopDepth}, {Cuts[SampleIndex], .97f, TopDepth},
            SampleIndex == 1 ? Blue : (Sign < 0 ? MakeColor(14, 3, 2) : MakeColor(15, 15, 11)));
  }
  if (GameState.SelectedCar != 0)
    DrawCarBox({0, 1.04f, -1.38f}, {1.86f, .12f, .32f}, ShadeColor(Blue, -1));
}
void Renderer::RenderInterface(const Game &GameState, int FramesPerSecond, bool Diagnostics) {
  const uint16_t White = MakeColor(15, 15, 13), Yellow = MakeColor(15, 13, 3),
                 Dark = MakeColor(1, 2, 2);
  char Buffer[40];
  const char *Confirm = GameState.ControlScheme == Controls::Pico       ? "A"
                        : GameState.ControlScheme == Controls::Keyboard ? "ENTER"
                        : GameState.ControlScheme == Controls::Gamepad  ? "A"
                                                                        : "GO";
  const char *Back = GameState.ControlScheme == Controls::Pico       ? "B"
                     : GameState.ControlScheme == Controls::Keyboard ? "ESC"
                     : GameState.ControlScheme == Controls::Gamepad  ? "B"
                                                                     : "BACK";
  const char *Pause = GameState.ControlScheme == Controls::Pico       ? "Y"
                      : GameState.ControlScheme == Controls::Keyboard ? "P"
                      : GameState.ControlScheme == Controls::Gamepad  ? "START"
                                                                      : "PAUSE";
  const char *AuxiliaryLabel = GameState.ControlScheme == Controls::Keyboard ? "SPACE"
                               : GameState.ControlScheme == Controls::Touch  ? "TAP"
                                                                             : "X";
  const char *Audio = GameState.ControlScheme == Controls::Keyboard ? "M"
                      : GameState.ControlScheme == Controls::Touch  ? "TAP"
                                                                    : "X";
  if (GameState.CurrentMode == GameMode::Title) {
    DrawRectangle(0, 0, FramebufferWidth, 25, Dark);
    DrawCenteredText(8, "GRAVELBYTE", White, 2);
    std::snprintf(Buffer, sizeof(Buffer), "PRESS %s TO CONTINUE", Confirm);
    DrawCenteredText(99, Buffer, White);
    std::snprintf(Buffer, sizeof(Buffer), "%s SOUND %s", Audio, GameState.Muted ? "OFF" : "ON");
    DrawCenteredText(111, Buffer, Yellow);
    return;
  }
  if (GameState.CurrentMode == GameMode::Finished) {
    if (GameState.ShowRecords) {
      DrawRectangle(2, 7, 116, 94, Dark);
      DrawCenteredText(12, "STAGE COMPLETE", Yellow);
      TimeText(Buffer, sizeof(Buffer), GameState.Elapsed);
      DrawCenteredText(23, Buffer, White, 2);
      DrawCenteredText(38, "GATE TARGET  BEST", Yellow);
      for (int Index = 0; Index < SectorCount; ++Index) {
        if (GameState.PriorSplits[Index] > 0)
          std::snprintf(Buffer, sizeof(Buffer), "%d %+.2f %+.2f", Index + 1,
                        GameState.Splits[Index] - GameState.GetDefaultSplits()[Index],
                        GameState.Splits[Index] - GameState.PriorSplits[Index]);
        else
          std::snprintf(Buffer, sizeof(Buffer), "%d %+.2f  --", Index + 1,
                        GameState.Splits[Index] - GameState.GetDefaultSplits()[Index]);
        DrawCenteredText(48 + Index * 9, Buffer,
                         GameState.Splits[Index] < GameState.GetDefaultSplits()[Index]
                             ? MakeColor(6, 15, 7)
                             : MakeColor(15, 6, 4));
      }
      DrawCenteredText(94, "SECONDS / CUMULATIVE", White);
    }
    std::snprintf(Buffer, sizeof(Buffer), "%s RECORDS", AuxiliaryLabel);
    DrawCenteredText(104, Buffer, Yellow);
    std::snprintf(Buffer, sizeof(Buffer), "%s RETRY  %s SELECT", Confirm, Back);
    DrawCenteredText(113, Buffer, White);
    return;
  }
  if (GameState.CurrentMode == GameMode::CarSelect) {
    DrawCenteredText(6, GameState.GetCarSpecification().Name, White,
                     std::strlen(GameState.GetCarSpecification().Name) <= 14 ? 2 : 1);
    DrawCenteredText(21, GameState.GetCarSpecification().Difficulty, Yellow);
    DrawText(5, 52, "<", White, 2);
    DrawText(107, 52, ">", White, 2);
    const char *Labels[] = {"SPEED", "ACCEL", "DRIFT"};
    const int Values[] = {GameState.GetCarSpecification().SpeedStatistic,
                          GameState.GetCarSpecification().AccelerationStatistic,
                          GameState.GetCarSpecification().DriftStatistic};
    for (int Row = 0; Row < 3; ++Row) {
      DrawText(15, Tuning::StatisticsTop + Row * Tuning::StatisticsRowHeight, Labels[Row], White);
      for (int SampleIndex = 0; SampleIndex < Tuning::StatisticBarCount; ++SampleIndex)
        DrawRectangle(53 + SampleIndex * 10,
                      Tuning::StatisticsTop + Row * Tuning::StatisticsRowHeight, 8, 4,
                      SampleIndex < Values[Row] ? Yellow : MakeColor(4, 5, 5));
    }
    std::snprintf(Buffer, sizeof(Buffer), "%s NEXT  %s BACK", Confirm, Back);
    DrawCenteredText(112, Buffer, White);
    return;
  }
  if (GameState.CurrentMode == GameMode::TrackSelect) {
    DrawRectangle(2, 3, 116, 24, Dark);
    DrawCenteredText(8, TrackNames[GameState.SelectedTrack], White);
    DrawCenteredText(19,
                     GameState.SelectedTrack == 0   ? "FOREST GRAVEL"
                     : GameState.SelectedTrack == 1 ? "DRY GRAVEL"
                                                    : "SNOW AND ICE",
                     Yellow);
    DrawText(5, 52, "<", White, 2);
    DrawText(107, 52, ">", White, 2);
    DrawRectangle(4, 88, 112, 29, Dark);
    TimeText(Buffer, sizeof(Buffer), GameState.GetDefaultSplits().back());
    char Line[40];
    std::snprintf(Line, sizeof(Line), "TO BEAT %s", Buffer);
    if (GameState.Unlocked(GameState.SelectedTrack))
      DrawCenteredText(92, Line, Yellow);
    if (!GameState.Unlocked(GameState.SelectedTrack)) {
      std::snprintf(Line, sizeof(Line), "BEAT %s", TrackNames[GameState.SelectedTrack - 1]);
      DrawCenteredText(92, "LOCKED", Yellow);
      DrawCenteredText(101, Line, White);
    } else
      DrawCenteredText(101, "1.8 KM  FIVE SPLITS", White);
    std::snprintf(Buffer, sizeof(Buffer),
                  GameState.Unlocked(GameState.SelectedTrack) ? "%s RACE  %s BACK"
                                                              : "%s LOCKED  %s BACK",
                  Confirm, Back);
    DrawCenteredText(110, Buffer, White);
    return;
  }
  DrawRectangle(2, 2, 36, 9, Dark);
  TimeText(Buffer, sizeof(Buffer), GameState.Elapsed);
  DrawText(4, 4, Buffer, White);
  DrawRectangle(77, 2, 41, 17, Dark);
  DrawText(79, 4, "TO BEAT", Yellow);
  TimeText(Buffer, sizeof(Buffer), GameState.ReferenceSplits.back());
  DrawText(79, 12, Buffer, White);
  DrawRectangle(2, 22, 4 * int(std::strlen(GameState.GetSectionName(GameState.Segment))) + 4, 9,
                Dark);
  DrawText(4, 24, GameState.GetSectionName(GameState.Segment), White);
  if (GameState.SplitMessage > 0 && GameState.CurrentMode == GameMode::Racing) {
    std::snprintf(Buffer, sizeof(Buffer), "%+.2fs", GameState.SplitDelta);
    DrawCenteredText(34, Buffer,
                     GameState.SplitDelta <= 0 ? MakeColor(6, 15, 7) : MakeColor(15, 6, 4));
  }
  // Pick the strongest curve in the next 90m, giving useful notice at racing speed.
  float Upcoming = 0;
  int Distance = 0;
  for (int Index = GameState.Segment + 3; Index < std::min(NodeCount, GameState.Segment + 16);
       ++Index)
    if (std::abs(GameState.Road[Index].Turn) > std::abs(Upcoming)) {
      Upcoming = GameState.Road[Index].Turn;
      Distance = (Index - GameState.Segment) * int(TrackSegmentLength);
    }
  DrawRectangle(49, 2, 22, 18, Dark);
  const int Sign = Upcoming < 0 ? -1 : 1;
  if (std::abs(Upcoming) < .007f) {
    DrawRectangle(59, 5, 2, 8, Yellow);
    DrawRectangle(57, 5, 6, 2, Yellow);
    DrawRectangle(58, 4, 4, 1, Yellow);
  } else {
    DrawRectangle(59, 7, 2, 6, Yellow);
    DrawRectangle(Sign > 0 ? 59 : 54, 6, 7, 2, Yellow);
    DrawRectangle(Sign > 0 ? 64 : 54, 4, 2, 6, Yellow);
    DrawRectangle(Sign > 0 ? 66 : 52, 5, 1, 4, Yellow);
  }
  std::snprintf(Buffer, sizeof(Buffer), "%d", Distance);
  DrawCenteredText(14, Buffer, White);
  DrawRectangle(93, 99, 25, 17, Dark);
  std::snprintf(Buffer, sizeof(Buffer), "%03d", int(GameState.Speed * 3.6f));
  DrawText(95, 101, Buffer, White, 2);
  DrawText(100, 112, "KMH", Yellow);
  DrawRectangle(3, 116, 114, 2, Dark);
  DrawRectangle(3, 116, int(114 * GameState.Progress()), 2, Yellow);
  for (int Index = 0; Index < SectorCount; ++Index) {
    int CoordinateX = 3 + int(113.f * (SectorEnds[Index] - 1) / (NodeCount - 5));
    const uint16_t CheckpointColor = GameState.Splits[Index] <= GameState.ReferenceSplits[Index]
                                         ? MakeColor(6, 15, 7)
                                         : MakeColor(15, 6, 4);
    DrawRectangle(CoordinateX - 1, 114, 3, 3,
                  Index < GameState.SplitCount ? CheckpointColor : White);
    if (Index >= GameState.SplitCount)
      DrawRectangle(CoordinateX, 115, 1, 1, Dark);
  }
  if (GameState.RecoveryMessage > 0) {
    DrawRectangle(12, 72, 96, 10, Dark);
    DrawCenteredText(75, "RECOVERED +3 SEC", Yellow);
  }
  if (GameState.Impact > .4f) {
    DrawRectangle(0, 0, FramebufferWidth, 1, MakeColor(15, 5, 2));
    DrawRectangle(0, 0, 1, FramebufferHeight, MakeColor(15, 5, 2));
  }
  if (GameState.CurrentMode == GameMode::Countdown) {
    DrawRectangle(45, 40, 30, 30, Dark);
    std::snprintf(Buffer, sizeof(Buffer), "%d", int(std::ceil(GameState.Countdown)));
    DrawCenteredText(45, Buffer, Yellow, 4);
  }
  if (GameState.CurrentMode == GameMode::Racing && GameState.Elapsed < .8f) {
    DrawRectangle(42, 42, 36, 19, Dark);
    DrawCenteredText(46, "GO", Yellow, 2);
  }
  if (GameState.CurrentMode == GameMode::Paused) {
    DrawRectangle(5, 36, 110, 61, Dark);
    DrawCenteredText(42, "PAUSED", Yellow, 2);
    std::snprintf(Buffer, sizeof(Buffer), "%s RESUME", Pause);
    DrawCenteredText(57, Buffer, White);
    std::snprintf(Buffer, sizeof(Buffer), "%s RETRY", Confirm);
    DrawCenteredText(67, Buffer, White);
    std::snprintf(Buffer, sizeof(Buffer), "%s SELECT", Back);
    DrawCenteredText(77, Buffer, White);
    std::snprintf(Buffer, sizeof(Buffer), "%s SOUND %s", Audio, GameState.Muted ? "OFF" : "ON");
    DrawCenteredText(88, Buffer, Yellow);
  }
  if (Diagnostics) {
    DrawRectangle(0, 22, 46, 15, Dark);
    std::snprintf(Buffer, sizeof(Buffer), "%d FPS", FramesPerSecond);
    DrawText(2, 24, Buffer, White);
    std::snprintf(Buffer, sizeof(Buffer), "%d TRI", FaceCount);
    DrawText(2, 31, Buffer, Yellow);
  }
}
} // namespace Rally
