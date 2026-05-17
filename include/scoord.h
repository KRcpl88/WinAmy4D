#pragma once

#include <array>
#include <cstdint>

class CSCoord {
public:
    inline static constexpr std::array<int, 1> LEVEL_SIZE{64};
    inline static constexpr std::array<int, 1> LEVEL_WIDTH{8};
    inline static constexpr std::array<int, 1> LEVEL_OFFSET{0};

    static constexpr int NUM_LEVELS = 1;
    static constexpr int MAX_LEVEL_WIDTH = 8;
    static constexpr int SIZE = 64;
    static constexpr int ULONG_SIZE_BITS = static_cast<int>(sizeof(std::uint64_t) * 8);
    static constexpr int SIZE_LONG = 1;


    int Level{0};
    int Rank{0};
    int File{0};

    CSCoord() = default;
    CSCoord(int level, int file, int rank);
    explicit CSCoord(int offset);

    void Validate() const;
    bool IsValid() const;

    static bool IsValid(int level, int file, int rank);
    static bool IsValid(int offset);

    int BitOffset() const;

    explicit operator int() const;

private:
    static void ValidateOffset(int offset);
};

