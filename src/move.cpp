#include "move.h"

CMove::CMove(const CSCoord& from, const CSCoord& to, std::uint32_t dwFlags)
    : m_From(from), m_To(to),
      m_dwBits(static_cast<std::uint32_t>(from.GetBitField()) |
              (static_cast<std::uint32_t>(to.GetBitField()) << 16) | dwFlags) {
}

const CSCoord& CMove::GetFromCoord() const {
    return m_From;
}

const CSCoord& CMove::GetToCoord() const {
    return m_To;
}


SFromToIndex CMove::GetFromToIndex() const {
    SFromToIndex RetVal(m_From.BitOffset(), m_To.BitOffset());
    
    return RetVal;
}

bool CMove::IsCapture() const {
    return HasFlag(FLAG_CAPTURE);
}

bool CMove::IsShortCastle() const {
    return HasFlag(FLAG_SCASTLE);
}

bool CMove::IsLongCastle() const {
    return HasFlag(FLAG_LCASTLE);
}

bool CMove::IsCastle() const {
    return HasFlag(FLAG_CANY);
}

bool CMove::IsPawnDoublePush() const {
    return HasFlag(FLAG_PAWND);
}

bool CMove::IsEnPassant() const {
    return HasFlag(FLAG_ENPASSANT);
}

bool CMove::HasPromotion() const {
    return HasFlag(PROMOTION_MASK);
}

int CMove::GetPromotionType() const {
    return static_cast<int>((m_dwBits & PROMOTION_MASK) >> PROMOTION_OFFSET);
}

bool CMove::IsTactical() const {
    return HasFlag(FLAG_TACTICAL);
}

void CMove::SetCapture(bool fValue) {
    SetFlag(FLAG_CAPTURE, fValue);
}

void CMove::SetShortCastle(bool fValue) {
    SetFlag(FLAG_SCASTLE, fValue);
}

void CMove::SetLongCastle(bool fValue) {
    SetFlag(FLAG_LCASTLE, fValue);
}

void CMove::SetPawnDoublePush(bool fValue) {
    SetFlag(FLAG_PAWND, fValue);
}

void CMove::SetEnPassant(bool fValue) {
    SetFlag(FLAG_ENPASSANT, fValue);
}

void CMove::SetPromotionType(int nPromotionType) {
    m_dwBits &= ~PROMOTION_MASK;
    m_dwBits |= (static_cast<std::uint32_t>(nPromotionType) << PROMOTION_OFFSET) &
              PROMOTION_MASK;
}

void CMove::ClearPromotion() {
    m_dwBits &= ~PROMOTION_MASK;
}

bool CMove::operator==(const CMove& other) const {
    return m_dwBits == other.m_dwBits;
}

bool CMove::operator!=(const CMove& other) const {
    return !(*this == other);
}

bool CMove::HasFlag(std::uint32_t dwMask) const {
    return (m_dwBits & dwMask) != 0;
}

void CMove::SetFlag(std::uint32_t dwMask, bool fValue) {
    if (fValue) {
        m_dwBits |= dwMask;
    } else {
        m_dwBits &= ~dwMask;
    }
}
