#pragma once
#include "game.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rally {
// One independently validated page per sector. Never erase the selected slot.
struct SaveSlot {
  uint32_t magic, sequence;
  SaveData data;
  uint32_t checksum;
};
static_assert(sizeof(SaveSlot) <= 256);
constexpr uint32_t JournalMagic = 0x47524a31;
inline uint32_t slot_checksum(const SaveSlot &slot) {
  uint32_t hash = tuning::FnvOffset;
  const auto *bytes = reinterpret_cast<const uint8_t *>(&slot);
  for (std::size_t i = 0; i < offsetof(SaveSlot, checksum); ++i)
    hash = (hash ^ bytes[i]) * tuning::FnvPrime;
  return hash;
}
inline bool valid_slot(const SaveSlot &slot) {
  return slot.magic == JournalMagic && slot.checksum == slot_checksum(slot) &&
         valid_save(slot.data);
}
inline int newest_slot(const SaveSlot &a, const SaveSlot &b) {
  const bool av = valid_slot(a), bv = valid_slot(b);
  if (!av)
    return bv ? 1 : -1;
  if (!bv)
    return 0;
  const uint32_t distance = b.sequence - a.sequence;
  return distance != 0 && distance < 0x80000000u ? 1 : 0;
}
inline SaveSlot make_slot(const SaveData &data, uint32_t sequence) {
  SaveSlot slot{JournalMagic, sequence, data, 0};
  slot.checksum = slot_checksum(slot);
  return slot;
}
enum class SaveResult { Unchanged, Saved, Failed };
inline SaveResult store_save(const SaveData &data, const SaveSlot &a, const SaveSlot &b,
                             void (*write)(int, const SaveSlot &)) {
  const int current = newest_slot(a, b);
  const auto &active = current == 0 ? a : b;
  if (current >= 0 && std::memcmp(&active.data, &data, sizeof(data)) == 0)
    return SaveResult::Unchanged;
  // Slot 1 also contains the old non-journal format; first migration uses 0.
  const int target = current == 0 ? 1 : 0;
  const auto next = make_slot(data, current < 0 ? 0 : active.sequence + 1);
  write(target, next);
  const auto &stored = target == 0 ? a : b;
  return valid_slot(stored) && std::memcmp(&stored, &next, sizeof(next)) == 0 ? SaveResult::Saved
                                                                              : SaveResult::Failed;
}
} // namespace rally
