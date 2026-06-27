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
 * search.c - tree searching routines
 */

#include "amy.h"

#include "search.h"
#include "bookup.h"
#include "commands.h"
#include "config.h"
#include "dbase.h"
#include "evaluation.h"
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "inline.h"
#include "mates.h"
#include "probe.h"
#include "random.h"
#include "recog.h"
#include "safe_malloc.h"
#include "search_io.h"
#include "state_machine.h"
#include "swap.h"
#include "time_ctl.h"
#include "utils.h"

#include <stdint.h>
#include <string.h>

#if HAVE_LIBPTHREAD
#include <pthread.h>
#endif

#if MP
#include <thread>
#include <vector>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#define NULLMOVE 1
#define FUTILITY 1
#define EXTENDED_FUTILITY 1
#define RAZORING 1

#define REVERSE "\x1B[7m"
#define NORMAL "\x1B[0m"

#define DEFERRED_DEPTH_OFFSET 32768

#define ALTERNATE_DELTA 1500

#if MP
#include <string.h>
/*
 * The parallel (ABDADA) search defers children that another thread is already
 * evaluating, remembering the move and the depth it should be searched at.
 * The deferred list reuses the CMove-typed search heap, so the (small) integer
 * depth is packed into a CMove slot. CMove is trivially copyable, so a byte
 * copy is well defined and round-trips exactly.
 */
static inline CMove EncodeDeferredDepth(int nDepth) {
    CMove mvEncoded;
    memcpy(&mvEncoded, &nDepth, sizeof(nDepth));
    return mvEncoded;
}

static inline int DecodeDeferredDepth(CMove mvEncoded) {
    int nDepth;
    memcpy(&nDepth, &mvEncoded, sizeof(nDepth));
    return nDepth;
}
#endif /* MP */

/*
 * We use fractional ply extensions.
 * See D. Levy, D. Broughton and M. Taylor: The SEX Algorithm in Computer Chess
 * ICCA Journal, Volume 2, No. 1, pp. 10-22.
 */
static const int OnePly = 16;

/*
 * Check extensions. Every check is extended one ply. Additional extensions
 * are awarded if there is only one legal reply or if it is a double or
 * discovered check.
 */

int ExtendInCheck = 14;
int ExtendDoubleCheck = 4;
int ExtendDiscoveredCheck = 4;
int ExtendSingularReply = 12;

/*
 * A passed pawn push to the seventh rank is extended.
 */

int ExtendPassedPawn = 14;
int ExtendZugzwang = 12;

/*
 * The tree below a null move is searched with reduced search depth.
 */

int ReduceNullMove = 48;
int ReduceNullMoveDeep = 65;

/*
 * Captures and recaptures are extended.
 */

int16_t ExtendRecapture[] = {0, 4, 6, 6, 8, 10};

static const int PVWindow = 250;
static const int ResearchWindow = 1500;

static const int MateDepth = 3;

/**
 * search tree data
 */

int MaxDepth;

unsigned long RCExt, ChkExt, DiscExt, DblExt, SingExt, PPExt, ZZExt;
unsigned long HardLimit, SoftLimit, SoftLimit2;
unsigned long StartTime, WallTimeStart;
unsigned long CurTime;
unsigned long FHTime;
bool AbortSearch;
bool NeedTime = false;
int PrintOK;
static int MaxSearchDepth = MAX_TREE_SIZE - 1;
int DoneAtRoot;

/*
 * Root moves to exclude from the next search(es). See SetExcludedRootMoves in
 * search.h. Default-empty, so a normal search is unaffected.
 */
#define MAX_EXCLUDED_ROOT_MOVES 8
static CMove rgExcludedRootMoves[MAX_EXCLUDED_ROOT_MOVES];
static uint16_t cExcludedRootMoves = 0;
static int EGTBDepth = 0;

static int NodesPerCheck = 1000;

OPTIONAL_ATOMIC unsigned long TotalNodes;

#if MP
int NumberOfCores;
#endif

/*
 * Search stati
 */

enum {
    Searching = 1,
    Pondering = 2,
    Puzzling = 3,
    Analyzing = 4,
    Interrupted = 5
};

enum { PVNode = 0, AllNode = 1, CutNode = 2, CutNodeNoNull = 3 };

int SearchMode = Searching;

/* Permanent Brain Variables */
CMove PBMove, PBActMove;
int PBHit;
CMove PBAltMove;

char BestLine[2048];
char ShortBestLine[2048];
char AnalysisLine[4096];

OPTIONAL_ATOMIC unsigned long HTry, HHit, PTry, PHit, STry, SHit;

/* prototypes for search routines */


/* search routines */

/*
 * Check if search should be terminated
 *
 * Here we also handle the case that we are in Permanent Brain and have to
 * check for user input.
 *
 */
