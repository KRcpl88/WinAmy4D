#include "hcoord.h"

#include <stdexcept>

#include "bitboard.h"

const int CHCoord::Relu16[15]{0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
const int CHCoord::NegRelu16[15]{7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0};

CHCoord::CHCoord(int nLevel, int file, int nRank)
    : m_nLevel(nLevel), m_nRank(nRank), m_nFile(file) {
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

bool CHCoord::IsValid(int nLevel, int file, int nRank) {
    const int nMaxLevelWidth = static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH);
    if ((nLevel < 0) || (nLevel >= nMaxLevelWidth)) {
        return false;
    }

    if ((nRank < 0) || (nRank >= nMaxLevelWidth)) {
        return false;
    }

    if (nRank < nLevel) {
        if ((file < (nMaxLevelWidth - RankWidth(nLevel, nRank))) || (file >= nMaxLevelWidth)) {
            return false;
        }
    } else {
        if ((file < 0) || (file >= (nMaxLevelWidth - RankWidth(nLevel, nRank)))) {
            return false;
        }
    }

    return true;
}

int CHCoord::RankWidth(int nLevel, int nRank) {
    return static_cast<int>(CBitBoard::MAX_LEVEL_WIDTH) - Relu16[7 + nRank - nLevel] -
           NegRelu16[7 + nRank - nLevel];
}

bool CHCoord::IsValid(int nOffset) {
    return (nOffset >= 0) && (static_cast<unsigned int>(nOffset) < CBitBoard::SIZE);
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
