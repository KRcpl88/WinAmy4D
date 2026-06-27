/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    all rights reserved.

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
 * dbase.c - global database manipulation routines
 */

#include <stdint.h>
#include <string.h>

#include "dbase.h"
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "mates.h"
#include "recog.h"
#include "safe_malloc.h"
#include "scoord.h"
#include "ucoord.h"
#include "search.h"
#include "swap.h"
#include "types.h"
#include "utils.h"
#include "ucoord.h"
#include "move.h"

#define INITIAL_GAME_LOG_SIZE 128 /* Initial size of game history */

/* Maximum number of EPD ops we attempt to parse */
#define MAX_EPD_OPS 15

bool CPosition::InCheck(int nSide) const {
    return (bool)(m_rgAtkFr[m_rgKingSq[nSide].BitOffset()] & m_rgMask[!nSide][0]);
}

bool CPosition::IsPassed(const CSCoord& sqCoord, int nSide) const {
    const uint16_t wSq = sqCoord.BitOffset();
    if (nSide == White)
        return !(m_rgMask[Black][Pawn] & PassedMaskW[wSq]);
    else
        return !(m_rgMask[White][Pawn] & PassedMaskB[wSq]);
}

/*
 * Names of pieces (language dependent)
 */
char PieceName[] = {' ', 'P', 'N', 'B', 'R', 'Q', 'K'};

/*
 * material Values of Pieces
 */

int Value[] = {0, 1000, 3500, 3500, 5500, 11000, 0};

/*
 * Masks for castle rights:
 */

const int8_t CastleMask[2][2] = {
    {0x01, 0x02}, /* White can castle king/queenp->GetTurn() */
    {0x04, 0x08}  /* dito for black */
};


// ---------------------------------------------------------------------------
// Attack deltas  (3-D board, UCoord(dx, dy, dz))
// index 0 unused; indices 1-7 match piece-type constants; index 7 = BLACK_PAWN
//
// The deltas are ordered in a way that allows them to be used in a single loop 
// for move generation. Use ATTACK_DELTA_COUNT to determine the number of deltas 
// for each piece type. 
//
// Use CSCoord::Step() to apply a delta to a CSCoord, which will return an 
// invalid coordinate if the result is out of bounds.  For sliding pieces, apply
// CSCoord::Step() until it returns an invalid coordinate, indicating it has gone
// off the edge of the board, to generate moves along the ray in that direction.
//
// Although these deltas are meant for a 3D game, they work equally well for 2D, 
// because the CSCoord::Step() will return an invalid CSCoord when the delta moves
// off the edge of the board, so the only valid deltas for a 2D game will be the 
// valid CSCoords.  These moves will be identical to normal 2D play.
// ---------------------------------------------------------------------------

const CUCoord ATTACK_DELTA[BPawn + 1][ATTACK_DELTA_MAX + 1] = {
    // [0] – unused (sentinel-terminated)
    { CUCoord() },
    // [1] WHITE_PAWN – attacks forward (rank+) on the same level and diagonally to adjacent levels
    { CUCoord(2,0,0), CUCoord(0,2,0), CUCoord(0,0,2), CUCoord(0,0,-2) },
    // [2] KNIGHT
    {
        CUCoord( 0, 1, 3), CUCoord(-1, 0, 3), CUCoord( 0,-1, 3), CUCoord( 1, 0, 3),
        CUCoord( 0, 3, 1), CUCoord(-3, 0, 1), CUCoord( 0,-3, 1), CUCoord( 3, 0, 1),
        CUCoord( 3, 1, 0), CUCoord( 3,-1, 0), CUCoord( 1,-3, 0), CUCoord(-1,-3, 0),
        CUCoord(-3, 1, 0), CUCoord(-3,-1, 0), CUCoord( 1, 3, 0), CUCoord(-1, 3, 0),
        CUCoord( 0, 1,-3), CUCoord(-1, 0,-3), CUCoord( 0,-1,-3), CUCoord( 1, 0,-3),
        CUCoord( 0, 3,-1), CUCoord(-3, 0,-1), CUCoord( 0,-3,-1), CUCoord( 3, 0,-1)
    },
    // [3] BISHOP  (diagonal axes in 3-D – pairs of axes)
    {
        CUCoord( 0, 0, 2), CUCoord( 2, 0, 0), CUCoord( 0,-2, 0),
        CUCoord(-2, 0, 0), CUCoord( 0, 2, 0), CUCoord( 0, 0,-2)
    },
    // [4] ROOK  (axis-aligned directions)
    {
        CUCoord( 0, 1, 1), CUCoord(-1, 0, 1), CUCoord( 0,-1, 1), CUCoord( 1, 0, 1),
        CUCoord( 1, 1, 0), CUCoord( 1,-1, 0), CUCoord(-1,-1, 0), CUCoord(-1, 1, 0),
        CUCoord( 0, 1,-1), CUCoord(-1, 0,-1), CUCoord( 0,-1,-1), CUCoord( 1, 0,-1)
    },
    // [5] QUEEN  = BISHOP dirs + ROOK dirs
    {
        CUCoord( 0, 0, 2), CUCoord( 2, 0, 0), CUCoord( 0,-2, 0),
        CUCoord(-2, 0, 0), CUCoord( 0, 2, 0), CUCoord( 0, 0,-2),
        CUCoord( 0, 1, 1), CUCoord(-1, 0, 1), CUCoord( 0,-1, 1), CUCoord( 1, 0, 1),
        CUCoord( 1, 1, 0), CUCoord( 1,-1, 0), CUCoord(-1,-1, 0), CUCoord(-1, 1, 0),
        CUCoord( 0, 1,-1), CUCoord(-1, 0,-1), CUCoord( 0,-1,-1), CUCoord( 1, 0,-1)
    },
    // [6] KING  (one step in every queen direction)
    {
        CUCoord( 0, 0, 2), CUCoord( 2, 0, 0), CUCoord( 0,-2, 0),
        CUCoord(-2, 0, 0), CUCoord( 0, 2, 0), CUCoord( 0, 0,-2),
        CUCoord( 0, 1, 1), CUCoord(-1, 0, 1), CUCoord( 0,-1, 1), CUCoord( 1, 0, 1),
        CUCoord( 1, 1, 0), CUCoord( 1,-1, 0), CUCoord(-1,-1, 0), CUCoord(-1, 1, 0),
        CUCoord( 0, 1,-1), CUCoord(-1, 0,-1), CUCoord( 0,-1,-1), CUCoord( 1, 0,-1)
    },
    // [7] BLACK_PAWN – attacks backward (rank-) on the same level and diagonally
    { CUCoord(0,-2,0), CUCoord(-2,0,0), CUCoord(0,0,2), CUCoord(0,0,-2) }
};

/* Number of attack deltas for each piece type (excluding the sentinel)
 */
const int ATTACK_DELTA_COUNT[BPawn + 1] = {0, 4, 24, 6, 12, 18, 18, 4};

/*
 * Compute attacks for a sliding piece (Bishop, Rook, Queen) using ray-walk.
 * Walks each direction in ATTACK_DELTA until hitting a blocker or board edge.
 */
CBitBoard ComputeSlidingAttacks(const CSCoord &sq, int nPieceType,
                                const CBitBoard &occupied) {
    CBitBoard attacks;
    for (int d = 0; d < ATTACK_DELTA_COUNT[nPieceType]; d++) {
        CUCoord dir = ATTACK_DELTA[nPieceType][d];
        CSCoord current = sq.Step(dir);
        while (current.IsValid()) {
            attacks.SetBit(current.BitOffset());
            if (occupied.TstBit(current.BitOffset()))
                break;
            current = current.Step(dir);
        }
    }
    return attacks;
}

/*
 * Compute attacks for a leaping piece (Pawn, Knight, King) using single step.
 * Steps once in each direction in ATTACK_DELTA.
 */
CBitBoard ComputeLeapAttacks(const CSCoord &sq, int nPieceType) {
    CBitBoard attacks;
    for (int d = 0; d < ATTACK_DELTA_COUNT[nPieceType]; d++) {
        CUCoord dir = ATTACK_DELTA[nPieceType][d];
        CSCoord target = sq.Step(dir);
        if (target.IsValid()) {
            attacks.SetBit(target.BitOffset());
        }
    }
    return attacks;
}


/*
 * Initialize the NextSQ table for all 3D squares.
 *
 * NextSQ[from][through] gives the next square beyond 'through' on the
 * sliding-piece ray that originates at 'from' and passes through 'through',
 * or -1 if no such square exists on the board.  This table drives the
 * incremental attack updates in GainAttack / LooseAttack.
 *
 * Must be called after InitMoves() (which zeroes the table).
 */
void InitNextSQ() {
    for (uint16_t wFromOffset = 0; wFromOffset < CBitBoard::SIZE; wFromOffset++) {
        if (!CSCoord::IsValid(wFromOffset))
            continue;
        CSCoord from(wFromOffset);
        // Queen covers all sliding directions (bishop + rook).
        for (int d = 0; d < ATTACK_DELTA_COUNT[Queen]; d++) {
            CUCoord dir = ATTACK_DELTA[Queen][d];
            CSCoord prev = from.Step(dir);
            if (!prev.IsValid())
                continue;
            CSCoord curr = prev.Step(dir);
            while (curr.IsValid()) {
                NextSQ[wFromOffset][prev.BitOffset()] =
                    static_cast<uint16_t>(curr.BitOffset());
                prev = curr;
                curr = curr.Step(dir);
            }
            // NextSQ[wFromOffset][prev.BitOffset()] stays -1 (end of ray).
        }
    }
}

/*
 * Initialize InterPath, Ray, BishopEPM, RookEPM, and QueenEPM for all 3D
 * squares using ATTACK_DELTA.  Overwrites the 2D-board values placed by
 * InitGeometry() with correct 3D values.
 *
 * Must be called after InitGeometry() so that the tables are zeroed first.
 */
void InitGeometry3D() {
    for (uint16_t wFromOffset = 0; wFromOffset < CBitBoard::SIZE; wFromOffset++) {
        if (!CSCoord::IsValid(wFromOffset))
            continue;
        CSCoord from(wFromOffset);

        // Reset EPM tables for every valid 3D square.
        BishopEPM[wFromOffset] = {};
        RookEPM[wFromOffset]   = {};
        QueenEPM[wFromOffset]  = {};
        WPawnEPM[wFromOffset]  = ComputeLeapAttacks(from, Pawn);
        BPawnEPM[wFromOffset]  = ComputeLeapAttacks(from, BPawn);

        // Bishop directions
        for (int d = 0; d < ATTACK_DELTA_COUNT[Bishop]; d++) {
            const CUCoord dir = ATTACK_DELTA[Bishop][d];
            CBitBoard interPath = {};
            CSCoord curr = from.Step(dir);
            while (curr.IsValid()) {
                const uint16_t wCurrOff = curr.BitOffset();
                BishopEPM[wFromOffset].SetBit(wCurrOff);
                QueenEPM[wFromOffset].SetBit(wCurrOff);
                InterPath[wFromOffset][wCurrOff] = interPath;
                interPath.SetBit(wCurrOff);
                curr = curr.Step(dir);
            }
            // Build Ray[from][each] = squares beyond that square along this ray.
            curr = from.Step(dir);
            while (curr.IsValid()) {
                CBitBoard ray = {};
                CSCoord beyond = curr.Step(dir);
                while (beyond.IsValid()) {
                    ray.SetBit(beyond.BitOffset());
                    beyond = beyond.Step(dir);
                }
                Ray[wFromOffset][curr.BitOffset()] = ray;
                curr = curr.Step(dir);
            }
        }

        // Rook directions
        for (int d = 0; d < ATTACK_DELTA_COUNT[Rook]; d++) {
            const CUCoord dir = ATTACK_DELTA[Rook][d];
            CBitBoard interPath = {};
            CSCoord curr = from.Step(dir);
            while (curr.IsValid()) {
                const uint16_t wCurrOff = curr.BitOffset();
                RookEPM[wFromOffset].SetBit(wCurrOff);
                QueenEPM[wFromOffset].SetBit(wCurrOff);
                InterPath[wFromOffset][wCurrOff] = interPath;
                interPath.SetBit(wCurrOff);
                curr = curr.Step(dir);
            }
            curr = from.Step(dir);
            while (curr.IsValid()) {
                CBitBoard ray = {};
                CSCoord beyond = curr.Step(dir);
                while (beyond.IsValid()) {
                    ray.SetBit(beyond.BitOffset());
                    beyond = beyond.Step(dir);
                }
                Ray[wFromOffset][curr.BitOffset()] = ray;
                curr = curr.Step(dir);
            }
        }
    }
}


static void UndoCastle(CPosition *, int);

/*
 * Routines to up/downdate the global database
 */

static void ShowMoveList(CPosition *p) {
    int nPly;
    for (nPly = 0; nPly < p->GetPly(); nPly++) {
        CMove move = p->GetGameLog()[nPly].gl_Move;
        Print(0, "%s\n", ICS_SAN(move));
    }
}

static void Panic(CPosition *p) {
    p->ShowPosition();
    ShowMoveList(p);
    fflush(stdout);
    abort();
}

#ifdef DEBUG
static void DebugEngine(CPosition *p) {
    unsigned int dwKingSq = p->GetKingSq(White).BitOffset();
    int nColor;
    unsigned int i;
    CBitBoard temp;

    for (i = 0; i < CBitBoard::SIZE; i++) {
        const unsigned int dwSquare = i;
        temp = p->GetAtkTo(i);
        while (temp) {
            const uint16_t wSq = temp.FindSetBit();
            temp.ClearLowestBit();
            if (!p->GetAtkFr(wSq).TstBit(dwSquare)) {
                Print(0, "AtkFr or AtkTo is bad on %c%c or %c%c\n", SQUARE(dwSquare),
                      SQUARE(wSq));
                ShowMoveList(p);
                p->ShowPosition();
                abort();
            }
        }
    }

    for (nColor = 0; nColor < 2; nColor++) {
        for (i = Pawn; i <= King; i++) {
            temp = p->GetMask(nColor, i);
            while (temp) {
                const uint16_t wSq = temp.FindSetBit();
                temp.ClearLowestBit();
                int nPc = (1 - 2 * nColor) * i;
                if (p->GetPiece(wSq) != nPc) {
                    Print(0, "Piece on %c%c is %d, expected %d!\n", SQUARE(wSq),
                          p->GetPiece(wSq), nPc);
                    ShowMoveList(p);
                    p->ShowPosition();
                    abort();
                }
            }
        }
    }

    if (p->GetAtkTo(dwKingSq) != KingEPM[dwKingSq]) {
        Print(0, "White king is bad:\n");
        PrintBitBoard(p->GetAtkTo(dwKingSq));
        Print(0, "should be:\n");
        PrintBitBoard(KingEPM[dwKingSq]);
        ShowMoveList(p);
        p->ShowPosition();
        abort();
    }
    dwKingSq = p->GetKingSq(Black).BitOffset();
    if (p->GetAtkTo(dwKingSq) != KingEPM[dwKingSq]) {
        Print(0, "Black king is bad:\n");
        PrintBitBoard(p->GetAtkTo(dwKingSq));
        Print(0, "should be:\n");
        PrintBitBoard(KingEPM[dwKingSq]);
        ShowMoveList(p);
        p->ShowPosition();
        abort();
    }
}
#endif

/*
 * Generate attacks for a piece "type" of "color" on square "square"
 */

