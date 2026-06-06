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
#include <mutex>
#include <string>
#include <thread>

// Posted to the main window when the engine finishes a search.
// WPARAM: unused (0)
// LPARAM: CMove encoded as int32_t (cast to LPARAM)
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

// Posted to the main window when a move-suggestion (hint) search finishes.
// The engine does NOT apply the move; the host highlights it as a
// recommendation. WPARAM/LPARAM unused; the move is read via GetBestMove().
#define WM_APP_ENGINE_HINT  (WM_APP + 2)

// Posted to the main window when a strategy computation finishes. The result
// (a formatted multi-line strategy description) is read via GetStrategyText().
// WPARAM/LPARAM unused.
#define WM_APP_ENGINE_STRATEGY  (WM_APP + 3)

enum class PlayerMode {
    ZeroPlayers = 0, // self-play
    OnePlayer   = 1, // human vs engine
    TwoPlayers  = 2  // human vs human
};

class GameController {
public:
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

    // Set a fixed per-move search time limit, in seconds. A value of 0 disables
    // the fixed limit and reverts to depth-based search termination (the engine
    // searches to GetDepth() plies). When a positive limit is set, the engine
    // searches as deeply as it can within the given number of seconds.
    void SetTimeLimit(int seconds);

    // Return the current fixed per-move time limit in seconds, or 0 if search
    // is depth-based.
    int  GetTimeLimit() const { return m_nTimeLimit; }

    // Set player mode (0 / 1 / 2 human players).
    void SetPlayerMode(PlayerMode mode) { m_PlayerMode = mode; }
    PlayerMode GetPlayerMode() const    { return m_PlayerMode; }

    // Apply a move to the current position. Must NOT be called while engine is thinking.
    void MakeMove(CMove move);

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
    // engine searches a clone of the current position to find the top 3 moves,
    // and for each one the opponent's most likely counter move and the
    // recommended response after that. The completion is posted as
    // WM_APP_ENGINE_STRATEGY to hwndTarget; the formatted result is retrieved
    // via GetStrategyText().
    void StartStrategySearch(HWND hwndTarget);

    // Request the engine to stop searching (sets AbortSearch).
    void PauseEngine();

    // Returns true while the engine thread is running.
    bool IsEngineRunning() const { return m_fEngineRunning.load(); }

    // Returns true while a strategy computation is in progress (see
    // StartStrategySearch). Used to show a "Thinking…" status message.
    bool IsComputingStrategy() const { return m_fComputingStrategy.load(); }

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

private:
    // Shared implementation for StartEngineSearch / StartHintSearch. Clones the
    // current position, searches the clone on a background thread, stores the
    // result in m_BestMove, and posts uCompletionMsg to hwndTarget.
    void StartSearchInternal(HWND hwndTarget, UINT uCompletionMsg);

    // Compute the strategy text by searching a clone of the current position.
    // Returns a formatted, human-readable multi-line description. Runs on the
    // engine thread (see StartStrategySearch); never mutates m_pPosition.
    std::string ComputeStrategyText();

    // Discard any cached strategy result so the next strategy request recomputes
    // it. Called whenever the position changes (move made/undone, new game,
    // position loaded). Callers must hold m_PositionMutex.
    void InvalidateStrategy();

    // Configure the engine's search-termination limits before starting a search.
    // A fixed per-move time limit (m_nTimeLimit > 0) is always used: the engine
    // searches as deeply as the chosen interval allows.
    void ApplySearchLimits();

    // Configure the search-termination limits for a strategy computation. Unlike
    // a single move search, the strategy runs one search per candidate move, so
    // a fixed per-move time limit would multiply out to (number of moves) × the
    // limit. Instead each candidate search is bounded by the configured depth,
    // and the per-move time limit is applied as a single wall-clock budget for
    // the whole strategy computation (enforced in ComputeStrategyText).
    void ApplyStrategySearchLimits();

    CPosition*          m_pPosition{nullptr};
    int                 m_nDepth{3};
    int                 m_nTimeLimit{30};
    PlayerMode          m_PlayerMode{PlayerMode::OnePlayer};
    std::atomic<bool>   m_fEngineRunning{false};
    std::atomic<bool>   m_fComputingStrategy{false};
    // Set by PauseEngine() to request that the user cancel an in-progress
    // search. Distinct from the engine's global AbortSearch flag, which is also
    // raised on normal time-limit termination of each search; only this flag
    // indicates a genuine user-initiated stop.
    std::atomic<bool>   m_fStopRequested{false};
    std::thread         m_EngineThread;
    std::mutex          m_PositionMutex;
    CMove               m_BestMove{};
    std::string         m_strStrategy;
    std::atomic<bool>   m_fStrategyValid{false};
};
