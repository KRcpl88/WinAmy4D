#pragma once

#include <cstdint>

struct scoord_bitfield_t {
    std::uint16_t value{0};

    constexpr scoord_bitfield_t() = default;
    constexpr scoord_bitfield_t(std::uint16_t valueIn) : value(valueIn) {
    }

    constexpr operator std::uint16_t() const {
        return value;
    }
};

class CUCoord;  // forward declaration

// CSCoordBase — a level/file/rank board coordinate that deliberately does NOT
// support conversion to or from a packed bit offset. It exists so that code
// dealing with a *swapped* (axis-permuted) board location used purely for
// rendering the 2D view cannot accidentally call BitOffset()/GetBitField():
// after an axis swap the bit offset of the swapped coordinate is meaningless
// and must never be used. CSCoord (below) derives from this and adds the
// bit-offset-aware machinery for ordinary board squares.
class CSCoordBase {
public:
    std::uint16_t m_nLevel{0};
    std::uint16_t m_nRank{0};
    std::uint16_t m_nFile{0};

    CSCoordBase() = default;
    CSCoordBase(std::uint16_t level, std::uint16_t file, std::uint16_t rank);

    void Validate() const;
    bool IsValid() const;

    static bool IsValid(std::uint16_t level, std::uint16_t file, std::uint16_t rank);

    bool operator==(const CSCoordBase& other) const;
    bool operator!=(const CSCoordBase& other) const;
};

class CSCoord : public CSCoordBase {
public:
    CSCoord() = default;
    CSCoord(std::uint16_t level, std::uint16_t file, std::uint16_t rank);
    explicit CSCoord(std::uint16_t offset);
    explicit CSCoord(scoord_bitfield_t bitfield);

    using CSCoordBase::IsValid;
    static bool IsValid(std::uint16_t offset);

    CSCoord Step(CUCoord Direction) const;

    std::uint16_t BitOffset() const;
    scoord_bitfield_t GetBitField() const;

    // Mirror rank within the level (rank 0↔max, 1↔max-1, etc.)
    CSCoord ReflectRank() const;

    explicit operator int() const;

private:
    static void ValidateOffset(std::uint16_t offset);
};
