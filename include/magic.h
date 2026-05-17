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

/*
 * magic.h - functions to calculate rook & bishop attacks with magic bitboards.
 */

#ifndef MAGIC_H
#define MAGIC_H

#include "bitboard.h"

extern uint16_t rook_table_offsets[64];
extern BitBoardBits rook_table[102400];

extern uint16_t bishop_table_offsets[64];
extern BitBoardBits bishop_table[5248];

extern const BitBoardBits rook_blocker_mask[];
extern const BitBoardBits bishop_blocker_mask[];

extern const BitBoardBits rook_magics[];
extern const BitBoardBits bishop_magics[];

extern const uint8_t rook_index_bits[];
extern const uint8_t bishop_index_bits[];

/**
 * Calculate the attacks of a rook given occupied squares.
 */
static inline CBitBoard rook_attacks(int sq, CBitBoard occupied) {
    BitBoardBits blockers = occupied.GetBits() & rook_blocker_mask[sq];
    BitBoardBits magic_index =
        (blockers * rook_magics[sq]) >> (64 - rook_index_bits[sq]);
    return CBitBoard(rook_table[2 * rook_table_offsets[sq] + magic_index]);
}

/**
 * Calculate the attacks of a bishop given occupied squares.
 */
static inline CBitBoard bishop_attacks(int sq, CBitBoard occupied) {
    BitBoardBits blockers = occupied.GetBits() & bishop_blocker_mask[sq];
    BitBoardBits magic_index =
        (blockers * bishop_magics[sq]) >> (64 - bishop_index_bits[sq]);
    return CBitBoard(bishop_table[bishop_table_offsets[sq] + magic_index]);
}

void InitMagic(void);

#endif /* MAGIC_H */
