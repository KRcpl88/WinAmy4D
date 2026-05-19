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
 * dbase.c - global database manipulation routines
 */

#include <stdint.h>
#include <string.h>

#include "dbase.h"
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "magic.h"
#include "mates.h"
#include "recog.h"
#include "safe_malloc.h"
#include "scoord.h"
#include "search.h"
#include "swap.h"
#include "types.h"
#include "utils.h"

#define INITIAL_GAME_LOG_SIZE 40 /* Initial size of game history */

/* Maximum number of EPD ops we attempt to parse */
#define MAX_EPD_OPS 15

bool CPosition::InCheck(int side) const {
    return (bool)(m_rgAtkFr[m_rgKingSq[side].BitOffset()] & m_rgMask[!side][0]);
}

bool CPosition::IsPassed(const CSCoord& sqCoord, int side) const {
    int sq = sqCoord.BitOffset();
    if (side == White)
        return !(m_rgMask[Black][Pawn] & PassedMaskW[sq]);
    else
        return !(m_rgMask[White][Pawn] & PassedMaskB[sq]);
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
    {0x01, 0x02}, /* White can castle king/queenp->m_nTurn */
    {0x04, 0x08}  /* dito for black */
};

/* local prototypes
 */

static void AtkSet(CPosition *, int, int, const CSCoord&);
static void AtkClr(CPosition *, const CSCoord&);
static void GainAttack(CPosition *, const CSCoord&, const CSCoord&);
static void LooseAttack(CPosition *, const CSCoord&, const CSCoord&);
static void GainAttacks(CPosition *, const CSCoord&);
static void LooseAttacks(CPosition *, const CSCoord&);

/*
 * Routines to up/downdate the global database
 */

static void ShowMoveList(CPosition *p) {
    int ply;
    for (ply = 0; ply < p->m_wPly; ply++) {
        CMove move = p->m_pGameLog[ply].gl_Move;
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
    int kingSq = p->m_rgKingSq[White].BitOffset();
    int i, color;
    CBitBoard temp;

    for (i = 0; i < CSCoord::SIZE; i++) {
        temp = p->m_rgAtkTo[i];
        while (temp) {
            int sq = (temp).FindSetBit();
            temp.ClearLowestBit();
            if (!p->m_rgAtkFr[sq].TstBit(i)) {
                Print(0, "AtkFr or AtkTo is bad on %c%c or %c%c\n", SQUARE(i),
                      SQUARE(sq));
                ShowMoveList(p);
                p->ShowPosition();
                abort();
            }
        }
    }

    for (color = 0; color < 2; color++) {
        for (i = Pawn; i <= King; i++) {
            temp = p->m_rgMask[color][i];
            while (temp) {
                int sq = (temp).FindSetBit();
                temp.ClearLowestBit();
                int pc = (1 - 2 * color) * i;
                if (p->m_rgPiece[sq] != pc) {
                    Print(0, "Piece on %c%c is %d, expected %d!\n", SQUARE(sq),
                          p->m_rgPiece[sq], pc);
                    ShowMoveList(p);
                    p->ShowPosition();
                    abort();
                }
            }
        }
    }

    if (p->m_rgAtkTo[kingSq] != KingEPM[kingSq]) {
        Print(0, "White king is bad:\n");
        PrintBitBoard(p->m_rgAtkTo[kingSq]);
        Print(0, "should be:\n");
        PrintBitBoard(KingEPM[kingSq]);
        ShowMoveList(p);
        p->ShowPosition();
        abort();
    }
    kingSq = p->m_rgKingSq[Black].BitOffset();
    if (p->m_rgAtkTo[kingSq] != KingEPM[kingSq]) {
        Print(0, "Black king is bad:\n");
        PrintBitBoard(p->m_rgAtkTo[kingSq]);
        Print(0, "should be:\n");
        PrintBitBoard(KingEPM[kingSq]);
        ShowMoveList(p);
        p->ShowPosition();
        abort();
    }
}
#endif

/*
 * Generate attacks for a piece "type" of "color" on square "square"
 */

static void AtkSet(CPosition *p, int type, int color, const CSCoord& squareCoord) {
    int square = squareCoord.BitOffset();
    CBitBoard attacks;

    switch (type) {
    case Pawn:
        attacks = PawnEPM[color][square];
        break;
    case Knight:
        attacks = KnightEPM[square];
        break;
    case Bishop:
        attacks = bishop_attacks(square, p->m_rgMask[0][0] | p->m_rgMask[1][0]);
        break;
    case Rook:
        attacks = rook_attacks(square, p->m_rgMask[0][0] | p->m_rgMask[1][0]);
        break;
    case Queen:
        attacks = bishop_attacks(square, p->m_rgMask[0][0] | p->m_rgMask[1][0]) |
                  rook_attacks(square, p->m_rgMask[0][0] | p->m_rgMask[1][0]);
        break;
    case King:
        attacks = KingEPM[square];
        break;
    default:
        printf("AtkSet(%d, %d, %d)\n", type, color, square);
        Panic(p);
        return; // never reached
    }

    p->m_rgAtkTo[square] = attacks;
    while (attacks) {
        int i = (attacks).FindSetBit();
        attacks.ClearLowestBit();
        p->m_rgAtkFr[i].SetBit(square);
    }
}

static void AtkClr(CPosition *p, const CSCoord& squareCoord) {
    int square = squareCoord.BitOffset();
    CBitBoard tmp = p->m_rgAtkTo[square];
    p->m_rgAtkTo[square] = 0;

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        p->m_rgAtkFr[i].ClrBit(square);
    }
}

/*
 * Recalculate Attacks from "from" to "to" after the piece on "to" has
 * been removed
 */

static void GainAttack(CPosition *p, const CSCoord& fromCoord,
                       const CSCoord& toCoord) {
    int from = fromCoord.BitOffset();
    int to = toCoord.BitOffset();
    signed char *nsq = NextSQ[from];
    int sq = to;
    const CBitBoard all = p->m_rgMask[0][0] | p->m_rgMask[1][0];

    for (;;) {
        sq = nsq[sq];
        if (sq < 0)
            break;

        p->m_rgAtkTo[from].SetBit(sq);
        p->m_rgAtkFr[sq].SetBit(from);

        if (all.TstBit(sq))
            break;
    }
}

/*
 * Recalculate Attacks from "from" to "to" after a piece has been put
 * onto "to"
 */

static void LooseAttack(CPosition *p, const CSCoord& fromCoord,
                        const CSCoord& toCoord) {
    int from = fromCoord.BitOffset();
    int to = toCoord.BitOffset();
    signed char *nsq = NextSQ[from];
    int sq = to;
    const CBitBoard all = p->m_rgMask[0][0] | p->m_rgMask[1][0];

    for (;;) {
        sq = nsq[sq];
        if (sq < 0)
            break;

        p->m_rgAtkTo[from].ClrBit(sq);
        p->m_rgAtkFr[sq].ClrBit(from);

        if (all.TstBit(sq))
            break;
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * the piece on this square has been removed
 */

static void GainAttacks(CPosition *p, const CSCoord& toCoord) {
    int to = toCoord.BitOffset();
    CBitBoard tmp = p->m_rgAtkFr[to] & p->m_SlidingPieces;

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        GainAttack(p, coord, toCoord);
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * a piece has been put onto this square
 */

static void LooseAttacks(CPosition *p, const CSCoord& toCoord) {
    int to = toCoord.BitOffset();
    CBitBoard tmp = p->m_rgAtkFr[to] & p->m_SlidingPieces;

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        LooseAttack(p, coord, toCoord);
    }
}

/*
 * Determines if a piece of type tp is a sliding piece.
 */
static inline bool is_sliding(int tp) { return tp >= Bishop && tp <= Queen; }

/*
 * Make a castle move
 * I separated this routine from the normal DoMove routine since it has
 * to move two pieces
 */

static void DoCastle(CPosition *p, CMove move) {
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    const CSCoord oldRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 3 : fromCoord.m_nFile - 4,
                               fromCoord.m_nRank);
    const CSCoord newRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 1 : fromCoord.m_nFile - 1,
                               fromCoord.m_nRank);
    int oldRookOffset = oldRookCoord.BitOffset();
    int newRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    AtkClr(p, fromCoord);

    /* rook looses its attacks */
    AtkClr(p, oldRookCoord);

    /* move king on the board */
    p->m_rgPiece[toOffset] = p->m_rgPiece[fromOffset];
    p->m_rgPiece[fromOffset] = Neutral;
    p->m_rgMask[p->m_nTurn][0].ClrBit(fromOffset);
    p->m_rgMask[p->m_nTurn][King].ClrBit(fromOffset);
    p->m_rgMask[p->m_nTurn][0].SetBit(toOffset);
    p->m_rgMask[p->m_nTurn][King].SetBit(toOffset);

    /* move rook on the board */
    p->m_rgPiece[newRookOffset] = p->m_rgPiece[oldRookOffset];
    p->m_rgPiece[oldRookOffset] = Neutral;
    p->m_rgMask[p->m_nTurn][0].ClrBit(oldRookOffset);
    p->m_rgMask[p->m_nTurn][Rook].ClrBit(oldRookOffset);
    p->m_SlidingPieces.ClrBit(oldRookOffset);
    p->m_rgMask[p->m_nTurn][0].SetBit(newRookOffset);
    p->m_rgMask[p->m_nTurn][Rook].SetBit(newRookOffset);
    p->m_SlidingPieces.SetBit(newRookOffset);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */

    GainAttacks(p, fromCoord);

    /* King and rook gain their attacks
     */

    AtkSet(p, King, p->m_nTurn, toCoord);
    AtkSet(p, Rook, p->m_nTurn, newRookCoord);
    p->m_rgKingSq[p->m_nTurn] = toCoord;

    /* update hashkey */
    /* Das koennte ich vorher berechnen! Ist dann nur eine Anweisung! */

    p->m_ullHKey ^= (HashKeys[p->m_nTurn][King][fromOffset] ^ HashKeys[p->m_nTurn][King][toOffset] ^
                HashKeys[p->m_nTurn][Rook][oldRookOffset] ^
                HashKeys[p->m_nTurn][Rook][newRookOffset]);
}

