#pragma once
#include "game.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace GravelByte {
// One independently validated page per sector. Never erase the selected slot.
struct SaveSlot {
  uint32_t Magic, Sequence;
  SaveData Data;
  uint32_t Checksum;
};
static_assert(sizeof(SaveSlot) <= 256);
constexpr uint32_t JournalMagic = 0x47524a31;
inline uint32_t SlotChecksum(const SaveSlot &Slot) {
  uint32_t Hash = Tuning::HashOffsetBasis;
  const auto *Bytes = reinterpret_cast<const uint8_t *>(&Slot);
  for (std::size_t Index = 0; Index < offsetof(SaveSlot, Checksum); ++Index)
    Hash = (Hash ^ Bytes[Index]) * Tuning::HashPrime;
  return Hash;
}
inline bool ValidSlot(const SaveSlot &Slot) {
  return Slot.Magic == JournalMagic && Slot.Checksum == SlotChecksum(Slot) && ValidSave(Slot.Data);
}
inline int NewestSlot(const SaveSlot &FirstSlot, const SaveSlot &SecondSlot) {
  const bool FirstSlotValid = ValidSlot(FirstSlot), SecondSlotValid = ValidSlot(SecondSlot);
  if (!FirstSlotValid)
    return SecondSlotValid ? 1 : -1;
  if (!SecondSlotValid)
    return 0;
  const uint32_t Distance = SecondSlot.Sequence - FirstSlot.Sequence;
  return Distance != 0 && Distance < 0x80000000u ? 1 : 0;
}
inline SaveSlot MakeSlot(const SaveData &Data, uint32_t Sequence) {
  SaveSlot Slot{JournalMagic, Sequence, Data, 0};
  Slot.Checksum = SlotChecksum(Slot);
  return Slot;
}
enum class SaveResult { Unchanged, Saved, Failed };
inline SaveResult StoreSave(const SaveData &Data, const SaveSlot &FirstSlot,
                            const SaveSlot &SecondSlot, void (*Write)(int, const SaveSlot &)) {
  const int Current = NewestSlot(FirstSlot, SecondSlot);
  const auto &Active = Current == 0 ? FirstSlot : SecondSlot;
  if (Current >= 0 && std::memcmp(&Active.Data, &Data, sizeof(Data)) == 0)
    return SaveResult::Unchanged;
  // Slot 1 also contains the old non-journal format; first migration uses 0.
  const int Target = Current == 0 ? 1 : 0;
  const auto Next = MakeSlot(Data, Current < 0 ? 0 : Active.Sequence + 1);
  Write(Target, Next);
  const auto &Stored = Target == 0 ? FirstSlot : SecondSlot;
  return ValidSlot(Stored) && std::memcmp(&Stored, &Next, sizeof(Next)) == 0 ? SaveResult::Saved
                                                                             : SaveResult::Failed;
}
} // namespace GravelByte
