#pragma once

#include "ucoord.h"

class CChord {
public:
    CUCoord m_Start{};
    CUCoord m_End{};

    CChord() = default;
    CChord(const CUCoord& start, const CUCoord& end);
    CChord(const CChord& other) = default;

    CChord& operator=(const CChord& other) = default;
    bool operator==(const CChord& other) const;
};
