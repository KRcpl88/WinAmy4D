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

#ifndef SEARCH_H
#define SEARCH_H

#include "config.h"
#include "dbase.h"
#include "searchdata.h"
#include <stdint.h>

#define INF 200000 /* max. score */
#define CMLIMIT                                                                \
    100000 /* scores above this (or below -CMLIMIT)                            \
            * indicate checkmate */
#define ON_EVALUATION (INF + 1)

#define MAX_TREE_SIZE 64 /* maximum depth we will search to */

typedef enum {
    PB_NO_PB_MOVE = 0,
    PB_NO_PB_HIT,
    PB_HIT,
    PB_ALT_COMMAND

} pb_result_t;

extern int ExtendInCheck;
extern int ExtendDoubleCheck;
extern int ExtendDiscoveredCheck;
extern int ExtendSingularReply;
extern int ExtendPassedPawn;
extern int ExtendZugzwang;
extern int ReduceNullMove;
extern int ReduceNullMoveDeep;
extern int16_t ExtendRecapture[];

extern unsigned long FHTime;
extern bool AbortSearch;

#if MP
extern int NumberOfCores;
#define MAX_SEARCH_THREADS 32
#endif

void SetMaxSearchDepth(int);

/*
 * Root-move exclusion (used to emulate MultiPV by repeated searches).
 *
 * SetExcludedRootMoves installs a set of moves that the root search
 * (IterateInt) will skip when it generates the root move list, so a subsequent
 * Iterate() returns the best move *other than* the excluded ones. This lets a
 * caller find the 2nd/3rd best root moves by searching repeatedly, excluding the
 * previously found best move(s) each time.
 *
 * The state is a process-global mirror of the MaxSearchDepth pattern and is
 * therefore NOT re-entrant: callers must not run two engine searches with
 * different exclusion sets concurrently. The default (empty) set leaves the
 * search completely unchanged, so the console engine and unit tests — which
 * never set exclusions — are unaffected. Always pair a Set call with a Clear
 * once the search completes.
 */
void SetExcludedRootMoves(const CMove *pMoves, uint16_t cMoves);
void ClearExcludedRootMoves(void);

#if MP
void StopHelpers(void);
void SetSearchThreadBackgroundPriority(void);
#endif

/*
 * Search mode constants.
 */
enum {
    Searching = 1,
    Pondering = 2,
    Puzzling = 3,
    Analyzing = 4,
    Interrupted = 5
};

/*
 * Node type constants used by NegaScout and IterateInt.
 */
enum { PVNode = 0, AllNode = 1, CutNode = 2, CutNodeNoNull = 3 };

/*
 * Fractional-ply value: one logical ply equals OnePly internal depth units.
 * See D. Levy et al., "The SEX Algorithm in Computer Chess."
 */
static const int OnePly = 16;

/* Aspiration window and re-search widths. */
static const int PVWindow = 250;
static const int ResearchWindow = 1500;

/* Minimum depth before an early mate-score exit is allowed. */
static const int MateDepth = 3;

/*
 * Search-state globals defined in search.cpp.
 */
extern int MaxDepth;
extern int MaxSearchDepth;
extern int SearchMode;
extern int DoneAtRoot;
extern bool NeedTime;
extern int PrintOK;
extern int NodesPerCheck;
extern unsigned long SoftLimit, SoftLimit2, HardLimit;
extern unsigned long StartTime, CurTime;
extern uint16_t cExcludedRootMoves;
extern CMove rgExcludedRootMoves[];
extern char BestLine[2048];
extern char AnalysisLine[4096];
extern CMove PBMove;
extern unsigned long RCExt, ChkExt, DiscExt, DblExt, SingExt, PPExt, ZZExt;

/*
 * Sort root moves by the node counts gathered in the previous iteration.
 * Moves with higher node counts (i.e., moves the engine spent more time on)
 * are moved towards the front so they are searched first in the next depth.
 */
void ResortMovesList(int nCnt, CMove *pMvs, unsigned long *pNodes);

#endif
