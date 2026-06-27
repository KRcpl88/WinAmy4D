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

#include "dbase.h"

#include "bookup.h"
#include "evaluation.h"
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "safe_malloc.h"
#include "search.h"
#include "search_io.h"
#include "state_machine.h"
#include "time_ctl.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if HAVE_LIBPTHREAD
#include <pthread.h>
#endif

#if MP
#include <thread>
#include <vector>
#endif

extern unsigned long DblExt, DiscExt, SingExt;
extern int ExtendDoubleCheck, ExtendDiscoveredCheck, ExtendSingularReply;
extern unsigned long HardLimit, SoftLimit, SoftLimit2;
extern unsigned long StartTime, WallTimeStart;
extern unsigned long CurTime;
extern unsigned long FHTime;
extern bool NeedTime;
extern int MaxDepth;
extern int SearchMode;
extern CMove PBMove, PBActMove;
extern int PBHit;
extern CMove PBAltMove;
extern char BestLine[2048];
extern char ShortBestLine[2048];
extern char AnalysisLine[4096];
extern OPTIONAL_ATOMIC unsigned long TotalNodes;
extern void *IterateInt(void *x);

enum {
    Searching = 1,
    Pondering = 2,
    Puzzling = 3,
    Analyzing = 4,
    Interrupted = 5
};

#define REVERSE "\x1B[7m"
#define NORMAL "\x1B[0m"

#if MP
extern int NumberOfCores;
extern bool AbortSearch;
extern void StopHelpers(void);
extern void SetSearchThreadBackgroundPriority(void);
extern std::vector<std::thread> HelperThreads;
#endif

/*
 * Decide wether to extend the check due to the following conditions:
 *  - double check
 *  - discovered check
 *  - check with only one legal response
 *
 */

int CPosition::CheckExtend() {
    CPosition *p = this;
    int nKp = p->m_rgKingSq[p->m_nTurn].BitOffset();
    CBitBoard att;

    att = p->m_rgAtkFr[nKp] & p->m_rgMask[OPP(p->m_nTurn)][0];

    if ((att).CountBits() > 1) {

        /*
         * double check, the king has to move
         * count no. of flight squares, if only one, extend deeper
         *
         */

        CBitBoard ff;

        int i;
        int nCnt = 0;

        DblExt++;

        ff = KingEPM[nKp] & ~p->m_rgMask[p->m_nTurn][0];
        att &= p->m_SlidingPieces;

        while (att) {
            i = (att).FindSetBit();
            att.ClearLowestBit();
            ff &= ~Ray[i][nKp];
        }

        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                nCnt++;
            if (nCnt > 1)
                return ExtendDoubleCheck;
        }
    } else {
        CBitBoard ff;
        CBitBoard def;
        CBitBoard tmp;

        int nAtp = (att).FindSetBit();
        int nCnt = 0;
        int i;
        int nNd = 0;

        /* discovered check */
        if (nAtp != (p->m_pActLog - 1)->gl_Move.GetToCoord().BitOffset()) {
            DiscExt++;
            nNd = ExtendDiscoveredCheck;
        }

        ff = KingEPM[nKp] & ~p->m_rgMask[p->m_nTurn][0];

        i = (att).FindSetBit();
        if (p->m_SlidingPieces.TstBit(i))
            ff &= ~Ray[i][nKp];

        /* check for king flight squares */
        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                nCnt++;
            if (nCnt > 1)
                return nNd;
        }

        /* Find all non-pinned defenders */
        def = p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][King];

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              BishopEPM[nKp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][nKp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Rook] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              RookEPM[nKp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][nKp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        /* All non-pinned defenders are in 'def' */
        tmp = p->m_rgAtkFr[nAtp] & def;

        nCnt += (tmp).CountBits();
        if (nCnt > 1)
            return nNd;

        /* if possible, try an interposition */
        if (p->m_SlidingPieces.TstBit(nAtp)) {
            tmp = InterPath[nAtp][nKp];
            while (tmp) {
                CBitBoard tmp2;
                i = (tmp).FindSetBit();
                tmp.ClearLowestBit();
                if ((tmp2 = p->m_rgAtkFr[i] & def)) {
                    nCnt += (tmp2).CountBits();
                }
                if (p->m_nTurn == White) {
                    const int nLw = static_cast<int>(
                        CBitBoard::LEVEL_WIDTH[CSCoord(static_cast<uint16_t>(i)).m_nLevel]);
                    if ((i - nLw) > 0 && p->m_rgMask[White][Pawn].TstBit(i - nLw) &&
                        def.TstBit(i - nLw))
                        nCnt++;
                }
                if (p->m_nTurn == Black) {
                    const int nLw = static_cast<int>(
                        CBitBoard::LEVEL_WIDTH[CSCoord(static_cast<uint16_t>(i)).m_nLevel]);
                    if ((i + nLw) < static_cast<int>(CBitBoard::SIZE) &&
                        p->m_rgMask[Black][Pawn].TstBit(i + nLw) && def.TstBit(i + nLw))
                        nCnt++;
                }
                if (nCnt > 1)
                    return nNd;
            }
        }
    }

    /* If we get here, we have only one legal move. */

    SingExt++;
    return ExtendSingularReply;
}

