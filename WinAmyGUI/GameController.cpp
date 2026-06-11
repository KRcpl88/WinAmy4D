/*
    WinAmyGUI — GameController
    Wraps CPosition and runs the engine search on a background thread.
*/

#include "GameController.h"

#include "utils.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

namespace {
// Number of ranked moves reported by a strategy computation.
constexpr int kStrategyRanks = 3;
} // namespace

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
    m_nRejectCount = 0;
    m_fEngineCorrupted = false;
    InvalidateStrategy();
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
    InvalidateStrategy();
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

// ---------------------------------------------------------------------------
// PGN (3D/4D) game load / save
//
// The move text uses the engine's level-aware SAN (CPosition::SAN /
// CPosition::ParseSAN), so a move's destination square is written as
// <level><file><rank> (e.g. "ea2a3"). A game therefore round-trips through a
// standard-looking PGN file that fully describes the 3D/4D moves. Files use the
// ".pgn4" extension. Games always start from the 4D initial position, so no
// FEN/SetUp tag is written or required.
// ---------------------------------------------------------------------------

namespace {

// A token is a game-result marker rather than a move.
bool IsPGNResultToken(const std::string &strToken) {
    return strToken == "1-0" || strToken == "0-1" ||
           strToken == "1/2-1/2" || strToken == "*";
}

// Split PGN movetext into bare move tokens, skipping tag pairs, comments
// ({...} and ;-to-end-of-line), recursive variations ((...)), NAGs ($n),
// move numbers and result markers. The returned tokens are SAN move strings.
std::vector<std::string> ExtractPGNMoveTokens(const std::string &strContent) {
    // Drop tag-pair lines (those whose first non-blank character is '[') and
    // keep the remaining movetext, preserving newlines for ';' comments.
    std::string strMoveText;
    size_t nPos = 0;
    while (nPos < strContent.size()) {
        size_t nEol = strContent.find('\n', nPos);
        size_t nLen = (nEol == std::string::npos) ? std::string::npos : nEol - nPos;
        std::string strLine = strContent.substr(nPos, nLen);
        nPos = (nEol == std::string::npos) ? strContent.size() : nEol + 1;

        size_t nFirst = strLine.find_first_not_of(" \t\r");
        if (nFirst != std::string::npos && strLine[nFirst] == '[')
            continue; // tag-pair line
        strMoveText += strLine;
        strMoveText += '\n';
    }

    std::vector<std::string> rgTokens;
    size_t i = 0;
    while (i < strMoveText.size()) {
        char c = strMoveText[i];
        if (c == '{') { // brace comment to matching '}'
            size_t nEnd = strMoveText.find('}', i);
            i = (nEnd == std::string::npos) ? strMoveText.size() : nEnd + 1;
        } else if (c == ';') { // line comment to end of line
            size_t nEnd = strMoveText.find('\n', i);
            i = (nEnd == std::string::npos) ? strMoveText.size() : nEnd + 1;
        } else if (c == '(') { // recursive variation — skip balanced parens
            int nDepth = 1;
            ++i;
            while (i < strMoveText.size() && nDepth > 0) {
                if (strMoveText[i] == '(') ++nDepth;
                else if (strMoveText[i] == ')') --nDepth;
                ++i;
            }
        } else if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
        } else {
            size_t k = i;
            while (k < strMoveText.size()) {
                char d = strMoveText[k];
                if (std::isspace(static_cast<unsigned char>(d)) ||
                    d == '{' || d == '(' || d == ';')
                    break;
                ++k;
            }
            rgTokens.push_back(strMoveText.substr(i, k - i));
            i = k;
        }
    }
    return rgTokens;
}

} // namespace

