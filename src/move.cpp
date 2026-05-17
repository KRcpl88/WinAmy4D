#include "move.h"

CMove::CMove(const CSCoord& from, const CSCoord& to, std::uint32_t flags)
    : m_from(from), m_to(to),
      m_bits(static_cast<std::uint32_t>(from.GetBitField()) |
             (static_cast<std::uint32_t>(to.GetBitField()) << 16) | flags) {
}

std::uint32_t CMove::GetBits() const {
    return m_bits;
}

const CSCoord& CMove::GetFromCoord() const {
    return m_from;
}

const CSCoord& CMove::GetToCoord() const {
    return m_to;
}

int CMove::GetFrom() const {
    return static_cast<int>(m_from);
}

int CMove::GetTo() const {
    return static_cast<int>(m_to);
}

CMove::operator std::uint32_t() const {
    return m_bits;
}

CMove CMove::operator|(std::uint32_t flags) const {
    CMove result = *this;
    result |= flags;
    return result;
}

std::uint32_t CMove::operator&(std::uint32_t flags) const {
    return m_bits & flags;
}

CMove& CMove::operator|=(std::uint32_t flags) {
    m_bits |= flags;
    return *this;
}