/*
 * Compute an optimistic score for a move.
 */

int CPosition::ScoreMove(CMove move) {
    CPosition *p = this;
    int nScore = 0;

    if (move.IsCapture())
        nScore += Value[TYPE(p->m_rgPiece[move.GetToCoord().BitOffset()])];
    if (move.HasPromotion())
        nScore += Value[PromoType(move)] - Value[Pawn];
    else if (TYPE(p->m_rgPiece[move.GetFromCoord().BitOffset()]) == Pawn) {
        if (p->m_nTurn == White && move.GetToCoord().BitOffset() >= ha7) {
            nScore += Value[Bishop];
        }
        if (p->m_nTurn == Black && move.GetToCoord().BitOffset() <= hh2) {
            nScore += Value[Bishop];
        }
    }

    if (move.IsEnPassant())
        nScore += Value[Pawn];

    return nScore;
}

/**
 * Print the SAN of a move prefixed by the move number.
 */
char *CPosition::NumberedSAN(CMove move, char *pszBuffer, size_t qwLen) {
    CPosition *p = this;
    char szSanBuffer[16];
    if (p->m_nTurn == White)
        snprintf(pszBuffer, qwLen, "%d. %s", 1 + (p->m_wPly + 1) / 2,
                 p->SAN(move, szSanBuffer));
    else
        snprintf(pszBuffer, qwLen, "%d. .. %s", 1 + p->m_wPly / 2,
                 p->SAN(move, szSanBuffer));

    return pszBuffer;
}

/*
 * Analyze the hashtable to find the principal variation.
 */

void CPosition::AnaLoop(int nDepth) {
    CPosition *p = this;
    CMove move;
    bool fDummy = false;
    int nScore;

#if MP
    if (ProbeHT(p->m_ullHKey, &nScore, 0, &move, &fDummy, 0, 0, NULL) == Useless)
        return;
#else
    if (ProbeHT(p->m_ullHKey, &nScore, 0, &move, &fDummy, 0) == Useless)
        return;
#endif

    if (p->Repeated(true) >= 2)
        return;

    if (p->LegalMove(move)) {
        int nIncheck;
        char szBuffer[16];

        p->DoMove(move);
        nIncheck = p->InCheck(OPP(p->m_nTurn));
        p->UndoMove(move);

        if (p->m_nTurn == White) {
            snprintf(szBuffer, sizeof(szBuffer), "%d. ", 1 + (p->m_wPly + 1) / 2);
            strcat(BestLine, szBuffer);
        }

        if (nIncheck) {
            strcat(BestLine, "<ill>");
            strcat(ShortBestLine, "<ill>");
            return;
        }

        char *pszSan = p->SAN(move, szBuffer);
        strcat(BestLine, pszSan);
        strcat(BestLine, " ");
        strcat(ShortBestLine, pszSan);
        strcat(ShortBestLine, " ");

        /* save move to ponder on ... */
        if (nDepth == 1)
            PBMove = move;

        p->DoMove(move);
        p->AnaLoop(nDepth + 1);
        p->UndoMove(move);
    } else if (move == M_HASHED) {
        strcat(BestLine, "..");
        strcat(ShortBestLine, "..");
    } else if (move == M_NULL) {
        strcat(BestLine, "<null>");
        strcat(ShortBestLine, "<null>");
    }
}