bool CSearchData::TerminateSearch() {
    CSearchData *pSd = this;
    if ((pSd->m_ulNodesCount + pSd->m_ulQNodesCount) > pSd->m_ulCheckNodesCount) {
        unsigned long dwNow = GetTime();

        pSd->m_ulCheckNodesCount = pSd->m_ulNodesCount + pSd->m_ulQNodesCount + NodesPerCheck;
        if (AbortSearch)
            return true;

        CurTime = dwNow;
        if (CurTime > (StartTime + ONE_SECOND))
            PrintOK = true;

        if (pSd->m_fMaster && InputReady()) {
            char szBuffer[64];
            struct SCommand *pTheCommand;

            ReadLine(szBuffer, 64);

            /*
             * the '.' command can only be handled here
             */

            if (szBuffer[0] == '.') {
                PrintDebug(0, "stat01: %d %ld %d %d %d\n",
                           (CurTime - StartTime), TotalNodes, pSd->m_wDepth,
                           pSd->m_wRootMoves - pSd->m_wMoveNum - 1, pSd->m_wRootMoves);
            }

            pTheCommand = ParseInput(szBuffer);

            if (pTheCommand) {
                if (SearchMode == Pondering && pTheCommand->move != M_NONE) {
                    if (pTheCommand->move == PBActMove) {
                        PBHit = true;
                        SearchMode = Searching;
                        Print(1, "OK!\n");
                        WallTimeStart = dwNow;

                        if (CurTime >= HardLimit)
                            return true;
                        if (DoneAtRoot)
                            return true;

                        return false;
                    } else {
                        PBHit = false;
                        PBAltMove = pTheCommand->move;
                        return true;
                    }
                }

                if (SearchMode == Puzzling && pTheCommand->move != M_NONE) {
                    PBAltMove = pTheCommand->move;
                    return true;
                }

                if (SearchMode == Analyzing && pTheCommand->move != M_NONE) {
                    ExecuteCommand(pTheCommand);
                    return true;
                }

                if (pTheCommand->allowed_during_search) {
                    ExecuteCommand(pTheCommand);

                    if (pTheCommand->interrupts_search) {
                        SearchMode = Interrupted;
                        return true;
                    }
                }
            }
        }

        if (SearchMode == Searching) {
            if (CurTime >= HardLimit)
                return true;
        }
    }
    return false;
}

/*
 * Support routine for recpature extensions
 */

static bool IsRecapture(int nPiece1, int nPiece2) {
    switch (TYPE(nPiece1)) {
    case Knight:
    case Bishop:
        return (TYPE(nPiece2) == Knight || TYPE(nPiece2) == Bishop);
    default:
        return TYPE(nPiece1) == TYPE(nPiece2);
    }
}

/*
 * Store the result of the full width search
 */

void CSearchData::StoreResult(int nScore, int nAlpha, int nBeta,
                        CMove move, int nDepth, int nThreat) {
    CSearchData *pSd = this;
    CPosition *p = pSd->m_pPosition;

    if (!(move.IsTactical()) && nScore > nAlpha) {
        pSd->m_rguHistoryTab[p->GetTurn()][move.GetFromCoord().BitOffset()][move.GetToCoord().BitOffset()] += nDepth * nDepth;
    }

    StoreHT(p->GetHashKey(), nScore, nAlpha, nBeta, move, nDepth, nThreat, pSd->m_wPly
#if MP
            ,
            pSd->m_pLocalHashTable
#endif
    );
}

/*
 * The quiescence search.
 *
 * we only do a full width search if the side to move was in check since
 * the horizon, otherwise we do only a capture search.
 *
 */

int CSearchData::Quies(int nAlpha, int nBeta, int nDepth) {
    CSearchData *pSd = this;
    CPosition *p = pSd->m_pPosition;
    int nBest;
    CMove move;
    int nTAlpha;
    int nTmp;

    pSd->EnterNode();

    pSd->m_ulQNodesCount++;
    TotalNodes++;

    /* max search depth reached */
    if (pSd->m_wPly >= MaxDepth || p->Repeated(false)) {
        nBest = 0;
        goto EXIT;
    }

    /*
     * Probe recognizers. If the probe is successful, use the
     * recognizer score as evaluation score.
     *
     * Otherwise, use EvaluatePosition()
     */

    switch (ProbeRecognizer(p, &nTmp)) {
    case ExactScore:
        nBest = nTmp;
        goto EXIT;
    case LowerBound:
        nBest = nTmp;
        if (nBest >= nBeta) {
            goto EXIT;
        }
        break;
    case UpperBound:
        nBest = nTmp;
        if (nBest <= nAlpha) {
            goto EXIT;
        }
        break;
    default:
        nBest = EvaluatePosition(p);
        break;
    }

    if (nBest >= nBeta) {
        goto EXIT;
    }

    nTAlpha = MAX(nAlpha, nBest);

    while ((move = pSd->NextMoveQ(nAlpha)) != M_NONE) {
        /*
         * A move that captures the opponent's king signals an illegal position
         * (the previous move left a king in check). Capturing the king wins, so
         * return a winning score instead of making the king-capturing move,
         * which would assert in DoMove.
         */
        if (p->IsKingCapture(move)) {
            nBest = INF - pSd->m_wPly;
            goto EXIT;
        }
        p->DoMove(move);
        if (p->InCheck(OPP(p->GetTurn())))
            p->UndoMove(move);
        else {
            nTmp = -pSd->Quies(-nBeta, -nTAlpha, nDepth - 1);
            p->UndoMove(move);
            if (nTmp >= nBeta) {
                nBest = nTmp;
                goto EXIT;
            }
            if (nTmp > nBest) {
                nBest = nTmp;
                if (nBest > nTAlpha) {
                    nTAlpha = nBest;
                }
            }
        }
    }

EXIT:

    pSd->LeaveNode();

    return nBest;
}

/*
 * The main negascout routine with handles things like
 * search extensions, hash table lookup etc.
 *
 * It recursively calls itself until depth is exhausted.
 * Than `quies` is called.
 *
 * This is modified to perform an 'ABDADA' search if MP is defined.
 * See Jean-Christophe Weill, "The ABDADA Distributed Minimax-Search Algorithm"
 * ICCA Journal, Volume 19, No. 1, pp. 3-16
 */

