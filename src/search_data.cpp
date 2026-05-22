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
 * next.c - move selection routines
 */

#include "amy.h"

#include "search.h"
#include "dbase.h"
#include "evaluation.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "safe_malloc.h"
#include "swap.h"
#include "utils.h"
#include <string.h>
#if MP
#include "hashtable.h"
#endif

/**
 * Description: Creates and initializes per-search state for move ordering and node bookkeeping.
 * Inputs: pPosition - position used by this search context.
 * Outputs: Initializes all members and allocates internal heaps/tables.
 */
CSearchData::CSearchData(CPosition *p) {
    CSearchData *sd = this;
    memset(sd, 0, sizeof(*sd));

    sd->m_pPosition = p;
    sd->m_pStatusTable =
        (struct SSearchStatus *)safe_calloc(MAX_TREE_SIZE,
                                           sizeof(struct SSearchStatus));
    sd->m_pCurrent = sd->m_pStatusTable;
    sd->m_pKillerTable =
        (struct SKillerEntry *)safe_calloc(MAX_TREE_SIZE,
                                          sizeof(struct SKillerEntry));
    sd->m_pKiller = sd->m_pKillerTable;

    sd->m_hHeap = allocate_heap();

    sd->m_pnDataHeap = NULL;
    sd->m_uDataHeapSize = 0;

#if MP
    sd->m_pLocalHashTable =
        (struct HTEntry *)safe_calloc(sizeof(struct HTEntry), L_HT_Size);
    sd->m_hDeferredHeap = allocate_heap();
#endif

    sd->m_wPly = 0;

}

/**
 * Description: Releases all heap/table resources owned by this search context.
 * Inputs: None.
 * Outputs: Frees all dynamically allocated members.
 */
CSearchData::~CSearchData() {
    CSearchData *sd = this;
    free(sd->m_pStatusTable);
    free(sd->m_pKillerTable);
    free(sd->m_pnDataHeap);
    free_heap(sd->m_hHeap);

#if MP
    free(sd->m_pLocalHashTable);
    free_heap(sd->m_hDeferredHeap);
#endif

}

/**
 * Description: Enters one search ply and initializes phase state for move generation at that ply.
 * Inputs: None.
 * Outputs: Increments ply state and pushes heap sections.
 */
void CSearchData::EnterNode() {
    CSearchData *sd = this;
    struct SSearchStatus *st;

    st = ++(sd->m_pCurrent);

    st->st_phase = HashMove;
    sd->m_wPly++;
    sd->m_pKiller++;

    push_section(sd->m_hHeap);
#if MP
    push_section(sd->m_hDeferredHeap);
#endif
}

/**
 * Description: Leaves one search ply and restores parent search state.
 * Inputs: None.
 * Outputs: Pops heap sections and decrements ply state.
 */
void CSearchData::LeaveNode() {
    CSearchData *sd = this;
    pop_section(sd->m_hHeap);
    sd->m_pCurrent--;
    sd->m_pKiller--;
    sd->m_wPly--;
#if MP
    pop_section(sd->m_hDeferredHeap);
#endif
}

static inline void GrowDataHeap(CSearchData *sd) {
    if (sd->m_hHeap->current_section->end > sd->m_uDataHeapSize) {
        sd->m_uDataHeapSize = sd->m_hHeap->current_section->end + 256;
        sd->m_pnDataHeap = (int32_t *)realloc(
            sd->m_pnDataHeap, sd->m_uDataHeapSize * sizeof(int32_t));
        if (sd->m_pnDataHeap == NULL) {
            perror("Cannot grow data_heap");
            exit(1);
        }
    }
}

/**
 * Description: Produces the next legal move from the normal move generator in ordering sequence.
 * Inputs: None.
 * Outputs: Returns next move or M_NONE when exhausted.
 */