void CPosition::AnalyzeHT(CMove move) {
    CPosition *p = this;
    p->NumberedSAN(move, BestLine, sizeof(BestLine));
    strcat(BestLine, " ");
    char szSanBuffer[16];
    strcpy(ShortBestLine, p->SAN(move, szSanBuffer));
    strcat(ShortBestLine, " ");
    p->DoMove(move);
    p->AnaLoop(1);
    p->UndoMove(move);
}

/**
 * Return the transposition-table best move for the current position, or M_NONE
 * if no usable move is stored. This recovers a principal-variation continuation
 * that a *prior* search already computed (and left in the hash table) without
 * launching a fresh search. Used by the GUI's strategy analysis to obtain the
 * recommended response from the candidate search itself rather than re-running
 * a separate search for it.
 */
CMove CPosition::ProbeBestMove() {
    CPosition *p = this;
    CMove move = M_NONE;
    int nScore;
    bool fDummy = false;

#if MP
    if (ProbeHT(p->m_ullHKey, &nScore, 0, &move, &fDummy, 0, 0, NULL) == Useless)
        return M_NONE;
#else
    if (ProbeHT(p->m_ullHKey, &nScore, 0, &move, &fDummy, 0) == Useless)
        return M_NONE;
#endif

    if (move == M_HASHED || move == M_NULL || move == M_NONE)
        return M_NONE;
    if (!p->LegalMove(move))
        return M_NONE;

    return move;
}

/**
 * The basic root iteration procedure.
 *
 * Parameters:
 *  p: the position to search
 *  score_ptr: pointer to return the root score in
 *  alternate_move: an alternate move to search
 *  alternate_score_ptr: a pointer to return the alternate score in
 */
CMove CPosition::Iterate(int *pScorePtr, CMove AlternateMove,
                         int *pAlternateScorePtr) {
    CPosition *p = this;
    float fSoft, fHard;
    int nCnt;
    CSearchData *pSd;

    FHTime = 0;

    StartTime = GetTime();
    CurTime = StartTime;
    WallTimeStart = StartTime;

    CalcTime(p, &fSoft, &fHard);

    heap_t heap = allocate_heap();
    nCnt = p->LegalMoves(heap);

    AbortSearch = false;
    NeedTime = false;

    TotalNodes = 0;

    /*
     * Check if we need to start searching at all
     */

    if (nCnt == 0) {
        free_heap(heap);
        /*
         * No legal move: the side to move is either checkmated or stalemated.
         * These paths return without running a search, so set *score_ptr
         * explicitly (mate / draw, side-to-move relative) — otherwise callers
         * that rank positions by score would read an uninitialized value.
         */
        if (!p->InCheck(p->m_nTurn)) {
            strcpy(AnalysisLine, "stalemate");
            if (pScorePtr != NULL)
                *pScorePtr = 0;
        } else {
            strcpy(AnalysisLine, "mate");
            if (pScorePtr != NULL)
                *pScorePtr = -INF;
        }
        return M_NONE;
    } else if (nCnt == 1 && SearchMode != Analyzing) {
        CMove OnlyMove = heap->data[heap->current_section->start];
        free_heap(heap);
        strcpy(AnalysisLine, "forced move");
        /*
         * A forced move is returned without searching; provide a static
         * evaluation (side-to-move relative) so score-ranking callers receive
         * a meaningful value rather than an uninitialized one.
         */
        if (pScorePtr != NULL) {
            InitEvaluation(p);
            *pScorePtr = EvaluatePosition(p);
        }
        return OnlyMove;
    }

    free_heap(heap);

    SoftLimit = StartTime + (int)(fSoft * ONE_SECOND);
    SoftLimit2 = StartTime + (int)(85 * fSoft);
    HardLimit = StartTime + (int)(fHard * ONE_SECOND);

    InitEvaluation(p);
    AgeHashTable();
    SearchHeader();

#if MP
    p->StartHelpers();
#else
    /*
     * Single-threaded build: the parallel search (helper threads) is compiled
     * out because MP is not defined. Log this once so it is clear from the log
     * that the engine is running in single-thread mode (the MP build instead
     * logs the chosen "Search threads: N" count in StartHelpers).
     */
    {
        static bool s_fSingleThreadLogged = false;
        if (!s_fSingleThreadLogged) {
            s_fSingleThreadLogged = true;
            Print(0, "Single-threaded search (MP not defined)\n");
        }
    }
#endif /* MP */

    pSd = new CSearchData(p);
    pSd->m_fMaster = true;
    pSd->m_AlternateMove = AlternateMove;

    /*
     * Publish the search data so the GUI can poll root-move progress while the
     * search runs (see CPosition::GetSearchData). Cleared before the data is
     * destroyed so a stale pointer is never read.
     */
    p->m_pSearchData = pSd;

    IterateInt(pSd);

    CMove BestMove = pSd->m_BestMove;
    if (pScorePtr != NULL) {
        *pScorePtr = pSd->m_nBestScore;
    }

    if (AlternateMove != M_NONE && pAlternateScorePtr != NULL) {
        *pAlternateScorePtr = pSd->m_nAlternateScore;
    }

    p->m_pSearchData = NULL;
    delete pSd;

#if MP
    StopHelpers();
#endif /* MP */

    return BestMove;
}

