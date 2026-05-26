#pragma once

#include "ucoord.h"

class CChord {
    CUCoord m_rgData[2] {};
public:
    CChord() = default;
    CChord(const CUCoord& start, const CUCoord& end);
    CChord(const CUCoord rgData[2]);
    CChord(const CChord& other) = default;

    CUCoord& operator[](int index) { return m_rgData[index]; }
    const CUCoord& operator[](int index) const { return m_rgData[index]; }

    CUCoord& GetX() { return m_rgData[0]; }
    const CUCoord& GetX() const { return m_rgData[0]; }
    void SetX(const CUCoord& value) { m_rgData[0] = value; }

    CUCoord& GetY() { return m_rgData[1]; }
    const CUCoord& GetY() const { return m_rgData[1]; }
    void SetY(const CUCoord& value) { m_rgData[1] = value; }

    CChord& operator=(const CChord& other) = default;
    bool operator==(const CChord& other) const;
};
