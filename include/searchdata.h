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

#ifndef SEARCHDATA_H
#define SEARCHDATA_H

#include "dbase.h"
#include <stdint.h>

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

    CMove m_rgPvSave[CBitBoard::SIZE];

    uint16_t m_wPly;

    bool m_fMaster; /* true if a master process */
    unsigned long m_ulNodesCount, m_ulQNodesCount, m_ulCheckNodesCount;

    CMove m_BestMove;
    int m_nBestScore;
    uint16_t m_wDepth;

    CMove m_AlternateMove;
    int m_nAlternateScore;

    uint16_t m_wRootMoves;
    uint16_t m_wMoveNum;

    explicit CSearchData(CPosition *pPosition);
    ~CSearchData();
    void EnterNode();
    void LeaveNode();
    CMove NextMove();
    CMove NextEvasion();
    CMove NextMoveQ(int nAlpha);
    void PutKiller(CMove mvMove);
    bool TerminateSearch();
    void InitSearch();
    void StoreResult(int nScore, int nAlpha, int nBeta, CMove mvMove, int nDepth, int nThreat);
    int Quies(int nAlpha, int nBeta, int nDepth);
#if MP
    int NegaScout(int nAlpha, int nBeta, int nDepth, int nNodeType, int nExclusiveP);
#else
    int NegaScout(int nAlpha, int nBeta, int nDepth, int nNodeType);
#endif
};

#endif
