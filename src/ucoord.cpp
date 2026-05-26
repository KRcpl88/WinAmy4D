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
    const int levelOffset = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH - CBitBoard::LEVEL_WIDTH[levelIndex]);
    const int fileOffset = static_cast<int>(CBitBoard::LEVEL_WIDTH[levelIndex]) - 1;
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

// CUCoordFloat

CUCoordFloat::CUCoordFloat(double x, double y, double z) {
    setX(x);
    setY(y);
    setZ(z);
}

CUCoordFloat::CUCoordFloat(const CUCoord& coord) {
    setX(static_cast<double>(coord.getX()));
    setY(static_cast<double>(coord.getY()));
    setZ(static_cast<double>(coord.getZ()));
}

double CUCoordFloat::getX() const {
    return m_rgdData[0];
}

void CUCoordFloat::setX(double value) {
    m_rgdData[0] = value;
}

double CUCoordFloat::getY() const {
    return m_rgdData[1];
}

void CUCoordFloat::setY(double value) {
    m_rgdData[1] = value;
}

double CUCoordFloat::getZ() const {
    return m_rgdData[2];
}

void CUCoordFloat::setZ(double value) {
    m_rgdData[2] = value;
}

bool CUCoordFloat::operator==(const CUCoordFloat& other) const {
    if (m_rgdData[0] != other.m_rgdData[0]) {
        return false;
    }
    if (m_rgdData[1] != other.m_rgdData[1]) {
        return false;
    }
    if (m_rgdData[2] != other.m_rgdData[2]) {
        return false;
    }
    return true;
}

CUCoordFloat CUCoordFloat::operator+(const CUCoordFloat& other) const {
    CUCoordFloat result;
    result.m_rgdData[0] = m_rgdData[0] + other.m_rgdData[0];
    result.m_rgdData[1] = m_rgdData[1] + other.m_rgdData[1];
    result.m_rgdData[2] = m_rgdData[2] + other.m_rgdData[2];
    return result;
}

// CUCoordRotate

CUCoordRotate::CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                             double rotation0, double rotation1) {
    m_rgAxes[0] = axis0;
    m_rgAxes[1] = axis1;
    m_rgdwRotation[0] = rotation0;
    m_rgdwRotation[1] = rotation1;
}

// CChord

CChord::CChord(const CUCoord& start, const CUCoord& end) : m_Start(start), m_End(end) {}

bool CChord::operator==(const CChord& other) const {
    return (m_Start == other.m_Start) && (m_End == other.m_End);
}

// CPoly

CPoly::CPoly(const CUCoordFloat& p0, const CUCoordFloat& p1, const CUCoordFloat& p2) {
    m_rfPoints[0] = p0;
    m_rfPoints[1] = p1;
    m_rfPoints[2] = p2;
}

bool CPoly::operator==(const CPoly& other) const {
    if (!(m_rfPoints[0] == other.m_rfPoints[0])) {
        return false;
    }
    if (!(m_rfPoints[1] == other.m_rfPoints[1])) {
        return false;
    }
    if (!(m_rfPoints[2] == other.m_rfPoints[2])) {
        return false;
    }
    return true;
}
