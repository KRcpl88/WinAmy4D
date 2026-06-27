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
    int nKp = m_rgKingSq[m_nTurn].BitOffset();
    CBitBoard att;

    att = m_rgAtkFr[nKp] & m_rgMask[OPP(m_nTurn)][0];

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

        ff = KingEPM[nKp] & ~m_rgMask[m_nTurn][0];
        att &= m_SlidingPieces;

        while (att) {
            i = (att).FindSetBit();
            att.ClearLowestBit();
            ff &= ~Ray[i][nKp];
        }

        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(m_rgAtkFr[i] & m_rgMask[OPP(m_nTurn)][0]))
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
        if (nAtp != (m_pActLog - 1)->gl_Move.GetToCoord().BitOffset()) {
            DiscExt++;
            nNd = ExtendDiscoveredCheck;
        }

        ff = KingEPM[nKp] & ~m_rgMask[m_nTurn][0];

        i = (att).FindSetBit();
        if (m_SlidingPieces.TstBit(i))
            ff &= ~Ray[i][nKp];

        /* check for king flight squares */
        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(m_rgAtkFr[i] & m_rgMask[OPP(m_nTurn)][0]))
                nCnt++;
            if (nCnt > 1)
                return nNd;
        }

        /* Find all non-pinned defenders */
        def = m_rgMask[m_nTurn][0] & ~m_rgMask[m_nTurn][King];

        tmp = (m_rgMask[OPP(m_nTurn)][Bishop] | m_rgMask[OPP(m_nTurn)][Queen]) &
              BishopEPM[nKp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][nKp];
            if (tmp2 && !(m_rgMask[OPP(m_nTurn)][0] & tmp2)) {
                tmp2 &= m_rgMask[m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        tmp = (m_rgMask[OPP(m_nTurn)][Rook] | m_rgMask[OPP(m_nTurn)][Queen]) &
              RookEPM[nKp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][nKp];
            if (tmp2 && !(m_rgMask[OPP(m_nTurn)][0] & tmp2)) {
                tmp2 &= m_rgMask[m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        /* All non-pinned defenders are in 'def' */
        tmp = m_rgAtkFr[nAtp] & def;

        nCnt += (tmp).CountBits();
        if (nCnt > 1)
            return nNd;

        /* if possible, try an interposition */
        if (m_SlidingPieces.TstBit(nAtp)) {
            tmp = InterPath[nAtp][nKp];
            while (tmp) {
                CBitBoard tmp2;
                i = (tmp).FindSetBit();
                tmp.ClearLowestBit();
                if ((tmp2 = m_rgAtkFr[i] & def)) {
                    nCnt += (tmp2).CountBits();
                }
                if (m_nTurn == White) {
                    const int nLw = static_cast<int>(
                        CBitBoard::LEVEL_WIDTH[CSCoord(static_cast<uint16_t>(i)).m_nLevel]);
                    if ((i - nLw) > 0 && m_rgMask[White][Pawn].TstBit(i - nLw) &&
                        def.TstBit(i - nLw))
                        nCnt++;
                }
                if (m_nTurn == Black) {
                    const int nLw = static_cast<int>(
                        CBitBoard::LEVEL_WIDTH[CSCoord(static_cast<uint16_t>(i)).m_nLevel]);
                    if ((i + nLw) < static_cast<int>(CBitBoard::SIZE) &&
                        m_rgMask[Black][Pawn].TstBit(i + nLw) && def.TstBit(i + nLw))
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
    int nScore = 0;

    if (move.IsCapture())
        nScore += Value[TYPE(m_rgPiece[move.GetToCoord().BitOffset()])];
    if (move.HasPromotion())
        nScore += Value[PromoType(move)] - Value[Pawn];
    else if (TYPE(m_rgPiece[move.GetFromCoord().BitOffset()]) == Pawn) {
        if (m_nTurn == White && move.GetToCoord().BitOffset() >= ha7) {
            nScore += Value[Bishop];
        }
        if (m_nTurn == Black && move.GetToCoord().BitOffset() <= hh2) {
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
    char szSanBuffer[16];
    if (m_nTurn == White)
        snprintf(pszBuffer, qwLen, "%d. %s", 1 + (m_wPly + 1) / 2,
                 SAN(move, szSanBuffer));
    else
        snprintf(pszBuffer, qwLen, "%d. .. %s", 1 + m_wPly / 2,
                 SAN(move, szSanBuffer));

    return pszBuffer;
}

/*
 * Analyze the hashtable to find the principal variation.
 */

void CPosition::AnaLoop(int nDepth) {
    CMove move;
    bool fDummy = false;
    int nScore;

#if MP
    if (ProbeHT(m_ullHKey, &nScore, 0, &move, &fDummy, 0, 0, NULL) == Useless)
        return;
#else
    if (ProbeHT(m_ullHKey, &nScore, 0, &move, &fDummy, 0) == Useless)
        return;
#endif

    if (Repeated(true) >= 2)
        return;

    if (LegalMove(move)) {
        int nIncheck;
        char szBuffer[16];

        DoMove(move);
        nIncheck = InCheck(OPP(m_nTurn));
        UndoMove(move);

        if (m_nTurn == White) {
            snprintf(szBuffer, sizeof(szBuffer), "%d. ", 1 + (m_wPly + 1) / 2);
            strcat(BestLine, szBuffer);
        }

        if (nIncheck) {
            strcat(BestLine, "<ill>");
            strcat(ShortBestLine, "<ill>");
            return;
        }

        char *pszSan = SAN(move, szBuffer);
        strcat(BestLine, pszSan);
        strcat(BestLine, " ");
        strcat(ShortBestLine, pszSan);
        strcat(ShortBestLine, " ");

        /* save move to ponder on ... */
        if (nDepth == 1)
            PBMove = move;

        DoMove(move);
        AnaLoop(nDepth + 1);
        UndoMove(move);
    } else if (move == M_HASHED) {
        strcat(BestLine, "..");
        strcat(ShortBestLine, "..");
    } else if (move == M_NULL) {
        strcat(BestLine, "<null>");
        strcat(ShortBestLine, "<null>");
    }
}

void CPosition::AnalyzeHT(CMove move) {
    NumberedSAN(move, BestLine, sizeof(BestLine));
    strcat(BestLine, " ");
    char szSanBuffer[16];
    strcpy(ShortBestLine, SAN(move, szSanBuffer));
    strcat(ShortBestLine, " ");
    DoMove(move);
    AnaLoop(1);
    UndoMove(move);
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
    CMove move = M_NONE;
    int nScore;
    bool fDummy = false;

#if MP
    if (ProbeHT(m_ullHKey, &nScore, 0, &move, &fDummy, 0, 0, NULL) == Useless)
        return M_NONE;
#else
    if (ProbeHT(m_ullHKey, &nScore, 0, &move, &fDummy, 0) == Useless)
        return M_NONE;
#endif

    if (move == M_HASHED || move == M_NULL || move == M_NONE)
        return M_NONE;
    if (!LegalMove(move))
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
    float fSoft, fHard;
    int nCnt;
    CSearchData *pSd;

    FHTime = 0;

    StartTime = GetTime();
    CurTime = StartTime;
    WallTimeStart = StartTime;

    CalcTime(this, &fSoft, &fHard);

    heap_t heap = allocate_heap();
    nCnt = LegalMoves(heap);

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
        if (!InCheck(m_nTurn)) {
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
            InitEvaluation(this);
            *pScorePtr = EvaluatePosition(this);
        }
        return OnlyMove;
    }

    free_heap(heap);

    SoftLimit = StartTime + (int)(fSoft * ONE_SECOND);
    SoftLimit2 = StartTime + (int)(85 * fSoft);
    HardLimit = StartTime + (int)(fHard * ONE_SECOND);

    InitEvaluation(this);
    AgeHashTable();
    SearchHeader();

#if MP
    StartHelpers();
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

    pSd = new CSearchData(this);
    pSd->m_fMaster = true;
    pSd->m_AlternateMove = AlternateMove;

    /*
     * Publish the search data so the GUI can poll root-move progress while the
     * search runs (see CPosition::GetSearchData). Cleared before the data is
     * destroyed so a stale pointer is never read.
     */
    m_pSearchData = pSd;

    pSd->IterateInt();

    CMove BestMove = pSd->m_BestMove;
    if (pScorePtr != NULL) {
        *pScorePtr = pSd->m_nBestScore;
    }

    if (AlternateMove != M_NONE && pAlternateScorePtr != NULL) {
        *pAlternateScorePtr = pSd->m_nAlternateScore;
    }

    m_pSearchData = NULL;
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
    CMove move = M_NONE;
    CPosition *pQ;

    SearchMode = Searching;

    /* Test book first */
    if (m_rgwOutOfBookCnt[m_nTurn] < 3) {
        move = SelectBook(this);

        if (move != M_NONE) {
            char szSanBuffer[32];
            Print(1, "Book move found: %s\n",
                  NumberedSAN(move, szSanBuffer, sizeof(szSanBuffer)));
            m_rgwOutOfBookCnt[m_nTurn] = 0;
        } else {
            m_rgwOutOfBookCnt[m_nTurn] += 1;
        }
    }

    if (move == M_NONE) {
        pQ = CPosition::Clone(this);
        move = pQ->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(pQ);
    }

    if (move != M_NONE) {
        double dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;
        DoTC(this, (int)(dElapsed + 0.5));

        char szSanBuffer[16];
        Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
              m_nTurn == White ? "White" : "Black", (m_wPly / 2) + 1,
              SAN(move, szSanBuffer));

        if (XBoardMode)
            PrintDebug(0, "move %s\n", ICS_SAN(move));

        DoMove(move);
    }
}

/**
 * Do a quiescence search only. Returns the score.
 */
int CPosition::QuiescenceSearch() {
    CSearchData *pSd;

    InitEvaluation(this);
    MaxDepth = MAX_TREE_SIZE - 1;

    pSd = new CSearchData(this);
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
    if (!LegalMove(PBMove)) {
        CPosition *pQ;

        pQ = CPosition::Clone(this);
        SearchMode = Puzzling;
        PBAltMove = M_NONE;

        Print(2, "Puzzling over a move to ponder on...\n");
        PBMove = pQ->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(pQ);

        if (SearchMode == Interrupted) {
            return PB_NO_PB_MOVE;
        }

        if (PBAltMove != M_NONE) {
            DoMove(PBAltMove);
            return PB_NO_PB_HIT;
        }

        if (!LegalMove(PBMove)) {
            Print(0, "No PB move.\n");
            return PB_NO_PB_MOVE;
        }
    }

    if (LegalMove(PBMove)) {
        CMove move = M_NONE;
        CPosition *pQ;
        bool fInbook = false;
        char szSanBuffer[16];

        pQ = CPosition::Clone(this);

        PBActMove = PBMove;
        PBAltMove = M_NONE;
        PBHit = false;

        Print(0, "%s(%d): %s (in Permanent Brain)\n",
              m_nTurn == White ? "White" : "Black", (m_wPly / 2) + 1,
              SAN(PBActMove, szSanBuffer));

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

            Print(0, "%s(%d): %s\n", m_nTurn == White ? "White" : "Black",
                  (m_wPly / 2) + 1, SAN(PBActMove, szSanBuffer));

            DoMove(PBActMove);
            DoTC(this, (int)(dElapsed + 0.5));

            Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
                  m_nTurn == White ? "White" : "Black", (m_wPly / 2) + 1,
                  SAN(move, szSanBuffer));

            if (XBoardMode) {
                PrintDebug(0, "move %s\n", ICS_SAN(move));
            }

            DoMove(move);

            return PB_HIT;
        } else if (!PBHit && PBAltMove != M_NONE) {
            Print(2, "PB not Hit! Alternate move is %s\n",
                  SAN(PBAltMove, szSanBuffer));

            DoMove(PBAltMove);

            return PB_NO_PB_HIT;
        }
    }

    return PB_NO_PB_MOVE;
}

/**
 * Analysis mode for xboard.
 */
void CPosition::AnalysisMode() {
    CPosition *pQ;

    SearchMode = Analyzing;

    pQ = CPosition::Clone(this);
    pQ->Iterate(NULL, M_NONE, NULL);
    CPosition::Free(pQ);
}

#if MP
/*
 * In parallel search start up all helper threads
 */

void CPosition::StartHelpers() {
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
        CSearchData *pSd = new CSearchData(CPosition::Clone(this));
        pSd->m_fMaster = false;
        PrintDebug(2, "StartHelpers: created helper clone %p for thread %d.\n",
                   (void *)pSd->m_pPosition, nthread);
        HelperThreads.emplace_back([pSd]() {
            SetSearchThreadBackgroundPriority();
            pSd->IterateInt();
        });
    }
}
#endif /* MP */
