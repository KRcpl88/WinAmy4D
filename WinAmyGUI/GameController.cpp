/*
    WinAmyGUI — GameController
    Wraps CPosition and runs the engine search on a background thread.
*/

#include "GameController.h"

#include <cassert>
#include <cctype>
#include <cstdio>

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
    // NOTE: Do NOT build the initial position here.  This object is a static
    // global that is constructed during C++ static initialisation, which runs
    // before WinMain calls GameController::InitEngine().  CPosition::Initial()
    // depends on the engine attack tables prepared by InitEngine(), so building
    // the position here would compute attacks (and therefore valid moves) from
    // uninitialised tables.  The initial game is started from WinMain via
    // NewGame() once the engine has been initialised.
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
    m_BestMove = M_NONE;
}

bool GameController::LoadFromEPD(const char *pszEPD) {
    if (!pszEPD || !*pszEPD)
        return false;

    PauseEngine();
    if (m_EngineThread.joinable())
        m_EngineThread.join();

    std::lock_guard<std::mutex> lock(m_PositionMutex);
    CPosition *pNewPosition = CPosition::CreateFromEPD(pszEPD);
    if (!pNewPosition)
        return false;

    if (m_pPosition)
        CPosition::Free(m_pPosition);
    m_pPosition = pNewPosition;
    m_BestMove = M_NONE;
    return true;
}

bool GameController::LoadFromEPDFile(const wchar_t *pszPath) {
    if (!pszPath || !*pszPath)
        return false;

    FILE *pFile = nullptr;
    if (_wfopen_s(&pFile, pszPath, L"rb") != 0 || !pFile)
        return false;

    if (fseek(pFile, 0, SEEK_END) != 0) {
        fclose(pFile);
        return false;
    }
    long nSize = ftell(pFile);
    if (nSize < 0) {
        fclose(pFile);
        return false;
    }
    rewind(pFile);

    std::string strEPD;
    strEPD.resize(static_cast<size_t>(nSize));
    if (nSize > 0) {
        size_t nRead = fread(strEPD.data(), 1, strEPD.size(), pFile);
        if (nRead != strEPD.size()) {
            fclose(pFile);
            return false;
        }
    }
    fclose(pFile);

    size_t nNewline = strEPD.find_first_of("\r\n");
    if (nNewline != std::string::npos)
        strEPD.erase(nNewline);

    size_t nStart = 0;
    while (nStart < strEPD.size() && std::isspace(static_cast<unsigned char>(strEPD[nStart])))
        ++nStart;
    if (nStart > 0)
        strEPD.erase(0, nStart);

    while (!strEPD.empty() && std::isspace(static_cast<unsigned char>(strEPD.back())))
        strEPD.pop_back();

    return LoadFromEPD(strEPD.c_str());
}

bool GameController::SaveToEPDFile(const wchar_t *pszPath) {
    if (!pszPath || !*pszPath)
        return false;

    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (!m_pPosition)
        return false;

    const char *pszEPD = m_pPosition->MakeEPD();
    if (!pszEPD)
        return false;

    FILE *pFile = nullptr;
    if (_wfopen_s(&pFile, pszPath, L"wb") != 0 || !pFile)
        return false;

    size_t nLength = strlen(pszEPD);
    size_t nWritten = fwrite(pszEPD, 1, nLength, pFile);
    fwrite("\n", 1, 1, pFile);
    fclose(pFile);

    return nWritten == nLength;
}

void GameController::SetDepth(int depth) {
    if (depth < 1) depth = 1;
    if (depth > 9) depth = 9;
    m_nDepth = depth;
    setMaxSearchDepth(depth);
}