void CPosition::AtkSet(int nType, int nColor, const CSCoord& squareCoord) {
    const unsigned int dwSquare = squareCoord.BitOffset();

    /*
     * The piece at squareCoord must already be on the board and must match
     * the nType/color arguments.  If m_rgPiece[dwSquare] is Neutral (0) or the
     * wrong colour the attack maps will be corrupted – trap that here before
     * it propagates silently.
     */
    AMY_ASSERT(TYPE(m_rgPiece[dwSquare]) == nType && SAME_COLOR(m_rgPiece[dwSquare], nColor),
               "AtkSet: piece at L%d/F%d/R%d (offset %u) is %d, "
               "expected type=%d color=%d\n",
               (int)squareCoord.m_nLevel, (int)squareCoord.m_nFile,
               (int)squareCoord.m_nRank, (unsigned)dwSquare,
               (int)m_rgPiece[dwSquare], nType, nColor);

    CBitBoard attacks;
    const CBitBoard occupied = m_rgMask[0][0] | m_rgMask[1][0];

    switch (nType) {
    case Pawn:
        attacks = ComputeLeapAttacks(squareCoord, nColor == White ? Pawn : BPawn);
        break;
    case Knight:
        attacks = ComputeLeapAttacks(squareCoord, Knight);
        break;
    case Bishop:
        attacks = ComputeSlidingAttacks(squareCoord, Bishop, occupied);
        break;
    case Rook:
        attacks = ComputeSlidingAttacks(squareCoord, Rook, occupied);
        break;
    case Queen:
        attacks = ComputeSlidingAttacks(squareCoord, Queen, occupied);
        break;
    case King:
        attacks = ComputeLeapAttacks(squareCoord, King);
        break;
    default:
        printf("AtkSet(%d, %d, %d)\n", nType, nColor, dwSquare);
        Panic(this);
        return; // never reached
    }

    m_rgAtkTo[dwSquare] = attacks;
    while (attacks) {
        const uint16_t i = attacks.FindSetBit();
        attacks.ClearLowestBit();
        m_rgAtkFr[i].SetBit(dwSquare);
    }
}

void CPosition::AtkClr(const CSCoord& squareCoord) {
    const unsigned int dwSquare = squareCoord.BitOffset();

    /*
     * AtkClr removes the attacks of the piece standing on squareCoord and is
     * always called while that piece is still on the board (just before it is
     * moved or captured).  An empty dwSquare here means a piece was removed
     * without its attacks ever being registered, or AtkClr is being run twice
     * for the same dwSquare — either way the attack maps are about to be left
     * inconsistent (a stale m_rgAtkTo row), so trap it at the source (no-op in
     * release builds).
     */
    AMY_ASSERT(m_rgPiece[dwSquare] != Neutral,
               "AtkClr: square L%d/F%d/R%d (offset %u) is empty\n",
               (int)squareCoord.m_nLevel, (int)squareCoord.m_nFile,
               (int)squareCoord.m_nRank, (unsigned)dwSquare);

    CBitBoard tmp = m_rgAtkTo[dwSquare];
    m_rgAtkTo[dwSquare] = {};

    while (tmp) {
        const uint16_t i = tmp.FindSetBit();
        tmp.ClearLowestBit();
        m_rgAtkFr[i].ClrBit(dwSquare);
    }
}

/*
 * Recalculate attacks from "from" to "to" after the piece on "to" has
 * been removed
 */

void CPosition::GainAttack(const CSCoord& fromCoord,
                       const CSCoord& toCoord) {
    const uint16_t wFrom = fromCoord.BitOffset();

    /*
     * GainAttack is only ever called for a sliding piece whose ray was
     * unblocked (a piece was removed from its path).  The sliding piece at
     * fromCoord must therefore still be on the board.
     */
    AMY_ASSERT(m_rgPiece[wFrom] != Neutral,
               "GainAttack: from square L%d/F%d/R%d (offset %u) is empty\n",
               (int)fromCoord.m_nLevel, (int)fromCoord.m_nFile,
               (int)fromCoord.m_nRank, (unsigned)wFrom);

    const uint16_t wTo = toCoord.BitOffset();
    const uint16_t *pNsq = NextSQ[wFrom];
    uint16_t wSq = wTo;
    const CBitBoard all = m_rgMask[0][0] | m_rgMask[1][0];

    for (;;) {
        wSq = pNsq[wSq];
        if (wSq == 0xffff)
            break;

        m_rgAtkTo[wFrom].SetBit(wSq);
        m_rgAtkFr[wSq].SetBit(wFrom);

        if (all.TstBit(wSq))
            break;
    }
}

/*
 * Recalculate attacks from "from" to "to" after a piece has been put
 * onto "to"
 */

void CPosition::LooseAttack(const CSCoord& fromCoord,
                         const CSCoord& toCoord) {
    const uint16_t wFrom = fromCoord.BitOffset();

    /*
     * LooseAttack is only ever called for a sliding piece whose ray was
     * blocked by a newly placed piece.  The sliding piece at fromCoord must
     * therefore still be on the board.
     */
    AMY_ASSERT(m_rgPiece[wFrom] != Neutral,
               "LooseAttack: from square L%d/F%d/R%d (offset %u) is empty\n",
               (int)fromCoord.m_nLevel, (int)fromCoord.m_nFile,
               (int)fromCoord.m_nRank, (unsigned)wFrom);

    const uint16_t wTo = toCoord.BitOffset();
    const uint16_t *pNsq = NextSQ[wFrom];
    uint16_t wSq = wTo;
    const CBitBoard all = m_rgMask[0][0] | m_rgMask[1][0];

    for (;;) {
        wSq = pNsq[wSq];
        if (wSq == 0xffff)
            break;

        const uint16_t wAttackSquare = wSq;
        m_rgAtkTo[wFrom].ClrBit(wAttackSquare);
        m_rgAtkFr[wAttackSquare].ClrBit(wFrom);

        if (all.TstBit(wAttackSquare))
            break;
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * the piece on this square has been removed
 */

void CPosition::GainAttacks(const CSCoord& toCoord) {
    const uint16_t wTo = toCoord.BitOffset();
    CBitBoard tmp = m_rgAtkFr[wTo] & m_SlidingPieces;

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        GainAttack(coord, toCoord);
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * a piece has been put onto this square
 */

void CPosition::LooseAttacks(const CSCoord& toCoord) {
    const uint16_t wTo = toCoord.BitOffset();
    CBitBoard tmp = m_rgAtkFr[wTo] & m_SlidingPieces;

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        LooseAttack(coord, toCoord);
    }
}

/*
 * Determines if a piece of type tp is a sliding piece.
 */
static inline bool is_sliding(int nTp) { return nTp >= Bishop && nTp <= Queen; }

/*
 * Make a castle move
 * I separated this routine from the normal DoMove routine since it has
 * to move two pieces
 */

static void DoCastle(CPosition *p, CMove move) {
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    const uint16_t wFromOffset = fromCoord.BitOffset();
    const uint16_t wToOffset = toCoord.BitOffset();
    const CSCoord oldRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 3 : fromCoord.m_nFile - 4,
                               fromCoord.m_nRank);
    const CSCoord newRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 1 : fromCoord.m_nFile - 1,
                               fromCoord.m_nRank);
    const uint16_t wOldRookOffset = oldRookCoord.BitOffset();
    const uint16_t wNewRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    p->AtkClr(fromCoord);

    /* rook looses its attacks */
    p->AtkClr(oldRookCoord);

    /* move king on the board */
    p->SetPiece(wToOffset, p->GetPiece(wFromOffset));
    p->SetPiece(wFromOffset, Neutral);
    p->GetMask(p->GetTurn(), 0).ClrBit(wFromOffset);
    p->GetMask(p->GetTurn(), King).ClrBit(wFromOffset);
    p->GetMask(p->GetTurn(), 0).SetBit(wToOffset);
    p->GetMask(p->GetTurn(), King).SetBit(wToOffset);

    /* move rook on the board */
    p->SetPiece(wNewRookOffset, p->GetPiece(wOldRookOffset));
    p->SetPiece(wOldRookOffset, Neutral);
    p->GetMask(p->GetTurn(), 0).ClrBit(wOldRookOffset);
    p->GetMask(p->GetTurn(), Rook).ClrBit(wOldRookOffset);
    p->GetSlidingPieces().ClrBit(wOldRookOffset);
    p->GetMask(p->GetTurn(), 0).SetBit(wNewRookOffset);
    p->GetMask(p->GetTurn(), Rook).SetBit(wNewRookOffset);
    p->GetSlidingPieces().SetBit(wNewRookOffset);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */

    p->GainAttacks(fromCoord);

    /* King and rook gain their attacks
     */

    p->AtkSet(King, p->GetTurn(), toCoord);
    p->AtkSet(Rook, p->GetTurn(), newRookCoord);
    p->SetKingSq(p->GetTurn(), toCoord);

    /* update hashkey */
    /* Das koennte ich vorher berechnen! Ist dann nur eine Anweisung! */

    p->SetHashKey(p->GetHashKey() ^
                (HashKeys[p->GetTurn()][King][wFromOffset] ^ HashKeys[p->GetTurn()][King][wToOffset] ^
                HashKeys[p->GetTurn()][Rook][wOldRookOffset] ^
                HashKeys[p->GetTurn()][Rook][wNewRookOffset]));
}

/*
 * Unmake a castle move
 */

static void UndoCastle(CPosition *p, CMove move) {
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    const uint16_t wFromOffset = fromCoord.BitOffset();
    const uint16_t wToOffset = toCoord.BitOffset();
    const CSCoord oldRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 3 : fromCoord.m_nFile - 4,
                               fromCoord.m_nRank);
    const CSCoord newRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 1 : fromCoord.m_nFile - 1,
                               fromCoord.m_nRank);
    const uint16_t wOldRookOffset = oldRookCoord.BitOffset();
    const uint16_t wNewRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    p->AtkClr(toCoord);

    /* rook looses its attacks */
    p->AtkClr(newRookCoord);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */
    p->LooseAttacks(fromCoord);

    /* move king on the board */
    p->SetPiece(wFromOffset, p->GetPiece(wToOffset));
    p->SetPiece(wToOffset, Neutral);
    p->GetMask(p->GetTurn(), 0).ClrBit(wToOffset);
    p->GetMask(p->GetTurn(), King).ClrBit(wToOffset);
    p->GetMask(p->GetTurn(), 0).SetBit(wFromOffset);
    p->GetMask(p->GetTurn(), King).SetBit(wFromOffset);

    /* move rook on the board */
    p->SetPiece(wOldRookOffset, p->GetPiece(wNewRookOffset));
    p->SetPiece(wNewRookOffset, Neutral);
    p->GetMask(p->GetTurn(), 0).ClrBit(wNewRookOffset);
    p->GetMask(p->GetTurn(), Rook).ClrBit(wNewRookOffset);
    p->GetSlidingPieces().ClrBit(wNewRookOffset);
    p->GetMask(p->GetTurn(), 0).SetBit(wOldRookOffset);
    p->GetMask(p->GetTurn(), Rook).SetBit(wOldRookOffset);
    p->GetSlidingPieces().SetBit(wOldRookOffset);

    /* King and rook gain their attacks
     */

    p->AtkSet(King, p->GetTurn(), fromCoord);
    p->AtkSet(Rook, p->GetTurn(), oldRookCoord);
    p->SetKingSq(p->GetTurn(), fromCoord);
}

/*
 * Make a move
 * updates the global database
 */

