#include "ucoord.h"

#include "bitboard.h"


CUCoord::CUCoord(int x, int y, int z) {
    setX(x);
    setY(y);
    setZ(z);
}

CUCoord::CUCoord(const CSCoord& scoord) {
    scoord.Validate();

    setZ(scoord.Level);
    const int levelOffset = CSCoord::MAX_LEVEL_WIDTH - CSCoord::LEVEL_WIDTH[scoord.Level];
    const int fileOffset = CSCoord::LEVEL_WIDTH[scoord.Level] - 1;
    setX(levelOffset + scoord.File + scoord.Rank);
    setY(levelOffset + scoord.Rank - scoord.File + fileOffset);
}

int CUCoord::getX() const {
    return data[0];
}

void CUCoord::setX(int value) {
    data[0] = value;
}

int CUCoord::getY() const {
    return data[1];
}

void CUCoord::setY(int value) {
    data[1] = value;
}

int CUCoord::getZ() const {
    return data[2];
}

void CUCoord::setZ(int value) {
    data[2] = value;
}

CUCoord::operator CSCoord() const {
    CSCoord scoord;
    scoord.Level = getZ();

    int levelOffset;
    int fileOffset;
    if ((scoord.Level >= 0) && (scoord.Level < CSCoord::NUM_LEVELS)) {
        levelOffset = CSCoord::MAX_LEVEL_WIDTH - CSCoord::LEVEL_WIDTH[scoord.Level];
        fileOffset = CSCoord::LEVEL_WIDTH[scoord.Level] - 1;
    } else {
        levelOffset = CSCoord::MAX_LEVEL_WIDTH;
        fileOffset = 0;
    }

    const int doubleFile = getX() - getY() + fileOffset;
    if ((doubleFile & 1) != 0) {
        scoord.File = -1;
    } else {
        scoord.File = doubleFile >> 1;
    }

    const int doubleRank = getX() + getY() - fileOffset;
    if ((doubleRank & 1) != 0) {
        scoord.Rank = -1;
    } else {
        scoord.Rank = (doubleRank >> 1) - levelOffset;
    }

    return scoord;
}

bool CUCoord::operator==(const CUCoord& other) const {
    if (data[0] != other.data[0]) {
        return false;
    }
    if (data[1] != other.data[1]) {
        return false;
    }
    if (data[2] != other.data[2]) {
        return false;
    }

    return true;
}

bool CUCoord::operator!=(const CUCoord& other) const {
    return !(*this == other);
}

CUCoord CUCoord::operator+(const CUCoord& other) const {
    CUCoord result;
    result.data[0] = data[0] + other.data[0];
    result.data[1] = data[1] + other.data[1];
    result.data[2] = data[2] + other.data[2];
    return result;
}

CUCoord CUCoord::operator-(const CUCoord& other) const {
    CUCoord result;
    result.data[0] = data[0] - other.data[0];
    result.data[1] = data[1] - other.data[1];
    result.data[2] = data[2] - other.data[2];
    return result;
}

CUCoord CUCoord::operator-() const {
    CUCoord result;
    result.data[0] = -data[0];
    result.data[1] = -data[1];
    result.data[2] = -data[2];
    return result;
}