bool GameController::LoadFromPGNFile(const wchar_t *pszPath) {
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

    std::string strContent;
    strContent.resize(static_cast<size_t>(nSize));
    if (nSize > 0) {
        size_t nRead = fread(strContent.data(), 1, strContent.size(), pFile);
        if (nRead != strContent.size()) {
            fclose(pFile);
            return false;
        }
    }
    fclose(pFile);

    std::vector<std::string> rgTokens = ExtractPGNMoveTokens(strContent);

    // Build the game on a fresh position so a parse failure leaves the current
    // game untouched.
    CPosition *pNewPosition = CPosition::Initial();
    if (!pNewPosition)
        return false;

    for (const std::string &strRaw : rgTokens) {
        if (IsPGNResultToken(strRaw))
            break;

        // Strip a leading move number / dots (e.g. "12." or "12...").
        size_t nStart = 0;
        while (nStart < strRaw.size() &&
               (std::isdigit(static_cast<unsigned char>(strRaw[nStart])) ||
                strRaw[nStart] == '.'))
            ++nStart;
        std::string strMove = strRaw.substr(nStart);

        // Strip trailing annotation glyphs ('!' / '?'); ParseSAN already
        // tolerates trailing '+' / '#'.
        while (!strMove.empty() &&
               (strMove.back() == '!' || strMove.back() == '?'))
            strMove.pop_back();

        if (strMove.empty())
            continue; // pure move-number token

        CMove themove = pNewPosition->ParseSAN(strMove.c_str());
        if (themove == M_NONE) {
            // Could not interpret a move — treat the file as invalid.
            CPosition::Free(pNewPosition);
            return false;
        }
        pNewPosition->DoMove(themove);
    }

    PauseEngine();
    if (m_EngineThread.joinable())
        m_EngineThread.join();

    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (m_pPosition)
        CPosition::Free(m_pPosition);
    m_pPosition = pNewPosition;
    m_BestMove = M_NONE;
    InvalidateStrategy();
    return true;
}

bool GameController::SaveToPGNFile(const wchar_t *pszPath) {
    if (!pszPath || !*pszPath)
        return false;

    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (!m_pPosition)
        return false;

    // Operate on a clone so the live game position is never mutated.
    CPosition *p = CPosition::Clone(m_pPosition);
    if (!p)
        return false;

    int nPly = p->GetPly();

    // Determine the result before unwinding the move list.
    const char *pszGameEnd = p->GameEnd();
    std::string strResult = pszGameEnd ? std::string(pszGameEnd) : std::string("*");
    // The header [Result] tag uses the short form (no trailing reason text).
    std::string strShortResult = strResult;
    {
        size_t nSpace = strShortResult.find(' ');
        if (nSpace != std::string::npos)
            strShortResult.erase(nSpace);
    }
    if (strShortResult.empty())
        strShortResult = "*";

    // Unwind to the initial position, recording the moves in play order.
    std::vector<CMove> rgMoves(static_cast<size_t>(nPly));
    for (int i = nPly; i > 0; --i) {
        CMove move = (p->GetActLog() - 1)->gl_Move;
        rgMoves[static_cast<size_t>(i - 1)] = move;
        p->UndoMove(move);
    }

    FILE *pFile = nullptr;
    if (_wfopen_s(&pFile, pszPath, L"w") != 0 || !pFile) {
        CPosition::Free(p);
        return false;
    }

    fprintf(pFile, "[Event \"WinAmy 4D game\"]\n");
    fprintf(pFile, "[Site \"?\"]\n");
    fprintf(pFile, "[Date \"????.??.??\"]\n");
    fprintf(pFile, "[Round \"?\"]\n");
    fprintf(pFile, "[White \"White\"]\n");
    fprintf(pFile, "[Black \"Black\"]\n");
    fprintf(pFile, "[Result \"%s\"]\n", strShortResult.c_str());
    fprintf(pFile, "[Variant \"4D\"]\n\n");

    int nWidth = 0;
    for (int i = 0; i < nPly; ++i) {
        CMove move = rgMoves[static_cast<size_t>(i)];
        if ((i & 1) == 0) {
            int nWritten = fprintf(pFile, "%d. ", (i / 2) + 1);
            if (nWritten > 0)
                nWidth += nWritten;
        }

        char san_buffer[32];
        char *san = p->SAN(move, san_buffer);
        fprintf(pFile, "%s ", san);
        nWidth += static_cast<int>(strlen(san)) + 1;
        if (nWidth > 67) {
            nWidth = 0;
            fprintf(pFile, "\n");
        }
        p->DoMove(move);
    }
    fprintf(pFile, "\n%s\n\n", strResult.c_str());
    fclose(pFile);

    CPosition::Free(p);
    return true;
}

