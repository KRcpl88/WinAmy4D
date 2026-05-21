#include "scoord.h"
#include "ucoord.h"

#include <stdexcept>

#include "bitboard.h"

const unsigned int CSCoord::LEVEL_SIZE[1]{CSCoord::SIZE};
const unsigned int CSCoord::LEVEL_WIDTH[1]{8U};
const unsigned int CSCoord::LEVEL_OFFSET[1]{0U};

CSCoord::CSCoord(int level, int file, int rank)
    : m_nLevel(level), m_nRank(rank), m_nFile(file) {
    Validate();
}

CSCoord::CSCoord(int offset) {
    ValidateOffset(offset);

    unsigned int levelIndex = NUM_LEVELS - 1;
    while (offset < static_cast<int>(LEVEL_OFFSET[levelIndex])) {
        --levelIndex;
    }
    m_nLevel = static_cast<int>(levelIndex);

    const int levelOffset = static_cast<int>(LEVEL_OFFSET[levelIndex]);
    const int levelWidth = static_cast<int>(LEVEL_WIDTH[levelIndex]);
    m_nRank = (offset - levelOffset) / levelWidth;
    m_nFile = (offset - levelOffset) % levelWidth;
}

CSCoord::CSCoord(scoord_bitfield_t bitfield) {
    m_nFile = bitfield & 0x0f;
    m_nRank = (bitfield >> 4) & 0x0f;
    m_nLevel = (bitfield >> 8) & 0x0f;
    Validate();
}

void CSCoord::Validate() const {
    if (!IsValid()) {
        throw std::out_of_range("CSCoord::Validate()");
    }
}

void CSCoord::ValidateOffset(int offset) {
    if (!IsValid(offset)) {
        throw std::out_of_range("CSCoord::ValidateOffset(offset) offset");
    }
}

bool CSCoord::IsValid() const {
    return IsValid(m_nLevel, m_nFile, m_nRank);
}

bool CSCoord::IsValid(int level, int file, int rank) {
    if (level < 0) {
        return false;
    }

    const unsigned int levelIndex = static_cast<unsigned int>(level);
    if (levelIndex >= NUM_LEVELS) {
        return false;
    }

    const int levelWidth = static_cast<int>(LEVEL_WIDTH[levelIndex]);

    if ((rank < 0) || (rank >= levelWidth) || (file < 0) || (file >= levelWidth)) {
        return false;
    }

    return true;
}

int CSCoord::BitOffset() const {
    if (m_nLevel < 0) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) level");
    }

    const unsigned int levelIndex = static_cast<unsigned int>(m_nLevel);
    if (levelIndex >= NUM_LEVELS) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) level");
    }

    const int levelWidth = static_cast<int>(LEVEL_WIDTH[levelIndex]);
    if ((m_nFile < 0) || (m_nFile >= levelWidth) || (m_nRank < 0) || (m_nRank >= levelWidth)) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) coordinate");
    }

    return static_cast<int>(LEVEL_OFFSET[levelIndex]) + m_nRank * levelWidth + m_nFile;
}

scoord_bitfield_t CSCoord::GetBitField() const {
    Validate();
    return static_cast<scoord_bitfield_t>((m_nLevel << 8) | (m_nRank << 4) | m_nFile);
}

// step in the given direction, returning an invalid coordinate if the result is out of bounds
CSCoord CSCoord::Step(CUCoord Direction) const
{
    return (CSCoord)(CUCoord(*this) + Direction);
}

bool CSCoord::IsValid(int offset) {
    return (offset >= 0) && (static_cast<unsigned int>(offset) < SIZE);
}

CSCoord CSCoord::ReflectRank() const {
    int maxRank = static_cast<int>(LEVEL_WIDTH[static_cast<unsigned int>(m_nLevel)]) - 1;
    return CSCoord(m_nLevel, m_nFile, maxRank - m_nRank);
}

CSCoord::operator int() const {
    Validate();
    return BitOffset();
}

