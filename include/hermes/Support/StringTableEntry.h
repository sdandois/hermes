#ifndef HERMES_SUPPORT_STRINGTABLEENTRY_H
#define HERMES_SUPPORT_STRINGTABLEENTRY_H

#include "llvm/ADT/ArrayRef.h"

#include <cstdint>

namespace hermes {

/// An entry in the string table inside the ConsecutiveStringStorage.
class StringTableEntry {
  static constexpr uint32_t UTF16_MASK = 1 << 31;
  static constexpr uint32_t IDENTIFIER_MASK = 1 << 30;

  /// The offset of this string in the string storage.
  uint32_t offset_;

  /// The length of this string. We use the most significant bit to represent
  /// whether it's a UTF16 string; we use the second most significant bit to
  /// represent whether it's an identifier.
  uint32_t length_;

 public:
  using StringTableRefTy = llvm::ArrayRef<StringTableEntry>;
  using StringStorageRefTy = llvm::ArrayRef<char>;

  StringTableEntry(uint32_t offset, uint32_t length, bool isUTF16)
      : offset_(offset), length_(length) {
    if (isUTF16) {
      length_ |= UTF16_MASK;
    }
  }
  /// Default constructor needed for string table resizing.
  StringTableEntry() = default;

  /// \return the offset of this entry's string in its storage.
  uint32_t getOffset() const {
    return offset_;
  }

  /// \return the length of this entry's string.
  uint32_t getLength() const {
    return length_ & (~UTF16_MASK) & (~IDENTIFIER_MASK);
  }

  /// \return whether this entry is UTF16.
  bool isUTF16() const {
    return length_ & UTF16_MASK;
  }

  /// \return whether this entry is an identifier.
  bool isIdentifier() const {
    return length_ & IDENTIFIER_MASK;
  }

  /// Mark this entry as an identifier.
  void markAsIdentifier() {
    length_ |= IDENTIFIER_MASK;
  }
};

} // namespace hermes

#endif // HERMES_SUPPORT_STRINGTABLEENTRY_H
