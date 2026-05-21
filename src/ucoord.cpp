#include "ucoord.h"

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
    scoord.m_nLevel = getZ();

    int levelOffset;
    int fileOffset;
    if ((scoord.m_nLevel >= 0) &&
        (static_cast<unsigned int>(scoord.m_nLevel) < CSCoord::NUM_LEVELS)) {
        const unsigned int levelIndex = static_cast<unsigned int>(scoord.m_nLevel);
        levelOffset = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH - CSCoord::LEVEL_WIDTH[levelIndex]);
        fileOffset = static_cast<int>(CSCoord::LEVEL_WIDTH[levelIndex]) - 1;
    } else {
        levelOffset = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH);
        fileOffset = 0;
    }

    const int doubleFile = getX() - getY() + fileOffset;
    if ((doubleFile & 1) != 0) {
        scoord.m_nFile = -1;
    } else {
        scoord.m_nFile = doubleFile >> 1;
    }

    const int doubleRank = getX() + getY() - fileOffset;
    if ((doubleRank & 1) != 0) {
        scoord.m_nRank = -1;
    } else {
        scoord.m_nRank = (doubleRank >> 1) - levelOffset;
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