void GameController::MakeMove(CMove move) {
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (m_pPosition) {
        // Defensive guard: a stale or duplicate WM_APP_ENGINE_MOVE can deliver a
        // move that is no longer valid for the current position (for example after
        // a New Game, or when the same engine best-move is delivered twice). The
        // second time, the move's from-square is already empty, so DoMove would
        // set an occupancy bit on a square whose piece is Neutral, corrupting the
        // board and making AtkSet/RecalcAttacks panic. Only apply legal moves.
        if (!m_pPosition->LegalMove(move))
            return;
        m_pPosition->DoMove(move);
#ifndef NDEBUG
        // Snapshot the incrementally maintained attack tables before the full
        // recompute below.  At this point m_pPosition already holds the correct
        // attacks (DoMove updated them via the gain/lose-attack path), so a
        // subsequent RecalcAttacks() must reproduce them exactly.  Verify both
        // the count and the actual attack squares are unchanged; any mismatch
        // signals a bug in the incremental gain/lose-attack updates.
        CBitBoard rgAtkToBefore[CBitBoard::SIZE];
        CBitBoard rgAtkFrBefore[CBitBoard::SIZE];
        for (unsigned int nSquare = 0; nSquare < CBitBoard::SIZE; ++nSquare) {
            rgAtkToBefore[nSquare] = m_pPosition->m_rgAtkTo[nSquare];
            rgAtkFrBefore[nSquare] = m_pPosition->m_rgAtkFr[nSquare];
        }
        const CBitBoard SlidingBefore = m_pPosition->m_SlidingPieces;
#endif
        // Safety net: the engine maintains attack tables incrementally via
        // gain/lose-attack updates inside DoMove.  The GUI applies only one
        // move per call, so a full recompute here is cheap and guarantees the
        // valid-move highlighting is always derived from a fully consistent
        // attack state, independent of the incremental path.
        m_pPosition->RecalcAttacks();
#ifndef NDEBUG
        for (unsigned int nSquare = 0; nSquare < CBitBoard::SIZE; ++nSquare) {
            assert(rgAtkToBefore[nSquare] == m_pPosition->m_rgAtkTo[nSquare] &&
                   "RecalcAttacks changed AtkTo: incremental attack update bug");
            assert(rgAtkFrBefore[nSquare] == m_pPosition->m_rgAtkFr[nSquare] &&
                   "RecalcAttacks changed AtkFr: incremental attack update bug");
        }
        assert(SlidingBefore == m_pPosition->m_SlidingPieces &&
               "RecalcAttacks changed SlidingPieces: incremental attack update bug");
#endif
    }
}

bool GameController::UndoLastHumanMove() {
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (!m_pPosition)
        return false;

    // Undo is only meaningful in 1-player mode (human vs engine). In 0-player
    // self-play and 2-player modes there is no single "human move" to take back.
    if (m_PlayerMode != PlayerMode::OnePlayer)
        return false;

    // The human plays White (turn 0); the engine plays Black (turn 1). To return
    // control to the human with their previous move reverted, undo back to a
    // White-to-move position:
    //   * If it is currently Black's turn, only the human's move needs undoing.
    //   * If it is currently White's turn, undo the engine's reply and the
    //     human's preceding move (two plies).
    int nPlies = (m_pPosition->m_nTurn == 1) ? 1 : 2;
    if (static_cast<unsigned int>(nPlies) > m_pPosition->m_wPly)
        nPlies = static_cast<int>(m_pPosition->m_wPly);
    if (nPlies == 0)
        return false;

    for (int nPly = 0; nPly < nPlies; ++nPly)
        m_pPosition->Undo();

    // Mirror MakeMove: keep the highlighting/attack state fully consistent.
    m_pPosition->RecalcAttacks();
    return true;
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

        // CPosition::Iterate runs the search on the object it is called on,
        // mutating it via DoMove/UndoMove throughout iterative deepening.  The
        // engine's own callers (SearchRoot, PermanentBrain) therefore clone the
        // position, search the clone, and free it — never searching the live
        // board.  The GUI must do the same: searching m_pPosition directly would
        // leave its incremental state (attack tables, hash keys, game log) in an
        // inconsistent state if anything is not perfectly restored, which can
        // produce an invalid best move.  Clone under the lock to take a
        // consistent snapshot, then search the clone without holding the lock.
        CPosition *pSearchPosition = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_PositionMutex);
            if (m_pPosition)
                pSearchPosition = CPosition::Clone(m_pPosition);
        }

        if (pSearchPosition) {
            int score = 0, altScore = 0;
            bestMove = pSearchPosition->Iterate(&score, M_NONE, &altScore);
            CPosition::Free(pSearchPosition);
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

std::string GameController::GetGameResultText() const {
    if (!m_pPosition)
        return std::string();

    const char *pszEnd = m_pPosition->GameEnd();
    if (!pszEnd)
        return std::string();

    // m_wPly counts half-moves (plies) played.  The number of full moves the
    // game lasted is therefore ceil(plies / 2).
    const int nMoves = (static_cast<int>(m_pPosition->m_wPly) + 1) / 2;

    std::string strResult(pszEnd);
    std::string strText;

    if (strResult.rfind("1-0", 0) == 0) {
        strText = "Checkmate \xe2\x80\x94 White wins";
    } else if (strResult.rfind("0-1", 0) == 0) {
        strText = "Checkmate \xe2\x80\x94 Black wins";
    } else {
        // Draw.  Use the reason supplied by GameEnd() (the text in braces).
        std::string strReason;
        const size_t nOpen = strResult.find('{');
        const size_t nClose = strResult.find('}');
        if (nOpen != std::string::npos && nClose != std::string::npos &&
            nClose > nOpen + 1) {
            strReason = strResult.substr(nOpen + 1, nClose - nOpen - 1);
        }
        if (strReason.empty())
            strReason = "Draw";
        strText = "Draw \xe2\x80\x94 " + strReason;
    }

    strText += " in ";
    strText += std::to_string(nMoves);
    strText += (nMoves == 1) ? " move" : " moves";
    return strText;
}
