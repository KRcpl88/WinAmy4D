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
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "probe.h"
#include "random.h"
#include "safe_malloc.h"
#include "search_io.h"
#include "swap.h"
#include "utils.h"
#include <string.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

extern OPTIONAL_ATOMIC unsigned long TotalNodes;

/**
 * Description: Creates and initializes per-search state for move ordering and node bookkeeping.
 * Inputs: pPosition - position used by this search context.
 * Outputs: Initializes all members and allocates internal heaps/tables.
 */
CSearchData::CSearchData(CPosition *p) {
    memset(this, 0, sizeof(*this));

    m_pPosition = p;
    m_pStatusTable =
        (struct SSearchStatus *)safe_calloc(MAX_TREE_SIZE,
                                           sizeof(struct SSearchStatus));
    m_pCurrent = m_pStatusTable;
    m_pKillerTable =
        (struct SKillerEntry *)safe_calloc(MAX_TREE_SIZE,
                                          sizeof(struct SKillerEntry));
    m_pKiller = m_pKillerTable;

    m_hHeap = allocate_heap();

    m_pnDataHeap = NULL;
    m_uDataHeapSize = 0;

#if MP
    m_pLocalHashTable =
        (struct HTEntry *)safe_calloc(sizeof(struct HTEntry), L_HT_Size);
    m_hDeferredHeap = allocate_heap();
#endif

    m_wPly = 0;

}

/**
 * Description: Releases all heap/table resources owned by this search context.
 * Inputs: None.
 * Outputs: Frees all dynamically allocated members.
 */
CSearchData::~CSearchData() {
    free(m_pStatusTable);
    free(m_pKillerTable);
    free(m_pnDataHeap);
    free_heap(m_hHeap);

#if MP
    free(m_pLocalHashTable);
    free_heap(m_hDeferredHeap);
#endif

}

/**
 * Description: Enters one search ply and initializes phase state for move generation at that ply.
 * Inputs: None.
 * Outputs: Increments ply state and pushes heap sections.
 */
void CSearchData::EnterNode() {
    struct SSearchStatus *pSt;

    pSt = ++(m_pCurrent);

    pSt->st_phase = HashMove;
    m_wPly++;
    m_pKiller++;

    push_section(m_hHeap);
#if MP
    push_section(m_hDeferredHeap);
#endif
}

/**
 * Description: Leaves one search ply and restores parent search state.
 * Inputs: None.
 * Outputs: Pops heap sections and decrements ply state.
 */
void CSearchData::LeaveNode() {
    pop_section(m_hHeap);
    m_pCurrent--;
    m_pKiller--;
    m_wPly--;
#if MP
    pop_section(m_hDeferredHeap);
#endif
}