/**
 * Search the root node.
 */
void CPosition::SearchRoot() {
    CPosition *p = this;
    CMove move = M_NONE;
    CPosition *pQ;

    SearchMode = Searching;

    /* Test book first */
    if (p->m_rgwOutOfBookCnt[p->m_nTurn] < 3) {
        move = SelectBook(p);

        if (move != M_NONE) {
            char szSanBuffer[32];
            Print(1, "Book move found: %s\n",
                  p->NumberedSAN(move, szSanBuffer, sizeof(szSanBuffer)));
            p->m_rgwOutOfBookCnt[p->m_nTurn] = 0;
        } else {
            p->m_rgwOutOfBookCnt[p->m_nTurn] += 1;
        }
    }

    if (move == M_NONE) {
        pQ = CPosition::Clone(p);
        move = pQ->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(pQ);
    }

    if (move != M_NONE) {
        double dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;
        DoTC(p, (int)(dElapsed + 0.5));

        char szSanBuffer[16];
        Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
              p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
              p->SAN(move, szSanBuffer));

        if (XBoardMode)
            PrintDebug(0, "move %s\n", ICS_SAN(move));

        p->DoMove(move);
    }
}

/**
 * Do a quiescence search only. Returns the score.
 */
int CPosition::QuiescenceSearch() {
    CPosition *p = this;
    CSearchData *pSd;

    InitEvaluation(p);
    MaxDepth = MAX_TREE_SIZE - 1;

    pSd = new CSearchData(p);
    pSd->m_fMaster = true;
    pSd->InitSearch();

    int nScore = pSd->Quies(-INF, INF, 0);
    delete pSd;

    return nScore;
}

/**
 * Implements the permanent brain.
 */
