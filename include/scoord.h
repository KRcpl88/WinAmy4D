#pragma once

#include <cstdint>

typedef std::uint16_t scoord_bitfield_t;

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


    int m_nLevel{0};
    int m_nRank{0};
    int m_nFile{0};

    CSCoord() = default;
    CSCoord(int level, int file, int rank);
    explicit CSCoord(int offset);
    explicit CSCoord(scoord_bitfield_t bitfield);

    void Validate() const;
    bool IsValid() const;

    static bool IsValid(int level, int file, int rank);
    static bool IsValid(int offset);

    CSCoord Step(CUCoord Direction) const;
    
    int BitOffset() const;
    scoord_bitfield_t GetBitField() const;

    // Mirror rank within the level (rank 0↔max, 1↔max-1, etc.)
    CSCoord ReflectRank() const;

    explicit operator int() const;

private:
    static void ValidateOffset(int offset);
};