CMove CSearchData::NextMove() {
    CSearchData *sd = this;
    heap_section_t section = sd->m_hHeap->current_section;
    struct SSearchStatus *st = sd->m_pCurrent;
    CPosition *p = sd->m_pPosition;
    CMove move;

    switch (st->st_phase) {
    case HashMove:
#ifdef VERBOSE
        Print(9, "HashMove\n");
#endif
        if (p->LegalMove(st->st_hashmove)) {
            st->st_phase = GenerateCaptures;
            return st->st_hashmove;
        } else {
            st->st_hashmove = M_NONE;
        }
    /* fall through */
    case GenerateCaptures: {
#ifdef VERBOSE
        Print(9, "GenerateCaptures\n");
#endif

        /*
         * Generate captures.
         */
        CBitBoard targets = p->m_rgMask[OPP(p->m_nTurn)][0];
        while (targets) {
            CSCoord to = (targets).FindSetBitCoord();
            targets.ClearLowestBit();

            p->GenTo(to, sd->m_hHeap);
        }

        CBitBoard promoting_pawns =
            p->m_rgMask[p->m_nTurn][Pawn] & SeventhRank[p->m_nTurn];
        while (promoting_pawns) {
            CSCoord from = (promoting_pawns).FindSetBitCoord();
            promoting_pawns.ClearLowestBit();

            p->GenFrom(from, sd->m_hHeap);
        }

        GrowDataHeap(sd);
        for (unsigned int j = section->start; j < section->end; j++) {
            sd->m_pnDataHeap[j] = SwapOff(p, sd->m_hHeap->data[j]);
        }

        unsigned int last_end = section->end;
        p->GenEnpas(sd->m_hHeap);
        GrowDataHeap(sd);

        for (unsigned int j = last_end; j < section->end; j++) {
            sd->m_pnDataHeap[j] = 0;
        }

        st->st_phase = GainingCapture;
    }
    /* fall through */
    case GainingCapture:
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->m_pnDataHeap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->m_pnDataHeap[i] > best) {
                    best = sd->m_pnDataHeap[i];
                    besti = i;
                }
            }
            if (best >= 0) {
                move = sd->m_hHeap->data[besti];
                section->end--;

                sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];
                sd->m_pnDataHeap[besti] = sd->m_pnDataHeap[section->end];

                if (move == st->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
    /* fall through */
    case Killer1: {
        move = sd->m_pKiller->killer1;
#ifdef VERBOSE
        Print(9, "Killer1\n");
#endif
        st->st_k1 = M_NONE;
        if (move != st->st_hashmove && p->LegalMove(move)) {
            st->st_phase = Killer2;
            st->st_k1 = move;

            return move;
        }
    }
    /* fall through */
    case Killer2: {
        move = sd->m_pKiller->killer2;
#ifdef VERBOSE
        Print(9, "Killer2\n");
#endif
        st->st_k2 = M_NONE;
        if (move != st->st_hashmove && p->LegalMove(move)) {
            st->st_phase = CounterMv;
            st->st_k2 = move;

            return move;
        }
    }
    /* fall through */
    case CounterMv: {
        CMove lmove = (p->m_pActLog - 1)->gl_Move;

#ifdef VERBOSE
        Print(9, "CounterMv\n");
#endif
        st->st_cm = M_NONE;
        if (lmove != M_NULL) {
            move = sd->m_rgCounterTab[p->m_nTurn][lmove.GetFromToIndex()];

            if (move != M_NONE && move != st->st_hashmove &&
                move != st->st_k1 && move != st->st_k2 && p->LegalMove(move)) {
                st->st_phase = Killer3;
                st->st_cm = move;

                return move;
            }
        }
    }
    /* fallthrough */
    case Killer3:
#ifdef VERBOSE
        Print(9, "Killer3\n");
#endif
        st->st_k3 = M_NONE;
        if (sd->m_wPly >= 2) {
            move = (sd->m_pKiller - 2)->killer1;

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_cm || !p->LegalMove(move))
                move = (sd->m_pKiller - 2)->killer2;

            if (move != st->st_hashmove && move != st->st_k1 &&
                move != st->st_k2 && move != st->st_cm && p->LegalMove(move)) {
                st->st_phase = LoosingCapture;
                st->st_k3 = move;

                return move;
            }
        }
        /* fallthrough */

    case LoosingCapture:
#ifdef VERBOSE
        Print(9, "LoosingCapture\n");
