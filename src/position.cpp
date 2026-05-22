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
extern int NumberOfCPUs;
extern bool AbortSearch;
extern void StopHelpers(void);
#if HAVE_LIBPTHREAD
extern pthread_t *tids;
#endif
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
    int kp = p->m_rgKingSq[p->m_nTurn].BitOffset();
    CBitBoard att;

    att = p->m_rgAtkFr[kp] & p->m_rgMask[OPP(p->m_nTurn)][0];

    if ((att).CountBits() > 1) {

        /*
         * double check, the king has to move
         * count no. of flight squares, if only one, extend deeper
         *
         */

        CBitBoard ff;

        int i;
        int cnt = 0;

        DblExt++;

        ff = KingEPM[kp] & ~p->m_rgMask[p->m_nTurn][0];
        att &= p->m_SlidingPieces;

        while (att) {
            i = (att).FindSetBit();
            att.ClearLowestBit();
            ff &= ~Ray[i][kp];
        }

        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                cnt++;
            if (cnt > 1)
                return ExtendDoubleCheck;
        }
    } else {
        CBitBoard ff;
        CBitBoard def;
        CBitBoard tmp;

        int atp = (att).FindSetBit();
        int cnt = 0;
        int i;
        int nd = 0;

        /* discovered check */
        if (atp != (p->m_pActLog - 1)->gl_Move.GetToCoord().BitOffset()) {
            DiscExt++;
            nd = ExtendDiscoveredCheck;
        }

        ff = KingEPM[kp] & ~p->m_rgMask[p->m_nTurn][0];

        i = (att).FindSetBit();
        if (p->m_SlidingPieces.TstBit(i))
            ff &= ~Ray[i][kp];

        /* check for king flight squares */
        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                cnt++;
            if (cnt > 1)
                return nd;
        }

        /* Find all non-pinned defenders */
        def = p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][King];

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              BishopEPM[kp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][kp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Rook] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              RookEPM[kp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][kp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        /* All non-pinned defenders are in 'def' */
        tmp = p->m_rgAtkFr[atp] & def;

        cnt += (tmp).CountBits();
        if (cnt > 1)
            return nd;

        /* if possible, try an interposition */
        if (p->m_SlidingPieces.TstBit(atp)) {
            tmp = InterPath[atp][kp];
            while (tmp) {
                CBitBoard tmp2;
                i = (tmp).FindSetBit();
                tmp.ClearLowestBit();
                if ((tmp2 = p->m_rgAtkFr[i] & def)) {
                    cnt += (tmp2).CountBits();
                }
                if (p->m_nTurn == White && (i - 8) > 0 &&
                    p->m_rgMask[White][Pawn].TstBit(i - 8) && def.TstBit(i - 8))
                    cnt++;
                if (p->m_nTurn == Black && (i + 8) < static_cast<int>(CBitBoard::SIZE) &&
                    p->m_rgMask[Black][Pawn].TstBit(i + 8) && def.TstBit(i + 8))
                    cnt++;
                if (cnt > 1)
                    return nd;
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
    int score = 0;

    if (move.IsCapture())
        score += Value[TYPE(p->m_rgPiece[move.GetToCoord().BitOffset()])];
    if (move.HasPromotion())
        score += Value[PromoType(move)] - Value[Pawn];
    else if (TYPE(p->m_rgPiece[move.GetFromCoord().BitOffset()]) == Pawn) {
        if (p->m_nTurn == White && move.GetToCoord().BitOffset() >= a7) {
            score += Value[Bishop];
        }
        if (p->m_nTurn == Black && move.GetToCoord().BitOffset() <= h2) {
            score += Value[Bishop];
        }
    }

    if (move.IsEnPassant())
        score += Value[Pawn];

    return score;
}

/**
 * Print the SAN of a move prefixed by the move number.
 */
char *CPosition::NumberedSAN(CMove move, char *buffer, size_t len) {
    CPosition *p = this;
    char san_buffer[16];
    if (p->m_nTurn == White)
        snprintf(buffer, len, "%d. %s", 1 + (p->m_wPly + 1) / 2,
                 p->SAN(move, san_buffer));
    else
        snprintf(buffer, len, "%d. .. %s", 1 + p->m_wPly / 2,
                 p->SAN(move, san_buffer));

    return buffer;
}

/*
 * Analyze the hashtable to find the principal variation.
 */

void CPosition::AnaLoop(int depth) {
    CPosition *p = this;
    CMove move;
    bool dummy = false;
    int score;

#if MP
    if (ProbeHT(p->m_ullHKey, &score, 0, &move, &dummy, 0, 0, NULL) == Useless)
        return;
#else
    if (ProbeHT(p->m_ullHKey, &score, 0, &move, &dummy, 0) == Useless)
        return;
#endif

    if (p->Repeated(true) >= 2)
        return;

    if (p->LegalMove(move)) {
        int incheck;
        char buffer[16];

        p->DoMove(move);
        incheck = p->InCheck(OPP(p->m_nTurn));
        p->UndoMove(move);

        if (p->m_nTurn == White) {
            snprintf(buffer, sizeof(buffer), "%d. ", 1 + (p->m_wPly + 1) / 2);
            strcat(BestLine, buffer);
        }

        if (incheck) {
            strcat(BestLine, "<ill>");
            strcat(ShortBestLine, "<ill>");
            return;
        }

        char *san = p->SAN(move, buffer);
        strcat(BestLine, san);
        strcat(BestLine, " ");
        strcat(ShortBestLine, san);
        strcat(ShortBestLine, " ");

        /* save move to ponder on ... */
        if (depth == 1)
            PBMove = move;

        p->DoMove(move);
        p->AnaLoop(depth + 1);
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
    char san_buffer[16];
    strcpy(ShortBestLine, p->SAN(move, san_buffer));
    strcat(ShortBestLine, " ");
    p->DoMove(move);
    p->AnaLoop(1);
    p->UndoMove(move);
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
CMove CPosition::Iterate(int *score_ptr, CMove alternate_move,
                         int *alternate_score_ptr) {
    CPosition *p = this;
    float soft, hard;
    int cnt;
    CSearchData *sd;

    FHTime = 0;

    StartTime = GetTime();
    CurTime = StartTime;
    WallTimeStart = StartTime;

    CalcTime(p, &soft, &hard);

    heap_t heap = allocate_heap();
    cnt = p->LegalMoves(heap);

    AbortSearch = false;
    NeedTime = false;

    TotalNodes = 0;

    /*
     * Check if we need to start searching at all
     */

    if (cnt == 0) {
        free_heap(heap);
        if (!p->InCheck(p->m_nTurn))
            strcpy(AnalysisLine, "stalemate");
        else
            strcpy(AnalysisLine, "mate");
        return M_NONE;
    } else if (cnt == 1 && SearchMode != Analyzing) {
        CMove only_move = heap->data[heap->current_section->start];
        free_heap(heap);
        strcpy(AnalysisLine, "forced move");
        return only_move;
    }

    free_heap(heap);

    SoftLimit = StartTime + (int)(soft * ONE_SECOND);
    SoftLimit2 = StartTime + (int)(85 * soft);
    HardLimit = StartTime + (int)(hard * ONE_SECOND);

    InitEvaluation(p);
    AgeHashTable();
    SearchHeader();

#if MP
    p->StartHelpers();
#endif /* MP */

    sd = new CSearchData(p);
    sd->m_fMaster = true;
    sd->m_AlternateMove = alternate_move;
    IterateInt(sd);

    CMove best_move = sd->m_BestMove;
    if (score_ptr != NULL) {
        *score_ptr = sd->m_nBestScore;
    }

    if (alternate_move != M_NONE && alternate_score_ptr != NULL) {
        *alternate_score_ptr = sd->m_nAlternateScore;
    }

    delete sd;

#if MP
    StopHelpers();
#endif /* MP */

    return best_move;
}

/**
 * Search the root node.
 */
void CPosition::SearchRoot() {
    CPosition *p = this;
    CMove move = M_NONE;
    CPosition *q;

    SearchMode = Searching;

    /* Test book first */
    if (p->m_rgwOutOfBookCnt[p->m_nTurn] < 3) {
        move = SelectBook(p);

        if (move != M_NONE) {
            char san_buffer[32];
            Print(1, "Book move found: %s\n",
                  p->NumberedSAN(move, san_buffer, sizeof(san_buffer)));
            p->m_rgwOutOfBookCnt[p->m_nTurn] = 0;
        } else {
            p->m_rgwOutOfBookCnt[p->m_nTurn] += 1;
        }
    }

    if (move == M_NONE) {
        q = CPosition::Clone(p);
        move = q->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(q);
    }

    if (move != M_NONE) {
        double elapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;
        DoTC(p, (int)(elapsed + 0.5));

        char san_buffer[16];
        Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
              p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
              p->SAN(move, san_buffer));

        if (XBoardMode)
            PrintNoLog(0, "move %s\n", ICS_SAN(move));

        p->DoMove(move);
    }
}

/**
 * Do a quiescence search only. Returns the score.
 */
int CPosition::QuiescenceSearch() {
    CPosition *p = this;
    CSearchData *sd;

    InitEvaluation(p);
    MaxDepth = MAX_TREE_SIZE - 1;

    sd = new CSearchData(p);
    sd->m_fMaster = true;
    sd->InitSearch();

    int score = sd->Quies(-INF, INF, 0);
    delete sd;

    return score;
}

/**
 * Implements the permanent brain.
 */
int CPosition::PermanentBrain() {
    CPosition *p = this;
    if (!p->LegalMove(PBMove)) {
        CPosition *q;

        q = CPosition::Clone(p);
        SearchMode = Puzzling;
        PBAltMove = M_NONE;

        Print(2, "Puzzling over a move to ponder on...\n");
        PBMove = q->Iterate(NULL, M_NONE, NULL);
        CPosition::Free(q);

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
        CPosition *q;
        bool inbook = false;
        char san_buffer[16];

        q = CPosition::Clone(p);

        PBActMove = PBMove;
        PBAltMove = M_NONE;
        PBHit = false;

        Print(0, "%s(%d): %s (in Permanent Brain)\n",
              p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
              p->SAN(PBActMove, san_buffer));

        q->DoMove(PBActMove);

        if (q->m_rgwOutOfBookCnt[q->m_nTurn] < 3) {
            move = SelectBook(q);
            if (move != M_NONE) {
                PBHit = false;
                PBAltMove = M_NONE;
                inbook = true;
            }
        }

        SearchMode = Pondering;

        if (!inbook) {
            move = q->Iterate(NULL, M_NONE, NULL);
        }

        CPosition::Free(q);

        if (SearchMode == Interrupted) {
            return PB_NO_PB_MOVE;
        }

        if (PBHit) {
            double elapsed =
                (double)(CurTime - WallTimeStart) / (double)ONE_SECOND;
            Print(2, "PB Hit! (elapsed %g secs)\n", elapsed);

            Print(0, "%s(%d): %s\n", p->m_nTurn == White ? "White" : "Black",
                  (p->m_wPly / 2) + 1, p->SAN(PBActMove, san_buffer));

            p->DoMove(PBActMove);
            DoTC(p, (int)(elapsed + 0.5));

            Print(0, REVERSE "%s(%d): %s" NORMAL "\n",
                  p->m_nTurn == White ? "White" : "Black", (p->m_wPly / 2) + 1,
                  p->SAN(move, san_buffer));

            if (XBoardMode) {
                PrintNoLog(0, "move %s\n", ICS_SAN(move));
            }

            p->DoMove(move);

            return PB_HIT;
        } else if (!PBHit && PBAltMove != M_NONE) {
            Print(2, "PB not Hit! Alternate move is %s\n",
                  p->SAN(PBAltMove, san_buffer));

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
    CPosition *q;

    SearchMode = Analyzing;

    q = CPosition::Clone(p);
    q->Iterate(NULL, M_NONE, NULL);
    CPosition::Free(q);
}

#if MP
/*
 * In parallel search start up all helper threads
 */

void CPosition::StartHelpers() {
    CPosition *p = this;
#if HAVE_LIBPTHREAD
    pthread_attr_t attr;
    int nthread;

    if (tids) {
        StopHelpers();
    }

    if (NumberOfCPUs < 2)
        return;

    tids = (pthread_t *)safe_calloc(NumberOfCPUs - 1, sizeof(pthread_t));

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    /*
     * Start up the helper threads.
     */

    for (nthread = 0; nthread < (NumberOfCPUs - 1); nthread++) {
        CSearchData *sd = new CSearchData(CPosition::Clone(p));
        sd->m_fMaster = false;
        pthread_create(tids + nthread, &attr, &IterateInt, sd);
    }

#endif /* HAVE_LIBPTHREAD */
}
#endif /* MP */

