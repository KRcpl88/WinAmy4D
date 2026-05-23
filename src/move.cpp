#include "move.h"

CMove::CMove(const CSCoord& from, const CSCoord& to, std::uint32_t flags)
    : m_From(from), m_To(to),
      m_dwBits(static_cast<std::uint32_t>(from.GetBitField()) |
              (static_cast<std::uint32_t>(to.GetBitField()) << 16) | flags) {
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

void CMove::SetCapture(bool value) {
    SetFlag(FLAG_CAPTURE, value);
}

void CMove::SetShortCastle(bool value) {
    SetFlag(FLAG_SCASTLE, value);
}

void CMove::SetLongCastle(bool value) {
    SetFlag(FLAG_LCASTLE, value);
}

void CMove::SetPawnDoublePush(bool value) {
    SetFlag(FLAG_PAWND, value);
}

void CMove::SetEnPassant(bool value) {
    SetFlag(FLAG_ENPASSANT, value);
}

void CMove::SetPromotionType(int promotionType) {
    m_dwBits &= ~PROMOTION_MASK;
    m_dwBits |= (static_cast<std::uint32_t>(promotionType) << PROMOTION_OFFSET) &
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

bool CMove::HasFlag(std::uint32_t mask) const {
    return (m_dwBits & mask) != 0;
}

void CMove::SetFlag(std::uint32_t mask, bool value) {
    if (value) {
        m_dwBits |= mask;
    } else {
        m_dwBits &= ~mask;
    }
}
