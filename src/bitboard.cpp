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

#include "bitboard.h"

void CBitBoard::ClearLowestBit() {
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
        if (m_rgBits[w] != 0) {
            m_rgBits[w] &= m_rgBits[w] - 1;
            return;
        }
    }
}

CBitBoard CBitBoard::SetMask(uint16_t wI) {
    CBitBoard bb;
    bb.SetBit(wI);
    return bb;
}

CBitBoard CBitBoard::ClrMask(uint16_t wI) {
    CBitBoard bb;
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
        bb.m_rgBits[w] = ~0ULL;
    bb.ClrBit(wI);
    return bb;
}

int CBitBoard::CountBits() const {
    int count = 0;
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
#if HAVE___BUILTIN_POPCOUNTLL
        count += __builtin_popcountll(m_rgBits[w]);
#else
        BitBoardBits qwX = m_rgBits[w];
        qwX = qwX - ((qwX >> 1) & 0x5555555555555555ULL);
        qwX = (qwX & 0x3333333333333333ULL) + ((qwX >> 2) & 0x3333333333333333ULL);
        count += (int)((((qwX + (qwX >> 4)) & 0x0F0F0F0F0F0F0F0FULL) * 0x0101010101010101ULL) >> 56);
#endif
    }
    return count;
}

uint16_t CBitBoard::FindSetBit() const {
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

CSCoord CBitBoard::FindSetBitCoord() const {
    return CSCoord(FindSetBit());
}

bool CBitBoard::IsEmpty() const {
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w) {
        if (m_rgBits[w] != 0) return false;
    }
    return true;
}

bool CBitBoard::IsNotEmpty() const {
    return !IsEmpty();
}

CBitBoard::operator bool() const {
    return !IsEmpty();
}

bool operator==(const CBitBoard &lhs, const CBitBoard &rhs) {
    for (uint16_t w = 0; w < CBitBoard::SIZE_ULONGLONG; ++w) {
        if (lhs.m_rgBits[w] != rhs.m_rgBits[w]) return false;
    }
    return true;
}

bool operator!=(const CBitBoard &lhs, const CBitBoard &rhs) {
    return !(lhs == rhs);
}

CBitBoard operator&(const CBitBoard &lhs, const CBitBoard &rhs) {
    CBitBoard result;
    for (uint16_t w = 0; w < CBitBoard::SIZE_ULONGLONG; ++w)
        result.m_rgBits[w] = lhs.m_rgBits[w] & rhs.m_rgBits[w];
    return result;
}

CBitBoard operator|(const CBitBoard &lhs, const CBitBoard &rhs) {
    CBitBoard result;
    for (uint16_t w = 0; w < CBitBoard::SIZE_ULONGLONG; ++w)
        result.m_rgBits[w] = lhs.m_rgBits[w] | rhs.m_rgBits[w];
    return result;
}

CBitBoard operator^(const CBitBoard &lhs, const CBitBoard &rhs) {
    CBitBoard result;
    for (uint16_t w = 0; w < CBitBoard::SIZE_ULONGLONG; ++w)
        result.m_rgBits[w] = lhs.m_rgBits[w] ^ rhs.m_rgBits[w];
    return result;
}

CBitBoard operator~(const CBitBoard &bb) {
    CBitBoard result;
    for (uint16_t w = 0; w < CBitBoard::SIZE_ULONGLONG; ++w)
        result.m_rgBits[w] = ~bb.m_rgBits[w];
    return result;
}

CBitBoard operator<<(const CBitBoard &bb, int nShift) {
    CBitBoard result;
    const int wordShift = nShift / CBitBoard::ULONGLONG_SIZE_BITS;
    const int bitShift = nShift % CBitBoard::ULONGLONG_SIZE_BITS;
    for (int nI = CBitBoard::SIZE_ULONGLONG - 1; nI >= 0; --nI) {
        const int nSrc = nI - wordShift;
        if (nSrc >= 0) {
            result.m_rgBits[nI] = bb.m_rgBits[nSrc] << bitShift;
            if (bitShift > 0 && nSrc - 1 >= 0)
                result.m_rgBits[nI] |= bb.m_rgBits[nSrc - 1] >> (CBitBoard::ULONGLONG_SIZE_BITS - bitShift);
        }
    }
    return result;
}

CBitBoard operator>>(const CBitBoard &bb, int nShift) {
    CBitBoard result;
    const int wordShift = nShift / CBitBoard::ULONGLONG_SIZE_BITS;
    const int bitShift = nShift % CBitBoard::ULONGLONG_SIZE_BITS;
    for (int nI = 0; nI < CBitBoard::SIZE_ULONGLONG; ++nI) {
        const int nSrc = nI + wordShift;
        if (nSrc < CBitBoard::SIZE_ULONGLONG) {
            result.m_rgBits[nI] = bb.m_rgBits[nSrc] >> bitShift;
            if (bitShift > 0 && nSrc + 1 < CBitBoard::SIZE_ULONGLONG)
                result.m_rgBits[nI] |= bb.m_rgBits[nSrc + 1] << (CBitBoard::ULONGLONG_SIZE_BITS - bitShift);
        }
    }
    return result;
}

CBitBoard &CBitBoard::operator|=(const CBitBoard &rhs) {
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
        m_rgBits[w] |= rhs.m_rgBits[w];
    return *this;
}

CBitBoard &CBitBoard::operator&=(const CBitBoard &rhs) {
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
        m_rgBits[w] &= rhs.m_rgBits[w];
    return *this;
}

CBitBoard &CBitBoard::operator^=(const CBitBoard &rhs) {
    for (uint16_t w = 0; w < SIZE_ULONGLONG; ++w)
        m_rgBits[w] ^= rhs.m_rgBits[w];
    return *this;
}

CBitBoard &CBitBoard::operator<<=(int nShift) {
    *this = *this << nShift;
    return *this;
}

CBitBoard &CBitBoard::operator>>=(int nShift) {
    *this = *this >> nShift;
    return *this;
}

CBitBoard operator*(const CBitBoard &lhs, const CBitBoard &rhs) {
    CBitBoard result;
    result.m_rgBits[0] = lhs.m_rgBits[0] * rhs.m_rgBits[0];
    return result;
}

bool operator!(const CBitBoard &bb) {
    return bb.IsEmpty();
}