#endif
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->m_pnDataHeap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->m_pnDataHeap[i] > best) {
                    best = sd->m_pnDataHeap[i];
                    besti = i;
                }
            }

            move = sd->m_hHeap->data[besti];
            section->end--;

            sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];
            sd->m_pnDataHeap[besti] = sd->m_pnDataHeap[section->end];

            st->st_phase = LoosingCapture;

            if (move == st->st_hashmove)
                continue;

            return move;
        }
        /* fallthrough */

    case GenerateRest: {
#ifdef VERBOSE
        Print(9, "GenerateRest\n");
#endif
        const CBitBoard empty = ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

        if (p->m_bCastle & CastleMask[p->m_nTurn][0]) {
            append_to_heap(sd->m_hHeap,
                           make_move(p->m_nTurn == White ? CASTLE_E1 : CASTLE_E8,
                                     p->m_nTurn == White ? CASTLE_G1 : CASTLE_G8, M_SCASTLE));
        }
        if (p->m_bCastle & CastleMask[p->m_nTurn][1]) {
            append_to_heap(sd->m_hHeap,
                           make_move(p->m_nTurn == White ? CASTLE_E1 : CASTLE_E8,
                                     p->m_nTurn == White ? CASTLE_C1 : CASTLE_C8, M_LCASTLE));
        }

        CBitBoard non_pawn = p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][Pawn];

        while (non_pawn) {
            int from = (non_pawn).FindSetBit();
            non_pawn.ClearLowestBit();
            CBitBoard attacks = p->m_rgAtkTo[from] & empty;
            while (attacks) {
                int to = (attacks).FindSetBit();
                attacks.ClearLowestBit();
                append_to_heap(sd->m_hHeap, make_move(from, to, 0));
            }
        }

        CBitBoard tmp = p->m_rgMask[p->m_nTurn][Pawn] & ~SeventhRank[p->m_nTurn];

        if (p->m_nTurn == White)
            tmp = ShiftUp(tmp);
        else
            tmp = ShiftDown(tmp);

        CBitBoard tmp2 = tmp &= empty;

        while (tmp2) {
            int to = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();

            int fr = (p->m_nTurn == White) ? to - 8 : to + 8;
            append_to_heap(sd->m_hHeap, make_move(fr, to, 0));
        }

        tmp &= ThirdRank[p->m_nTurn];

        if (p->m_nTurn == White)
            tmp = ShiftUp(tmp);
        else
            tmp = ShiftDown(tmp);

        tmp &= empty;

        while (tmp) {
            int to = (tmp).FindSetBit();
            tmp.ClearLowestBit();

            int fr = (p->m_nTurn == White) ? to - 16 : to + 16;
            append_to_heap(sd->m_hHeap, make_move(fr, to, M_PAWND));
        }

        st->st_phase = HistoryMoves;
    }

    case HistoryMoves:
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif
        while (section->end > section->start) {
            int besti = section->start;
            int best =
                sd->m_rguHistoryTab[p->m_nTurn][sd->m_hHeap->data[besti].GetFromToIndex()];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                int hval = sd->m_rguHistoryTab[p->m_nTurn]
                                        [sd->m_hHeap->data[i].GetFromToIndex()];
                if (hval > best) {
                    best = hval;
                    besti = i;
                }
            }
            move = sd->m_hHeap->data[besti];

            section->end--;
            sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_k3 || move == st->st_cm)
                continue;

            return move;
        }

    default:
        break;
    }

    return M_NONE;
}

/**
 * Description: Produces the next legal move from the in-check evasion generator in ordering sequence.
 * Inputs: None.
 * Outputs: Returns next evasion move or M_NONE when exhausted.
 */