void GameController::SetDepth(int depth) {
    if (depth < 1) depth = 1;
    if (depth > 9) depth = 9;
    m_nDepth = depth;
    setMaxSearchDepth(depth);
    // Search time is derived from the search depth: 10 seconds per ply, so
    // depth 9 yields a 90 second search.
    m_nTimeLimit = depth * 10;
    // Any cached strategy was computed at the previous depth and is now stale;
    // drop it so the next "recommend strategy" re-searches at the new depth.
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    InvalidateStrategy();
}

void GameController::SetTimeLimit(int seconds) {
    if (seconds < 0) seconds = 0;
    m_nTimeLimit = seconds;
    // Any cached strategy was computed under the previous time limit and is now
    // stale; drop it so the next "recommend strategy" re-searches at the new
    // limit.
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    InvalidateStrategy();
}

void GameController::ApplySearchLimits() {
    if (m_nTimeLimit > 0) {
        // Fixed time-per-move: let the engine search as deeply as it can within
        // the time budget. MaxSearchDepth is raised to (just under) the engine's
        // hard ceiling so the wall-clock limit, not the depth, governs.
        SetFixedTimePerMove(m_nTimeLimit);
        setMaxSearchDepth(MAX_TREE_SIZE - 2);
    } else {
        // Depth-based: restore the default time control (so the clock does not
        // cut the search short) and cap the search at the configured depth.
        SetDefaultTimeControl();
        setMaxSearchDepth(m_nDepth);
    }
}

void GameController::ApplyStrategySearchLimits() {
    // The strategy runs up to three full-time searches (one per ranked move),
    // each excluding the previously found best move(s). Use the same fixed
    // per-move time budget as a single search; the cumulative cost is bounded by
    // (number of ranked moves) × the per-move limit.
    if (m_nTimeLimit > 0) {
        SetFixedTimePerMove(m_nTimeLimit);
        setMaxSearchDepth(MAX_TREE_SIZE - 2);
    } else {
        SetDefaultTimeControl();
        setMaxSearchDepth(m_nDepth);
    }
}

