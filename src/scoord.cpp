#include "scoord.h"

#include <stdexcept>

#include "bitboard.h"

const int CSCoord::LEVEL_SIZE[1]{64};
const int CSCoord::LEVEL_WIDTH[1]{8};
const int CSCoord::LEVEL_OFFSET[1]{0};

CSCoord::CSCoord(int level, int file, int rank)
    : Level(level), Rank(rank), File(file) {
    Validate();
}

CSCoord::CSCoord(int offset) {
    ValidateOffset(offset);

    Level = NUM_LEVELS - 1;
    while (offset < LEVEL_OFFSET[Level]) {
        --Level;
    }

    Rank = (offset - LEVEL_OFFSET[Level]) / LEVEL_WIDTH[Level];
    File = (offset - LEVEL_OFFSET[Level]) % LEVEL_WIDTH[Level];
}

CSCoord::CSCoord(scoord_bitfield_t bitfield) {
    File = bitfield & 0x0f;
    Rank = (bitfield >> 4) & 0x0f;
    Level = (bitfield >> 8) & 0x0f;
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
    return IsValid(Level, File, Rank);
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
    if ((Level < 0) || (Level >= NUM_LEVELS)) {
        throw std::out_of_range("BitBoard::BitOffset(Level, File, Rank) level");
    }

    if ((File < 0) || (File >= LEVEL_WIDTH[Level]) || (Rank < 0) || (Rank >= LEVEL_WIDTH[Level])) {
        throw std::out_of_range("BitBoard::BitOffset(Level, File, Rank) coordinate");
    }

    return LEVEL_OFFSET[Level] + Rank * LEVEL_WIDTH[Level] + File;
}

scoord_bitfield_t CSCoord::GetBitField() const {
    Validate();
    return static_cast<scoord_bitfield_t>((Level << 8) | (Rank << 4) | File);
}

bool CSCoord::IsValid(int offset) {
    return (offset < SIZE) && (offset >= 0);
}

CSCoord::operator int() const {
    Validate();
    return BitOffset();
}
