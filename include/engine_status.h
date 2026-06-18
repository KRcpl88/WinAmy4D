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

#ifndef ENGINE_STATUS_H
#define ENGINE_STATUS_H

#include "dbase.h"          // CPosition, CMove
#include "state_machine.h"  // ui_state_t

#include <cstdint>
#include <mutex>

class CSearchData;

// ---------------------------------------------------------------------------
// SEngineStatusSnapshot
//
// A consistent, point-in-time copy of the engine's externally visible status.
// The whole struct is filled while CEngineStatus holds its lock exactly once,
// so the GUI can read every field together without observing a half-updated
// state and without repeatedly taking the lock.
// ---------------------------------------------------------------------------
struct SEngineStatusSnapshot {
    // The move most recently played by the engine (M_NONE if none yet).
    CMove LastMove;
    // The piece captured by LastMove, or Neutral if it was not a capture.
    int8_t nCapturedPiece;
    // Side to move in the current position (White / Black).
    int8_t nSideToMove;
    // Full-move number of the current position ((ply / 2) + 1).
    int nMoveNumber;
    // Current state-machine state (waiting / calculating / ...).
    ui_state_t State;

    // ---- Search-progress fields (snapshot of the live CSearchData) ----
    // Number of root moves the in-flight search will examine, or 0 when no
    // search is active.
    int nTotalMoves;
    // Number of root moves examined so far at the current iteration depth.
    int nMovesSearched;
    // Configured maximum search depth (the iterative-deepening target).
    int nSearchDepth;
    // Current iterative-deepening iteration (root-search depth).
    int nIteration;
};

// ---------------------------------------------------------------------------
// CEngineStatus
//
// Thread-safe channel through which the engine / state machine publishes its
// progress to an observer (e.g. the GUI status bar). The src engine library
// historically reported status by printing to stdout, which a windowed host
// cannot capture; CEngineStatus replaces that for GUI consumers without
// changing how the engine actually plays.
//
// The engine side calls the Set* methods; the observer side calls the Get*
// methods. All access is serialised by a single mutex. The status holds only
// NON-OWNING pointers to the live CPosition and CSearchData; it never frees
// them. The observer never receives those pointers directly — it obtains a
// CPosition *clone* (GetPositionClone) or a value snapshot (GetSnapshot).
// ---------------------------------------------------------------------------
class CEngineStatus {
  public:
    CEngineStatus();
    ~CEngineStatus();

    CEngineStatus(const CEngineStatus &) = delete;
    CEngineStatus &operator=(const CEngineStatus &) = delete;

    // ---- Engine-side mutators (all take the lock) ----

    // Publish the position the state machine is currently operating on. Stored
    // as a non-owning pointer; the caller retains ownership.
    void SetPosition(CPosition *pPosition);

    // Publish the CSearchData driving the in-flight search (or null when the
    // search ends). Stored as a non-owning pointer; the caller retains
    // ownership and must clear it (SetSearchData(nullptr)) before destroying
    // the data so a stale pointer is never read.
    void SetSearchData(CSearchData *pSearchData);

    // Record the move the engine just played and the piece it captured
    // (Neutral when the move was not a capture).
    void SetLastMove(CMove Move, int8_t nCapturedPiece);

    // Publish the current state-machine state.
    void SetState(ui_state_t State);

    // ---- Observer-side accessors (all take the lock) ----

    // Return a freshly allocated clone of the current position, or null if no
    // position has been published. Ownership transfers to the caller, which
    // must free it with CPosition::Free.
    CPosition *GetPositionClone() const;

    // Atomically capture the full engine status (last move, captured piece,
    // side to move, move number, state, and search-progress figures) under a
    // single lock acquisition.
    SEngineStatusSnapshot GetSnapshot() const;

    // Convenience helper for the GUI status bar: returns the in-flight search
    // progress as a whole-number percentage in [0, 100], or -1 when no search
    // is active (or progress is not yet known). When non-null, the out
    // parameters additionally receive the current full-move number, side to
    // move, and state-machine state, so the GUI can detect when to refresh the
    // board. Acquires the lock once for everything.
    int GetProgressPercent(int *pnMoveNumber, int *pnSideToMove,
                           ui_state_t *pState) const;

  private:
    // Fill a snapshot from the current members. The caller must hold m_Mutex.
    SEngineStatusSnapshot BuildSnapshotLocked() const;

    mutable std::mutex m_Mutex;

    CPosition *m_pPosition;     // non-owning
    CSearchData *m_pSearchData; // non-owning
    CMove m_LastMove;
    int8_t m_nCapturedPiece;
    ui_state_t m_State;
};

#endif // ENGINE_STATUS_H