void CPosition::DoMove(CMove move) {
    CPosition *p = this;
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    const uint16_t wFromOffset = fromCoord.BitOffset();
    const uint16_t wToOffset = toCoord.BitOffset();
    int8_t nTp = TYPE(p->m_rgPiece[wFromOffset]);

    /*
     * The moving piece must exist on the from-square and belong to the side to
     * move. A move out of an empty square (or out of an opponent's piece) is an
     * illegal move that should have been rejected by move generation / legality
     * checking; trap it here before it corrupts the board and the attack maps
     * (an empty from-square yields nTp == Neutral, which later panics in AtkSet).
     */
    AMY_ASSERT(nTp != Neutral && SAME_COLOR(p->m_rgPiece[wFromOffset], p->m_nTurn),
               "DoMove moving a non-friendly piece (%d) from L%d/F%d/R%d\n",
               (int)p->m_rgPiece[wFromOffset], fromCoord.m_nLevel,
               fromCoord.m_nFile, fromCoord.m_nRank);
    AMY_ASSERT(p->m_pActLog >= p->m_pGameLog &&
                   p->m_pActLog < p->m_pGameLog + p->m_cGameLog,
               "DoMove: m_pActLog out of range (ply=%u size=%u act=%p base=%p)\n",
               (unsigned)p->m_wPly, p->m_cGameLog, (void *)p->m_pActLog,
               (void *)p->m_pGameLog);
    AMY_ASSERT(p->m_pActLog == p->m_pGameLog + p->m_wPly,
               "DoMove: m_pActLog/ply mismatch (ply=%u index=%u)\n",
               (unsigned)p->m_wPly,
               (unsigned)(p->m_pActLog - p->m_pGameLog));

    /* save EnPassant and Castling */
    p->m_pActLog->gl_EnPassant = p->m_EnPassant;
    p->m_pActLog->gl_Castle = p->m_bCastle;
    p->m_pActLog->gl_HashKey = p->m_ullHKey;
    p->m_pActLog->gl_PawnKey = p->m_ullPKey;

    if (move.IsCastle()) {
        DoCastle(p, move);
        p->m_bCastle &= ~(CastleMask[p->m_nTurn][0] | CastleMask[p->m_nTurn][1]);
    } else {
        /* piece looses its attacks */
        p->AtkClr(fromCoord);

        if (nTp == King) {
            p->m_rgKingSq[p->m_nTurn] = toCoord;
        }

        /* remove it from the board */
        p->m_rgPiece[wFromOffset] = Neutral;
        p->m_rgMask[p->m_nTurn][0].ClrBit(wFromOffset);
        p->m_rgMask[p->m_nTurn][nTp].ClrBit(wFromOffset);
        if (is_sliding(nTp))
            p->m_SlidingPieces.ClrBit(wFromOffset);
        /* re-calculate attacks through from-square */
        p->GainAttacks(fromCoord);

        /* update hashkey */
        p->m_ullHKey ^= HashKeys[p->m_nTurn][nTp][wFromOffset];
        if (nTp == Pawn)
            p->m_ullPKey ^= HashKeys[p->m_nTurn][Pawn][wFromOffset];

        if (nTp == King) {
            /* No more castling rights */
            p->m_bCastle &= ~(CastleMask[p->m_nTurn][0] | CastleMask[p->m_nTurn][1]);
        } else if (nTp == Rook) {
            if (wFromOffset == (p->m_nTurn == White ? hh1 : hh8))
                p->m_bCastle &= ~(CastleMask[p->m_nTurn][0]);
            if (wFromOffset == (p->m_nTurn == White ? ha1 : ha8))
                p->m_bCastle &= ~(CastleMask[p->m_nTurn][1]);
        }
        if (move.IsCapture()) {
            const int8_t nCapturedPiece = p->m_rgPiece[wToOffset];
            int nSp = TYPE(nCapturedPiece);

            /*
             * A capture move (M_CAPTURE) must land on a real opposing piece
             * that is not a King. Capturing a king is illegal (a legal move
             * generator must never let the opponent's king be taken), and an
             * M_CAPTURE flag on an empty or own-coloured square indicates a
             * malformed/mis-encoded move. Either way the resulting state is
             * corrupt: the matching UndoMove restores p->m_pActLog->gl_Piece via
             * AtkSet(TYPE(gl_Piece), ...), so a Neutral/invalid captured piece
             * later panics in AtkSet's default case. Trap it here so the corrupt
             * state is both logged and trapped in the debugger (no-op in release
             * builds).
             */
            AMY_ASSERT(nSp != Neutral && nSp != King &&
                           SAME_COLOR(nCapturedPiece, OPP(p->m_nTurn)),
                       "DoMove capturing an invalid piece (%d) at L%d/F%d/R%d "
                       "(from L%d/F%d/R%d) - illegal move reached DoMove\n",
                       (int)nCapturedPiece, toCoord.m_nLevel, toCoord.m_nFile,
                       toCoord.m_nRank, fromCoord.m_nLevel, fromCoord.m_nFile,
                       fromCoord.m_nRank);

            /* piece looses its attacks */
            p->AtkClr(toCoord);

            /* remember type of captured piece */
            p->m_pActLog->gl_Piece = p->m_rgPiece[wToOffset];

            p->m_rgMask[OPP(p->m_nTurn)][0].ClrBit(wToOffset);
            p->m_rgMask[OPP(p->m_nTurn)][nSp].ClrBit(wToOffset);
            if (is_sliding(nSp))
                p->m_SlidingPieces.ClrBit(wToOffset);

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] -= Value[nSp];
            if (nSp != Pawn)
                p->m_rgnNonPawn[OPP(p->m_nTurn)] -= Value[nSp];

            /* update material signature */
            if (!(p->m_rgMask[OPP(p->m_nTurn)][nSp])) {
                p->m_rgbMaterialSignature[OPP(p->m_nTurn)] &= ~SIGNATURE_BIT(nSp);
            }

            /* update hashkey */
            p->m_ullHKey ^= HashKeys[OPP(p->m_nTurn)][nSp][wToOffset];
            if (nSp == Pawn)
                p->m_ullPKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][wToOffset];
            if (wToOffset == (OPP(p->m_nTurn) == White ? hh1 : hh8)) {
                p->m_bCastle &= ~(CastleMask[OPP(p->m_nTurn)][0]);
            }
            if (wToOffset == (OPP(p->m_nTurn) == White ? ha1 : ha8)) {
                p->m_bCastle &= ~(CastleMask[OPP(p->m_nTurn)][1]);
            }
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.m_nLevel, toCoord.m_nFile,
                p->m_nTurn == White ? toCoord.m_nRank - 1 : toCoord.m_nRank + 1);
            const uint16_t wCapturedPawnOffset = capturedPawnCoord.BitOffset();

            /* piece looses its attacks */
            p->AtkClr(capturedPawnCoord);

            /* captured piece must be a pawn */
            p->m_pActLog->gl_Piece = ((OPP(p->m_nTurn) == White) ? Pawn : -Pawn);

            p->m_rgMask[OPP(p->m_nTurn)][0].ClrBit(wCapturedPawnOffset);
            p->m_rgMask[OPP(p->m_nTurn)][Pawn].ClrBit(wCapturedPawnOffset);

            /* re-calculate attacks through to-square */
            p->GainAttacks(capturedPawnCoord);

            /* remove captured pawn from the board */
            p->m_rgPiece[wCapturedPawnOffset] = Neutral;

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] -= Value[Pawn];

            /* update material signature */
            if (!(p->m_rgMask[OPP(p->m_nTurn)][Pawn])) {
                p->m_rgbMaterialSignature[OPP(p->m_nTurn)] &= ~SIGNATURE_BIT(Pawn);
            }

            /* update hashkey */
            p->m_ullHKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][wCapturedPawnOffset];
            p->m_ullPKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][wCapturedPawnOffset];

            /* re-calculate attacks through to-square */
            p->LooseAttacks(toCoord);
        } else {
            /* re-calculate attacks through to-square */
            p->LooseAttacks(toCoord);
        }

        if (move.HasPromotion()) {
            /* Promote piece */
            nTp = PromoType(move);

            /* Update own material */
            p->m_rgnMaterial[p->m_nTurn] += Value[nTp] - Value[Pawn];
            p->m_rgnNonPawn[p->m_nTurn] += Value[nTp];

            if (!(p->m_rgMask[p->m_nTurn][Pawn])) {
                p->m_rgbMaterialSignature[p->m_nTurn] &= ~SIGNATURE_BIT(Pawn);
            }
            p->m_rgbMaterialSignature[p->m_nTurn] |= SIGNATURE_BIT(nTp);
        }

        /* put it on the board again */
        p->m_rgPiece[wToOffset] = (p->m_nTurn == White) ? nTp : -nTp;
        p->m_rgMask[p->m_nTurn][0].SetBit(wToOffset);
        p->m_rgMask[p->m_nTurn][nTp].SetBit(wToOffset);
        if (is_sliding(nTp))
            p->m_SlidingPieces.SetBit(wToOffset);

        /* piece gains its attacks */
        p->AtkSet(nTp, p->m_nTurn, toCoord);

        /* update hashkey */
        p->m_ullHKey ^= HashKeys[p->m_nTurn][nTp][wToOffset];
        if (nTp == Pawn)
            p->m_ullPKey ^= HashKeys[p->m_nTurn][Pawn][wToOffset];
    }

    /* Check if loss of castling rights */
    if (p->m_bCastle != p->m_pActLog->gl_Castle) {
        p->m_ullHKey ^= HashKeysCastle[p->m_pActLog->gl_Castle];
        p->m_ullHKey ^= HashKeysCastle[p->m_bCastle];
    }

    /*
     * Check if double pawn push. There is a little trick here:
     * We only set the enPassant flag if there is a possibility
     * of an enPassant capture at all. This increases the efficiency of
     * the transposition table.
     */

    p->m_EnPassant = InvalidSquareCoord();
    if (move.IsPawnDoublePush()) {
        const CSCoord passantCoord(toCoord.m_nLevel, toCoord.m_nFile,
                                   p->m_nTurn == White ? toCoord.m_nRank - 1 : toCoord.m_nRank + 1);
        const uint16_t wPassantOffset = passantCoord.BitOffset();
        if (p->m_rgAtkFr[wPassantOffset] & p->m_rgMask[OPP(p->m_nTurn)][Pawn]) {
            p->m_EnPassant = passantCoord;
        }
    }

    if ((p->m_EnPassant.IsValid() != p->m_pActLog->gl_EnPassant.IsValid()) ||
        (p->m_EnPassant.IsValid() &&
         p->m_EnPassant.BitOffset() != p->m_pActLog->gl_EnPassant.BitOffset())) {
        if (p->m_pActLog->gl_EnPassant.IsValid()) {
            p->m_ullHKey ^= HashKeysEP[p->m_pActLog->gl_EnPassant.BitOffset()];
        }
        if (p->m_EnPassant.IsValid()) {
            p->m_ullHKey ^= HashKeysEP[p->m_EnPassant.BitOffset()];
        }
    }

    /* Update SGameLog */
    p->m_pActLog->gl_Move = move;
    p->m_wPly++;

    /* Grow gameLog if needed. */
    if (p->m_wPly >= p->m_cGameLog) {
        const unsigned int nOldSize = p->m_cGameLog;
        const unsigned int nNewSize = p->m_cGameLog * 2;
        SGameLog *pNewGameLog = (SGameLog *)safe_realloc(
            p->m_pGameLog, sizeof(SGameLog) * nNewSize);
        PrintDebug(9,
                   "DoMove: game log growth %u -> %u at ply %u (old=%p new=%p)\n",
                   nOldSize, nNewSize, (unsigned)p->m_wPly,
                   (void *)p->m_pGameLog, (void *)pNewGameLog);
        p->m_cGameLog = nNewSize;
        p->m_pGameLog = pNewGameLog;
        /* Zero the newly allocated portion (realloc does not initialize) */
        memset(p->m_pGameLog + nOldSize, 0,
               sizeof(SGameLog) * (nNewSize - nOldSize));
        p->m_pActLog = p->m_pGameLog + p->m_wPly;
    } else {
        p->m_pActLog++;
    }
    AMY_ASSERT(p->m_pActLog == p->m_pGameLog + p->m_wPly,
               "DoMove: post-update m_pActLog/ply mismatch (ply=%u index=%u)\n",
               (unsigned)p->m_wPly,
               (unsigned)(p->m_pActLog - p->m_pGameLog));

    /* Check if reversible move */
    if (move.IsCapture() || move.HasPromotion() || move.IsCastle() || nTp == Pawn) {
        p->m_pActLog->gl_IrrevCount = 0;
    } else {
        p->m_pActLog->gl_IrrevCount = (p->m_pActLog - 1)->gl_IrrevCount + 1;
    }

    /* Swap p->turns */
    p->m_nTurn = OPP(p->m_nTurn);
    p->m_ullHKey ^= STMKey;
}

void CPosition::UndoMove(CMove move) {
    CPosition *p = this;
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    const uint16_t wFromOffset = fromCoord.BitOffset();
    const uint16_t wToOffset = toCoord.BitOffset();
    int8_t nTp = TYPE(p->m_rgPiece[wToOffset]);

    /* Swap p->turns */
    p->m_nTurn = OPP(p->m_nTurn);

    /* Decrement ActLog */
    p->m_pActLog--;
    p->m_wPly--;

    /*
     * After the turn swap p->m_nTurn is the colour that played the move being
     * undone, so the to-square must currently hold one of that side's pieces
     * (the piece that moved there, possibly a promoted piece). If it does not,
     * the board/attack maps are already corrupt before we start undoing - trap
     * it here rather than letting AtkSet/AtkClr operate on bogus state. Castling
     * relocates two pieces and is handled by UndoCastle, so it is exempt.
     */
    AMY_ASSERT(move.IsCastle() ||
                   (nTp != Neutral &&
                    SAME_COLOR(p->m_rgPiece[wToOffset], p->m_nTurn)),
               "UndoMove: no friendly piece (%d) on the to-square L%d/F%d/R%d\n",
               (int)p->m_rgPiece[wToOffset], toCoord.m_nLevel, toCoord.m_nFile,
               toCoord.m_nRank);

    if (move.IsCastle()) {
        UndoCastle(p, move);
    } else {
        /* piece looses its attacks */
        p->AtkClr(toCoord);

        if (nTp == King) {
            p->m_rgKingSq[p->m_nTurn] = fromCoord;
        }

        /* update masks */
        p->m_rgMask[p->m_nTurn][0].ClrBit(wToOffset);
        p->m_rgMask[p->m_nTurn][nTp].ClrBit(wToOffset);
        if (is_sliding(nTp))
            p->m_SlidingPieces.ClrBit(wToOffset);

        if (move.HasPromotion()) {
            /* Update own material */
            p->m_rgnMaterial[p->m_nTurn] -= Value[nTp] - Value[Pawn];
            p->m_rgnNonPawn[p->m_nTurn] -= Value[nTp];

            /* update material signature */
            if (!(p->m_rgMask[p->m_nTurn][nTp])) {
                p->m_rgbMaterialSignature[p->m_nTurn] &= ~SIGNATURE_BIT(nTp);
            }

            /* Unpromote piece */
            nTp = Pawn;

            /* update material signature */
            p->m_rgbMaterialSignature[p->m_nTurn] |= SIGNATURE_BIT(Pawn);
        }

        if (move.IsCapture()) {
            int8_t nSp = p->m_pActLog->gl_Piece;

            /*
             * gl_Piece is the piece that DoMove removed from the to-square; on
             * undo it is restored to the board and re-registered in the attack
             * maps via AtkSet(TYPE(nSp), ...). It must therefore be a real
             * opposing piece of type Pawn..Queen (never Neutral, never a King -
             * kings are not capturable). A Neutral/invalid value here is exactly
             * the corruption that makes AtkSet hit its default case and panic;
             * trap it before that happens so the failing state is logged and
             * caught in the debugger (no-op in release builds).
             */
            AMY_ASSERT(TYPE(nSp) >= Pawn && TYPE(nSp) <= Queen &&
                           SAME_COLOR(nSp, OPP(p->m_nTurn)),
                       "UndoMove restoring an invalid captured piece (%d) at "
                       "L%d/F%d/R%d - would panic in AtkSet\n",
                       (int)nSp, toCoord.m_nLevel, toCoord.m_nFile,
                       toCoord.m_nRank);

            p->m_rgPiece[wToOffset] = nSp;
            nSp = TYPE(nSp);
            p->m_rgMask[OPP(p->m_nTurn)][0].SetBit(wToOffset);
            p->m_rgMask[OPP(p->m_nTurn)][nSp].SetBit(wToOffset);
            if (is_sliding(nSp)) {
                p->m_SlidingPieces.SetBit(wToOffset);
            }

            /*
             * piece gains its attacks - must run AFTER the captured piece is
             * placed back on the board (m_rgPiece[wToOffset] = nSp above), since
             * AtkSet reads m_rgPiece[square] to validate/seed the attack maps.
             */
            p->AtkSet(nSp, OPP(p->m_nTurn), toCoord);

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] += Value[nSp];
            if (nSp != Pawn)
                p->m_rgnNonPawn[OPP(p->m_nTurn)] += Value[nSp];

            /* update material signature */
            p->m_rgbMaterialSignature[OPP(p->m_nTurn)] |= SIGNATURE_BIT(nSp);
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.m_nLevel, toCoord.m_nFile,
                p->m_nTurn == White ? toCoord.m_nRank - 1 : toCoord.m_nRank + 1);
            const uint16_t wCapturedPawnOffset = capturedPawnCoord.BitOffset();

            p->m_rgMask[OPP(p->m_nTurn)][0].SetBit(wCapturedPawnOffset);
            p->m_rgMask[OPP(p->m_nTurn)][Pawn].SetBit(wCapturedPawnOffset);

            /* re-calculate attacks through to-square */
            p->LooseAttacks(capturedPawnCoord);

            /* restore captured pawn to the board */
            p->m_rgPiece[wCapturedPawnOffset] = (OPP(p->m_nTurn) == White) ? Pawn : -Pawn;
            p->m_rgPiece[wToOffset] = Neutral;

            /*
             * piece gains its attacks - must run AFTER the captured pawn is
             * placed back on the board (m_rgPiece[wCapturedPawnOffset] above),
             * since AtkSet reads m_rgPiece[square] to validate the attack maps.
             */
            p->AtkSet(Pawn, OPP(p->m_nTurn), capturedPawnCoord);

            /* re-calculate attacks through to-square */
            p->GainAttacks(toCoord);

            /* Update oppponents material */
            p->m_rgnMaterial[OPP(p->m_nTurn)] += Value[Pawn];

            /* update material signature */
            p->m_rgbMaterialSignature[OPP(p->m_nTurn)] |= SIGNATURE_BIT(Pawn);
        } else {
            p->m_rgPiece[wToOffset] = Neutral;

            /* re-calculate attacks through to-square */
            p->GainAttacks(toCoord);
        }

        /* re-calculate attacks through from-square */
        p->LooseAttacks(fromCoord);

        /* put it on the board again */
        p->m_rgPiece[wFromOffset] = (p->m_nTurn == White) ? nTp : -nTp;
        p->m_rgMask[p->m_nTurn][0].SetBit(wFromOffset);
        p->m_rgMask[p->m_nTurn][nTp].SetBit(wFromOffset);
        if (is_sliding(nTp))
            p->m_SlidingPieces.SetBit(wFromOffset);

        /* piece gains its attacks */
        p->AtkSet(nTp, p->m_nTurn, fromCoord);
    }

    /* restore EnPassant and Castling */
    p->m_EnPassant = p->m_pActLog->gl_EnPassant;
    p->m_bCastle = p->m_pActLog->gl_Castle;

    p->m_ullHKey = p->m_pActLog->gl_HashKey;
    p->m_ullPKey = p->m_pActLog->gl_PawnKey;

    /*
    DebugEngine(move);
    */
}

/*
 * Undo the last move that was played in this position.
 *
 * This is the public, encapsulated entry point for the "undo" feature: callers
 * (such as the GUI or command interface) do not need to reach into the internal
 * game log to retrieve the last move.  Returns true if a move was undone, or
 * false if the position is already at the start of the game (nothing to undo).
 */

bool CPosition::Undo() {
    CPosition *p = this;
    if (p->m_wPly == 0)
        return false;
    p->UndoMove((p->m_pActLog - 1)->gl_Move);
    return true;
}

/*
 * Make a null move, i.e. swap the p->GetTurn() on the move
 */

