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

#ifndef INLINE_H
#define INLINE_H

#include "bitboard.h"
#include "dbase.h"
#include "types.h"
#include <stdint.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

extern CBitBoard ShiftUpMask, ShiftDownMask;
extern CBitBoard ShiftLeftMask, ShiftRightMask;

static inline CBitBoard ShiftUp(CBitBoard x) { return (x << 8) & ShiftUpMask; }

static inline CBitBoard ShiftDown(CBitBoard x) {
    return (x >> 8) & ShiftDownMask;
}

static inline CBitBoard ShiftLeft(CBitBoard x) {
    return (x << 1) & ShiftLeftMask;
}

static inline CBitBoard ShiftRight(CBitBoard x) {
    return (x >> 1) & ShiftRightMask;
}

/**
 * Calculate the 'king distance' between two squares.
 */
static inline int KingDist(CSCoord sq1, CSCoord sq2) {
    int file_dist = ABS(sq1.m_nFile - sq2.m_nFile);
    int rank_dist = ABS(sq1.m_nRank - sq2.m_nRank);

    return MAX(file_dist, rank_dist);
}

/**
 * Calculate the 'minimum distance' between two squares.
 */
static inline int MinDist(CSCoord sq1, CSCoord sq2) {
    int file_dist = ABS(sq1.m_nFile - sq2.m_nFile);
    int rank_dist = ABS(sq1.m_nRank - sq2.m_nRank);

    return MIN(file_dist, rank_dist);
}

/**
 * Calculate the 'Manhattan distance' between two squares.
 */
static inline int ManhattanDist(CSCoord sq1, CSCoord sq2) {
    int file_dist = ABS(sq1.m_nFile - sq2.m_nFile);
    int rank_dist = ABS(sq1.m_nRank - sq2.m_nRank);

    return file_dist + rank_dist;
}

static inline int FileDist(CSCoord sq1, CSCoord sq2) {
    return ABS(sq1.m_nFile - sq2.m_nFile);
}

/**
 * Calculate the distance of 'sq' to any edge on the chessboard
 */
static inline int EdgeDist(CSCoord sq) {
    const int width = CSCoord::LEVEL_WIDTH[sq.m_nLevel];
    int filedist = MIN(sq.m_nFile, (width - 1) - sq.m_nFile);
    int rankdist = MIN(sq.m_nRank, (width - 1) - sq.m_nRank);

    return MAX(filedist, rankdist);
}

/**
 * Create a move from from square, to square and flags.
 */
static inline CMove make_move(CSCoord from, CSCoord to, int flags) {
    return CMove(from, to, static_cast<uint32_t>(flags));
}

static inline CMove make_move(int from, int to, int flags) {
    return make_move(CSCoord(from), CSCoord(to), flags);
}

/**
 * Create a promotion move from from square, to square and flags.
 */
static inline CMove make_promotion(CSCoord from, CSCoord to, int type, int flags) {
    CMove move = make_move(from, to, flags);
    move.SetPromotionType(type);
    return move;
}

static inline CMove make_promotion(int from, int to, int type, int flags) {
    CMove move = make_move(from, to, flags);
    move.SetPromotionType(type);
    return move;
}

/**
 * Returns if the square is a promotion square.
 */
static inline bool is_promo_square(CSCoord sq) {
    const int width = CSCoord::LEVEL_WIDTH[sq.m_nLevel];
    return sq.m_nRank == 0 || sq.m_nRank == (width - 1);
}

/*
 * Determine type of promotion from move
 */
static inline int8_t PromoType(CMove move) {
    return static_cast<int8_t>(move.GetPromotionType());
}

#endif /* INLINE_H */
