/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef BITBOARD_H
#define BITBOARD_H

#include "amy.h"
#include "config.h"
#include "scoord.h"

#include <stdint.h>

typedef uint64_t BitBoardBits;

class CBitBoard {
  public:
    static constexpr uint16_t NUM_LEVELS = 15U;
    static constexpr uint16_t MAX_LEVEL_WIDTH = 8U;
    static constexpr uint16_t SIZE = 344U;
    static constexpr uint16_t ULONGLONG_SIZE_BITS = static_cast<uint16_t>(sizeof(std::uint64_t) * 8U);
    static constexpr uint16_t SIZE_ULONGLONG = (SIZE + ULONGLONG_SIZE_BITS - 1U) / ULONGLONG_SIZE_BITS;
    static constexpr uint16_t LEVEL_SIZE[NUM_LEVELS] = {1, 4, 9, 16, 25, 36, 49, 64, 49, 36, 25, 16, 9, 4, 1};
    static constexpr uint16_t LEVEL_WIDTH[NUM_LEVELS] = {1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1};
    static constexpr uint16_t LEVEL_OFFSET[NUM_LEVELS] = {0U, 1, 5, 14, 30, 55, 91, 140, 204, 253, 289, 314, 330, 339, 343};

    CBitBoard() : m_rgBits{} {}
    explicit CBitBoard(BitBoardBits bits) : m_rgBits{bits} {}

    // Bit manipulation methods
    void SetBit(uint16_t i) {
        m_rgBits[i / ULONGLONG_SIZE_BITS] |= (1ULL << (i % ULONGLONG_SIZE_BITS));
    }
    void ClrBit(uint16_t i) {
        m_rgBits[i / ULONGLONG_SIZE_BITS] &= ~(1ULL << (i % ULONGLONG_SIZE_BITS));
    }
    bool TstBit(uint16_t i) const {
        return (m_rgBits[i / ULONGLONG_SIZE_BITS] & (1ULL << (i % ULONGLONG_SIZE_BITS))) != 0;
    }
    void ClearLowestBit();

    // Static mask constructors
    static CBitBoard SetMask(uint16_t i);
    static CBitBoard ClrMask(uint16_t i);

    // Counting and scanning
    int CountBits() const;

    uint16_t FindSetBit() const;

    CSCoord FindSetBitCoord() const;

    // State queries
    bool IsEmpty() const;
    bool IsNotEmpty() const;
    explicit operator bool() const;


    // Bitwise operators
    friend bool operator==(const CBitBoard &lhs, const CBitBoard &rhs);
    friend bool operator!=(const CBitBoard &lhs, const CBitBoard &rhs);
    friend CBitBoard operator&(const CBitBoard &lhs, const CBitBoard &rhs);
    friend CBitBoard operator|(const CBitBoard &lhs, const CBitBoard &rhs);
    friend CBitBoard operator^(const CBitBoard &lhs, const CBitBoard &rhs);
    friend CBitBoard operator~(const CBitBoard &bb);

    // Shift operators (multi-word)
    friend CBitBoard operator<<(const CBitBoard &bb, int shift);
    friend CBitBoard operator>>(const CBitBoard &bb, int shift);

    // Compound assignment operators
    CBitBoard &operator|=(const CBitBoard &rhs);
    CBitBoard &operator&=(const CBitBoard &rhs);
    CBitBoard &operator^=(const CBitBoard &rhs);
    CBitBoard &operator<<=(int shift);
    CBitBoard &operator>>=(int shift);

    // Arithmetic (needed for magic bitboard multiplication, operates on first word)
    friend CBitBoard operator*(const CBitBoard &lhs, const CBitBoard &rhs);

    // Comparison with zero (for if(bitboard) patterns)
    friend bool operator!(const CBitBoard &bb);

  private:
    BitBoardBits m_rgBits[SIZE_ULONGLONG];
};

#endif /* BITBOARD_H */
