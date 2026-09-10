#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

// An XZ barycentric query detects terrain folded over the drivable road,
// independently of the renderer's depth test or its camera projection.
static float TriangleHeight(GravelByte::Vector3 Position, GravelByte::Vector3 FirstVertex,
                            GravelByte::Vector3 SecondVertex, GravelByte::Vector3 ThirdVertex) {
  float Difference = (SecondVertex.CoordinateZ - ThirdVertex.CoordinateZ) *
                         (FirstVertex.CoordinateX - ThirdVertex.CoordinateX) +
                     (ThirdVertex.CoordinateX - SecondVertex.CoordinateX) *
                         (FirstVertex.CoordinateZ - ThirdVertex.CoordinateZ);
  if (std::abs(Difference) < .001f)
    return -std::numeric_limits<float>::infinity();
  float HorizontalFraction = ((SecondVertex.CoordinateZ - ThirdVertex.CoordinateZ) *
                                  (Position.CoordinateX - ThirdVertex.CoordinateX) +
                              (ThirdVertex.CoordinateX - SecondVertex.CoordinateX) *
                                  (Position.CoordinateZ - ThirdVertex.CoordinateZ)) /
                             Difference;
  float Value = ((ThirdVertex.CoordinateZ - FirstVertex.CoordinateZ) *
                     (Position.CoordinateX - ThirdVertex.CoordinateX) +
                 (FirstVertex.CoordinateX - ThirdVertex.CoordinateX) *
                     (Position.CoordinateZ - ThirdVertex.CoordinateZ)) /
                Difference;
  return HorizontalFraction >= 0 && Value >= 0 && HorizontalFraction + Value <= 1
             ? HorizontalFraction * FirstVertex.CoordinateY + Value * SecondVertex.CoordinateY +
                   (1 - HorizontalFraction - Value) * ThirdVertex.CoordinateY
             : -std::numeric_limits<float>::infinity();
}
int main() {
  using namespace GravelByte;
  Game GameState;
  int Samples = 0;
  for (int Track = 0; Track < TrackCount; ++Track) {
    GameState.SelectCarAndTrack(1, Track);
    for (int Count = 1; Count < NodeCount - 1; ++Count)
      for (float SegmentFraction : {0.f, .25f, .5f, .75f})
        for (float Fraction : {-.95f, -.5f, 0.f, .5f, .95f}) {
          Vector3 Position =
              GameState.Roadside(Count, GameState.Road[Count].HalfWidth * Fraction) *
                  (1 - SegmentFraction) +
              GameState.Roadside(Count + 1, GameState.Road[Count + 1].HalfWidth * Fraction) *
                  SegmentFraction;
          ++Samples;
          for (int Index = std::max(0, Count - 5); Index < std::min(NodeCount - 1, Count + 31);
               ++Index)
            for (int Side = 0; Side < 2; ++Side) {
              int Far = Side ? 9 : 0, Bank = Side ? 8 : 1, Verge = Side ? 7 : 2;
              const auto &CurrentRing = GameState.Terrain[Index];
              const auto &NextRing = GameState.Terrain[Index + 1];
              float Height = std::max(
                  {TriangleHeight(Position, CurrentRing[Far], NextRing[Far], NextRing[Bank]),
                   TriangleHeight(Position, CurrentRing[Far], NextRing[Bank], CurrentRing[Bank]),
                   TriangleHeight(Position, CurrentRing[Bank], NextRing[Bank], NextRing[Verge]),
                   TriangleHeight(Position, CurrentRing[Bank], NextRing[Verge],
                                  CurrentRing[Verge])});
              if (Height > Position.CoordinateY + .1f) {
                std::fprintf(stderr,
                             "Terrain strip %d intrudes %.2fm above road node %d (t=%.2f, width "
                             "fraction=%.2f)\n",
                             Index, Height - Position.CoordinateY, Count, SegmentFraction,
                             Fraction);
                return 1;
              }
            }
        }
  }
  std::printf("PASS: %d road corridor samples clear of overlapping hillsides\n", Samples);
}