static inline void GrowDataHeap(CSearchData *pSd) {
    if (pSd->m_hHeap->current_section->end > pSd->m_uDataHeapSize) {
        pSd->m_uDataHeapSize = pSd->m_hHeap->current_section->end + 256;
        pSd->m_pnDataHeap = (int32_t *)realloc(
            pSd->m_pnDataHeap, pSd->m_uDataHeapSize * sizeof(int32_t));
        if (pSd->m_pnDataHeap == NULL) {
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
    heap_section_t pSection = m_hHeap->current_section;
    struct SSearchStatus *pSt = m_pCurrent;
    CPosition *p = m_pPosition;
    CMove move;

    switch (pSt->st_phase) {
    case HashMove:
#ifdef VERBOSE
        Print(9, "HashMove\n");
#endif
        if (p->LegalMove(pSt->st_hashmove)) {
            pSt->st_phase = GenerateCaptures;
            return pSt->st_hashmove;
        } else {
            pSt->st_hashmove = M_NONE;
        }
    /* fall through */
    case GenerateCaptures: {
#ifdef VERBOSE
        Print(9, "GenerateCaptures\n");
#endif

        /*
         * Generate captures.
         */
        CBitBoard targets = p->GetMask(OPP(p->GetTurn()), 0);
        while (targets) {
            CSCoord to = (targets).FindSetBitCoord();
            targets.ClearLowestBit();

            p->GenTo(to, m_hHeap);
        }

        CBitBoard PromotingPawns =
            p->GetMask(p->GetTurn(), Pawn) & PrePromoRank[p->GetTurn()];
        while (PromotingPawns) {
            CSCoord from = (PromotingPawns).FindSetBitCoord();
            PromotingPawns.ClearLowestBit();

            p->GenFrom(from, m_hHeap);
        }

        GrowDataHeap(this);
        for (unsigned int j = pSection->start; j < pSection->end; j++) {
            m_pnDataHeap[j] = SwapOff(p, m_hHeap->data[j]);
        }

        unsigned int dwLastEnd = pSection->end;
        p->GenEnpas(m_hHeap);
        GrowDataHeap(this);

        for (unsigned int j = dwLastEnd; j < pSection->end; j++) {
            m_pnDataHeap[j] = 0;
        }

        pSt->st_phase = GainingCapture;
    }
    /* fall through */
    case GainingCapture:
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (m_pnDataHeap[i] > nBest) {
                    nBest = m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            if (nBest >= 0) {
                move = m_hHeap->data[dwBestI];
                pSection->end--;

                m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];
                m_pnDataHeap[dwBestI] = m_pnDataHeap[pSection->end];

                if (move == pSt->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
    /* fall through */
    case Killer1: {
        move = m_pKiller->killer1;
#ifdef VERBOSE
        Print(9, "Killer1\n");
#endif
        pSt->st_k1 = M_NONE;
        if (move != pSt->st_hashmove && p->LegalMove(move)) {
            pSt->st_phase = Killer2;
            pSt->st_k1 = move;

            return move;
        }
    }
    /* fall through */
    case Killer2: {
        move = m_pKiller->killer2;
#ifdef VERBOSE
        Print(9, "Killer2\n");
#endif
        pSt->st_k2 = M_NONE;
        if (move != pSt->st_hashmove && p->LegalMove(move)) {
            pSt->st_phase = CounterMv;
            pSt->st_k2 = move;

            return move;
        }
    }
    /* fall through */
    case CounterMv: {
        CMove LastMove = (p->GetActLog() - 1)->gl_Move;

#ifdef VERBOSE
        Print(9, "CounterMv\n");
#endif
        pSt->st_cm = M_NONE;
        if (LastMove != M_NULL) {
            move = m_rgCounterTab[p->GetTurn()][LastMove.GetFromCoord().BitOffset()][LastMove.GetToCoord().BitOffset()];

            if (move != M_NONE && move != pSt->st_hashmove &&
                move != pSt->st_k1 && move != pSt->st_k2 && p->LegalMove(move)) {
                pSt->st_phase = Killer3;
                pSt->st_cm = move;

                return move;
            }
        }
    }
    /* fallthrough */
    case Killer3:
#ifdef VERBOSE
        Print(9, "Killer3\n");
#endif
        pSt->st_k3 = M_NONE;
        if (m_wPly >= 2) {
            move = (m_pKiller - 2)->killer1;

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_cm || !p->LegalMove(move))
                move = (m_pKiller - 2)->killer2;

            if (move != pSt->st_hashmove && move != pSt->st_k1 &&
                move != pSt->st_k2 && move != pSt->st_cm && p->LegalMove(move)) {
                pSt->st_phase = LoosingCapture;
                pSt->st_k3 = move;

                return move;
            }
        }
        /* fallthrough */

    case LoosingCapture:
#ifdef VERBOSE
        Print(9, "LoosingCapture\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (m_pnDataHeap[i] > nBest) {
                    nBest = m_pnDataHeap[i];
                    dwBestI = i;
                }
            }

            move = m_hHeap->data[dwBestI];
            pSection->end--;

            m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];
            m_pnDataHeap[dwBestI] = m_pnDataHeap[pSection->end];

            pSt->st_phase = LoosingCapture;

            if (move == pSt->st_hashmove)
                continue;

            return move;
        }
        /* fallthrough */

    case GenerateRest: {
#ifdef VERBOSE
        Print(9, "GenerateRest\n");
#endif
        const CBitBoard empty = ~(p->GetMask(White, 0) | p->GetMask(Black, 0));

        if (p->GetCastle() & CastleMask[p->GetTurn()][0]) {
            append_to_heap(m_hHeap,
                           make_move(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8,
                                     p->GetTurn() == White ? CASTLE_G1 : CASTLE_G8, M_SCASTLE));
        }
        if (p->GetCastle() & CastleMask[p->GetTurn()][1]) {
            append_to_heap(m_hHeap,
                           make_move(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8,
                                     p->GetTurn() == White ? CASTLE_C1 : CASTLE_C8, M_LCASTLE));
        }

        CBitBoard NonPawn = p->GetMask(p->GetTurn(), 0) & ~p->GetMask(p->GetTurn(), Pawn);

        while (NonPawn) {
            int nFrom = (NonPawn).FindSetBit();
            NonPawn.ClearLowestBit();
            CBitBoard attacks = p->GetAtkTo(nFrom) & empty;
            while (attacks) {
                int nTo = (attacks).FindSetBit();
                attacks.ClearLowestBit();
                append_to_heap(m_hHeap, make_move(nFrom, nTo, 0));
            }
        }

        {
            const int nDirection = (p->GetTurn() == White) ? 1 : -1;
            CBitBoard pawns = p->GetMask(p->GetTurn(), Pawn) & ~PrePromoRank[p->GetTurn()];
            while (pawns) {
                CSCoord fromCoord = pawns.FindSetBitCoord();
                pawns.ClearLowestBit();
                const int nWidth = static_cast<int>(CBitBoard::LEVEL_WIDTH[fromCoord.m_nLevel]);
                const uint16_t nNewRank =
                    static_cast<uint16_t>(static_cast<int>(fromCoord.m_nRank) + nDirection);
                if (nNewRank >= nWidth)
                    continue;
                CSCoord toCoord(fromCoord.m_nLevel, fromCoord.m_nFile, nNewRank);
                if (is_promo_square(toCoord) || p->GetPiece(toCoord.BitOffset()) != Neutral ||
                    !pawn_may_move_to(toCoord))
                    continue;
                append_to_heap(m_hHeap, make_move(fromCoord, toCoord, 0));
                const uint16_t nHomeRank =
                    static_cast<uint16_t>((p->GetTurn() == White) ? 1 : (nWidth - 2));
                if (fromCoord.m_nLevel == MAIN_LEVEL &&
                    fromCoord.m_nRank == nHomeRank) {
                    const uint16_t nDblRank =
                        static_cast<uint16_t>(static_cast<int>(fromCoord.m_nRank) + 2 * nDirection);
                    if (nDblRank < nWidth) {
                        CSCoord dblCoord(fromCoord.m_nLevel, fromCoord.m_nFile, nDblRank);
                        if (!is_promo_square(dblCoord) &&
                            p->GetPiece(dblCoord.BitOffset()) == Neutral) {
                            append_to_heap(m_hHeap, make_move(fromCoord, dblCoord, M_PAWND));
                        }
                    }
                }
            }
        }

        pSt->st_phase = HistoryMoves;
    }

    case HistoryMoves:
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif
        while (pSection->end > pSection->start) {
            int nBestI = pSection->start;
            int nBest =
                m_rguHistoryTab[p->GetTurn()][m_hHeap->data[nBestI].GetFromCoord().BitOffset()][m_hHeap->data[nBestI].GetToCoord().BitOffset()];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                int nHval = m_rguHistoryTab[p->GetTurn()]
                                        [m_hHeap->data[i].GetFromCoord().BitOffset()]
                                        [m_hHeap->data[i].GetToCoord().BitOffset()];
                if (nHval > nBest) {
                    nBest = nHval;
                    nBestI = i;
                }
            }
            move = m_hHeap->data[nBestI];

            pSection->end--;
            m_hHeap->data[nBestI] = m_hHeap->data[pSection->end];

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_k3 || move == pSt->st_cm)
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
    heap_section_t pSection = m_hHeap->current_section;
    struct SSearchStatus *pSt = m_pCurrent;
    CPosition *p = m_pPosition;
    CMove move;

    switch (pSt->st_phase) {
    case HashMove:
#ifdef VERBOSE
        Print(9, "HashMove\n");
#endif
        if (p->LegalMove(pSt->st_hashmove)) {
            pSt->st_phase = GenerateCaptures;
            return pSt->st_hashmove;
        } else {
            pSt->st_hashmove = M_NONE;
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

        int nKp = p->GetKingSq(p->GetTurn()).BitOffset();

        CBitBoard targets =
            (p->GetAtkFr(nKp) | p->GetAtkTo(nKp)) & p->GetMask(OPP(p->GetTurn()), 0);

        while (targets) {
            CSCoord to = (targets).FindSetBitCoord();
            targets.ClearLowestBit();
            p->GenTo(to, m_hHeap);
        }

        GrowDataHeap(this);
        for (unsigned int j = pSection->start; j < pSection->end; j++) {
            m_pnDataHeap[j] = SwapOff(p, m_hHeap->data[j]);
        }

        unsigned int dwLastEnd = pSection->end;
        p->GenEnpas(m_hHeap);
        GrowDataHeap(this);

        for (unsigned int j = dwLastEnd; j < pSection->end; j++) {
            m_pnDataHeap[j] = 0;
        }
    }
        /* fall through */
    case GainingCapture:
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (m_pnDataHeap[i] > nBest) {
                    nBest = m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            if (nBest >= 0) {
                move = m_hHeap->data[dwBestI];
                pSection->end--;

                m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];
                m_pnDataHeap[dwBestI] = m_pnDataHeap[pSection->end];

                pSt->st_phase = GainingCapture;

                if (move == pSt->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
        /* fall through */
    case Killer1: {
        move = m_pKiller->killer1;
#ifdef VERBOSE
        Print(9, "Killer1\n");
#endif
        pSt->st_k1 = M_NONE;
        if (move != pSt->st_hashmove && p->LegalMove(move)) {
            pSt->st_phase = Killer2;
            pSt->st_k1 = move;

            return move;
        }
    }
        /* fall through */
    case Killer2: {
        move = m_pKiller->killer2;
#ifdef VERBOSE
        Print(9, "Killer2\n");
#endif
        pSt->st_k2 = M_NONE;
        if (move != pSt->st_hashmove && p->LegalMove(move)) {
            pSt->st_phase = CounterMv;
            pSt->st_k2 = move;

            return move;
        }
    }
        /* fall through */
    case CounterMv: {
        CMove LastMove = (p->GetActLog() - 1)->gl_Move;

#ifdef VERBOSE
        Print(9, "CounterMv\n");
#endif
        pSt->st_cm = M_NONE;
        if (LastMove != M_NULL) {
            move = m_rgCounterTab[p->GetTurn()][LastMove.GetFromCoord().BitOffset()][LastMove.GetToCoord().BitOffset()];

            if (move != M_NONE && move != pSt->st_hashmove &&
                move != pSt->st_k1 && move != pSt->st_k2 && p->LegalMove(move)) {
                pSt->st_phase = Killer3;
                pSt->st_cm = move;

                return move;
            }
        }
    }
        /* fall through */
    case Killer3:
#ifdef VERBOSE
        Print(9, "Killer3\n");
#endif
        pSt->st_k3 = M_NONE;
        if (m_wPly >= 2) {
            move = (m_pKiller - 2)->killer1;

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_cm || !p->LegalMove(move))
                move = (m_pKiller - 2)->killer2;

            if (move != pSt->st_hashmove && move != pSt->st_k1 &&
                move != pSt->st_k2 && move != pSt->st_cm && p->LegalMove(move)) {
                pSt->st_phase = /* HistoryMoves; */ LoosingCapture;
                pSt->st_k3 = move;

                return move;
            }
        }
        /* fall through */
    case LoosingCapture:
#ifdef VERBOSE
        Print(9, "LoosingCapture\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (m_pnDataHeap[i] > nBest) {
                    nBest = m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            move = m_hHeap->data[dwBestI];

            pSection->end--;
            m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];
            m_pnDataHeap[dwBestI] = m_pnDataHeap[pSection->end];

            pSt->st_phase = LoosingCapture;

            if (move == pSt->st_hashmove)
                continue;

            return move;
        }

        /* fall through */
    case GenerateRest: {
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif

        const int nKp =
            p->GetKingSq(p->GetTurn()).BitOffset(); /* (Mask[Side][King]).FindSetBit(); */
        const CBitBoard empty = ~(p->GetMask(White, 0) | p->GetMask(Black, 0));

        CBitBoard KingFlightSquares = p->GetAtkTo(nKp) & empty;

        while (KingFlightSquares) {
            int nTo = (KingFlightSquares).FindSetBit();
            KingFlightSquares.ClearLowestBit();
            if (!(p->GetAtkFr(nTo) & p->GetMask(OPP(p->GetTurn()), 0)))
                append_to_heap(m_hHeap, make_move(nKp, nTo, 0));
        }

        CBitBoard SlidingAttackers =
            (p->GetMask(OPP(p->GetTurn()), Bishop) | p->GetMask(OPP(p->GetTurn()), Rook) |
             p->GetMask(OPP(p->GetTurn()), Queen)) &
            p->GetAtkFr(nKp);

        CBitBoard interpositions;

        while (SlidingAttackers) {
            int nAttackerSq = (SlidingAttackers).FindSetBit();
            SlidingAttackers.ClearLowestBit();
            interpositions = InterPath[nKp][nAttackerSq];
        }

        CBitBoard NonPawns = (p->GetMask(p->GetTurn(), 0) & ~p->GetMask(p->GetTurn(), King)) &
                             ~p->GetMask(p->GetTurn(), Pawn);

        while (NonPawns) {
            int nFrom = (NonPawns).FindSetBit();
            NonPawns.ClearLowestBit();
            CBitBoard blocking = p->GetAtkTo(nFrom) & empty & interpositions;

            while (blocking) {
                int nTo = (blocking).FindSetBit();
                blocking.ClearLowestBit();
                append_to_heap(m_hHeap, make_move(nFrom, nTo, 0));
            }
        }

        {
            const int nDirection = (p->GetTurn() == White) ? 1 : -1;
            CBitBoard pawns = p->GetMask(p->GetTurn(), Pawn);
            while (pawns) {
                CSCoord fromCoord = pawns.FindSetBitCoord();
                pawns.ClearLowestBit();
                const int nWidth = static_cast<int>(CBitBoard::LEVEL_WIDTH[fromCoord.m_nLevel]);
                const int nNewRank = static_cast<int>(fromCoord.m_nRank) + nDirection;
                if (nNewRank < 0 || nNewRank >= nWidth)
                    continue;
                CSCoord toCoord(fromCoord.m_nLevel, fromCoord.m_nFile,
                                static_cast<uint16_t>(nNewRank));
                if (p->GetPiece(toCoord.BitOffset()) != Neutral)
                    continue;
                if (is_promo_square(toCoord)) {
                    append_to_heap(m_hHeap, make_promotion(fromCoord, toCoord, Queen, 0));
                    append_to_heap(m_hHeap, make_promotion(fromCoord, toCoord, Knight, 0));
                    append_to_heap(m_hHeap, make_promotion(fromCoord, toCoord, Rook, 0));
                    append_to_heap(m_hHeap, make_promotion(fromCoord, toCoord, Bishop, 0));
                } else if (pawn_may_move_to(toCoord)) {
                    append_to_heap(m_hHeap, make_move(fromCoord, toCoord, 0));
                    const int nHomeRank = (p->GetTurn() == White) ? 1 : (nWidth - 2);
                    if (fromCoord.m_nLevel == MAIN_LEVEL &&
                        static_cast<int>(fromCoord.m_nRank) == nHomeRank) {
                        const int nDblRank = static_cast<int>(fromCoord.m_nRank) + 2 * nDirection;
                        if (nDblRank >= 0 && nDblRank < nWidth) {
                            CSCoord dblCoord(fromCoord.m_nLevel, fromCoord.m_nFile,
                                            static_cast<uint16_t>(nDblRank));
                            if (!is_promo_square(dblCoord) &&
                                p->GetPiece(dblCoord.BitOffset()) == Neutral) {
                                append_to_heap(m_hHeap, make_move(fromCoord, dblCoord, M_PAWND));
                            }
                        }
                    }
                }
            }
        }

        pSt->st_phase = HistoryMoves;
    }

        /* fall through */
    case HistoryMoves:
#ifdef VERBOSE
        Print(9, "HistoryMoves\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest =
                m_rguHistoryTab[p->GetTurn()][m_hHeap->data[dwBestI].GetFromCoord().BitOffset()][m_hHeap->data[dwBestI].GetToCoord().BitOffset()];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                int nHval = m_rguHistoryTab[p->GetTurn()]
                                        [m_hHeap->data[i].GetFromCoord().BitOffset()]
                                        [m_hHeap->data[i].GetToCoord().BitOffset()];
                if (nHval > nBest) {
                    nBest = nHval;
                    dwBestI = i;
                }
            }
            move = m_hHeap->data[dwBestI];

            pSection->end--;
            m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_k3 || move == pSt->st_cm)
                continue;

            return move;
        }
    default:
        break;
    }

    return M_NONE;
}