int CSearchData::NegaScout(int nAlpha, int nBeta,
                           const int nDepth, int nNodeType
#if MP
                           ,
                           int nExclusiveP
#endif /* MP  */
) {
    CSearchData *pSd = this;
    CPosition *p = pSd->m_pPosition;
    struct SSearchStatus *pSt;
    int nBest = -INF;
    CMove bestm = M_NONE;
    int nTmp;
    int nTAlpha;
    CMove lmove;
    CMove move;
    int nExtend = 0;
    bool fThreat = false;
    int nReduceExtensions;
    int nNextType;
    bool fWasFutile = false;
    bool fInCheck = false;
#if FUTILITY
    int nIsFutile;
    int nOptimistic = 0;
#endif

    pSd->EnterNode();

    pSd->m_ulNodesCount++;
    TotalNodes++;

    /* check for search termination */
    if (pSd->m_fMaster && pSd->TerminateSearch()) {
        AbortSearch = true;
        goto EXIT;
    }

#if MP
    /*
     * Helper (non-master) threads periodically yield the CPU so the GUI / UI
     * thread and other processes on the machine stay responsive while the
     * engine is searching. The same node-count throttle used for master
     * termination keeps this off the hot path. Checking AbortSearch here also
     * lets helpers stop promptly when StopHelpers() joins them.
     */
    if (!pSd->m_fMaster) {
        if (AbortSearch) {
            goto EXIT;
        }
        if ((pSd->m_ulNodesCount + pSd->m_ulQNodesCount) >
            pSd->m_ulCheckNodesCount) {
            pSd->m_ulCheckNodesCount =
                pSd->m_ulNodesCount + pSd->m_ulQNodesCount + NodesPerCheck;
            std::this_thread::yield();
        }
    }
#endif /* MP */

    /* max search depth reached */
    if (pSd->m_wPly >= MaxDepth)
        goto EXIT;

    /*
     * Check for insufficent material or theoretical draw.
     */

    if (/* InsufMat(p) || p->CheckDraw() || */ p->Repeated(false)) {
        nBest = 0;
        goto EXIT;
    }

    /*
     * check extension
     */

    fInCheck = p->InCheck(p->GetTurn());
    if (fInCheck && p->GetMaterial(p->GetTurn()) > 0) {
        nExtend += p->CheckExtend();
        ChkExt++;
    }

    /*
     * Check the hashtable
     */

    pSt = pSd->m_pCurrent;

    HTry++;
#if MP
    switch (ProbeHT(p->GetHashKey(), &nTmp, nDepth, &(pSt->st_hashmove), &fThreat, pSd->m_wPly,
                    nExclusiveP, pSd->m_pLocalHashTable))
#else
    switch (ProbeHT(p->GetHashKey(), &nTmp, nDepth, &(pSt->st_hashmove), &fThreat, pSd->m_wPly))
#endif /* MP */
    {
    case ExactScore:
        HHit++;
        nBest = nTmp;
        goto EXIT;
    case UpperBound:
        if (nTmp <= nAlpha) {
            HHit++;
            nBest = nTmp;
            goto EXIT;
        }
        break;
    case LowerBound:
        if (nTmp >= nBeta) {
            HHit++;
            nBest = nTmp;
            goto EXIT;
        }
        break;
    case Useless:
        fThreat = !fInCheck && MateThreat(p, OPP(p->GetTurn()));
        break;
#if MP
    case OnEvaluation:
        nBest = -ON_EVALUATION;
        goto EXIT;
#endif
    default:
        break;
    }

    /*
     * Probe EGTB
     */

    if (nDepth > EGTBDepth && ProbeEGTB(p, &nTmp, pSd->m_wPly)) {
        nBest = nTmp;
        goto EXIT;
    }

    /*
     * Probe recognizers
     */

    switch (ProbeRecognizer(p, &nTmp)) {
    case ExactScore:
        nBest = nTmp;
        goto EXIT;
    case LowerBound:
        if (nTmp >= nBeta) {
            nBest = nTmp;
            goto EXIT;
        }
        break;
    case UpperBound:
        if (nTmp <= nAlpha) {
            nBest = nTmp;
            goto EXIT;
        }
        break;
    }

#if NULLMOVE

    /*
     * Null move search.
     * See Christian Donninger, "Null Move and Deep Search"
     * ICCA Journal Volume 16, No. 3, pp. 137-143
     */

    if (!fInCheck && nNodeType == CutNode && !fThreat) {
        int nNextDepth;
        int nms;

        nNextDepth = nDepth - ReduceNullMove;

        if (nNextDepth > 0) {
            nNextDepth = nDepth - ReduceNullMoveDeep;
        }

        p->DoNull();
        if (nNextDepth < 0) {
            nms = -pSd->Quies(-nBeta, -nBeta + 1, 0);
        } else {
#if MP
            nms = -pSd->NegaScout(-nBeta, -nBeta + 1, nNextDepth, AllNode, 0);
#else
            nms = -pSd->NegaScout(-nBeta, -nBeta + 1, nNextDepth, AllNode);
#endif
        }
        p->UndoNull();

        if (AbortSearch)
            goto EXIT;
        if (nms >= nBeta) {
            if (p->GetNonPawn(p->GetTurn()) >= Value[Queen]) {
                nBest = nms;
                goto EXIT;
            } else {
                if (nNextDepth < 0) {
                    nms = pSd->Quies(nBeta - 1, nBeta, 0);
                } else {
#if MP
                    nms = pSd->NegaScout(nBeta - 1, nBeta, nNextDepth,
                                    CutNodeNoNull, 0);
#else
                    nms = pSd->NegaScout(nBeta - 1, nBeta, nNextDepth,
                                    CutNodeNoNull);
#endif
                }

                if (nms >= nBeta) {
                    nBest = nms;
                    goto EXIT;
                } else {
                    nExtend += ExtendZugzwang;
                    ZZExt++;
                }
            }
        } else if (nms <= -CMLIMIT) {
            fThreat = true;
        }
    }
#endif /* NULLMOVE */

    lmove = (p->GetActLog() - 1)->gl_Move;
    nReduceExtensions = (pSd->m_wPly > 2 * pSd->m_wDepth);
    nTAlpha = nAlpha;

    switch (nNodeType) {
    case AllNode:
        nNextType = CutNode;
        break;
    case CutNode:
    case CutNodeNoNull:
        nNextType = AllNode;
        break;
    default:
        nNextType = PVNode;
        break;
    }

#if FUTILITY
    nIsFutile = !fInCheck && !fThreat && nAlpha < CMLIMIT && nAlpha > -CMLIMIT;
    if (nIsFutile) {
        if (p->GetTurn() == White) {
            nOptimistic = MaterialBalance(p) + MaxPos;
        } else {
            nOptimistic = -MaterialBalance(p) + MaxPos;
        }
    }
#endif /* FUTILITY */

    /*
     * Internal iterative deepening. If we do not have a move, we try
     * a shallow search to find a good candidate.
     */

    if (nDepth > 2 * OnePly && ((nAlpha + 1) != nBeta) &&
        !p->LegalMove(pSt->st_hashmove)) {
#if MP
        pSd->NegaScout(nAlpha, nBeta, nDepth - 2 * OnePly, PVNode, 0);
#else
        pSd->NegaScout(nAlpha, nBeta, nDepth - 2 * OnePly, PVNode);
#endif
        pSt->st_hashmove = pSd->m_rgPvSave[pSd->m_wPly + 1];
    }

    /*
     * Search all legal moves
     */

    while ((move = fInCheck ? pSd->NextEvasion() : pSd->NextMove()) != M_NONE) {
        int nNextDepth = nExtend;

        if (move.IsCastle() && !p->MayCastle(move))
            continue;

        /*
         * If this move captures the opponent's king the position is illegal:
         * the previous (opponent) move left its king in check and slipped past
         * the legality filter. Capturing the king wins outright, so return a
         * winning score immediately. This also avoids calling IsCheckingMove
         * or DoMove on a king-capturing move, both of which assert on this
         * illegal scenario.
         */

        if (p->IsKingCapture(move)) {
            nBest = INF - pSd->m_wPly;
            goto EXIT;
        }

        /*
         * recapture extension
         */

        if ((move.IsCapture()) && (lmove.IsCapture()) &&
            move.GetToCoord().BitOffset() == lmove.GetToCoord().BitOffset() &&
            IsRecapture(p->GetPiece(move.GetToCoord().BitOffset()), (p->GetActLog() - 1)->gl_Piece)) {
            RCExt += 1;
            nNextDepth += ExtendRecapture[TYPE(p->GetPiece(move.GetToCoord().BitOffset()))];
        }

        /*
         * passed pawn push extension
         */

        if (TYPE(p->GetPiece(move.GetFromCoord().BitOffset())) == Pawn &&
            p->GetNonPawn(OPP(p->GetTurn())) <= Value[Queen]) {
            const CSCoord& toCoord = move.GetToCoord();
            const int nWidth = CBitBoard::LEVEL_WIDTH[toCoord.m_nLevel];

            if (((p->GetTurn() == White && toCoord.m_nRank >= nWidth - 2) ||
                 (p->GetTurn() == Black && toCoord.m_nRank <= 1)) &&
                p->IsPassed(toCoord, p->GetTurn()) && SwapOff(p, move) >= 0) {
                nNextDepth += ExtendPassedPawn;
                PPExt += 1;
            }
        }

        /*
         * limit extensions to sensible range.
         */

        if (nReduceExtensions)
            nNextDepth /= 2;

        nNextDepth += nDepth - OnePly;

#if FUTILITY

        /*
         * Futility cutoffs
         */

        if (nIsFutile) {
            if (nNextDepth < 0 && !p->IsCheckingMove(move)) {
                nTmp = nOptimistic + p->ScoreMove(move);
                if (nTmp <= nAlpha) {
                    if (nTmp > nBest) {
                        nBest = nTmp;
                        bestm = move;
                        fWasFutile = true;
                    }
                    continue;
                }
            }
#if EXTENDED_FUTILITY

            /*
             * Extended futility cutoffs and limited razoring.
             * See Ernst A. Heinz, "Extended Futility Pruning"
             * ICCA Journal Volume 21, No. 2, pp 75-83
             */

            else if (nNextDepth >= 0 && nNextDepth < OnePly &&
                     !p->IsCheckingMove(move)) {
                nTmp = nOptimistic + p->ScoreMove(move) + (3 * Value[Pawn]);
                if (nTmp <= nAlpha) {
                    if (nTmp > nBest) {
                        nBest = nTmp;
                        bestm = move;
                        fWasFutile = true;
                    }
                    continue;
                }
            }
#if RAZORING
            else if (nNextDepth >= OnePly && nNextDepth < 2 * OnePly &&
                     !p->IsCheckingMove(move)) {
                nTmp = nOptimistic + p->ScoreMove(move) + (6 * Value[Pawn]);
                if (nTmp <= nAlpha) {
                    nNextDepth -= OnePly;
                }
            }
#endif /* RAZORING */
#endif /* EXTENDED_FUTILITY */
        }

#endif /* FUTILITY */

        p->DoMove(move);
        if (p->InCheck(OPP(p->GetTurn()))) {
            p->UndoMove(move);
        } else {
            /*
             * Check extension
             */

            if (p->GetMaterial(p->GetTurn()) > 0 && p->InCheck(p->GetTurn())) {
                nNextDepth +=
                    (nReduceExtensions) ? ExtendInCheck >> 1 : ExtendInCheck;
            }

            /*
             * Recursively search this position. If depth is exhausted, use
             * quies, otherwise use negascout.
             */

            if (nNextDepth < 0) {
                nTmp = -pSd->Quies(-nBeta, -nTAlpha, 0);
            } else if (bestm != M_NONE && !fWasFutile) {
#if MP
                nTmp = -pSd->NegaScout(-nTAlpha - 1, -nTAlpha, nNextDepth,
                                 nNextType, bestm != M_NONE);
                if (nTmp != ON_EVALUATION && nTmp > nTAlpha && nTmp < nBeta) {
                    nTmp = -pSd->NegaScout(-nBeta, -nTmp, nNextDepth,
                                     nNodeType == PVNode ? PVNode : AllNode,
                                     bestm != M_NONE);
                }
#else
                nTmp =
                    -pSd->NegaScout(-nTAlpha - 1, -nTAlpha, nNextDepth, nNextType);
                if (nTmp > nTAlpha && nTmp < nBeta) {
                    nTmp = -pSd->NegaScout(-nBeta, -nTmp, nNextDepth,
                                     nNodeType == PVNode ? PVNode : AllNode);
                }
#endif /* MP */
            } else {
#if MP
                nTmp = -pSd->NegaScout(-nBeta, -nTAlpha, nNextDepth, nNextType,
                                 bestm != M_NONE);
#else
                nTmp = -pSd->NegaScout(-nBeta, -nTAlpha, nNextDepth, nNextType);
#endif /* MP */
            }

            p->UndoMove(move);

            if (AbortSearch)
                goto EXIT;

#if MP
            if (nTmp == ON_EVALUATION) {
                /*
                 * This child is ON_EVALUATION. Remember move and
                 * depth.
                 */
                append_to_heap(pSd->m_hDeferredHeap, move);
                append_to_heap(pSd->m_hDeferredHeap,
                               EncodeDeferredDepth(nNextDepth +
                                                   DEFERRED_DEPTH_OFFSET));
            } else {
#endif /* MP */

                /*
                 * beta cutoff, enter move in Killer/Countermove table
                 */

                if (nTmp >= nBeta) {
                    if (!(move.IsTactical())) {
                        pSd->PutKiller(move);
                        pSd->m_rgCounterTab[p->GetTurn()][lmove.GetFromCoord().BitOffset()][lmove.GetToCoord().BitOffset()] = move;
                    }
                    pSd->StoreResult(nTmp, nAlpha, nBeta, move, nDepth, fThreat);
                    nBest = nTmp;
                    goto EXIT;
                }

                /*
                 * Improvement on best move to date
                 */

                if (nTmp > nBest) {
                    nBest = nTmp;
                    bestm = move;
                    fWasFutile = false;

                    if (nBest > nTAlpha) {
                        nTAlpha = nBest;
                    }
                }

                nNextType = CutNode;
#if MP
            }
#endif /* MP */
        }
    }

#if MP

    /*
     * Now search all moves which were ON_EVALUATION in pass one.
     */
    for (unsigned int dwDeferredIndex =
             pSd->m_hDeferredHeap->current_section->start;
         dwDeferredIndex < pSd->m_hDeferredHeap->current_section->end;
         dwDeferredIndex += 2) {

        move = pSd->m_hDeferredHeap->data[dwDeferredIndex];
        int nNextDepth =
            DecodeDeferredDepth(pSd->m_hDeferredHeap->data[dwDeferredIndex + 1]) -
            DEFERRED_DEPTH_OFFSET;

        p->DoMove(move);

        nTmp = -pSd->NegaScout(-nTAlpha - 1, -nTAlpha, nNextDepth, nNextType, 0);

        if (nTmp == ON_EVALUATION) {
            printf("Oops...\n");
        }

        if (nTmp > nTAlpha && nTmp < nBeta) {
            nTmp = -pSd->NegaScout(-nBeta, -nTAlpha, nNextDepth,
                             nNodeType == PVNode ? PVNode : AllNode, 0);
        }

        p->UndoMove(move);

        /*
         * beta cutoff, enter move in Killer/Countermove table
         */

        if (nTmp >= nBeta) {
            if (!(move.IsTactical())) {
                pSd->PutKiller(move);
                pSd->m_rgCounterTab[p->GetTurn()][lmove.GetFromCoord().BitOffset()][lmove.GetToCoord().BitOffset()] = move;
            }
            pSd->StoreResult(nTmp, nAlpha, nBeta, move, nDepth, fThreat);
            nBest = nTmp;
            goto EXIT;
        }

        /*
         * Improvement on best move to date
         */

        if (nTmp > nBest) {
            nBest = nTmp;
            bestm = move;
            fWasFutile = false;

            if (nBest > nTAlpha) {
                nTAlpha = nBest;
            }
        }

        nNextType = CutNode;
    }
#endif /* MP */

    /*
     * If we get here, no legal move was found.
     * Score this position as mate or draw.
     */

    if (bestm == M_NONE) {
        if (fInCheck)
            nBest = -INF + pSd->m_wPly;
        else
            nBest = 0;
    }

    if (!fWasFutile) {
        pSd->StoreResult(nBest, nAlpha, nBeta, bestm, nDepth, fThreat);
    }

EXIT:

    if (nNodeType == PVNode) {
        pSd->m_rgPvSave[pSd->m_wPly] = bestm;
    }

    pSd->LeaveNode();
    return nBest;
}

