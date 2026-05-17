#pragma once

#include <array>

#include "scoord.h"

class CUCoord {
public:
    std::array<int, 3> data{};

    CUCoord() = default;
    CUCoord(int x, int y, int z);
    explicit CUCoord(const CSCoord& scoord);

    int getX() const;
    void setX(int value);
    int getY() const;
    void setY(int value);
    int getZ() const;
    void setZ(int value);

    explicit operator CSCoord() const;

    bool operator==(const CUCoord& other) const;
    bool operator!=(const CUCoord& other) const;
    CUCoord operator+(const CUCoord& other) const;
    CUCoord operator-(const CUCoord& other) const;
    CUCoord operator-() const;
};