/*
 * Unmake a castle move
 */

static void UndoCastle(CPosition *p, CMove move) {
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    const CSCoord oldRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 3 : fromCoord.m_nFile - 4,
                               fromCoord.m_nRank);
    const CSCoord newRookCoord(fromCoord.m_nLevel,
                               move.IsShortCastle() ? fromCoord.m_nFile + 1 : fromCoord.m_nFile - 1,
                               fromCoord.m_nRank);
    int oldRookOffset = oldRookCoord.BitOffset();
    int newRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    AtkClr(p, toCoord);

    /* rook looses its attacks */
    AtkClr(p, newRookCoord);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */
    LooseAttacks(p, fromCoord);

    /* move king on the board */
    p->m_rgPiece[fromOffset] = p->m_rgPiece[toOffset];
    p->m_rgPiece[toOffset] = Neutral;
    p->m_rgMask[p->m_nTurn][0].ClrBit(toOffset);
    p->m_rgMask[p->m_nTurn][King].ClrBit(toOffset);
    p->m_rgMask[p->m_nTurn][0].SetBit(fromOffset);
    p->m_rgMask[p->m_nTurn][King].SetBit(fromOffset);

    /* move rook on the board */
    p->m_rgPiece[oldRookOffset] = p->m_rgPiece[newRookOffset];
    p->m_rgPiece[newRookOffset] = Neutral;
    p->m_rgMask[p->m_nTurn][0].ClrBit(newRookOffset);
    p->m_rgMask[p->m_nTurn][Rook].ClrBit(newRookOffset);
    p->m_SlidingPieces.ClrBit(newRookOffset);
    p->m_rgMask[p->m_nTurn][0].SetBit(oldRookOffset);
    p->m_rgMask[p->m_nTurn][Rook].SetBit(oldRookOffset);
    p->m_SlidingPieces.SetBit(oldRookOffset);

    /* King and rook gain their attacks
     */

    AtkSet(p, King, p->m_nTurn, fromCoord);
    AtkSet(p, Rook, p->m_nTurn, oldRookCoord);
    p->m_rgKingSq[p->m_nTurn] = fromCoord;
}

/*
 * Make a move
 * updates the global database
 */