bool GameController::MakeMove(CMove move) {
    std::lock_guard<std::mutex> lock(m_PositionMutex);
    if (m_pPosition) {
        // Defensive guard: a stale or duplicate WM_APP_ENGINE_MOVE can deliver a
        // move that is no longer valid for the current position (for example after
        // a New Game, or when the same engine best-move is delivered twice). The
        // second time, the move's from-square is already empty, so DoMove would
        // set an occupancy bit on a square whose piece is Neutral, corrupting the
        // board and making AtkSet/RecalcAttacks panic. Only apply legal moves.
        if (!m_pPosition->LegalMove(move)) {
            // Log the rejection so the discarded move is visible in the log.
            // SAN() is unsafe here because it internally calls DoMove/UndoMove,
            // which would corrupt the board for an illegal/stale move; instead
            // format the raw source and destination coordinates directly.
            //
            // Only the SAME move rejected repeatedly counts toward corruption:
            // if a different invalid move arrives, restart the count at 1.
            if (m_nRejectCount > 0 && move == m_LastRejectedMove) {
                ++m_nRejectCount;
            } else {
                m_nRejectCount = 1;
                m_LastRejectedMove = move;
            }
            const CSCoord &FromCoord = move.GetFromCoord();
            const CSCoord &ToCoord = move.GetToCoord();
            const char *pszSide = (m_pPosition->GetTurn() == 0) ? "White" : "Black";
            Print(0,
                  "Move rejected (not valid) for %s: %c%c%c-%c%c%c; "
                  "turn is unchanged (attempt %d of %d)\n",
                  pszSide, 'a' + FromCoord.m_nLevel, 'a' + FromCoord.m_nFile,
                  '1' + FromCoord.m_nRank, 'a' + ToCoord.m_nLevel,
                  'a' + ToCoord.m_nFile, '1' + ToCoord.m_nRank, m_nRejectCount,
                  kMaxRejectRetries);

            if (m_nRejectCount >= kMaxRejectRetries) {
                // The same invalid move has been retried too many times: treat
                // the engine as corrupted. Switch to human-only play so the host
                // stops asking the engine to search; the user can still save the
                // game.
                m_fEngineCorrupted = true;
                m_PlayerMode = PlayerMode::TwoPlayers;
                Print(0,
                      "Engine corrupted: same invalid move retried %d times; "
                      "stopping play and switching to human only\n",
                      m_nRejectCount);
            }
            return false;
        }

        // A valid move ends any run of rejections.
        m_nRejectCount = 0;
        m_fEngineCorrupted = false;


        // Log every move that is actually applied to the game. SAN must be
        // computed from the position *before* the move is made, and GetTurn()
        // (also pre-move) identifies the side that is moving. Player moves are
        // additionally logged at the GUI call site, so a move that appears here
        // with no preceding "Player made move" entry was made by the computer.
        {
            char szSan[32];
            const char *pszSan = m_pPosition->SAN(move, szSan);
            const char *pszSide = (m_pPosition->GetTurn() == 0) ? "White" : "Black";
            Print(0, "Move made: %s by %s\n", pszSan, pszSide);
        }

        m_pPosition->DoMove(move);
        // The position has changed, so any cached strategy result no longer
        // applies; force a recompute on the next strategy request.
        InvalidateStrategy();
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
            rgAtkToBefore[nSquare] = m_pPosition->GetAtkTo(nSquare);
            rgAtkFrBefore[nSquare] = m_pPosition->GetAtkFr(nSquare);
        }
        const CBitBoard SlidingBefore = m_pPosition->GetSlidingPieces();
#endif
        // Safety net: the engine maintains attack tables incrementally via
        // gain/lose-attack updates inside DoMove.  The GUI applies only one
        // move per call, so a full recompute here is cheap and guarantees the
        // valid-move highlighting is always derived from a fully consistent
        // attack state, independent of the incremental path.
        m_pPosition->RecalcAttacks();
#ifndef NDEBUG
        for (unsigned int nSquare = 0; nSquare < CBitBoard::SIZE; ++nSquare) {
            assert(rgAtkToBefore[nSquare] == m_pPosition->GetAtkTo(nSquare) &&
                   "RecalcAttacks changed AtkTo: incremental attack update bug");
            assert(rgAtkFrBefore[nSquare] == m_pPosition->GetAtkFr(nSquare) &&
                   "RecalcAttacks changed AtkFr: incremental attack update bug");
        }
        assert(SlidingBefore == m_pPosition->GetSlidingPieces() &&
               "RecalcAttacks changed SlidingPieces: incremental attack update bug");
#endif
        return true;
    }
    return false;
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
    uint16_t wPlies = (m_pPosition->GetTurn() == 1) ? 1 : 2;
    if (wPlies > m_pPosition->GetPly())
        wPlies = m_pPosition->GetPly();
    if (wPlies == 0)
        return false;

    for (uint16_t wPly = 0; wPly < wPlies; ++wPly)
        m_pPosition->Undo();

    // Mirror MakeMove: keep the highlighting/attack state fully consistent.
    m_pPosition->RecalcAttacks();
    InvalidateStrategy();
    return true;
}

// ---------------------------------------------------------------------------
// Engine thread
// ---------------------------------------------------------------------------

void GameController::StartEngineSearch(HWND hwndTarget) {
    StartSearchInternal(hwndTarget, WM_APP_ENGINE_MOVE);
}

void GameController::StartHintSearch(HWND hwndTarget) {
    StartSearchInternal(hwndTarget, WM_APP_ENGINE_HINT);
}

