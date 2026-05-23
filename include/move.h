#pragma once

#include <cstdint>

#include "scoord.h"


struct SFromToIndex
{
    uint16_t nFromOffset;
    uint16_t nToOffset;

    SFromToIndex (uint16_t nFrom, uint16_t nTo) :
        nFromOffset(nFrom), nToOffset(nTo)
    {}

    friend bool operator==(const SFromToIndex &lhs, const SFromToIndex &rhs) {
        return (lhs.nFromOffset == rhs.nFromOffset) && (lhs.nToOffset == rhs.nToOffset);
    }    
};

class CMove {
public:
    static constexpr std::uint32_t FLAG_CAPTURE = (1u << 12);
    static constexpr std::uint32_t FLAG_SCASTLE = (1u << 13);
    static constexpr std::uint32_t FLAG_LCASTLE = (1u << 14);
    static constexpr std::uint32_t FLAG_PAWND = (1u << 15);
    static constexpr std::uint32_t FLAG_ENPASSANT = (1u << 28);
    static constexpr std::uint32_t PROMOTION_OFFSET = 29;
    static constexpr std::uint32_t PROMOTION_MASK = (7u << PROMOTION_OFFSET);
    static constexpr std::uint32_t FLAG_CANY = (FLAG_SCASTLE | FLAG_LCASTLE);
    static constexpr std::uint32_t FLAG_TACTICAL =
        (FLAG_CAPTURE | FLAG_ENPASSANT | PROMOTION_MASK);

    CMove() = default;
    CMove(const CSCoord& from, const CSCoord& to, std::uint32_t flags);

    const CSCoord& GetFromCoord() const;
    const CSCoord& GetToCoord() const;
    SFromToIndex GetFromToIndex() const;

    bool IsCapture() const;
    bool IsShortCastle() const;
    bool IsLongCastle() const;
    bool IsCastle() const;
    bool IsPawnDoublePush() const;
    bool IsEnPassant() const;
    bool HasPromotion() const;
    int GetPromotionType() const;
    bool IsTactical() const;

    void SetCapture(bool value = true);
    void SetShortCastle(bool value = true);
    void SetLongCastle(bool value = true);
    void SetPawnDoublePush(bool value = true);
    void SetEnPassant(bool value = true);
    void SetPromotionType(int promotionType);
    void ClearPromotion();

    bool operator==(const CMove& other) const;
    bool operator!=(const CMove& other) const;

private:
    bool HasFlag(std::uint32_t mask) const;
    void SetFlag(std::uint32_t mask, bool value);

    CSCoord m_From{};
    CSCoord m_To{};
    std::uint32_t m_dwBits{0};
};