void CPosition::DoMove(CMove move) {
    CPosition *p = this;
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    int8_t tp = TYPE(p->m_rgPiece[fromOffset]);

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
        AtkClr(p, fromCoord);

        if (tp == King) {
            p->m_rgKingSq[p->m_nTurn] = toCoord;
        }

        /* remove it from the board */
        p->m_rgPiece[fromOffset] = Neutral;
        p->m_rgMask[p->m_nTurn][0].ClrBit(fromOffset);
        p->m_rgMask[p->m_nTurn][tp].ClrBit(fromOffset);
        if (is_sliding(tp))
            p->m_SlidingPieces.ClrBit(fromOffset);
        /* re-calculate attacks through from-square */
        GainAttacks(p, fromCoord);

        /* update hashkey */
        p->m_ullHKey ^= HashKeys[p->m_nTurn][tp][fromOffset];
        if (tp == Pawn)
            p->m_ullPKey ^= HashKeys[p->m_nTurn][Pawn][fromOffset];

        if (tp == King) {
            /* No more castling rights */
            p->m_bCastle &= ~(CastleMask[p->m_nTurn][0] | CastleMask[p->m_nTurn][1]);
        } else if (tp == Rook) {
            if (fromOffset == (p->m_nTurn == White ? h1 : h8))
                p->m_bCastle &= ~(CastleMask[p->m_nTurn][0]);
            if (fromOffset == (p->m_nTurn == White ? a1 : a8))
                p->m_bCastle &= ~(CastleMask[p->m_nTurn][1]);
        }
        if (move.IsCapture()) {
            int sp = TYPE(p->m_rgPiece[toOffset]);

            /* piece looses its attacks */
            AtkClr(p, toCoord);

            /* remember type of captured piece */
            p->m_pActLog->gl_Piece = p->m_rgPiece[toOffset];

            p->m_rgMask[OPP(p->m_nTurn)][0].ClrBit(toOffset);
            p->m_rgMask[OPP(p->m_nTurn)][sp].ClrBit(toOffset);
            if (is_sliding(sp))
                p->m_SlidingPieces.ClrBit(toOffset);

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] -= Value[sp];
            if (sp != Pawn)
                p->m_rgnNonPawn[OPP(p->m_nTurn)] -= Value[sp];

            /* update material signature */
            if (!(p->m_rgMask[OPP(p->m_nTurn)][sp])) {
                p->m_rgbMaterialSignature[OPP(p->m_nTurn)] &= ~SIGNATURE_BIT(sp);
            }

            /* update hashkey */
            p->m_ullHKey ^= HashKeys[OPP(p->m_nTurn)][sp][toOffset];
            if (sp == Pawn)
                p->m_ullPKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][toOffset];
            if (toOffset == (OPP(p->m_nTurn) == White ? h1 : h8)) {
                p->m_bCastle &= ~(CastleMask[OPP(p->m_nTurn)][0]);
            }
            if (toOffset == (OPP(p->m_nTurn) == White ? a1 : a8)) {
                p->m_bCastle &= ~(CastleMask[OPP(p->m_nTurn)][1]);
            }
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.m_nLevel, toCoord.m_nFile,
                p->m_nTurn == White ? toCoord.m_nRank - 1 : toCoord.m_nRank + 1);
            int capturedPawnOffset = capturedPawnCoord.BitOffset();

            /* piece looses its attacks */
            AtkClr(p, capturedPawnCoord);

            /* captured piece must be a pawn */
            p->m_pActLog->gl_Piece = ((OPP(p->m_nTurn) == White) ? Pawn : -Pawn);

            p->m_rgMask[OPP(p->m_nTurn)][0].ClrBit(capturedPawnOffset);
            p->m_rgMask[OPP(p->m_nTurn)][Pawn].ClrBit(capturedPawnOffset);

            /* re-calculate attacks through to-square */
            GainAttacks(p, capturedPawnCoord);

            /* remove captured pawn from the board */
            p->m_rgPiece[capturedPawnOffset] = Neutral;

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] -= Value[Pawn];

            /* update material signature */
            if (!(p->m_rgMask[OPP(p->m_nTurn)][Pawn])) {
                p->m_rgbMaterialSignature[OPP(p->m_nTurn)] &= ~SIGNATURE_BIT(Pawn);
            }

            /* update hashkey */
            p->m_ullHKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][capturedPawnOffset];
            p->m_ullPKey ^= HashKeys[OPP(p->m_nTurn)][Pawn][capturedPawnOffset];

            /* re-calculate attacks through to-square */
            LooseAttacks(p, toCoord);
        } else {
            /* re-calculate attacks through to-square */
            LooseAttacks(p, toCoord);
        }

        if (move.HasPromotion()) {
            /* Promote piece */
            tp = PromoType(move);

            /* Update own material */
            p->m_rgnMaterial[p->m_nTurn] += Value[tp] - Value[Pawn];
            p->m_rgnNonPawn[p->m_nTurn] += Value[tp];

            if (!(p->m_rgMask[p->m_nTurn][Pawn])) {
                p->m_rgbMaterialSignature[p->m_nTurn] &= ~SIGNATURE_BIT(Pawn);
            }
            p->m_rgbMaterialSignature[p->m_nTurn] |= SIGNATURE_BIT(tp);
        }

        /* put it on the board again */
        p->m_rgPiece[toOffset] = (p->m_nTurn == White) ? tp : -tp;
        p->m_rgMask[p->m_nTurn][0].SetBit(toOffset);
        p->m_rgMask[p->m_nTurn][tp].SetBit(toOffset);
        if (is_sliding(tp))
            p->m_SlidingPieces.SetBit(toOffset);

        /* piece gains its attacks */
        AtkSet(p, tp, p->m_nTurn, toCoord);

        /* update hashkey */
        p->m_ullHKey ^= HashKeys[p->m_nTurn][tp][toOffset];
        if (tp == Pawn)
            p->m_ullPKey ^= HashKeys[p->m_nTurn][Pawn][toOffset];
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
        int passantOffset = passantCoord.BitOffset();
        if (p->m_rgAtkFr[passantOffset] & p->m_rgMask[OPP(p->m_nTurn)][Pawn]) {
            p->m_EnPassant = passantCoord;
        }
    }

    if ((p->m_EnPassant.IsValid() != p->m_pActLog->gl_EnPassant.IsValid()) ||
        (p->m_EnPassant.IsValid() &&
         p->m_EnPassant.BitOffset() != p->m_pActLog->gl_EnPassant.BitOffset())) {
        p->m_ullHKey ^=
            HashKeysEP[p->m_pActLog->gl_EnPassant.IsValid()
                           ? p->m_pActLog->gl_EnPassant.BitOffset()
                           : 0];
        p->m_ullHKey ^= HashKeysEP[p->m_EnPassant.IsValid() ? p->m_EnPassant.BitOffset()
                                                     : 0];
    }

    /* Update SGameLog */
    p->m_pActLog->gl_Move = move;
    p->m_wPly++;

    /* Grow gameLog if needed. */
    if (p->m_wPly >= p->m_cGameLog) {
        p->m_cGameLog *= 2;
        p->m_pGameLog =
            (SGameLog *)(SGameLog *)realloc(p->m_pGameLog, sizeof(SGameLog) * p->m_cGameLog);
        p->m_pActLog = p->m_pGameLog + p->m_wPly;
    } else {
        p->m_pActLog++;
    }

    /* Check if reversible move */
    if (move.IsCapture() || move.HasPromotion() || move.IsCastle() || tp == Pawn) {
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
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    int8_t tp = TYPE(p->m_rgPiece[toOffset]);

    /* Swap p->turns */
    p->m_nTurn = OPP(p->m_nTurn);

    /* Decrement ActLog */
    p->m_pActLog--;
    p->m_wPly--;

    if (move.IsCastle()) {
        UndoCastle(p, move);
    } else {
        /* piece looses its attacks */
        AtkClr(p, toCoord);

        if (tp == King) {
            p->m_rgKingSq[p->m_nTurn] = fromCoord;
        }

        /* update masks */
        p->m_rgMask[p->m_nTurn][0].ClrBit(toOffset);
        p->m_rgMask[p->m_nTurn][tp].ClrBit(toOffset);
        if (is_sliding(tp))
            p->m_SlidingPieces.ClrBit(toOffset);

        if (move.HasPromotion()) {
            /* Update own material */
            p->m_rgnMaterial[p->m_nTurn] -= Value[tp] - Value[Pawn];
            p->m_rgnNonPawn[p->m_nTurn] -= Value[tp];

            /* update material signature */
            if (!(p->m_rgMask[p->m_nTurn][tp])) {
                p->m_rgbMaterialSignature[p->m_nTurn] &= ~SIGNATURE_BIT(tp);
            }

            /* Unpromote piece */
            tp = Pawn;

            /* update material signature */
            p->m_rgbMaterialSignature[p->m_nTurn] |= SIGNATURE_BIT(Pawn);
        }

        if (move.IsCapture()) {
            int8_t sp = p->m_pActLog->gl_Piece;

            /* piece gains its attacks */
            AtkSet(p, TYPE(sp), OPP(p->m_nTurn), toCoord);

            p->m_rgPiece[toOffset] = sp;
            sp = TYPE(sp);
            p->m_rgMask[OPP(p->m_nTurn)][0].SetBit(toOffset);
            p->m_rgMask[OPP(p->m_nTurn)][sp].SetBit(toOffset);
            if (is_sliding(sp))
                p->m_SlidingPieces.SetBit(toOffset);

            /* Update oppponents material and PawnCount */
            p->m_rgnMaterial[OPP(p->m_nTurn)] += Value[sp];
            if (sp != Pawn)
                p->m_rgnNonPawn[OPP(p->m_nTurn)] += Value[sp];

            /* update material signature */
            p->m_rgbMaterialSignature[OPP(p->m_nTurn)] |= SIGNATURE_BIT(sp);
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.m_nLevel, toCoord.m_nFile,
                p->m_nTurn == White ? toCoord.m_nRank - 1 : toCoord.m_nRank + 1);
            int capturedPawnOffset = capturedPawnCoord.BitOffset();

            /* piece looses its attacks */
            AtkSet(p, Pawn, OPP(p->m_nTurn), capturedPawnCoord);

            p->m_rgMask[OPP(p->m_nTurn)][0].SetBit(capturedPawnOffset);
            p->m_rgMask[OPP(p->m_nTurn)][Pawn].SetBit(capturedPawnOffset);

            /* re-calculate attacks through to-square */
            LooseAttacks(p, capturedPawnCoord);

            /* remove captured pawn from the board */
            p->m_rgPiece[capturedPawnOffset] = (OPP(p->m_nTurn) == White) ? Pawn : -Pawn;
            p->m_rgPiece[toOffset] = Neutral;

            /* re-calculate attacks through to-square */
            GainAttacks(p, toCoord);

            /* Update oppponents material */
            p->m_rgnMaterial[OPP(p->m_nTurn)] += Value[Pawn];

            /* update material signature */
            p->m_rgbMaterialSignature[OPP(p->m_nTurn)] |= SIGNATURE_BIT(Pawn);
        } else {
            p->m_rgPiece[toOffset] = Neutral;

            /* re-calculate attacks through to-square */
            GainAttacks(p, toCoord);
        }

        /* re-calculate attacks through from-square */
        LooseAttacks(p, fromCoord);

        /* put it on the board again */
        p->m_rgPiece[fromOffset] = (p->m_nTurn == White) ? tp : -tp;
        p->m_rgMask[p->m_nTurn][0].SetBit(fromOffset);
        p->m_rgMask[p->m_nTurn][tp].SetBit(fromOffset);
        if (is_sliding(tp))
            p->m_SlidingPieces.SetBit(fromOffset);

        /* piece gains its attacks */
        AtkSet(p, tp, p->m_nTurn, fromCoord);
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
 * Make a null move, i.e. swap the p->m_nTurn on the move
 */

void CPosition::DoNull() {
    CPosition *p = this;
    /* Update SGameLog */
    p->m_pActLog->gl_Move = M_NULL;
    p->m_pActLog->gl_EnPassant = p->m_EnPassant;
    p->m_pActLog->gl_Castle = p->m_bCastle;
    p->m_pActLog->gl_HashKey = p->m_ullHKey;
    p->m_EnPassant = InvalidSquareCoord();

    if ((p->m_EnPassant.IsValid() != p->m_pActLog->gl_EnPassant.IsValid()) ||
        (p->m_EnPassant.IsValid() &&
         p->m_EnPassant.BitOffset() != p->m_pActLog->gl_EnPassant.BitOffset())) {
        p->m_ullHKey ^=
            HashKeysEP[p->m_pActLog->gl_EnPassant.IsValid()
                           ? p->m_pActLog->gl_EnPassant.BitOffset()
                           : 0];
        p->m_ullHKey ^= HashKeysEP[p->m_EnPassant.IsValid() ? p->m_EnPassant.BitOffset()
                                                     : 0];
    }

    p->m_wPly++;

    /* Grow gameLog if needed. */
    if (p->m_wPly >= p->m_cGameLog) {
        p->m_cGameLog *= 2;
        p->m_pGameLog =
            (SGameLog *)(SGameLog *)realloc(p->m_pGameLog, sizeof(SGameLog) * p->m_cGameLog);
        p->m_pActLog = p->m_pGameLog + p->m_wPly;
    } else {
        p->m_pActLog++;
    }

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
    p->m_nTurn = OPP(p->m_nTurn);

    /* Decrement ActLog */
    p->m_pActLog--;
    p->m_wPly--;

    p->m_EnPassant = p->m_pActLog->gl_EnPassant;
    p->m_ullHKey = p->m_pActLog->gl_HashKey;
}

/*
 * Given the Masks and the p->m_rgPiece[] array, recalculate all necessary data
 */

void CPosition::RecalcAttacks() {
    CPosition *p = this;
    int i;
    CBitBoard tmp;

    for (i = 0; i < CSCoord::SIZE; i++) {
        p->m_rgAtkTo[i] = p->m_rgAtkFr[i] = 0;
    }

    for (i = Pawn; i <= King; i++) {
        p->m_rgMask[White][i] = p->m_rgMask[Black][i] = 0;
    }

    p->m_SlidingPieces = 0;

    p->m_rgnMaterial[White] = p->m_rgnMaterial[Black] = 0;
    p->m_rgnNonPawn[White] = p->m_rgnNonPawn[Black] = 0;
    p->m_rgbMaterialSignature[White] = p->m_rgbMaterialSignature[Black] = 0;
    p->m_ullHKey = p->m_ullPKey = 0;

    tmp = p->m_rgMask[White][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int pc = p->m_rgPiece[i];
        tmp.ClearLowestBit();
        p->m_rgMask[White][pc].SetBit(i);
        if (is_sliding(pc))
            p->m_SlidingPieces.SetBit(i);
        p->m_rgnMaterial[White] += Value[pc];
        p->m_ullHKey ^= HashKeys[White][pc][i];
        if (pc != Pawn)
            p->m_rgnNonPawn[White] += Value[pc];
        else {
            p->m_ullPKey ^= HashKeys[White][Pawn][i];
        }

        if (pc != King) {
            p->m_rgbMaterialSignature[White] |= SIGNATURE_BIT(pc);
        }
    }

    tmp = p->m_rgMask[Black][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int pc = -p->m_rgPiece[i];
        tmp.ClearLowestBit();
        p->m_rgMask[Black][pc].SetBit(i);
        if (is_sliding(pc))
            p->m_SlidingPieces.SetBit(i);
        p->m_rgnMaterial[Black] += Value[pc];
        p->m_ullHKey ^= HashKeys[Black][pc][i];
        if (pc != Pawn)
            p->m_rgnNonPawn[Black] += Value[pc];
        else {
            p->m_ullPKey ^= HashKeys[Black][Pawn][i];
        }

        if (pc != King) {
            p->m_rgbMaterialSignature[Black] |= SIGNATURE_BIT(pc);
        }
    }

    tmp = p->m_rgMask[White][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        AtkSet(p, p->m_rgPiece[coord.BitOffset()], White, coord);
    }

    tmp = p->m_rgMask[Black][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        AtkSet(p, -p->m_rgPiece[coord.BitOffset()], Black, coord);
    }

    p->m_rgKingSq[White] = (p->m_rgMask[White][King]).FindSetBitCoord();
    p->m_rgKingSq[Black] = (p->m_rgMask[Black][King]).FindSetBitCoord();

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
    int square = squareCoord.BitOffset();
    CBitBoard tmp = p->m_rgAtkFr[square] & p->m_rgMask[p->m_nTurn][0];

    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        tmp.ClearLowestBit();
        if (TYPE(p->m_rgPiece[coord.BitOffset()]) == Pawn &&
            is_promo_square(squareCoord)) {
            append_to_heap(heap, make_promotion(coord, squareCoord, Queen, M_CAPTURE));
            append_to_heap(heap, make_promotion(coord, squareCoord, Knight, M_CAPTURE));
            append_to_heap(heap, make_promotion(coord, squareCoord, Rook, M_CAPTURE));
            append_to_heap(heap, make_promotion(coord, squareCoord, Bishop, M_CAPTURE));
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
    int square = squareCoord.BitOffset();
    if (TYPE(p->m_rgPiece[square]) != Pawn) {
        CBitBoard tmp;

        tmp = p->m_rgAtkTo[square] & ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

        while (tmp) {
            CSCoord coord = (tmp).FindSetBitCoord();
            tmp.ClearLowestBit();
            append_to_heap(heap, make_move(squareCoord, coord, 0));
        }

        /* Generate castling moves
         * we will check legality later...
         */

        if (TYPE(p->m_rgPiece[square]) == King) {
            if (p->m_bCastle & CastleMask[p->m_nTurn][0]) {
                /* OK, we might castle king p->m_nTurn */
                append_to_heap(heap, make_move(p->m_nTurn == White ? e1 : e8,
                                               p->m_nTurn == White ? g1 : g8,
                                               M_SCASTLE));
            }
            if (p->m_bCastle & CastleMask[p->m_nTurn][1]) {
                append_to_heap(heap, make_move(p->m_nTurn == White ? e1 : e8,
                                               p->m_nTurn == White ? c1 : c8,
                                               M_LCASTLE));
            }
        }
    } else {
        const int width = CSCoord::LEVEL_WIDTH[squareCoord.m_nLevel];
        const int direction = (p->m_nTurn == White) ? 1 : -1;
        CSCoord sqCoord(squareCoord.m_nLevel, squareCoord.m_nFile,
                        squareCoord.m_nRank + direction);
        int sq = sqCoord.BitOffset();

        if (p->m_rgPiece[sq] == Neutral) {
            if (is_promo_square(sqCoord)) {
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Queen, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Knight, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Rook, 0));
                append_to_heap(heap, make_promotion(squareCoord, sqCoord, Bishop, 0));
            } else {
                append_to_heap(heap, make_move(squareCoord, sqCoord, 0));

                const int homeRank = (p->m_nTurn == White) ? 1 : (width - 2);
                if (squareCoord.m_nRank == homeRank) {
                    CSCoord dblCoord(squareCoord.m_nLevel, squareCoord.m_nFile,
                                     squareCoord.m_nRank + 2 * direction);
                    sq = dblCoord.BitOffset();
                    if (p->m_rgPiece[sq] == Neutral) {
                        append_to_heap(heap, make_move(squareCoord, dblCoord, M_PAWND));
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
    const CSCoord kingHome((p->m_nTurn == White) ? e1 : e8);
    /* Sometimes there might be a legal castling move, but for the
       wrong p->m_nTurn, probably from the Countermove table */
    if (fromCoord.m_nLevel != kingHome.m_nLevel || fromCoord.m_nFile != kingHome.m_nFile ||
        fromCoord.m_nRank != kingHome.m_nRank)
        return false;

    if (p->InCheck(p->m_nTurn))
        return false;

    /* king p->m_nTurn castling */
    if (move.IsShortCastle() && (p->m_bCastle & CastleMask[p->m_nTurn][0])) {
        int fs = (p->m_nTurn == White ? f1 : f8);
        int gs = (p->m_nTurn == White ? g1 : g8);

        /* Check if f and g square are empty */
        if (p->m_rgPiece[fs] == Neutral && p->m_rgPiece[gs] == Neutral) {
            /* Check if f and g square are not attacked by opponent */
            if ((p->m_rgAtkFr[fs] | p->m_rgAtkFr[gs]) & p->m_rgMask[OPP(p->m_nTurn)][0])
                return false;
            else
                return true;
        }
    }

    /* queen p->m_nTurn castling */
    if (move.IsLongCastle() && (p->m_bCastle & CastleMask[p->m_nTurn][1])) {
        int bs = (p->m_nTurn == White ? b1 : b8);
        int cs = (p->m_nTurn == White ? c1 : c8);
        int ds = (p->m_nTurn == White ? d1 : d8);

        /* Check if b, c and d square are empty */
        if (p->m_rgPiece[bs] == Neutral && p->m_rgPiece[cs] == Neutral &&
            p->m_rgPiece[ds] == Neutral) {
            /* Check if c and d square are not attacked by opponent */
            if ((p->m_rgAtkFr[cs] | p->m_rgAtkFr[ds]) & p->m_rgMask[OPP(p->m_nTurn)][0])
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
    const CSCoord& toCoord = move.GetToCoord();
    int fr = frCoord.BitOffset();
    int to = toCoord.BitOffset();

    if (move == M_NONE || move == M_NULL)
        return false;

    /* There must be a piece on the square */
    if (!SAME_COLOR(p->m_rgPiece[fr], p->m_nTurn))
        return false;

    /* if a promotion, moving piece must be a pawn */
    if (move.HasPromotion() && TYPE(p->m_rgPiece[fr]) != Pawn)
        return false;

    /* if the move is a pawn move to the 1st/8th rank, it must be
     * be a promotion.
     */
    if (TYPE(p->m_rgPiece[fr]) == Pawn && !move.HasPromotion()) {
        const int levelWidth = CSCoord::LEVEL_WIDTH[toCoord.m_nLevel];
        if (toCoord.m_nRank == 0 || toCoord.m_nRank == (levelWidth - 1))
            return false;
    }

    if (move.IsCapture()) {
        /* There must be an enemy piece on the target square, and we
         * must attack that square
         */

        if (!SAME_COLOR(p->m_rgPiece[to], OPP(p->m_nTurn)) ||
            !p->m_rgAtkTo[fr].TstBit(to)) {
            return false;
        }
        return true;
    } else if (move.IsEnPassant()) {
        /* The moving piece must be a pawn, and the target square must be
         * the enpassant square
         */

        if (!p->m_EnPassant.IsValid())
            return false;
        if (TYPE(p->m_rgPiece[fr]) != Pawn || to != p->m_EnPassant.BitOffset())
            return false;
        if (!p->m_rgAtkTo[fr].TstBit(to))
            return false;

        return true;
    } else if (move.IsCastle()) {
        /* Call the castling test routine */
        return p->MayCastle(move);
    } else {
        /* target sqaure must be empty */
        if (p->m_rgPiece[to] != Neutral)
            return false;

        if (TYPE(p->m_rgPiece[fr]) != Pawn) {
            /* if no pawn, we must attack to square */
            if (!p->m_rgAtkTo[fr].TstBit(to))
                return false;
            if (move.IsPawnDoublePush())
                return false;
            return true;
        } else {
            /* use NextPos array to check if legal move */
            const int levelWidth = CSCoord::LEVEL_WIDTH[frCoord.m_nLevel];
            const int rankStep = (p->m_nTurn == White ? 1 : -1);
            int ttRank = frCoord.m_nRank + rankStep;
            if (ttRank < 0 || ttRank >= levelWidth)
                return false;
            int tt = CSCoord(frCoord.m_nLevel, frCoord.m_nFile, ttRank).BitOffset();
            if (move.IsPawnDoublePush()) {
                if (p->m_rgPiece[tt] != Neutral)
                    return false;
                ttRank += rankStep;
                if (ttRank < 0 || ttRank >= levelWidth)
                    return false;
                tt = CSCoord(frCoord.m_nLevel, frCoord.m_nFile, ttRank).BitOffset();
            }
            if (tt != to)
                return false;

            if (p->m_nTurn == White && toCoord.m_nRank == (levelWidth - 1) &&
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
 * Test wether a move will give check
 */

bool CPosition::IsCheckingMove(CMove move) {
    CPosition *p = this;
    const CSCoord& frCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fr = frCoord.BitOffset();
    int to = toCoord.BitOffset();
    int tp = TYPE(p->m_rgPiece[fr]);
    int kp = p->m_rgMask[OPP(p->m_nTurn)][King].FindSetBit();
    CBitBoard tmp;

    /* Is it a direct check ? */

    if (move.HasPromotion())
        tp = PromoType(move);

    switch (tp) {
    case Knight:
        if (KnightEPM[kp].TstBit(to))
            return true;
        break;
    case Bishop:
        if (BishopEPM[kp].TstBit(to)) {
            if (!((p->m_rgMask[White][0] | p->m_rgMask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Rook:
        if (RookEPM[kp].TstBit(to)) {
            if (!((p->m_rgMask[White][0] | p->m_rgMask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Queen:
        if (QueenEPM[kp].TstBit(to)) {
            if (!((p->m_rgMask[White][0] | p->m_rgMask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Pawn:
        {
            const CSCoord kingCoord(kp);
            int fileDelta = kingCoord.m_nFile - toCoord.m_nFile;
            int rankDelta = kingCoord.m_nRank - toCoord.m_nRank;
            if (kingCoord.m_nLevel == toCoord.m_nLevel) {
                if (p->m_nTurn == White && rankDelta == 1 &&
                    (fileDelta == -1 || fileDelta == 1))
                    return true;
                if (p->m_nTurn == Black && rankDelta == -1 &&
                    (fileDelta == -1 || fileDelta == 1))
                    return true;
            }
        }
        break;
    }

    /*
     * No direct check...
     * Let's see if it might be a discovered check...
     */

    tmp = (p->m_rgMask[p->m_nTurn][Bishop] | p->m_rgMask[p->m_nTurn][Rook] |
           p->m_rgMask[p->m_nTurn][Queen]) &
          QueenEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        CBitBoard tmp2;

        tmp.ClearLowestBit();
        if (TYPE(p->m_rgPiece[i]) == Bishop && !BishopEPM[kp].TstBit(i))
            continue;
        if (TYPE(p->m_rgPiece[i]) == Rook && !RookEPM[kp].TstBit(i))
            continue;

        tmp2 = (p->m_rgMask[White][0] | p->m_rgMask[Black][0]) & InterPath[kp][i];
        if ((tmp2).CountBits() == 1 && (tmp2).FindSetBit() == fr)
            return true;
    }

    return false;
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
    int kp = p->m_rgKingSq[OPP(p->m_nTurn)].BitOffset();
    CBitBoard *ip = InterPath[kp];
    CBitBoard fsq = p->m_rgMask[p->m_nTurn][0];
    CBitBoard all = (p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

    /* First find all blockers, i.e. pieces that give check when they move
     * from their current square
     */

    tmp = (p->m_rgMask[p->m_nTurn][Bishop] | p->m_rgMask[p->m_nTurn][Queen]) & BishopEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (ip[i] && !(ip[i] & p->m_rgMask[OPP(p->m_nTurn)][0])) {
            CBitBoard tmp2 = p->m_rgMask[p->m_nTurn][0] & ip[i];

            if ((tmp2).CountBits() == 1) {
                CSCoord coord = (tmp2).FindSetBitCoord();

                if (fsq.TstBit(coord.BitOffset())) {
                    p->GenFrom(coord, heap);
                    fsq.ClrBit(coord.BitOffset());
                }
            }
        }
    }

    tmp = (p->m_rgMask[p->m_nTurn][Rook] | p->m_rgMask[p->m_nTurn][Queen]) & RookEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (ip[i] && !(ip[i] & p->m_rgMask[OPP(p->m_nTurn)][0])) {
            CBitBoard tmp2 = p->m_rgMask[p->m_nTurn][0] & ip[i];

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
    tmp = BishopEPM[kp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Bishop] | p->m_rgMask[p->m_nTurn][Queen];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2 = p->m_rgAtkTo[sq] & tmp;
        fr.ClearLowestBit();

        while (tmp2) {
            int sq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            if (InterPath[kp][sq2] & all)
                continue;
            append_to_heap(heap, make_move(sq, sq2, 0));
        }
    }

    /* Find direct checks by Rook or Queen */
    tmp = RookEPM[kp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Rook] | p->m_rgMask[p->m_nTurn][Queen];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2 = p->m_rgAtkTo[sq] & tmp;
        fr.ClearLowestBit();

        while (tmp2) {
            int sq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            if (InterPath[kp][sq2] & all)
                continue;
            append_to_heap(heap, make_move(sq, sq2, 0));
        }
    }

    /* Find direct checks by Knight */
    tmp = KnightEPM[kp];
    tmp &= ~all;

    fr = p->m_rgMask[p->m_nTurn][Knight];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2;

        fr.ClearLowestBit();
        tmp2 = p->m_rgAtkTo[sq] & tmp;

        while (tmp2) {
            int sq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            append_to_heap(heap, make_move(sq, sq2, 0));
        }
    }

    /*
     * last find pawn checks
     */

    tmp = (p->m_nTurn == White) ? BPawnEPM[kp] : WPawnEPM[kp];
    tmp &= ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

    while (tmp) {
        int sq = (tmp).FindSetBit();
        tmp.ClearLowestBit();

        if (p->m_nTurn == White) {
            if (p->m_rgPiece[sq - 8] == Pawn) {
                append_to_heap(heap, make_move(sq - 8, sq, 0));
            }
        } else {
            if (p->m_rgPiece[sq + 8] == -Pawn) {
                append_to_heap(heap, make_move(sq + 8, sq, 0));
            }
        }
    }
}

/*
 * Repetition check
 * if mode = true, count the number of repetitions of current position
 * if mode = false, only check if current position is repeated
 */

int CPosition::Repeated(int mode) {
    CPosition *p = this;
    int i, cnt = 0;
    struct SGameLog *gl;

    if (p->m_wPly == 0)
        return 0;

    if (p->m_pActLog->gl_IrrevCount >= 100)
        return 3;

    gl = p->m_pActLog - 1;
    for (i = p->m_pActLog->gl_IrrevCount; i > 0; i--, gl--) {
        if (gl->gl_HashKey == p->m_ullHKey) {
            if (mode)
                cnt++;
            else
                return true;
        }
    }

    return cnt;
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
char *CPosition::SAN(CMove move, char *buffer) {
    CPosition *p = this;
    char *x = buffer;

    const CSCoord& toCoord = move.GetToCoord();
    const CSCoord& frCoord = move.GetFromCoord();
    int to = toCoord.BitOffset();
    int fr = frCoord.BitOffset();
    int8_t tp = TYPE(p->m_rgPiece[fr]);

    if (tp == Pawn) {
        if (move.IsCapture() || move.IsEnPassant()) {
            *(x++) = 'a' + frCoord.m_nFile;
            *(x++) = 'x';
        }
        *(x++) = 'a' + toCoord.m_nFile;
        *(x++) = '1' + toCoord.m_nRank;

        if (move.HasPromotion()) {
            *(x++) = '=';
            *(x++) = PieceName[PromoType(move)];
        }
    } else if (move.IsCastle()) {
        *(x++) = 'O';
        *(x++) = '-';
        *(x++) = 'O';
        if (move.IsLongCastle()) {
            *(x++) = '-';
            *(x++) = 'O';
        }
    } else {
        CBitBoard tmp;
        bool aamb = false, /* set for ambigous moves */
            ramb = false,  /* set means ambigous rank */
            famb = false;  /* set means ambigous file */

        tmp = p->m_rgAtkFr[to] & p->m_rgMask[p->m_nTurn][tp];

        /* check for ambigous move */
        while (tmp) {
            CSCoord coord = (tmp).FindSetBitCoord();
            tmp.ClearLowestBit();
            if (coord.BitOffset() != fr) {
                int incheck;
                CMove tmove(coord, move.GetToCoord(), 0);

                /* seems there is another piece of the same type which
                 * can move to the same destination square.
                 * Let's see wether it is pinned...
                 */

                if (p->m_rgPiece[to])
                    tmove.SetCapture();

                p->DoMove(tmove);
                incheck = p->InCheck(OPP(p->m_nTurn));
                p->UndoMove(tmove);

                if (incheck)
                    continue;

                aamb = true;
                if (coord.m_nFile == frCoord.m_nFile)
                    famb = true;
                if (coord.m_nRank == frCoord.m_nRank)
                    ramb = true;
            }
        }

        *(x++) = PieceName[tp];
        if (aamb) {
            if (!famb)
                *(x++) = 'a' + frCoord.m_nFile;
            else {
                if (!ramb)
                    *(x++) = '1' + frCoord.m_nRank;
                else {
                    *(x++) = 'a' + frCoord.m_nFile;
                    *(x++) = '1' + frCoord.m_nRank;
                }
            }
        }

        if (move.IsCapture() || move.IsEnPassant())
            *(x++) = 'x';

        *(x++) = 'a' + toCoord.m_nFile;
        *(x++) = '1' + toCoord.m_nRank;
    }

    p->DoMove(move);
    if (p->InCheck(p->m_nTurn)) {
        if (!p->LegalMoves(NULL))
            *(x++) = '#';
        else
            *(x++) = '+';
    }
    p->UndoMove(move);

    *x = '\0';
    return buffer;
}

/*
 * Generate the ICS SAN for a move
 */

char *ICS_SAN(CMove move) {
    static char buffer[16];
    char *x = buffer;

    const CSCoord toCoord = move.GetToCoord();
    const CSCoord frCoord = move.GetFromCoord();

    *(x++) = 'a' + frCoord.m_nFile;
    *(x++) = '1' + frCoord.m_nRank;
    if (move.IsCapture() || move.IsEnPassant()) {
        *(x++) = 'x';
    }
    *(x++) = 'a' + toCoord.m_nFile;
    *(x++) = '1' + toCoord.m_nRank;
    if (move.HasPromotion()) {
        *(x++) = PieceName[PromoType(move)];
    }
    *x = '\0';
    return buffer;
}

/*
 * Parse a move string in e2e4 notation
 */

CMove parse_gsan_internal(CPosition *p, char *san, heap_t heap) {
    if (!strncmp(san, "O-O-O", 5) || !strncmp(san, "o-o-o", 5) ||
        !strncmp(san, "0-0-0", 5)) {
        CMove move(CSCoord(p->m_nTurn == White ? e1 : e8),
                   CSCoord(p->m_nTurn == White ? c1 : c8), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        CMove move(CSCoord(p->m_nTurn == White ? e1 : e8),
                   CSCoord(p->m_nTurn == White ? g1 : g8), M_SCASTLE);
        if (p->MayCastle(move))
            return move;
    }

    if (strlen(san) < 4) {
        return M_NONE;
    }

    (void)p->LegalMoves(heap);

    int fr = *san - 'a' + 8 * (*(san + 1) - '1');
    int to = *(san + 2) - 'a' + 8 * (*(san + 3) - '1');

    int mask = fr + (to << 6);

    for (unsigned int i = heap->current_section->start;
         i < heap->current_section->end; i++) {
        CMove move = heap->data[i];
        if (move.GetFromToIndex() == mask) {
            if (move.HasPromotion() && strlen(san) >= 5) {
                char p = *(san + 4);
                move.ClearPromotion();

                if (p == 'q' || p == 'Q') {
                    move.SetPromotionType(Queen);
                } else if (p == 'r' || p == 'R') {
                    move.SetPromotionType(Rook);
                } else if (p == 'n' || p == 'N') {
                    move.SetPromotionType(Knight);
                } else if (p == 'b' || p == 'B') {
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

CMove CPosition::ParseGSAN(char *san) {
    CPosition *p = this;
    heap_t heap = allocate_heap();
    CMove move = parse_gsan_internal(p, san, heap);
    free_heap(heap);

    return move;
}

/*
 * Parse a move string in e2e4 notation against a supplied move list
 */

CMove ParseGSANList(char *san, Color side, CMove *mvs, int cnt) {
    int fr, to;
    int mask;
    int i;

    if (!strncmp(san, "O-O-O", 5) || !strncmp(san, "o-o-o", 5) ||
        !strncmp(san, "0-0-0", 5)) {
        CMove move(CSCoord(side == White ? e1 : e8),
                   CSCoord(side == White ? c1 : c8), M_LCASTLE);

        for (i = 0; i < cnt; i++)
            if (move == mvs[i])
                return move;
        return M_NONE;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        CMove move(CSCoord(side == White ? e1 : e8),
                   CSCoord(side == White ? g1 : g8), M_SCASTLE);

        for (i = 0; i < cnt; i++)
            if (move == mvs[i])
                return move;
        return M_NONE;
    }

    fr = *san - 'a' + 8 * (*(san + 1) - '1');
    to = *(san + 2) - 'a' + 8 * (*(san + 3) - '1');

    mask = fr + (to << 6);

    for (i = 0; i < cnt; i++) {
        if (mvs[i].GetFromToIndex() == mask) {
            if (mvs[i].HasPromotion()) {
                char p = *(san + 4);
                CMove move = mvs[i];
                move.ClearPromotion();

                if (p == 'q' || p == 'Q') {
                    move.SetPromotionType(Queen);
                } else if (p == 'r' || p == 'R') {
                    move.SetPromotionType(Rook);
                } else if (p == 'n' || p == 'N') {
                    move.SetPromotionType(Knight);
                } else if (p == 'b' || p == 'B') {
                    move.SetPromotionType(Bishop);
                } else {
                    return M_NONE;
                }
                return move;
            } else
                return mvs[i];
        }
    }
    return M_NONE;
}

/*
 * Test a pseudolegal move for legality
 */

static bool TryMove(CPosition *p, CMove move) {
    bool tmp;
    p->DoMove(move);
    tmp = p->InCheck(OPP(p->m_nTurn));
    p->UndoMove(move);

    return !tmp;
}

/*
 * Parse a move string (in SAN)
 */
static CMove parse_san_with_heap(CPosition *p, const char *san, heap_t heap) {
    int tp = Neutral;
    int frk = -1, ffl = -1, trk = -1, tfl = -1;
    int pro = 0;
    unsigned int i;
    CMove move;

    /* Check castling first */

    if (!strncmp(san, "O-O-O", 5) || !strncmp(san, "o-o-o", 5) ||
        !strncmp(san, "0-0-0", 5)) {
        move = CMove(CSCoord(p->m_nTurn == White ? e1 : e8),
                     CSCoord(p->m_nTurn == White ? c1 : c8), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
        else
            return M_NONE;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        move = CMove(CSCoord(p->m_nTurn == White ? e1 : e8),
                     CSCoord(p->m_nTurn == White ? g1 : g8), M_SCASTLE);
        if (p->MayCastle(move))
            return move;
        else
            return M_NONE;
    }

    p->PLegalMoves(heap);

    /* special handling of pawn captures a la 'cd' */
    if (strlen(san) == 2 && *san >= 'a' && *san <= 'h' && *(san + 1) >= 'a' &&
        *(san + 1) <= 'h') {
        ffl = *san - 'a';
        tfl = *(san + 1) - 'a';

        for (i = heap->current_section->start; i < heap->current_section->end;
             i++) {
            move = heap->data[i];
            const CSCoord& frCoord = move.GetFromCoord();
            const CSCoord& toCoord = move.GetToCoord();
            int fr = frCoord.BitOffset();

            if (TYPE(p->m_rgPiece[fr]) == Pawn &&
                (move.IsCapture() || move.IsEnPassant()) && frCoord.m_nFile == ffl &&
                toCoord.m_nFile == tfl && TryMove(p, move))
                return move;
        }
        return M_NONE;
    }

    /* Next examine the string */
    for (; *san; san++) {
        switch (*san) {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h': {
            ffl = tfl;
            tfl = *san - 'a';
            break;
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8': {
            frk = trk;
            trk = *san - '1';
            break;
        }
        case 'N':
            tp = Knight;
            break;
        case 'B':
            tp = Bishop;
            break;
        case 'R':
            tp = Rook;
            break;
        case 'Q':
            tp = Queen;
            break;
        case 'K':
            tp = King;
            break;
        case '=':
            san++;
            if (*san == 'Q')
                pro = Queen;
            else if (*san == 'R')
                pro = Rook;
            else if (*san == 'B')
                pro = Bishop;
            else if (*san == 'N')
                pro = Knight;
            else
                return M_NONE;
            break;
        case 'x':
        case '+':
        case '#':
            break;
        default:
            return M_NONE;
        }
    }

    if (tp == Neutral)
        tp = Pawn;

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        move = heap->data[i];
        const CSCoord& frCoord = move.GetFromCoord();
        const CSCoord& toCoord = move.GetToCoord();
        int fr = frCoord.BitOffset();

        if (TYPE(p->m_rgPiece[fr]) != tp)
            continue;
        if (toCoord.m_nFile != tfl || toCoord.m_nRank != trk)
            continue;
        if (ffl != -1 && frCoord.m_nFile != ffl)
            continue;
        if (frk != -1 && frCoord.m_nRank != frk)
            continue;
        if (pro && (PromoType(move) != pro))
            continue;
        if (!TryMove(p, move))
            continue;

        return move;
    }

    return M_NONE;
}

CMove CPosition::ParseSAN(const char *san) {
    CPosition *p = this;
    heap_t heap = allocate_heap();
    CMove move = parse_san_with_heap(p, san, heap);
    free_heap(heap);
    return move;
}

/*
 * Parse a move string (in SAN) against supplied move list
 */

CMove ParseSANList(char *san, Color side, CMove *mvs, int cnt, int *pmap) {
    int tp = Neutral;
    int frk = -1, ffl = -1, trk = -1, tfl = -1;
    int pro = 0;
    CMove move;
    int i;

    /* Check castling first */

    if (!strncmp(san, "O-O-O", 5) || !strncmp(san, "o-o-o", 5) ||
        !strncmp(san, "0-0-0", 5)) {
        move = CMove(CSCoord(side == White ? e1 : e8),
                     CSCoord(side == White ? c1 : c8), M_LCASTLE);
        for (i = 0; i < cnt; i++)
            if (move == mvs[i])
                return move;
        return M_NONE;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        move = CMove(CSCoord(side == White ? e1 : e8),
                     CSCoord(side == White ? g1 : g8), M_SCASTLE);
        for (i = 0; i < cnt; i++)
            if (move == mvs[i])
                return move;
        return M_NONE;
    }

    /* special handling of pawn captures a la 'cd' */
    if (strlen(san) == 2 && *san >= 'a' && *san <= 'h' && *(san + 1) >= 'a' &&
        *(san + 1) <= 'h') {
        ffl = *san - 'a';
        tfl = *(san + 1) - 'a';

        for (i = 0; i < cnt; i++) {
            const CSCoord& frCoord = mvs[i].GetFromCoord();
            const CSCoord& toCoord = mvs[i].GetToCoord();
            int fr = frCoord.BitOffset();

            if (pmap[fr] == Pawn && (mvs[i].IsCapture() || mvs[i].IsEnPassant()) &&
                frCoord.m_nFile == ffl && toCoord.m_nFile == tfl)
                return mvs[i];
        }
        return M_NONE;
    }

    /* Next examine the string */
    for (; *san; san++) {
        switch (*san) {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h': {
            ffl = tfl;
            tfl = *san - 'a';
            break;
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8': {
            frk = trk;
            trk = *san - '1';
            break;
        }
        case 'N':
            tp = Knight;
            break;
        case 'B':
            tp = Bishop;
            break;
        case 'R':
            tp = Rook;
            break;
        case 'Q':
            tp = Queen;
            break;
        case 'K':
            tp = King;
            break;
        case '=':
            san++;
            if (*san == 'Q')
                pro = Queen;
            else if (*san == 'R')
                pro = Rook;
            else if (*san == 'B')
                pro = Bishop;
            else if (*san == 'N')
                pro = Knight;
            else
                return M_NONE;
            break;
        case 'x':
        case '+':
        case '#':
            break;
        default:
            return M_NONE;
        }
    }

    if (tp == Neutral)
        tp = Pawn;

    for (i = 0; i < cnt; i++) {
        const CSCoord& frCoord = mvs[i].GetFromCoord();
        const CSCoord& toCoord = mvs[i].GetToCoord();
        int fr = frCoord.BitOffset();

        if (TYPE(pmap[fr]) != tp)
            continue;
        if (toCoord.m_nFile != tfl || toCoord.m_nRank != trk)
            continue;
        if (ffl != -1 && frCoord.m_nFile != ffl)
            continue;
        if (frk != -1 && frCoord.m_nRank != frk)
            continue;
        if (pro && (PromoType(mvs[i]) != pro))
            continue;

        return mvs[i];
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

void legal_moves_internal(CPosition *p, heap_t heap, heap_t tmp_heap) {
    CBitBoard tmp;

    tmp = p->m_rgMask[OPP(p->m_nTurn)][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(tmp_heap);
        p->GenTo(coord, tmp_heap);

        for (i = tmp_heap->current_section->start;
             i < tmp_heap->current_section->end; i++) {
            CMove move = tmp_heap->data[i];
            p->DoMove(move);
            if (!p->InCheck(OPP(p->m_nTurn))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }

        pop_section(tmp_heap);
    }

    tmp = p->m_rgMask[p->m_nTurn][0];
    while (tmp) {
        CSCoord coord = (tmp).FindSetBitCoord();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(tmp_heap);
        p->GenFrom(coord, tmp_heap);

        for (i = tmp_heap->current_section->start;
             i < tmp_heap->current_section->end; i++) {
            CMove move = tmp_heap->data[i];
            if ((move.IsCastle()) && !p->MayCastle(move))
                continue;

            p->DoMove(move);
            if (!p->InCheck(OPP(p->m_nTurn))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }

        pop_section(tmp_heap);
    }

    push_section(tmp_heap);
    p->GenEnpas(tmp_heap);
    {
        unsigned int i;
        for (i = tmp_heap->current_section->start;
             i < tmp_heap->current_section->end; i++) {
            CMove move = tmp_heap->data[i];
            p->DoMove(move);
            if (!p->InCheck(OPP(p->m_nTurn))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }
    }
    pop_section(tmp_heap);
}

int CPosition::LegalMoves(heap_t heap) {
    CPosition *p = this;
    heap_t tmp_heap = allocate_heap();
    heap_t destination = heap;

    if (heap == NULL) {
        destination = allocate_heap();
    }

    legal_moves_internal(p, destination, tmp_heap);

    int cnt =
        destination->current_section->end - destination->current_section->start;

    if (heap == NULL) {
        free_heap(destination);
    }

    free_heap(tmp_heap);

    return cnt;
}

/*
 * Print the current position
 */

void CPosition::ShowPosition() {
    CPosition *p = this;
    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];

        if (level > 0) {
            Print(0, "\n");
        }
        if (CSCoord::NUM_LEVELS > 1) {
            Print(0, "      Level %d\n", level + 1);
        }

        Print(0, "        ");
        for (int file = 0; file < width; file++) {
            Print(0, "+---");
        }
        Print(0, "+\n");

        for (int rk = width - 1; rk >= 0; rk--) {
            char indicator =
                ((rk == width - 1) && (p->m_nTurn)) || ((rk == 0) && (!p->m_nTurn)) ? '>' : ' ';
            Print(0, "    %c %c ", indicator, '1' + rk);
            for (int fl = 0; fl < width; fl++) {
                const int square = static_cast<int>(CSCoord(level, fl, rk));

                Print(0, "|");
                if (p->m_EnPassant.IsValid() && square == p->m_EnPassant.BitOffset())
                    Print(0, "<E>");
                else {
                    if (p->m_rgPiece[square] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                    Print(0, "%c", PieceName[TYPE(p->m_rgPiece[square])]);
                    if (p->m_rgPiece[square] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                }
            }
            if (rk == 4) {
                int bit;
                Print(0, "|   Black (%5d, %5d)  ", p->m_rgnMaterial[Black],
                      p->m_rgnNonPawn[Black]);
                for (bit = 0; bit < 5; bit++) {
                    Print(0, "%c",
                          (p->m_rgbMaterialSignature[Black] & (1 << bit))
                              ? PieceName[bit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (rk == 3) {
                int bit;
                Print(0, "|   White (%5d, %5d)  ", p->m_rgnMaterial[White],
                      p->m_rgnNonPawn[White]);
                for (bit = 0; bit < 5; bit++) {
                    Print(0, "%c",
                          (p->m_rgbMaterialSignature[White] & (1 << bit))
                              ? PieceName[bit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (rk == 6) {
                Print(0, "|   Hashkey: %llx\n", p->m_ullHKey);
            } else if (rk == 1) {
                Print(0, "|   Index: %d\n", RECOGNIZER_INDEX(p));
            } else if (rk == 0) {
                Print(0, "|   MateThreat: %d %d\n", MateThreat(p, White),
                      MateThreat(p, Black));
            } else {
                Print(0, "|\n");
            }

            Print(0, "        ");
            for (int file = 0; file < width; file++) {
                Print(0, "+---");
            }
            Print(0, "+\n");
        }

        Print(0, "         ");
        for (int file = 0; file < width; file++) {
            Print(0, "  %c ", 'a' + file);
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
    char san_buffer[16];

    heap_t heap = allocate_heap();

    push_section(heap);
    p->LegalMoves(heap);

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        CMove move = heap->data[i];
        Print(0, "%s ", p->SAN(move, san_buffer));
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
            Print(0, "%s ", p->SAN(move, san_buffer));
        }
        Print(0, "\n");
    }

    free_heap(heap);
}

static void TestSearchGenerator(CSearchData &sd,
                                CMove (CSearchData::*generator)()) {
    bool comma = false;
    sd.EnterNode();

    while (true) {
        CMove move = (sd.*generator)();
        if (move == M_NONE) {
            break;
        }

        if (sd.m_pPosition->LegalMove(move)) {
            if (comma) {
                Print(0, ", ");
            }
            char san_buffer[16];
            Print(0, "%s", sd.m_pPosition->SAN(move, san_buffer));
            comma = true;
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

    bool comma = false;
    sd.EnterNode();
    while (true) {
        CMove move = NextMoveQFixedAlpha(sd);
        if (move == M_NONE) {
            break;
        }
        if (sd.m_pPosition->LegalMove(move)) {
            if (comma) {
                Print(0, ", ");
            }
            char san_buffer[16];
            Print(0, "%s", sd.m_pPosition->SAN(move, san_buffer));
            comma = true;
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
static void ReadEPD(CPosition *p, const char *epd_input) {
    int rk = 7, fl = 0;
    int i;
    char *ops[MAX_EPD_OPS];
    char *line;
    char san_buffer[16];
    char *x;

    /* Make a copy of the input string, since it will be destroyed
     * due to the use of strtok, sorry :-)
     */

    line = (char *)safe_malloc(strlen(epd_input) + 1);
    strcpy(line, epd_input);
    x = line;

    for (i = 0; i < CSCoord::SIZE; i++)
        p->m_rgPiece[i] = Neutral;
    p->m_rgMask[White][0] = p->m_rgMask[Black][0] = 0;

    /* scan piece placement */
    while (rk >= 0) {
        switch (*x) {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            fl += (*x) - '0';
            break;
        case '-':
            fl += 1;
            break;
        case 'P':
            p->m_rgPiece[fl + (rk << 3)] = Pawn;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'N':
            p->m_rgPiece[fl + (rk << 3)] = Knight;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'B':
            p->m_rgPiece[fl + (rk << 3)] = Bishop;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'R':
            p->m_rgPiece[fl + (rk << 3)] = Rook;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'Q':
            p->m_rgPiece[fl + (rk << 3)] = Queen;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'K':
            p->m_rgPiece[fl + (rk << 3)] = King;
            p->m_rgMask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'p':
            p->m_rgPiece[fl + (rk << 3)] = -Pawn;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'n':
            p->m_rgPiece[fl + (rk << 3)] = -Knight;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'b':
            p->m_rgPiece[fl + (rk << 3)] = -Bishop;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'r':
            p->m_rgPiece[fl + (rk << 3)] = -Rook;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'q':
            p->m_rgPiece[fl + (rk << 3)] = -Queen;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'k':
            p->m_rgPiece[fl + (rk << 3)] = -King;
            p->m_rgMask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case '/':
            fl = 0;
            rk--;
            break;
        case ' ':
            rk = -1;
        }
        x++;
    }

    /* scan p->m_nTurn to move */
    if (*x == 'w') {
        p->m_nTurn = White;
    } else {
        p->m_nTurn = Black;
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    /* scan castling status */
    p->m_bCastle = 0;
    if (*x != '-') {
        if (*x == 'K') {
            p->m_bCastle |= CastleMask[White][0];
            x++;
        }
        if (*x == 'Q') {
            p->m_bCastle |= CastleMask[White][1];
            x++;
        }
        if (*x == 'k') {
            p->m_bCastle |= CastleMask[Black][0];
            x++;
        }
        if (*x == 'q') {
            p->m_bCastle |= CastleMask[Black][1];
            x++;
        }
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    /* scan enpassant status */
    p->m_EnPassant = InvalidSquareCoord();
    if (*x != '-') {
        p->m_EnPassant = CSCoord(0, *x - 'a', *(x + 1) - '1');
        x++;
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    p->RecalcAttacks();
    p->m_wPly = 0;

    i = 0;
    ops[i] = strtok(x, ";");
    while (ops[i]) {
        i++;
        if (i >= MAX_EPD_OPS)
            break;
        ops[i] = strtok(NULL, ";");
    }

    goodmove[0] = M_NONE;
    badmove[0] = M_NONE;

    for (i = 0; ops[i] && i < (MAX_EPD_OPS - 1); i++) {
        char *op = strtok(ops[i], " ");

        if (op) {
            if (!strcmp(op, "bm")) {
                int cnt = 0;

                while ((op = strtok(NULL, " "))) {
                    CMove mv = p->ParseSAN(op);
                    if (mv != M_NONE) {
                        goodmove[cnt] = mv;
                        Print(0, "best move is %s\n",
                              p->SAN(goodmove[cnt], san_buffer));
                        cnt++;
                        if (cnt >= MAX_EPD_MOVES - 1)
                            break;
                    }
                }
                goodmove[cnt] = M_NONE;
            } else if (!strcmp(op, "am")) {
                int cnt = 0;

                while ((op = strtok(NULL, " "))) {
                    CMove mv = p->ParseSAN(op);
                    if (mv != M_NONE) {
                        badmove[cnt] = mv;
                        Print(0, "bad move is %s\n",
                              p->SAN(badmove[cnt], san_buffer));
                        cnt++;
                        if (cnt >= MAX_EPD_MOVES - 1)
                            break;
                    }
                }
                badmove[cnt] = M_NONE;
            }
        }
    }

    /* free the memory allocated
     */

    free(line);
}

/**
 * Create an EPD of the current position
 */

char *CPosition::MakeEPD() {
    CPosition *p = this;
    static char epdbuffer[2048];
    char wname[] = " PNBRQK";
    char bname[] = " pnbrqk";
    char san_buffer[16];

    char *x = epdbuffer;

    for (int level = 0; level < CSCoord::NUM_LEVELS; level++) {
        const int width = CSCoord::LEVEL_WIDTH[level];
        for (int i = width - 1; i >= 0; i--) {
            uint8_t cnt = 0;
            for (int j = 0; j < width; j++) {
                const int square = static_cast<int>(CSCoord(level, j, i));
                if (p->m_rgPiece[square] == Neutral) {
                    cnt++;
                    if (j == width - 1)
                        *(x++) = '0' + cnt;
                } else {
                    if (cnt)
                        *(x++) = '0' + cnt;
                    cnt = 0;
                    if (p->m_rgPiece[square] > 0)
                        *(x++) = wname[TYPE(p->m_rgPiece[square])];
                    else
                        *(x++) = bname[TYPE(p->m_rgPiece[square])];
                }
            }
            if ((level == CSCoord::NUM_LEVELS - 1) && (i == 0))
                *(x++) = ' ';
            else if (i == 0)
                *(x++) = '|';
            else
                *(x++) = '/';
        }
    }
    if (p->m_nTurn == White)
        *(x++) = 'w';
    else
        *(x++) = 'b';
    *(x++) = ' ';

    if (p->m_bCastle & CastleMask[White][0])
        *(x++) = 'K';
    if (p->m_bCastle & CastleMask[White][1])
        *(x++) = 'Q';
    if (p->m_bCastle & CastleMask[Black][0])
        *(x++) = 'k';
    if (p->m_bCastle & CastleMask[Black][1])
        *(x++) = 'q';
    if (!p->m_bCastle)
        *(x++) = '-';
    *(x++) = ' ';

    if (p->m_EnPassant.IsValid()) {
        *(x++) = 'a' + p->m_EnPassant.m_nFile;
        *(x++) = '1' + p->m_EnPassant.m_nRank;
    } else
        *(x++) = '-';
    *(x++) = '\0';

    if (goodmove[0] != M_NONE) {
        int i;
        strcat(epdbuffer, " bm");
        for (i = 0; goodmove[i] != M_NONE; i++) {
            strcat(epdbuffer, " ");
            strcat(epdbuffer, p->SAN(goodmove[i], san_buffer));
        }
        strcat(epdbuffer, ";");
    }

    if (badmove[0] != M_NONE) {
        int i;
        strcat(epdbuffer, " am");
        for (i = 0; badmove[i] != M_NONE; i++) {
            strcat(epdbuffer, " ");
            strcat(epdbuffer, p->SAN(badmove[i], san_buffer));
        }
        strcat(epdbuffer, ";");
    }
    return epdbuffer;
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
static bool has_only_bishops(const CPosition *p, Color side) {
    return (p->m_rgMask[side][Bishop] != 0ULL) &&
           ((p->m_rgMask[side][Knight] | p->m_rgMask[side][Rook] |
             p->m_rgMask[side][Queen]) == 0ULL);
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

bool IsPassed(const CPosition *p, const CSCoord& sqCoord, int side) {
    int sq = sqCoord.BitOffset();
    if (side == White)
        return !(p->m_rgMask[Black][Pawn] & PassedMaskW[sq]);
    else
        return !(p->m_rgMask[White][Pawn] & PassedMaskB[sq]);
}

/**
 * Create a position from an EPD
 */

CPosition *CPosition::CreateFromEPD(const char *epd) {
    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    p->m_cGameLog = INITIAL_GAME_LOG_SIZE;
    p->m_pGameLog = (SGameLog *)safe_calloc(p->m_cGameLog, sizeof(SGameLog));
    p->m_pActLog = p->m_pGameLog;
    ReadEPD(p, epd);
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
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");

    /* we are 'in book' in the InitalPosition */
    p->m_rgwOutOfBookCnt[White] = p->m_rgwOutOfBookCnt[Black] = 0;

    return p;
}

CPosition *CPosition::Clone(const CPosition *src) {
    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    memcpy(p, src, sizeof(CPosition));

    p->m_cGameLog = src->m_cGameLog;
    p->m_pGameLog = (SGameLog *)safe_calloc(p->m_cGameLog, sizeof(SGameLog));
    memcpy(p->m_pGameLog, src->m_pGameLog, sizeof(SGameLog) * p->m_cGameLog);

    p->m_pActLog = p->m_pGameLog + (src->m_pActLog - src->m_pGameLog);

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

