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
extern int NumberOfCPUs;
#endif

typedef enum {
    HashMove,
    GenerateCaptures,
    GainingCapture,
    Killer1,
    Killer2,
    CounterMv,
    Killer3,
    GenerateRest,
    LoosingCapture,
    HistoryMoves,
    Done,
    GenerateQChecks,
    QChecks
} SearchPhase;

struct SSearchStatus {
    SearchPhase st_phase;
    CMove st_hashmove;
    CMove st_k1, st_k2, st_kl, st_cm, st_k3;
};

struct SKillerEntry {
    CMove killer1, killer2;    /* killer moves */
    uint32_t kcount1, kcount2; /* killer count */
};

#if MP
struct HTEntry;
#endif

class CSearchData {
  public:
    CPosition *m_pPosition;

    struct SSearchStatus *m_pCurrent;
    struct SSearchStatus *m_pStatusTable;
    struct SKillerEntry *m_pKiller;
    struct SKillerEntry *m_pKillerTable;
#if MP
    struct HTEntry *m_pLocalHashTable;
    heap_t m_hDeferredHeap;
#endif

    heap_t m_hHeap;
    int32_t *m_pnDataHeap;
    unsigned int m_uDataHeapSize;

    CMove m_rgCounterTab[2][4096];      /* counter moves per side */
    unsigned int m_rguHistoryTab[2][4096]; /* history moves per side */

    CMove m_rgPvSave[64];

    uint16_t m_wPly;

    bool m_fMaster; /* true if a master process */
    unsigned long m_ulNodesCount, m_ulQNodesCount, m_ulCheckNodesCount;

    CMove m_mvBestMove;
    int m_nBestScore;
    uint16_t m_wDepth;

    CMove m_mvAlternateMove;
    int m_nAlternateScore;

    uint16_t m_wRootMoves;
    uint16_t m_wMoveNum;

    /**
     * Description: Creates and initializes per-search state for move ordering and node bookkeeping.
     * Inputs: pPosition - position used by this search context.
     * Outputs: Initializes all members and allocates internal heaps/tables.
     */
    explicit CSearchData(CPosition *pPosition);
    /**
     * Description: Releases all heap/table resources owned by this search context.
     * Inputs: None.
     * Outputs: Frees all dynamically allocated members.
     */
    ~CSearchData();
    /**
     * Description: Enters one search ply and initializes phase state for move generation at that ply.
     * Inputs: None.
     * Outputs: Increments ply state and pushes heap sections.
     */
    void EnterNode();
    /**
     * Description: Leaves one search ply and restores parent search state.
     * Inputs: None.
     * Outputs: Pops heap sections and decrements ply state.
     */
    void LeaveNode();
    /**
     * Description: Produces the next legal move from the normal move generator in ordering sequence.
     * Inputs: None.
     * Outputs: Returns next move or M_NONE when exhausted.
     */
    CMove NextMove();
    /**
     * Description: Produces the next legal move from the in-check evasion generator in ordering sequence.
     * Inputs: None.
     * Outputs: Returns next evasion move or M_NONE when exhausted.
     */
    CMove NextEvasion();
    /**
     * Description: Produces the next tactical move for quiescence search.
     * Inputs: nAlpha - current alpha bound used for pruning tactical generation.
     * Outputs: Returns next quiescence move or M_NONE when exhausted.
     */
    CMove NextMoveQ(int nAlpha);
    /**
     * Description: Updates killer move tables for the current ply using a newly found cutoff move.
     * Inputs: mvMove - candidate killer move.
     * Outputs: Updates killer entries and usage counters.
     */
    void PutKiller(CMove mvMove);
};

CMove Iterate(CPosition *, int *, CMove, int *);
void SearchRoot(CPosition *);
void AnalysisMode(CPosition *);
pb_result_t PermanentBrain(CPosition *);
int QuiescenceSearch(CPosition *);
void setMaxSearchDepth(int);

#if MP
void StopHelpers(void);
#endif

#endif
