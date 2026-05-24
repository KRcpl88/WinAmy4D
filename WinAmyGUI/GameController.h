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

#include <atomic>
#include <mutex>
#include <thread>

// Posted to the main window when the engine finishes a search.
// WPARAM: unused (0)
// LPARAM: CMove encoded as int32_t (cast to LPARAM)
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

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

    // Set the engine search depth (1–9).
    void SetDepth(int depth);

    // Return current search depth.
    int  GetDepth() const { return m_nDepth; }

    // Set player mode (0 / 1 / 2 human players).
    void SetPlayerMode(PlayerMode mode) { m_PlayerMode = mode; }
    PlayerMode GetPlayerMode() const    { return m_PlayerMode; }

    // Apply a move to the current position. Must NOT be called while engine is thinking.
    void MakeMove(CMove move);

    // Start the engine search asynchronously. The result is posted as
    // WM_APP_ENGINE_MOVE to hwndTarget when the search completes.
    void StartEngineSearch(HWND hwndTarget);

    // Request the engine to stop searching (sets AbortSearch).
    void PauseEngine();

    // Returns true while the engine thread is running.
    bool IsEngineRunning() const { return m_fEngineRunning.load(); }

    // Access the current position (read-only while engine is running).
    const CPosition* GetPosition() const { return m_pPosition; }
    CPosition*       GetPosition()       { return m_pPosition; }

    // Returns true if the game has ended (checkmate / stalemate / draw).
    bool IsGameOver() const;

    // Returns the game-end string or nullptr if game is still in progress.
    const char* GetGameEndMessage() const;

    // Retrieve the best move found by the last engine search.
    CMove GetBestMove() const { return m_BestMove; }

private:
    CPosition*          m_pPosition{nullptr};
    int                 m_nDepth{3};
    PlayerMode          m_PlayerMode{PlayerMode::OnePlayer};
    std::atomic<bool>   m_fEngineRunning{false};
    std::thread         m_EngineThread;
    std::mutex          m_PositionMutex;
    CMove               m_BestMove{};
};
