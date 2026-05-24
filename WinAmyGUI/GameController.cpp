/*
    WinAmyGUI — GameController
    Wraps CPosition and runs the engine search on a background thread.
*/

#include "GameController.h"

// ---------------------------------------------------------------------------
// Static engine initialisation
// ---------------------------------------------------------------------------

void GameController::InitEngine() {
    InitMoves();
    InitAll();
    HashInit();
    AllocateHT();
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

GameController::GameController() {
    NewGame();
}

GameController::~GameController() {
    // Stop any running engine thread before destruction.
    PauseEngine();
    if (m_EngineThread.joinable())
        m_EngineThread.join();

    if (m_pPosition)
        CPosition::Free(m_pPosition);
}

// ---------------------------------------------------------------------------
// Game management
// ---------------------------------------------------------------------------

void GameController::NewGame() {
    PauseEngine();
    if (m_EngineThread.joinable())
        m_EngineThread.join();

    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (m_pPosition)
        CPosition::Free(m_pPosition);
    m_pPosition = CPosition::Initial();
}

void GameController::SetDepth(int depth) {
    if (depth < 1) depth = 1;
    if (depth > 9) depth = 9;
    m_nDepth = depth;
    setMaxSearchDepth(depth);
}

void GameController::MakeMove(CMove move) {
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (m_pPosition)
        m_pPosition->DoMove(move);
}

// ---------------------------------------------------------------------------
// Engine thread
// ---------------------------------------------------------------------------

void GameController::StartEngineSearch(HWND hwndTarget) {
    if (m_fEngineRunning.load())
        return;

    setMaxSearchDepth(m_nDepth);
    m_fEngineRunning.store(true);
    AbortSearch = false;

    if (m_EngineThread.joinable())
        m_EngineThread.join();

    m_EngineThread = std::thread([this, hwndTarget]() {
        CMove bestMove = M_NONE;
        {
            std::lock_guard<std::mutex> lock(m_PositionMutex);
            if (m_pPosition) {
                int score = 0, altScore = 0;
                bestMove = m_pPosition->Iterate(&score, M_NONE, &altScore);
            }
        }
        m_BestMove = bestMove;
        m_fEngineRunning.store(false);

        // Notify the window — no move data in the message, caller uses GetBestMove().
        PostMessage(hwndTarget, WM_APP_ENGINE_MOVE, 0, 0);
    });
}

void GameController::PauseEngine() {
    AbortSearch = true;
}

// ---------------------------------------------------------------------------
// Game state queries
// ---------------------------------------------------------------------------

bool GameController::IsGameOver() const {
    return GetGameEndMessage() != nullptr;
}

const char* GameController::GetGameEndMessage() const {
    if (!m_pPosition)
        return nullptr;
    return m_pPosition->GameEnd();
}