void CPosition::DoNull() {
    CPosition *p = this;

    /*
     * A null move hands the move to the opponent without changing the board, so
     * it must never be played while the side to move is in check (the opponent
     * could simply capture the king on the reply). The search only takes the
     * null-move branch when !incheck; assert the same invariant here so a stray
     * caller is logged and trapped (no-op in release builds).
     */
    AMY_ASSERT(!p->InCheck(p->m_nTurn),
               "DoNull called while side %d is in check\n", p->m_nTurn);
    AMY_ASSERT(p->m_pActLog >= p->m_pGameLog &&
                   p->m_pActLog < p->m_pGameLog + p->m_cGameLog,
               "DoNull: m_pActLog out of range (ply=%u size=%u act=%p base=%p)\n",
               (unsigned)p->m_wPly, p->m_cGameLog, (void *)p->m_pActLog,
               (void *)p->m_pGameLog);
    AMY_ASSERT(p->m_pActLog == p->m_pGameLog + p->m_wPly,
               "DoNull: m_pActLog/ply mismatch (ply=%u index=%u)\n",
               (unsigned)p->m_wPly,
               (unsigned)(p->m_pActLog - p->m_pGameLog));

    /* Update SGameLog */
    p->m_pActLog->gl_Move = M_NULL;
    p->m_pActLog->gl_EnPassant = p->m_EnPassant;
    p->m_pActLog->gl_Castle = p->m_bCastle;
    p->m_pActLog->gl_HashKey = p->m_ullHKey;
    p->m_EnPassant = InvalidSquareCoord();

    if ((p->m_EnPassant.IsValid() != p->m_pActLog->gl_EnPassant.IsValid()) ||
        (p->m_EnPassant.IsValid() &&
         p->m_EnPassant.BitOffset() != p->m_pActLog->gl_EnPassant.BitOffset())) {
        if (p->m_pActLog->gl_EnPassant.IsValid()) {
            p->m_ullHKey ^= HashKeysEP[p->m_pActLog->gl_EnPassant.BitOffset()];
        }
        if (p->m_EnPassant.IsValid()) {
            p->m_ullHKey ^= HashKeysEP[p->m_EnPassant.BitOffset()];
        }
    }

    p->m_wPly++;

    /* Grow gameLog if needed. */
    if (p->m_wPly >= p->m_cGameLog) {
        const unsigned int nOldSize = p->m_cGameLog;
        const unsigned int nNewSize = p->m_cGameLog * 2;
        SGameLog *pNewGameLog = (SGameLog *)safe_realloc(
            p->m_pGameLog, sizeof(SGameLog) * nNewSize);
        PrintDebug(9,
                   "DoNull: game log growth %u -> %u at ply %u (old=%p new=%p)\n",
                   nOldSize, nNewSize, (unsigned)p->m_wPly,
                   (void *)p->m_pGameLog, (void *)pNewGameLog);
        p->m_cGameLog = nNewSize;
        p->m_pGameLog = pNewGameLog;
        /* Zero the newly allocated portion (realloc does not initialize) */
        memset(p->m_pGameLog + nOldSize, 0,
               sizeof(SGameLog) * (nNewSize - nOldSize));
        p->m_pActLog = p->m_pGameLog + p->m_wPly;
    } else {
        p->m_pActLog++;
    }
    AMY_ASSERT(p->m_pActLog == p->m_pGameLog + p->m_wPly,
               "DoNull: post-update m_pActLog/ply mismatch (ply=%u index=%u)\n",
               (unsigned)p->m_wPly,
               (unsigned)(p->m_pActLog - p->m_pGameLog));

    /* treat null move as irreversible */
    p->m_pActLog->gl_IrrevCount = 0;

    /* swap p->turns */
    p->m_nTurn = OPP(p->m_nTurn);
    p->m_ullHKey ^= STMKey;
}

/*
 * Unmake a null move
 */

void CPosition::UndoNull() {
    CPosition *p = this;

    /*
     * UndoNull must balance a prior DoNull: there has to be at least one ply on
     * the game log and the entry being undone must actually be a null move.
     * Undoing past ply 0, or undoing a real move as if it were a null move,
     * would silently corrupt the game log / hash key - trap it here (no-op in
     * release builds).
     */
    AMY_ASSERT(p->m_wPly > 0, "UndoNull called at ply 0 (nothing to undo)\n");

    p->m_nTurn = OPP(p->m_nTurn);

    /* Decrement ActLog */
    p->m_pActLog--;
    p->m_wPly--;

    AMY_ASSERT(p->m_pActLog->gl_Move == M_NULL,
               "UndoNull: log entry being undone is not a null move\n");

    p->m_EnPassant = p->m_pActLog->gl_EnPassant;
    p->m_ullHKey = p->m_pActLog->gl_HashKey;
}

/*
 * Given the Masks and the p->GetPiece() array, recalculate all necessary data
 */

void CPosition::RecalcAttacks() {
    CPosition *p = this;
    int i;
    CBitBoard tmp;

    PrintDebug(9, "RecalcAttacks: performing full attack recalculation\n");

    for (unsigned int dwSquare = 0; dwSquare < CBitBoard::SIZE; dwSquare++) {
        p->m_rgAtkTo[dwSquare] = p->m_rgAtkFr[dwSquare] = {};
    }

    for (i = Pawn; i <= King; i++) {
        p->m_rgMask[White][i] = p->m_rgMask[Black][i] = {};
    }

    p->m_SlidingPieces = {};

    p->m_rgnMaterial[White] = p->m_rgnMaterial[Black] = 0;
    p->m_rgnNonPawn[White] = p->m_rgnNonPawn[Black] = 0;
    p->m_rgbMaterialSignature[White] = p->m_rgbMaterialSignature[Black] = 0;
    p->m_ullHKey = p->m_ullPKey = 0;

    tmp = p->m_rgMask[White][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int nPc = p->m_rgPiece[i];
        /*
         * m_rgMask[White][0] must only have bits set for squares that actually
         * hold a white piece.  A stale bit (nPc <= 0) means the occupancy mask
         * and the piece array have diverged — catch it before the downstream
         * mask/attack tables are built on corrupted data.
         */
        AMY_ASSERT(nPc > 0,
                   "RecalcAttacks: m_rgMask[White][0] bit %u set but "
                   "m_rgPiece[%u]=%d (not a white piece)\n",
                   (unsigned)i, (unsigned)i, nPc);
        tmp.ClearLowestBit();
        p->m_rgMask[White][nPc].SetBit(i);
        if (is_sliding(nPc))
            p->m_SlidingPieces.SetBit(i);
        p->m_rgnMaterial[White] += Value[nPc];
        p->m_ullHKey ^= HashKeys[White][nPc][i];
        if (nPc != Pawn)
            p->m_rgnNonPawn[White] += Value[nPc];
        else {
            p->m_ullPKey ^= HashKeys[White][Pawn][i];
        }

        if (nPc != King) {
            p->m_rgbMaterialSignature[White] |= SIGNATURE_BIT(nPc);
        }
    }

    tmp = p->m_rgMask[Black][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int nPc = -p->m_rgPiece[i];
        /*
         * m_rgMask[Black][0] must only have bits set for squares that actually
         * hold a black piece (stored as negative values).  A stale bit means
         * the occupancy mask and the piece array have diverged.
         */
        AMY_ASSERT(p->m_rgPiece[i] < 0,
                   "RecalcAttacks: m_rgMask[Black][0] bit %u set but "
                   "m_rgPiece[%u]=%d (not a black piece)\n",
                   (unsigned)i, (unsigned)i, (int)p->m_rgPiece[i]);
        tmp.ClearLowestBit();
        p->m_rgMask[Black][nPc].SetBit(i);
        if (is_sliding(nPc))
            p->m_SlidingPieces.SetBit(i);
        p->m_rgnMaterial[Black] += Value[nPc];
        p->m_ullHKey ^= HashKeys[Black][nPc][i];
        if (nPc != Pawn)
            p->m_rgnNonPawn[Black] += Value[nPc];
        else {
            p->m_ullPKey ^= HashKeys[Black][Pawn][i];
        }

        if (nPc != King) {
            p->m_rgbMaterialSignature[Black] |= SIGNATURE_BIT(nPc);
        }
    }

    tmp = p->m_rgMask[White][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        p->AtkSet(p->m_rgPiece[coord.BitOffset()], White, coord);
    }

    tmp = p->m_rgMask[Black][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        p->AtkSet(-p->m_rgPiece[coord.BitOffset()], Black, coord);
    }

    p->m_rgKingSq[White] = (p->m_rgMask[White][King]).FindSetBitCoord();
    p->m_rgKingSq[Black] = (p->m_rgMask[Black][King]).FindSetBitCoord();

    /*
     * Post-build invariant: every bit recorded in m_rgAtkFr must correspond to
     * a real piece.  A stale bit from an empty dwSquare means either the occupancy
     * masks (m_rgMask[side][0]) fed into this function were already corrupted, or
     * a bug in AtkSet generated a bogus entry.  Either way, if GenTo ever sees
     * such a bit it will emit a move from an empty dwSquare and DoMove will assert.
     */
    for (unsigned int nSq = 0; nSq < CBitBoard::SIZE; nSq++) {
        CBitBoard frBits = p->m_rgAtkFr[nSq];
        while (frBits) {
            const uint16_t nFrom = frBits.FindSetBit();
            frBits.ClearLowestBit();
            AMY_ASSERT(p->m_rgPiece[nFrom] != Neutral,
                       "RecalcAttacks: m_rgAtkFr[%u] bit %u set from empty "
                       "square (from L%d/F%d/R%d attacking L%d/F%d/R%d)\n",
                       (unsigned)nSq, (unsigned)nFrom,
                       (int)CSCoord(nFrom).m_nLevel, (int)CSCoord(nFrom).m_nFile,
                       (int)CSCoord(nFrom).m_nRank,
                       (int)CSCoord(nSq).m_nLevel, (int)CSCoord(nSq).m_nFile,
                       (int)CSCoord(nSq).m_nRank);
        }
    }

    /*
     * Symmetric post-build invariant on m_rgAtkTo: a non-empty attack row for
     * dwSquare nFrom means "the piece on nFrom attacks these squares", so nFrom
     * itself must hold a real piece.  A bit set in m_rgAtkTo from an empty
     * dwSquare is exactly the corruption that makes GenFrom emit a move out of an
     * empty dwSquare (which then trips the DoMove guard).  Catch it here so the
     * stale attack row is logged and trapped (no-op in release builds).
     */
    for (unsigned int nFrom = 0; nFrom < CBitBoard::SIZE; nFrom++) {
        if (p->m_rgAtkTo[nFrom].IsNotEmpty()) {
            AMY_ASSERT(p->m_rgPiece[nFrom] != Neutral,
                       "RecalcAttacks: m_rgAtkTo[%u] is non-empty but square "
                       "L%d/F%d/R%d is empty\n",
                       (unsigned)nFrom, (int)CSCoord(nFrom).m_nLevel,
                       (int)CSCoord(nFrom).m_nFile, (int)CSCoord(nFrom).m_nRank);
        }
    }

    p->m_ullHKey ^= HashKeysCastle[p->m_bCastle];
    if (p->m_nTurn == Black)
        p->m_ullHKey ^= STMKey;

    if (p->m_EnPassant.IsValid()) {
        p->m_ullHKey ^= HashKeysEP[p->m_EnPassant.BitOffset()];
    }
}

/*
 * Generate all capturing moves to a square "square"
 */
void CPosition::GenTo(const CSCoord& squareCoord, heap_t heap) {
    CPosition *p = this;
    const unsigned int dwSquare = squareCoord.BitOffset();

    /*
     * GenTo generates capture moves TO squareCoord.  The target dwSquare must
     * hold a real (non-empty) piece for the capture to be valid.
     */
    AMY_ASSERT(p->m_rgPiece[dwSquare] != Neutral,
               "GenTo: target square L%d/F%d/R%d (offset %u) is empty\n",
               (int)squareCoord.m_nLevel, (int)squareCoord.m_nFile,
               (int)squareCoord.m_nRank, (unsigned)dwSquare);

    CBitBoard tmp = p->m_rgAtkFr[dwSquare] & p->m_rgMask[p->m_nTurn][0];

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();

        /*
         * The bit in m_rgAtkFr & m_rgMask[turn][0] must correspond to a real
         * friendly piece.  A stale bit in either table (occupancy mask or attack
         * map) would produce a move out of an empty dwSquare, which DoMove traps.
         */
        AMY_ASSERT(p->m_rgPiece[coord.BitOffset()] != Neutral &&
                       SAME_COLOR(p->m_rgPiece[coord.BitOffset()], p->m_nTurn),
                   "GenTo: m_rgAtkFr/m_rgMask indicate an attack from empty or "
                   "wrong-color square (from L%d/F%d/R%d to L%d/F%d/R%d), "
                   "piece=%d\n",
                   (int)coord.m_nLevel, (int)coord.m_nFile, (int)coord.m_nRank,
                   (int)squareCoord.m_nLevel, (int)squareCoord.m_nFile,
                   (int)squareCoord.m_nRank,
                   (int)p->m_rgPiece[coord.BitOffset()]);
        if (TYPE(p->m_rgPiece[coord.BitOffset()]) == Pawn) {
            if (is_promo_square(squareCoord)) {
                append_to_heap(heap, make_promotion(coord, squareCoord, Queen, M_CAPTURE));
                append_to_heap(heap, make_promotion(coord, squareCoord, Knight, M_CAPTURE));
                append_to_heap(heap, make_promotion(coord, squareCoord, Rook, M_CAPTURE));
                append_to_heap(heap, make_promotion(coord, squareCoord, Bishop, M_CAPTURE));
            } else if (pawn_may_move_to(squareCoord)) {
                append_to_heap(heap, make_move(coord, squareCoord, M_CAPTURE));
            }
            /* else: edge rank of a non-promotion level — illegal pawn target */
        } else {
            append_to_heap(heap, make_move(coord, squareCoord, M_CAPTURE));
        }
    }
}

void CPosition::GenEnpas(heap_t heap) {
    CPosition *p = this;
    CBitBoard tmp;

    if (!p->m_EnPassant.IsValid())
        return;

    tmp = p->m_rgAtkFr[p->m_EnPassant.BitOffset()] & p->m_rgMask[p->m_nTurn][Pawn];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        append_to_heap(heap, make_move(coord, p->m_EnPassant, M_ENPASSANT));
    }
}

/*
 * Generate all non-capturing moves from "square"
 */

void CPosition::GenFrom(const CSCoord& squareCoord, heap_t heap) {
    CPosition *p = this;
    const unsigned int dwSquare = squareCoord.BitOffset();

    /*
     * GenFrom generates non-capturing moves OUT of squareCoord, so that dwSquare
     * must hold a real friendly piece.  For a non-pawn it derives the move
     * targets from m_rgAtkTo[dwSquare]; if that attack row is stale for an empty
     * (or wrong-colour) dwSquare it would emit a move out of an empty dwSquare,
     * which DoMove traps.  Catch the corrupt attack table here, at its point of
     * use, before the bad move is ever generated (no-op in release builds).
     */
    AMY_ASSERT(p->m_rgPiece[dwSquare] != Neutral &&
                   SAME_COLOR(p->m_rgPiece[dwSquare], p->m_nTurn),
               "GenFrom: source square L%d/F%d/R%d (offset %u) holds no friendly "
               "piece (piece=%d)\n",
               (int)squareCoord.m_nLevel, (int)squareCoord.m_nFile,
               (int)squareCoord.m_nRank, (unsigned)dwSquare,
               (int)p->m_rgPiece[dwSquare]);

    if (TYPE(p->m_rgPiece[dwSquare]) != Pawn) {
        CBitBoard tmp;

        tmp = p->m_rgAtkTo[dwSquare] & ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

        while (tmp) {
            CSCoord coord = (tmp).FindSetBitCoord();
            tmp.ClearLowestBit();
            append_to_heap(heap, make_move(squareCoord, coord, 0));
        }

        /* Generate castling moves
         * we will check legality later...
         * Castling is only valid on the main board (level 7).
         */

        if (TYPE(p->m_rgPiece[dwSquare]) == King && squareCoord.m_nLevel == MAIN_LEVEL) {
            if (p->m_bCastle & CastleMask[p->m_nTurn][0]) {
                /* OK, we might castle king p->m_nTurn */
                append_to_heap(heap, make_move(p->m_nTurn == White ? CASTLE_E1 : CASTLE_E8,
                                               p->m_nTurn == White ? CASTLE_G1 : CASTLE_G8,
                                               M_SCASTLE));
            }
            if (p->m_bCastle & CastleMask[p->m_nTurn][1]) {
                append_to_heap(heap, make_move(p->m_nTurn == White ? CASTLE_E1 : CASTLE_E8,
                                               p->m_nTurn == White ? CASTLE_C1 : CASTLE_C8,
                                               M_LCASTLE));
            }
        }
    } else {
        const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[squareCoord.m_nLevel]);
        const int nDirection = (p->m_nTurn == White) ? 1 : -1;
        const uint16_t nNewRank =
            static_cast<uint16_t>(static_cast<int>(squareCoord.m_nRank) + nDirection);
        if (nNewRank >= wWidth)
            return;
        CSCoord sqCoord(squareCoord.m_nLevel, squareCoord.m_nFile, nNewRank);
        uint16_t wSq = sqCoord.BitOffset();

        if (p->m_rgPiece[wSq] == Neutral) {
            if (is_promo_square(sqCoord)) {
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Queen, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Knight, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Rook, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Bishop, 0));
            } else if (pawn_may_move_to(sqCoord)) {
                append_to_heap(heap, make_move(squareCoord, sqCoord, 0));

                /* The two-square double push (and therefore en passant) is
                 * only allowed on the main board (level h). On every other
                 * level pawns advance a single square at a time. */
                const uint16_t nHomeRank =
                    static_cast<uint16_t>((p->m_nTurn == White) ? 1 : (wWidth - 2));
                if (squareCoord.m_nLevel == MAIN_LEVEL && squareCoord.m_nRank == nHomeRank) {
                    const uint16_t nDblRank =
                        static_cast<uint16_t>(static_cast<int>(squareCoord.m_nRank) + 2 * nDirection);
                    if (nDblRank < wWidth) {
                        CSCoord dblCoord(squareCoord.m_nLevel, squareCoord.m_nFile, nDblRank);
                        wSq = dblCoord.BitOffset();
                        if (p->m_rgPiece[wSq] == Neutral) {
                            append_to_heap(heap, make_move(squareCoord, dblCoord, M_PAWND));
                        }
                    }
                }
            }
        }
    }
}

