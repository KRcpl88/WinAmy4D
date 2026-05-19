#include "scoord.h"

#include <stdexcept>

#include "bitboard.h"

const int CSCoord::LEVEL_SIZE[1]{CSCoord::SIZE};
const int CSCoord::LEVEL_WIDTH[1]{8};
const int CSCoord::LEVEL_OFFSET[1]{0};

CSCoord::CSCoord(int level, int file, int rank)
    : m_nLevel(level), m_nRank(rank), m_nFile(file) {
    Validate();
}

CSCoord::CSCoord(int offset) {
    ValidateOffset(offset);

    m_nLevel = NUM_LEVELS - 1;
    while (offset < LEVEL_OFFSET[m_nLevel]) {
        --m_nLevel;
    }

    m_nRank = (offset - LEVEL_OFFSET[m_nLevel]) / LEVEL_WIDTH[m_nLevel];
    m_nFile = (offset - LEVEL_OFFSET[m_nLevel]) % LEVEL_WIDTH[m_nLevel];
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
    if ((level < 0) || (level >= NUM_LEVELS)) {
        return false;
    }

    if ((rank < 0) || (rank >= LEVEL_WIDTH[level]) || (file < 0) || (file >= LEVEL_WIDTH[level])) {
        return false;
    }

    return true;
}

int CSCoord::BitOffset() const {
    if ((m_nLevel < 0) || (m_nLevel >= NUM_LEVELS)) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) level");
    }

    if ((m_nFile < 0) || (m_nFile >= LEVEL_WIDTH[m_nLevel]) || (m_nRank < 0) || (m_nRank >= LEVEL_WIDTH[m_nLevel])) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) coordinate");
    }

    return LEVEL_OFFSET[m_nLevel] + m_nRank * LEVEL_WIDTH[m_nLevel] + m_nFile;
}

scoord_bitfield_t CSCoord::GetBitField() const {
    Validate();
    return static_cast<scoord_bitfield_t>((m_nLevel << 8) | (m_nRank << 4) | m_nFile);
}

bool CSCoord::IsValid(int offset) {
    return (offset < SIZE) && (offset >= 0);
}

CSCoord CSCoord::ReflectRank() const {
    int maxRank = LEVEL_WIDTH[m_nLevel] - 1;
    return CSCoord(m_nLevel, m_nFile, maxRank - m_nRank);
}

CSCoord::operator int() const {
    Validate();
    return BitOffset();
}