CMove CSearchData::NextEvasion() {
    CSearchData *sd = this;
    heap_section_t section = sd->m_hHeap->current_section;
    struct SSearchStatus *st = sd->m_pCurrent;
    CPosition *p = sd->m_pPosition;
    CMove move;

    switch (st->st_phase) {
    case HashMove:
#ifdef VERBOSE
        Print(9, "HashMove\n");
#endif
        if (p->LegalMove(st->st_hashmove)) {
            st->st_phase = GenerateCaptures;
            return st->st_hashmove;
        } else {
            st->st_hashmove = M_NONE;
        }
        /* fall through */
    case GenerateCaptures: {
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif

        /*
         * Generate captures. If in check, generate only
         * captures by the king or to pieces which give
         * check
         */

        int kp = p->m_rgKingSq[p->m_nTurn].BitOffset();

        CBitBoard targets =
            (p->m_rgAtkFr[kp] | p->m_rgAtkTo[kp]) & p->m_rgMask[OPP(p->m_nTurn)][0];

        while (targets) {
            CSCoord to = (targets).FindSetBitCoord();
            targets.ClearLowestBit();
            p->GenTo(to, sd->m_hHeap);
        }

        GrowDataHeap(sd);
        for (unsigned int j = section->start; j < section->end; j++) {
            sd->m_pnDataHeap[j] = SwapOff(p, sd->m_hHeap->data[j]);
        }

        unsigned int last_end = section->end;
        p->GenEnpas(sd->m_hHeap);
        GrowDataHeap(sd);

        for (unsigned int j = last_end; j < section->end; j++) {
            sd->m_pnDataHeap[j] = 0;
        }
    }
        /* fall through */
    case GainingCapture:
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->m_pnDataHeap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->m_pnDataHeap[i] > best) {
                    best = sd->m_pnDataHeap[i];
                    besti = i;
                }
            }
            if (best >= 0) {
                move = sd->m_hHeap->data[besti];
                section->end--;

                sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];
                sd->m_pnDataHeap[besti] = sd->m_pnDataHeap[section->end];

                st->st_phase = GainingCapture;

                if (move == st->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
        /* fall through */
    case Killer1: {
        move = sd->m_pKiller->killer1;
#ifdef VERBOSE
        Print(9, "Killer1\n");
#endif
        st->st_k1 = M_NONE;
        if (move != st->st_hashmove && p->LegalMove(move)) {
            st->st_phase = Killer2;
            st->st_k1 = move;

            return move;
        }
    }
        /* fall through */
    case Killer2: {
        move = sd->m_pKiller->killer2;
#ifdef VERBOSE
        Print(9, "Killer2\n");
#endif
        st->st_k2 = M_NONE;
        if (move != st->st_hashmove && p->LegalMove(move)) {
            st->st_phase = CounterMv;
            st->st_k2 = move;

            return move;
        }
    }
        /* fall through */
    case CounterMv: {
        CMove lmove = (p->m_pActLog - 1)->gl_Move;

#ifdef VERBOSE
        Print(9, "CounterMv\n");
#endif
        st->st_cm = M_NONE;
        if (lmove != M_NULL) {
            move = sd->m_rgCounterTab[p->m_nTurn][lmove.GetFromToIndex()];

            if (move != M_NONE && move != st->st_hashmove &&
                move != st->st_k1 && move != st->st_k2 && p->LegalMove(move)) {
                st->st_phase = Killer3;
                st->st_cm = move;

                return move;
            }
        }
    }
        /* fall through */
    case Killer3:
#ifdef VERBOSE
        Print(9, "Killer3\n");
#endif
        st->st_k3 = M_NONE;
        if (sd->m_wPly >= 2) {
            move = (sd->m_pKiller - 2)->killer1;

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_cm || !p->LegalMove(move))
                move = (sd->m_pKiller - 2)->killer2;

            if (move != st->st_hashmove && move != st->st_k1 &&
                move != st->st_k2 && move != st->st_cm && p->LegalMove(move)) {
                st->st_phase = /* HistoryMoves; */ LoosingCapture;
                st->st_k3 = move;

                return move;
            }
        }
        /* fall through */
    case LoosingCapture:
#ifdef VERBOSE
        Print(9, "LoosingCapture\n");
