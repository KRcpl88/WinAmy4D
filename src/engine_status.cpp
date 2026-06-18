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
 * engine_status.cpp - thread-safe engine status channel (see engine_status.h)
 */

#include "engine_status.h"

#include "position.h"
#include "search.h"
#include "searchdata.h"

CEngineStatus::CEngineStatus()
    : m_pPosition(nullptr), m_pSearchData(nullptr), m_LastMove(M_NONE),
      m_nCapturedPiece(Neutral), m_State(STATE_WAITING) {}

CEngineStatus::~CEngineStatus() {
    // CEngineStatus only holds non-owning pointers, so there is nothing to
    // free here. The CPosition and CSearchData are owned by the engine /
    // state machine that published them.
}

void CEngineStatus::SetPosition(CPosition *pPosition) {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_pPosition = pPosition;
}

void CEngineStatus::SetSearchData(CSearchData *pSearchData) {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_pSearchData = pSearchData;
}

void CEngineStatus::SetLastMove(CMove Move, int8_t nCapturedPiece) {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_LastMove = Move;
    m_nCapturedPiece = nCapturedPiece;
}

void CEngineStatus::SetState(ui_state_t State) {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    m_State = State;
}

CPosition *CEngineStatus::GetPositionClone() const {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    if (m_pPosition == nullptr) {
        return nullptr;
    }
    return CPosition::Clone(m_pPosition);
}

SEngineStatusSnapshot CEngineStatus::BuildSnapshotLocked() const {
    SEngineStatusSnapshot Snapshot;

    Snapshot.LastMove = m_LastMove;
    Snapshot.nCapturedPiece = m_nCapturedPiece;
    Snapshot.State = m_State;

    if (m_pPosition != nullptr) {
        Snapshot.nSideToMove = m_pPosition->GetTurn();
        Snapshot.nMoveNumber = (m_pPosition->GetPly() / 2) + 1;
    } else {
        Snapshot.nSideToMove = White;
        Snapshot.nMoveNumber = 1;
    }

    Snapshot.nSearchDepth = GetMaxSearchDepth();

    if (m_pSearchData != nullptr) {
        Snapshot.nTotalMoves = m_pSearchData->m_wRootMoves;
        Snapshot.nMovesSearched = m_pSearchData->m_wMoveNum;
        Snapshot.nIteration = m_pSearchData->m_wDepth;
    } else {
        Snapshot.nTotalMoves = 0;
        Snapshot.nMovesSearched = 0;
        Snapshot.nIteration = 0;
    }

    return Snapshot;
}

SEngineStatusSnapshot CEngineStatus::GetSnapshot() const {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    return BuildSnapshotLocked();
}

int CEngineStatus::GetProgressPercent(int *pnMoveNumber, int *pnSideToMove,
                                      ui_state_t *pState) const {
    std::lock_guard<std::mutex> Lock(m_Mutex);
    const SEngineStatusSnapshot Snapshot = BuildSnapshotLocked();

    if (pnMoveNumber != nullptr) {
        *pnMoveNumber = Snapshot.nMoveNumber;
    }
    if (pnSideToMove != nullptr) {
        *pnSideToMove = Snapshot.nSideToMove;
    }
    if (pState != nullptr) {
        *pState = Snapshot.State;
    }

    const int nTotalMoves = Snapshot.nTotalMoves;
    if (nTotalMoves <= 0) {
        return -1;
    }

    // Iterative deepening searches every root move once per iteration. The root
    // loop runs depths 1 .. (max search depth - 1), so the number of iterations
    // is one less than the configured search depth. Progress climbs smoothly
    // from 0 to 100 across the whole search rather than resetting each
    // iteration.
    int nIterations = Snapshot.nSearchDepth - 1;
    if (nIterations < 1) {
        nIterations = 1;
    }

    int nDepth = Snapshot.nIteration;
    if (nDepth < 1) {
        nDepth = 1;
    }
    if (nDepth > nIterations) {
        nDepth = nIterations;
    }

    int nDone = Snapshot.nMovesSearched;
    if (nDone < 0) {
        nDone = 0;
    }
    if (nDone > nTotalMoves) {
        nDone = nTotalMoves;
    }

    const long lDone = (long)(nDepth - 1) * nTotalMoves + nDone;
    const long lAll = (long)nIterations * nTotalMoves;
    if (lAll <= 0) {
        return -1;
    }

    int nPercent = (int)((lDone * 100) / lAll);
    if (nPercent < 0) {
        nPercent = 0;
    }
    if (nPercent > 100) {
        nPercent = 100;
    }
    return nPercent;
}
