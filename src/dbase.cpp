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
#include "swap.h"
#include "types.h"
#include "utils.h"

#define INITIAL_GAME_LOG_SIZE 40 /* Initial size of game history */

/* Maximum number of EPD ops we attempt to parse */
#define MAX_EPD_OPS 15

bool CPosition::InCheck(int side) const {
    return (bool)(atkFr[kingSq[side].BitOffset()] & mask[!side][0]);
}

bool CPosition::IsPassed(int sq, int side) const {
    if (side == White)
        return !(mask[Black][Pawn] & PassedMaskW[sq]);
    else
        return !(mask[White][Pawn] & PassedMaskB[sq]);
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
    {0x01, 0x02}, /* White can castle king/queenp->turn */
    {0x04, 0x08}  /* dito for black */
};

/* local prototypes
 */

static void AtkSet(CPosition *, int, int, int);
static void AtkClr(CPosition *, int);
static void GainAttack(CPosition *, int, int);
static void LooseAttack(CPosition *, int, int);
static void GainAttacks(CPosition *, int);
static void LooseAttacks(CPosition *, int);

/*
 * Routines to up/downdate the global database
 */

static void ShowMoveList(CPosition *p) {
    int ply;
    for (ply = 0; ply < p->ply; ply++) {
        CMove move = p->gameLog[ply].gl_Move;
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
    int kingSq = p->kingSq[White].BitOffset();
    int i, color;
    CBitBoard temp;

    for (i = 0; i < 64; i++) {
        temp = p->atkTo[i];
        while (temp) {
            int sq = (temp).FindSetBit();
            temp.ClearLowestBit();
            if (!p->atkFr[sq].TstBit(i)) {
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
            temp = p->mask[color][i];
            while (temp) {
                int sq = (temp).FindSetBit();
                temp.ClearLowestBit();
                int pc = (1 - 2 * color) * i;
                if (p->piece[sq] != pc) {
                    Print(0, "Piece on %c%c is %d, expected %d!\n", SQUARE(sq),
                          p->piece[sq], pc);
                    ShowMoveList(p);
                    p->ShowPosition();
                    abort();
                }
            }
        }
    }

    if (p->atkTo[kingSq] != KingEPM[kingSq]) {
        Print(0, "White king is bad:\n");
        PrintBitBoard(p->atkTo[kingSq]);
        Print(0, "should be:\n");
        PrintBitBoard(KingEPM[kingSq]);
        ShowMoveList(p);
        p->ShowPosition();
        abort();
    }
    kingSq = p->kingSq[Black].BitOffset();
    if (p->atkTo[kingSq] != KingEPM[kingSq]) {
        Print(0, "Black king is bad:\n");
        PrintBitBoard(p->atkTo[kingSq]);
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

static void AtkSet(CPosition *p, int type, int color, int square) {
    CBitBoard attacks;

    switch (type) {
    case Pawn:
        attacks = PawnEPM[color][square];
        break;
    case Knight:
        attacks = KnightEPM[square];
        break;
    case Bishop:
        attacks = bishop_attacks(square, p->mask[0][0] | p->mask[1][0]);
        break;
    case Rook:
        attacks = rook_attacks(square, p->mask[0][0] | p->mask[1][0]);
        break;
    case Queen:
        attacks = bishop_attacks(square, p->mask[0][0] | p->mask[1][0]) |
                  rook_attacks(square, p->mask[0][0] | p->mask[1][0]);
        break;
    case King:
        attacks = KingEPM[square];
        break;
    default:
        printf("AtkSet(%d, %d, %d)\n", type, color, square);
        Panic(p);
        return; // never reached
    }

    p->atkTo[square] = attacks;
    while (attacks) {
        int i = (attacks).FindSetBit();
        attacks.ClearLowestBit();
        p->atkFr[i].SetBit(square);
    }
}

static void AtkClr(CPosition *p, int square) {
    CBitBoard tmp = p->atkTo[square];
    p->atkTo[square] = 0;

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        p->atkFr[i].ClrBit(square);
    }
}

/*
 * Recalculate Attacks from "from" to "to" after the piece on "to" has
 * been removed
 */

static void GainAttack(CPosition *p, int from, int to) {
    signed char *nsq = NextSQ[from];
    int sq = to;
    const CBitBoard all = p->mask[0][0] | p->mask[1][0];

    for (;;) {
        sq = nsq[sq];
        if (sq < 0)
            break;

        p->atkTo[from].SetBit(sq);
        p->atkFr[sq].SetBit(from);

        if (all.TstBit(sq))
            break;
    }
}

/*
 * Recalculate Attacks from "from" to "to" after a piece has been put
 * onto "to"
 */

static void LooseAttack(CPosition *p, int from,
                        int to) {
    signed char *nsq = NextSQ[from];
    int sq = to;
    const CBitBoard all = p->mask[0][0] | p->mask[1][0];

    for (;;) {
        sq = nsq[sq];
        if (sq < 0)
            break;

        p->atkTo[from].ClrBit(sq);
        p->atkFr[sq].ClrBit(from);

        if (all.TstBit(sq))
            break;
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * the piece on this square has been removed
 */

static void GainAttacks(CPosition *p, int to) {
    CBitBoard tmp = p->atkFr[to] & p->slidingPieces;
    int i;

    while (tmp) {
        i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        GainAttack(p, i, to);
    }
}

/*
 * Recalculate all ray attacks which pass through square "to" after
 * a piece has been put onto this square
 */

static void LooseAttacks(CPosition *p, int to) {
    CBitBoard tmp = p->atkFr[to] & p->slidingPieces;
    int i;

    while (tmp) {
        i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        LooseAttack(p, i, to);
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
    const CSCoord oldRookCoord(fromCoord.Level,
                               move.IsShortCastle() ? fromCoord.File + 3 : fromCoord.File - 4,
                               fromCoord.Rank);
    const CSCoord newRookCoord(fromCoord.Level,
                               move.IsShortCastle() ? fromCoord.File + 1 : fromCoord.File - 1,
                               fromCoord.Rank);
    int oldRookOffset = oldRookCoord.BitOffset();
    int newRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    AtkClr(p, fromOffset);

    /* rook looses its attacks */
    AtkClr(p, oldRookOffset);

    /* move king on the board */
    p->piece[toOffset] = p->piece[fromOffset];
    p->piece[fromOffset] = Neutral;
    p->mask[p->turn][0].ClrBit(fromOffset);
    p->mask[p->turn][King].ClrBit(fromOffset);
    p->mask[p->turn][0].SetBit(toOffset);
    p->mask[p->turn][King].SetBit(toOffset);

    /* move rook on the board */
    p->piece[newRookOffset] = p->piece[oldRookOffset];
    p->piece[oldRookOffset] = Neutral;
    p->mask[p->turn][0].ClrBit(oldRookOffset);
    p->mask[p->turn][Rook].ClrBit(oldRookOffset);
    p->slidingPieces.ClrBit(oldRookOffset);
    p->mask[p->turn][0].SetBit(newRookOffset);
    p->mask[p->turn][Rook].SetBit(newRookOffset);
    p->slidingPieces.SetBit(newRookOffset);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */

    GainAttacks(p, fromOffset);

    /* King and rook gain their attacks
     */

    AtkSet(p, King, p->turn, toOffset);
    AtkSet(p, Rook, p->turn, newRookOffset);
    p->kingSq[p->turn] = toCoord;

    /* update hashkey */
    /* Das koennte ich vorher berechnen! Ist dann nur eine Anweisung! */

    p->hkey ^= (HashKeys[p->turn][King][fromOffset] ^ HashKeys[p->turn][King][toOffset] ^
                HashKeys[p->turn][Rook][oldRookOffset] ^
                HashKeys[p->turn][Rook][newRookOffset]);
}

/*
 * Unmake a castle move
 */

static void UndoCastle(CPosition *p, CMove move) {
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    const CSCoord oldRookCoord(fromCoord.Level,
                               move.IsShortCastle() ? fromCoord.File + 3 : fromCoord.File - 4,
                               fromCoord.Rank);
    const CSCoord newRookCoord(fromCoord.Level,
                               move.IsShortCastle() ? fromCoord.File + 1 : fromCoord.File - 1,
                               fromCoord.Rank);
    int oldRookOffset = oldRookCoord.BitOffset();
    int newRookOffset = newRookCoord.BitOffset();

    /* king looses its attacks */
    AtkClr(p, toOffset);

    /* rook looses its attacks */
    AtkClr(p, newRookOffset);

    /* re-calculate attacks through king-square
     * no need to do it for the rook, since it was on the edge of the board
     * For the same reason we don't have to LooseAttacks on any of the
     * new king/rook squares
     */
    LooseAttacks(p, fromOffset);

    /* move king on the board */
    p->piece[fromOffset] = p->piece[toOffset];
    p->piece[toOffset] = Neutral;
    p->mask[p->turn][0].ClrBit(toOffset);
    p->mask[p->turn][King].ClrBit(toOffset);
    p->mask[p->turn][0].SetBit(fromOffset);
    p->mask[p->turn][King].SetBit(fromOffset);

    /* move rook on the board */
    p->piece[oldRookOffset] = p->piece[newRookOffset];
    p->piece[newRookOffset] = Neutral;
    p->mask[p->turn][0].ClrBit(newRookOffset);
    p->mask[p->turn][Rook].ClrBit(newRookOffset);
    p->slidingPieces.ClrBit(newRookOffset);
    p->mask[p->turn][0].SetBit(oldRookOffset);
    p->mask[p->turn][Rook].SetBit(oldRookOffset);
    p->slidingPieces.SetBit(oldRookOffset);

    /* King and rook gain their attacks
     */

    AtkSet(p, King, p->turn, fromOffset);
    AtkSet(p, Rook, p->turn, oldRookOffset);
    p->kingSq[p->turn] = fromCoord;
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
    int8_t tp = TYPE(p->piece[fromOffset]);

    /* save EnPassant and Castling */
    p->actLog->gl_EnPassant = p->enPassant;
    p->actLog->gl_Castle = p->castle;
    p->actLog->gl_HashKey = p->hkey;
    p->actLog->gl_PawnKey = p->pkey;

    if (move.IsCastle()) {
        DoCastle(p, move);
        p->castle &= ~(CastleMask[p->turn][0] | CastleMask[p->turn][1]);
    } else {
        /* piece looses its attacks */
        AtkClr(p, fromOffset);

        if (tp == King) {
            p->kingSq[p->turn] = toCoord;
        }

        /* remove it from the board */
        p->piece[fromOffset] = Neutral;
        p->mask[p->turn][0].ClrBit(fromOffset);
        p->mask[p->turn][tp].ClrBit(fromOffset);
        if (is_sliding(tp))
            p->slidingPieces.ClrBit(fromOffset);
        /* re-calculate attacks through from-square */
        GainAttacks(p, fromOffset);

        /* update hashkey */
        p->hkey ^= HashKeys[p->turn][tp][fromOffset];
        if (tp == Pawn)
            p->pkey ^= HashKeys[p->turn][Pawn][fromOffset];

        if (tp == King) {
            /* No more castling rights */
            p->castle &= ~(CastleMask[p->turn][0] | CastleMask[p->turn][1]);
        } else if (tp == Rook) {
            if (fromOffset == (p->turn == White ? h1 : h8))
                p->castle &= ~(CastleMask[p->turn][0]);
            if (fromOffset == (p->turn == White ? a1 : a8))
                p->castle &= ~(CastleMask[p->turn][1]);
        }
        if (move.IsCapture()) {
            int sp = TYPE(p->piece[toOffset]);

            /* piece looses its attacks */
            AtkClr(p, toOffset);

            /* remember type of captured piece */
            p->actLog->gl_Piece = p->piece[toOffset];

            p->mask[OPP(p->turn)][0].ClrBit(toOffset);
            p->mask[OPP(p->turn)][sp].ClrBit(toOffset);
            if (is_sliding(sp))
                p->slidingPieces.ClrBit(toOffset);

            /* Update oppponents material and PawnCount */
            p->material[OPP(p->turn)] -= Value[sp];
            if (sp != Pawn)
                p->nonPawn[OPP(p->turn)] -= Value[sp];

            /* update material signature */
            if (!(p->mask[OPP(p->turn)][sp])) {
                p->material_signature[OPP(p->turn)] &= ~SIGNATURE_BIT(sp);
            }

            /* update hashkey */
            p->hkey ^= HashKeys[OPP(p->turn)][sp][toOffset];
            if (sp == Pawn)
                p->pkey ^= HashKeys[OPP(p->turn)][Pawn][toOffset];
            if (toOffset == (OPP(p->turn) == White ? h1 : h8)) {
                p->castle &= ~(CastleMask[OPP(p->turn)][0]);
            }
            if (toOffset == (OPP(p->turn) == White ? a1 : a8)) {
                p->castle &= ~(CastleMask[OPP(p->turn)][1]);
            }
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.Level, toCoord.File,
                p->turn == White ? toCoord.Rank - 1 : toCoord.Rank + 1);
            int capturedPawnOffset = capturedPawnCoord.BitOffset();

            /* piece looses its attacks */
            AtkClr(p, capturedPawnOffset);

            /* captured piece must be a pawn */
            p->actLog->gl_Piece = ((OPP(p->turn) == White) ? Pawn : -Pawn);

            p->mask[OPP(p->turn)][0].ClrBit(capturedPawnOffset);
            p->mask[OPP(p->turn)][Pawn].ClrBit(capturedPawnOffset);

            /* re-calculate attacks through to-square */
            GainAttacks(p, capturedPawnOffset);

            /* remove captured pawn from the board */
            p->piece[capturedPawnOffset] = Neutral;

            /* Update oppponents material and PawnCount */
            p->material[OPP(p->turn)] -= Value[Pawn];

            /* update material signature */
            if (!(p->mask[OPP(p->turn)][Pawn])) {
                p->material_signature[OPP(p->turn)] &= ~SIGNATURE_BIT(Pawn);
            }

            /* update hashkey */
            p->hkey ^= HashKeys[OPP(p->turn)][Pawn][capturedPawnOffset];
            p->pkey ^= HashKeys[OPP(p->turn)][Pawn][capturedPawnOffset];

            /* re-calculate attacks through to-square */
            LooseAttacks(p, toOffset);
        } else {
            /* re-calculate attacks through to-square */
            LooseAttacks(p, toOffset);
        }

        if (move.HasPromotion()) {
            /* Promote piece */
            tp = PromoType(move);

            /* Update own material */
            p->material[p->turn] += Value[tp] - Value[Pawn];
            p->nonPawn[p->turn] += Value[tp];

            if (!(p->mask[p->turn][Pawn])) {
                p->material_signature[p->turn] &= ~SIGNATURE_BIT(Pawn);
            }
            p->material_signature[p->turn] |= SIGNATURE_BIT(tp);
        }

        /* put it on the board again */
        p->piece[toOffset] = (p->turn == White) ? tp : -tp;
        p->mask[p->turn][0].SetBit(toOffset);
        p->mask[p->turn][tp].SetBit(toOffset);
        if (is_sliding(tp))
            p->slidingPieces.SetBit(toOffset);

        /* piece gains its attacks */
        AtkSet(p, tp, p->turn, toOffset);

        /* update hashkey */
        p->hkey ^= HashKeys[p->turn][tp][toOffset];
        if (tp == Pawn)
            p->pkey ^= HashKeys[p->turn][Pawn][toOffset];
    }

    /* Check if loss of castling rights */
    if (p->castle != p->actLog->gl_Castle) {
        p->hkey ^= HashKeysCastle[p->actLog->gl_Castle];
        p->hkey ^= HashKeysCastle[p->castle];
    }

    /*
     * Check if double pawn push. There is a little trick here:
     * We only set the enPassant flag if there is a possibility
     * of an enPassant capture at all. This increases the efficiency of
     * the transposition table.
     */

    p->enPassant = InvalidSquareCoord();
    if (move.IsPawnDoublePush()) {
        const CSCoord passantCoord(toCoord.Level, toCoord.File,
                                   p->turn == White ? toCoord.Rank - 1 : toCoord.Rank + 1);
        int passantOffset = passantCoord.BitOffset();
        if (p->atkFr[passantOffset] & p->mask[OPP(p->turn)][Pawn]) {
            p->enPassant = passantCoord;
        }
    }

    if ((p->enPassant.IsValid() != p->actLog->gl_EnPassant.IsValid()) ||
        (p->enPassant.IsValid() &&
         p->enPassant.BitOffset() != p->actLog->gl_EnPassant.BitOffset())) {
        p->hkey ^=
            HashKeysEP[p->actLog->gl_EnPassant.IsValid()
                           ? p->actLog->gl_EnPassant.BitOffset()
                           : 0];
        p->hkey ^= HashKeysEP[p->enPassant.IsValid() ? p->enPassant.BitOffset()
                                                     : 0];
    }

    /* Update GameLog */
    p->actLog->gl_Move = move;
    p->ply++;

    /* Grow gameLog if needed. */
    if (p->ply >= p->gameLogSize) {
        p->gameLogSize *= 2;
        p->gameLog =
            (GameLog *)(GameLog *)realloc(p->gameLog, sizeof(GameLog) * p->gameLogSize);
        p->actLog = p->gameLog + p->ply;
    } else {
        p->actLog++;
    }

    /* Check if reversible move */
    if (move.IsCapture() || move.HasPromotion() || move.IsCastle() || tp == Pawn) {
        p->actLog->gl_IrrevCount = 0;
    } else {
        p->actLog->gl_IrrevCount = (p->actLog - 1)->gl_IrrevCount + 1;
    }

    /* Swap p->turns */
    p->turn = OPP(p->turn);
    p->hkey ^= STMKey;
}

void CPosition::UndoMove(CMove move) {
    CPosition *p = this;
    const CSCoord& fromCoord = move.GetFromCoord();
    const CSCoord& toCoord = move.GetToCoord();
    int fromOffset = fromCoord.BitOffset();
    int toOffset = toCoord.BitOffset();
    int8_t tp = TYPE(p->piece[toOffset]);

    /* Swap p->turns */
    p->turn = OPP(p->turn);

    /* Decrement ActLog */
    p->actLog--;
    p->ply--;

    if (move.IsCastle()) {
        UndoCastle(p, move);
    } else {
        /* piece looses its attacks */
        AtkClr(p, toOffset);

        if (tp == King) {
            p->kingSq[p->turn] = fromCoord;
        }

        /* update masks */
        p->mask[p->turn][0].ClrBit(toOffset);
        p->mask[p->turn][tp].ClrBit(toOffset);
        if (is_sliding(tp))
            p->slidingPieces.ClrBit(toOffset);

        if (move.HasPromotion()) {
            /* Update own material */
            p->material[p->turn] -= Value[tp] - Value[Pawn];
            p->nonPawn[p->turn] -= Value[tp];

            /* update material signature */
            if (!(p->mask[p->turn][tp])) {
                p->material_signature[p->turn] &= ~SIGNATURE_BIT(tp);
            }

            /* Unpromote piece */
            tp = Pawn;

            /* update material signature */
            p->material_signature[p->turn] |= SIGNATURE_BIT(Pawn);
        }

        if (move.IsCapture()) {
            int8_t sp = p->actLog->gl_Piece;

            /* piece gains its attacks */
            AtkSet(p, TYPE(sp), OPP(p->turn), toOffset);

            p->piece[toOffset] = sp;
            sp = TYPE(sp);
            p->mask[OPP(p->turn)][0].SetBit(toOffset);
            p->mask[OPP(p->turn)][sp].SetBit(toOffset);
            if (is_sliding(sp))
                p->slidingPieces.SetBit(toOffset);

            /* Update oppponents material and PawnCount */
            p->material[OPP(p->turn)] += Value[sp];
            if (sp != Pawn)
                p->nonPawn[OPP(p->turn)] += Value[sp];

            /* update material signature */
            p->material_signature[OPP(p->turn)] |= SIGNATURE_BIT(sp);
        } else if (move.IsEnPassant()) {
            const CSCoord capturedPawnCoord(
                toCoord.Level, toCoord.File,
                p->turn == White ? toCoord.Rank - 1 : toCoord.Rank + 1);
            int capturedPawnOffset = capturedPawnCoord.BitOffset();

            /* piece looses its attacks */
            AtkSet(p, Pawn, OPP(p->turn), capturedPawnOffset);

            p->mask[OPP(p->turn)][0].SetBit(capturedPawnOffset);
            p->mask[OPP(p->turn)][Pawn].SetBit(capturedPawnOffset);

            /* re-calculate attacks through to-square */
            LooseAttacks(p, capturedPawnOffset);

            /* remove captured pawn from the board */
            p->piece[capturedPawnOffset] = (OPP(p->turn) == White) ? Pawn : -Pawn;
            p->piece[toOffset] = Neutral;

            /* re-calculate attacks through to-square */
            GainAttacks(p, toOffset);

            /* Update oppponents material */
            p->material[OPP(p->turn)] += Value[Pawn];

            /* update material signature */
            p->material_signature[OPP(p->turn)] |= SIGNATURE_BIT(Pawn);
        } else {
            p->piece[toOffset] = Neutral;

            /* re-calculate attacks through to-square */
            GainAttacks(p, toOffset);
        }

        /* re-calculate attacks through from-square */
        LooseAttacks(p, fromOffset);

        /* put it on the board again */
        p->piece[fromOffset] = (p->turn == White) ? tp : -tp;
        p->mask[p->turn][0].SetBit(fromOffset);
        p->mask[p->turn][tp].SetBit(fromOffset);
        if (is_sliding(tp))
            p->slidingPieces.SetBit(fromOffset);

        /* piece gains its attacks */
        AtkSet(p, tp, p->turn, fromOffset);
    }

    /* restore EnPassant and Castling */
    p->enPassant = p->actLog->gl_EnPassant;
    p->castle = p->actLog->gl_Castle;

    p->hkey = p->actLog->gl_HashKey;
    p->pkey = p->actLog->gl_PawnKey;

    /*
    DebugEngine(move);
    */
}

/*
 * Make a null move, i.e. swap the p->turn on the move
 */

void CPosition::DoNull() {
    CPosition *p = this;
    /* Update GameLog */
    p->actLog->gl_Move = M_NULL;
    p->actLog->gl_EnPassant = p->enPassant;
    p->actLog->gl_Castle = p->castle;
    p->actLog->gl_HashKey = p->hkey;
    p->enPassant = InvalidSquareCoord();

    if ((p->enPassant.IsValid() != p->actLog->gl_EnPassant.IsValid()) ||
        (p->enPassant.IsValid() &&
         p->enPassant.BitOffset() != p->actLog->gl_EnPassant.BitOffset())) {
        p->hkey ^=
            HashKeysEP[p->actLog->gl_EnPassant.IsValid()
                           ? p->actLog->gl_EnPassant.BitOffset()
                           : 0];
        p->hkey ^= HashKeysEP[p->enPassant.IsValid() ? p->enPassant.BitOffset()
                                                     : 0];
    }

    p->ply++;

    /* Grow gameLog if needed. */
    if (p->ply >= p->gameLogSize) {
        p->gameLogSize *= 2;
        p->gameLog =
            (GameLog *)(GameLog *)realloc(p->gameLog, sizeof(GameLog) * p->gameLogSize);
        p->actLog = p->gameLog + p->ply;
    } else {
        p->actLog++;
    }

    /* treat null move as irreversible */
    p->actLog->gl_IrrevCount = 0;

    /* swap p->turns */
    p->turn = OPP(p->turn);
    p->hkey ^= STMKey;
}

/*
 * Unmake a null move
 */

void CPosition::UndoNull() {
    CPosition *p = this;
    p->turn = OPP(p->turn);

    /* Decrement ActLog */
    p->actLog--;
    p->ply--;

    p->enPassant = p->actLog->gl_EnPassant;
    p->hkey = p->actLog->gl_HashKey;
}

/*
 * Given the Masks and the p->piece[] array, recalculate all necessary data
 */

void CPosition::RecalcAttacks() {
    CPosition *p = this;
    int i;
    CBitBoard tmp;

    for (i = 0; i < 64; i++) {
        p->atkTo[i] = p->atkFr[i] = 0;
    }

    for (i = Pawn; i <= King; i++) {
        p->mask[White][i] = p->mask[Black][i] = 0;
    }

    p->slidingPieces = 0;

    p->material[White] = p->material[Black] = 0;
    p->nonPawn[White] = p->nonPawn[Black] = 0;
    p->material_signature[White] = p->material_signature[Black] = 0;
    p->hkey = p->pkey = 0;

    tmp = p->mask[White][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int pc = p->piece[i];
        tmp.ClearLowestBit();
        p->mask[White][pc].SetBit(i);
        if (is_sliding(pc))
            p->slidingPieces.SetBit(i);
        p->material[White] += Value[pc];
        p->hkey ^= HashKeys[White][pc][i];
        if (pc != Pawn)
            p->nonPawn[White] += Value[pc];
        else {
            p->pkey ^= HashKeys[White][Pawn][i];
        }

        if (pc != King) {
            p->material_signature[White] |= SIGNATURE_BIT(pc);
        }
    }

    tmp = p->mask[Black][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        int pc = -p->piece[i];
        tmp.ClearLowestBit();
        p->mask[Black][pc].SetBit(i);
        if (is_sliding(pc))
            p->slidingPieces.SetBit(i);
        p->material[Black] += Value[pc];
        p->hkey ^= HashKeys[Black][pc][i];
        if (pc != Pawn)
            p->nonPawn[Black] += Value[pc];
        else {
            p->pkey ^= HashKeys[Black][Pawn][i];
        }

        if (pc != King) {
            p->material_signature[Black] |= SIGNATURE_BIT(pc);
        }
    }

    tmp = p->mask[White][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        AtkSet(p, p->piece[i], White, i);
    }

    tmp = p->mask[Black][0];
    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        AtkSet(p, -p->piece[i], Black, i);
    }

    p->kingSq[White] = CSCoord((p->mask[White][King]).FindSetBit());
    p->kingSq[Black] = CSCoord((p->mask[Black][King]).FindSetBit());

    p->hkey ^= HashKeysCastle[p->castle];
    if (p->turn == Black)
        p->hkey ^= STMKey;

    if (p->enPassant.IsValid()) {
        p->hkey ^= HashKeysEP[p->enPassant.BitOffset()];
    }
}

/*
 * Generate all capturing moves to a square "square"
 */
void CPosition::GenTo(int square, heap_t heap) {
    CPosition *p = this;
    CBitBoard tmp = p->atkFr[square] & p->mask[p->turn][0];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (TYPE(p->piece[i]) == Pawn && is_promo_square(CSCoord(square))) {
            append_to_heap(heap, make_promotion(i, square, Queen, M_CAPTURE));
            append_to_heap(heap, make_promotion(i, square, Knight, M_CAPTURE));
            append_to_heap(heap, make_promotion(i, square, Rook, M_CAPTURE));
            append_to_heap(heap, make_promotion(i, square, Bishop, M_CAPTURE));
        } else {
            append_to_heap(heap, make_move(i, square, M_CAPTURE));
        }
    }
}

void CPosition::GenEnpas(heap_t heap) {
    CPosition *p = this;
    CBitBoard tmp;

    if (!p->enPassant.IsValid())
        return;

    tmp = p->atkFr[p->enPassant.BitOffset()] & p->mask[p->turn][Pawn];
    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        append_to_heap(heap, make_move(i, p->enPassant.BitOffset(), M_ENPASSANT));
    }
}

/*
 * Generate all non-capturing moves from "square"
 */

void CPosition::GenFrom(int square, heap_t heap) {
    CPosition *p = this;
    if (TYPE(p->piece[square]) != Pawn) {
        CBitBoard tmp;

        tmp = p->atkTo[square] & ~(p->mask[White][0] | p->mask[Black][0]);

        while (tmp) {
            int i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            append_to_heap(heap, make_move(square, i, 0));
        }

        /* Generate castling moves
         * we will check legality later...
         */

        if (TYPE(p->piece[square]) == King) {
            if (p->castle & CastleMask[p->turn][0]) {
                /* OK, we might castle king p->turn */
                append_to_heap(heap, make_move(p->turn == White ? e1 : e8,
                                               p->turn == White ? g1 : g8,
                                               M_SCASTLE));
            }
            if (p->castle & CastleMask[p->turn][1]) {
                append_to_heap(heap, make_move(p->turn == White ? e1 : e8,
                                               p->turn == White ? c1 : c8,
                                               M_LCASTLE));
            }
        }
    } else {
        int sq = (p->turn == White ? square + 8 : square - 8);

        if (p->piece[sq] == Neutral) {
            if (is_promo_square(CSCoord(sq))) {
                append_to_heap(heap, make_promotion(square, sq, Queen, 0));
                append_to_heap(heap, make_promotion(square, sq, Knight, 0));
                append_to_heap(heap, make_promotion(square, sq, Rook, 0));
                append_to_heap(heap, make_promotion(square, sq, Bishop, 0));
            } else {
                append_to_heap(heap, make_move(square, sq, 0));

                if ((p->turn == White && square <= h2) ||
                    (p->turn == Black && square >= a7)) {
                    sq = (p->turn == White ? sq + 8 : sq - 8);
                    if (p->piece[sq] == Neutral) {
                        append_to_heap(heap, make_move(square, sq, M_PAWND));
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
    const CSCoord kingHome((p->turn == White) ? e1 : e8);
    /* Sometimes there might be a legal castling move, but for the
       wrong p->turn, probably from the Countermove table */
    if (fromCoord.Level != kingHome.Level || fromCoord.File != kingHome.File ||
        fromCoord.Rank != kingHome.Rank)
        return false;

    if (p->InCheck(p->turn))
        return false;

    /* king p->turn castling */
    if (move.IsShortCastle() && (p->castle & CastleMask[p->turn][0])) {
        int fs = (p->turn == White ? f1 : f8);
        int gs = (p->turn == White ? g1 : g8);

        /* Check if f and g square are empty */
        if (p->piece[fs] == Neutral && p->piece[gs] == Neutral) {
            /* Check if f and g square are not attacked by opponent */
            if ((p->atkFr[fs] | p->atkFr[gs]) & p->mask[OPP(p->turn)][0])
                return false;
            else
                return true;
        }
    }

    /* queen p->turn castling */
    if (move.IsLongCastle() && (p->castle & CastleMask[p->turn][1])) {
        int bs = (p->turn == White ? b1 : b8);
        int cs = (p->turn == White ? c1 : c8);
        int ds = (p->turn == White ? d1 : d8);

        /* Check if b, c and d square are empty */
        if (p->piece[bs] == Neutral && p->piece[cs] == Neutral &&
            p->piece[ds] == Neutral) {
            /* Check if c and d square are not attacked by opponent */
            if ((p->atkFr[cs] | p->atkFr[ds]) & p->mask[OPP(p->turn)][0])
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
    if (!SAME_COLOR(p->piece[fr], p->turn))
        return false;

    /* if a promotion, moving piece must be a pawn */
    if (move.HasPromotion() && TYPE(p->piece[fr]) != Pawn)
        return false;

    /* if the move is a pawn move to the 1st/8th rank, it must be
     * be a promotion.
     */
    if (TYPE(p->piece[fr]) == Pawn && !move.HasPromotion()) {
        const int levelWidth = CSCoord::LEVEL_WIDTH[toCoord.Level];
        if (toCoord.Rank == 0 || toCoord.Rank == (levelWidth - 1))
            return false;
    }

    if (move.IsCapture()) {
        /* There must be an enemy piece on the target square, and we
         * must attack that square
         */

        if (!SAME_COLOR(p->piece[to], OPP(p->turn)) ||
            !p->atkTo[fr].TstBit(to)) {
            return false;
        }
        return true;
    } else if (move.IsEnPassant()) {
        /* The moving piece must be a pawn, and the target square must be
         * the enpassant square
         */

        if (!p->enPassant.IsValid())
            return false;
        if (TYPE(p->piece[fr]) != Pawn || to != p->enPassant.BitOffset())
            return false;
        if (!p->atkTo[fr].TstBit(to))
            return false;

        return true;
    } else if (move.IsCastle()) {
        /* Call the castling test routine */
        return p->MayCastle(move);
    } else {
        /* target sqaure must be empty */
        if (p->piece[to] != Neutral)
            return false;

        if (TYPE(p->piece[fr]) != Pawn) {
            /* if no pawn, we must attack to square */
            if (!p->atkTo[fr].TstBit(to))
                return false;
            if (move.IsPawnDoublePush())
                return false;
            return true;
        } else {
            /* use NextPos array to check if legal move */
            const int levelWidth = CSCoord::LEVEL_WIDTH[frCoord.Level];
            const int rankStep = (p->turn == White ? 1 : -1);
            int ttRank = frCoord.Rank + rankStep;
            if (ttRank < 0 || ttRank >= levelWidth)
                return false;
            int tt = CSCoord(frCoord.Level, frCoord.File, ttRank).BitOffset();
            if (move.IsPawnDoublePush()) {
                if (p->piece[tt] != Neutral)
                    return false;
                ttRank += rankStep;
                if (ttRank < 0 || ttRank >= levelWidth)
                    return false;
                tt = CSCoord(frCoord.Level, frCoord.File, ttRank).BitOffset();
            }
            if (tt != to)
                return false;

            if (p->turn == White && toCoord.Rank == (levelWidth - 1) &&
                !move.HasPromotion())
                return false;
            if (p->turn == Black && toCoord.Rank == 0 && !move.HasPromotion())
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
    int tp = TYPE(p->piece[fr]);
    int kp = p->mask[OPP(p->turn)][King].FindSetBit();
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
            if (!((p->mask[White][0] | p->mask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Rook:
        if (RookEPM[kp].TstBit(to)) {
            if (!((p->mask[White][0] | p->mask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Queen:
        if (QueenEPM[kp].TstBit(to)) {
            if (!((p->mask[White][0] | p->mask[Black][0]) & InterPath[kp][to]))
                return true;
        }
        break;
    case Pawn:
        {
            const CSCoord kingCoord(kp);
            int fileDelta = kingCoord.File - toCoord.File;
            int rankDelta = kingCoord.Rank - toCoord.Rank;
            if (kingCoord.Level == toCoord.Level) {
                if (p->turn == White && rankDelta == 1 &&
                    (fileDelta == -1 || fileDelta == 1))
                    return true;
                if (p->turn == Black && rankDelta == -1 &&
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

    tmp = (p->mask[p->turn][Bishop] | p->mask[p->turn][Rook] |
           p->mask[p->turn][Queen]) &
          QueenEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        CBitBoard tmp2;

        tmp.ClearLowestBit();
        if (TYPE(p->piece[i]) == Bishop && !BishopEPM[kp].TstBit(i))
            continue;
        if (TYPE(p->piece[i]) == Rook && !RookEPM[kp].TstBit(i))
            continue;

        tmp2 = (p->mask[White][0] | p->mask[Black][0]) & InterPath[kp][i];
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
    int kp = p->kingSq[OPP(p->turn)].BitOffset();
    CBitBoard *ip = InterPath[kp];
    CBitBoard fsq = p->mask[p->turn][0];
    CBitBoard all = (p->mask[White][0] | p->mask[Black][0]);

    /* First find all blockers, i.e. pieces that give check when they move
     * from their current square
     */

    tmp = (p->mask[p->turn][Bishop] | p->mask[p->turn][Queen]) & BishopEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (ip[i] && !(ip[i] & p->mask[OPP(p->turn)][0])) {
            CBitBoard tmp2 = p->mask[p->turn][0] & ip[i];

            if ((tmp2).CountBits() == 1) {
                int j = (tmp2).FindSetBit();

                if (fsq.TstBit(j)) {
                    p->GenFrom(j, heap);
                    fsq.ClrBit(j);
                }
            }
        }
    }

    tmp = (p->mask[p->turn][Rook] | p->mask[p->turn][Queen]) & RookEPM[kp];

    while (tmp) {
        int i = (tmp).FindSetBit();
        tmp.ClearLowestBit();
        if (ip[i] && !(ip[i] & p->mask[OPP(p->turn)][0])) {
            CBitBoard tmp2 = p->mask[p->turn][0] & ip[i];

            if ((tmp2).CountBits() == 1) {
                int j = (tmp2).FindSetBit();

                if (fsq.TstBit(j)) {
                    p->GenFrom(j, heap);
                    fsq.ClrBit(j);
                }
            }
        }
    }

    /* Find direct checks by Bishop or Queen */
    tmp = BishopEPM[kp];
    tmp &= ~all;

    fr = p->mask[p->turn][Bishop] | p->mask[p->turn][Queen];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2 = p->atkTo[sq] & tmp;
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

    fr = p->mask[p->turn][Rook] | p->mask[p->turn][Queen];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2 = p->atkTo[sq] & tmp;
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

    fr = p->mask[p->turn][Knight];
    fr &= fsq;

    while (fr) {
        int sq = (fr).FindSetBit();
        CBitBoard tmp2;

        fr.ClearLowestBit();
        tmp2 = p->atkTo[sq] & tmp;

        while (tmp2) {
            int sq2 = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            append_to_heap(heap, make_move(sq, sq2, 0));
        }
    }

    /*
     * last find pawn checks
     */

    tmp = (p->turn == White) ? BPawnEPM[kp] : WPawnEPM[kp];
    tmp &= ~(p->mask[White][0] | p->mask[Black][0]);

    while (tmp) {
        int sq = (tmp).FindSetBit();
        tmp.ClearLowestBit();

        if (p->turn == White) {
            if (p->piece[sq - 8] == Pawn) {
                append_to_heap(heap, make_move(sq - 8, sq, 0));
            }
        } else {
            if (p->piece[sq + 8] == -Pawn) {
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
    struct GameLog *gl;

    if (p->ply == 0)
        return 0;

    if (p->actLog->gl_IrrevCount >= 100)
        return 3;

    gl = p->actLog - 1;
    for (i = p->actLog->gl_IrrevCount; i > 0; i--, gl--) {
        if (gl->gl_HashKey == p->hkey) {
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
    int8_t tp = TYPE(p->piece[fr]);

    if (tp == Pawn) {
        if (move.IsCapture() || move.IsEnPassant()) {
            *(x++) = 'a' + frCoord.File;
            *(x++) = 'x';
        }
        *(x++) = 'a' + toCoord.File;
        *(x++) = '1' + toCoord.Rank;

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
        int i;

        tmp = p->atkFr[to] & p->mask[p->turn][tp];

        /* check for ambigous move */
        while (tmp) {
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            if (i != fr) {
                int incheck;
                CMove tmove(CSCoord(i), move.GetToCoord(), 0);

                /* seems there is another piece of the same type which
                 * can move to the same destination square.
                 * Let's see wether it is pinned...
                 */

                if (p->piece[to])
                    tmove.SetCapture();

                p->DoMove(tmove);
                incheck = p->InCheck(OPP(p->turn));
                p->UndoMove(tmove);

                if (incheck)
                    continue;

                aamb = true;
                if (CSCoord(i).File == frCoord.File)
                    famb = true;
                if (CSCoord(i).Rank == frCoord.Rank)
                    ramb = true;
            }
        }

        *(x++) = PieceName[tp];
        if (aamb) {
            if (!famb)
                *(x++) = 'a' + frCoord.File;
            else {
                if (!ramb)
                    *(x++) = '1' + frCoord.Rank;
                else {
                    *(x++) = 'a' + frCoord.File;
                    *(x++) = '1' + frCoord.Rank;
                }
            }
        }

        if (move.IsCapture() || move.IsEnPassant())
            *(x++) = 'x';

        *(x++) = 'a' + toCoord.File;
        *(x++) = '1' + toCoord.Rank;
    }

    p->DoMove(move);
    if (p->InCheck(p->turn)) {
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

    *(x++) = 'a' + frCoord.File;
    *(x++) = '1' + frCoord.Rank;
    if (move.IsCapture() || move.IsEnPassant()) {
        *(x++) = 'x';
    }
    *(x++) = 'a' + toCoord.File;
    *(x++) = '1' + toCoord.Rank;
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
        CMove move(CSCoord(p->turn == White ? e1 : e8),
                   CSCoord(p->turn == White ? c1 : c8), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        CMove move(CSCoord(p->turn == White ? e1 : e8),
                   CSCoord(p->turn == White ? g1 : g8), M_SCASTLE);
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
    tmp = p->InCheck(OPP(p->turn));
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
        move = CMove(CSCoord(p->turn == White ? e1 : e8),
                     CSCoord(p->turn == White ? c1 : c8), M_LCASTLE);
        if (p->MayCastle(move))
            return move;
        else
            return M_NONE;
    }

    if (!strncmp(san, "O-O", 3) || !strncmp(san, "o-o", 3) ||
        !strncmp(san, "0-0", 3)) {
        move = CMove(CSCoord(p->turn == White ? e1 : e8),
                     CSCoord(p->turn == White ? g1 : g8), M_SCASTLE);
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

            if (TYPE(p->piece[fr]) == Pawn &&
                (move.IsCapture() || move.IsEnPassant()) && frCoord.File == ffl &&
                toCoord.File == tfl && TryMove(p, move))
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

        if (TYPE(p->piece[fr]) != tp)
            continue;
        if (toCoord.File != tfl || toCoord.Rank != trk)
            continue;
        if (ffl != -1 && frCoord.File != ffl)
            continue;
        if (frk != -1 && frCoord.Rank != frk)
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
                frCoord.File == ffl && toCoord.File == tfl)
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
        if (toCoord.File != tfl || toCoord.Rank != trk)
            continue;
        if (ffl != -1 && frCoord.File != ffl)
            continue;
        if (frk != -1 && frCoord.Rank != frk)
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

    tmp = p->mask[OPP(p->turn)][0];
    while (tmp) {
        int j = (tmp).FindSetBit();
        tmp.ClearLowestBit();

        p->GenTo(j, heap);
    }

    tmp = p->mask[p->turn][0];
    while (tmp) {
        int j = (tmp).FindSetBit();
        tmp.ClearLowestBit();

        p->GenFrom(j, heap);
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

    tmp = p->mask[OPP(p->turn)][0];
    while (tmp) {
        int j = (tmp).FindSetBit();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(tmp_heap);
        p->GenTo(j, tmp_heap);

        for (i = tmp_heap->current_section->start;
             i < tmp_heap->current_section->end; i++) {
            CMove move = tmp_heap->data[i];
            p->DoMove(move);
            if (!p->InCheck(OPP(p->turn))) {
                append_to_heap(heap, move);
            }
            p->UndoMove(move);
        }

        pop_section(tmp_heap);
    }

    tmp = p->mask[p->turn][0];
    while (tmp) {
        int j = (tmp).FindSetBit();
        unsigned int i;
        tmp.ClearLowestBit();

        push_section(tmp_heap);
        p->GenFrom(j, tmp_heap);

        for (i = tmp_heap->current_section->start;
             i < tmp_heap->current_section->end; i++) {
            CMove move = tmp_heap->data[i];
            if ((move.IsCastle()) && !p->MayCastle(move))
                continue;

            p->DoMove(move);
            if (!p->InCheck(OPP(p->turn))) {
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
            if (!p->InCheck(OPP(p->turn))) {
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
                ((rk == width - 1) && (p->turn)) || ((rk == 0) && (!p->turn)) ? '>' : ' ';
            Print(0, "    %c %c ", indicator, '1' + rk);
            for (int fl = 0; fl < width; fl++) {
                const int square = static_cast<int>(CSCoord(level, fl, rk));

                Print(0, "|");
                if (p->enPassant.IsValid() && square == p->enPassant.BitOffset())
                    Print(0, "<E>");
                else {
                    if (p->piece[square] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                    Print(0, "%c", PieceName[TYPE(p->piece[square])]);
                    if (p->piece[square] < 0)
                        Print(0, "*");
                    else
                        Print(0, " ");
                }
            }
            if (rk == 4) {
                int bit;
                Print(0, "|   Black (%5d, %5d)  ", p->material[Black],
                      p->nonPawn[Black]);
                for (bit = 0; bit < 5; bit++) {
                    Print(0, "%c",
                          (p->material_signature[Black] & (1 << bit))
                              ? PieceName[bit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (rk == 3) {
                int bit;
                Print(0, "|   White (%5d, %5d)  ", p->material[White],
                      p->nonPawn[White]);
                for (bit = 0; bit < 5; bit++) {
                    Print(0, "%c",
                          (p->material_signature[White] & (1 << bit))
                              ? PieceName[bit + 1]
                              : '.');
                }
                Print(0, "\n");
            } else if (rk == 6) {
                Print(0, "|   Hashkey: %llx\n", p->hkey);
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

    for (i = 0; i < 64; i++)
        p->piece[i] = Neutral;
    p->mask[White][0] = p->mask[Black][0] = 0;

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
            p->piece[fl + (rk << 3)] = Pawn;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'N':
            p->piece[fl + (rk << 3)] = Knight;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'B':
            p->piece[fl + (rk << 3)] = Bishop;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'R':
            p->piece[fl + (rk << 3)] = Rook;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'Q':
            p->piece[fl + (rk << 3)] = Queen;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'K':
            p->piece[fl + (rk << 3)] = King;
            p->mask[White][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'p':
            p->piece[fl + (rk << 3)] = -Pawn;
            p->mask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'n':
            p->piece[fl + (rk << 3)] = -Knight;
            p->mask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'b':
            p->piece[fl + (rk << 3)] = -Bishop;
            p->mask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'r':
            p->piece[fl + (rk << 3)] = -Rook;
            p->mask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'q':
            p->piece[fl + (rk << 3)] = -Queen;
            p->mask[Black][0].SetBit(fl + (rk << 3));
            fl++;
            break;
        case 'k':
            p->piece[fl + (rk << 3)] = -King;
            p->mask[Black][0].SetBit(fl + (rk << 3));
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

    /* scan p->turn to move */
    if (*x == 'w') {
        p->turn = White;
    } else {
        p->turn = Black;
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    /* scan castling status */
    p->castle = 0;
    if (*x != '-') {
        if (*x == 'K') {
            p->castle |= CastleMask[White][0];
            x++;
        }
        if (*x == 'Q') {
            p->castle |= CastleMask[White][1];
            x++;
        }
        if (*x == 'k') {
            p->castle |= CastleMask[Black][0];
            x++;
        }
        if (*x == 'q') {
            p->castle |= CastleMask[Black][1];
            x++;
        }
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    /* scan enpassant status */
    p->enPassant = InvalidSquareCoord();
    if (*x != '-') {
        p->enPassant = CSCoord(0, *x - 'a', *(x + 1) - '1');
        x++;
    }

    /* skip white space */
    while (*(++x) == ' ')
        ;

    p->RecalcAttacks();
    p->ply = 0;

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
                if (p->piece[square] == Neutral) {
                    cnt++;
                    if (j == width - 1)
                        *(x++) = '0' + cnt;
                } else {
                    if (cnt)
                        *(x++) = '0' + cnt;
                    cnt = 0;
                    if (p->piece[square] > 0)
                        *(x++) = wname[TYPE(p->piece[square])];
                    else
                        *(x++) = bname[TYPE(p->piece[square])];
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
    if (p->turn == White)
        *(x++) = 'w';
    else
        *(x++) = 'b';
    *(x++) = ' ';

    if (p->castle & CastleMask[White][0])
        *(x++) = 'K';
    if (p->castle & CastleMask[White][1])
        *(x++) = 'Q';
    if (p->castle & CastleMask[Black][0])
        *(x++) = 'k';
    if (p->castle & CastleMask[Black][1])
        *(x++) = 'q';
    if (!p->castle)
        *(x++) = '-';
    *(x++) = ' ';

    if (p->enPassant.IsValid()) {
        *(x++) = 'a' + p->enPassant.File;
        *(x++) = '1' + p->enPassant.Rank;
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
    if (p->actLog->gl_IrrevCount >= 100) {
        return "1/2-1/2 {50 move rule}";
    }

    if (p->Repeated(true) >= 2) {
        return "1/2-1/2 {Draw by repetition}";
    }

    if (p->material[White] == 0 && p->material[Black] == 0) {
        return "1/2-1/2 {Insufficient material}";
    }

    if (!p->LegalMoves(NULL)) {
        if (p->InCheck(p->turn)) {
            if (p->turn == Black) {
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
    return (p->mask[side][Bishop] != 0ULL) &&
           ((p->mask[side][Knight] | p->mask[side][Rook] |
             p->mask[side][Queen]) == 0ULL);
}
/*
 * Check if this is a theoretical draw
 */
bool CPosition::CheckDraw() const {
    const CPosition *p = this;
    if (p->material[Black] == 0) {
        if (p->nonPawn[White] == 0) {
            if (!(p->mask[White][Pawn] & NotAFileMask)) {
                if (p->mask[Black][King] & CornerMaskA8)
                    return true;
            }
            if (!(p->mask[White][Pawn] & NotHFileMask)) {
                if (p->mask[Black][King] & CornerMaskH8)
                    return true;
            }
        } else if (has_only_bishops(p, White)) {
            if (!(p->mask[White][Pawn] & NotAFileMask) &&
                (p->mask[Black][King] & CornerMaskA8)) {
                if (p->mask[White][Bishop] & BlackSquaresMask)
                    return true;
            }
            if (!(p->mask[White][Pawn] & NotHFileMask) &&
                (p->mask[Black][King] & CornerMaskH8)) {
                if (p->mask[White][Bishop] & WhiteSquaresMask)
                    return true;
            }
        }
    }
    if (p->material[White] == 0) {
        if (p->nonPawn[Black] == 0) {
            if (!(p->mask[Black][Pawn] & NotAFileMask)) {
                if (p->mask[White][King] & CornerMaskA1)
                    return true;
            }
            if (!(p->mask[Black][Pawn] & NotHFileMask)) {
                if (p->mask[White][King] & CornerMaskH1)
                    return true;
            }
        } else if (has_only_bishops(p, Black)) {
            if (!(p->mask[Black][Pawn] & NotAFileMask) &&
                (p->mask[White][King] & CornerMaskA1)) {
                if (p->mask[Black][Bishop] & WhiteSquaresMask)
                    return true;
            }
            if (!(p->mask[Black][Pawn] & NotHFileMask) &&
                (p->mask[White][King] & CornerMaskH1)) {
                if (p->mask[Black][Bishop] & BlackSquaresMask)
                    return true;
            }
        }
    }
    return false;
}

/*
 * Check if the pawn is passed
 */

bool IsPassed(const CPosition *p, int sq, int side) {
    if (side == White)
        return !(p->mask[Black][Pawn] & PassedMaskW[sq]);
    else
        return !(p->mask[White][Pawn] & PassedMaskB[sq]);
}

/**
 * Create a position from an EPD
 */

CPosition *CPosition::CreateFromEPD(const char *epd) {
    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    p->gameLogSize = INITIAL_GAME_LOG_SIZE;
    p->gameLog = (GameLog *)safe_calloc(p->gameLogSize, sizeof(GameLog));
    p->actLog = p->gameLog;
    ReadEPD(p, epd);
    p->actLog->gl_IrrevCount = 0;

    /* default for book usage is no book */
    p->outOfBookCnt[White] = p->outOfBookCnt[Black] = 3;

    return p;
}

/**
 * Create a position in the usual starting position
 */

CPosition *CPosition::Initial() {
    CPosition *p = CPosition::CreateFromEPD(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -");

    /* we are 'in book' in the InitalPosition */
    p->outOfBookCnt[White] = p->outOfBookCnt[Black] = 0;

    return p;
}

CPosition *CPosition::Clone(const CPosition *src) {
    CPosition *p = (CPosition *)safe_calloc(1, sizeof(CPosition));
    memcpy(p, src, sizeof(CPosition));

    p->gameLogSize = src->gameLogSize;
    p->gameLog = (GameLog *)safe_calloc(p->gameLogSize, sizeof(GameLog));
    memcpy(p->gameLog, src->gameLog, sizeof(GameLog) * p->gameLogSize);

    p->actLog = p->gameLog + (src->actLog - src->gameLog);

    return p;
}

/**
 * Release the resources connected with a Position
 */

void CPosition::Free(CPosition *p) {
    if (p) {
        free(p->gameLog);
        free(p);
    }
}