/**
 * Initialize the search variables.
 */
void CSearchData::InitSearch() {
    CSearchData *pSd = this;
    pSd->m_wPly = 0;
    pSd->m_ulNodesCount = pSd->m_ulQNodesCount = pSd->m_ulCheckNodesCount = 0;
    RCExt = ChkExt = DiscExt = DblExt = SingExt = PPExt = ZZExt = 0;
    PrintOK = (SearchMode == Analyzing) ? true : false;
    DoneAtRoot = false;
    EGTBProbe = EGTBProbeSucc = 0;

    /* Initialize scoring tables */

    HTry = HHit = PTry = PHit = STry = SHit = 0;
}

// Marcin Ciura's gap sequence for shell sort
static int gaps[] = {57, 23, 10, 4, 1};

/**
 * Resort the root move list. Keeps the first element unchanged,
 * and sorts the remaining moves by number of nodes searched
 * in decreasing order.
 */
static void ResortMovesList(int cnt, CMove *pMvs, unsigned long *nodes) {
    if (cnt <= 0)
        return;

    // Skip over the first element
    cnt -= 1;
    pMvs++;
    nodes++;

    for (int nGapIndex = 0; nGapIndex < 5; nGapIndex++) {
        int nGap = gaps[nGapIndex];
        for (int nI = nGap; nI < cnt; nI++) {
            int nJ;
            CMove mvs_tmp = pMvs[nI];
            unsigned long nodes_tmp = nodes[nI];

            for (nJ = nI; (nJ >= nGap) && (nodes[nJ - nGap] < nodes_tmp); nJ -= nGap) {
                nodes[nJ] = nodes[nJ - nGap];
                pMvs[nJ] = pMvs[nJ - nGap];
            }
            nodes[nJ] = nodes_tmp;
            pMvs[nJ] = mvs_tmp;
        }
    }
}

