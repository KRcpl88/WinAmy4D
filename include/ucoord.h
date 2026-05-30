#pragma once

#include <unordered_set>

#include "scoord.h"

class CChord;

class CUCoord {
    int m_rgnData[3]{};

public:
    enum EOutlineType 
    {
        OT_full = 0,
        OT_square_z,
        OT_square_y,
        OT_square_x,
        OT_hex_1,
        OT_hex_2,
        OT_hex_3,
        OT_hex_4
    };

    CUCoord() = default;
    CUCoord(int nX, int nY, int nZ);
    CUCoord(const int rgnData[3]);
    explicit CUCoord(const CSCoord& scoord);

    bool GetOutline( __inout std::unordered_set<CChord> & Chords, EOutlineType eOutlineType = OT_hex_1) const;

    int GetX() const;
    void SetX(int nValue);
    int GetY() const;
    void SetY(int nValue);
    int GetZ() const;
    void SetZ(int nValue);

    int& operator[](uint16_t i) { return m_rgnData[i]; }
    const int& operator[](uint16_t i) const { return m_rgnData[i]; }

    explicit operator CSCoord() const;

    bool operator==(const CUCoord& other) const;
    bool operator!=(const CUCoord& other) const;
    CUCoord operator+(const CUCoord& other) const;
    CUCoord operator-(const CUCoord& other) const;
    CUCoord operator-() const;
};
