#include "hcoord.h"

#include <stdexcept>

#include "bitboard.h"


CHCoord::CHCoord(int level, int file, int rank)
    : Level(level), Rank(rank), File(file) {
}

CHCoord::CHCoord(const CSCoord& scoord) {
    scoord.Validate();

    Level = scoord.Rank + Relu16[scoord.Level];
    Rank = scoord.Rank + NegRelu16[scoord.Level];
    File = scoord.File + Relu16[scoord.Level];
}

void CHCoord::Validate() const {
    if (!IsValid()) {
        throw std::out_of_range("CHCoord::Validate()");
    }
}

bool CHCoord::IsValid() const {
    return IsValid(Level, File, Rank);
}

bool CHCoord::IsValid(int level, int file, int rank) {
    if ((level < 0) || (level >= CSCoord::MAX_LEVEL_WIDTH)) {
        return false;
    }

    if ((rank < 0) || (rank >= CSCoord::MAX_LEVEL_WIDTH)) {
        return false;
    }

    if (rank < level) {
        if ((file < (CSCoord::MAX_LEVEL_WIDTH - RankWidth(level, rank))) || (file >= CSCoord::MAX_LEVEL_WIDTH)) {
            return false;
        }
    } else {
        if ((file < 0) || (file >= (CSCoord::MAX_LEVEL_WIDTH - RankWidth(level, rank)))) {
            return false;
        }
    }

    return true;
}

int CHCoord::RankWidth(int level, int rank) {
    return CSCoord::MAX_LEVEL_WIDTH - Relu16[7 + rank - level] - NegRelu16[7 + rank - level];
}

bool CHCoord::IsValid(int offset) {
    return (offset < CSCoord::SIZE) && (offset >= 0);
}

CHCoord::operator int() const {
    Validate();
    CSCoord sc = static_cast<CSCoord>(*this);
    return sc.BitOffset();
}

CHCoord::operator CSCoord() const {
    CSCoord ret;
    ret.Level = 7 + Level - Rank;
    ret.File = File - NegRelu16[Rank + 7 - Level];
    ret.Rank = Level - NegRelu16[Rank + 7 - Level];
    return ret;
}