/*
 * This routine searches a chess position. It uses iterative deepening,
 * aspiration window and scout search.
 */

void *IterateInt(void *pX) {
    unsigned long rgNodes[256];
    int nLast = 0;
    double dElapsed;
    CSearchData *pSd = (CSearchData *)pX;
    CPosition *p;
    bool fAnyPvPrinted = false;
    bool fPvValid = false;

    if (!pSd->m_fMaster) {
#ifdef _WIN32
        Sleep((DWORD)((50.0 + 100.0 * Random()) / 1000.0));
#else
        usleep((useconds_t)(50 + 100 * Random()));
#endif
    }
    p = pSd->m_pPosition;

    pSd->InitSearch();
    pSd->m_wRootMoves = (uint16_t)p->LegalMoves(pSd->m_hHeap);

    // The root move list lives at a fixed index range within the shared search
    // heap. Move generation deeper in the search appends to that same heap, and
    // append_to_heap() may realloc() the underlying data buffer, relocating it
    // and invalidating any cached pointer into it. Remember the root section
    // start index and re-derive `mvs` from the (possibly moved) heap->data after
    // every operation that can grow the heap, so we never read through a
    // dangling pointer (which previously yielded a corrupt best move that
    // crashed in CPosition::SAN).
    const unsigned int dwRootStart = pSd->m_hHeap->current_section->start;
    CMove *pMvs = pSd->m_hHeap->data + dwRootStart;

    /*
     * Drop any caller-excluded root moves (used to emulate MultiPV by repeated
     * searches that each exclude the previously found best move). Compacting the
     * move array in place and shrinking m_wRootMoves makes the rest of the
     * search ignore them entirely. The default (empty) set is a no-op.
     */
    if (cExcludedRootMoves > 0) {
        uint16_t w = 0;
        for (uint16_t wR = 0; wR < pSd->m_wRootMoves; wR++) {
            bool fExcluded = false;
            for (uint16_t wE = 0; wE < cExcludedRootMoves; wE++) {
                if (pMvs[wR] == rgExcludedRootMoves[wE]) {
                    fExcluded = true;
                    break;
                }
            }
            if (!fExcluded) {
                pMvs[w++] = pMvs[wR];
            }
        }
        pSd->m_wRootMoves = w;
    }

    pSd->m_nBestScore = p->GetMaterial(p->GetTurn()) - p->GetMaterial(OPP(p->GetTurn()));

    /*
     * If every legal move was excluded there is nothing to search. Bail out with
     * no best move rather than dereferencing mvs[0] below.
     */
    if (pSd->m_wRootMoves == 0) {
        /*
         * No legal root moves were generated (stalemate/checkmate, or every
         * move was excluded above). Log the guard so a search that bails out
         * with no best move can be distinguished from other early exits when
         * auditing the log file.
         */
        PrintDebug(2, "IterateInt: no legal root moves (%s); returning M_NONE.\n",
                   pSd->m_fMaster ? "master" : "helper");
        pSd->m_BestMove = M_NONE;
        if (!pSd->m_fMaster) {
            PrintDebug(2, "IterateInt: freeing helper clone %p.\n",
                       (void *)pSd->m_pPosition);
            CPosition::Free(pSd->m_pPosition);
            delete pSd;
        }
        return NULL;
    }

    if (!pMvs[0].IsTactical())
        pSd->PutKiller(pMvs[0]);

    MaxDepth = MAX_TREE_SIZE - 1;

    for (pSd->m_wDepth = 1; pSd->m_wDepth < MaxSearchDepth; pSd->m_wDepth++) {
        int nAlpha = pSd->m_nBestScore - PVWindow;
        int nBeta = pSd->m_nBestScore + PVWindow;
        bool fIsPv = true;
        bool fPvStable = true;

        for (pSd->m_wMoveNum = 0; pSd->m_wMoveNum < pSd->m_wRootMoves; pSd->m_wMoveNum++) {
            int nTmp;
            int nNextDepth = (pSd->m_wDepth - 2) * OnePly;
            CMove move = pMvs[pSd->m_wMoveNum];
            bool fIsAlternate = !fIsPv && move == pSd->m_AlternateMove;

            rgNodes[pSd->m_wMoveNum] = pSd->m_ulNodesCount;

            if (pSd->m_fMaster && PrintOK) {
                char szTimeBuffer[16];
                char szSanBuffer[32];

                PrintDebug(
                    2, "%2d  %s   %2d/%2d  %s      \r", pSd->m_wDepth,
                    FormatTime(CurTime - StartTime, szTimeBuffer,
                               sizeof(szTimeBuffer)),
                    pSd->m_wMoveNum + 1, pSd->m_wRootMoves,
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
            if (p->InCheck(p->GetTurn()))
                nNextDepth += ExtendInCheck;

            if (nNextDepth >= 0) {
                int nEffectiveAlpha =
                    fIsAlternate ? (nAlpha - ALTERNATE_DELTA) : nAlpha;
#if MP
                nTmp = -pSd->NegaScout(-nBeta, -nEffectiveAlpha, nNextDepth,
                                 fIsPv ? PVNode : CutNode, 0);
#else
                nTmp = -pSd->NegaScout(-nBeta, -nEffectiveAlpha, nNextDepth,
                                 fIsPv ? PVNode : CutNode);
#endif
                if (fIsAlternate) {
                    pSd->m_nAlternateScore = nTmp;
                }
            } else {
                nTmp = -pSd->Quies(-nBeta, -nAlpha, 0);
            }
            p->UndoMove(move);
            // Move generation during the search above may have realloc'd the
            // heap; re-derive `mvs` so it points into the live data buffer.
            pMvs = pSd->m_hHeap->data + dwRootStart;
            if (AbortSearch)
                goto final;

            if (fIsPv && nTmp <= nAlpha) {

                /*
                 * Fail low on principal variation.
                 * Open window, take some time, and re-search.
                 */

                fPvStable = false;

                if (pSd->m_fMaster && PrintOK) {
                    char szSanBuffer[32];
                    SearchOutputFailHighLow(
                        pSd->m_wDepth, CurTime - StartTime, false,
                        p->NumberedSAN(move, szSanBuffer, sizeof(szSanBuffer)),
                        pSd->m_ulNodesCount + pSd->m_ulQNodesCount);
                }

                NeedTime = true;

                nBeta = nTmp;
                nAlpha = nTmp - ResearchWindow;

                p->DoMove(move);
                if (nNextDepth >= 0) {
#if MP
                    nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                    nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                } else {
                    nTmp = -pSd->Quies(-nBeta, -nAlpha, 0);
                }
                p->UndoMove(move);
                pMvs = pSd->m_hHeap->data + dwRootStart;
                if (AbortSearch)
                    goto final;

                if (nTmp <= nAlpha) {
                    nBeta = nTmp;
                    nAlpha = -INF;

                    p->DoMove(move);
                    if (nNextDepth >= 0) {
#if MP
                        nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode,
                                         0);
#else
                        nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                    } else {
                        nTmp = -pSd->Quies(-nBeta, -nAlpha, 0);
                    }
                    p->UndoMove(move);
                    pMvs = pSd->m_hHeap->data + dwRootStart;
                    if (AbortSearch)
                        goto final;
                }
                rgNodes[pSd->m_wMoveNum] = pSd->m_ulNodesCount - rgNodes[pSd->m_wMoveNum];
            } else if (nTmp >= nBeta) {

                /*
                 * Fail high.
                 * Re-search with open window.
                 */

                fPvStable = false;

                if (pSd->m_wMoveNum != 0) {
                    unsigned long dwTn = rgNodes[pSd->m_wMoveNum];
                    int nJ;

                    for (nJ = pSd->m_wMoveNum; nJ > 0; nJ--) {
                        pMvs[nJ] = pMvs[nJ - 1];
                        rgNodes[nJ] = rgNodes[nJ - 1];
                    }
                    pMvs[0] = move;
                    rgNodes[0] = dwTn;

                    if (!(move.IsTactical()))
                        pSd->PutKiller(move);
                    PBMove = M_NONE;
                    fIsPv = true;

                    FHTime = (CurTime - StartTime) / ONE_SECOND;
                }

                if (pSd->m_fMaster && PrintOK) {
                    char szSanBuffer[32];
                    SearchOutputFailHighLow(
                        pSd->m_wDepth, CurTime - StartTime, true,
                        p->NumberedSAN(pMvs[0], szSanBuffer, sizeof(szSanBuffer)),
                        pSd->m_ulNodesCount + pSd->m_ulQNodesCount);
                }

                nAlpha = nTmp;
                nBeta = nTmp + ResearchWindow;

                p->DoMove(move);
                if (nNextDepth >= 0) {
#if MP
                    nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode, 0);
#else
                    nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                } else {
                    nTmp = -pSd->Quies(-nBeta, -nAlpha, 0);
                }
                p->UndoMove(move);
                pMvs = pSd->m_hHeap->data + dwRootStart;
                if (AbortSearch)
                    goto final;

                if (nTmp >= nBeta) {
                    nAlpha = nTmp;
                    nBeta = INF;

                    p->DoMove(move);
                    if (nNextDepth >= 0) {
#if MP
                        nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode,
                                         0);
#else
                        nTmp = -pSd->NegaScout(-nBeta, -nAlpha, nNextDepth, PVNode);
#endif
                    } else {
                        nTmp = -pSd->Quies(-nBeta, -nAlpha, 0);
                    }
                    p->UndoMove(move);
                    pMvs = pSd->m_hHeap->data + dwRootStart;
                    if (AbortSearch)
                        goto final;
                }
                rgNodes[0] = pSd->m_ulNodesCount - rgNodes[0];
            } else {
                rgNodes[pSd->m_wMoveNum] = pSd->m_ulNodesCount - rgNodes[pSd->m_wMoveNum];
            }

            if (AbortSearch)
                goto final;

            if (fIsPv) {
                pSd->m_nBestScore = nTmp;

                if (pSd->m_fMaster) {
                    char szScoreAsText[16];
                    p->AnalyzeHT(pMvs[0]);
                    fPvValid = true;

                    snprintf(AnalysisLine, sizeof(AnalysisLine),
                             "%2d: (%7s) %s", pSd->m_wDepth,
                             FormatScore(pSd->m_nBestScore, szScoreAsText,
                                         sizeof(szScoreAsText)),
                             BestLine);

                    if (PrintOK) {
                        SearchOutput(pSd->m_wDepth, CurTime - StartTime,
                                     (p->GetTurn()) ? -pSd->m_nBestScore
                                               : pSd->m_nBestScore,
                                     BestLine, pSd->m_ulNodesCount + pSd->m_ulQNodesCount);

                        fAnyPvPrinted = true;
                    }
                }

                nAlpha = pSd->m_nBestScore;
                nBeta = pSd->m_nBestScore + 1;
                fIsPv = false;
            }

            if (pSd->m_fMaster && pSd->m_wMoveNum == 0 && !NeedTime &&
                CurTime > SoftLimit) {
                if (SearchMode == Searching) {
                    AbortSearch = true;
                    goto final;
                } else if (SearchMode == Pondering) {
                    DoneAtRoot = true;
                }
            }
        }

        if (pSd->m_fMaster && (PrintOK || (pSd->m_wDepth > MateDepth &&
                                       (pSd->m_nBestScore < -CMLIMIT ||
                                        pSd->m_nBestScore > CMLIMIT)))) {
            SearchOutput(pSd->m_wDepth, CurTime - StartTime,
                         (p->GetTurn()) ? -pSd->m_nBestScore : pSd->m_nBestScore, BestLine,
                         pSd->m_ulNodesCount + pSd->m_ulQNodesCount);

            fAnyPvPrinted = true;
        }

        if (pSd->m_nBestScore < -CMLIMIT || pSd->m_nBestScore > CMLIMIT) {
            if (nLast > CMLIMIT && pSd->m_nBestScore >= nLast &&
                pSd->m_wDepth > MateDepth) {
                if (SearchMode == Searching)
                    break;
                else
                    DoneAtRoot = true;
            }
            if (SearchMode == Searching && nLast < CMLIMIT &&
                pSd->m_nBestScore <= nLast && pSd->m_wDepth > MateDepth)
                break;
            nLast = pSd->m_nBestScore;
        }

        NeedTime = false;
        pMvs = pSd->m_hHeap->data + dwRootStart;
        ResortMovesList(pSd->m_wRootMoves, pMvs, rgNodes);

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

        if (pSd->m_wDepth > 3) {
            /*
             * Do ten checks per second.
             */

            dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

            NodesPerCheck =
                (dElapsed == 0.0)
                    ? 1000
                    : (int)((pSd->m_ulNodesCount + pSd->m_ulQNodesCount) / dElapsed / 10);
        }

        if (SearchMode == Puzzling && pSd->m_wDepth > 4)
            break;

        if (pSd->m_fMaster &&
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

    if (CurTime <= StartTime)
        StartTime--;
    dElapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

    if (pSd->m_fMaster) {
        if (fPvValid && !fAnyPvPrinted) {
            // Make sure there is a PV printed
            SearchOutput(pSd->m_wDepth, CurTime - StartTime,
                         (p->GetTurn()) ? -pSd->m_nBestScore : pSd->m_nBestScore, BestLine,
                         pSd->m_ulNodesCount + pSd->m_ulQNodesCount);
        }

        char szBuf1[16], szBuf2[16], szBuf3[16], szBuf4[16], szBuf5[16], szBuf6[16],
            szBuf7[16];

        unsigned long dwNps = (unsigned long)(TotalNodes / dElapsed);

        Print(2, "Nodes = %s, QPerc: %d %%, time = %g secs, %s nodes/s\n",
              FormatCount(TotalNodes, szBuf1, sizeof(szBuf1)),
              Percentage(pSd->m_ulQNodesCount, pSd->m_ulNodesCount + pSd->m_ulQNodesCount),
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
    pMvs = pSd->m_hHeap->data + dwRootStart;
    pSd->m_BestMove = pMvs[0];

    /*
     * The returned best move must be legal in the (now restored) root position.
     * Returning an illegal move here is exactly the failure mode behind the
     * reported "engine recommended an illegal move" bug, so assert it: the
     * failure is logged and trapped in the debugger (no-op in release).
     */
    AMY_ASSERT(pSd->m_BestMove == M_NONE || p->LegalMove(pSd->m_BestMove),
               "IterateInt returning an illegal best move: from L%d/F%d/R%d "
               "to L%d/F%d/R%d\n",
               pSd->m_BestMove.GetFromCoord().m_nLevel,
               pSd->m_BestMove.GetFromCoord().m_nFile,
               pSd->m_BestMove.GetFromCoord().m_nRank,
               pSd->m_BestMove.GetToCoord().m_nLevel,
               pSd->m_BestMove.GetToCoord().m_nFile,
               pSd->m_BestMove.GetToCoord().m_nRank);

    if (!pSd->m_fMaster) {
        PrintDebug(2, "IterateInt: freeing helper clone %p.\n",
                   (void *)pSd->m_pPosition);
        CPosition::Free(pSd->m_pPosition);
        delete pSd;
    }

    return NULL;
}

#if MP
/*
 * Helper threads used for the parallel (ABDADA) search. The master search runs
 * on the calling thread; these are the additional worker threads.
 */
std::vector<std::thread> HelperThreads;

/*
 * Lower the priority of the current search thread so that the GUI / UI thread
 * and other processes on the machine remain responsive while the engine is
 * thinking. Called at the start of every helper thread.
 */
void SetSearchThreadBackgroundPriority(void) {
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

/*
 * In parallel search stop all helper threads
 */

void StopHelpers(void) {
    if (!HelperThreads.empty()) {
        AbortSearch = true;
        for (std::thread &thread : HelperThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        HelperThreads.clear();
    }
}

#endif /* MP */

/**
 * Set the maximum depth for the root search.
 */
void SetMaxSearchDepth(int nMaxSearchDepth) {
    if (nMaxSearchDepth > 0 && nMaxSearchDepth < (MAX_TREE_SIZE - 1)) {
        MaxSearchDepth = nMaxSearchDepth;
    }
}

void SetExcludedRootMoves(const CMove *pMoves, uint16_t cMoves) {
    if (cMoves > MAX_EXCLUDED_ROOT_MOVES) {
        cMoves = MAX_EXCLUDED_ROOT_MOVES;
    }
    for (uint16_t wI = 0; wI < cMoves; wI++) {
        rgExcludedRootMoves[wI] = pMoves[wI];
    }
    cExcludedRootMoves = cMoves;
}

void ClearExcludedRootMoves(void) { cExcludedRootMoves = 0; }
