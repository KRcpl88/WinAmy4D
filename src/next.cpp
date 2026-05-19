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

#include "next.h"
#include "dbase.h"
#include "evaluation.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "safe_malloc.h"
#include "search.h"
#include "swap.h"
#include "utils.h"
#if MP
#include "hashtable.h"
#endif

struct SearchData *CreateSearchData(CPosition *p) {
    struct SearchData *sd =
        (struct SearchData *)safe_calloc(1, sizeof(struct SearchData));

    sd->position = p;
    sd->statusTable =
        (struct SearchStatus *)safe_calloc(MAX_TREE_SIZE,
                                           sizeof(struct SearchStatus));
    sd->current = sd->statusTable;
    sd->killerTable =
        (struct KillerEntry *)safe_calloc(MAX_TREE_SIZE,
                                          sizeof(struct KillerEntry));
    sd->killer = sd->killerTable;

    sd->heap = allocate_heap();

    sd->data_heap = NULL;
    sd->data_heap_size = 0;

#if MP
    sd->localHashTable =
        (struct HTEntry *)safe_calloc(sizeof(struct HTEntry), L_HT_Size);
    sd->deferred_heap = allocate_heap();
#endif

    sd->ply = 0;

    return sd;
}

void FreeSearchData(struct SearchData *sd) {
    free(sd->statusTable);
    free(sd->killerTable);
    free(sd->data_heap);
    free_heap(sd->heap);

#if MP
    free(sd->localHashTable);
    free_heap(sd->deferred_heap);
#endif

    free(sd);
}

void EnterNode(struct SearchData *sd) {
    struct SearchStatus *st;

    st = ++(sd->current);

    st->st_phase = HashMove;
    sd->ply++;
    sd->killer++;

    push_section(sd->heap);
#if MP
    push_section(sd->deferred_heap);
#endif
}

void LeaveNode(struct SearchData *sd) {
    pop_section(sd->heap);
    sd->current--;
    sd->killer--;
    sd->ply--;
#if MP
    pop_section(sd->deferred_heap);
#endif
}

static inline void grow_data_heap(struct SearchData *sd) {
    if (sd->heap->current_section->end > sd->data_heap_size) {
        sd->data_heap_size = sd->heap->current_section->end + 256;
        sd->data_heap = (int32_t *)realloc(
            sd->data_heap, sd->data_heap_size * sizeof(int32_t));
        if (sd->data_heap == NULL) {
            perror("Cannot grow data_heap");
            exit(1);
        }
    }
}