/*
 * Test if castling is legal
 */

bool CPosition::MayCastle(CMove move) {
    CPosition *p = this;
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord kingHome(static_cast<uint16_t>((p->m_nTurn == White) ? CASTLE_E1 : CASTLE_E8));
    /* Sometimes there might be a legal castling move, but for the
       wrong p->m_nTurn, probably from the Countermove table */
    if (fromCoord.m_nLevel != kingHome.m_nLevel || fromCoord.m_nFile != kingHome.m_nFile ||
        fromCoord.m_nRank != kingHome.m_nRank)
        return false;

    /* The castling-rights flags can be inconsistent with the actual piece
     * placement: they may be loaded verbatim from an EPD/FEN, or arrive as a
     * stale castle move from the hash/countermove tables.  In the 4D variant
     * the king and rooks need not sit on the main-level home squares even when
     * rights are set, so verify the pieces are really there before allowing a
     * castle.  Without this, DoCastle would shuffle non-existent pieces and
     * corrupt the board (mask bits set on empty squares). */
    if (TYPE(p->m_rgPiece[kingHome.BitOffset()]) != King ||
        !SAME_COLOR(p->m_rgPiece[kingHome.BitOffset()], p->m_nTurn))
        return false;

    if (p->InCheck(p->m_nTurn))
        return false;

    /* king p->m_nTurn castling */
    if (move.IsShortCastle() && (p->m_bCastle & CastleMask[p->m_nTurn][0])) {
        int nFs = (p->m_nTurn == White ? CASTLE_F1 : CASTLE_F8);
        int nGs = (p->m_nTurn == White ? CASTLE_G1 : CASTLE_G8);
        int nHs = (p->m_nTurn == White ? CASTLE_H1 : CASTLE_H8);

        /* The king-side rook must actually be on its home square */
        if (TYPE(p->m_rgPiece[nHs]) != Rook ||
            !SAME_COLOR(p->m_rgPiece[nHs], p->m_nTurn))
            return false;

        /* Check if f and g square are empty */
        if (p->m_rgPiece[nFs] == Neutral && p->m_rgPiece[nGs] == Neutral) {
            /* Check if f and g square are not attacked by opponent */
            if ((p->m_rgAtkFr[nFs] | p->m_rgAtkFr[nGs]) & p->m_rgMask[OPP(p->m_nTurn)][0])
                return false;
            else
                return true;
        }
    }

    /* queen p->m_nTurn castling */
    if (move.IsLongCastle() && (p->m_bCastle & CastleMask[p->m_nTurn][1])) {
        int nAs = (p->m_nTurn == White ? CASTLE_A1 : CASTLE_A8);
        int nBs = (p->m_nTurn == White ? CASTLE_B1 : CASTLE_B8);
        int nCs = (p->m_nTurn == White ? CASTLE_C1 : CASTLE_C8);
        int nDs = (p->m_nTurn == White ? CASTLE_D1 : CASTLE_D8);

        /* The queen-side rook must actually be on its home square */
        if (TYPE(p->m_rgPiece[nAs]) != Rook ||
            !SAME_COLOR(p->m_rgPiece[nAs], p->m_nTurn))
            return false;

        /* Check if b, c and d square are empty */
        if (p->m_rgPiece[nBs] == Neutral && p->m_rgPiece[nCs] == Neutral &&
            p->m_rgPiece[nDs] == Neutral) {
            /* Check if c and d square are not attacked by opponent */
            if ((p->m_rgAtkFr[nCs] | p->m_rgAtkFr[nDs]) & p->m_rgMask[OPP(p->m_nTurn)][0])
                return false;
            else
                return true;
        }
    }

    return false;
}

/*
 * Test if a move is legal
 */

bool CPosition::LegalMove(CMove move) {
    CPosition *p = this;
    const CSCoord& frCoord = move.GetFromCoord();
    if (!frCoord.IsValid())
        return false;
        
    const CSCoord& toCoord = move.GetToCoord();
    if (!toCoord.IsValid())
        return false;

    const uint16_t wFr = frCoord.BitOffset();
    const uint16_t wTo = toCoord.BitOffset();

    if (move == M_NONE || move == M_NULL)
        return false;

    /* There must be a piece on the square */
    if (!SAME_COLOR(p->m_rgPiece[wFr], p->m_nTurn))
        return false;

    /* if a promotion, moving piece must be a pawn */
    if (move.HasPromotion() && TYPE(p->m_rgPiece[wFr]) != Pawn)
        return false;

    /* A promotion is only legal when the destination is an actual promotion
     * square.  In the 3D variant promotion depends on the destination square
     * (is_promo_square), not on the pawn's source rank, so a well-formed
     * promotion always targets a promotion square.  A promotion onto a
     * non-promotion square indicates a malformed move (for example a mis-encoded
     * generator move) and must be rejected before it can corrupt the board. */
    if (move.HasPromotion() && !is_promo_square(toCoord)) {
        AMY_ASSERT(false,
                   "promotion move targets non-promotion square: from %u to %u\n",
                   wFr, wTo);
        return false;
    }

    /* if the move is a pawn move to the 1st/8th rank, it must be
     * be a promotion.
     */
    if (TYPE(p->m_rgPiece[wFr]) == Pawn && !move.HasPromotion()) {
        const uint16_t wLevelWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[toCoord.m_nLevel]);
        if (toCoord.m_nRank == 0 || toCoord.m_nRank == (wLevelWidth - 1))
            return false;
    }

    if (move.IsCapture()) {
        /* There must be an enemy piece on the target square, and we
         * must attack that square
         */

        if (!SAME_COLOR(p->m_rgPiece[wTo], OPP(p->m_nTurn)) ||
            !p->m_rgAtkTo[wFr].TstBit(wTo)) {
            return false;
        }
        return true;
    } else if (move.IsEnPassant()) {
        /* The moving piece must be a pawn, and the target square must be
         * the enpassant square
         */

        if (!p->m_EnPassant.IsValid())
            return false;
        if (TYPE(p->m_rgPiece[wFr]) != Pawn || wTo != p->m_EnPassant.BitOffset())
            return false;
        if (!p->m_rgAtkTo[wFr].TstBit(wTo))
            return false;

        return true;
    } else if (move.IsCastle()) {
        /* Call the castling test routine */
        return p->MayCastle(move);
    } else {
        /* target sqaure must be empty */
        if (p->m_rgPiece[wTo] != Neutral)
            return false;

        if (TYPE(p->m_rgPiece[wFr]) != Pawn) {
            /* if no pawn, we must attack to square */
            if (!p->m_rgAtkTo[wFr].TstBit(wTo))
                return false;
            if (move.IsPawnDoublePush())
                return false;
            return true;
        } else {
            /* use NextPos array to check if legal move */
            const uint16_t wLevelWidth =
                static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[frCoord.m_nLevel]);
            const int nRankStep = (p->m_nTurn == White ? 1 : -1);
            int nTtRank = frCoord.m_nRank + nRankStep;
            if (nTtRank < 0 || nTtRank >= wLevelWidth)
                return false;
            uint16_t wTt = CSCoord(frCoord.m_nLevel, frCoord.m_nFile,
                                  static_cast<uint16_t>(nTtRank))
                              .BitOffset();
            if (move.IsPawnDoublePush()) {
                /* Double pushes (and therefore en passant) exist only on the
                 * main board's home rank.  Reject any double-push flagged move
                 * that does not originate there - e.g. a stale hash/killer move
                 * referring to a pawn on another level - so legality stays
                 * level-aware. */
                const uint16_t nHomeRank = static_cast<uint16_t>(
                    (p->m_nTurn == White) ? 1 : (wLevelWidth - 2));
                if (frCoord.m_nLevel != MAIN_LEVEL ||
                    frCoord.m_nRank != nHomeRank) {
                    return false;
                }
                if (p->m_rgPiece[wTt] != Neutral)
                    return false;
                nTtRank += nRankStep;
                if (nTtRank < 0 || nTtRank >= wLevelWidth)
                    return false;
                wTt = CSCoord(frCoord.m_nLevel, frCoord.m_nFile,
                             static_cast<uint16_t>(nTtRank))
                         .BitOffset();
            }
            if (wTt != wTo)
                return false;

            if (p->m_nTurn == White && toCoord.m_nRank == (wLevelWidth - 1) &&
                !move.HasPromotion())
                return false;
            if (p->m_nTurn == Black && toCoord.m_nRank == 0 && !move.HasPromotion())
                return false;

            return true;
        }
    }
    /* return false; */ /* never reached */
}

/*
 * Test wether a move will give check.
 *
 * In 4D the precomputed 2D endpoint masks (KnightEPM/KingEPM) and the
 * pawn/slider geometry used by the old heuristic do not capture cross-level
 * attacks or the many sliding directions, so it produced both false positives
 * (e.g. a pawn "attacking" the square straight ahead) and large numbers of
 * false negatives (missed cross-level and discovered checks).  Determine the
 * answer exactly by making the move, testing whether the side to move is now
 * in check, and unmaking it.  This is the same make/test/unmake pattern used
 * for legality elsewhere and is inherently level-correct.
 */

bool CPosition::IsCheckingMove(CMove move) {
    CPosition *p = this;
    /*
     * A move that captures the opponent's king is illegal and must never be
     * passed to DoMove (which asserts on king captures). Abstractly it is not
     * a "checking" move either: if the side to move has just captured the
     * enemy king it has already won the game and therefore cannot itself be
     * left in check. Report it as a non-checking move so callers that use this
     * predicate (e.g. futility pruning) never make the move.
     */
    if (p->IsKingCapture(move)) {
        const CSCoord &frCoord = move.GetFromCoord();
        const CSCoord &toCoord = move.GetToCoord();
        PrintDebug(9,
                   "IsCheckingMove: king-capture move from L%d/F%d/R%d to "
                   "L%d/F%d/R%d treated as non-checking (illegal position)\n",
                   frCoord.m_nLevel, frCoord.m_nFile, frCoord.m_nRank,
                   toCoord.m_nLevel, toCoord.m_nFile, toCoord.m_nRank);
        return false;
    }
    p->DoMove(move);
    const bool fGivesCheck = p->InCheck(p->m_nTurn);
    p->UndoMove(move);
    return fGivesCheck;
}

/*
 * Test whether a move captures the opponent's king. Reaching such a move means
 * the position is illegal (the previous move left a king in check), so this is
 * used by the search to recognise and prune the offending branch before any
 * king-capturing DoMove is attempted.
 */

bool CPosition::IsKingCapture(CMove move) const {
    return move.IsCapture() &&
           TYPE(m_rgPiece[move.GetToCoord().BitOffset()]) == King;
}

/*
 * Generate all non-capturing checking moves. Actually this routine only
 * generates 'candidate' moves for checks. Some move generated here may
 * not be checks!
 */

void CPosition::GenChecks(heap_t heap) {
    CPosition *p = this;
    CBitBoard tmp;
    CBitBoard fr;
    int nKp = p->m_rgKingSq[OPP(p->m_nTurn)].BitOffset();
    CBitBoard *pIp = InterPath[nKp];
    CBitBoard fsq = p->m_rgMask[p->m_nTurn][0];
    CBitBoard all = (p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

    /* First find all blockers, i.e. pieces that give check when they move
     * from their current square
     */

    tmp = (p->m_rgMask[p->m_nTurn][Bishop] | p->m_rgMask[p->m_nTurn][Queen]) & BishopEPM[nKp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (pIp[i] && !(pIp[i] & p->m_rgMask[OPP(p->m_nTurn)][0])) {
            CBitBoard tmp2 = p->m_rgMask[p->m_nTurn][0] & pIp[i];

            if ((tmp2).CountBits() == 1) {
                CSCoord coord = (tmp2).FindSetBitCoord();

                if (fsq.TstBit(coord.BitOffset())) {
                    p->GenFrom(coord, heap);
                    fsq.ClrBit(coord.BitOffset());
                }
            }
        }
    }

    tmp = (p->m_rgMask[p->m_nTurn][Rook] | p->m_rgMask[p->m_nTurn][Queen]) & RookEPM[nKp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (pIp[i] && !(pIp[i] & p->m_rgMask[OPP(p->m_nTurn)][0])) {
            CBitBoard tmp2 = p->m_rgMask[p->m_nTurn][0] & pIp[i];

            if ((tmp2).CountBits() == 1) {
                CSCoord coord = (tmp2).FindSetBitCoord();

                if (fsq.TstBit(coord.BitOffset())) {
                    p->GenFrom(coord, heap);
                    fsq.ClrBit(coord.BitOffset());
                }
            }
        }
    }

    /* Find direct checks by Bishop or Queen */
    tmp = BishopEPM[nKp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Bishop] | p->m_rgMask[p->m_nTurn][Queen];
    fr &= fsq;

    while (fr) {
        int nSq = (fr).FindSetBit();
        CBitBoard tmp2 = p->m_rgAtkTo[nSq] & tmp;
        fr.ClearLowestBit();

        while (tmp2) {
            int nSq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            if (InterPath[nKp][nSq2] & all)
                continue;
            append_to_heap(heap, make_move(nSq, nSq2, 0));
        }
    }

    /* Find direct checks by Rook or Queen */
    tmp = RookEPM[nKp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Rook] | p->m_rgMask[p->m_nTurn][Queen];
    fr &= fsq;

    while (fr) {
        int nSq = (fr).FindSetBit();
        CBitBoard tmp2 = p->m_rgAtkTo[nSq] & tmp;
        fr.ClearLowestBit();

        while (tmp2) {
            int nSq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            if (InterPath[nKp][nSq2] & all)
                continue;
            append_to_heap(heap, make_move(nSq, nSq2, 0));
        }
    }

    /* Find direct checks by Knight */
    tmp = KnightEPM[nKp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Knight];
    fr &= fsq;

    while (fr) {
        int nSq = (fr).FindSetBit();
        CBitBoard tmp2;

        fr.ClearLowestBit();
        tmp2 = p->m_rgAtkTo[nSq] & tmp;

        while (tmp2) {
            int nSq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            append_to_heap(heap, make_move(nSq, nSq2, 0));
        }
    }

    /*
     * last find pawn checks
     */

    tmp = (p->m_nTurn == White) ? BPawnEPM[nKp] : WPawnEPM[nKp];
    tmp &= ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

    while (tmp) {
        int nSq = (tmp).FindSetBit();
        tmp.ClearLowestBit();

        if (p->m_nTurn == White) {
            const CSCoord sqCoord(static_cast<uint16_t>(nSq));
            const int nPawnOff = nSq - static_cast<int>(CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]);
            if (nPawnOff >= 0 && p->m_rgPiece[nPawnOff] == Pawn) {
                append_to_heap(heap, make_move(nPawnOff, nSq, 0));
            }
        } else {
            const CSCoord sqCoord(static_cast<uint16_t>(nSq));
            const int nPawnOff = nSq + static_cast<int>(CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]);
            if (nPawnOff < static_cast<int>(CBitBoard::SIZE) && p->m_rgPiece[nPawnOff] == -Pawn) {
                append_to_heap(heap, make_move(nPawnOff, nSq, 0));
            }
        }
    }
}

/*
 * Repetition check
 * if mode = true, count the number of repetitions of current position
 * if mode = false, only check if current position is repeated
 */

