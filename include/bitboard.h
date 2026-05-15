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

#include <stdint.h>

#define SetMask(i) (1ULL << (i))
#define ClrMask(i) (~SetMask(i))

#define SetBit(b, i) ((b) |= SetMask(i))
#define ClrBit(b, i) ((b) &= ClrMask(i))
#define TstBit(b, i) ((b) & SetMask(i))

typedef uint64_t BitBoardBits;

#ifdef __cplusplus
extern "C" {
#endif

#if HAVE___BUILTIN_POPCOUNTLL
#define CountBits(x) __builtin_popcountll(x)
#else
int CountBits(BitBoardBits);
#endif

#if HAVE___BUILTIN_CTZLL
#define FindSetBit(x) __builtin_ctzll(x)
#else
int FindSetBit(BitBoardBits);
#endif

#ifdef __cplusplus
}

// Undefine macros that conflict with CBitboard method names
#undef SetBit
#undef ClrBit
#undef TstBit

class CBitboard {
  public:
    CBitboard() : m_bits(0) {}
    CBitboard(BitBoardBits bits) : m_bits(bits) {}

    // Accessor methods matching the bitboard.h macros
    void SetBit(int i) { m_bits |= (1ULL << i); }
    void ClrBit(int i) { m_bits &= ~(1ULL << i); }
    bool TstBit(int i) const { return (m_bits & (1ULL << i)) != 0; }

    int CountBits() const;
    int FindSetBit() const;

    // Static mask helpers
    static BitBoardBits GetSetMask(int i) { return 1ULL << i; }
    static BitBoardBits GetClrMask(int i) { return ~(1ULL << i); }

    // State queries
    bool IsEmpty() const { return m_bits == 0; }
    bool IsNotEmpty() const { return m_bits != 0; }

    // Raw bits access
    BitBoardBits GetBits() const { return m_bits; }

    // Bitwise operators
    friend bool operator==(const CBitboard &lhs, const CBitboard &rhs) {
        return lhs.m_bits == rhs.m_bits;
    }
    friend bool operator!=(const CBitboard &lhs, const CBitboard &rhs) {
        return lhs.m_bits != rhs.m_bits;
    }
    friend CBitboard operator&(const CBitboard &lhs, const CBitboard &rhs) {
        return CBitboard(lhs.m_bits & rhs.m_bits);
    }
    friend CBitboard operator|(const CBitboard &lhs, const CBitboard &rhs) {
        return CBitboard(lhs.m_bits | rhs.m_bits);
    }
    friend CBitboard operator~(const CBitboard &bb) {
        return CBitboard(~bb.m_bits);
    }

    // Compound assignment operators
    CBitboard &operator|=(const CBitboard &rhs) {
        m_bits |= rhs.m_bits;
        return *this;
    }
    CBitboard &operator&=(const CBitboard &rhs) {
        m_bits &= rhs.m_bits;
        return *this;
    }

  private:
    BitBoardBits m_bits;
};

#endif /* __cplusplus */

#endif