CMove NextMove(struct SearchData *sd) {
    heap_section_t section = sd->heap->current_section;
    struct SearchStatus *st = sd->current;
    CPosition *p = sd->position;
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
            int to = (targets).FindSetBit();
            targets.ClearLowestBit();

            p->GenTo(CSCoord(to), sd->heap);
        }

        CBitBoard promoting_pawns =
            p->m_rgMask[p->m_nTurn][Pawn] & SeventhRank[p->m_nTurn];
        while (promoting_pawns) {
            int from = (promoting_pawns).FindSetBit();
            promoting_pawns.ClearLowestBit();

            p->GenFrom(CSCoord(from), sd->heap);
        }

        grow_data_heap(sd);
        for (unsigned int j = section->start; j < section->end; j++) {
            sd->data_heap[j] = SwapOff(p, sd->heap->data[j]);
        }

        unsigned int last_end = section->end;
        p->GenEnpas(sd->heap);
        grow_data_heap(sd);

        for (unsigned int j = last_end; j < section->end; j++) {
            sd->data_heap[j] = 0;
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
            int best = sd->data_heap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->data_heap[i] > best) {
                    best = sd->data_heap[i];
                    besti = i;
                }
            }
            if (best >= 0) {
                move = sd->heap->data[besti];
                section->end--;

                sd->heap->data[besti] = sd->heap->data[section->end];
                sd->data_heap[besti] = sd->data_heap[section->end];

                if (move == st->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
    /* fall through */
    case Killer1: {
        move = sd->killer->killer1;
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
        move = sd->killer->killer2;
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
            move = sd->counterTab[p->m_nTurn][lmove.GetFromToIndex()];

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
        if (sd->ply >= 2) {
            move = (sd->killer - 2)->killer1;

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_cm || !p->LegalMove(move))
                move = (sd->killer - 2)->killer2;

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
            int best = sd->data_heap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->data_heap[i] > best) {
                    best = sd->data_heap[i];
                    besti = i;
                }
            }

            move = sd->heap->data[besti];
            section->end--;

            sd->heap->data[besti] = sd->heap->data[section->end];
            sd->data_heap[besti] = sd->data_heap[section->end];

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
            append_to_heap(sd->heap,
                           make_move(p->m_nTurn == White ? e1 : e8,
                                     p->m_nTurn == White ? g1 : g8, M_SCASTLE));
        }
        if (p->m_bCastle & CastleMask[p->m_nTurn][1]) {
            append_to_heap(sd->heap,
                           make_move(p->m_nTurn == White ? e1 : e8,
                                     p->m_nTurn == White ? c1 : c8, M_LCASTLE));
        }

        CBitBoard non_pawn = p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][Pawn];

        while (non_pawn) {
            int from = (non_pawn).FindSetBit();
            non_pawn.ClearLowestBit();
            CBitBoard attacks = p->m_rgAtkTo[from] & empty;
            while (attacks) {
                int to = (attacks).FindSetBit();
                attacks.ClearLowestBit();
                append_to_heap(sd->heap, make_move(from, to, 0));
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
            append_to_heap(sd->heap, make_move(fr, to, 0));
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
            append_to_heap(sd->heap, make_move(fr, to, M_PAWND));
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
                sd->historyTab[p->m_nTurn][sd->heap->data[besti].GetFromToIndex()];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                int hval = sd->historyTab[p->m_nTurn]
                                        [sd->heap->data[i].GetFromToIndex()];
                if (hval > best) {
                    best = hval;
                    besti = i;
                }
            }
            move = sd->heap->data[besti];

            section->end--;
            sd->heap->data[besti] = sd->heap->data[section->end];

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

CMove NextEvasion(struct SearchData *sd) {
    heap_section_t section = sd->heap->current_section;
    struct SearchStatus *st = sd->current;
    CPosition *p = sd->position;
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
            int to = (targets).FindSetBit();
            targets.ClearLowestBit();
            p->GenTo(CSCoord(to), sd->heap);
        }

        grow_data_heap(sd);
        for (unsigned int j = section->start; j < section->end; j++) {
            sd->data_heap[j] = SwapOff(p, sd->heap->data[j]);
        }

        unsigned int last_end = section->end;
        p->GenEnpas(sd->heap);
        grow_data_heap(sd);

        for (unsigned int j = last_end; j < section->end; j++) {
            sd->data_heap[j] = 0;
        }
    }
        /* fall through */
    case GainingCapture:
        while (section->end > section->start) {
            unsigned int besti = section->start;
            int best = sd->data_heap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->data_heap[i] > best) {
                    best = sd->data_heap[i];
                    besti = i;
                }
            }
            if (best >= 0) {
                move = sd->heap->data[besti];
                section->end--;

                sd->heap->data[besti] = sd->heap->data[section->end];
                sd->data_heap[besti] = sd->data_heap[section->end];

                st->st_phase = GainingCapture;

                if (move == st->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
        /* fall through */
    case Killer1: {
        move = sd->killer->killer1;
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
        move = sd->killer->killer2;
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
            move = sd->counterTab[p->m_nTurn][lmove.GetFromToIndex()];

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
        if (sd->ply >= 2) {
            move = (sd->killer - 2)->killer1;

            if (move == st->st_hashmove || move == st->st_k1 ||
                move == st->st_k2 || move == st->st_cm || !p->LegalMove(move))
                move = (sd->killer - 2)->killer2;

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
            int best = sd->data_heap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->data_heap[i] > best) {
                    best = sd->data_heap[i];
                    besti = i;
                }
            }
            move = sd->heap->data[besti];

            section->end--;
            sd->heap->data[besti] = sd->heap->data[section->end];
            sd->data_heap[besti] = sd->data_heap[section->end];

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
                append_to_heap(sd->heap, make_move(kp, to, 0));
        }

        CBitBoard sliding_attackers =
            (p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Rook] |
             p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
            p->m_rgAtkFr[kp];

        CBitBoard interpositions = 0;

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
                append_to_heap(sd->heap, make_move(from, to, 0));
            }
        }

        CBitBoard pawns = p->m_rgMask[p->m_nTurn][Pawn];

        if (p->m_nTurn == White)
            pawns = ShiftUp(pawns);
        else
            pawns = ShiftDown(pawns);

        CBitBoard pawns_to = pawns = (pawns & empty);

        while (pawns_to) {
            int to = (pawns_to).FindSetBit();
            pawns_to.ClearLowestBit();
            int fr = (p->m_nTurn == White) ? to - 8 : to + 8;

            if (is_promo_square(CSCoord(to))) {
                append_to_heap(sd->heap, make_promotion(fr, to, Queen, 0));
                append_to_heap(sd->heap, make_promotion(fr, to, Knight, 0));
                append_to_heap(sd->heap, make_promotion(fr, to, Rook, 0));
                append_to_heap(sd->heap, make_promotion(fr, to, Bishop, 0));
            } else
                append_to_heap(sd->heap, make_move(fr, to, 0));
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
            append_to_heap(sd->heap, make_move(fr, to, M_PAWND));
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
                sd->historyTab[p->m_nTurn][sd->heap->data[besti].GetFromToIndex()];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                int hval = sd->historyTab[p->m_nTurn]
                                        [sd->heap->data[i].GetFromToIndex()];
                if (hval > best) {
                    best = hval;
                    besti = i;
                }
            }
            move = sd->heap->data[besti];

            section->end--;
            sd->heap->data[besti] = sd->heap->data[section->end];

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