/*
 * Emit a single quiescence capture of the victim on "to" by the attacker on
 * "from".  Whether the move is a promotion is decided solely by the *destination*
 * square (is_promo_square), never by the attacker's source rank.
 *
 * This matters in the 3-D variant: a pawn can reach a promotion square via a
 * cross-level capture from a square that is NOT on the pre-promotion rank, and
 * conversely a pawn on the pre-promotion rank can capture across levels onto a
 * square that is NOT a promotion square.  Keying promotion off the source rank
 * (PrePromoRank) — a flat 8x8-board assumption — produced illegal moves such as
 * a pawn capturing onto a promotion square without promoting, corrupting the
 * board state during search.
 */
static void EmitQCapture(CSearchData *pSd, CPosition *p, int nFrom, int nTo) {
    heap_section_t pSection = pSd->m_hHeap->current_section;
    CMove Move;

    if (TYPE(p->GetPiece(static_cast<uint16_t>(nFrom))) == Pawn) {
        const CSCoord ToCoord(static_cast<uint16_t>(nTo));
        if (is_promo_square(ToCoord)) {
            Move = make_promotion(nFrom, nTo, Queen, M_CAPTURE);
        } else if (pawn_may_move_to(ToCoord)) {
            Move = make_move(nFrom, nTo, M_CAPTURE);
        } else {
            /* A cross-level capture can land a pawn on the edge rank of a
             * non-promotion level, which is an illegal pawn target.  This is a
             * legal, expected situation that the authoritative GenTo generator
             * also silently skips (see dbase.cpp), not an internal error, so do
             * not assert here. */
            return;
        }
    } else {
        Move = make_move(nFrom, nTo, M_CAPTURE);
    }

    int nSwap = SwapOff(p, Move);
    if (nSwap >= 0) {
        append_to_heap(pSd->m_hHeap, Move);
        GrowDataHeap(pSd);
        pSd->m_pnDataHeap[pSection->end - 1] = nSwap;
    }
}

