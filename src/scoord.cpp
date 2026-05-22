#include "scoord.h"
#include "ucoord.h"

#include <stdexcept>

#include "bitboard.h"

CSCoord::CSCoord(std::uint16_t level, std::uint16_t file, std::uint16_t rank)
    : m_nLevel(level), m_nRank(rank), m_nFile(file) {
    Validate();
}

CSCoord::CSCoord(std::uint16_t offset) {
    ValidateOffset(offset);

    unsigned int levelIndex = CBitBoard::NUM_LEVELS - 1;
    while (offset < CBitBoard::LEVEL_OFFSET[levelIndex]) {
        --levelIndex;
    }
    m_nLevel = static_cast<std::uint16_t>(levelIndex);

    const unsigned int levelOffset = CBitBoard::LEVEL_OFFSET[levelIndex];
    const unsigned int levelWidth = CBitBoard::LEVEL_WIDTH[levelIndex];
    m_nRank = static_cast<std::uint16_t>((offset - levelOffset) / levelWidth);
    m_nFile = static_cast<std::uint16_t>((offset - levelOffset) % levelWidth);
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

void CSCoord::ValidateOffset(std::uint16_t offset) {
    if (!IsValid(offset)) {
        throw std::out_of_range("CSCoord::ValidateOffset(offset) offset");
    }
}

bool CSCoord::IsValid() const {
    return IsValid(m_nLevel, m_nFile, m_nRank);
}

bool CSCoord::IsValid(std::uint16_t level, std::uint16_t file, std::uint16_t rank) {
    if (level >= CBitBoard::NUM_LEVELS) {
        return false;
    }

    const unsigned int levelWidth = CBitBoard::LEVEL_WIDTH[level];
    if ((rank >= levelWidth) || (file >= levelWidth)) {
        return false;
    }

    return true;
}

std::uint16_t CSCoord::BitOffset() const {
    if (m_nLevel >= CBitBoard::NUM_LEVELS) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) level");
    }

    const unsigned int levelWidth = CBitBoard::LEVEL_WIDTH[m_nLevel];
    if ((m_nFile >= levelWidth) || (m_nRank >= levelWidth)) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) coordinate");
    }

    return static_cast<std::uint16_t>(CBitBoard::LEVEL_OFFSET[m_nLevel] + m_nRank * levelWidth + m_nFile);
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

bool CSCoord::IsValid(std::uint16_t offset) {
    return offset < CBitBoard::SIZE;
}

CSCoord CSCoord::ReflectRank() const {
    const std::uint16_t maxRank = static_cast<std::uint16_t>(CBitBoard::LEVEL_WIDTH[m_nLevel] - 1U);
    return CSCoord(m_nLevel, m_nFile, static_cast<std::uint16_t>(maxRank - m_nRank));
}

CSCoord::operator int() const {
    Validate();
    return BitOffset();
}