static void GenerateQCaptures(struct SearchData *sd, int alpha) {
    heap_section_t section = sd->heap->current_section;
    CPosition *p = sd->position;
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
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
            }
        }

        tmp = p->m_rgAtkTo[i] & p->m_rgMask[OPP(p->m_nTurn)][0];
        while (tmp) {
            int sw;
            j = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            CMove move = make_promotion(i, j, Queen, M_CAPTURE);
            if ((sw = SwapOff(p, move)) >= 0) {
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
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
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
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
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
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
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
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
                append_to_heap(sd->heap, move);
                grow_data_heap(sd);
                sd->data_heap[section->end - 1] = sw;
            }
        }
    }
}

CMove NextMoveQ(struct SearchData *sd, int alpha) {
    heap_section_t section = sd->heap->current_section;
    struct SearchStatus *st = sd->current;
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
            int best = sd->data_heap[besti];

            for (unsigned int i = section->start + 1; i < section->end; i++) {
                if (sd->data_heap[i] > best) {
                    best = sd->data_heap[i];
                    besti = i;
                }
            }

            move = sd->heap->data[besti];
            section->end--;
            sd->heap->data[besti] = sd->heap->data[section->end];
            sd->data_heap[besti] = sd->data_heap[section->end];

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

void PutKiller(struct SearchData *sd, CMove m) {
    struct KillerEntry *k = sd->killer;

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

static void test_next_move(struct SearchData *sd,
                           CMove (*func)(struct SearchData *)) {
    bool comma = false;
    EnterNode(sd);

    while (true) {
        CMove move = func(sd);
        if (move == M_NONE)
            break;

        if (sd->position->LegalMove(move)) {
            if (comma) {
                Print(0, ", ");
            }
            char san_buffer[16];
            Print(0, "%s", sd->position->SAN(move, san_buffer));
            comma = true;
        }
    }
    LeaveNode(sd);
    Print(0, "\n");
}

CMove next_move_q_fixed_alpha(struct SearchData *sd) {
    return NextMoveQ(sd, -500000);
}

void TestNextGenerators(CPosition *p) {
    struct SearchData *sd = CreateSearchData(p);
    Print(0, "NextMove:\n");
    test_next_move(sd, &NextMove);
    Print(0, "\nNextEvasion:\n");
    test_next_move(sd, &NextEvasion);
    Print(0, "\nNextMoveQ:\n");
    test_next_move(sd, &next_move_q_fixed_alpha);
}