static void GenerateQCaptures(CSearchData *pSd, int nAlpha) {
    heap_section_t pSection = pSd->m_hHeap->current_section;
    CPosition *p = pSd->m_pPosition;
    CBitBoard pwn7th;
    CBitBoard att, def;
    int nScore;
    int i;

    att = p->GetMask(p->GetTurn(), 0);

    /*
     * Handle the non-capturing promotion pushes first.  A pawn can only push
     * forward onto a promotion square from the pre-promotion rank, so iterate
     * exactly those pawns.  Their *captures* are handled uniformly by the
     * per-victim loops below (via EmitQCapture), which decide promotion from the
     * destination square, so the pre-promotion pawns are NOT removed from "Att".
     */
    pwn7th = p->GetMask(p->GetTurn(), Pawn) & PrePromoRank[p->GetTurn()];

    while (pwn7th) {
        int nNext;

        i = (pwn7th).FindSetBit();
        pwn7th.ClearLowestBit();
        const CSCoord iCoord(static_cast<uint16_t>(i));
        nNext = (p->GetTurn() == White)
                   ? i + static_cast<int>(CBitBoard::LEVEL_WIDTH[iCoord.m_nLevel])
                   : i - static_cast<int>(CBitBoard::LEVEL_WIDTH[iCoord.m_nLevel]);

        if (p->GetPiece(nNext) == Neutral && is_promo_square(CSCoord(static_cast<uint16_t>(nNext)))) {
            CMove move = make_promotion(i, nNext, Queen, 0);
            int nSw;
            if ((nSw = SwapOff(p, move)) >= 0) {
                append_to_heap(pSd->m_hHeap, move);
                GrowDataHeap(pSd);
                pSd->m_pnDataHeap[pSection->end - 1] = nSw;
            }
        }
    }

    if (p->GetTurn() == White) {
        nScore = MaterialBalance(p) + MaxPos;
    } else {
        nScore = -MaterialBalance(p) + MaxPos;
    }

    if (nScore + Value[Queen] <= nAlpha)
        return;
    def = p->GetMask(OPP(p->GetTurn()), Queen);
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->GetAtkFr(i) & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            EmitQCapture(pSd, p, j, i);
        }
    }
    if (nScore + Value[Rook] <= nAlpha)
        return;
    def = p->GetMask(OPP(p->GetTurn()), Rook);
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->GetAtkFr(i) & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            EmitQCapture(pSd, p, j, i);
        }
    }
    if (nScore + Value[Bishop] <= nAlpha)
        return;
    def = p->GetMask(OPP(p->GetTurn()), Bishop) | p->GetMask(OPP(p->GetTurn()), Knight);
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->GetAtkFr(i) & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            EmitQCapture(pSd, p, j, i);
        }
    }
    if (nScore + Value[Pawn] <= nAlpha)
        return;
    def = p->GetMask(OPP(p->GetTurn()), Pawn);
    while (def) {
        CBitBoard tmp2;
        int j;
        i = (def).FindSetBit();
        def.ClearLowestBit();
        tmp2 = p->GetAtkFr(i) & att;
        while (tmp2) {
            j = (tmp2).FindSetBit();
            tmp2.ClearLowestBit();
            EmitQCapture(pSd, p, j, i);
        }
    }
}

