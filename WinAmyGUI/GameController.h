#pragma once

#include <windows.h>

#include "dbase.h"
#include "hashtable.h"
#include "heap.h"
#include "init.h"
#include "move.h"
#include "movedata.h"
#include "search.h"
#include "scoord.h"
#include "time_ctl.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Posted to the main window when the engine finishes a search.
// WPARAM: the search generation token (see GetSearchGeneration()); the host
//         ignores the message if it does not match the current generation,
//         which is how a search aborted by a user move is discarded.
// LPARAM: unused (0); the move is read via GetBestMove().
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

// Posted to the main window when a move-suggestion (hint) search finishes.
// The engine does NOT apply the move; the host highlights it as a
// recommendation. WPARAM carries the search generation token (see
// WM_APP_ENGINE_MOVE); LPARAM unused; the move is read via GetBestMove().
#define WM_APP_ENGINE_HINT  (WM_APP + 2)

// Posted to the main window when a strategy computation finishes. The result
// (a formatted multi-line strategy description) is read via GetStrategyText().
// WPARAM carries the search generation token (see WM_APP_ENGINE_MOVE); LPARAM
// unused.
#define WM_APP_ENGINE_STRATEGY  (WM_APP + 3)

// Posted to the main window periodically while a strategy computation is in
// progress so the host can refresh the status bar with a "% complete" figure.
// The percentage is read via GetStrategyProgressPercent(). WPARAM/LPARAM unused.
#define WM_APP_ENGINE_PROGRESS  (WM_APP + 4)

enum class PlayerMode {
    ZeroPlayers = 0, // self-play
    OnePlayer   = 1, // human vs engine
    TwoPlayers  = 2  // human vs human
};

class GameController {
public:
    // Number of consecutive invalid moves tolerated before the engine is
    // declared corrupted (see MakeMove / IsEngineCorrupted).
    static constexpr int kMaxRejectRetries = 3;

    GameController();
    ~GameController();

    // One-time engine initialisation — call once at application start.
    static void InitEngine();

    // Start a new game from the initial position.
    void NewGame();
    bool LoadFromEPD(const char *pszEPD);
    bool LoadFromEPDFile(const wchar_t *pszPath);
    bool SaveToEPDFile(const wchar_t *pszPath);

    // Load / save the full game (move list) in PGN format. The 4D board is
    // represented using the level-aware SAN notation (a destination square is
    // written as <level><file><rank>, e.g. "ea2a3"), so the on-disk format is
    // standard PGN whose move text encodes the extra (3D/4D) dimension. The
    // conventional extension for these files is ".pgn4".
    bool LoadFromPGNFile(const wchar_t *pszPath);
    bool SaveToPGNFile(const wchar_t *pszPath);

    // Set the engine search depth (1–9).
    void SetDepth(int depth);

    // Return current search depth.
    int  GetDepth() const { return m_nDepth; }

    // Set a fixed per-move search time limit, in seconds. The engine searches as
    // deeply as it can within the given number of seconds. Changing the limit
    // invalidates any cached strategy result (which was computed under the
    // previous limit) so the next "recommend strategy" re-searches.
    void SetTimeLimit(int seconds);

    // Return the current fixed per-move time limit in seconds, or 0 if search
    // is depth-based.
    int  GetTimeLimit() const { return m_nTimeLimit; }

    // Set player mode (0 / 1 / 2 human players).
    void SetPlayerMode(PlayerMode mode) { m_PlayerMode = mode; }
    PlayerMode GetPlayerMode() const    { return m_PlayerMode; }

    // Apply a move to the current position. Must NOT be called while engine is
    // thinking. Returns true if the move was legal and applied, false if it was
    // rejected as not valid (in which case the position is left unchanged and an
    // internal consecutive-rejection counter is advanced; see IsEngineCorrupted).
    bool MakeMove(CMove move);

    // Returns true once the engine has retried the SAME invalid move
    // kMaxRejectRetries times in a row. When this happens MakeMove also switches
    // the player mode to TwoPlayers (human only) so the host can stop searching
    // and let the user save the game. The flag is cleared by NewGame() and by
    // any subsequently applied valid move.
    bool IsEngineCorrupted() const { return m_fEngineCorrupted; }

    // Undo the last full move in 1-player mode: reverts the engine's reply and
    // the human player's preceding move so the human may move again. Returns true
    // if any move was undone. Must NOT be called while the engine is thinking.
    bool UndoLastHumanMove();

    // Start the engine search asynchronously. The result is posted as
    // WM_APP_ENGINE_MOVE to hwndTarget when the search completes.
    void StartEngineSearch(HWND hwndTarget);

    // Start a move-suggestion (hint) search asynchronously. Identical to
    // StartEngineSearch except the completion is posted as WM_APP_ENGINE_HINT
    // so the host can highlight the move as a recommendation without applying
    // it. The best move is retrieved via GetBestMove().
    void StartHintSearch(HWND hwndTarget);

