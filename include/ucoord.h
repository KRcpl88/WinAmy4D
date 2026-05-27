#pragma once

#include "scoord.h"

class CChord;

class CUCoord {
public:
    int m_rgnData[3]{};

    CUCoord() = default;
    CUCoord(int nX, int nY, int nZ);
    CUCoord(const int rgnData[3]);
    explicit CUCoord(const CSCoord& scoord);

    bool GetOutline( __inout_ecount(cChords) CChord* prgChords, __in size_t cChords) const;

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