void GameController::StartSearchInternal(HWND hwndTarget, UINT uCompletionMsg) {
    if (m_fEngineRunning.load())
        return;

    ApplySearchLimits();
    m_fEngineRunning.store(true);
    m_fStopRequested.store(false);
    AbortSearch = false;

    // Tag this search with a fresh generation token so its completion message
    // can be matched (and a stale one — e.g. from a search the user aborted by
    // moving — discarded) by the host. See GetSearchGeneration().
    const uint32_t nGen = ++m_nSearchGen;

    // Capture the search start time and per-move time budget so the UI can
    // report progress for this single-move search (engine move or hint). A
    // timed search runs until the budget elapses, so elapsed/budget is a smooth,
    // monotonic progress measure. A pure depth-limited search has no budget
    // (0), and GetEngineSearchProgressPercent() then reports "indeterminate".
    {
        using namespace std::chrono;
        const long long nNowMs =
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
                .count();
        m_nEngineSearchStartMs.store(nNowMs);
        m_nEngineSearchBudgetMs.store(m_nTimeLimit > 0
                                          ? static_cast<long long>(m_nTimeLimit) * 1000
                                          : 0);
    }

    if (m_EngineThread.joinable())
        m_EngineThread.join();

    // Scenario 2: log the engine search BEFORE it is initiated. Both the
    // engine's own move search and the suggest-move (hint) search run through
    // here; label them distinctly. The matching AFTER log (with the chosen move
    // and its score) is emitted in the search thread once Iterate() returns.
    const bool fHint = (uCompletionMsg == WM_APP_ENGINE_HINT);
    const char *pszLabel = fHint ? "Suggest move" : "Engine move";
    Print(0, "%s: starting search (time limit %ds)...\n", pszLabel, m_nTimeLimit);

    m_EngineThread = std::thread([this, hwndTarget, uCompletionMsg, pszLabel, nGen]() {
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

            // Scenario 2: log the search AFTER it completes, including the chosen
            // move and its score. SAN is computed on the (restored) root position
            // the search ran on, before it is freed.
            char szSan[32];
            const char *pszSan = "(none)";
            if (bestMove != M_NONE)
                pszSan = pSearchPosition->SAN(bestMove, szSan);
            char szScore[16];
            FormatScore(score, szScore, sizeof(szScore));
            Print(0, "%s: search complete, best move %s (score %s)\n",
                  pszLabel, pszSan, szScore);

            CPosition::Free(pSearchPosition);
        } else {
            Print(0, "%s: search complete, best move (none)\n", pszLabel);
        }

        m_BestMove = bestMove;
        m_fEngineRunning.store(false);

        // Notify the window — no move data in the message, caller uses GetBestMove().
        // WPARAM carries this search's generation so the host can discard the
        // message if the search was superseded (e.g. aborted by a user move).
        PostMessage(hwndTarget, uCompletionMsg, static_cast<WPARAM>(nGen), 0);
    });
}

void GameController::PauseEngine() {
    m_fStopRequested.store(true);
    AbortSearch = true;
}

void GameController::AbortAndJoinSearch() {
    // Request the running search to stop. The engine honours AbortSearch (and
    // the strategy loop additionally honours m_fStopRequested between its
    // per-move searches), so the engine thread returns promptly.
    m_fStopRequested.store(true);
    AbortSearch = true;

    // Block until the engine thread has fully exited. This guarantees the
    // shared engine state (hash table, search globals) is no longer in use, so
    // the caller may safely mutate the position and start a new search.
    if (m_EngineThread.joinable()) {
        m_EngineThread.join();
    }

    // The finishing thread has already posted its completion message. Advance
    // the generation so that message is now stale and the host ignores it.
    ++m_nSearchGen;
    m_fEngineRunning.store(false);
    m_fComputingStrategy.store(false);
}

int GameController::GetEngineSearchProgressPercent() const {
    const long long nBudgetMs = m_nEngineSearchBudgetMs.load();
    if (nBudgetMs <= 0) {
        // Pure depth-limited search: no time budget to measure against.
        return -1;
    }

    using namespace std::chrono;
    const long long nNowMs =
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count();
    long long nElapsedMs = nNowMs - m_nEngineSearchStartMs.load();
    if (nElapsedMs < 0) {
        nElapsedMs = 0;
    }
    int nPercent = static_cast<int>((nElapsedMs * 100) / nBudgetMs);
    if (nPercent > 100) {
        nPercent = 100;
    }
    return nPercent;
}

