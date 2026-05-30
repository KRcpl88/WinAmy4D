#include "ucoord.h"

#include <limits>

#include "bitboard.h"
#include "chord.h"

CUCoord::CUCoord(int nX, int nY, int nZ) {
    SetX(nX);
    SetY(nY);
    SetZ(nZ);
}

CUCoord::CUCoord(const int rgnData[3]) {
    SetX(rgnData[0]);
    SetY(rgnData[1]);
    SetZ(rgnData[2]);
}

CUCoord::CUCoord(const CSCoord& scoord) {
    scoord.Validate();

    SetZ(scoord.m_nLevel);
    const unsigned int levelIndex = static_cast<unsigned int>(scoord.m_nLevel);
    const int levelOffset = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH - CBitBoard::LEVEL_WIDTH[levelIndex]);
    const int fileOffset = static_cast<int>(CBitBoard::LEVEL_WIDTH[levelIndex]) - 1;
    SetX(levelOffset + scoord.m_nFile + scoord.m_nRank);
    SetY(levelOffset + scoord.m_nRank - scoord.m_nFile + fileOffset);
}

CChord g_krgFullCellOutline[24]
{
    // +x/+y quadrant
    {{ 0.0,  0.0, -1.0}, { 0.5,  0.5, -0.5}},
    {{ 0.5,  0.5, -0.5}, { 0.0,  1.0,  0.0}},
    {{ 0.0,  1.0,  0.0}, { 0.5,  0.5,  0.5}},
    {{ 0.5,  0.5, -0.5}, { 1.0,  0.0,  0.0}},
    {{ 1.0,  0.0,  0.0}, { 0.5,  0.5,  0.5}},
    {{ 0.5,  0.5,  0.5}, { 0.0,  0.0,  1.0}},
    // +x/-y quadrant
    {{ 0.0,  0.0, -1.0}, { 0.5, -0.5, -0.5}},
    {{ 0.5, -0.5, -0.5}, { 0.0, -1.0,  0.0}},
    {{ 0.0, -1.0,  0.0}, { 0.5, -0.5,  0.5}},
    {{ 0.5, -0.5, -0.5}, { 1.0,  0.0,  0.0}},
    {{ 1.0,  0.0,  0.0}, { 0.5, -0.5,  0.5}},
    {{ 0.5, -0.5,  0.5}, { 0.0,  0.0,  1.0}},
    // -x/-y quadrant
    {{ 0.0,  0.0, -1.0}, {-0.5, -0.5, -0.5}},
    {{-0.5, -0.5, -0.5}, { 0.0, -1.0,  0.0}},
    {{ 0.0, -1.0,  0.0}, {-0.5, -0.5,  0.5}},
    {{-0.5, -0.5, -0.5}, {-1.0,  0.0,  0.0}},
    {{-1.0,  0.0,  0.0}, {-0.5, -0.5,  0.5}},
    {{-0.5, -0.5,  0.5}, { 0.0,  0.0,  1.0}},
    // -x/+y quadrant
    {{ 0.0,  0.0, -1.0}, {-0.5,  0.5, -0.5}},
    {{-0.5,  0.5, -0.5}, { 0.0,  1.0,  0.0}},
    {{ 0.0,  1.0,  0.0}, {-0.5,  0.5,  0.5}},
    {{-0.5,  0.5, -0.5}, {-1.0,  0.0,  0.0}},
    {{-1.0,  0.0,  0.0}, {-0.5,  0.5,  0.5}},
    {{-0.5,  0.5,  0.5}, { 0.0,  0.0,  1.0}},
};


CChord g_krgSquareZCellOutline[4]
{
    {{ 1.0,  0.0, -0.25}, { 0.0, -1.0, -0.25}},
    {{ 0.0, -1.0, -0.25}, {-1.0,  0.0, -0.25}},
    {{-1.0,  0.0, -0.25}, { 0.0,  1.0, -0.25}},
    {{ 0.0,  1.0, -0.25}, { 1.0,  0.0, -0.25}},
};  

CChord g_krgHex1CellOutline[6]
{
    {{-1.0,  0.0,  0.0}, {-0.5, -0.5,  0.5}},
    {{-1.0,  0.0,  0.0}, {-0.5,  0.5, -0.5}},
    {{ 0.0, -1.0,  0.0}, {-0.5, -0.5,  0.5}},
    {{ 0.0, -1.0,  0.0}, { 0.5, -0.5, -0.5}},
    {{ 0.0,  0.0, -1.0}, {-0.5,  0.5, -0.5}},
    {{ 0.0,  0.0, -1.0}, { 0.5, -0.5, -0.5}},
};  

