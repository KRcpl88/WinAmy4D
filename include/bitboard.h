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
    static constexpr uint16_t SIZE = 343U;
    static constexpr uint16_t BITBOARD_SIZE_BITS = 9U;  // ceil(log2(SIZE))
    static constexpr uint16_t ULONGLONG_SIZE_BITS =
        static_cast<uint16_t>(sizeof(std::uint64_t) * 8U);
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
    void ClearLowestBit() {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
            if (m_rgBits[w] != 0) {
                m_rgBits[w] &= m_rgBits[w] - 1;
                return;
            }
        }
    }

    // Static mask constructors
    static CBitBoard SetMask(uint16_t i) {
        CBitBoard bb;
        bb.SetBit(i);
        return bb;
    }
    static CBitBoard ClrMask(uint16_t i) {
        CBitBoard bb;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            bb.m_rgBits[w] = ~0ULL;
        bb.ClrBit(i);
        return bb;
    }

    // Counting and scanning
    int CountBits() const {
        int count = 0;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
#if HAVE___BUILTIN_POPCOUNTLL
            count += __builtin_popcountll(m_rgBits[w]);
#else
            BitBoardBits x = m_rgBits[w];
            x = x - ((x >> 1) & 0x5555555555555555ULL);
            x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
            count += (int)((((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL) *
                            0x0101010101010101ULL) >>
                           56);
#endif
        }
        return count;
    }

    uint16_t FindSetBit() const {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
            if (m_rgBits[w] != 0) {
                BitBoardBits word = m_rgBits[w];
                uint16_t bit = 0;
                while ((word & 1ULL) == 0) {
                    word >>= 1;
                    ++bit;
                }
                return static_cast<uint16_t>(w * ULONGLONG_SIZE_BITS + bit);
            }
        }
        return 0;  // undefined if empty
    }

    CSCoord FindSetBitCoord() const { return CSCoord(FindSetBit()); }

    // State queries
    bool IsEmpty() const {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
            if (m_rgBits[w] != 0) return false;
        }
        return true;
    }
    bool IsNotEmpty() const { return !IsEmpty(); }
    explicit operator bool() const { return !IsEmpty(); }


    // Bitwise operators
    friend bool operator==(const CBitBoard &lhs, const CBitBoard &rhs) {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
            if (lhs.m_rgBits[w] != rhs.m_rgBits[w]) return false;
        }
        return true;
    }
    friend bool operator!=(const CBitBoard &lhs, const CBitBoard &rhs) {
        return !(lhs == rhs);
    }
    friend CBitBoard operator&(const CBitBoard &lhs, const CBitBoard &rhs) {
        CBitBoard result;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            result.m_rgBits[w] = lhs.m_rgBits[w] & rhs.m_rgBits[w];
        return result;
    }
    friend CBitBoard operator|(const CBitBoard &lhs, const CBitBoard &rhs) {
        CBitBoard result;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            result.m_rgBits[w] = lhs.m_rgBits[w] | rhs.m_rgBits[w];
        return result;
    }
    friend CBitBoard operator^(const CBitBoard &lhs, const CBitBoard &rhs) {
        CBitBoard result;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            result.m_rgBits[w] = lhs.m_rgBits[w] ^ rhs.m_rgBits[w];
        return result;
    }
    friend CBitBoard operator~(const CBitBoard &bb) {
        CBitBoard result;
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            result.m_rgBits[w] = ~bb.m_rgBits[w];
        return result;
    }

    // Shift operators (multi-word)
    friend CBitBoard operator<<(const CBitBoard &bb, int shift) {
        CBitBoard result;
        const int wordShift = shift / ULONGLONG_SIZE_BITS;
        const int bitShift = shift % ULONGLONG_SIZE_BITS;
        for (int i = SIZE_ULONGLONG - 1; i >= 0; --i) {
            const int src = i - wordShift;
            if (src >= 0) {
                result.m_rgBits[i] = bb.m_rgBits[src] << bitShift;
                if (bitShift > 0 && src - 1 >= 0)
                    result.m_rgBits[i] |=
                        bb.m_rgBits[src - 1] >> (ULONGLONG_SIZE_BITS - bitShift);
            }
        }
        return result;
    }
    friend CBitBoard operator>>(const CBitBoard &bb, int shift) {
        CBitBoard result;
        const int wordShift = shift / ULONGLONG_SIZE_BITS;
        const int bitShift = shift % ULONGLONG_SIZE_BITS;
        for (int i = 0; i < SIZE_ULONGLONG; ++i) {
            const int src = i + wordShift;
            if (src < SIZE_ULONGLONG) {
                result.m_rgBits[i] = bb.m_rgBits[src] >> bitShift;
                if (bitShift > 0 && src + 1 < SIZE_ULONGLONG)
                    result.m_rgBits[i] |=
                        bb.m_rgBits[src + 1] << (ULONGLONG_SIZE_BITS - bitShift);
            }
        }
        return result;
    }

    // Compound assignment operators
    CBitBoard &operator|=(const CBitBoard &rhs) {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            m_rgBits[w] |= rhs.m_rgBits[w];
        return *this;
    }
    CBitBoard &operator&=(const CBitBoard &rhs) {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            m_rgBits[w] &= rhs.m_rgBits[w];
        return *this;
    }
    CBitBoard &operator^=(const CBitBoard &rhs) {
        for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
            m_rgBits[w] ^= rhs.m_rgBits[w];
        return *this;
    }
    CBitBoard &operator<<=(int shift) {
        *this = *this << shift;
        return *this;
    }
    CBitBoard &operator>>=(int shift) {
        *this = *this >> shift;
        return *this;
    }

    // Arithmetic (needed for magic bitboard multiplication, operates on first word)
    friend CBitBoard operator*(const CBitBoard &lhs, const CBitBoard &rhs) {
        CBitBoard result;
        result.m_rgBits[0] = lhs.m_rgBits[0] * rhs.m_rgBits[0];
        return result;
    }

    // Comparison with zero (for if(bitboard) patterns)
    friend bool operator!(const CBitBoard &bb) { return bb.IsEmpty(); }

  private:
    BitBoardBits m_rgBits[SIZE_ULONGLONG];
};

#endif /* BITBOARD_H */
