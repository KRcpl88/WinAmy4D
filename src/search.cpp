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
static int EGTBDepth = 0;

static int NodesPerCheck;

OPTIONAL_ATOMIC unsigned long TotalNodes;

#if MP
int NumberOfCPUs;
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
    CSearchData *sd = this;
    if ((sd->m_ulNodesCount + sd->m_ulQNodesCount) > sd->m_ulCheckNodesCount) {
        unsigned long now = GetTime();

        sd->m_ulCheckNodesCount = sd->m_ulNodesCount + sd->m_ulQNodesCount + NodesPerCheck;
        if (AbortSearch)
            return true;

        CurTime = now;
        if (CurTime > (StartTime + ONE_SECOND))
            PrintOK = true;

        if (sd->m_fMaster && InputReady()) {
            char buffer[64];
            struct SCommand *theCommand;

            ReadLine(buffer, 64);

            /*
             * the '.' command can only be handled here
             */

            if (buffer[0] == '.') {
                PrintNoLog(0, "stat01: %d %ld %d %d %d\n",
                           (CurTime - StartTime), TotalNodes, sd->m_wDepth,
                           sd->m_wRootMoves - sd->m_wMoveNum - 1, sd->m_wRootMoves);
            }

            theCommand = ParseInput(buffer);

            if (theCommand) {
                if (SearchMode == Pondering && theCommand->move != M_NONE) {
                    if (theCommand->move == PBActMove) {
                        PBHit = true;
                        SearchMode = Searching;
                        Print(1, "OK!\n");
                        WallTimeStart = now;

                        if (CurTime >= HardLimit)
                            return true;
                        if (DoneAtRoot)
                            return true;

                        return false;
                    } else {
                        PBHit = false;
                        PBAltMove = theCommand->move;
                        return true;
                    }
                }

                if (SearchMode == Puzzling && theCommand->move != M_NONE) {
                    PBAltMove = theCommand->move;
                    return true;
                }

                if (SearchMode == Analyzing && theCommand->move != M_NONE) {
                    ExecuteCommand(theCommand);
                    return true;
                }

                if (theCommand->allowed_during_search) {
                    ExecuteCommand(theCommand);

                    if (theCommand->interrupts_search) {
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

static bool IsRecapture(int piece1, int piece2) {
    switch (TYPE(piece1)) {
    case Knight:
    case Bishop:
        return (TYPE(piece2) == Knight || TYPE(piece2) == Bishop);
    default:
        return TYPE(piece1) == TYPE(piece2);
    }
}

/*
 * Store the result of the full width search
 */

void CSearchData::StoreResult(int score, int alpha, int beta,
                        CMove move, int depth, int threat) {
    CSearchData *sd = this;
    CPosition *p = sd->m_pPosition;

    if (!(move.IsTactical()) && score > alpha) {
        sd->m_rguHistoryTab[p->m_nTurn][move.GetFromToIndex()] += depth * depth;
    }

    StoreHT(p->m_ullHKey, score, alpha, beta, move, depth, threat, sd->m_wPly
#if MP
            ,
            sd->m_pLocalHashTable
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

int CSearchData::Quies(int alpha, int beta, int depth) {
    CSearchData *sd = this;
    CPosition *p = sd->m_pPosition;
    int best;
    CMove move;
    int talpha;
    int tmp;

    sd->EnterNode();

    sd->m_ulQNodesCount++;
    TotalNodes++;

    /* max search depth reached */
    if (sd->m_wPly >= MaxDepth || p->Repeated(false)) {
        best = 0;
        goto EXIT;
    }

    /*
     * Probe recognizers. If the probe is successful, use the
     * recognizer score as evaluation score.
     *
     * Otherwise, use EvaluatePosition()
     */

    switch (ProbeRecognizer(p, &tmp)) {
    case ExactScore:
        best = tmp;
        goto EXIT;
    case LowerBound:
        best = tmp;
        if (best >= beta) {
            goto EXIT;
        }
        break;
    case UpperBound:
        best = tmp;
        if (best <= alpha) {
            goto EXIT;
        }
        break;
    default:
        best = EvaluatePosition(p);
        break;
    }

    if (best >= beta) {
        goto EXIT;
    }

    talpha = MAX(alpha, best);

    while ((move = sd->NextMoveQ(alpha)) != M_NONE) {
        p->DoMove(move);
        if (p->InCheck(OPP(p->m_nTurn)))
            p->UndoMove(move);
        else {
            tmp = -sd->Quies(-beta, -talpha, depth - 1);
            p->UndoMove(move);
            if (tmp >= beta) {
                best = tmp;
                goto EXIT;
            }
            if (tmp > best) {
                best = tmp;
                if (best > talpha) {
                    talpha = best;
                }
            }
        }
    }

EXIT:

    sd->LeaveNode();

    return best;
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

int CSearchData::NegaScout(int alpha, int beta,
                           const int depth, int node_type
#if MP
                           ,
                           int exclusiveP
#endif /* MP  */
) {
    CSearchData *sd = this;
    CPosition *p = sd->m_pPosition;
    struct SSearchStatus *st;
    int best = -INF;
    CMove bestm = M_NONE;
    int tmp;
    int talpha;
    CMove lmove;
    CMove move;
    int extend = 0;
    bool threat = false;
    int reduce_extensions;
    int next_type;
    bool was_futile = false;
    bool incheck = false;
#if FUTILITY
    int is_futile;
    int optimistic = 0;
#endif

    sd->EnterNode();

    sd->m_ulNodesCount++;
    TotalNodes++;

    /* check for search termination */
    if (sd->m_fMaster && sd->TerminateSearch()) {
        AbortSearch = true;
        goto EXIT;
    }

    /* max search depth reached */
    if (sd->m_wPly >= MaxDepth)
        goto EXIT;

    /*
     * Check for insufficent material or theoretical draw.
     */

    if (/* InsufMat(p) || p->CheckDraw() || */ p->Repeated(false)) {
        best = 0;
        goto EXIT;
    }

    /*
     * check extension
     */

    incheck = p->InCheck(p->m_nTurn);
    if (incheck && p->m_rgnMaterial[p->m_nTurn] > 0) {
        extend += p->CheckExtend();
        ChkExt++;
    }

    /*
     * Check the hashtable
     */

    st = sd->m_pCurrent;

    HTry++;
#if MP
    switch (ProbeHT(p->m_ullHKey, &tmp, depth, &(st->st_hashmove), &threat, sd->m_wPly,
                    exclusiveP, sd->m_pLocalHashTable))
#else
    switch (ProbeHT(p->m_ullHKey, &tmp, depth, &(st->st_hashmove), &threat, sd->m_wPly))
#endif /* MP */
    {
    case ExactScore:
        HHit++;
        best = tmp;
        goto EXIT;
    case UpperBound:
        if (tmp <= alpha) {
            HHit++;
            best = tmp;
            goto EXIT;
        }
        break;
    case LowerBound:
        if (tmp >= beta) {
            HHit++;
            best = tmp;
            goto EXIT;
        }
        break;
    case Useless:
        threat = !incheck && MateThreat(p, OPP(p->m_nTurn));
        break;
#if MP
    case OnEvaluation:
        best = -ON_EVALUATION;
        goto EXIT;
#endif
    default:
        break;
    }

    /*
     * Probe EGTB
     */

    if (depth > EGTBDepth && ProbeEGTB(p, &tmp, sd->m_wPly)) {
        best = tmp;
        goto EXIT;
    }

    /*
     * Probe recognizers
     */

    switch (ProbeRecognizer(p, &tmp)) {
    case ExactScore:
        best = tmp;
        goto EXIT;
    case LowerBound:
        if (tmp >= beta) {
            best = tmp;
            goto EXIT;
        }
        break;
    case UpperBound:
        if (tmp <= alpha) {
            best = tmp;
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

    if (!incheck && node_type == CutNode && !threat) {
        int next_depth;
        int nms;

        next_depth = depth - ReduceNullMove;

        if (next_depth > 0) {
            next_depth = depth - ReduceNullMoveDeep;
        }

        p->DoNull();
        if (next_depth < 0) {
            nms = -sd->Quies(-beta, -beta + 1, 0);
        } else {
#if MP
            nms = -sd->NegaScout(-beta, -beta + 1, next_depth, AllNode, 0);
#else
            nms = -sd->NegaScout(-beta, -beta + 1, next_depth, AllNode);
#endif
        }
        p->UndoNull();

        if (AbortSearch)
            goto EXIT;
        if (nms >= beta) {
            if (p->m_rgnNonPawn[p->m_nTurn] >= Value[Queen]) {
                best = nms;
                goto EXIT;
            } else {
                if (next_depth < 0) {
                    nms = sd->Quies(beta - 1, beta, 0);
                } else {
#if MP
                    nms = sd->NegaScout(beta - 1, beta, next_depth,
                                    CutNodeNoNull, 0);
#else
                    nms = sd->NegaScout(beta - 1, beta, next_depth,
                                    CutNodeNoNull);
#endif
                }

                if (nms >= beta) {
                    best = nms;
                    goto EXIT;
                } else {
                    extend += ExtendZugzwang;
                    ZZExt++;
                }
            }
        } else if (nms <= -CMLIMIT) {
            threat = true;
        }
    }
#endif /* NULLMOVE */

    lmove = (p->m_pActLog - 1)->gl_Move;
    reduce_extensions = (sd->m_wPly > 2 * sd->m_wDepth);
    talpha = alpha;

    switch (node_type) {
    case AllNode:
        next_type = CutNode;
        break;
    case CutNode:
    case CutNodeNoNull:
        next_type = AllNode;
        break;
    default:
        next_type = PVNode;
        break;
    }

#if FUTILITY
    is_futile = !incheck && !threat && alpha < CMLIMIT && alpha > -CMLIMIT;
    if (is_futile) {
        if (p->m_nTurn == White) {
            optimistic = MaterialBalance(p) + MaxPos;
        } else {
            optimistic = -MaterialBalance(p) + MaxPos;
        }
    }
#endif /* FUTILITY */

    /*
     * Internal iterative deepening. If we do not have a move, we try
     * a shallow search to find a good candidate.
     */

    if (depth > 2 * OnePly && ((alpha + 1) != beta) &&
        !p->LegalMove(st->st_hashmove)) {
#if MP
        sd->NegaScout(alpha, beta, depth - 2 * OnePly, PVNode, 0);
#else
        sd->NegaScout(alpha, beta, depth - 2 * OnePly, PVNode);
#endif
        st->st_hashmove = sd->m_rgPvSave[sd->m_wPly + 1];
    }

    /*
     * Search all legal moves
     */

    while ((move = incheck ? sd->NextEvasion() : sd->NextMove()) != M_NONE) {
        int next_depth = extend;

        if (move.IsCastle() && !p->MayCastle(move))
            continue;

        /*
         * recapture extension
         */

        if ((move.IsCapture()) && (lmove.IsCapture()) &&
            move.GetToCoord().BitOffset() == lmove.GetToCoord().BitOffset() &&
            IsRecapture(p->m_rgPiece[move.GetToCoord().BitOffset()], (p->m_pActLog - 1)->gl_Piece)) {
            RCExt += 1;
            next_depth += ExtendRecapture[TYPE(p->m_rgPiece[move.GetToCoord().BitOffset()])];
        }

        /*
         * passed pawn push extension
         */

        if (TYPE(p->m_rgPiece[move.GetFromCoord().BitOffset()]) == Pawn &&
            p->m_rgnNonPawn[OPP(p->m_nTurn)] <= Value[Queen]) {
            const CSCoord& toCoord = move.GetToCoord();
            const int width = CBitBoard::LEVEL_WIDTH[toCoord.m_nLevel];

            if (((p->m_nTurn == White && toCoord.m_nRank >= width - 2) ||
                 (p->m_nTurn == Black && toCoord.m_nRank <= 1)) &&
                p->IsPassed(toCoord, p->m_nTurn) && SwapOff(p, move) >= 0) {
                next_depth += ExtendPassedPawn;
                PPExt += 1;
            }
        }

        /*
         * limit extensions to sensible range.
         */

        if (reduce_extensions)
            next_depth /= 2;

        next_depth += depth - OnePly;

#if FUTILITY

        /*
         * Futility cutoffs
         */

        if (is_futile) {
            if (next_depth < 0 && !p->IsCheckingMove(move)) {
                tmp = optimistic + p->ScoreMove(move);
                if (tmp <= alpha) {
                    if (tmp > best) {
                        best = tmp;
                        bestm = move;
                        was_futile = true;
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

            else if (next_depth >= 0 && next_depth < OnePly &&
                     !p->IsCheckingMove(move)) {
                tmp = optimistic + p->ScoreMove(move) + (3 * Value[Pawn]);
                if (tmp <= alpha) {
                    if (tmp > best) {
                        best = tmp;
                        bestm = move;
                        was_futile = true;
                    }
                    continue;
                }
            }
#if RAZORING
            else if (next_depth >= OnePly && next_depth < 2 * OnePly &&
                     !p->IsCheckingMove(move)) {
                tmp = optimistic + p->ScoreMove(move) + (6 * Value[Pawn]);
                if (tmp <= alpha) {
                    next_depth -= OnePly;
                }
            }
#endif /* RAZORING */
#endif /* EXTENDED_FUTILITY */
        }

#endif /* FUTILITY */

        p->DoMove(move);
        if (p->InCheck(OPP(p->m_nTurn))) {
            p->UndoMove(move);
        } else {
            /*
             * Check extension
             */

            if (p->m_rgnMaterial[p->m_nTurn] > 0 && p->InCheck(p->m_nTurn)) {
                next_depth +=
                    (reduce_extensions) ? ExtendInCheck >> 1 : ExtendInCheck;
            }

            /*
             * Recursively search this position. If depth is exhausted, use
             * quies, otherwise use negascout.
             */

            if (next_depth < 0) {
                tmp = -sd->Quies(-beta, -talpha, 0);
            } else if (bestm != M_NONE && !was_futile) {
#if MP
                tmp = -sd->NegaScout(-talpha - 1, -talpha, next_depth,
                                 next_type, bestm != M_NONE);
                if (tmp != ON_EVALUATION && tmp > talpha && tmp < beta) {
                    tmp = -sd->NegaScout(-beta, -tmp, next_depth,
                                     node_type == PVNode ? PVNode : AllNode,
                                     bestm != M_NONE);
                }
#else
                tmp =
                    -sd->NegaScout(-talpha - 1, -talpha, next_depth, next_type);
                if (tmp > talpha && tmp < beta) {
                    tmp = -sd->NegaScout(-beta, -tmp, next_depth,
                                     node_type == PVNode ? PVNode : AllNode);
                }
#endif /* MP */
            } else {
#if MP
                tmp = -sd->NegaScout(-beta, -talpha, next_depth, next_type,
                                 bestm != M_NONE);
#else
                tmp = -sd->NegaScout(-beta, -talpha, next_depth, next_type);
#endif /* MP */
            }

            p->UndoMove(move);

            if (AbortSearch)
                goto EXIT;

#if MP
            if (tmp == ON_EVALUATION) {
                /*
                 * This child is ON_EVALUATION. Remember move and
                 * depth.
                 */
                append_to_heap(sd->m_hDeferredHeap, move);
                append_to_heap(sd->m_hDeferredHeap,
                               next_depth + DEFERRED_DEPTH_OFFSET);
            } else {
#endif /* MP */

                /*
                 * beta cutoff, enter move in Killer/Countermove table
                 */

                if (tmp >= beta) {
                    if (!(move.IsTactical())) {
                        sd->PutKiller(move);
                        sd->m_rgCounterTab[p->m_nTurn][lmove.GetFromToIndex()] = move;
                    }
                    sd->StoreResult(tmp, alpha, beta, move, depth, threat);
                    best = tmp;
                    goto EXIT;
                }

                /*
                 * Improvement on best move to date
                 */

                if (tmp > best) {
                    best = tmp;
                    bestm = move;
                    was_futile = false;

                    if (best > talpha) {
                        talpha = best;
                    }
                }

                next_type = CutNode;
#if MP
            }
#endif /* MP */
        }
    }

#if MP

    /*
     * Now search all moves which were ON_EVALUATION in pass one.
     */
    for (unsigned int deferred_index =
             sd->m_hDeferredHeap->current_section->start;
         deferred_index < sd->m_hDeferredHeap->current_section->end;
         deferred_index += 2) {

        move = sd->m_hDeferredHeap->data[deferred_index];
        int next_depth =
            sd->m_hDeferredHeap->data[deferred_index + 1] - DEFERRED_DEPTH_OFFSET;

        p->DoMove(move);

        tmp = -sd->NegaScout(-talpha - 1, -talpha, next_depth, next_type, 0);

        if (tmp == ON_EVALUATION) {
            printf("Oops...\n");
        }

        if (tmp > talpha && tmp < beta) {
            tmp = -sd->NegaScout(-beta, -talpha, next_depth,
                             node_type == PVNode ? PVNode : AllNode, 0);
        }

        p->UndoMove(move);

        /*
         * beta cutoff, enter move in Killer/Countermove table
         */

        if (tmp >= beta) {
            if (!(move.IsTactical())) {
                sd->PutKiller(move);
                sd->m_rgCounterTab[p->m_nTurn][lmove.GetFromToIndex()] = move;
            }
            sd->StoreResult(tmp, alpha, beta, move, depth, threat);
            best = tmp;
            goto EXIT;
        }

        /*
         * Improvement on best move to date
         */

        if (tmp > best) {
            best = tmp;
            bestm = move;
            was_futile = false;

            if (best > talpha) {
                talpha = best;
            }
        }

        next_type = CutNode;
    }
#endif /* MP */

    /*
     * If we get here, no legal move was found.
     * Score this position as mate or draw.
     */

    if (bestm == M_NONE) {
        if (incheck)
            best = -INF + sd->m_wPly;
        else
            best = 0;
    }

    if (!was_futile) {
        sd->StoreResult(best, alpha, beta, bestm, depth, threat);
    }

EXIT:

    if (node_type == PVNode) {
        sd->m_rgPvSave[sd->m_wPly] = bestm;
    }

    sd->LeaveNode();
    return best;
}

/**
 * Initialize the search variables.
 */
void CSearchData::InitSearch() {
    CSearchData *sd = this;
    sd->m_wPly = 0;
    sd->m_ulNodesCount = sd->m_ulQNodesCount = sd->m_ulCheckNodesCount = 0;
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
static void ResortMovesList(int cnt, CMove *mvs, unsigned long *nodes) {
    if (cnt <= 0)
        return;

    // Skip over the first element
    cnt -= 1;
    mvs++;
    nodes++;

    for (int gap_index = 0; gap_index < 5; gap_index++) {
        int gap = gaps[gap_index];
        for (int i = gap; i < cnt; i++) {
            int j;
            CMove mvs_tmp = mvs[i];
            unsigned long nodes_tmp = nodes[i];

            for (j = i; (j >= gap) && (nodes[j - gap] < nodes_tmp); j -= gap) {
                nodes[j] = nodes[j - gap];
                mvs[j] = mvs[j - gap];
            }
            nodes[j] = nodes_tmp;
            mvs[j] = mvs_tmp;
        }
    }
}

/*
 * This routine searches a chess position. It uses iterative deepening,
 * aspiration window and scout search.
 */

void *IterateInt(void *x) {
    unsigned long nodes[256];
    int last = 0;
    double elapsed;
    CSearchData *sd = (CSearchData *)x;
    CPosition *p;
    bool any_pv_printed = false;
    bool pv_valid = false;

    if (!sd->m_fMaster) {
#ifdef _WIN32
        Sleep((DWORD)((50.0 + 100.0 * Random()) / 1000.0));
#else
        usleep((useconds_t)(50 + 100 * Random()));
#endif
    }
    p = sd->m_pPosition;

    sd->InitSearch();
    sd->m_wRootMoves = (uint16_t)p->LegalMoves(sd->m_hHeap);

    CMove *mvs = sd->m_hHeap->data + sd->m_hHeap->current_section->start;

    sd->m_nBestScore = p->m_rgnMaterial[p->m_nTurn] - p->m_rgnMaterial[OPP(p->m_nTurn)];

    if (!mvs[0].IsTactical())
        sd->PutKiller(mvs[0]);

    MaxDepth = MAX_TREE_SIZE - 1;

    for (sd->m_wDepth = 1; sd->m_wDepth < MaxSearchDepth; sd->m_wDepth++) {
        int alpha = sd->m_nBestScore - PVWindow;
        int beta = sd->m_nBestScore + PVWindow;
        bool is_pv = true;
        bool pv_stable = true;

        for (sd->m_wMoveNum = 0; sd->m_wMoveNum < sd->m_wRootMoves; sd->m_wMoveNum++) {
            int tmp;
            int next_depth = (sd->m_wDepth - 2) * OnePly;
            CMove move = mvs[sd->m_wMoveNum];
            bool is_alternate = !is_pv && move == sd->m_AlternateMove;

            nodes[sd->m_wMoveNum] = sd->m_ulNodesCount;

            if (sd->m_fMaster && PrintOK) {
                char time_buffer[16];
                char san_buffer[32];

                PrintNoLog(
                    2, "%2d  %s   %2d/%2d  %s      \r", sd->m_wDepth,
                    FormatTime(CurTime - StartTime, time_buffer,
                               sizeof(time_buffer)),
                    sd->m_wMoveNum + 1, sd->m_wRootMoves,
                    p->NumberedSAN(move, san_buffer, sizeof(san_buffer)));
            }

            p->DoMove(move);
            if (p->InCheck(p->m_nTurn))
                next_depth += ExtendInCheck;

            if (next_depth >= 0) {
                int effective_alpha =
                    is_alternate ? (alpha - ALTERNATE_DELTA) : alpha;
#if MP
                tmp = -sd->NegaScout(-beta, -effective_alpha, next_depth,
                                 is_pv ? PVNode : CutNode, 0);
#else
                tmp = -sd->NegaScout(-beta, -effective_alpha, next_depth,
                                 is_pv ? PVNode : CutNode);
#endif
                if (is_alternate) {
                    sd->m_nAlternateScore = tmp;
                }
            } else {
                tmp = -sd->Quies(-beta, -alpha, 0);
            }
            p->UndoMove(move);
            if (AbortSearch)
                goto final;

            if (is_pv && tmp <= alpha) {

                /*
                 * Fail low on principal variation.
                 * Open window, take some time, and re-search.
                 */

                pv_stable = false;

                if (sd->m_fMaster && PrintOK) {
                    char san_buffer[32];
                    SearchOutputFailHighLow(
                        sd->m_wDepth, CurTime - StartTime, false,
                        p->NumberedSAN(move, san_buffer, sizeof(san_buffer)),
                        sd->m_ulNodesCount + sd->m_ulQNodesCount);
                }

                NeedTime = true;

                beta = tmp;
                alpha = tmp - ResearchWindow;

                p->DoMove(move);
                if (next_depth >= 0) {
#if MP
                    tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode, 0);
#else
                    tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode);
#endif
                } else {
                    tmp = -sd->Quies(-beta, -alpha, 0);
                }
                p->UndoMove(move);
                if (AbortSearch)
                    goto final;

                if (tmp <= alpha) {
                    beta = tmp;
                    alpha = -INF;

                    p->DoMove(move);
                    if (next_depth >= 0) {
#if MP
                        tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode,
                                         0);
#else
                        tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode);
#endif
                    } else {
                        tmp = -sd->Quies(-beta, -alpha, 0);
                    }
                    p->UndoMove(move);
                    if (AbortSearch)
                        goto final;
                }
                nodes[sd->m_wMoveNum] = sd->m_ulNodesCount - nodes[sd->m_wMoveNum];
            } else if (tmp >= beta) {

                /*
                 * Fail high.
                 * Re-search with open window.
                 */

                pv_stable = false;

                if (sd->m_wMoveNum != 0) {
                    unsigned long tn = nodes[sd->m_wMoveNum];
                    int j;

                    for (j = sd->m_wMoveNum; j > 0; j--) {
                        mvs[j] = mvs[j - 1];
                        nodes[j] = nodes[j - 1];
                    }
                    mvs[0] = move;
                    nodes[0] = tn;

                    if (!(move.IsTactical()))
                        sd->PutKiller(move);
                    PBMove = M_NONE;
                    is_pv = true;

                    FHTime = (CurTime - StartTime) / ONE_SECOND;
                }

                if (sd->m_fMaster && PrintOK) {
                    char san_buffer[32];
                    SearchOutputFailHighLow(
                        sd->m_wDepth, CurTime - StartTime, true,
                        p->NumberedSAN(mvs[0], san_buffer, sizeof(san_buffer)),
                        sd->m_ulNodesCount + sd->m_ulQNodesCount);
                }

                alpha = tmp;
                beta = tmp + ResearchWindow;

                p->DoMove(move);
                if (next_depth >= 0) {
#if MP
                    tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode, 0);
#else
                    tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode);
#endif
                } else {
                    tmp = -sd->Quies(-beta, -alpha, 0);
                }
                p->UndoMove(move);
                if (AbortSearch)
                    goto final;

                if (tmp >= beta) {
                    alpha = tmp;
                    beta = INF;

                    p->DoMove(move);
                    if (next_depth >= 0) {
#if MP
                        tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode,
                                         0);
#else
                        tmp = -sd->NegaScout(-beta, -alpha, next_depth, PVNode);
#endif
                    } else {
                        tmp = -sd->Quies(-beta, -alpha, 0);
                    }
                    p->UndoMove(move);
                    if (AbortSearch)
                        goto final;
                }
                nodes[0] = sd->m_ulNodesCount - nodes[0];
            } else {
                nodes[sd->m_wMoveNum] = sd->m_ulNodesCount - nodes[sd->m_wMoveNum];
            }

            if (AbortSearch)
                goto final;

            if (is_pv) {
                sd->m_nBestScore = tmp;

                if (sd->m_fMaster) {
                    char score_as_text[16];
                    p->AnalyzeHT(mvs[0]);
                    pv_valid = true;

                    snprintf(AnalysisLine, sizeof(AnalysisLine),
                             "%2d: (%7s) %s", sd->m_wDepth,
                             FormatScore(sd->m_nBestScore, score_as_text,
                                         sizeof(score_as_text)),
                             BestLine);

                    if (PrintOK) {
                        SearchOutput(sd->m_wDepth, CurTime - StartTime,
                                     (p->m_nTurn) ? -sd->m_nBestScore
                                               : sd->m_nBestScore,
                                     BestLine, sd->m_ulNodesCount + sd->m_ulQNodesCount);

                        any_pv_printed = true;
                    }
                }

                alpha = sd->m_nBestScore;
                beta = sd->m_nBestScore + 1;
                is_pv = false;
            }

            if (sd->m_fMaster && sd->m_wMoveNum == 0 && !NeedTime &&
                CurTime > SoftLimit) {
                if (SearchMode == Searching) {
                    AbortSearch = true;
                    goto final;
                } else if (SearchMode == Pondering) {
                    DoneAtRoot = true;
                }
            }
        }

        if (sd->m_fMaster && (PrintOK || (sd->m_wDepth > MateDepth &&
                                       (sd->m_nBestScore < -CMLIMIT ||
                                        sd->m_nBestScore > CMLIMIT)))) {
            SearchOutput(sd->m_wDepth, CurTime - StartTime,
                         (p->m_nTurn) ? -sd->m_nBestScore : sd->m_nBestScore, BestLine,
                         sd->m_ulNodesCount + sd->m_ulQNodesCount);

            any_pv_printed = true;
        }

        if (sd->m_nBestScore < -CMLIMIT || sd->m_nBestScore > CMLIMIT) {
            if (last > CMLIMIT && sd->m_nBestScore >= last &&
                sd->m_wDepth > MateDepth) {
                if (SearchMode == Searching)
                    break;
                else
                    DoneAtRoot = true;
            }
            if (SearchMode == Searching && last < CMLIMIT &&
                sd->m_nBestScore <= last && sd->m_wDepth > MateDepth)
                break;
            last = sd->m_nBestScore;
        }

        NeedTime = false;
        ResortMovesList(sd->m_wRootMoves, mvs, nodes);

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

        if (sd->m_wDepth > 3) {
            /*
             * Do ten checks per second.
             */

            elapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

            NodesPerCheck =
                (elapsed == 0.0)
                    ? 1000
                    : (int)((sd->m_ulNodesCount + sd->m_ulQNodesCount) / elapsed / 10);
        }

        if (SearchMode == Puzzling && sd->m_wDepth > 4)
            break;

        if (sd->m_fMaster &&
            ((CurTime > SoftLimit) || (pv_stable && CurTime > SoftLimit2))) {
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
    elapsed = (double)(CurTime - StartTime) / (double)ONE_SECOND;

    if (sd->m_fMaster) {
        if (pv_valid && !any_pv_printed) {
            // Make sure there is a PV printed
            SearchOutput(sd->m_wDepth, CurTime - StartTime,
                         (p->m_nTurn) ? -sd->m_nBestScore : sd->m_nBestScore, BestLine,
                         sd->m_ulNodesCount + sd->m_ulQNodesCount);
        }

        char buf1[16], buf2[16], buf3[16], buf4[16], buf5[16], buf6[16],
            buf7[16];

        unsigned long nps = (unsigned long)(TotalNodes / elapsed);

        Print(2, "Nodes = %s, QPerc: %d %%, time = %g secs, %s nodes/s\n",
              FormatCount(TotalNodes, buf1, sizeof(buf1)),
              Percentage(sd->m_ulQNodesCount, sd->m_ulNodesCount + sd->m_ulQNodesCount),
              elapsed, FormatCount(nps, buf2, sizeof(buf2)));

        Print(2,
              "Extensions: Check: %s  DblChk: %s  DiscChk: %s  SingReply: %s\n"
              "            Recapture: %s   Passed Pawn: %s   Zugzwang: %s\n",
              FormatCount(ChkExt, buf1, sizeof(buf1)),
              FormatCount(DblExt, buf2, sizeof(buf2)),
              FormatCount(DiscExt, buf3, sizeof(buf3)),
              FormatCount(SingExt, buf4, sizeof(buf4)),
              FormatCount(RCExt, buf5, sizeof(buf5)),
              FormatCount(PPExt, buf6, sizeof(buf6)),
              FormatCount(ZZExt, buf7, sizeof(buf7)));

        Print(2,
              "Hashing: Trans: %s/%s = %d %%   Pawn: %s/%s = %d %%\n"
              "         Eval: %s/%s = %d %%\n",
              FormatCount(HHit, buf1, sizeof(buf1)),
              FormatCount(HTry, buf2, sizeof(buf2)), Percentage(HHit, HTry),
              FormatCount(PHit, buf3, sizeof(buf3)),
              FormatCount(PTry, buf4, sizeof(buf4)), Percentage(PHit, PTry),
              FormatCount(SHit, buf5, sizeof(buf5)),
              FormatCount(STry, buf6, sizeof(buf6)), Percentage(SHit, STry));

        if (EGTBProbe != 0) {
            Print(2, "EGTB Hits/Probes = %s/%s\n",
                  FormatCount(EGTBProbeSucc, buf1, sizeof(buf1)),
                  FormatCount(EGTBProbe, buf2, sizeof(buf2)));
        }

        ShowHashStatistics();
    }

    sd->m_BestMove = mvs[0];

    if (!sd->m_fMaster) {
        CPosition::Free(sd->m_pPosition);
        delete sd;
    }

    return NULL;
}

#if MP && HAVE_LIBPTHREAD
pthread_t *tids = NULL;
#endif /* MP && HAVE_LIBPTHREAD */

#if MP

/*
 * In parallel search stop all helper threads
 */

void StopHelpers(void) {
#if HAVE_LIBPTHREAD
    if (tids) {
        int nthread;
        void *dummy;

        AbortSearch = true;
        for (nthread = 0; nthread < (NumberOfCPUs - 1); nthread++) {
            pthread_join(tids[nthread], &dummy);
        }
        free(tids);
        tids = NULL;
    }
#endif /* HAVE_LIBPTHREAD */
}

#endif /* MP */

/**
 * Set the maximum depth for the root search.
 */
void setMaxSearchDepth(int max_search_depth) {
    if (max_search_depth > 0 && max_search_depth < (MAX_TREE_SIZE - 1)) {
        MaxSearchDepth = max_search_depth;
    }
}