int GameController::GetSearchCountdownSeconds() const {
    // No countdown for very short limits (5s or less) — the number would barely
    // be readable and the search ends almost immediately.
    if (m_nTimeLimit <= 5) {
        return -1;
    }
    const long long nBudgetMs = m_nEngineSearchBudgetMs.load();
    if (nBudgetMs <= 0) {
        // Pure depth-limited search: no deadline to count down to.
        return -1;
    }

    using namespace std::chrono;
    const long long nNowMs =
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
            .count();
    // The countdown tracks the search clock directly (no padding): the engine's
    // timer is reliable enough that it finishes very close to the limit. If the
    // search finishes first the host clears the countdown; if the countdown
    // reaches zero first it simply stays at 0 until the search returns (a couple
    // of seconds at most).
    const long long nDeadlineMs = m_nEngineSearchStartMs.load() + nBudgetMs;
    long long nRemainingMs = nDeadlineMs - nNowMs;
    if (nRemainingMs < 0) {
        nRemainingMs = 0;
    }
    // Round up so the final partial second still shows "1" rather than "0".
    return static_cast<int>((nRemainingMs + 999) / 1000);
}

// ---------------------------------------------------------------------------
// Strategy computation
// ---------------------------------------------------------------------------

void GameController::StartStrategySearch(HWND hwndTarget) {
    if (m_fEngineRunning.load()) {
        return;
    }

    ApplyStrategySearchLimits();
    m_fEngineRunning.store(true);
    m_fComputingStrategy.store(true);
    m_fStopRequested.store(false);
    AbortSearch = false;

    // Tag this strategy computation with a fresh generation token so its
    // completion message can be discarded if it is superseded (e.g. the user
    // makes a move while it runs). See GetSearchGeneration().
    const uint32_t nGen = ++m_nSearchGen;

    // Reset the progress counters so the status bar starts at 0%. The total is
    // established once ComputeStrategyText() has enumerated the candidate pool.
    m_nProgressDone.store(0);
    m_nProgressTotal.store(0);

    // Capture the search start time and a cumulative time budget so the UI can
    // run a single countdown across the whole strategy computation. The strategy
    // performs up to kStrategyRanks full-time searches, so the upper bound on the
    // wall-clock cost is kStrategyRanks × the per-move limit. If the searches
    // finish early the countdown is simply cleared.
    {
        using namespace std::chrono;
        const long long nNowMs =
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch())
                .count();
        m_nEngineSearchStartMs.store(nNowMs);
        m_nEngineSearchBudgetMs.store(
            m_nTimeLimit > 0
                ? static_cast<long long>(m_nTimeLimit) * 1000 * kStrategyRanks
                : 0);
    }

    if (m_EngineThread.joinable()) {
        m_EngineThread.join();
    }

    // Scenario 3: log the strategy search BEFORE it is initiated. The matching
    // AFTER log (with the strategic results) is emitted in the search thread
    // once ComputeStrategyText() returns, so the log shows what the engine did
    // to determine the optimal strategy.
    Print(0, "Strategy: starting search (time limit %ds per move)...\n",
          m_nTimeLimit);

    m_EngineThread = std::thread([this, hwndTarget, nGen]() {
        std::string strStrategy = ComputeStrategyText(hwndTarget);

        // Scenario 3: log the strategy search AFTER it completes, including the
        // strategic results produced by the search.
        Print(0, "Strategy: search complete, strategic results:\n%s\n",
              strStrategy.c_str());

        {
            std::lock_guard<std::mutex> lock(m_PositionMutex);
            m_strStrategy = strStrategy;
            m_fStrategyValid.store(true);
        }
        m_fComputingStrategy.store(false);
        m_fEngineRunning.store(false);
        // WPARAM carries this search's generation so the host can discard the
        // message if the strategy computation was superseded (e.g. by a user
        // move). See GetSearchGeneration().
        PostMessage(hwndTarget, WM_APP_ENGINE_STRATEGY, static_cast<WPARAM>(nGen), 0);
    });
}

void GameController::InvalidateStrategy() {
    m_strStrategy.clear();
    m_StrategyBestMove = M_NONE;
    m_StrategyMoves.clear();
    m_fStrategyValid.store(false);
}

