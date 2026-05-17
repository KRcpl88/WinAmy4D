#pragma once

#include <cstdint>

#include "scoord.h"

class CMove {
public:
    CMove() = default;
    CMove(const CSCoord& from, const CSCoord& to, std::uint32_t flags);

    std::uint32_t GetBits() const;
    const CSCoord& GetFromCoord() const;
    const CSCoord& GetToCoord() const;
    int GetFrom() const;
    int GetTo() const;

    operator std::uint32_t() const;

    CMove operator|(std::uint32_t flags) const;
    std::uint32_t operator&(std::uint32_t flags) const;
    CMove& operator|=(std::uint32_t flags);

private:
    CSCoord m_from{};
    CSCoord m_to{};
    std::uint32_t m_bits{0};
};