    // Start an asynchronous strategy computation for the current player. The
    // engine searches a clone of the current position to find the top 3 moves
    // (via repeated full-time searches that each exclude the previously found
    // best move), and for each one the opponent's most likely counter move and
    // the recommended response after that. The completion is posted as
    // WM_APP_ENGINE_STRATEGY to hwndTarget; the formatted result is retrieved
    // via GetStrategyText().
    void StartStrategySearch(HWND hwndTarget);

    // Request the engine to stop searching (sets AbortSearch).
    void PauseEngine();

    // Stop the in-flight search and block until the engine thread has fully
    // finished, so the shared engine state (hash table, search globals) is free
    // before the caller mutates the position or starts a new search. The
    // search's completion message (already posted by the finishing thread) is
    // made stale by advancing the search generation, so the host ignores its
    // now-irrelevant result. Safe to call when no search is running. Must be
    // called on the UI thread (it may briefly block the message pump).
    void AbortAndJoinSearch();

    // Monotonic token identifying the most recently started search. Each engine
    // completion message (move / hint / strategy) carries the generation that
    // was current when its search started; the host compares it against this
    // value and discards messages whose generation no longer matches (e.g. a
    // search aborted because the user made a move). See AbortAndJoinSearch().
    uint32_t GetSearchGeneration() const { return m_nSearchGen.load(); }

    // Returns true while the engine thread is running.
    bool IsEngineRunning() const { return m_fEngineRunning.load(); }

    // Returns true while a strategy computation is in progress (see
    // StartStrategySearch). Used to show a "Thinking…" status message.
    bool IsComputingStrategy() const { return m_fComputingStrategy.load(); }

    // Progress of the in-flight strategy computation, as a whole-number percent
    // in [0, 100] (moves searched so far divided by the total number of searches
    // the computation will perform). Returns -1 when no total has been
    // established yet (e.g. before the candidate pool has been enumerated).
    int GetStrategyProgressPercent() const {
        const int nTotal = m_nProgressTotal.load();
        if (nTotal <= 0) {
            return -1;
        }
        int nDone = m_nProgressDone.load();
        if (nDone > nTotal) {
            nDone = nTotal;
        }
        return (nDone * 100) / nTotal;
    }

    // Progress of the in-flight single-move search (engine move or suggest-move
    // / hint), as a whole-number percent in [0, 100]. Because such a search is
    // bounded by a fixed per-move time budget, progress is measured as elapsed
    // time divided by that budget. Returns -1 when no time budget is in effect
    // (a pure depth-limited search), in which case the host shows an
    // indeterminate "thinking" message instead of a percentage.
    int GetEngineSearchProgressPercent() const;

    // Number of whole seconds remaining on the current search's countdown, for
    // display in the status bar. The countdown runs from (search start) to
    // (search start + time budget), tracking the engine's search clock directly.
    // If the search finishes first the host clears the countdown; if the
    // countdown reaches zero first it holds at zero until the search returns
    // (a couple of seconds at most). Applies to both single-move searches and
    // strategy computations (whose budget spans all of their internal searches).
    // Returns -1 when no countdown should be shown — i.e. when the configured
    // time limit is 5 seconds or less (too short to be worth a countdown) or no
    // time budget is in effect.
    int GetSearchCountdownSeconds() const;

    // Access the current position (read-only while engine is running).
    const CPosition* GetPosition() const { return m_pPosition; }
    CPosition*       GetPosition()       { return m_pPosition; }

    // Returns true if the game has ended (checkmate / stalemate / draw).
    bool IsGameOver() const;

    // Returns the game-end string or nullptr if game is still in progress.
    const char* GetGameEndMessage() const;

    // Returns a human-friendly description of the game result (outcome, which
    // side won, and in how many moves), or an empty string if the game is
    // still in progress.
    std::string GetGameResultText() const;

    // Returns a description of the most recently played move for display in the
    // status bar, formatted as "<Side> move #<N> <SAN>" (e.g.
    // "White move #7 Phe2he3"), where <Side> is the side that played the move,
    // <N> is the full-move number it belongs to, and <SAN> is the engine's
    // level-aware SAN for that move. Returns an empty string when no move has
    // been played yet (the game is still at its starting position).
    std::string GetLastMoveText() const;

    // Retrieve the best move found by the last engine search.
    CMove GetBestMove() const { return m_BestMove; }

    // Retrieve the formatted strategy text produced by the last strategy
    // computation (see StartStrategySearch).
    std::string GetStrategyText() const { return m_strStrategy; }

