#include "hcoord.h"

#include <stdexcept>

#include "bitboard.h"

const int CHCoord::Relu16[15]{0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
const int CHCoord::NegRelu16[15]{7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0};

CHCoord::CHCoord(int level, int file, int rank)
    : m_nLevel(level), m_nRank(rank), m_nFile(file) {
}

CHCoord::CHCoord(const CSCoord& scoord) {
    scoord.Validate();

    m_nLevel = scoord.m_nRank + Relu16[scoord.m_nLevel];
    m_nRank = scoord.m_nRank + NegRelu16[scoord.m_nLevel];
    m_nFile = scoord.m_nFile + Relu16[scoord.m_nLevel];
}

void CHCoord::Validate() const {
    if (!IsValid()) {
        throw std::out_of_range("CHCoord::Validate()");
    }
}

bool CHCoord::IsValid() const {
    return IsValid(m_nLevel, m_nFile, m_nRank);
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
    ret.m_nLevel = 7 + m_nLevel - m_nRank;
    ret.m_nFile = m_nFile - NegRelu16[m_nRank + 7 - m_nLevel];
    ret.m_nRank = m_nLevel - NegRelu16[m_nRank + 7 - m_nLevel];
    return ret;
}
