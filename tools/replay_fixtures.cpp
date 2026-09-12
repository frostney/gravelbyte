// Deterministic public-control replay fixtures for storage and device validation.
#include "game.hpp"
#include "ghost.hpp"
#include "save_journal.hpp"
#include "test_driver.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <vector>
using namespace GravelByte;
static bool Write(const std::filesystem::path &Path, const void *Bytes, std::size_t Count) {
  FILE *Output = std::fopen(Path.c_str(), "wb");
  if (!Output)
    return false;
  const bool Complete = std::fwrite(Bytes, 1, Count, Output) == Count;
  return std::fclose(Output) == 0 && Complete;
}
int main(int ArgumentCount, char **Arguments) {
  if (ArgumentCount != 2)
    return 2;
  const std::filesystem::path Directory(Arguments[1]);
  std::filesystem::create_directories(Directory);
  Game Records;
  Records.SelectedCar = 0;
  Records.DriftHintSeen = true;
  std::vector<uint8_t> Flash(RecordCount * 2 * GhostSlotBytes, 0xff);
  for (int Track = 0; Track < TrackCount; ++Track)
    for (int Car = 0; Car < CarCount; ++Car) {
      Game Race;
      Race.SelectCarAndTrack(Car, Track);
      Race.CurrentMode = GameMode::Racing;
      std::vector<uint8_t> Inputs;
      for (int Frame = 0; Frame < 15000 && Race.CurrentMode != GameMode::Finished; ++Frame) {
        const auto Input = TestDriver(Race);
        Inputs.push_back(
            uint8_t(Input.Left | (Input.Right << 1) | (Input.Throttle << 2) | (Input.Brake << 3)));
        Race.Update(.02f, Input);
      }
      if (!Race.NewRecord || Race.Recoveries)
        return 3;
      GhostTransfer Transfer;
      FillGhostTransfer(Race, Transfer);
      if (!ValidGhost(Transfer.Header, Transfer.Poses.data(), Race))
        return 4;
      Records.Records[Race.RecordIndex()] = Race.Records[Race.RecordIndex()];
      if (Track == 0 && Car == 0) {
        Game BrowserRecord;
        BrowserRecord.SelectedCar = 0;
        BrowserRecord.Records[0] = Race.Records[0];
        BrowserRecord.DriftHintSeen = true;
        const auto Save = EncodeSave(BrowserRecord);
        if (!Write(Directory / "finch-driving-inputs.bin", Inputs.data(), Inputs.size()) ||
            !Write(Directory / "best-records-v4.bin", &Save, sizeof(Save)) ||
            !Write(Directory / "best-ghost-v1.bin", &Transfer, sizeof(Transfer)))
          return 5;
      }
      // The hardware workload fixture keeps a second car two nodes ahead so
      // fade-out cannot accidentally turn a ghost benchmark into a solo race.
      // Browser fixtures above remain genuine unmodified public-control runs.
      for (uint32_t Index = 0; Index < Transfer.Header.Count; ++Index)
        Transfer.Poses[Index].NodeIndex = std::min<int>(297, Transfer.Poses[Index].NodeIndex + 2);
      Transfer.Header.Checksum = HashGhostBytes(Tuning::HashOffsetBasis, &Transfer.Header,
                                                offsetof(GhostHeader, Checksum));
      Transfer.Header.Checksum = HashGhostBytes(Transfer.Header.Checksum, Transfer.Poses.data(),
                                                Transfer.Header.Count * sizeof(ReplayPose));
      std::memcpy(Flash.data() + Race.RecordIndex() * 2 * GhostSlotBytes, &Transfer,
                  sizeof(Transfer));
    }
  std::array<uint8_t, 8192> Journal;
  Journal.fill(0xff);
  const auto Slot = MakeSlot(EncodeSave(Records), 0);
  std::memcpy(Journal.data(), &Slot, sizeof(Slot));
  return Write(Directory / "ghost-region.bin", Flash.data(), Flash.size()) &&
                 Write(Directory / "journal.bin", Journal.data(), Journal.size())
             ? 0
             : 6;
}