/**
 * Description: Produces the next tactical move for quiescence search.
 * Inputs: nAlpha - current alpha bound used for pruning tactical generation.
 * Outputs: Returns next quiescence move or M_NONE when exhausted.
 */
CMove CSearchData::NextMoveQ(int nAlpha) {
    heap_section_t pSection = m_hHeap->current_section;
    struct SSearchStatus *pSt = m_pCurrent;
    CMove move;

    switch (pSt->st_phase) {
    case HashMove:
    case GenerateCaptures:
#ifdef VERBOSE
        Print(9, "GenerateCaptures\n");
#endif
        GenerateQCaptures(this, nAlpha);
        pSt->st_phase = GainingCapture;

        /* fall through */
    case GainingCapture:
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (m_pnDataHeap[i] > nBest) {
                    nBest = m_pnDataHeap[i];
                    dwBestI = i;
                }
            }

            move = m_hHeap->data[dwBestI];
            pSection->end--;
            m_hHeap->data[dwBestI] = m_hHeap->data[pSection->end];
            m_pnDataHeap[dwBestI] = m_pnDataHeap[pSection->end];

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
void CSearchData::PutKiller(CMove Move) {
    struct SKillerEntry *pK = m_pKiller;

    if (Move == pK->killer1) {
        pK->kcount1 += 1;
    } else if (Move == pK->killer2) {
        pK->kcount2 += 1;
        if (pK->kcount2 > pK->kcount1) {
            int nTmpCount;
            CMove tmpMove;

            nTmpCount = pK->kcount1;
            pK->kcount1 = pK->kcount2;
            pK->kcount2 = nTmpCount;

            tmpMove = pK->killer1;
            pK->killer1 = pK->killer2;
            pK->killer2 = tmpMove;
        }
    } else {
        if (pK->killer1 == M_NONE) {
            pK->killer1 = Move;
            pK->kcount1 = 1;
        } else {
            pK->killer2 = Move;
            pK->kcount2 = 1;
        }
    }
}

#define ALTERNATE_DELTA 1500

/*
 * This routine searches a chess position. It uses iterative deepening,
 * aspiration window and scout search.
 */
void CSearchData::IterateInt() {
    unsigned long rgNodes[256];
    int nLast = 0;
    double dElapsed;
    CPosition *p;
    bool fAnyPvPrinted = false;
    bool fPvValid = false;

    if (!m_fMaster) {
#ifdef _WIN32
        Sleep((DWORD)((50.0 + 100.0 * Random()) / 1000.0));
#else
        usleep((useconds_t)(50 + 100 * Random()));
#endif
    }
    p = m_pPosition;

    InitSearch();
    m_wRootMoves = (uint16_t)p->LegalMoves(m_hHeap);

    // The root move list lives at a fixed index range within the shared search
    // heap. Move generation deeper in the search appends to that same heap, and
    // append_to_heap() may realloc() the underlying data buffer, relocating it
    // and invalidating any cached pointer into it. Remember the root section
    // start index and re-derive `mvs` from the (possibly moved) heap->data after
    // every operation that can grow the heap, so we never read through a
    // dangling pointer (which previously yielded a corrupt best move that
    // crashed in CPosition::SAN).
    const unsigned int dwRootStart = m_hHeap->current_section->start;
    CMove *pMvs = m_hHeap->data + dwRootStart;

    /*
     * Drop any caller-excluded root moves (used to emulate MultiPV by repeated
     * searches that each exclude the previously found best move). Compacting the
     * move array in place and shrinking m_wRootMoves makes the rest of the
     * search ignore them entirely. The default (empty) set is a no-op.
     */
    if (cExcludedRootMoves > 0) {
        uint16_t wWriteIndex = 0;
        for (uint16_t r = 0; r < m_wRootMoves; r++) {
            bool fExcluded = false;
            for (uint16_t e = 0; e < cExcludedRootMoves; e++) {
                if (pMvs[r] == rgExcludedRootMoves[e]) {
                    fExcluded = true;
                    break;
                }
            }
            if (!fExcluded) {
                pMvs[wWriteIndex++] = pMvs[r];
            }
        }
        m_wRootMoves = wWriteIndex;
    }

    m_nBestScore = p->GetMaterial(p->GetTurn()) - p->GetMaterial(OPP(p->GetTurn()));

    /*
     * If every legal move was excluded there is nothing to search. Bail out with
     * no best move rather than dereferencing mvs[0] below.
     */
    if (m_wRootMoves == 0) {
        /*
         * No legal root moves were generated (stalemate/checkmate, or every
         * move was excluded above). Log the guard so a search that bails out
         * with no best move can be distinguished from other early exits when
         * auditing the log file.
         */
        PrintDebug(2, "IterateInt: no legal root moves (%s); returning M_NONE.\n",
                   m_fMaster ? "master" : "helper");
        m_BestMove = M_NONE;
        if (!m_fMaster) {
            PrintDebug(2, "IterateInt: freeing helper clone %p.\n",
                       (void *)m_pPosition);
            CPosition::Free(m_pPosition);
            delete this;
        }
        return;
    }

    if (!pMvs[0].IsTactical()) {
        PutKiller(pMvs[0]);
    }

    MaxDepth = MAX_TREE_SIZE - 1;

    for (m_wDepth = 1; m_wDepth < MaxSearchDepth; m_wDepth++) {
        int nAlpha = m_nBestScore - PVWindow;
        int nBeta = m_nBestScore + PVWindow;
        bool fIsPv = true;
        bool fPvStable = true;

        for (m_wMoveNum = 0; m_wMoveNum < m_wRootMoves; m_wMoveNum++) {
            int nTmp;
            int nNextDepth = (m_wDepth - 2) * OnePly;
            CMove move = pMvs[m_wMoveNum];
            bool fIsAlternate = !fIsPv && move == m_AlternateMove;

            rgNodes[m_wMoveNum] = m_ulNodesCount;

            if (m_fMaster && PrintOK) {
                char szTimeBuffer[16];
                char szSanBuffer[32];

                PrintDebug(
                    2, "%2d  %s   %2d/%2d  %s      \r", m_wDepth,
                    FormatTime(CurTime - StartTime, szTimeBuffer,
                               sizeof(szTimeBuffer)),
                    m_wMoveNum + 1, m_wRootMoves,
                    p->NumberedSAN(move, szSanBuffer, sizeof(szSanBuffer)));
            }

            p->DoMove(move);
            /*
             * Root moves come from LegalMoves(), so the side that just moved
             * must not have left its own king in check. If it has, an illegal
             * move slipped into the root list (the class of bug behind the
             * "engine recommended an illegal move" reports): log it and trap
             * into the debugger so the position/move can be inspected.
             */
            AMY_ASSERT(
                !p->InCheck(OPP(p->GetTurn())),
                "Illegal root move left own king in check: from L%d/F%d/R%d "
                "to L%d/F%d/R%d\n",
                move.GetFromCoord().m_nLevel, move.GetFromCoord().m_nFile,
                move.GetFromCoord().m_nRank, move.GetToCoord().m_nLevel,
                move.GetToCoord().m_nFile, move.GetToCoord().m_nRank);
            if (p->InCheck(p->GetTurn())) {
                nNextDepth += ExtendInCheck;
            }

            if (nNextDepth >= 0) {
                int nEffectiveAlpha =
                    fIsAlternate ? (nAlpha - ALTERNATE_DELTA) : nAlpha;
#if MP
                nTmp = -NegaScout(-nBeta, -nEffectiveAlpha, nNextDepth,
                                  fIsPv ? PVNode : CutNode, 0);
#else
                nTmp = -NegaScout(-nBeta, -nEffectiveAlpha, nNextDepth,
                                  fIsPv ? PVNode : CutNode);
#endif
                if (fIsAlternate) {
                    m_nAlternateScore = nTmp;
                }
            } else {
                nTmp = -Quies(-nBeta, -nAlpha, 0);
            }
            p->UndoMove(move);
            // Move generation during the search above may have realloc'd the
            // heap; re-derive `mvs` so it points into the live data buffer.
            pMvs = m_hHeap->data + dwRootStart;
            if (AbortSearch) {
                goto final;
            }

            if (fIsPv && nTmp <= nAlpha) {

                /*
                 * Fail low on principal variation.
                 * Open window, take some time, and re-search.
                 */

                fPvStable = false;

                if (m_fMaster && PrintOK) {
                    char szSanBuffer[32];
                    SearchOutputFailHighLow(
                        m_wDepth, CurTime - StartTime, false,
                        p->NumberedSAN(move, szSanBuffer, sizeof(szSanBuffer)),
                        m_ulNodesCount + m_ulQNodesCount);
                }

                NeedTime = true;

                nBeta = nTmp;
                nAlpha = nTmp - ResearchWindow;

                p->DoMove(move);
                if (nNextDepth >= 0) {
#if MP
                    nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                    nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                } else {
                    nTmp = -Quies(-nBeta, -nAlpha, 0);
                }
                p->UndoMove(move);
                pMvs = m_hHeap->data + dwRootStart;
                if (AbortSearch) {
                    goto final;
                }

                if (nTmp <= nAlpha) {
                    nBeta = nTmp;
                    nAlpha = -INF;

                    p->DoMove(move);
                    if (nNextDepth >= 0) {
#if MP
                        nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                        nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                    } else {
                        nTmp = -Quies(-nBeta, -nAlpha, 0);
                    }
                    p->UndoMove(move);
                    pMvs = m_hHeap->data + dwRootStart;
                    if (AbortSearch) {
                        goto final;
                    }
                }
                rgNodes[m_wMoveNum] = m_ulNodesCount - rgNodes[m_wMoveNum];
            } else if (nTmp >= nBeta) {

                /*
                 * Fail high.
                 * Re-search with open window.
                 */

                fPvStable = false;

                if (m_wMoveNum != 0) {
                    unsigned long dwTn = rgNodes[m_wMoveNum];
                    int j;

                    for (j = m_wMoveNum; j > 0; j--) {
                        pMvs[j] = pMvs[j - 1];
                        rgNodes[j] = rgNodes[j - 1];
                    }
                    pMvs[0] = move;
                    rgNodes[0] = dwTn;

                    if (!(move.IsTactical())) {
                        PutKiller(move);
                    }
                    PBMove = M_NONE;
                    fIsPv = true;

                    FHTime = (CurTime - StartTime) / ONE_SECOND;
                }

                if (m_fMaster && PrintOK) {
                    char szSanBuffer[32];
                    SearchOutputFailHighLow(
                        m_wDepth, CurTime - StartTime, true,
                        p->NumberedSAN(pMvs[0], szSanBuffer, sizeof(szSanBuffer)),
                        m_ulNodesCount + m_ulQNodesCount);
                }

                nAlpha = nTmp;
                nBeta = nTmp + ResearchWindow;

                p->DoMove(move);
                if (nNextDepth >= 0) {
#if MP
                    nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                    nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                } else {
                    nTmp = -Quies(-nBeta, -nAlpha, 0);
                }
                p->UndoMove(move);
                pMvs = m_hHeap->data + dwRootStart;
                if (AbortSearch) {
                    goto final;
                }

                if (nTmp >= nBeta) {
                    nAlpha = nTmp;
                    nBeta = INF;

                    p->DoMove(move);
                    if (nNextDepth >= 0) {
#if MP
                        nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                        nTmp = -NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                    } else {
                        nTmp = -Quies(-nBeta, -nAlpha, 0);
                    }
                    p->UndoMove(move);
                    pMvs = m_hHeap->data + dwRootStart;
                    if (AbortSearch) {
                        goto final;
                    }
                }
                rgNodes[0] = m_ulNodesCount - rgNodes[0];
            } else {
                rgNodes[m_wMoveNum] = m_ulNodesCount - rgNodes[m_wMoveNum];
            }

            if (AbortSearch) {
                goto final;
            }

            if (fIsPv) {
                m_nBestScore = nTmp;

                if (m_fMaster) {
                    char szScoreAsText[16];
                    p->AnalyzeHT(pMvs[0]);
                    fPvValid = true;

                    snprintf(AnalysisLine, sizeof(AnalysisLine),
                             "%2d: (%7s) %s", m_wDepth,
                             FormatScore(m_nBestScore, szScoreAsText,
                                         sizeof(szScoreAsText)),
                             BestLine);

                    if (PrintOK) {
                        SearchOutput(m_wDepth, CurTime - StartTime,
                                     (p->GetTurn()) ? -m_nBestScore
                                                    : m_nBestScore,
                                     BestLine, m_ulNodesCount + m_ulQNodesCount);

                        fAnyPvPrinted = true;
                    }
                }

                nAlpha = m_nBestScore;
                nBeta = m_nBestScore + 1;
                fIsPv = false;
            }

            if (m_fMaster && m_wMoveNum == 0 && !NeedTime &&
                CurTime > SoftLimit) {
                if (SearchMode == Searching) {
                    AbortSearch = true;
                    goto final;
                } else if (SearchMode == Pondering) {
                    DoneAtRoot = true;
                }
            }
        }

        if (m_fMaster && (PrintOK || (m_wDepth > MateDepth &&
                                      (m_nBestScore < -CMLIMIT ||
                                       m_nBestScore > CMLIMIT)))) {
            SearchOutput(m_wDepth, CurTime - StartTime,
                         (p->GetTurn()) ? -m_nBestScore : m_nBestScore, BestLine,
                         m_ulNodesCount + m_ulQNodesCount);

            fAnyPvPrinted = true;
        }

        if (m_nBestScore < -CMLIMIT || m_nBestScore > CMLIMIT) {
            if (nLast > CMLIMIT && m_nBestScore >= nLast &&
                m_wDepth > MateDepth) {
                if (SearchMode == Searching) {
                    break;
                } else {
                    DoneAtRoot = true;
                }
            }
            if (SearchMode == Searching && nLast < CMLIMIT &&
                m_nBestScore <= nLast && m_wDepth > MateDepth) {
                break;
            }
            nLast = m_nBestScore;
        }

        NeedTime = false;
        pMvs = m_hHeap->data + dwRootStart;
        ResortMovesList(m_wRootMoves, pMvs, rgNodes);

        /*
            if(Depth > 5 && pv_stable) {
                double pv_percentage;
                int matval = NPVal[Side] / Value[Knight];
                double easy_threshold;
                if(matval > 10) matval = 10;

                easy_threshold = 0.95 - matval * 0.017;

                nodes_per_iteration = Nodes - nodes_per_iteration;
                pv_percentage = (double) nodes[0] /
                                (double) (nodes_per_iteration);

                if(pv_percentage >= easy_threshold) {
                    Print(1, "                    -> easy move\n");

                    SoftLimit = StartTime +
                                2 * (SoftLimit - StartTime) / 3;
                }
            }
        */

        CurTime = GetTime();

        if (m_wDepth > 3) {
            /*
             * Do ten checks per second.
             */

            dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

            NodesPerCheck =
                (dElapsed == 0.0)
                    ? 1000
                    : (int)((m_ulNodesCount + m_ulQNodesCount) / dElapsed / 10);
        }

        if (SearchMode == Puzzling && m_wDepth > 4) {
            break;
        }

        if (m_fMaster &&
            ((CurTime > SoftLimit) || (fPvStable && CurTime > SoftLimit2))) {
            if (SearchMode == Searching) {
                AbortSearch = true;
                break;
            } else if (SearchMode == Pondering) {
                DoneAtRoot = true;
            }
        }
    }

final:

    if (CurTime <= StartTime) {
        StartTime--;
    }
    dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

    if (m_fMaster) {
        if (fPvValid && !fAnyPvPrinted) {
            // Make sure there is a PV printed
            SearchOutput(m_wDepth, CurTime - StartTime,
                         (p->GetTurn()) ? -m_nBestScore : m_nBestScore, BestLine,
                         m_ulNodesCount + m_ulQNodesCount);
        }

        char szBuf1[16], szBuf2[16], szBuf3[16], szBuf4[16], szBuf5[16], szBuf6[16],
            szBuf7[16];

        unsigned long dwNps = (unsigned long)(TotalNodes / dElapsed);

        Print(2, "Nodes = %s, QPerc: %d %%, time = %g secs, %s nodes/s\n",
              FormatCount(TotalNodes, szBuf1, sizeof(szBuf1)),
              Percentage(m_ulQNodesCount, m_ulNodesCount + m_ulQNodesCount),
              dElapsed, FormatCount(dwNps, szBuf2, sizeof(szBuf2)));

        Print(2,
              "Extensions: Check: %s  DblChk: %s  DiscChk: %s  SingReply: %s\n"
              "            Recapture: %s   Passed Pawn: %s   Zugzwang: %s\n",
              FormatCount(ChkExt, szBuf1, sizeof(szBuf1)),
              FormatCount(DblExt, szBuf2, sizeof(szBuf2)),
              FormatCount(DiscExt, szBuf3, sizeof(szBuf3)),
              FormatCount(SingExt, szBuf4, sizeof(szBuf4)),
              FormatCount(RCExt, szBuf5, sizeof(szBuf5)),
              FormatCount(PPExt, szBuf6, sizeof(szBuf6)),
              FormatCount(ZZExt, szBuf7, sizeof(szBuf7)));

        Print(2,
              "Hashing: Trans: %s/%s = %d %%   Pawn: %s/%s = %d %%\n"
              "         Eval: %s/%s = %d %%\n",
              FormatCount(HHit, szBuf1, sizeof(szBuf1)),
              FormatCount(HTry, szBuf2, sizeof(szBuf2)), Percentage(HHit, HTry),
              FormatCount(PHit, szBuf3, sizeof(szBuf3)),
              FormatCount(PTry, szBuf4, sizeof(szBuf4)), Percentage(PHit, PTry),
              FormatCount(SHit, szBuf5, sizeof(szBuf5)),
              FormatCount(STry, szBuf6, sizeof(szBuf6)), Percentage(SHit, STry));

        if (EGTBProbe != 0) {
            Print(2, "EGTB Hits/Probes = %s/%s\n",
                  FormatCount(EGTBProbeSucc, szBuf1, sizeof(szBuf1)),
                  FormatCount(EGTBProbe, szBuf2, sizeof(szBuf2)));
        }

        ShowHashStatistics();
    }

    // Reached via the normal loop exit or via `goto final`; re-derive `mvs` one
    // last time in case the heap moved, so the returned best move is read from
    // the live buffer rather than a stale (freed) one.
    pMvs = m_hHeap->data + dwRootStart;
    m_BestMove = pMvs[0];

    /*
     * The returned best move must be legal in the (now restored) root position.
     * Returning an illegal move here is exactly the failure mode behind the
     * reported "engine recommended an illegal move" bug, so assert it: the
     * failure is logged and trapped in the debugger (no-op in release).
     */
    AMY_ASSERT(m_BestMove == M_NONE || p->LegalMove(m_BestMove),
               "IterateInt returning an illegal best move: from L%d/F%d/R%d "
               "to L%d/F%d/R%d\n",
               m_BestMove.GetFromCoord().m_nLevel,
               m_BestMove.GetFromCoord().m_nFile,
               m_BestMove.GetFromCoord().m_nRank,
               m_BestMove.GetToCoord().m_nLevel,
               m_BestMove.GetToCoord().m_nFile,
               m_BestMove.GetToCoord().m_nRank);

    if (!m_fMaster) {
        PrintDebug(2, "IterateInt: freeing helper clone %p.\n",
                   (void *)m_pPosition);
        CPosition::Free(m_pPosition);
        delete this;
    }
}