int CPosition::PermanentBrain() {
    CPosition *p = this;
    if (!p->LegalMove(PBMove)) {
        CPosition *pQ;

        pQ = CPosition::Clone(p);
        SearchMode = Puzzling;
        PBAltMove = M_NONE;

        Print(2, "Puzzling over a move to ponder on...\n");
        PBMove = pQ->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(pQ);

        if (SearchMode == Interrupted) {
            return PB_NO_PB_MOVE;
        }

        if (PBAltMove != M_NONE) {
            p->DoMove(PBAltMove);
            return PB_NO_PB_HIT;
        }

        if (!p->LegalMove(PBMove)) {
            Print(0, "No PB move.\n");
            return PB_NO_PB_MOVE;
        }
    }

    if (p->LegalMove(PBMove)) {
        CMove move = M_NONE;
        CPosition *pQ;
        bool fInbook = false;
        char szSanBuffer[16];

        pQ = CPosition::Clone(p);

        PBActMove = PBMove;
        PBAltMove = M_NONE;
        PBHit = false;

        Print(0, "%s(%d): %s (in Permanent Brain)\n",
              p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
              p->SAN(PBActMove, szSanBuffer));

        pQ->DoMove(PBActMove);

        if (pQ->m_rgwOutOfBookCnt[pQ->m_nTurn] < 3) {
            move = SelectBook(pQ);
            if (move != M_NONE) {
                PBHit = false;
                PBAltMove = M_NONE;
               fInbook = true;
            }
        }

        SearchMode = Pondering;

        if (!fInbook) {
            move = pQ->Iterate(NULL, M_NONE, NULL);
        }

        CPosition::Free(pQ);

        if (SearchMode == Interrupted) {
            return PB_NO_PB_MOVE;
        }

        if (PBHit) {
            double dElapsed =
                (double)(CurTime - WallTimeStart) / (double)ONE_SECOND;
            Print(2, "PB Hit! (elapsed %g secs)\n", dElapsed);

            Print(0, "%s(%d): %s\n", p->m_nTurn == White ? "White" : "Black",
                  (p->m_wPly / 2) + 1, p->SAN(PBActMove, szSanBuffer));

            p->DoMove(PBActMove);
            DoTC(p, (int)(dElapsed + 0.5));

            Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
                  p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
                  p->SAN(move, szSanBuffer));

            if (XBoardMode) {
                PrintDebug(0, "move %s\n", ICS_SAN(move));
            }

            p->DoMove(move);

            return PB_HIT;
        } else if (!PBHit && PBAltMove != M_NONE) {
            Print(2, "PB not Hit! Alternate move is %s\n",
                  p->SAN(PBAltMove, szSanBuffer));

            p->DoMove(PBAltMove);

            return PB_NO_PB_HIT;
        }
    }

    return PB_NO_PB_MOVE;
}

/**
 * Analysis mode for xboard.
 */
void CPosition::AnalysisMode() {
    CPosition *p = this;
    CPosition *pQ;

    SearchMode = Analyzing;

    pQ = CPosition::Clone(p);
    pQ->Iterate(NULL, M_NONE, NULL);
    CPosition::Free(pQ);
}

#if MP
/*
 * In parallel search start up all helper threads
 */

void CPosition::StartHelpers() {
    CPosition *p = this;
    int nHelperThreads;
    int nthread;

    if (!HelperThreads.empty()) {
        StopHelpers();
    }

    /*
     * Default the number of search threads to the number of logical CPUs when
     * it has not been set explicitly (e.g. via -cpu or .amyrc). This lets the
     * parallel search engage for both the console engine and the GUI without
     * any extra configuration.
     */
    if (NumberOfCores <= 0) {
        NumberOfCores = (int)std::thread::hardware_concurrency();
        if (NumberOfCores <= 0) {
            NumberOfCores = 1;
        }
    }

    if (NumberOfCores > MAX_SEARCH_THREADS) {
        NumberOfCores = MAX_SEARCH_THREADS;
    }

    nHelperThreads = NumberOfCores - 1;
    if (nHelperThreads < 0) {
        nHelperThreads = 0;
    }

    Print(0, "StartHelpers, %d cores, %d helper threads.\n\n",
          NumberOfCores, nHelperThreads);

    if (NumberOfCores < 2) {
        return;
    }

    /*
     * Start up the helper threads. Each helper searches its own clone of the
     * position; results are shared through the global transposition table.
     * Helpers run at background priority so the GUI / UI thread stays
     * responsive.
     */

    HelperThreads.reserve(NumberOfCores - 1);
    for (nthread = 0; nthread < (NumberOfCores - 1); nthread++) {
        CSearchData *pSd = new CSearchData(CPosition::Clone(p));
        pSd->m_fMaster = false;
        PrintDebug(2, "StartHelpers: created helper clone %p for thread %d.\n",
                   (void *)pSd->m_pPosition, nthread);
        HelperThreads.emplace_back([pSd]() {
            SetSearchThreadBackgroundPriority();
            IterateInt(pSd);
        });
    }
}
#endif /* MP */