#endif
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->m_pnDataHeap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->m_pnDataHeap[i] > best) {
                    best = sd->m_pnDataHeap[i];
                    besti = i;
                }
            }
            move = sd->m_hHeap->data[besti];

            section->end--;
            sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];
            sd->m_pnDataHeap[besti] = sd->m_pnDataHeap[section->end];

            st->st_phase = LoosingCapture;

            if (move == st->st_hashmove)
                continue;

            return move;
        }

        /* fall through */
    case GenerateRest: {
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif

        const int kp =
            p->m_rgKingSq[p->m_nTurn].BitOffset(); /* (Mask[Side][King]).FindSetBit(); */
        const CBitBoard empty = ~(p->m_rgMask[White][0] | p->m_rgMask[Black][0]);

        CBitBoard king_flight_squares = p->m_rgAtkTo[kp] & empty;

        while (king_flight_squares) {
            int to = (king_flight_squares).FindSetBit();
            king_flight_squares.ClearLowestBit();
            if (!(p->m_rgAtkFr[to] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                append_to_heap(sd->m_hHeap, make_move(kp, to, 0));
        }

        CBitBoard sliding_attackers =
            (p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Rook] |
             p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
            p->m_rgAtkFr[kp];

        CBitBoard interpositions;

        while (sliding_attackers) {
            int attacker_sq = (sliding_attackers).FindSetBit();
            sliding_attackers.ClearLowestBit();
            interpositions = InterPath[kp][attacker_sq];
        }

        CBitBoard non_pawns = (p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][King]) &
                             ~p->m_rgMask[p->m_nTurn][Pawn];

        while (non_pawns) {
            int from = (non_pawns).FindSetBit();
            non_pawns.ClearLowestBit();
            CBitBoard blocking = p->m_rgAtkTo[from] & empty & interpositions;

            while (blocking) {
                int to = (blocking).FindSetBit();
                blocking.ClearLowestBit();
                append_to_heap(sd->m_hHeap, make_move(from, to, 0));
            }
        }

        CBitBoard pawns = p->m_rgMask[p->m_nTurn][Pawn];

        if (p->m_nTurn == White)
            pawns = ShiftUp(pawns);
        else
            pawns = ShiftDown(pawns);

        CBitBoard pawns_to = pawns = (pawns & empty);

        while (pawns_to) {
            CSCoord toCoord = (pawns_to).FindSetBitCoord();
            const uint16_t to = toCoord.BitOffset();
            pawns_to.ClearLowestBit();
            int fr = (p->m_nTurn == White) ? to - 8 : to + 8;

            if (is_promo_square(toCoord)) {
                append_to_heap(sd->m_hHeap, make_promotion(fr, to, Queen, 0));
                append_to_heap(sd->m_hHeap, make_promotion(fr, to, Knight, 0));
                append_to_heap(sd->m_hHeap, make_promotion(fr, to, Rook, 0));
                append_to_heap(sd->m_hHeap, make_promotion(fr, to, Bishop, 0));
            } else
                append_to_heap(sd->m_hHeap, make_move(fr, to, 0));
        }

        pawns &= ThirdRank[p->m_nTurn];

        if (p->m_nTurn == White)
            pawns = ShiftUp(pawns);
        else
            pawns = ShiftDown(pawns);

        pawns &= empty;

        while (pawns) {
            int to = (pawns).FindSetBit();
            pawns.ClearLowestBit();
            int fr = (p->m_nTurn == White) ? to - 16 : to + 16;
            append_to_heap(sd->m_hHeap, make_move(fr, to, M_PAWND));
        }

        st->st_phase = HistoryMoves;
    }

        /* fall through */
    case HistoryMoves:
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best =
                sd->m_rguHistoryTab[p->m_nTurn][sd->m_hHeap->data[besti].GetFromToIndex()];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                int hval = sd->m_rguHistoryTab[p->m_nTurn]
                                        [sd->m_hHeap->data[i].GetFromToIndex()];
                if (hval > best) {
                    best = hval;
                    besti = i;
                }
            }
            move = sd->m_hHeap->data[besti];

            section->end--;
            sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_k3 || move == st->st_cm)
                continue;

            return move;
        }
    default:
        break;
    }

    return M_NONE;
}

