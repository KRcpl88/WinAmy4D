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
    CBitBoard() : m_ullBits(0) {}
    CBitBoard(BitBoardBits bits) : m_ullBits(bits) {}

    // Bit manipulation methods
    void SetBit(uint16_t i) { m_ullBits |= (1ULL << i); }
    void ClrBit(uint16_t i) { m_ullBits &= ~(1ULL << i); }
    bool TstBit(uint16_t i) const { return (m_ullBits & (1ULL << i)) != 0; }
    void ClearLowestBit() { m_ullBits &= m_ullBits - 1; }

    // Static mask constructors
    static CBitBoard SetMask(int i) { return CBitBoard(1ULL << i); }
    static CBitBoard ClrMask(int i) { return CBitBoard(~(1ULL << i)); }

    // Counting and scanning
    int CountBits() const {
#if HAVE___BUILTIN_POPCOUNTLL
        return __builtin_popcountll(m_ullBits);
#else
        BitBoardBits x = m_ullBits;
        x = x - ((x >> 1) & 0x5555555555555555ULL);
        x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
        return (int)((((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL) *
                      0x0101010101010101ULL) >>
                     56);
#endif
    }

    uint16_t FindSetBit() const {
#if HAVE___BUILTIN_CTZLL
        return static_cast<uint16_t>(__builtin_ctzll(m_ullBits));
#else
        // DeBruijn sequence method
        static const int index64[64] = {
            0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61,
            54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4,  62,
            46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45,
            25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5,  63};
        const BitBoardBits debruijn64 = 0x03f79d71b4cb0a89ULL;
        return static_cast<uint16_t>(
            index64[((m_ullBits ^ (m_ullBits - 1)) * debruijn64) >> 58]);
#endif
    }

    CSCoord FindSetBitCoord() const { return CSCoord(FindSetBit()); }

    // State queries
    bool IsEmpty() const { return m_ullBits == 0; }
    bool IsNotEmpty() const { return m_ullBits != 0; }
    explicit operator bool() const { return m_ullBits != 0; }

    // Raw bits access
    BitBoardBits GetBits() const { return m_ullBits; }

    // Bitwise operators
    friend bool operator==(const CBitBoard &lhs, const CBitBoard &rhs) {
        return lhs.m_ullBits == rhs.m_ullBits;
    }
    friend bool operator!=(const CBitBoard &lhs, const CBitBoard &rhs) {
        return lhs.m_ullBits != rhs.m_ullBits;
    }
    friend CBitBoard operator&(const CBitBoard &lhs, const CBitBoard &rhs) {
        return CBitBoard(lhs.m_ullBits & rhs.m_ullBits);
    }
    friend CBitBoard operator|(const CBitBoard &lhs, const CBitBoard &rhs) {
        return CBitBoard(lhs.m_ullBits | rhs.m_ullBits);
    }
    friend CBitBoard operator^(const CBitBoard &lhs, const CBitBoard &rhs) {
        return CBitBoard(lhs.m_ullBits ^ rhs.m_ullBits);
    }
    friend CBitBoard operator~(const CBitBoard &bb) {
        return CBitBoard(~bb.m_ullBits);
    }

    // Shift operators
    friend CBitBoard operator<<(const CBitBoard &bb, int shift) {
        return CBitBoard(bb.m_ullBits << shift);
    }
    friend CBitBoard operator>>(const CBitBoard &bb, int shift) {
        return CBitBoard(bb.m_ullBits >> shift);
    }

    // Compound assignment operators
    CBitBoard &operator|=(const CBitBoard &rhs) {
        m_ullBits |= rhs.m_ullBits;
        return *this;
    }
    CBitBoard &operator&=(const CBitBoard &rhs) {
        m_ullBits &= rhs.m_ullBits;
        return *this;
    }
    CBitBoard &operator^=(const CBitBoard &rhs) {
        m_ullBits ^= rhs.m_ullBits;
        return *this;
    }
    CBitBoard &operator<<=(int shift) {
        m_ullBits <<= shift;
        return *this;
    }
    CBitBoard &operator>>=(int shift) {
        m_ullBits >>= shift;
        return *this;
    }

    // Arithmetic (needed for magic bitboard multiplication)
    friend CBitBoard operator*(const CBitBoard &lhs, const CBitBoard &rhs) {
        return CBitBoard(lhs.m_ullBits * rhs.m_ullBits);
    }

    // Comparison with zero (for if(bitboard) patterns)
    friend bool operator!(const CBitBoard &bb) { return bb.m_ullBits == 0; }

  private:
    BitBoardBits m_ullBits;
};

#endif /* BITBOARD_H */
