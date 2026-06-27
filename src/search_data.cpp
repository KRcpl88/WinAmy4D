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
    CSearchData *pSd = this;
    memset(pSd, 0, sizeof(*pSd));

    pSd->m_pPosition = p;
    pSd->m_pStatusTable =
        (struct SSearchStatus *)safe_calloc(MAX_TREE_SIZE,
                                           sizeof(struct SSearchStatus));
    pSd->m_pCurrent = pSd->m_pStatusTable;
    pSd->m_pKillerTable =
        (struct SKillerEntry *)safe_calloc(MAX_TREE_SIZE,
                                          sizeof(struct SKillerEntry));
    pSd->m_pKiller = pSd->m_pKillerTable;

    pSd->m_hHeap = allocate_heap();

    pSd->m_pnDataHeap = NULL;
    pSd->m_uDataHeapSize = 0;

#if MP
    pSd->m_pLocalHashTable =
        (struct HTEntry *)safe_calloc(sizeof(struct HTEntry), L_HT_Size);
    pSd->m_hDeferredHeap = allocate_heap();
#endif

    pSd->m_wPly = 0;

}

/**
 * Description: Releases all heap/table resources owned by this search context.
 * Inputs: None.
 * Outputs: Frees all dynamically allocated members.
 */
CSearchData::~CSearchData() {
    CSearchData *pSd = this;
    free(pSd->m_pStatusTable);
    free(pSd->m_pKillerTable);
    free(pSd->m_pnDataHeap);
    free_heap(pSd->m_hHeap);

#if MP
    free(pSd->m_pLocalHashTable);
    free_heap(pSd->m_hDeferredHeap);
#endif

}

/**
 * Description: Enters one search ply and initializes phase state for move generation at that ply.
 * Inputs: None.
 * Outputs: Increments ply state and pushes heap sections.
 */
void CSearchData::EnterNode() {
    CSearchData *pSd = this;
    struct SSearchStatus *pSt;

    pSt = ++(pSd->m_pCurrent);

    pSt->st_phase = HashMove;
    pSd->m_wPly++;
    pSd->m_pKiller++;

    push_section(pSd->m_hHeap);
#if MP
    push_section(pSd->m_hDeferredHeap);
#endif
}

/**
 * Description: Leaves one search ply and restores parent search state.
 * Inputs: None.
 * Outputs: Pops heap sections and decrements ply state.
 */