static void GenerateQCaptures(CSearchData *sd, int alpha) {
    heap_section_t section = sd->m_hHeap->current_section;
    CPosition *p = sd->m_pPosition;
    CBitBoard pwn7th;
    CBitBoard att, def;
    int score;
    int i;

    att = p->m_rgMask[p->m_nTurn][0];

    /* Handle pawn promotions first */
    pwn7th = p->m_rgMask[p->m_nTurn][Pawn] & SeventhRank[p->m_nTurn];
    att &= ~pwn7th;

    while (pwn7th) {
        int next;
        int j;
        CBitBoard tmp;

        i = (pwn7th).FindSetBit();
        pwn7th.ClearLowestBit();
        next = (p->m_nTurn == White) ? i + 8 : i - 8;

        if (p->m_rgPiece[next] == Neutral) {
            CMove move = make_promotion(i, next, Queen, 0);
            int sw;
            if ((sw = SwapOff(p, move)) >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }

        tmp = p->m_rgAtkTo[i] & p->m_rgMask[OPP(p->m_nTurn)][0];
        while (tmp) {
            int sw;
            j = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            CMove move = make_promotion(i, j, Queen, M_CAPTURE);
            if ((sw = SwapOff(p, move)) >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }
    }

    if (p->m_nTurn == White) {
        score = MaterialBalance(p) + MaxPos;
    } else {
        score = -MaterialBalance(p) + MaxPos;
    }

    if (score + Value[Queen] <= alpha)
        return;
    def = p->m_rgMask[OPP(p->m_nTurn)][Queen];
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->m_rgAtkFr[i] & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            CMove move = make_move(j, i, M_CAPTURE);
            int sw = SwapOff(p, move);
            if (sw >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }
    }
    if (score + Value[Rook] <= alpha)
        return;
    def = p->m_rgMask[OPP(p->m_nTurn)][Rook];
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->m_rgAtkFr[i] & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            CMove move = make_move(j, i, M_CAPTURE);
            int sw = SwapOff(p, move);
            if (sw >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }
    }
    if (score + Value[Bishop] <= alpha)
        return;
    def = p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Knight];
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->m_rgAtkFr[i] & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            CMove move = make_move(j, i, M_CAPTURE);
            int sw = SwapOff(p, move);
            if (sw >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }
    }
    if (score + Value[Pawn] <= alpha)
        return;
    def = p->m_rgMask[OPP(p->m_nTurn)][Pawn];
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->m_rgAtkFr[i] & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            CMove move = make_move(j, i, M_CAPTURE);
            int sw = SwapOff(p, move);
            if (sw >= 0) {
                append_to_heap(sd->m_hHeap, move);
                GrowDataHeap(sd);
                sd->m_pnDataHeap[section->end - 1] = sw;
            }
        }
    }
}

/**
 * Description: Produces the next tactical move for quiescence search.
 * Inputs: nAlpha - current alpha bound used for pruning tactical generation.
 * Outputs: Returns next quiescence move or M_NONE when exhausted.
 */
CMove CSearchData::NextMoveQ(int alpha) {
    CSearchData *sd = this;
    heap_section_t section = sd->m_hHeap->current_section;
    struct SSearchStatus *st = sd->m_pCurrent;
    CMove move;

    switch (st->st_phase) {
    case HashMove:
    case GenerateCaptures:
#ifdef VERBOSE
        Print(9, "GenerateCaptures\n");
#endif
        GenerateQCaptures(sd, alpha);
        st->st_phase = GainingCapture;

        /* fall through */
    case GainingCapture:
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->m_pnDataHeap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->m_pnDataHeap[i] > best) {
                    best = sd->m_pnDataHeap[i];
                    besti = i;
                }
            }

            move = sd->m_hHeap->data[besti];
            section->end--;
            sd->m_hHeap->data[besti] = sd->m_hHeap->data[section->end];
            sd->m_pnDataHeap[besti] = sd->m_pnDataHeap[section->end];

            return move;
        }
    default:
        break;
    }

    return M_NONE;
}

/*
 * Enter move in Killertable
 */

/**
 * Description: Updates killer move tables for the current ply using a newly found cutoff move.
 * Inputs: mvMove - candidate killer move.
 * Outputs: Updates killer entries and usage counters.
 */
void CSearchData::PutKiller(CMove m) {
    CSearchData *sd = this;
    struct SKillerEntry *k = sd->m_pKiller;

    if (m == k->killer1) {
        k->kcount1 += 1;
    } else if (m == k->killer2) {
        k->kcount2 += 1;
        if (k->kcount2 > k->kcount1) {
            int tmpCount;
            CMove tmpMove;

            tmpCount = k->kcount1;
            k->kcount1 = k->kcount2;
            k->kcount2 = tmpCount;

            tmpMove = k->killer1;
            k->killer1 = k->killer2;
            k->killer2 = tmpMove;
        }
    } else {
        if (k->killer1 == M_NONE) {
            k->killer1 = m;
            k->kcount1 = 1;
        } else {
            k->killer2 = m;
            k->kcount2 = 1;
        }
    }
}