int CPosition::Repeated(int nMode) {
    CPosition *p = this;
    int i, nCnt = 0;
    struct SGameLog *pGl;

    if (p->m_wPly == 0)
        return 0;

    if (p->m_pActLog->gl_IrrevCount >= 100)
        return 3;

    pGl = p->m_pActLog - 1;
    for (i = p->m_pActLog->gl_IrrevCount; i > 0; i--, pGl--) {
        if (pGl->gl_HashKey == p->m_ullHKey) {
            if (nMode)
                nCnt++;
            else
                return true;
        }
    }

    return nCnt;
}

/*
 * Generate the SAN (Standard Algebraic Notation) for a move.
 *
 * Args:
 *   p: pointer to the current position
 *   move: the legal move in position to generate the SAN for
 *   buffer: a pointer to a buffer to place the generated string in.
 *           There is no bounds checking, so the buffer should be large
 *           enough to hold the generated SAN.
 *
 * Returns:
 *   the pointer to the generated string (buffer)
 */
char *CPosition::SAN(CMove move, char *pszBuffer) {
    CPosition *p = this;
    char *pszX = pszBuffer;

    const CSCoord& toCoord = move.GetToCoord();
    const CSCoord& frCoord = move.GetFromCoord();
    const uint16_t wFr = frCoord.BitOffset();
    int8_t nTp = TYPE(p->m_rgPiece[wFr]);

    if (move.IsCastle()) {
        *(pszX++) = 'O';
        *(pszX++) = '-';
        *(pszX++) = 'O';
        if (move.IsLongCastle()) {
            *(pszX++) = '-';
            *(pszX++) = 'O';
        }
    } else {
        /* Full explicit notation: always emit the moving piece's letter
         * (including 'P' for pawns), the complete source square
         * (level + file + rank) and the complete destination square, so the
         * mover and both squares are stated unambiguously.  This avoids any
         * confusion about which piece is being moved in 4D, where several
         * same-type pieces (or pawns) can share file and rank across levels. */
        *(pszX++) = PieceName[nTp];
        *(pszX++) = 'a' + frCoord.m_nLevel;
        *(pszX++) = 'a' + frCoord.m_nFile;
        *(pszX++) = '1' + frCoord.m_nRank;

        if (move.IsCapture() || move.IsEnPassant())
            *(pszX++) = 'x';

        *(pszX++) = 'a' + toCoord.m_nLevel;
        *(pszX++) = 'a' + toCoord.m_nFile;
        *(pszX++) = '1' + toCoord.m_nRank;

        if (move.HasPromotion()) {
            *(pszX++) = '=';
            *(pszX++) = PieceName[PromoType(move)];
        }
    }

    p->DoMove(move);
    if (p->InCheck(p->m_nTurn)) {
        if (!p->LegalMoves(NULL))
            *(pszX++) = '#';
        else
            *(pszX++) = '+';
    }
    p->UndoMove(move);

    *pszX = '\0';
    return pszBuffer;
}

/*
 * Generate the ICS SAN for a move
 */

char *ICS_SAN(CMove move) {
    static char szBuffer[16];
    char *pszX = szBuffer;

    const CSCoord toCoord = move.GetToCoord();
    const CSCoord frCoord = move.GetFromCoord();

    *(pszX++) = 'a' + frCoord.m_nLevel;
    *(pszX++) = 'a' + frCoord.m_nFile;
    *(pszX++) = '1' + frCoord.m_nRank;
    if (move.IsCapture() || move.IsEnPassant()) {
        *(pszX++) = 'x';
    }
    *(pszX++) = 'a' + toCoord.m_nLevel;
    *(pszX++) = 'a' + toCoord.m_nFile;
    *(pszX++) = '1' + toCoord.m_nRank;
    if (move.HasPromotion()) {
        *(pszX++) = PieceName[PromoType(move)];
    }
    *pszX = '\0';
    return szBuffer;
}

/*
 * Parse a move string in e2e4 notation
 */

CMove parse_gsan_internal(CPosition *p, char *pszSan, heap_t heap) {
    if (!strncmp(pszSan, "O-O-O", 5) || !strncmp(pszSan, "o-o-o", 5) ||
        !strncmp(pszSan, "0-0-0", 5)) {
        CMove move(CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8)),
                   CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_C1 : CASTLE_C8)), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
    }

    if (!strncmp(pszSan, "O-O", 3) || !strncmp(pszSan, "o-o", 3) ||
        !strncmp(pszSan, "0-0", 3)) {
        CMove move(CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8)),
                   CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_G1 : CASTLE_G8)), M_SCASTLE);
        if (p->MayCastle(move))
            return move;
    }

    if (strlen(pszSan) < 6) {
        return M_NONE;
    }

    (void)p->LegalMoves(heap);

    int nFrLevel = *(pszSan + 0) - 'a';
    int nFrFile  = *(pszSan + 1) - 'a';
    int nFrRank  = *(pszSan + 2) - '1';
    int nToLevel = *(pszSan + 3) - 'a';
    int nToFile  = *(pszSan + 4) - 'a';
    int nToRank  = *(pszSan + 5) - '1';

    if (!CSCoord::IsValid(nFrLevel, nFrFile, nFrRank) ||
        !CSCoord::IsValid(nToLevel, nToFile, nToRank))
        return M_NONE;

    int nFr = CSCoord(nFrLevel, nFrFile, nFrRank).BitOffset();
    int nTo = CSCoord(nToLevel, nToFile, nToRank).BitOffset();

    SFromToIndex mask(nFr , nTo);

    for (unsigned int i = heap->current_section->start;
         i < heap->current_section->end; i++) {
        CMove move = heap->data[i];
        if (move.GetFromToIndex() == mask) {
            if (move.HasPromotion() && strlen(pszSan) >= 7) {
                char cPromotion = *(pszSan + 6);
                move.ClearPromotion();

                if (cPromotion == 'q' || cPromotion == 'Q') {
                    move.SetPromotionType(Queen);
                } else if (cPromotion == 'r' || cPromotion == 'R') {
                    move.SetPromotionType(Rook);
                } else if (cPromotion == 'n' || cPromotion == 'N') {
                    move.SetPromotionType(Knight);
                } else if (cPromotion == 'b' || cPromotion == 'B') {
                    move.SetPromotionType(Bishop);
                } else {
                    return M_NONE;
                }
                return move;
            } else
                return move;
        }
    }
    return M_NONE;
}

CMove CPosition::ParseGSAN(char *pszSan) {
    CPosition *p = this;
    heap_t heap = allocate_heap();
    CMove move = parse_gsan_internal(p, pszSan, heap);
    free_heap(heap);

    return move;
}

/*
 * Parse a move string in e2e4 notation against a supplied move list
 */

CMove ParseGSANList(char *pszSan, Color nSide, CMove *pMvs, int nCnt) {
    int nFr, nTo;
    int i;

    if (!strncmp(pszSan, "O-O-O", 5) || !strncmp(pszSan, "o-o-o", 5) ||
        !strncmp(pszSan, "0-0-0", 5)) {
        CMove move(CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_E1 : CASTLE_E8)),
                   CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_C1 : CASTLE_C8)), M_LCASTLE);

        for (i = 0; i < nCnt; i++)
            if (move == pMvs[i])
                return move;
        return M_NONE;
    }

    if (!strncmp(pszSan, "O-O", 3) || !strncmp(pszSan, "o-o", 3) ||
        !strncmp(pszSan, "0-0", 3)) {
        CMove move(CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_E1 : CASTLE_E8)),
                   CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_G1 : CASTLE_G8)), M_SCASTLE);

        for (i = 0; i < nCnt; i++)
            if (move == pMvs[i])
                return move;
        return M_NONE;
    }

    int nFrLevel = *(pszSan + 0) - 'a';
    int nFrFile  = *(pszSan + 1) - 'a';
    int nFrRank  = *(pszSan + 2) - '1';
    int nToLevel = *(pszSan + 3) - 'a';
    int nToFile  = *(pszSan + 4) - 'a';
    int nToRank  = *(pszSan + 5) - '1';

    if (!CSCoord::IsValid(nFrLevel, nFrFile, nFrRank) ||
        !CSCoord::IsValid(nToLevel, nToFile, nToRank))
        return M_NONE;

    nFr = CSCoord(nFrLevel, nFrFile, nFrRank).BitOffset();
    nTo = CSCoord(nToLevel, nToFile, nToRank).BitOffset();

    SFromToIndex mask(nFr , nTo);

    for (i = 0; i < nCnt; i++) {
        if (pMvs[i].GetFromToIndex() == mask) {
            if (pMvs[i].HasPromotion()) {
                char cPromotion = *(pszSan + 6);
                CMove move = pMvs[i];
                move.ClearPromotion();

                if (cPromotion == 'q' || cPromotion == 'Q') {
                    move.SetPromotionType(Queen);
                } else if (cPromotion == 'r' || cPromotion == 'R') {
                    move.SetPromotionType(Rook);
                } else if (cPromotion == 'n' || cPromotion == 'N') {
                    move.SetPromotionType(Knight);
                } else if (cPromotion == 'b' || cPromotion == 'B') {
                    move.SetPromotionType(Bishop);
                } else {
                    return M_NONE;
                }
                return move;
            } else
                return pMvs[i];
        }
    }
    return M_NONE;
}

/*
 * Test a pseudolegal move for legality
 */

static bool TryMove(CPosition *p, CMove move) {
    bool fTmp;
    p->DoMove(move);
    fTmp = p->InCheck(OPP(p->GetTurn()));
    p->UndoMove(move);

    return !fTmp;
}

/*
 * Parse a move string (in SAN)
 */
static CMove parse_san_with_heap(CPosition *p, const char *pszSan, heap_t heap) {
    int nTp = Neutral;
    int nFrk = -1, nFfl = -1, nFll = -1, nTll = -1, nTrk = -1, nTfl = -1;
    int nPro = 0;
    unsigned int i;
    CMove move;

    /* Check castling first */

    if (!strncmp(pszSan, "O-O-O", 5) || !strncmp(pszSan, "o-o-o", 5) ||
        !strncmp(pszSan, "0-0-0", 5)) {
        move = CMove(CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8)),
                     CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_C1 : CASTLE_C8)), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
        else
            return M_NONE;
    }

    if (!strncmp(pszSan, "O-O", 3) || !strncmp(pszSan, "o-o", 3) ||
        !strncmp(pszSan, "0-0", 3)) {
        move = CMove(CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8)),
                     CSCoord(static_cast<uint16_t>(p->GetTurn() == White ? CASTLE_G1 : CASTLE_G8)), M_SCASTLE);
        if (p->MayCastle(move))
            return move;
        else
            return M_NONE;
    }

    p->PLegalMoves(heap);

    /* Find the pszEnd of the meaningful SAN string (strip trailing +/# check
     * indicators) */
    const char *pszEnd = pszSan + strlen(pszSan);
    while (pszEnd > pszSan && (*(pszEnd - 1) == '+' || *(pszEnd - 1) == '#'))
        pszEnd--;

    /* Handle promotion suffix =X */
    const char *pszEq = NULL;
    for (const char *q = pszSan; q < pszEnd; q++) {
        if (*q == '=') {
            pszEq = q;
            break;
        }
    }
    if (pszEq != NULL) {
        if (pszEq + 1 >= pszEnd)
            return M_NONE;
        char cPc = *(pszEq + 1);
        if (cPc == 'Q')
            nPro = Queen;
        else if (cPc == 'R')
            nPro = Rook;
        else if (cPc == 'B')
            nPro = Bishop;
        else if (cPc == 'N')
            nPro = Knight;
        else
            return M_NONE;
        pszEnd = pszEq;
    }

    /* Destination square: last 3 meaningful chars =
     *   level letter (a-o) + file letter (a-h) + rank digit (1-8) */
    if (pszEnd - pszSan < 3)
        return M_NONE;

    char cRankCh  = *(pszEnd - 1);
    char cFileCh  = *(pszEnd - 2);
    char cLevelCh = *(pszEnd - 3);

    if (cRankCh < '1' || cRankCh > '8')
        return M_NONE;
    if (cFileCh < 'a' || cFileCh > 'h')
        return M_NONE;
    if (cLevelCh < 'a' || cLevelCh > 'o')
        return M_NONE;

    nTrk = cRankCh  - '1';
    nTfl = cFileCh  - 'a';
    nTll = cLevelCh - 'a';

    if (!CSCoord::IsValid(nTll, nTfl, nTrk))
        return M_NONE;

    /* Process pszPrefix (everything before the destination 3-char square) */
    const char *pszPrefix     = pszSan;
    const char *pszPrefixEnd = pszEnd - 3;

    /* Optional piece letter at the start of the pszPrefix.  The generator now
     * emits an explicit 'P' for pawns, so accept it (and keep accepting the
     * traditional pawn form with no leading letter for backward
     * compatibility). */
    if (pszPrefix < pszPrefixEnd) {
        switch (*pszPrefix) {
        case 'P': nTp = Pawn;   pszPrefix++; break;
        case 'N': nTp = Knight; pszPrefix++; break;
        case 'B': nTp = Bishop; pszPrefix++; break;
        case 'R': nTp = Rook;   pszPrefix++; break;
        case 'Q': nTp = Queen;  pszPrefix++; break;
        case 'K': nTp = King;   pszPrefix++; break;
        default:  break;
        }
    }

    /* Remaining pszPrefix: source-square disambiguation.  Standard 2D
     * disambiguation is an optional from-file (a-h) and/or from-rank (1-8).
     * In 4D, when two same-type pieces share file and rank but sit on
     * different levels, the generator emits the FULL source square
     * (level letter a-o + file letter a-h + rank digit 1-8); detect that
     * 3-character form first so SAN round-trips. */
    char szDis[8];
    int ndis = 0;
    for (const char *q = pszPrefix; q < pszPrefixEnd; q++) {
        if (*q == 'x' || *q == '+' || *q == '#') {
            continue;
        }
        if (ndis >= static_cast<int>(sizeof(szDis))) {
            return M_NONE;
        }
        szDis[ndis++] = *q;
    }

    if (ndis == 3 && szDis[0] >= 'a' && szDis[0] <= 'o' && szDis[1] >= 'a' &&
        szDis[1] <= 'h' && szDis[2] >= '1' && szDis[2] <= '8') {
        nFll = szDis[0] - 'a';
        nFfl = szDis[1] - 'a';
        nFrk = szDis[2] - '1';
    } else {
        for (int k = 0; k < ndis; k++) {
            char cDisambiguation = szDis[k];
            if (cDisambiguation >= 'a' && cDisambiguation <= 'h') {
                nFfl = cDisambiguation - 'a';
            } else if (cDisambiguation >= '1' && cDisambiguation <= '8') {
                nFrk = cDisambiguation - '1';
            } else {
                return M_NONE;
            }
        }
    }

    if (nTp == Neutral)
        nTp = Pawn;

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        move = heap->data[i];
        const CSCoord& frCoord = move.GetFromCoord();
        const CSCoord& toCoord = move.GetToCoord();
        const uint16_t wFr = frCoord.BitOffset();

        if (TYPE(p->GetPiece(wFr)) != nTp)
            continue;
        if (toCoord.m_nLevel != nTll || toCoord.m_nFile != nTfl ||
            toCoord.m_nRank != nTrk)
            continue;
        if (nFll != -1 && frCoord.m_nLevel != nFll)
            continue;
        if (nFfl != -1 && frCoord.m_nFile != nFfl)
            continue;
        if (nFrk != -1 && frCoord.m_nRank != nFrk)
            continue;
        if (nPro && (PromoType(move) != nPro))
            continue;
        if (!TryMove(p, move))
            continue;

        return move;
    }

    return M_NONE;
}

CMove CPosition::ParseSAN(const char *pszSan) {
    CPosition *p = this;
    heap_t heap = allocate_heap();
    CMove move = parse_san_with_heap(p, pszSan, heap);
    free_heap(heap);
    return move;
}

/*
 * Parse a move string (in SAN) against supplied move list
 */