std::string GameController::ComputeStrategyText(HWND hwndTarget) {
    // Search a clone so the live game position is never mutated (CPosition's
    // search entry points mutate the object they run on).
    CPosition *p = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_PositionMutex);
        if (m_pPosition) {
            p = CPosition::Clone(m_pPosition);
        }
    }
    if (!p) {
        return std::string();
    }

    // The engine is single-PV: each search yields one best move with a reliable
    // score, while the runner-up root moves are searched with narrow windows so
    // their exact scores are not preserved. To report the top kStrategyRanks
    // moves we therefore emulate MultiPV by running that many full-time searches,
    // each excluding the best move(s) already found at the root. Every search
    // uses the full per-move time budget, so the ranking reflects deep evaluation
    // rather than whatever the move generator happens to emit first.
    if (m_nTimeLimit > 0) {
        SetFixedTimePerMove(m_nTimeLimit);
        setMaxSearchDepth(MAX_TREE_SIZE - 2);
    } else {
        SetDefaultTimeControl();
        setMaxSearchDepth(m_nDepth);
    }

    // Enumerate the current player's legal moves only to size the ranking and
    // detect the no-moves case; the searches themselves operate on the position.
    heap_t heap = allocate_heap();
    int cnt = p->LegalMoves(heap);
    free_heap(heap);

    if (cnt <= 0) {
        CPosition::Free(p);
        return std::string("No legal moves are available for the current player.");
    }

    // A ranked move together with the engine's evaluation of it (from the current
    // player's perspective), the opponent's best reply, our recommended response
    // to that reply, and whether the move ends the game.
    struct SCandidate {
        CMove       Move;
        int         nValue;
        std::string strSan;
        CMove       Reply;
        CMove       Response;
        std::string strReplySan;
        std::string strResponseSan;
        bool        bGameOver;
    };

    // One full-time search per reported rank (bounded by the number of legal
    // moves). The status-bar countdown spans all of them (see
    // StartStrategySearch). Progress is reported as completed searches / nRanks.
    const int nRanks = (cnt < kStrategyRanks) ? cnt : kStrategyRanks;
    m_nProgressDone.store(0);
    m_nProgressTotal.store(nRanks);

    // Advance the progress counter by one completed search and notify the UI so
    // it can refresh the status bar. Posting (not sending) keeps the engine
    // thread decoupled from the window thread.
    auto BumpProgress = [this, hwndTarget]() {
        m_nProgressDone.fetch_add(1);
        if (hwndTarget) {
            PostMessage(hwndTarget, WM_APP_ENGINE_PROGRESS, 0, 0);
        }
    };

    std::vector<SCandidate> rgCandidates;
    rgCandidates.reserve(static_cast<size_t>(nRanks));

    // Root moves found so far, excluded from subsequent searches so each search
    // surfaces the next-best move.
    std::vector<CMove> rgExcluded;
    rgExcluded.reserve(static_cast<size_t>(nRanks));

    for (int r = 0; r < nRanks; ++r) {
        // Honour a user cancellation before starting another full-time search.
        if (m_fStopRequested.load()) {
            break;
        }

        // Install the exclusion list for this rank. The exclusion API is a
        // process-global, non-reentrant facility; this is safe because the GUI
        // serialises engine searches (m_fEngineRunning), so only this strategy
        // computation manipulates it at a time.
        if (!rgExcluded.empty()) {
            SetExcludedRootMoves(rgExcluded.data(),
                                 static_cast<uint16_t>(rgExcluded.size()));
        } else {
            ClearExcludedRootMoves();
        }

        Print(0, "Strategy: searching for move #%d of %d (time limit %ds)...\n",
              r + 1, nRanks, m_nTimeLimit);

        // Search the cloned root directly: Iterate returns the best move and a
        // score from the side-to-move (our) perspective, so no negation is
        // needed. Iterate restores the position before returning.
        int nScore = 0;
        CMove best = p->Iterate(&nScore, M_NONE, nullptr);

        // Always clear the exclusions immediately so no later search (here or in
        // any future caller) inherits them.
        ClearExcludedRootMoves();

        BumpProgress();

        if (best == M_NONE) {
            // No further distinct move could be found (the remaining pool was
            // exhausted, or the search was cut short before yielding a move).
            break;
        }

        // SAN is computed from the position *before* the move is made.
        char szSan[32];
        std::string strSan = p->SAN(best, szSan);

        // Derive the opponent's most likely reply and our recommended response
        // from the transposition table left behind by the search just run, so we
        // spend no extra search time on them. A TT miss simply yields "(none)".
        std::string strReplySan;
        std::string strResponseSan;
        CMove Reply = M_NONE;
        CMove Response = M_NONE;
        bool bGameOver = false;

        p->DoMove(best);
        bGameOver = (p->GameEnd() != nullptr);
        if (!bGameOver) {
            Reply = p->ProbeBestMove();
            if (Reply != M_NONE) {
                char szReplySan[32];
                strReplySan = p->SAN(Reply, szReplySan);

                p->DoMove(Reply);
                Response = p->ProbeBestMove();
                if (Response != M_NONE) {
                    char szResponseSan[32];
                    strResponseSan = p->SAN(Response, szResponseSan);
                }
                p->UndoMove(Reply);
            }
        }
        p->UndoMove(best);

        SCandidate Cand;
        Cand.Move = best;
        Cand.nValue = nScore;
        Cand.strSan = strSan;
        Cand.Reply = Reply;
        Cand.Response = Response;
        Cand.strReplySan = strReplySan;
        Cand.strResponseSan = strResponseSan;
        Cand.bGameOver = bGameOver;
        rgCandidates.push_back(Cand);

        // Exclude this move from the remaining searches.
        rgExcluded.push_back(best);

        char szValue[16];
        FormatScore(nScore, szValue, sizeof(szValue));
        Print(0,
              "Strategy: move #%d %s value %s, opponent best reply %s, "
              "response %s\n",
              r + 1, strSan.c_str(), szValue,
              bGameOver ? "(none)"
                        : (strReplySan.empty() ? "(none)" : strReplySan.c_str()),
              bGameOver ? "(game over)"
                        : (strResponseSan.empty() ? "(none)"
                                                  : strResponseSan.c_str()));
    }

    // Time-limited searches surface moves best-first by construction, but the
    // clock is not guaranteed to preserve a strict ordering; sort the (<=3)
    // results by our-perspective score, highest first, as a defensive measure.
    std::stable_sort(rgCandidates.begin(), rgCandidates.end(),
                     [](const SCandidate &a, const SCandidate &b) {
                         return a.nValue > b.nValue;
                     });

    const size_t nTop = rgCandidates.size();

    // Record Suggested Move #1 (the top-ranked move) so the suggest-move feature
    // can reuse it without launching a fresh search. Published together with
    // m_strStrategy / m_fStrategyValid by the strategy thread.
    m_StrategyBestMove = (nTop > 0) ? rgCandidates[0].Move : CMove(M_NONE);
    m_StrategyMoves.clear();
    m_StrategyMoves.reserve(nTop);
    for (size_t nIndex = 0; nIndex < nTop; ++nIndex) {
        m_StrategyMoves.push_back(rgCandidates[nIndex].Move);
    }

    if (nTop == 0) {
        CPosition::Free(p);
        return std::string(
            "No strategy could be determined for the current player.");
    }

    std::ostringstream oss;
    for (size_t i = 0; i < nTop; ++i) {
        const SCandidate &Cand = rgCandidates[i];

        oss << "Suggested Move #" << (i + 1) << ": " << Cand.strSan << "\n";

        if (Cand.bGameOver) {
            // The suggested move ends the game (checkmate / stalemate): there is
            // no opponent reply to consider.
            oss << "Opponent's likely counter move: (none \xe2\x80\x94 no reply)\n";
            oss << "Respond with: (game over)\n";
        } else {
            if (Cand.strReplySan.empty()) {
                oss << "Opponent's likely counter move: (none)\n";
            } else {
                oss << "Opponent's likely counter move: " << Cand.strReplySan
                    << "\n";
            }
            if (Cand.strResponseSan.empty()) {
                oss << "Respond with: (none)\n";
            } else {
                oss << "Respond with: " << Cand.strResponseSan << "\n";
            }
        }

        if (i + 1 < nTop) {
            oss << "\n";
        }
    }

    CPosition::Free(p);
    return oss.str();
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
    const int nMoves = (static_cast<int>(m_pPosition->GetPly()) + 1) / 2;

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
