#pragma once

#include <cstdint>

typedef std::uint16_t scoord_bitfield_t;

class CSCoord {
public:
    static const int LEVEL_SIZE[1];
    static const int LEVEL_WIDTH[1];
    static const int LEVEL_OFFSET[1];

    static constexpr int NUM_LEVELS = 1;
    static constexpr int MAX_LEVEL_WIDTH = 8;
    static constexpr int SIZE = 64;
    static constexpr int ULONG_SIZE_BITS = static_cast<int>(sizeof(std::uint64_t) * 8);
    static constexpr int SIZE_LONG = 1;


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

    int BitOffset() const;
    scoord_bitfield_t GetBitField() const;

    // Mirror rank within the level (rank 0↔max, 1↔max-1, etc.)
    CSCoord ReflectRank() const;

    explicit operator int() const;

private:
    static void ValidateOffset(int offset);
};