CMove ParseSANList(char *pszSan, Color nSide, CMove *pMvs, int nCnt, int *pmap) {
    int nTp = Neutral;
    int nFrk = -1, nFfl = -1, nFll = -1, nTll = -1, nTrk = -1, nTfl = -1;
    int nPro = 0;
    CMove move;
    int i;

    /* Check castling first */

    if (!strncmp(pszSan, "O-O-O", 5) || !strncmp(pszSan, "o-o-o", 5) ||
        !strncmp(pszSan, "0-0-0", 5)) {
        move = CMove(CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_E1 : CASTLE_E8)),
                     CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_C1 : CASTLE_C8)), M_LCASTLE);
        for (i = 0; i < nCnt; i++)
            if (move == pMvs[i])
                return move;
        return M_NONE;
    }

    if (!strncmp(pszSan, "O-O", 3) || !strncmp(pszSan, "o-o", 3) ||
        !strncmp(pszSan, "0-0", 3)) {
        move = CMove(CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_E1 : CASTLE_E8)),
                     CSCoord(static_cast<uint16_t>(nSide == White ? CASTLE_G1 : CASTLE_G8)), M_SCASTLE);
        for (i = 0; i < nCnt; i++)
            if (move == pMvs[i])
                return move;
        return M_NONE;
    }

    /* Find the pszEnd of the meaningful SAN string (strip trailing +/# check
     * indicators) */
    const char *pszEnd = pszSan + strlen(pszSan);
    while (pszEnd > pszSan && (*(pszEnd - 1) == '+' || *(pszEnd - 1) == '#'))
        pszEnd--;

    /* Handle promotion suffix =X */
    const char *pszEq = NULL;
    for (const char *q = pszSan; q < pszEnd; q++) {
        if (*q == '=') {
            pszEq = q;
            break;
        }
    }
    if (pszEq != NULL) {
        if (pszEq + 1 >= pszEnd)
            return M_NONE;
        char cPc = *(pszEq + 1);
        if (cPc == 'Q')
            nPro = Queen;
        else if (cPc == 'R')
            nPro = Rook;
        else if (cPc == 'B')
            nPro = Bishop;
        else if (cPc == 'N')
            nPro = Knight;
        else
            return M_NONE;
        pszEnd = pszEq;
    }

    /* Destination square: last 3 meaningful chars =
     *   level letter (a-o) + file letter (a-h) + rank digit (1-8) */
    if (pszEnd - pszSan < 3)
        return M_NONE;

    char cRankCh  = *(pszEnd - 1);
    char cFileCh  = *(pszEnd - 2);
    char cLevelCh = *(pszEnd - 3);

    if (cRankCh < '1' || cRankCh > '8')
        return M_NONE;
    if (cFileCh < 'a' || cFileCh > 'h')
        return M_NONE;
    if (cLevelCh < 'a' || cLevelCh > 'o')
        return M_NONE;

    nTrk = cRankCh  - '1';
    nTfl = cFileCh  - 'a';
    nTll = cLevelCh - 'a';

    if (!CSCoord::IsValid(nTll, nTfl, nTrk))
        return M_NONE;

    /* Process pszPrefix (everything before the destination 3-char square) */
    const char *pszPrefix     = pszSan;
    const char *pszPrefixEnd = pszEnd - 3;

    /* Optional piece letter at the start of the pszPrefix.  The generator now
     * emits an explicit 'P' for pawns, so accept it (and keep accepting the
     * traditional pawn form with no leading letter for backward
     * compatibility). */
    if (pszPrefix < pszPrefixEnd) {
        switch (*pszPrefix) {
        case 'P': nTp = Pawn;   pszPrefix++; break;
        case 'N': nTp = Knight; pszPrefix++; break;
        case 'B': nTp = Bishop; pszPrefix++; break;
        case 'R': nTp = Rook;   pszPrefix++; break;
        case 'Q': nTp = Queen;  pszPrefix++; break;
        case 'K': nTp = King;   pszPrefix++; break;
        default:  break;
        }
    }

    /* Remaining pszPrefix: source-square disambiguation.  Standard 2D
     * disambiguation is an optional from-file (a-h) and/or from-rank (1-8).
     * In 4D, when two same-type pieces share file and rank but sit on
     * different levels, the generator emits the FULL source square
     * (level letter a-o + file letter a-h + rank digit 1-8); detect that
     * 3-character form first so SAN round-trips. */
    char szDis[8];
    int ndis = 0;
    for (const char *q = pszPrefix; q < pszPrefixEnd; q++) {
        if (*q == 'x' || *q == '+' || *q == '#') {
            continue;
        }
        if (ndis >= static_cast<int>(sizeof(szDis))) {
            return M_NONE;
        }
        szDis[ndis++] = *q;
    }

    if (ndis == 3 && szDis[0] >= 'a' && szDis[0] <= 'o' && szDis[1] >= 'a' &&
        szDis[1] <= 'h' && szDis[2] >= '1' && szDis[2] <= '8') {
        nFll = szDis[0] - 'a';
        nFfl = szDis[1] - 'a';
        nFrk = szDis[2] - '1';
    } else {
        for (int k = 0; k < ndis; k++) {
            char cDisambiguation = szDis[k];
            if (cDisambiguation >= 'a' && cDisambiguation <= 'h') {
                nFfl = cDisambiguation - 'a';
            } else if (cDisambiguation >= '1' && cDisambiguation <= '8') {
                nFrk = cDisambiguation - '1';
            } else {
                return M_NONE;
            }
        }
    }

    if (nTp == Neutral)
        nTp = Pawn;

    for (i = 0; i < nCnt; i++) {
        const CSCoord& frCoord = pMvs[i].GetFromCoord();
        const CSCoord& toCoord = pMvs[i].GetToCoord();
        const uint16_t wFr = frCoord.BitOffset();

        if (TYPE(pmap[wFr]) != nTp)
            continue;
        if (toCoord.m_nLevel != nTll || toCoord.m_nFile != nTfl ||
            toCoord.m_nRank != nTrk)
            continue;
        if (nFll != -1 && frCoord.m_nLevel != nFll)
            continue;
        if (nFfl != -1 && frCoord.m_nFile != nFfl)
            continue;
        if (nFrk != -1 && frCoord.m_nRank != nFrk)
            continue;
        if (nPro && (PromoType(pMvs[i]) != nPro))
            continue;

        return pMvs[i];
    }

    return M_NONE;
}

/*
 * Generate all pseudolegal (!) moves
 * or test if there are any, if mvs = NULL
 */

void CPosition::PLegalMoves(heap_t heap) {
    CPosition *p = this;
    CBitBoard tmp;

    tmp = p->m_rgMask[OPP(p->m_nTurn)][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();

        p->GenTo(coord, heap);
    }

    tmp = p->m_rgMask[p->m_nTurn][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();

        p->GenFrom(coord, heap);
    }

    p->GenEnpas(heap);
}

/**
 * Generate all strictly legal moves.
 *
 * Returns:
 *     the number of generated moves
 */

void legal_moves_internal(CPosition *p, heap_t heap, heap_t pTmpHeap) {
    CBitBoard tmp;

    tmp = p->GetMask(OPP(p->GetTurn()), 0);
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(pTmpHeap);
        p->GenTo(coord, pTmpHeap);

        for (i = pTmpHeap->current_section->start;
             i < pTmpHeap->current_section->end; i++) {
            CMove move = pTmpHeap->data[i];
            /*
             * Capturing a king is never a legal move. In an arbitrary
             * (e.g. user-supplied) position the opponent's king may be left
             * en prise, in which case GenTo would emit a capture of it. Skip
             * such moves so they never reach DoMove (which traps on a king
             * capture).
             */
            if (TYPE(p->GetPiece(move.GetToCoord().BitOffset())) == King) {
                continue;
            }
            p->DoMove(move);
            if (!p->InCheck(OPP(p->GetTurn()))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }

        pop_section(pTmpHeap);
    }

    tmp = p->GetMask(p->GetTurn(), 0);
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(pTmpHeap);
        p->GenFrom(coord, pTmpHeap);

        for (i = pTmpHeap->current_section->start;
             i < pTmpHeap->current_section->end; i++) {
            CMove move = pTmpHeap->data[i];
            if ((move.IsCastle()) && !p->MayCastle(move))
                continue;

            /* Capturing a king is never a legal move (see note above). */
            if (TYPE(p->GetPiece(move.GetToCoord().BitOffset())) == King) {
                continue;
            }

            p->DoMove(move);
            if (!p->InCheck(OPP(p->GetTurn()))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }

        pop_section(pTmpHeap);
    }

    push_section(pTmpHeap);
    p->GenEnpas(pTmpHeap);
    {
        unsigned int i;
        for (i = pTmpHeap->current_section->start;
             i < pTmpHeap->current_section->end; i++) {
            CMove move = pTmpHeap->data[i];
            p->DoMove(move);
            if (!p->InCheck(OPP(p->GetTurn()))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }
    }
    pop_section(pTmpHeap);
}

int CPosition::LegalMoves(heap_t heap) {
    CPosition *p = this;
    heap_t pTmpHeap = allocate_heap();
    heap_t hDestination = heap;

    if (heap == NULL) {
        hDestination = allocate_heap();
    }

    legal_moves_internal(p, hDestination, pTmpHeap);

    int nCnt =
        hDestination->current_section->end - hDestination->current_section->start;

    if (heap == NULL) {
        free_heap(hDestination);
    }

    free_heap(pTmpHeap);

    return nCnt;
}

/*
 * Print the current position
 */

void CPosition::ShowPosition() {
    CPosition *p = this;
    const int nNumLevels = static_cast<int>(CBitBoard::NUM_LEVELS);
    for (int nLevel = nNumLevels - 1; nLevel >= 0; nLevel--) {
        const int nWidth = CBitBoard::LEVEL_WIDTH[nLevel];

        if (nLevel < (nNumLevels - 1)) {
            Print(0, "\n");
        }
        if (nNumLevels > 1) {
            Print(0, "      Level %c\n", static_cast<char>('a' + nLevel));
        }

        Print(0, "        ");
        for (int nFile = 0; nFile < nWidth; nFile++) {
            Print(0, "+---");
        }
        Print(0, "+\n");

        for (int nRk = nWidth - 1; nRk >= 0; nRk--) {
            char cIndicator =
                ((nRk == nWidth - 1) && (p->m_nTurn)) || ((nRk == 0) && (!p->m_nTurn)) ? '>' : ' ';
            Print(0, "    %c %c ", cIndicator, '1' + nRk);
            for (int nFl = 0; nFl < nWidth; nFl++) {
                const int nSquare = static_cast<int>(CSCoord(nLevel, nFl, nRk));

                Print(0, "|");
                if (p->m_EnPassant.IsValid() && nSquare == p->m_EnPassant.BitOffset())
                    Print(0, "<E>");
                else {
                    if (p->m_rgPiece[nSquare] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                    Print(0, "%c", PieceName[TYPE(p->m_rgPiece[nSquare])]);
                    if (p->m_rgPiece[nSquare] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                }
            }
            if (nRk == 4) {
                int nBit;
                Print(0, "|   Black (%5d, %5d)  ", p->m_rgnMaterial[Black],
                      p->m_rgnNonPawn[Black]);
                for (nBit = 0; nBit < 5; nBit++) {
                    Print(0, "%c",
                          (p->m_rgbMaterialSignature[Black] & (1 << nBit))
                              ? PieceName[nBit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (nRk == 3) {
                int nBit;
                Print(0, "|   White (%5d, %5d)  ", p->m_rgnMaterial[White],
                      p->m_rgnNonPawn[White]);
                for (nBit = 0; nBit < 5; nBit++) {
                    Print(0, "%c",
                          (p->m_rgbMaterialSignature[White] & (1 << nBit))
                              ? PieceName[nBit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (nRk == 6) {
                Print(0, "|   Hashkey: %llx\n", p->m_ullHKey);
            } else if (nRk == 1) {
                Print(0, "|   Index: %d\n", RECOGNIZER_INDEX(p));
            } else if (nRk == 0) {
                Print(0, "|   MateThreat: %d %d\n", MateThreat(p, White),
                      MateThreat(p, Black));
            } else {
                Print(0, "|\n");
            }

            Print(0, "        ");
            for (int nFile = 0; nFile < nWidth; nFile++) {
                Print(0, "+---");
            }
            Print(0, "+\n");
        }

        Print(0, "         ");
        for (int nFile = 0; nFile < nWidth; nFile++) {
            Print(0, "  %c ", 'a' + nFile);
        }
        Print(0, "\n");
    }
}

/*
 * Display all legal moves.
 */

void CPosition::ShowMoves() {
    CPosition *p = this;
    unsigned int i;
    char szSanBuffer[16];

    heap_t heap = allocate_heap();

    push_section(heap);
    p->LegalMoves(heap);

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        CMove move = heap->data[i];
        Print(0, "%s ", p->SAN(move, szSanBuffer));
        if (p->IsCheckingMove(move))
            Print(0, "(check) ");
        if (!p->LegalMove(move)) {
            Print(0, "(rejected?!) ");
        }
        if (move.IsCapture() || move.IsEnPassant()) {
            Print(0, "(%d) ", SwapOff(p, move));
        }
    }
    Print(0, "\n");

    pop_section(heap);

    push_section(heap);
    p->GenChecks(heap);

    if (heap->current_section->end > heap->current_section->start) {
        Print(0, "Checks: ");
        for (i = heap->current_section->start; i < heap->current_section->end;
             i++) {
            CMove move = heap->data[i];
            Print(0, "%s ", p->SAN(move, szSanBuffer));
        }
        Print(0, "\n");
    }

    free_heap(heap);
}

static void TestSearchGenerator(CSearchData &sd,
                                CMove (CSearchData::*pGenerator)()) {
    bool fComma = false;
    sd.EnterNode();

    while (true) {
        CMove move = (sd.*pGenerator)();
        if (move == M_NONE) {
            break;
        }

        if (sd.m_pPosition->LegalMove(move)) {
            if (fComma) {
                Print(0, ", ");
            }
            char szSanBuffer[16];
            Print(0, "%s", sd.m_pPosition->SAN(move, szSanBuffer));
            fComma = true;
        }
    }

    sd.LeaveNode();
    Print(0, "\n");
}

static CMove NextMoveQFixedAlpha(CSearchData &sd) {
    return sd.NextMoveQ(-500000);
}

void CPosition::TestNextGenerators() {
    CSearchData sd(this);
    Print(0, "NextMove:\n");
    TestSearchGenerator(sd, &CSearchData::NextMove);
    Print(0, "\nNextEvasion:\n");
    TestSearchGenerator(sd, &CSearchData::NextEvasion);
    Print(0, "\nNextMoveQ:\n");

    bool fComma = false;
    sd.EnterNode();
    while (true) {
        CMove move = NextMoveQFixedAlpha(sd);
        if (move == M_NONE) {
            break;
        }
        if (sd.m_pPosition->LegalMove(move)) {
            if (fComma) {
                Print(0, ", ");
            }
            char szSanBuffer[16];
            Print(0, "%s", sd.m_pPosition->SAN(move, szSanBuffer));
            fComma = true;
        }
    }
    sd.LeaveNode();
    Print(0, "\n");
}

/*
 * EPD stuff
 */

CMove goodmove[MAX_EPD_MOVES];
CMove badmove[MAX_EPD_MOVES];

/**
 * Read a position from an EPD string.
 */
static void ReadEPD(CPosition *p, const char *pszEpdInput) {
    unsigned int dwLevel = 0;
    int nRk = static_cast<int>(CBitBoard::LEVEL_WIDTH[0]) - 1;
    unsigned int dwFl = 0;
    int i;
    char *rgpszOps[MAX_EPD_OPS];
    char *pszLine;
    char szSanBuffer[16];
    char *pszX;

    /* Make a copy of the input string, since it will be destroyed
     * due to the use of strtok, sorry :-)
     */

    pszLine = (char *)safe_malloc(strlen(pszEpdInput) + 1);
    strcpy(pszLine, pszEpdInput);
    pszX = pszLine;

    for (unsigned int dwSquare = 0; dwSquare < CBitBoard::SIZE; dwSquare++)
        p->SetPiece(dwSquare, Neutral);
    p->GetMask(White, 0) = p->GetMask(Black, 0) = {};

    /* scan piece placement across all levels; levels are separated by '|' */
    while (nRk >= 0) {
        switch (*pszX) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            dwFl += (*pszX) - '0';
            break;
        case '-':
            dwFl += 1;
            break;
        case 'P':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, Pawn);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'N':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, Knight);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'B':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, Bishop);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'R':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, Rook);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'Q':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, Queen);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'K':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, King);
                p->GetMask(White, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'p':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -Pawn);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'n':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -Knight);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'b':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -Bishop);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'r':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -Rook);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'q':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -Queen);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case 'k':
            if (dwFl < CBitBoard::LEVEL_WIDTH[dwLevel]) {
                const int nSq = static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(dwFl), nRk));
                p->SetPiece(nSq, -King);
                p->GetMask(Black, 0).SetBit(nSq);
            }
            dwFl++;
            break;
        case '/':
            dwFl = 0;
            nRk--;
            break;
        case '|':
            dwFl = 0;
            dwLevel++;
            if (dwLevel < CBitBoard::NUM_LEVELS) {
                nRk = static_cast<int>(CBitBoard::LEVEL_WIDTH[dwLevel]) - 1;
            } else {
                nRk = -1;
            }
            break;
        case ' ':
            nRk = -1;
        }
        pszX++;
    }

    /* scan p->GetTurn() to move */
    if (*pszX == 'w') {
        p->SetTurn(White);
    } else {
        p->SetTurn(Black);
    }

    /* skip white space */
    while (*(++pszX) == ' ')
        ;

    /* scan castling status */
    p->SetCastle(0);
    if (*pszX != '-') {
        if (*pszX == 'K') {
            p->SetCastle(p->GetCastle() | (CastleMask[White][0]));
            pszX++;
        }
        if (*pszX == 'Q') {
            p->SetCastle(p->GetCastle() | (CastleMask[White][1]));
            pszX++;
        }
        if (*pszX == 'k') {
            p->SetCastle(p->GetCastle() | (CastleMask[Black][0]));
            pszX++;
        }
        if (*pszX == 'q') {
            p->SetCastle(p->GetCastle() | (CastleMask[Black][1]));
            pszX++;
        }
    }

    /* skip white space */
    while (*(++pszX) == ' ')
        ;

    /* scan enpassant status */
    p->SetEnPassant(InvalidSquareCoord());
    if (*pszX != '-') {
        p->SetEnPassant(CSCoord(0, *pszX - 'a', *(pszX + 1) - '1'));
        pszX++;
    }

    /* skip white space */
    while (*(++pszX) == ' ')
        ;

    p->RecalcAttacks();
    p->SetPly(0);

    i = 0;
    rgpszOps[i] = strtok(pszX, ";");
    while (rgpszOps[i]) {
        i++;
        if (i >= MAX_EPD_OPS)
            break;
        rgpszOps[i] = strtok(NULL, ";");
    }

    goodmove[0] = M_NONE;
    badmove[0] = M_NONE;

    for (i = 0; rgpszOps[i] && i < (MAX_EPD_OPS - 1); i++) {
        char *pszOp = strtok(rgpszOps[i], " ");

        if (pszOp) {
            if (!strcmp(pszOp, "bm")) {
                int nCnt = 0;

                while ((pszOp = strtok(NULL, " "))) {
                    CMove mv = p->ParseSAN(pszOp);
                    if (mv != M_NONE) {
                        goodmove[nCnt] = mv;
                        Print(0, "best move is %s\n",
                              p->SAN(goodmove[nCnt], szSanBuffer));
                        nCnt++;
                        if (nCnt >= MAX_EPD_MOVES - 1)
                            break;
                    }
                }
                goodmove[nCnt] = M_NONE;
            } else if (!strcmp(pszOp, "am")) {
                int nCnt = 0;

                while ((pszOp = strtok(NULL, " "))) {
                    CMove mv = p->ParseSAN(pszOp);
                    if (mv != M_NONE) {
                        badmove[nCnt] = mv;
                        Print(0, "bad move is %s\n",
                              p->SAN(badmove[nCnt], szSanBuffer));
                        nCnt++;
                        if (nCnt >= MAX_EPD_MOVES - 1)
                            break;
                    }
                }
                badmove[nCnt] = M_NONE;
            }
        }
    }

    /* free the memory allocated
     */

    free(pszLine);
}

