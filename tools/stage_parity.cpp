// Emit generated roads for numerical comparisons between native and WASM builds.
#include "game.hpp"
#include <cstdio>
using namespace GravelByte;
int main() {
  for (uint32_t Seed : {0u, 1u, 2u, 0x1234abcdu, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
    Game Stage;
    Stage.SetChallengeSeed(Seed);
    std::printf("seed %08lx biome %d length %.5f checkpoints", static_cast<unsigned long>(Seed),
                Stage.SelectedTrack, Stage.SegmentLength);
    for (int Checkpoint : Stage.SectorEnds)
      std::printf(" %d", Checkpoint);
    std::printf("\n");
    for (int Index = 0; Index < NodeCount; ++Index) {
      const auto &Node = Stage.Road[Index];
      std::printf("%d %.6f %.6f %.6f %.6f\n", Index, Node.Position.CoordinateX,
                  Node.Position.CoordinateY, Node.Position.CoordinateZ, Node.Turn);
    }
  }
}
