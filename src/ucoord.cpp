#include "ucoord.h"

#include <limits>

#include "bitboard.h"


CUCoord::CUCoord(int x, int y, int z) {
    setX(x);
    setY(y);
    setZ(z);
}

CUCoord::CUCoord(const CSCoord& scoord) {
    scoord.Validate();

    setZ(scoord.m_nLevel);
    const unsigned int levelIndex = static_cast<unsigned int>(scoord.m_nLevel);
    const int levelOffset = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH - CSCoord::LEVEL_WIDTH[levelIndex]);
    const int fileOffset = static_cast<int>(CSCoord::LEVEL_WIDTH[levelIndex]) - 1;
    setX(levelOffset + scoord.m_nFile + scoord.m_nRank);
    setY(levelOffset + scoord.m_nRank - scoord.m_nFile + fileOffset);
}

int CUCoord::getX() const {
    return m_rgnData[0];
}

void CUCoord::setX(int value) {
    m_rgnData[0] = value;
}

int CUCoord::getY() const {
    return m_rgnData[1];
}

void CUCoord::setY(int value) {
    m_rgnData[1] = value;
}

int CUCoord::getZ() const {
    return m_rgnData[2];
}

void CUCoord::setZ(int value) {
    m_rgnData[2] = value;
}

CUCoord::operator CSCoord() const {
    CSCoord scoord;
    const int level = getZ();
    const std::uint16_t invalidCoord = (std::numeric_limits<std::uint16_t>::max)();

    int levelOffset;
    int fileOffset;
    if ((level >= 0) && (static_cast<unsigned int>(level) < CSCoord::NUM_LEVELS)) {
        scoord.m_nLevel = static_cast<std::uint16_t>(level);
        const unsigned int levelIndex = static_cast<unsigned int>(level);
        levelOffset = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH - CSCoord::LEVEL_WIDTH[levelIndex]);
        fileOffset = static_cast<int>(CSCoord::LEVEL_WIDTH[levelIndex]) - 1;
    } else {
        scoord.m_nLevel = invalidCoord;
        levelOffset = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH);
        fileOffset = 0;
    }

    const int doubleFile = getX() - getY() + fileOffset;
    if ((doubleFile & 1) != 0) {
        scoord.m_nFile = invalidCoord;
    } else {
        scoord.m_nFile = static_cast<std::uint16_t>(doubleFile >> 1);
    }

    const int doubleRank = getX() + getY() - fileOffset;
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