/**
 * Create an EPD of the current position
 */

char *CPosition::MakeEPD() {
    CPosition *p = this;
    static char szEpdbuffer[2048];
    char szWName[] = " PNBRQK";
    char szBName[] = " pnbrqk";
    char szSanBuffer[16];

    char *pszX = szEpdbuffer;

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (int i = static_cast<int>(dwWidth) - 1; i >= 0; i--) {
            uint8_t bCnt = 0;
            for (unsigned int j = 0; j < dwWidth; j++) {
                const int nSquare =
                    static_cast<int>(CSCoord(static_cast<int>(dwLevel), static_cast<int>(j), i));
                if (p->m_rgPiece[nSquare] == Neutral) {
                    bCnt++;
                    if (j == (dwWidth - 1))
                        *(pszX++) = '0' + bCnt;
                } else {
                    if (bCnt)
                        *(pszX++) = '0' + bCnt;
                    bCnt = 0;
                    if (p->m_rgPiece[nSquare] > 0)
                        *(pszX++) = szWName[TYPE(p->m_rgPiece[nSquare])];
                    else
                        *(pszX++) = szBName[TYPE(p->m_rgPiece[nSquare])];
                }
            }
            if ((dwLevel == (CBitBoard::NUM_LEVELS - 1)) && (i == 0))
                *(pszX++) = ' ';
            else if (i == 0)
                *(pszX++) = '|';
            else
                *(pszX++) = '/';
        }
    }
    if (p->m_nTurn == White)
        *(pszX++) = 'w';
    else
        *(pszX++) = 'b';
    *(pszX++) = ' ';

    if (p->m_bCastle & CastleMask[White][0])
        *(pszX++) = 'K';
    if (p->m_bCastle & CastleMask[White][1])
        *(pszX++) = 'Q';
    if (p->m_bCastle & CastleMask[Black][0])
        *(pszX++) = 'k';
    if (p->m_bCastle & CastleMask[Black][1])
        *(pszX++) = 'q';
    if (!p->m_bCastle)
        *(pszX++) = '-';
    *(pszX++) = ' ';

    if (p->m_EnPassant.IsValid()) {
        *(pszX++) = 'a' + p->m_EnPassant.m_nFile;
        *(pszX++) = '1' + p->m_EnPassant.m_nRank;
    } else
        *(pszX++) = '-';
    *(pszX++) = '\0';

    if (goodmove[0] != M_NONE) {
        int i;
        strcat(szEpdbuffer, " bm");
        for (i = 0; goodmove[i] != M_NONE; i++) {
            strcat(szEpdbuffer, " ");
            strcat(szEpdbuffer, p->SAN(goodmove[i], szSanBuffer));
        }
        strcat(szEpdbuffer, ";");
    }

    if (badmove[0] != M_NONE) {
        int i;
        strcat(szEpdbuffer, " am");
        for (i = 0; badmove[i] != M_NONE; i++) {
            strcat(szEpdbuffer, " ");
            strcat(szEpdbuffer, p->SAN(badmove[i], szSanBuffer));
        }
        strcat(szEpdbuffer, ";");
    }
    return szEpdbuffer;
}

/*
 * Check if game is technically ended.
 *
 * Returns NULL if not, otherwise a descriptive string.
 *
 */

const char *CPosition::GameEnd() {
    CPosition *p = this;
    if (p->m_pActLog->gl_IrrevCount >= 100) {
        return "1/2-1/2 {50 move rule}";
    }

    if (p->Repeated(true) >= 2) {
        return "1/2-1/2 {Draw by repetition}";
    }

    if (p->m_rgnMaterial[White] == 0 && p->m_rgnMaterial[Black] == 0) {
        return "1/2-1/2 {Insufficient material}";
    }

    if (!p->LegalMoves(NULL)) {
        if (p->InCheck(p->m_nTurn)) {
            if (p->m_nTurn == Black) {
                return "1-0 {White mates}";
            } else {
                return "0-1 {Black mates}";
            }
        } else {
            return "1/2-1/2 {Stalemate}";
        }
    }

    return NULL;
}

/**
 * Returns true if the given side only has a bishops and no other
 * major pieces.
 */
static bool has_only_bishops(const CPosition *p, Color nSide) {
    return (p->GetMask(nSide, Bishop).IsNotEmpty()) &&
           ((p->GetMask(nSide, Knight) | p->GetMask(nSide, Rook) |
             p->GetMask(nSide, Queen)).IsEmpty());
}
/*
 * Check if this is a theoretical draw
 */
bool CPosition::CheckDraw() const {
    const CPosition *p = this;
    if (p->m_rgnMaterial[Black] == 0) {
        if (p->m_rgnNonPawn[White] == 0) {
            if (!(p->m_rgMask[White][Pawn] & NotAFileMask)) {
                if (p->m_rgMask[Black][King] & CornerMaskA8)
                    return true;
            }
            if (!(p->m_rgMask[White][Pawn] & NotHFileMask)) {
                if (p->m_rgMask[Black][King] & CornerMaskH8)
                    return true;
            }
        } else if (has_only_bishops(p, White)) {
            if (!(p->m_rgMask[White][Pawn] & NotAFileMask) &&
                (p->m_rgMask[Black][King] & CornerMaskA8)) {
                if (p->m_rgMask[White][Bishop] & BlackSquaresMask)
                    return true;
            }
            if (!(p->m_rgMask[White][Pawn] & NotHFileMask) &&
                (p->m_rgMask[Black][King] & CornerMaskH8)) {
                if (p->m_rgMask[White][Bishop] & WhiteSquaresMask)
                    return true;
            }
        }
    }
    if (p->m_rgnMaterial[White] == 0) {
        if (p->m_rgnNonPawn[Black] == 0) {
            if (!(p->m_rgMask[Black][Pawn] & NotAFileMask)) {
                if (p->m_rgMask[White][King] & CornerMaskA1)
                    return true;
            }
            if (!(p->m_rgMask[Black][Pawn] & NotHFileMask)) {
                if (p->m_rgMask[White][King] & CornerMaskH1)
                    return true;
            }
        } else if (has_only_bishops(p, Black)) {
            if (!(p->m_rgMask[Black][Pawn] & NotAFileMask) &&
                (p->m_rgMask[White][King] & CornerMaskA1)) {
                if (p->m_rgMask[Black][Bishop] & WhiteSquaresMask)
                    return true;
            }
            if (!(p->m_rgMask[Black][Pawn] & NotHFileMask) &&
                (p->m_rgMask[White][King] & CornerMaskH1)) {
                if (p->m_rgMask[Black][Bishop] & BlackSquaresMask)
                    return true;
            }
        }
    }
    return false;
}

/*
 * Check if the pawn is passed
 */

bool IsPassed(const CPosition *p, const CSCoord& sqCoord, int nSide) {
    const uint16_t wSq = sqCoord.BitOffset();
    if (nSide == White)
        return !(p->GetMask(Black, Pawn) & PassedMaskW[wSq]);
    else
        return !(p->GetMask(White, Pawn) & PassedMaskB[wSq]);
}

/**
 * Validate the castling-rights flags of a freshly parsed EPD position.
 *
 * Castling rights are read verbatim from the EPD text, but a right is only
 * meaningful when the relevant king and rook actually occupy their home
 * squares.  In the 4D variant castling is confined to the main level, so for
 * every declared right verify the friendly King is on its E-file home square
 * and the friendly Rook is on the matching corner (H for king-side, A for
 * queen-side).  Returns false when any declared right is inconsistent with the
 * board.
 */
static bool EpdCastlingRightsValid(const CPosition *p) {
    const int8_t bCastle = p->GetCastle();

    if (bCastle & CastleMask[White][0]) {
        if (TYPE(p->GetPiece(CASTLE_E1)) != King ||
            !SAME_COLOR(p->GetPiece(CASTLE_E1), White)) {
            return false;
        }
        if (TYPE(p->GetPiece(CASTLE_H1)) != Rook ||
            !SAME_COLOR(p->GetPiece(CASTLE_H1), White)) {
            return false;
        }
    }
    if (bCastle & CastleMask[White][1]) {
        if (TYPE(p->GetPiece(CASTLE_E1)) != King ||
            !SAME_COLOR(p->GetPiece(CASTLE_E1), White)) {
            return false;
        }
        if (TYPE(p->GetPiece(CASTLE_A1)) != Rook ||
            !SAME_COLOR(p->GetPiece(CASTLE_A1), White)) {
            return false;
        }
    }
    if (bCastle & CastleMask[Black][0]) {
        if (TYPE(p->GetPiece(CASTLE_E8)) != King ||
            !SAME_COLOR(p->GetPiece(CASTLE_E8), Black)) {
            return false;
        }
        if (TYPE(p->GetPiece(CASTLE_H8)) != Rook ||
            !SAME_COLOR(p->GetPiece(CASTLE_H8), Black)) {
            return false;
        }
    }
    if (bCastle & CastleMask[Black][1]) {
        if (TYPE(p->GetPiece(CASTLE_E8)) != King ||
            !SAME_COLOR(p->GetPiece(CASTLE_E8), Black)) {
            return false;
        }
        if (TYPE(p->GetPiece(CASTLE_A8)) != Rook ||
            !SAME_COLOR(p->GetPiece(CASTLE_A8), Black)) {
            return false;
        }
    }
    return true;
}

/**
 * Check whether an EPD string describes a valid position.
 *
 * Currently this validates the castling rights against the actual king/rook
 * placement; an EPD that declares a castling right with no matching king or
 * rook on its home square is rejected.  Returns false for a null/empty EPD or
 * any detected validity issue.
 */
bool CPosition::IsValidEPD(const char *pszEpd) {
    if (pszEpd == nullptr || *pszEpd == '\0') {
        return false;
    }

    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    p->m_cGameLog = INITIAL_GAME_LOG_SIZE;
    p->m_pGameLog = (SGameLog *)safe_calloc(p->m_cGameLog, sizeof(SGameLog));
    p->m_pActLog = p->m_pGameLog;
    ReadEPD(p, pszEpd);

    const bool fValid = EpdCastlingRightsValid(p);

    CPosition::Free(p);
    return fValid;
}

/**
 * Create a position from an EPD
 */

CPosition *CPosition::CreateFromEPD(const char *pszEpd) {
    if (pszEpd == nullptr || *pszEpd == '\0') {
        return nullptr;
    }

    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    p->m_cGameLog = INITIAL_GAME_LOG_SIZE;
    p->m_pGameLog = (SGameLog *)safe_calloc(p->m_cGameLog, sizeof(SGameLog));
    p->m_pActLog = p->m_pGameLog;
    ReadEPD(p, pszEpd);

    /* Reject EPDs whose castling rights are inconsistent with the board. */
    if (!EpdCastlingRightsValid(p)) {
        CPosition::Free(p);
        return nullptr;
    }

    p->m_pActLog->gl_IrrevCount = 0;

    /* default for book usage is no book */
    p->m_rgwOutOfBookCnt[White] = p->m_rgwOutOfBookCnt[Black] = 3;

    return p;
}

/**
 * Create a position in the usual starting position
 */

CPosition *CPosition::Initial() {
    CPosition *p = CPosition::CreateFromEPD(
        "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|ppppppp/7/7/7/7/7/PPPPPPP|rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR|rnbqnbr/ppppppp/7/7/7/PPPPPPP/RNBQNBR|pppppp/6/6/6/6/PPPPPP| w KQkq -");

    /* we are 'in book' in the InitalPosition */
    p->m_rgwOutOfBookCnt[White] = p->m_rgwOutOfBookCnt[Black] = 0;

    return p;
}

CPosition *CPosition::Clone(const CPosition *pSrc) {
    if (pSrc == NULL) {
        AMY_ASSERT(pSrc != NULL, "CPosition::Clone: source position is null.\n");
        return NULL;
    }

    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    AMY_ASSERT(p != NULL, "CPosition::Clone: allocation failed for source %p.\n",
               (const void *)pSrc);
    memcpy(p, pSrc, sizeof(CPosition));

    p->m_cGameLog = pSrc->m_cGameLog;
    p->m_pGameLog = (SGameLog *)safe_calloc(p->m_cGameLog, sizeof(SGameLog));
    memcpy(p->m_pGameLog, pSrc->m_pGameLog, sizeof(SGameLog) * p->m_cGameLog);

    p->m_pActLog = p->m_pGameLog + (pSrc->m_pActLog - pSrc->m_pGameLog);

    return p;
}

/**
 * Release the resources connected with a Position
 */

void CPosition::Free(CPosition *p) {
    if (p) {
        free(p->m_pGameLog);
        free(p);
    }
}
