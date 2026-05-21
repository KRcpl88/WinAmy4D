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

class CSCoord {
public:
    static const unsigned int LEVEL_SIZE[1];
    static const unsigned int LEVEL_WIDTH[1];
    static const unsigned int LEVEL_OFFSET[1];

    static constexpr unsigned int NUM_LEVELS = 1U;
    static constexpr unsigned int MAX_LEVEL_WIDTH = 8U;
    static constexpr unsigned int SIZE = 64U;
    static constexpr unsigned int ULONG_SIZE_BITS = static_cast<unsigned int>(sizeof(std::uint64_t) * 8U);
    static constexpr unsigned int SIZE_LONG = 1U;


    std::uint16_t m_nLevel{0};
    std::uint16_t m_nRank{0};
    std::uint16_t m_nFile{0};

    CSCoord() = default;
    CSCoord(std::uint16_t level, std::uint16_t file, std::uint16_t rank);
    explicit CSCoord(std::uint16_t offset);
    explicit CSCoord(scoord_bitfield_t bitfield);

    void Validate() const;
    bool IsValid() const;

    static bool IsValid(std::uint16_t level, std::uint16_t file, std::uint16_t rank);
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