size_t g_cCellOutline[3]
{
    ARRAYSIZE(g_krgFullCellOutline),
    ARRAYSIZE(g_krgSquareZCellOutline),
    ARRAYSIZE(g_krgHex1CellOutline)
};

CChord* g_krgpCellOutline[3]
{
    g_krgFullCellOutline,
    g_krgSquareZCellOutline,
    g_krgHex1CellOutline
};

bool CUCoord::GetOutline( __inout std::unordered_set<CChord> & Chords, EOutlineType eOutlineType ) const
{
    CUCoordFloat Origin = (CUCoordFloat)(*this);

    for (size_t i = 0; (g_cCellOutline[eOutlineType] > i); ++i)
    {
        Chords.insert( CChord(Origin + g_krgpCellOutline[eOutlineType][i].GetStart(), 
            Origin + g_krgpCellOutline[eOutlineType][i].GetEnd()));
    }

    return true;
}

int CUCoord::GetX() const {
    return m_rgnData[0];
}

void CUCoord::SetX(int nValue) {
    m_rgnData[0] = nValue;
}

int CUCoord::GetY() const {
    return m_rgnData[1];
}

void CUCoord::SetY(int nValue) {
    m_rgnData[1] = nValue;
}

int CUCoord::GetZ() const {
    return m_rgnData[2];
}

void CUCoord::SetZ(int nValue) {
    m_rgnData[2] = nValue;
}

CUCoord::operator CSCoord() const {
    CSCoord scoord;
    const int level = GetZ();
    const std::uint16_t invalidCoord = (std::numeric_limits<std::uint16_t>::max)();

    int levelOffset;
    int fileOffset;
    if ((level >= 0) && (static_cast<unsigned int>(level) < CBitBoard::NUM_LEVELS)) {
        scoord.m_nLevel = static_cast<std::uint16_t>(level);
        const unsigned int levelIndex = static_cast<unsigned int>(level);
        levelOffset = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH - CBitBoard::LEVEL_WIDTH[levelIndex]);
        fileOffset = static_cast<int>(CBitBoard::LEVEL_WIDTH[levelIndex]) - 1;
    } else {
        scoord.m_nLevel = invalidCoord;
        levelOffset = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
        fileOffset = 0;
    }

    const int doubleFile = GetX() - GetY() + fileOffset;
    if ((doubleFile & 1) != 0) {
        scoord.m_nFile = invalidCoord;
    } else {
        scoord.m_nFile = static_cast<std::uint16_t>(doubleFile >> 1);
    }

    const int doubleRank = GetX() + GetY() - fileOffset;
    if ((doubleRank & 1) != 0) {
        scoord.m_nRank = invalidCoord;
    } else {
        scoord.m_nRank = static_cast<std::uint16_t>((doubleRank >> 1) - levelOffset);
    }

    return scoord;
}

bool CUCoord::operator==(const CUCoord& other) const {
    if (m_rgnData[0] != other.m_rgnData[0]) {
        return false;
    }
    if (m_rgnData[1] != other.m_rgnData[1]) {
        return false;
    }
    if (m_rgnData[2] != other.m_rgnData[2]) {
        return false;
    }

    return true;
}

bool CUCoord::operator!=(const CUCoord& other) const {
    return !(*this == other);
}

CUCoord CUCoord::operator+(const CUCoord& other) const {
    CUCoord result;
    result.m_rgnData[0] = m_rgnData[0] + other.m_rgnData[0];
    result.m_rgnData[1] = m_rgnData[1] + other.m_rgnData[1];
    result.m_rgnData[2] = m_rgnData[2] + other.m_rgnData[2];
    return result;
}

CUCoord CUCoord::operator-(const CUCoord& other) const {
    CUCoord result;
    result.m_rgnData[0] = m_rgnData[0] - other.m_rgnData[0];
    result.m_rgnData[1] = m_rgnData[1] - other.m_rgnData[1];
    result.m_rgnData[2] = m_rgnData[2] - other.m_rgnData[2];
    return result;
}

CUCoord CUCoord::operator-() const {
    CUCoord result;
    result.m_rgnData[0] = -m_rgnData[0];
    result.m_rgnData[1] = -m_rgnData[1];
    result.m_rgnData[2] = -m_rgnData[2];
    return result;
}
