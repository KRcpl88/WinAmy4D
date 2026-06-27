#include "scoord.h"
#include "ucoord.h"

#include <stdexcept>

#include "bitboard.h"

CSCoordBase::CSCoordBase(std::uint16_t wLevel, std::uint16_t file, std::uint16_t wRank)
    : m_nLevel(wLevel), m_nRank(wRank), m_nFile(file) {
    Validate();
}

CSCoord::CSCoord(std::uint16_t wLevel, std::uint16_t file, std::uint16_t wRank)
    : CSCoordBase(wLevel, file, wRank) {
}

CSCoord::CSCoord(std::uint16_t wOffset) {
    ValidateOffset(wOffset);

    unsigned int dwLevelIndex = CBitBoard::NUM_LEVELS - 1;
    while (wOffset < CBitBoard::LEVEL_OFFSET[dwLevelIndex]) {
        --dwLevelIndex;
    }
    m_nLevel = static_cast<std::uint16_t>(dwLevelIndex);

    const unsigned int dwLevelOffset = CBitBoard::LEVEL_OFFSET[dwLevelIndex];
    const unsigned int dwLevelWidth = CBitBoard::LEVEL_WIDTH[dwLevelIndex];
    m_nRank = static_cast<std::uint16_t>((wOffset - dwLevelOffset) / dwLevelWidth);
    m_nFile = static_cast<std::uint16_t>((wOffset - dwLevelOffset) % dwLevelWidth);
}

CSCoord::CSCoord(scoord_bitfield_t bitfield) {
    m_nFile = bitfield & 0x0f;
    m_nRank = (bitfield >> 4) & 0x0f;
    m_nLevel = (bitfield >> 8) & 0x0f;
    Validate();
}

void CSCoordBase::Validate() const {
    if (!IsValid()) {
        throw std::out_of_range("CSCoordBase::Validate()");
    }
}

void CSCoord::ValidateOffset(std::uint16_t wOffset) {
    if (!IsValid(wOffset)) {
        throw std::out_of_range("CSCoord::ValidateOffset(offset) offset");
    }
}

bool CSCoordBase::IsValid() const {
    return IsValid(m_nLevel, m_nFile, m_nRank);
}

bool CSCoordBase::IsValid(std::uint16_t wLevel, std::uint16_t file, std::uint16_t wRank) {
    if (wLevel >= CBitBoard::NUM_LEVELS) {
        return false;
    }

    const unsigned int dwLevelWidth = CBitBoard::LEVEL_WIDTH[wLevel];
    if ((wRank >= dwLevelWidth) || (file >= dwLevelWidth)) {
        return false;
    }

    return true;
}

std::uint16_t CSCoord::BitOffset() const {
    if (m_nLevel >= CBitBoard::NUM_LEVELS) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) level");
    }

    const unsigned int dwLevelWidth = CBitBoard::LEVEL_WIDTH[m_nLevel];
    if ((m_nFile >= dwLevelWidth) || (m_nRank >= dwLevelWidth)) {
        throw std::out_of_range("BitBoard::BitOffset(m_nLevel, m_nFile, m_nRank) coordinate");
    }

    return static_cast<std::uint16_t>(CBitBoard::LEVEL_OFFSET[m_nLevel] + m_nRank * dwLevelWidth + m_nFile);
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

bool CSCoord::IsValid(std::uint16_t wOffset) {
    return wOffset < CBitBoard::SIZE;
}

CSCoord CSCoord::ReflectRank() const {
    const std::uint16_t wMaxRank = static_cast<std::uint16_t>(CBitBoard::LEVEL_WIDTH[m_nLevel] - 1U);
    return CSCoord(m_nLevel, m_nFile, static_cast<std::uint16_t>(wMaxRank - m_nRank));
}

CSCoord::operator int() const {
    Validate();
    return BitOffset();
}

bool CSCoordBase::operator==(const CSCoordBase& other) const {
    return m_nLevel == other.m_nLevel
        && m_nFile  == other.m_nFile
        && m_nRank  == other.m_nRank;
}

bool CSCoordBase::operator!=(const CSCoordBase& other) const {
    return !(*this == other);
}