void CSearchData::LeaveNode() {
    CSearchData *pSd = this;
    pop_section(pSd->m_hHeap);
    pSd->m_pCurrent--;
    pSd->m_pKiller--;
    pSd->m_wPly--;
#if MP
    pop_section(pSd->m_hDeferredHeap);
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
    CSearchData *pSd = this;
    heap_section_t pSection = pSd->m_hHeap->current_section;
    struct SSearchStatus *pSt = pSd->m_pCurrent;
    CPosition *p = pSd->m_pPosition;
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

            p->GenTo(to, pSd->m_hHeap);
        }

        CBitBoard PromotingPawns =
            p->GetMask(p->GetTurn(), Pawn) & PrePromoRank[p->GetTurn()];
        while (PromotingPawns) {
            CSCoord from = (PromotingPawns).FindSetBitCoord();
            PromotingPawns.ClearLowestBit();

            p->GenFrom(from, pSd->m_hHeap);
        }

        GrowDataHeap(pSd);
        for (unsigned int j = pSection->start; j < pSection->end; j++) {
            pSd->m_pnDataHeap[j] = SwapOff(p, pSd->m_hHeap->data[j]);
        }

        unsigned int dwLastEnd = pSection->end;
        p->GenEnpas(pSd->m_hHeap);
        GrowDataHeap(pSd);

        for (unsigned int j = dwLastEnd; j < pSection->end; j++) {
            pSd->m_pnDataHeap[j] = 0;
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
            int nBest = pSd->m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (pSd->m_pnDataHeap[i] > nBest) {
                    nBest = pSd->m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            if (nBest >= 0) {
                move = pSd->m_hHeap->data[dwBestI];
                pSection->end--;

                pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];
                pSd->m_pnDataHeap[dwBestI] = pSd->m_pnDataHeap[pSection->end];

                if (move == pSt->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
    /* fall through */
    case Killer1: {
        move = pSd->m_pKiller->killer1;
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
        move = pSd->m_pKiller->killer2;
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
            move = pSd->m_rgCounterTab[p->GetTurn()][LastMove.GetFromCoord().BitOffset()][LastMove.GetToCoord().BitOffset()];

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
        if (pSd->m_wPly >= 2) {
            move = (pSd->m_pKiller - 2)->killer1;

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_cm || !p->LegalMove(move))
                move = (pSd->m_pKiller - 2)->killer2;

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
            int nBest = pSd->m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (pSd->m_pnDataHeap[i] > nBest) {
                    nBest = pSd->m_pnDataHeap[i];
                    dwBestI = i;
                }
            }

            move = pSd->m_hHeap->data[dwBestI];
            pSection->end--;

            pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];
            pSd->m_pnDataHeap[dwBestI] = pSd->m_pnDataHeap[pSection->end];

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
            append_to_heap(pSd->m_hHeap,
                           make_move(p->GetTurn() == White ? CASTLE_E1 : CASTLE_E8,
                                     p->GetTurn() == White ? CASTLE_G1 : CASTLE_G8, M_SCASTLE));
        }
        if (p->GetCastle() & CastleMask[p->GetTurn()][1]) {
            append_to_heap(pSd->m_hHeap,
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
                append_to_heap(pSd->m_hHeap, make_move(nFrom, nTo, 0));
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
                append_to_heap(pSd->m_hHeap, make_move(fromCoord, toCoord, 0));
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
                            append_to_heap(pSd->m_hHeap, make_move(fromCoord, dblCoord, M_PAWND));
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
                pSd->m_rguHistoryTab[p->GetTurn()][pSd->m_hHeap->data[nBestI].GetFromCoord().BitOffset()][pSd->m_hHeap->data[nBestI].GetToCoord().BitOffset()];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                int nHval = pSd->m_rguHistoryTab[p->GetTurn()]
                                        [pSd->m_hHeap->data[i].GetFromCoord().BitOffset()]
                                        [pSd->m_hHeap->data[i].GetToCoord().BitOffset()];
                if (nHval > nBest) {
                    nBest = nHval;
                    nBestI = i;
                }
            }
            move = pSd->m_hHeap->data[nBestI];

            pSection->end--;
            pSd->m_hHeap->data[nBestI] = pSd->m_hHeap->data[pSection->end];

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
    CSearchData *pSd = this;
    heap_section_t pSection = pSd->m_hHeap->current_section;
    struct SSearchStatus *pSt = pSd->m_pCurrent;
    CPosition *p = pSd->m_pPosition;
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
            p->GenTo(to, pSd->m_hHeap);
        }

        GrowDataHeap(pSd);
        for (unsigned int j = pSection->start; j < pSection->end; j++) {
            pSd->m_pnDataHeap[j] = SwapOff(p, pSd->m_hHeap->data[j]);
        }

        unsigned int dwLastEnd = pSection->end;
        p->GenEnpas(pSd->m_hHeap);
        GrowDataHeap(pSd);

        for (unsigned int j = dwLastEnd; j < pSection->end; j++) {
            pSd->m_pnDataHeap[j] = 0;
        }
    }
        /* fall through */
    case GainingCapture:
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = pSd->m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (pSd->m_pnDataHeap[i] > nBest) {
                    nBest = pSd->m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            if (nBest >= 0) {
                move = pSd->m_hHeap->data[dwBestI];
                pSection->end--;

                pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];
                pSd->m_pnDataHeap[dwBestI] = pSd->m_pnDataHeap[pSection->end];

                pSt->st_phase = GainingCapture;

                if (move == pSt->st_hashmove)
                    continue;

                return move;
            } else
                break;
        }
        /* fall through */
    case Killer1: {
        move = pSd->m_pKiller->killer1;
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
        move = pSd->m_pKiller->killer2;
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
            move = pSd->m_rgCounterTab[p->GetTurn()][LastMove.GetFromCoord().BitOffset()][LastMove.GetToCoord().BitOffset()];

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
        if (pSd->m_wPly >= 2) {
            move = (pSd->m_pKiller - 2)->killer1;

            if (move == pSt->st_hashmove || move == pSt->st_k1 ||
                move == pSt->st_k2 || move == pSt->st_cm || !p->LegalMove(move))
                move = (pSd->m_pKiller - 2)->killer2;

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
            int nBest = pSd->m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (pSd->m_pnDataHeap[i] > nBest) {
                    nBest = pSd->m_pnDataHeap[i];
                    dwBestI = i;
                }
            }
            move = pSd->m_hHeap->data[dwBestI];

            pSection->end--;
            pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];
            pSd->m_pnDataHeap[dwBestI] = pSd->m_pnDataHeap[pSection->end];

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
                append_to_heap(pSd->m_hHeap, make_move(nKp, nTo, 0));
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
                append_to_heap(pSd->m_hHeap, make_move(nFrom, nTo, 0));
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
                    append_to_heap(pSd->m_hHeap, make_promotion(fromCoord, toCoord, Queen, 0));
                    append_to_heap(pSd->m_hHeap, make_promotion(fromCoord, toCoord, Knight, 0));
                    append_to_heap(pSd->m_hHeap, make_promotion(fromCoord, toCoord, Rook, 0));
                    append_to_heap(pSd->m_hHeap, make_promotion(fromCoord, toCoord, Bishop, 0));
                } else if (pawn_may_move_to(toCoord)) {
                    append_to_heap(pSd->m_hHeap, make_move(fromCoord, toCoord, 0));
                    const int nHomeRank = (p->GetTurn() == White) ? 1 : (nWidth - 2);
                    if (fromCoord.m_nLevel == MAIN_LEVEL &&
                        static_cast<int>(fromCoord.m_nRank) == nHomeRank) {
                        const int nDblRank = static_cast<int>(fromCoord.m_nRank) + 2 * nDirection;
                        if (nDblRank >= 0 && nDblRank < nWidth) {
                            CSCoord dblCoord(fromCoord.m_nLevel, fromCoord.m_nFile,
                                            static_cast<uint16_t>(nDblRank));
                            if (!is_promo_square(dblCoord) &&
                                p->GetPiece(dblCoord.BitOffset()) == Neutral) {
                                append_to_heap(pSd->m_hHeap, make_move(fromCoord, dblCoord, M_PAWND));
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
                pSd->m_rguHistoryTab[p->GetTurn()][pSd->m_hHeap->data[dwBestI].GetFromCoord().BitOffset()][pSd->m_hHeap->data[dwBestI].GetToCoord().BitOffset()];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                int nHval = pSd->m_rguHistoryTab[p->GetTurn()]
                                        [pSd->m_hHeap->data[i].GetFromCoord().BitOffset()]
                                        [pSd->m_hHeap->data[i].GetToCoord().BitOffset()];
                if (nHval > nBest) {
                    nBest = nHval;
                    dwBestI = i;
                }
            }
            move = pSd->m_hHeap->data[dwBestI];

            pSection->end--;
            pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];

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
    CSearchData *pSd = this;
    heap_section_t pSection = pSd->m_hHeap->current_section;
    struct SSearchStatus *pSt = pSd->m_pCurrent;
    CMove move;

    switch (pSt->st_phase) {
    case HashMove:
    case GenerateCaptures:
#ifdef VERBOSE
        Print(9, "GenerateCaptures\n");
#endif
        GenerateQCaptures(pSd, nAlpha);
        pSt->st_phase = GainingCapture;

        /* fall through */
    case GainingCapture:
#ifdef VERBOSE
        Print(9, "GainingCapture\n");
#endif
        while (pSection->end > pSection->start) {
            unsigned int dwBestI = pSection->start;
            int nBest = pSd->m_pnDataHeap[dwBestI];

            for (unsigned int i = pSection->start + 1; i < pSection->end; i++) {
                if (pSd->m_pnDataHeap[i] > nBest) {
                    nBest = pSd->m_pnDataHeap[i];
                    dwBestI = i;
                }
            }

            move = pSd->m_hHeap->data[dwBestI];
            pSection->end--;
            pSd->m_hHeap->data[dwBestI] = pSd->m_hHeap->data[pSection->end];
            pSd->m_pnDataHeap[dwBestI] = pSd->m_pnDataHeap[pSection->end];

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
    CSearchData *pSd = this;
    struct SKillerEntry *pK = pSd->m_pKiller;

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