    // Returns true if a strategy result has been computed for the current
    // position and is still valid (i.e. the position has not changed since).
    // The cache is invalidated whenever the position changes (a move is made
    // or undone, a new game starts, or a position is loaded), so the strategy
    // search only needs to run once per position.
    bool HasStrategy() const { return m_fStrategyValid; }

    // Retrieve the top-ranked move (Suggested Move #1) from the last strategy
    // computation. Valid only when HasStrategy() is true; returns M_NONE if the
    // strategy produced no move (e.g. no legal moves). The strategy candidates
    // are ranked best-first, so this is the engine's single best recommendation.
    CMove GetStrategyBestMove() const { return m_StrategyBestMove; }

    // Retrieve all ranked moves from the last strategy computation.
    std::vector<CMove> GetStrategyMoves() const { return m_StrategyMoves; }

private:
    // Shared implementation for StartEngineSearch / StartHintSearch. Clones the
    // current position, searches the clone on a background thread, stores the
    // result in m_BestMove, and posts uCompletionMsg to hwndTarget.
    void StartSearchInternal(HWND hwndTarget, UINT uCompletionMsg);

    // Compute the strategy text by searching a clone of the current position.
    // Returns a formatted, human-readable multi-line description. Runs on the
    // engine thread (see StartStrategySearch); never mutates m_pPosition.
    // Posts WM_APP_ENGINE_PROGRESS to hwndTarget as candidates are searched so
    // the host can show a progress percentage.
    std::string ComputeStrategyText(HWND hwndTarget);

    // Discard any cached strategy result so the next strategy request recomputes
    // it. Called whenever the position changes (move made/undone, new game,
    // position loaded). Callers must hold m_PositionMutex.
    void InvalidateStrategy();

    // Configure the engine's search-termination limits before starting a search.
    // A fixed per-move time limit (m_nTimeLimit > 0) is always used: the engine
    // searches as deeply as the chosen interval allows.
    void ApplySearchLimits();

    // Configure the search-termination limits for a strategy computation. The
    // strategy now performs up to three full-time searches (one per ranked move,
    // see ComputeStrategyText), so this installs the same fixed per-move time
    // budget used for single searches; the cumulative wall-clock cost is at most
    // (number of ranked moves) × the per-move limit.
    void ApplyStrategySearchLimits();

    CPosition*          m_pPosition{nullptr};
    int                 m_nDepth{3};
    int                 m_nTimeLimit{15};
    PlayerMode          m_PlayerMode{PlayerMode::TwoPlayers};
    std::atomic<bool>   m_fEngineRunning{false};
    std::atomic<bool>   m_fComputingStrategy{false};
    // Set by PauseEngine() to request that the user cancel an in-progress
    // search. Distinct from the engine's global AbortSearch flag, which is also
    // raised on normal time-limit termination of each search; only this flag
    // indicates a genuine user-initiated stop.
    std::atomic<bool>   m_fStopRequested{false};
    // Monotonically increasing token advanced each time a search starts and
    // each time a running search is aborted via AbortAndJoinSearch(). Used to
    // tag and validate engine completion messages so a search whose result is
    // no longer wanted (the user moved while it was running) is ignored.
    std::atomic<uint32_t> m_nSearchGen{0};
    std::thread         m_EngineThread;
    mutable std::mutex  m_PositionMutex;
    CMove               m_BestMove{};
    std::string         m_strStrategy;
    CMove               m_StrategyBestMove{};
    std::vector<CMove>  m_StrategyMoves;
    std::atomic<bool>   m_fStrategyValid{false};
    // Progress of the in-flight strategy computation: number of candidate
    // searches completed (m_nProgressDone) out of the total number planned
    // (m_nProgressTotal). Read by GetStrategyProgressPercent() on the UI thread.
    std::atomic<int>    m_nProgressDone{0};
    std::atomic<int>    m_nProgressTotal{0};
    // Timing of the in-flight single-move (engine / hint) search, used by
    // GetEngineSearchProgressPercent(). m_nEngineSearchStartMs is a
    // steady_clock timestamp (milliseconds) captured when the search starts;
    // m_nEngineSearchBudgetMs is the per-move time budget in milliseconds, or 0
    // for a pure depth-limited search with no time budget.
    std::atomic<long long> m_nEngineSearchStartMs{0};
    std::atomic<long long> m_nEngineSearchBudgetMs{0};
    // Number of consecutive times the SAME invalid move has been rejected by
    // MakeMove (tracked in m_LastRejectedMove) since the last valid move (or
    // NewGame). When it reaches kMaxRejectRetries the engine is declared
    // corrupted (m_fEngineCorrupted) and play is switched to human only. A
    // successfully applied move and NewGame() both reset this. A different
    // rejected move restarts the count at 1.
    int                 m_nRejectCount{0};
    CMove               m_LastRejectedMove{};
    bool                m_fEngineCorrupted{false};
};
