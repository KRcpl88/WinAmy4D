/*
    WinAmyGUI — WinAmy4dWnd.cpp

    Implementation of CWinAmy4dWnd: window class registration, message loop,
    main-window and 3D child-window procedures, and all command/event handlers
    for the WinAmy 4D chess GUI. This is a pure encapsulation of the logic that
    previously lived as file-scope globals and free functions in main.cpp.
*/

#include "WinAmy4dWnd.h"

#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wchar.h>
#include <cstring>
#include <string>

#include "resource.h"

#include "dbase.h"
#include "heap.h"
#include "move.h"
#include "searchdata.h"
#include "utils.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const wchar_t* APP_CLASS = L"WinAmyGUI_Window";
static const wchar_t* APP_TITLE = L"WinAmy 4D Chess";
static const wchar_t* RENDER_CLASS = L"WinAmyGUI_Render3D";

// Toolbar / control layout
static constexpr int TOOLBAR_H   = 36;   // pixel height of the button panel
static constexpr int STATUSBAR_H = 22;
static constexpr int BTN_W       = 100;
static constexpr int BTN_H       = 28;
static constexpr int BTN_Y       = (TOOLBAR_H - BTN_H) / 2;
static constexpr int BTN_GAP     = 6;
// Shared width for the 2D plane selector and the 3D grid-type selector so the
// two dropdowns line up in the same toolbar slot when switching view modes.
static constexpr int DROPDOWN_W  = 160;

// Timer used to poll single-move (engine / hint) search progress, whose
// percentage is time-based and so has no push event to refresh the status bar.
static constexpr UINT_PTR IDT_SEARCH_PROGRESS = 0xA001;
static constexpr UINT     SEARCH_PROGRESS_MS  = 250;

// ---------------------------------------------------------------------------
// Command-line logging support
//
// WinAmyGUI is a /SUBSYSTEM:WINDOWS application and therefore has no attached
// console: anything the engine writes to stdout (via Print / PrintDebug) is
// discarded and cannot be captured in GUI mode. To diagnose engine behaviour
// (e.g. illegal or non-optimal moves) the only reliable sink is the engine log
// file, which is written whenever a log file has been opened via OpenLogFile().
//
// Logging is opt-in through a command-line switch so normal runs are unaffected:
//
//     WinAmyGUI.exe -log                 -> logs to "Amy.log"
//     WinAmyGUI.exe -log <filename>      -> logs to <filename>
//     WinAmyGUI.exe -log:<filename>      -> logs to <filename>
//     WinAmyGUI.exe -log=<filename>      -> logs to <filename>
//
// The -debug switch additionally enables PrintDebug output to the log file:
//
//     WinAmyGUI.exe -log -debug          -> also mirrors PrintDebug to the log
//
// "-", "--" and "/" prefixes are all accepted and the switch is case-insensitive.
// ---------------------------------------------------------------------------

static bool MatchDebugSwitch(const char* pszArg) {
    // Skip a leading "-", "--" or "/" prefix.
    if (pszArg[0] == '/') {
        ++pszArg;
    } else {
        while (pszArg[0] == '-') {
            ++pszArg;
        }
    }

    return _stricmp(pszArg, "debug") == 0;
}

static const char* MatchLogSwitch(const char* pszArg) {
    // Skip a leading "-", "--" or "/" prefix.
    if (pszArg[0] == '/') {
        ++pszArg;
    } else {
        while (pszArg[0] == '-') {
            ++pszArg;
        }
    }

    if (_strnicmp(pszArg, "log", 3) != 0) {
        return nullptr;
    }

    const char* pszRest = pszArg + 3;
    if (pszRest[0] == '\0') {
        return "";                 // bare switch; filename (if any) is a separate token
    }
    if ((pszRest[0] == ':') || (pszRest[0] == '=')) {
        return pszRest + 1;        // inline filename, e.g. -log:game.log
    }
    return nullptr;                // e.g. "-logfoo" is not our switch
}

static void ConfigureLoggingFromCommandLine(LPSTR lpCmdLine) {
    static const char* const kDefaultLogName = "Amy.log";

    if (lpCmdLine == nullptr) {
        return;
    }

    // GetCommandLine-style splitting is overkill here; a simple whitespace tokenizer
    // is sufficient for the supported switches. Work on a mutable copy.
    std::string strCmdLine(lpCmdLine);
    std::vector<std::string> Tokens;
    {
        size_t nPos = 0;
        while (nPos < strCmdLine.size()) {
            while ((nPos < strCmdLine.size()) &&
                   ((strCmdLine[nPos] == ' ') || (strCmdLine[nPos] == '\t'))) {
                ++nPos;
            }
            size_t nStart = nPos;
            while ((nPos < strCmdLine.size()) &&
                   (strCmdLine[nPos] != ' ') && (strCmdLine[nPos] != '\t')) {
                ++nPos;
            }
            if (nPos > nStart) {
                Tokens.push_back(strCmdLine.substr(nStart, nPos - nStart));
            }
        }
    }

    // First pass: enable debug mode if requested. This is order-independent and
    // does not consume any other tokens.
    for (size_t nIndex = 0; nIndex < Tokens.size(); ++nIndex) {
        if (MatchDebugSwitch(Tokens[nIndex].c_str())) {
            g_nDebugMode = 1;
        }
    }

    for (size_t nIndex = 0; nIndex < Tokens.size(); ++nIndex) {
        const char* pszFile = MatchLogSwitch(Tokens[nIndex].c_str());
        if (pszFile == nullptr) {
            continue;
        }

        std::string strFile;
        if (pszFile[0] != '\0') {
            strFile = pszFile;                         // inline filename
        } else if ((nIndex + 1 < Tokens.size()) &&
                   (Tokens[nIndex + 1][0] != '-') &&
                   (Tokens[nIndex + 1][0] != '/')) {
            strFile = Tokens[++nIndex];                // filename in next token
        } else {
            strFile = kDefaultLogName;                 // default
        }

        OpenLogFile(strFile.c_str());
        Print(0, "WinAmyGUI: logging enabled -> %s\n", strFile.c_str());
#ifdef HAVE___BUILTIN_POPCOUNTLL
        Print(0, "BUILTIN_POPCOUNTLL = %d, ", HAVE___BUILTIN_POPCOUNTLL);
#endif
#ifdef HAVE_POPCNT
        Print(0, "POPCNT = %d, ", HAVE_POPCNT);
#endif
#ifdef HAVE_SYS_TIME_H
        Print(0, "SYS_TIME = %d, ", HAVE_SYS_TIME_H);
#endif
#ifdef HAVE_UNISTD_H
        Print(0, "UNISTD = %d, ", HAVE_UNISTD_H);
#endif
#ifdef HAVE_SYS_SOCKET_H
        Print(0, "SYS_SOCKET = %d, ", HAVE_SYS_SOCKET_H);
#endif
#ifdef HAVE_NETDB_H
        Print(0, "NETDB_H = %d, ", HAVE_NETDB_H);
#endif
#ifdef HAVE_SELECT
        Print(0, "SELECT = %d, ", HAVE_SELECT);
#endif
#ifdef HAVE_GETHOSTNAME
        Print(0, "GETHOSTNAME = %d, ", HAVE_GETHOSTNAME);
#endif
#ifdef HAVE_SETBUF
        Print(0, "SETBUF = %d", HAVE_SETBUF);
#endif
        Print(0, "\n");
#if MP
        Print(0, "Multiprocessor support.\n\n");
#else
        Print(0, "No multiprocessor support.\n\n");
#endif
        return;
    }
}


// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

CWinAmy4dWnd::CWinAmy4dWnd() = default;
CWinAmy4dWnd::~CWinAmy4dWnd() = default;

// ---------------------------------------------------------------------------
// Static window-procedure thunks
//
// The instance pointer is supplied via CreateWindowExW's lpParam and stashed
// in GWLP_USERDATA on WM_NCCREATE, then retrieved on every subsequent message
// so the static callback can forward to the matching instance method.
// ---------------------------------------------------------------------------

LRESULT CALLBACK CWinAmy4dWnd::StaticWndProc(HWND hWnd, UINT uMsg,
                                             WPARAM wParam, LPARAM lParam) {
    CWinAmy4dWnd* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CWinAmy4dWnd*>(pcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<CWinAmy4dWnd*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    if (pThis)
        return pThis->WndProc(hWnd, uMsg, wParam, lParam);
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK CWinAmy4dWnd::StaticRender3DProc(HWND hWnd, UINT uMsg,
                                                  WPARAM wParam, LPARAM lParam) {
    CWinAmy4dWnd* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* pcs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<CWinAmy4dWnd*>(pcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<CWinAmy4dWnd*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    if (pThis)
        return pThis->Render3DProc(hWnd, uMsg, wParam, lParam);
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Game / move helpers
// ---------------------------------------------------------------------------

bool CWinAmy4dWnd::IsHumanAllowedToMove(const CPosition* pPos) const {
    if (!pPos) return false;
    PlayerMode eMode = m_Game.GetPlayerMode();
    if (eMode == PlayerMode::TwoPlayers) return true;
    if (eMode == PlayerMode::OnePlayer) return (pPos->GetTurn() == 0);
    return false;
}

void CWinAmy4dWnd::CollectLegalDestinationsForSquare(
    const CPosition* pPos,
    const CSCoord& From,
    std::vector<CSCoord>& rgDests) {
    rgDests.clear();
    if (!pPos || !From.IsValid()) return;

    uint16_t wOff = From.BitOffset();
    int8_t nPiece = pPos->GetPiece(wOff);
    if (nPiece == 0) return;

    CPosition* pMovePos = CPosition::Clone(pPos);
    if (!pMovePos) return;

    pMovePos->SetTurn((nPiece > 0) ? 0 : 1);

    heap_t pHeap = allocate_heap();
    push_section(pHeap);
    pMovePos->LegalMoves(pHeap);
    for (unsigned int nIndex = pHeap->current_section->start;
         nIndex < pHeap->current_section->end; ++nIndex) {
        CMove mv = pHeap->data[nIndex];
        if (mv.GetFromCoord() == From) {
            rgDests.push_back(mv.GetToCoord());
        }
    }
    free_heap(pHeap);
    CPosition::Free(pMovePos);
}

void CWinAmy4dWnd::AppendMoveHighlightSquares(
    std::vector<CSCoord>& Squares,
    const CMove& mv) {
    const CSCoord rgMoveSquares[] = {
        mv.GetFromCoord(),
        mv.GetToCoord(),
    };

    for (const CSCoord& sqMove : rgMoveSquares) {
        if (!sqMove.IsValid()) {
            continue;
        }

        bool fFound = false;
        for (const CSCoord& sqExisting : Squares) {
            if (sqExisting.IsValid()
                    && sqExisting.BitOffset() == sqMove.BitOffset()) {
                fFound = true;
                break;
            }
        }
        if (!fFound) {
            Squares.push_back(sqMove);
        }
    }
}

void CWinAmy4dWnd::CollectLegalMoveHighlightsForSide(
    const CPosition* pPos,
    HighlightSide eSide,
    std::vector<CSCoord>& Squares) {
    Squares.clear();
    if (!pPos || eSide == HighlightSide::None) {
        return;
    }

    CPosition* pMovePos = CPosition::Clone(pPos);
    if (!pMovePos) {
        return;
    }

    pMovePos->SetTurn(eSide == HighlightSide::White ? 0 : 1);

    heap_t pHeap = allocate_heap();
    push_section(pHeap);
    pMovePos->LegalMoves(pHeap);
    for (unsigned int nIndex = pHeap->current_section->start;
         nIndex < pHeap->current_section->end; ++nIndex) {
        AppendMoveHighlightSquares(Squares, pHeap->data[nIndex]);
    }
    free_heap(pHeap);
    CPosition::Free(pMovePos);
}

bool CWinAmy4dWnd::TryMakeSelectedMove(const CPosition* pPos, const CSCoord& sqTo) {
    if (!pPos || !IsHumanAllowedToMove(pPos)) return false;

    uint16_t wSelOff = m_SelectedSquare.BitOffset();
    int8_t nSelPiece = pPos->GetPiece(wSelOff);
    if (nSelPiece == 0) return false;

    bool fSelIsWhite = (nSelPiece > 0);
    bool fWhiteTurn = (pPos->GetTurn() == 0);
    if (fSelIsWhite != fWhiteTurn) return false;

    // The user is committing a move with one of their own pieces. If a
    // suggest-move / strategy / engine search is still running, abort it and
    // wait for the engine thread to finish before touching the live position:
    // this discards the search's now-irrelevant result and frees the shared
    // engine state so the next search can start. The caller only invokes this
    // for a square already known to be a legal destination of the selected
    // piece, so a move is about to be made.
    if (m_Game.IsEngineRunning()) {
        m_Game.AbortAndJoinSearch();
    }

    heap_t pHeap = allocate_heap();
    push_section(pHeap);
    const_cast<CPosition*>(pPos)->LegalMoves(pHeap);
    for (unsigned int nIndex = pHeap->current_section->start;
         nIndex < pHeap->current_section->end; ++nIndex) {
        CMove mv = pHeap->data[nIndex];
        if (mv.GetFromCoord() == m_SelectedSquare && mv.GetToCoord() == sqTo) {
            // Log that the human player initiated this move (the side moving is
            // the side to move in the current position). MakeMove() logs the
            // move itself; a MakeMove log with no matching "Player made move"
            // entry therefore indicates a computer move.
            {
                char szSan[32];
                const char *pszSan = const_cast<CPosition *>(pPos)->SAN(mv, szSan);
                const char *pszSide = (pPos->GetTurn() == 0) ? "White" : "Black";
                Print(0, "Player made move: %s by %s\n", pszSan, pszSide);
            }
            m_Game.MakeMove(mv);
            free_heap(pHeap);
            return true;
        }
    }

    free_heap(pHeap);
    return false;
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

// Returns the size of the renderable client area (below the toolbar,
// above the status bar). Used by the D3D renderer to size its swap chain.
SIZE CWinAmy4dWnd::GetRenderAreaSize(HWND hWnd) const {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top - TOOLBAR_H - STATUSBAR_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return SIZE{ w, h };
}

// Returns the extra top-left offset (within the 2D render area) needed to
// center the board whenever the client area is larger than the board itself.
POINT CWinAmy4dWnd::Get2DBoardOffset(HWND hWnd) const {
    RECT rcClient{};
    GetClientRect(hWnd, &rcClient);
    int nClientW = rcClient.right - rcClient.left;
    int nClientH = rcClient.bottom - rcClient.top - TOOLBAR_H - STATUSBAR_H;
    if (nClientH < 0) nClientH = 0;

    SIZE boardSz = BoardRenderer::GetBoardAreaSize();
    POINT ptOffset{ 0, 0 };
    if (nClientW > boardSz.cx) {
        ptOffset.x = (nClientW - boardSz.cx) / 2;
    }
    if (nClientH > boardSz.cy) {
        ptOffset.y = (nClientH - boardSz.cy) / 2;
    }
    return ptOffset;
}

// Handles a click on a CSCoord that came from the 3D pick. Mirrors the
// selection/move logic in OnSquareClick but skips the 2D hit-test.
void CWinAmy4dWnd::OnSquareClick3D(const CSCoord& sq) {
    // Input is intentionally NOT blocked while the engine is searching; see the
    // note in OnSquareClick. TryMakeSelectedMove() aborts the in-flight search
    // when the user actually commits a move.
    if (m_Game.IsGameOver()
        || m_Game.GetPlayerMode() == PlayerMode::ZeroPlayers) return;
    const CPosition* pos = m_Game.GetPosition();
    if (!pos) return;

    if (!m_fHaveSelection) {
        uint16_t off = sq.BitOffset();
        int8_t piece = pos->GetPiece(off);
        if (piece == 0) return;
        m_fHaveSelection = true;
        m_SelectedSquare = sq;
        CollectLegalDestinationsForSquare(pos, sq, m_rgLegalDests);
        InvalidateRect(m_hRender3D ? m_hRender3D : m_hWnd, nullptr, FALSE);
    } else {
        bool madeMove = false;
        for (const auto& dest : m_rgLegalDests) {
            if (dest == sq) {
                madeMove = TryMakeSelectedMove(pos, sq);
                break;
            }
        }
        m_fHaveSelection = false;
        m_rgLegalDests.clear();
        InvalidateRect(m_hRender3D ? m_hRender3D : m_hWnd, nullptr, FALSE);
        if (madeMove) {
            m_fHaveHint = false;
            m_fStrategyHints = false;
            m_HintSquares.clear();
            RefreshLegalMoveHighlights();
            UpdateSuggestMoveButton();
            UpdateStatusBar();
            if (!m_Game.IsGameOver()) {
                MaybeStartEngine();
            }
            // The move may have aborted an in-flight search; refresh the pause
            // menu so it reflects whether the engine is now running again.
            UpdatePauseMenu();
        }
    }
}

// Render3D child window proc — handles mouse for orbit/zoom and paint via D3D.
LRESULT CWinAmy4dWnd::Render3DProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1; // suppress flicker; the swap chain owns the surface
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        if (m_D3DRenderer.IsInitialized()) {
            const CSCoord* sel = m_fHaveSelection ? &m_SelectedSquare : nullptr;
            std::vector<CSCoord> HintSquares = GetHintSquaresForRender();
            m_D3DRenderer.Render(m_Game.GetPosition(), sel, m_rgLegalDests,
                                 HintSquares);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (m_D3DRenderer.IsInitialized())
            m_D3DRenderer.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        if (m_D3DRenderer.IsInitialized())
            m_D3DRenderer.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        if (m_D3DRenderer.IsInitialized()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            m_D3DRenderer.OnMouseUp(x, y);
            if (m_D3DRenderer.LastInteractionWasClick()) {
                CSCoord sq = m_D3DRenderer.HitTest3D(x, y);
                if (sq.IsValid()) OnSquareClick3D(sq);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (m_D3DRenderer.IsInitialized())
            m_D3DRenderer.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Run — register classes, create the window, pump messages
// ---------------------------------------------------------------------------

int CWinAmy4dWnd::Run(HINSTANCE hInstance, int /*nCmdShow*/, LPSTR lpCmdLine) {
    // Enable optional file logging before the engine starts so that any startup
    // diagnostics are captured. See ConfigureLoggingFromCommandLine() for the
    // accepted switches; logging is off unless requested on the command line.
    ConfigureLoggingFromCommandLine(lpCmdLine);

    // Initialise common controls (needed for spin/status bar).
    INITCOMMONCONTROLSEX icce{sizeof(icce), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icce);

    // Initialise chess engine.
    GameController::InitEngine();

    // Start the initial game now that the engine attack tables are ready.
    // (The GameController constructor intentionally does NOT do this, because
    // it runs during construction before InitEngine().)
    m_Game.NewGame();

    // Register window class.
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = StaticWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.lpszClassName = APP_CLASS;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    // Register the child render window class used for 3D mode.
    WNDCLASSEXW rwc{};
    rwc.cbSize        = sizeof(rwc);
    rwc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    rwc.lpfnWndProc   = StaticRender3DProc;
    rwc.hInstance     = hInstance;
    rwc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    rwc.hbrBackground = nullptr; // we own the surface via the swap chain
    rwc.lpszClassName = RENDER_CLASS;
    RegisterClassExW(&rwc);

    SIZE boardSz = BoardRenderer::GetBoardAreaSize();
    int winW = boardSz.cx + 16;
    int winH = boardSz.cy + TOOLBAR_H + STATUSBAR_H + 60;

    HWND hWnd = CreateWindowExW(
        0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        nullptr, nullptr, hInstance, this
    );

    ShowWindow(hWnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hWnd);

    // Start the first engine move if in auto mode.
    MaybeStartEngine();

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ---------------------------------------------------------------------------
// CreateControls — called once on WM_CREATE
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::CreateControls(HWND hWnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
    int x = BTN_GAP;

    auto makeBtn = [&](const wchar_t* label, int id, int w = BTN_W) -> HWND {
        HWND h = CreateWindowExW(0, L"BUTTON", label,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, BTN_Y, w, BTN_H, hWnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        x += w + BTN_GAP;
        return h;
    };

    // 2D/3D view toggle — always enabled regardless of current view mode.
    // Label is kept in sync with m_eViewMode by UpdateViewToggleButton.
    m_hBtnViewToggle = makeBtn(L"Switch to 3D", IDC_BTN_VIEW_TOGGLE, 110);

    // Grid-type dropdown — only enabled in 3D mode. The list contents map
    // 1:1 onto CUCoord::EOutlineType in declaration order, so combobox
    // index N == (EOutlineType)(OT_full + N).
    {
        int nCbH = 220; // includes dropdown extent (8 items + decorations).
        m_nDropdownX = x; // shared origin for the 2D/3D mode dropdowns.
        m_hCbGridType = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                | CBS_DROPDOWNLIST,
            x, BTN_Y, DROPDOWN_W, nCbH, hWnd,
            (HMENU)(INT_PTR)IDC_CB_GRID_TYPE, hInst, nullptr);
        x += DROPDOWN_W + BTN_GAP;
        static const wchar_t* kGridLabels[] = {
            L"Full Dodecahedron",
            L"Square (x/y plane)",
            L"Square (x/z plane)",
            L"Square (y/z plane)",
            L"Hex 1 (-X,-Y,-Z)",
            L"Hex 2 (-X,-Y,+Z)",
            L"Hex 3 (-X,+Y,-Z)",
            L"Hex 4 (+X,-Y,-Z)",
        };
        for (auto* psz : kGridLabels) {
            SendMessageW(m_hCbGridType, CB_ADDSTRING, 0, (LPARAM)psz);
        }
        int nInitial = static_cast<int>(m_D3DRenderer.GetOutlineType())
                     - static_cast<int>(CUCoord::OT_full);
        SendMessageW(m_hCbGridType, CB_SETCURSEL, (WPARAM)nInitial, 0);
        EnableWindow(m_hCbGridType, FALSE); // 2D by default.
    }

    x += BTN_GAP * 2;

    // 3D-mode toggle: when checked the camera rotates so the selected outline
    // type faces the view; when unchecked (the default) changing the outline
    // type leaves the camera orientation unchanged. Placed between the grid-type
    // selector and the zoom buttons.
    {
        m_hBtnRotateGrid = CreateWindowExW(0, L"BUTTON", L"Rotate Grid",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
            x, BTN_Y, 100, BTN_H, hWnd,
            (HMENU)(INT_PTR)IDC_BTN_ROTATE_GRID, hInst, nullptr);
        x += 100 + BTN_GAP;
        // Default to unchecked, matching the renderer's m_bRotateGrid default.
        SendMessageW(m_hBtnRotateGrid, BM_SETCHECK, BST_UNCHECKED, 0);
    }
    x += BTN_GAP;

    // 3D-mode zoom controls.
    m_hBtnZoomIn  = makeBtn(L"Zoom +", IDC_BTN_ZOOM_IN,  60);
    m_hBtnZoomOut = makeBtn(L"Zoom -", IDC_BTN_ZOOM_OUT, 60);
    // 2D-mode control: selects which plane of the 4D board the flat view shows
    // (an axis swap applied purely for rendering). Hidden in 3D mode.
    {
        int nCbH = 140;
        m_hCbSwapAxes = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
            m_nDropdownX, BTN_Y, DROPDOWN_W, nCbH, hWnd,
            (HMENU)(INT_PTR)IDC_CB_SWAP_AXES, hInst, nullptr);
        // Index order matches the switch in OnCommand: 0=x/y, 1=x/z, 2=y/z.
        static const wchar_t* kSwapLabels[] = {
            L"x/y plane",
            L"x/z plane",
            L"y/z plane",
        };
        for (auto* psz : kSwapLabels) {
            SendMessageW(m_hCbSwapAxes, CB_ADDSTRING, 0, (LPARAM)psz);
        }
        SendMessageW(m_hCbSwapAxes, CB_SETCURSEL, 0, 0);
    }

    // Initial visibility for the current (2D) view mode: the 3D-only view
    // controls are hidden, while the 2D-only plane selector stays visible.
    ShowWindow(m_hCbGridType,  SW_HIDE);
    ShowWindow(m_hBtnZoomIn,   SW_HIDE);
    ShowWindow(m_hBtnZoomOut,  SW_HIDE);
    ShowWindow(m_hBtnRotateGrid, SW_HIDE);
    ShowWindow(m_hCbSwapAxes,  SW_SHOW);

    // Status bar
    m_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);

    // 3D render child window — covers the area between the toolbar and the
    // status bar. Initially hidden; SetViewMode toggles visibility.
    m_hRender3D = CreateWindowExW(0, RENDER_CLASS, nullptr,
        WS_CHILD,
        0, TOOLBAR_H, 10, 10,
        hWnd, nullptr, hInst, this);

    // Set initial menu / button states.
    UpdatePlayerMenu();
    UpdatePauseMenu();
    UpdateLegalMoveHighlightMenu();
    UpdateViewToggleButton();
    UpdateAxisControls();
    UpdateSuggestMoveButton();
}

// ---------------------------------------------------------------------------
// OnNewGame
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnNewGame() {
    m_Game.PauseEngine();
    m_fPaused = false;
    m_fHaveSelection = false;
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_fGameOverAnnounced = false;
    m_HintSquares.clear();
    m_rgLegalDests.clear();
    m_Game.NewGame();
    RefreshLegalMoveHighlights();
    // Preserve the depth previously selected via the Options menu.
    UpdatePauseMenu();
    UpdateSuggestMoveButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateStatusBar();
    MaybeStartEngine();
}

void CWinAmy4dWnd::OnLoadEPDGame() {
    wchar_t rgPath[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter =
        L"EPD4 Files (*.epd4)\0*.epd4\0"
        L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = rgPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"epd4";

    if (!GetOpenFileNameW(&ofn))
        return;

    if (!m_Game.LoadFromEPDFile(rgPath)) {
        MessageBoxW(m_hWnd, L"Failed to load EPD file.", APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    m_fPaused = false;
    m_fHaveSelection = false;
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_fGameOverAnnounced = false;
    m_HintSquares.clear();
    m_rgLegalDests.clear();
    RefreshLegalMoveHighlights();
    UpdatePauseMenu();
    UpdateSuggestMoveButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateStatusBar();
    MaybeStartEngine();
}

void CWinAmy4dWnd::OnSaveEPDGame() {
    wchar_t rgPath[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter =
        L"EPD4 Files (*.epd4)\0*.epd4\0"
        L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = rgPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"epd4";

    if (!GetSaveFileNameW(&ofn))
        return;

    if (!m_Game.SaveToEPDFile(rgPath)) {
        MessageBoxW(m_hWnd, L"Failed to save EPD file.", APP_TITLE, MB_OK | MB_ICONERROR);
    }
}

void CWinAmy4dWnd::OnLoadPGNGame() {
    wchar_t rgPath[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter =
        L"PGN4 Files (*.pgn4)\0*.pgn4\0"
        L"PGN Files (*.pgn)\0*.pgn\0"
        L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = rgPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"pgn4";

    if (!GetOpenFileNameW(&ofn))
        return;

    if (!m_Game.LoadFromPGNFile(rgPath)) {
        MessageBoxW(m_hWnd, L"Failed to load PGN file.", APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    m_fPaused = false;
    m_fHaveSelection = false;
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_fGameOverAnnounced = false;
    m_HintSquares.clear();
    m_rgLegalDests.clear();
    RefreshLegalMoveHighlights();
    UpdatePauseMenu();
    UpdateSuggestMoveButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateStatusBar();
    MaybeStartEngine();
}

void CWinAmy4dWnd::OnSavePGNGame() {
    wchar_t rgPath[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter =
        L"PGN4 Files (*.pgn4)\0*.pgn4\0"
        L"PGN Files (*.pgn)\0*.pgn\0"
        L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = rgPath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"pgn4";

    if (!GetSaveFileNameW(&ofn))
        return;

    if (!m_Game.SaveToPGNFile(rgPath)) {
        MessageBoxW(m_hWnd, L"Failed to save PGN file.", APP_TITLE, MB_OK | MB_ICONERROR);
    }
}

// ---------------------------------------------------------------------------
// MaybeStartEngine — trigger engine search if it is the engine's turn
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::MaybeStartEngine() {
    if (m_Game.IsGameOver()) return;
    if (m_Game.IsEngineRunning()) return;
    if (m_fPaused) return;

    const CPosition* pos = m_Game.GetPosition();
    if (!pos) return;

    PlayerMode mode = m_Game.GetPlayerMode();
    if (mode == PlayerMode::TwoPlayers) return;

    bool engineTurn = false;
    if (mode == PlayerMode::ZeroPlayers) {
        engineTurn = true;
    } else if (mode == PlayerMode::OnePlayer) {
        // Engine plays Black (turn 1).
        engineTurn = (pos->GetTurn() == 1);
    }

    if (engineTurn) {
        m_Game.StartEngineSearch(m_hWnd);
        StartSearchProgressTimer();
        UpdateStatusBar();
        UpdatePauseMenu();
    }
}

// ---------------------------------------------------------------------------
// OnEngineMove — handle WM_APP_ENGINE_MOVE
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnEngineMove(LPARAM /*lParam*/) {
    CMove move = m_Game.GetBestMove();
    if (move == M_NONE) {
        UpdatePauseMenu();
        UpdateStatusBar();
        // The engine returns M_NONE when there is no move (checkmate or
        // stalemate on its turn); announce the result in that case too.
        MaybeAnnounceGameOver();
        return;
    }

    bool fApplied = m_Game.MakeMove(move);
    if (!fApplied && m_Game.IsEngineCorrupted()) {
        // The engine kept returning the same invalid move; MakeMove has already
        // switched to human-only play. Tell the user and stop searching so they
        // can save the game.
        m_fHaveSelection = false;
        m_rgLegalDests.clear();
        m_fHaveHint = false;
        m_fStrategyHints = false;
        m_HintSquares.clear();
        RefreshLegalMoveHighlights();
        UpdateSuggestMoveButton();
        UpdatePlayerMenu();
        UpdatePauseMenu();
        InvalidateRect(m_hWnd, nullptr, TRUE);
        if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
        UpdateStatusBar();
        MessageBoxW(m_hWnd,
                    L"Engine corrupted: the engine repeatedly returned an "
                    L"invalid move. Play has been switched to human only so you "
                    L"can save the game.",
                    APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }
    m_fHaveSelection = false;
    m_rgLegalDests.clear();
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_HintSquares.clear();
    RefreshLegalMoveHighlights();
    UpdateSuggestMoveButton();

    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateStatusBar();

    if (m_Game.IsGameOver()) {
        UpdatePauseMenu();
        MaybeAnnounceGameOver();
        return;
    }

    // For self-play, immediately start the engine again (other side).
    if (m_Game.GetPlayerMode() == PlayerMode::ZeroPlayers && !m_fPaused) {
        m_Game.StartEngineSearch(m_hWnd);
        StartSearchProgressTimer();
        UpdateStatusBar();
        UpdatePauseMenu();
    } else {
        MaybeStartEngine();
        UpdatePauseMenu();
    }
}

// ---------------------------------------------------------------------------
// ClearHint — drop any active move suggestion highlight
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::ClearHint() {
    if (!m_fHaveHint && !m_fStrategyHints && m_HintSquares.empty()) {
        return;
    }
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_HintSquares.clear();
    UpdateSuggestMoveButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
}

void CWinAmy4dWnd::RefreshLegalMoveHighlights() {
    CollectLegalMoveHighlightsForSide(m_Game.GetPosition(), m_eHighlightSide,
                                      m_LegalMoveHintSquares);
}

void CWinAmy4dWnd::SetLegalMoveHighlightSide(HighlightSide eSide) {
    if (m_eHighlightSide == eSide) {
        m_eHighlightSide = HighlightSide::None;
    } else {
        m_eHighlightSide = eSide;
    }

    RefreshLegalMoveHighlights();
    UpdateLegalMoveHighlightMenu();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) {
        InvalidateRect(m_hRender3D, nullptr, FALSE);
    }
}

void CWinAmy4dWnd::UpdateLegalMoveHighlightMenu() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) {
        return;
    }

    CheckMenuItem(hMenu, IDM_HIGHLIGHT_WHITE,
                  MF_BYCOMMAND
                      | (m_eHighlightSide == HighlightSide::White
                             ? MF_CHECKED
                             : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_HIGHLIGHT_BLACK,
                  MF_BYCOMMAND
                      | (m_eHighlightSide == HighlightSide::Black
                             ? MF_CHECKED
                             : MF_UNCHECKED));
}

void CWinAmy4dWnd::UpdateSuggestMoveButton() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) {
        return;
    }
    bool fEnabled = !m_fStrategyHints
                 && !m_Game.IsEngineRunning()
                 && !m_Game.IsGameOver()
                 && m_Game.GetPlayerMode() != PlayerMode::ZeroPlayers;
    EnableMenuItem(hMenu, IDM_SUGGEST,
        MF_BYCOMMAND | (fEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
}

std::vector<CSCoord> CWinAmy4dWnd::GetHintSquaresForRender() const {
    std::vector<CSCoord> Squares = m_LegalMoveHintSquares;
    for (const CSCoord& sqHint : m_HintSquares) {
        bool fFound = false;
        for (const CSCoord& sqExisting : Squares) {
            if (sqHint.IsValid() && sqExisting.IsValid()
                    && sqHint.BitOffset() == sqExisting.BitOffset()) {
                fFound = true;
                break;
            }
        }
        if (!fFound) {
            Squares.push_back(sqHint);
        }
    }
    return Squares;
}

// ---------------------------------------------------------------------------
// OnSuggestMove — ask the engine to recommend a move for the human player
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnSuggestMove() {
    if (m_Game.IsEngineRunning()) {
        return;
    }
    if (m_Game.IsGameOver()) {
        return;
    }
    if (m_fStrategyHints) {
        return;
    }
    // A suggestion only makes sense when a human is to move.
    if (m_Game.GetPlayerMode() == PlayerMode::ZeroPlayers) {
        return;
    }

    const CPosition* pos = m_Game.GetPosition();
    if (!pos) {
        return;
    }

    // In 1-player mode the human plays White (turn 0); don't suggest a move
    // while it is the engine's turn.
    if (m_Game.GetPlayerMode() == PlayerMode::OnePlayer && pos->GetTurn() == 1) {
        return;
    }

    // Clear any stale suggestion and current selection, then run the search.
    ClearHint();
    m_fHaveSelection = false;
    m_rgLegalDests.clear();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) {
        InvalidateRect(m_hRender3D, nullptr, FALSE);
    }

    m_Game.StartHintSearch(m_hWnd);
    StartSearchProgressTimer();
    UpdateStatusBar();
    UpdatePauseMenu();
}

// ---------------------------------------------------------------------------
// OnEngineHint — handle WM_APP_ENGINE_HINT (suggestion search completed)
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnEngineHint(LPARAM /*lParam*/) {
    CMove move = m_Game.GetBestMove();
    UpdateStatusBar();
    UpdatePauseMenu();
    if (move == M_NONE) {
        // No legal move to suggest (checkmate/stalemate) — nothing to show.
        return;
    }

    m_HintSquares.clear();
    AppendMoveHighlightSquares(m_HintSquares, move);
    m_fStrategyHints = false;
    m_fHaveHint = true;
    UpdateSuggestMoveButton();

    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// OnStrategy — compute a short strategy (top 3 moves) for the current player
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnStrategy() {
    if (m_Game.IsEngineRunning()) {
        return;
    }
    if (m_Game.IsGameOver()) {
        return;
    }

    const CPosition* pos = m_Game.GetPosition();
    if (!pos) {
        return;
    }

    // If a strategy has already been computed for the current position, reuse it
    // instead of running the search again — the cache is invalidated whenever a
    // move is made, so this stays valid until the player moves again.
    if (m_Game.HasStrategy()) {
        OnEngineStrategy(0);
        return;
    }

    // Clear any stale suggestion / selection so the board isn't left with
    // highlights that no longer reflect the current interaction.
    ClearHint();
    m_fHaveSelection = false;
    m_rgLegalDests.clear();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) {
        InvalidateRect(m_hRender3D, nullptr, FALSE);
    }

    m_Game.StartStrategySearch(m_hWnd);
    StartSearchProgressTimer();
    UpdateStatusBar();
    UpdatePauseMenu();
}

// ---------------------------------------------------------------------------
// OnEngineStrategy — handle WM_APP_ENGINE_STRATEGY (strategy search completed)
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnEngineStrategy(LPARAM /*lParam*/) {
    m_HintSquares.clear();
    for (const CMove& mv : m_Game.GetStrategyMoves()) {
        AppendMoveHighlightSquares(m_HintSquares, mv);
    }
    m_fHaveHint = !m_HintSquares.empty();
    m_fStrategyHints = m_fHaveHint;
    UpdateSuggestMoveButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) {
        InvalidateRect(m_hRender3D, nullptr, FALSE);
    }

    UpdateStatusBar();
    UpdatePauseMenu();

    std::string strStrategy = m_Game.GetStrategyText();
    if (strStrategy.empty()) {
        strStrategy = "No strategy could be computed.";
    }

    int nLen = MultiByteToWideChar(CP_UTF8, 0, strStrategy.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> rgwText(nLen > 0 ? static_cast<size_t>(nLen) : 1, 0);
    if (nLen > 0) {
        MultiByteToWideChar(CP_UTF8, 0, strStrategy.c_str(), -1, rgwText.data(), nLen);
    }

    MessageBoxW(m_hWnd, rgwText.data(), L"Strategy", MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// OnSquareClick — handle a left-click on the board area
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::OnSquareClick(POINT pt) {
    // Note: input is intentionally NOT blocked while the engine is searching.
    // The user may select any piece to inspect its legal moves, and may commit
    // a move of their own; TryMakeSelectedMove() aborts the in-flight search
    // when a move is actually made (see below).
    if (m_Game.IsGameOver()) return;
    if (m_Game.GetPlayerMode() == PlayerMode::ZeroPlayers) return;

    const CPosition* pos = m_Game.GetPosition();
    if (!pos) return;

    // pt is already in board coordinates (scroll + toolbar applied by caller).
    CSCoord sq = m_Renderer.HitTest(pt);

    if (!sq.IsValid()) {
        m_fHaveSelection = false;
        m_rgLegalDests.clear();
        InvalidateRect(m_hWnd, nullptr, TRUE);
        return;
    }

    if (!m_fHaveSelection) {
        // Select any occupied square to inspect legal moves for that side.
        uint16_t off = sq.BitOffset();
        int8_t piece = pos->GetPiece(off);
        if (piece == 0) return;

        m_fHaveSelection = true;
        m_SelectedSquare = sq;
        CollectLegalDestinationsForSquare(pos, sq, m_rgLegalDests);

        InvalidateRect(m_hWnd, nullptr, TRUE);
    } else {
        // Attempt to make a move to the clicked destination.
        bool madeMove = false;
        for (const auto& dest : m_rgLegalDests) {
            if (dest == sq) {
                madeMove = TryMakeSelectedMove(pos, sq);
                break;
            }
        }

        m_fHaveSelection = false;
        m_rgLegalDests.clear();
        InvalidateRect(m_hWnd, nullptr, TRUE);

        if (madeMove) {
            m_fHaveHint = false;
            m_fStrategyHints = false;
            m_HintSquares.clear();
            RefreshLegalMoveHighlights();
            UpdateSuggestMoveButton();
            UpdateStatusBar();
            if (!m_Game.IsGameOver()) {
                MaybeStartEngine();
            } else {
                MaybeAnnounceGameOver();
            }
            // The move may have aborted an in-flight search; refresh the pause
            // menu so it reflects whether the engine is now running again.
            UpdatePauseMenu();
        }
    }
}

// ---------------------------------------------------------------------------
// Search-progress polling timer
//
// Single-move searches (the engine's own move and the suggest-move / hint
// search) and strategy computations all report a percentage of completion based
// on the number of moves searched. There is no engine event to push such
// updates to the UI, so a low-frequency timer polls the progress percentage and
// refreshes the status bar while any search runs.
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::StartSearchProgressTimer() {
    // SetTimer with an existing id simply resets it, so calling this on each
    // search start (including self-play continuations) is safe.
    SetTimer(m_hWnd, IDT_SEARCH_PROGRESS, SEARCH_PROGRESS_MS, nullptr);
}

void CWinAmy4dWnd::StopSearchProgressTimer() {
    KillTimer(m_hWnd, IDT_SEARCH_PROGRESS);
}

// ---------------------------------------------------------------------------
// SearchProgressPercent
// ---------------------------------------------------------------------------

// Progress of the in-flight engine move / suggest-move search, expressed as a
// whole-number percentage of the root moves to be searched, or -1 when no
// search is active (or progress is not yet known).
//
// Iterative deepening searches every root move once per depth, so the total
// work is (number of iterations) x (root moves). Progress is therefore the root
// moves searched at completed depths plus those searched so far at the current
// depth, divided by that total. The figure climbs smoothly from 0 to 100 across
// the whole search rather than resetting on each new iteration.
//
// The values are read from the live CSearchData of the position being searched
// (see GameController::GetEngineSearchPosition / CPosition::GetSearchData). A
// momentarily torn read only perturbs the reported percentage, which is
// harmless.
int CWinAmy4dWnd::SearchProgressPercent() const {
    const CPosition* pSearchPos = m_Game.GetEngineSearchPosition();
    if (!pSearchPos) {
        return -1;
    }

    const CSearchData* pSearchData = pSearchPos->GetSearchData();
    if (!pSearchData) {
        return -1;
    }

    int nTotalMoves = pSearchData->m_wRootMoves;
    if (nTotalMoves <= 0) {
        return -1;
    }

    // The root loop runs depths 1 .. (max depth - 1), so the number of
    // iterations is one less than the configured search depth.
    int nIterations = m_Game.GetDepth() - 1;
    if (nIterations < 1) {
        nIterations = 1;
    }

    int nDepth = pSearchData->m_wDepth;
    if (nDepth < 1) {
        nDepth = 1;
    }
    if (nDepth > nIterations) {
        nDepth = nIterations;
    }

    int nDone = pSearchData->m_wMoveNum;
    if (nDone < 0) {
        nDone = 0;
    }
    if (nDone > nTotalMoves) {
        nDone = nTotalMoves;
    }

    long lDone = (long)(nDepth - 1) * nTotalMoves + nDone;
    long lAll = (long)nIterations * nTotalMoves;
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

// ---------------------------------------------------------------------------
// UpdateStatusBar
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::UpdateStatusBar() {
    if (!m_hStatus) return;
    const char* gameEnd = m_Game.GetGameEndMessage();
    if (gameEnd) {
        // Show the friendly result text (outcome, winner, move count).
        std::string strResult = m_Game.GetGameResultText();
        if (strResult.empty())
            strResult = gameEnd;
        wchar_t wide[256]{};
        MultiByteToWideChar(CP_UTF8, 0, strResult.c_str(), -1, wide, 256);
        SendMessageW(m_hStatus, WM_SETTEXT, 0, (LPARAM)wide);
        return;
    }

    const CPosition* pos = m_Game.GetPosition();
    if (!pos) return;

    // Prefix the status with the most recently played move (e.g.
    // "White move #7 Phe2he3 - ") so the user can see what the previous side
    // just did. Empty at the start of a game (no move played yet).
    std::wstring strPrefix;
    std::string strLastMove = m_Game.GetLastMoveText();
    if (!strLastMove.empty()) {
        wchar_t wideLast[160]{};
        MultiByteToWideChar(CP_UTF8, 0, strLastMove.c_str(), -1, wideLast, 160);
        strPrefix = wideLast;
        strPrefix += L" - ";
    }

    wchar_t buf[256];
    const wchar_t* turn = (pos->GetTurn() == 0) ? L"White to move" : L"Black to move";
    if (m_Game.IsComputingStrategy() || m_Game.IsEngineRunning()) {
        // A search is in progress. Show the percentage of the work completed,
        // measured as the fraction of root moves searched (engine move / hint) or
        // ranked searches completed (strategy). Until the engine has published a
        // figure the percentage is indeterminate, so a plain "thinking" message
        // is shown instead.
        const bool fStrategy = m_Game.IsComputingStrategy();
        const wchar_t* label = fStrategy ? L"Thinking" : L"Engine thinking";
        int nPercent = fStrategy ? m_Game.GetStrategyProgressPercent()
                                 : SearchProgressPercent();
        if (nPercent >= 0) {
            swprintf_s(buf, 256, L"%s%s  [%s... %d%%]", strPrefix.c_str(), turn,
                       label, nPercent);
        } else {
            swprintf_s(buf, 256, L"%s%s  [%s...]", strPrefix.c_str(), turn, label);
        }
    } else {
        swprintf_s(buf, 256, L"%s%s", strPrefix.c_str(), turn);
    }
    SendMessageW(m_hStatus, WM_SETTEXT, 0, (LPARAM)buf);
}

// ---------------------------------------------------------------------------
// MaybeAnnounceGameOver — show a one-time result dialog when the game ends
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::MaybeAnnounceGameOver() {
    if (m_fGameOverAnnounced) return;
    if (!m_Game.IsGameOver()) return;

    m_fGameOverAnnounced = true;

    // Make sure the final position (and result banner) is on screen before the
    // modal dialog appears.
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateWindow(m_hWnd);

    std::string strResult = m_Game.GetGameResultText();
    if (strResult.empty()) return;

    wchar_t wide[256]{};
    MultiByteToWideChar(CP_UTF8, 0, strResult.c_str(), -1, wide, 256);
    MessageBoxW(m_hWnd, wide, L"Game Over", MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// UpdatePlayerMenu / UpdatePauseMenu / SetPlayerModeAction / TogglePause
//
// All player-mode and pause UI now lives on the Options menu (the toolbar
// only carries view-related controls). The helpers below are the single
// source of truth for keeping those menu items in sync with engine state.
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::UpdatePlayerMenu() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) return;
    PlayerMode mode = m_Game.GetPlayerMode();
    int nIdCheck = (mode == PlayerMode::ZeroPlayers) ? IDM_PLAYERS_0
                 : (mode == PlayerMode::OnePlayer)   ? IDM_PLAYERS_1
                                                     : IDM_PLAYERS_2;
    CheckMenuRadioItem(hMenu, IDM_PLAYERS_FIRST, IDM_PLAYERS_LAST,
                       nIdCheck, MF_BYCOMMAND);
}

void CWinAmy4dWnd::UpdatePauseMenu() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) return;
    // Pause is meaningful only while the engine is actively thinking, or
    // while we have explicitly paused it (so the user can resume).
    bool bEnabled = m_Game.IsEngineRunning() || m_fPaused;
    EnableMenuItem(hMenu, IDM_PAUSE,
        MF_BYCOMMAND | (bEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    CheckMenuItem(hMenu, IDM_PAUSE,
        MF_BYCOMMAND | (m_fPaused ? MF_CHECKED : MF_UNCHECKED));
    UpdateUndoMenu();

    // Strategy can only be computed when the engine is idle and the game is
    // still in progress.
    CPosition* pStrategyPos = m_Game.GetPosition();
    bool fStrategyEnabled = !m_Game.IsEngineRunning()
                         && pStrategyPos != nullptr
                         && !m_Game.IsGameOver();
    EnableMenuItem(hMenu, IDM_STRATEGY,
        MF_BYCOMMAND | (fStrategyEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
}

// Undo is offered only in 1-player mode (human vs engine), when the engine is
// not thinking and there is at least one played move to take back.
void CWinAmy4dWnd::UpdateUndoMenu() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) return;
    CPosition* pPos = m_Game.GetPosition();
    bool fEnabled = m_Game.GetPlayerMode() == PlayerMode::OnePlayer
                 && !m_Game.IsEngineRunning()
                 && pPos != nullptr
                 && pPos->GetPly() > 0;
    EnableMenuItem(hMenu, IDM_UNDO,
        MF_BYCOMMAND | (fEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
}

// Take back the engine's reply and the human player's preceding move so the
// human may move again. Only valid in 1-player mode while the engine is idle.
void CWinAmy4dWnd::OnUndoMove() {
    if (m_Game.GetPlayerMode() != PlayerMode::OnePlayer) return;
    if (m_Game.IsEngineRunning()) return;

    if (!m_Game.UndoLastHumanMove()) return;

    m_fHaveSelection = false;
    m_fHaveHint = false;
    m_fStrategyHints = false;
    m_fGameOverAnnounced = false;
    m_HintSquares.clear();
    m_rgLegalDests.clear();
    RefreshLegalMoveHighlights();

    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hRender3D) InvalidateRect(m_hRender3D, nullptr, FALSE);
    UpdateStatusBar();
    UpdatePauseMenu();
    UpdateSuggestMoveButton();
}

void CWinAmy4dWnd::SetPlayerModeAction(PlayerMode mode) {
    m_Game.SetPlayerMode(mode);
    UpdatePlayerMenu();
    if (mode == PlayerMode::TwoPlayers) {
        m_Game.PauseEngine();
        m_fPaused = false;
        UpdatePauseMenu();
    } else {
        MaybeStartEngine();
        UpdatePauseMenu();
    }
}

void CWinAmy4dWnd::TogglePause() {
    if (m_Game.IsEngineRunning()) {
        m_fPaused = true;
        m_Game.PauseEngine();
        UpdatePauseMenu();
        UpdateStatusBar();
    } else if (m_fPaused) {
        m_fPaused = false;
        UpdatePauseMenu();
        MaybeStartEngine();
    }
}

// ---------------------------------------------------------------------------
// SetViewMode — toggle between 2D GDI rendering and 3D Direct3D 11 rendering
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::UpdateOutlinesMenuItem() {
    HMENU hMenu = GetMenu(m_hWnd);
    if (!hMenu) return;
    bool bOn = m_D3DRenderer.IsInitialized() ? m_D3DRenderer.GetShowOutlines() : true;
    CheckMenuItem(hMenu, IDM_VIEW_SHOW_GRIDLINES,
        MF_BYCOMMAND | (bOn ? MF_CHECKED : MF_UNCHECKED));
}

void CWinAmy4dWnd::UpdateViewToggleButton() {
    if (!m_hBtnViewToggle) return;
    SetWindowTextW(m_hBtnViewToggle,
        m_eViewMode == ViewMode::Mode2D ? L"Switch to 3D" : L"Switch to 2D");
}

void CWinAmy4dWnd::UpdateAxisControls() {
    // The axis-swap dropdown is a 2D-view control selecting which plane of the
    // 4D board the flat view renders. Keep its selection in sync with the
    // renderer's current view plane (indices: 0=x/y, 1=x/z, 2=y/z).
    if (m_hCbSwapAxes) {
        int nIndex = 0;
        switch (m_Renderer.GetViewPlane()) {
        case BoardRenderer::ViewPlane::PlaneXY: nIndex = 0; break;
        case BoardRenderer::ViewPlane::PlaneXZ: nIndex = 1; break;
        case BoardRenderer::ViewPlane::PlaneYZ: nIndex = 2; break;
        }
        SendMessageW(m_hCbSwapAxes, CB_SETCURSEL, (WPARAM)nIndex, 0);
    }
}

void CWinAmy4dWnd::SetViewMode(ViewMode mode) {
    if (mode == m_eViewMode) return;
    m_eViewMode = mode;

    HMENU hMenu = GetMenu(m_hWnd);
    CheckMenuItem(hMenu, IDM_VIEW_2D, MF_BYCOMMAND | (mode == ViewMode::Mode2D ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_3D, MF_BYCOMMAND | (mode == ViewMode::Mode3D ? MF_CHECKED : MF_UNCHECKED));

    if (mode == ViewMode::Mode3D) {
        // Position the child render window over the render area.
        SIZE sz = GetRenderAreaSize(m_hWnd);
        SetWindowPos(m_hRender3D, nullptr, 0, TOOLBAR_H, sz.cx, sz.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        // Lazy-create the D3D renderer the first time the user enters 3D.
        if (!m_D3DRenderer.IsInitialized()) {
            if (!m_D3DRenderer.Initialize(m_hRender3D)) {
                MessageBoxW(m_hWnd, L"Failed to initialise Direct3D 11. Reverting to 2D view.",
                            L"WinAmy 4D", MB_OK | MB_ICONERROR);
                m_eViewMode = ViewMode::Mode2D;
                CheckMenuItem(hMenu, IDM_VIEW_2D, MF_BYCOMMAND | MF_CHECKED);
                CheckMenuItem(hMenu, IDM_VIEW_3D, MF_BYCOMMAND | MF_UNCHECKED);
                UpdateViewToggleButton();
                UpdateGridMenuEnabled();
                return;
            }
        } else {
            m_D3DRenderer.Resize(sz.cx, sz.cy);
        }
        ShowWindow(m_hRender3D, SW_SHOW);
        ShowScrollBar(m_hWnd, SB_BOTH, FALSE);
        // 3D-only toolbar controls become visible; the 2D plane selector hides.
        ShowWindow(m_hCbGridType,  SW_SHOW);
        ShowWindow(m_hBtnZoomIn,   SW_SHOW);
        ShowWindow(m_hBtnZoomOut,  SW_SHOW);
        ShowWindow(m_hBtnRotateGrid, SW_SHOW);
        ShowWindow(m_hCbSwapAxes,  SW_HIDE);
        // Enable the 3D-only View menu items.
        EnableMenuItem(hMenu, IDM_VIEW_SHOW_GRIDLINES, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(hMenu, IDM_VIEW_RESET_VIEW,     MF_BYCOMMAND | MF_ENABLED);
        UpdateOutlinesMenuItem();
        UpdateAxisControls();
        // Reflect the renderer's actual grid type in the menu checkmark
        // and combobox selection (the renderer is the source of truth —
        // the menu and combobox are just UI).
        {
            CUCoord::EOutlineType eType = m_D3DRenderer.GetOutlineType();
            CheckMenuRadioItem(hMenu, IDM_GRID_FIRST, IDM_GRID_LAST,
                               MenuIdFromGridType(eType), MF_BYCOMMAND);
            if (m_hCbGridType) {
                int nIndex = static_cast<int>(eType)
                           - static_cast<int>(CUCoord::OT_full);
                SendMessageW(m_hCbGridType, CB_SETCURSEL, (WPARAM)nIndex, 0);
            }
        }
    } else {
        ShowWindow(m_hRender3D, SW_HIDE);
        ShowScrollBar(m_hWnd, SB_BOTH, TRUE);
        UpdateScrollBars(m_hWnd);
        // 3D-only toolbar controls hide; the 2D plane selector becomes visible.
        ShowWindow(m_hCbGridType,  SW_HIDE);
        ShowWindow(m_hBtnZoomIn,   SW_HIDE);
        ShowWindow(m_hBtnZoomOut,  SW_HIDE);
        ShowWindow(m_hBtnRotateGrid, SW_HIDE);
        ShowWindow(m_hCbSwapAxes,  SW_SHOW);
        // Disable the 3D-only View menu items.
        EnableMenuItem(hMenu, IDM_VIEW_SHOW_GRIDLINES,
            MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        EnableMenuItem(hMenu, IDM_VIEW_RESET_VIEW,
            MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
        UpdateAxisControls();
    }
    UpdateGridMenuEnabled();
    UpdateViewToggleButton();
    InvalidateRect(m_hWnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// SetDepthFromMenu — set the search depth via menu checkmark
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::SetDepthFromMenu(int nDepth) {
    m_Game.SetDepth(nDepth);

    HMENU hMenu  = GetMenu(m_hWnd);
    HMENU hOpts  = GetSubMenu(hMenu, 1);
    HMENU hDepth = GetSubMenu(hOpts, 0);

    // The Search Depth menu IDs are contiguous (IDM_DEPTH_1..IDM_DEPTH_9), so
    // check the selected depth and clear the rest by arithmetic on the base ID.
    for (int i = 0; i < 9; ++i) {
        CheckMenuItem(hDepth, IDM_DEPTH_1 + i, MF_BYCOMMAND | MF_UNCHECKED);
    }
    CheckMenuItem(hDepth, IDM_DEPTH_1 + (nDepth - 1), MF_BYCOMMAND | MF_CHECKED);
}

// ---------------------------------------------------------------------------
// Grid (cell outline) type menu — IDM_GRID_FIRST..IDM_GRID_LAST map 1:1 onto
// CUCoord::EOutlineType OT_full..OT_hex_4. CheckMenuRadioItem gives proper
// radio behaviour across the contiguous ID range.
// ---------------------------------------------------------------------------

CUCoord::EOutlineType CWinAmy4dWnd::GridTypeFromMenuId(int nMenuId) {
    int nOffset = nMenuId - IDM_GRID_FIRST;
    if (nOffset < 0) nOffset = 0;
    if (nOffset > (IDM_GRID_LAST - IDM_GRID_FIRST)) nOffset = (IDM_GRID_LAST - IDM_GRID_FIRST);
    return static_cast<CUCoord::EOutlineType>(
        static_cast<int>(CUCoord::OT_full) + nOffset);
}

int CWinAmy4dWnd::MenuIdFromGridType(CUCoord::EOutlineType eType) {
    return IDM_GRID_FIRST + (static_cast<int>(eType) - static_cast<int>(CUCoord::OT_full));
}

void CWinAmy4dWnd::SetGridType(CUCoord::EOutlineType eType) {
    if (m_D3DRenderer.IsInitialized()) {
        m_D3DRenderer.SetOutlineType(eType);
    } else {
        // Renderer not yet created — we still want subsequent UI to reflect
        // the chosen type. SetOutlineType is safe pre-init (it just caches).
        m_D3DRenderer.SetOutlineType(eType);
    }
    HMENU hMenu = GetMenu(m_hWnd);
    if (hMenu) {
        CheckMenuRadioItem(hMenu, IDM_GRID_FIRST, IDM_GRID_LAST,
                           MenuIdFromGridType(eType), MF_BYCOMMAND);
    }
    if (m_hCbGridType) {
        int nIndex = static_cast<int>(eType) - static_cast<int>(CUCoord::OT_full);
        // CB_SETCURSEL does not fire CBN_SELCHANGE, so this is safe to call
        // even when the change originated from the combobox itself.
        SendMessageW(m_hCbGridType, CB_SETCURSEL, (WPARAM)nIndex, 0);
    }
}

void CWinAmy4dWnd::SetGridTypeFromMenu(int nMenuId) {
    if (nMenuId < IDM_GRID_FIRST || nMenuId > IDM_GRID_LAST) return;
    SetGridType(GridTypeFromMenuId(nMenuId));
}

void CWinAmy4dWnd::UpdateGridMenuEnabled() {
    HMENU hMenu = GetMenu(m_hWnd);
    BOOL bEnabled = (m_eViewMode == ViewMode::Mode3D) ? TRUE : FALSE;
    if (hMenu) {
        UINT uState = bEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED);
        for (int nId = IDM_GRID_FIRST; nId <= IDM_GRID_LAST; ++nId) {
            EnableMenuItem(hMenu, nId, MF_BYCOMMAND | uState);
        }
    }
    if (m_hCbGridType) {
        EnableWindow(m_hCbGridType, bEnabled);
    }
}

// ---------------------------------------------------------------------------
// UpdateScrollBars — recompute scroll range from board and client sizes
// ---------------------------------------------------------------------------

void CWinAmy4dWnd::UpdateScrollBars(HWND hWnd) {
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int clientW = clientRect.right  - clientRect.left;
    int clientH = clientRect.bottom - clientRect.top - TOOLBAR_H - STATUSBAR_H;
    if (clientH < 0) clientH = 0;

    SIZE boardSz = BoardRenderer::GetBoardAreaSize();

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;

    // Horizontal
    si.nMin  = 0;
    si.nMax  = boardSz.cx;
    si.nPage = clientW;
    if (m_nScrollX > boardSz.cx - (int)si.nPage) {
        m_nScrollX = std::max(0, (int)boardSz.cx - (int)si.nPage);
    }
    si.nPos  = m_nScrollX;
    SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);

    // Vertical
    si.nMin  = 0;
    si.nMax  = boardSz.cy;
    si.nPage = (UINT)clientH;
    if (m_nScrollY > boardSz.cy - (int)si.nPage) {
        m_nScrollY = std::max(0, (int)boardSz.cy - (int)si.nPage);
    }
    si.nPos  = m_nScrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}

// ---------------------------------------------------------------------------
// Window Procedure
// ---------------------------------------------------------------------------

LRESULT CWinAmy4dWnd::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateControls(hWnd);
        SetDepthFromMenu(3);
        UpdateScrollBars(hWnd);
        UpdateGridMenuEnabled();
        return 0;

    case WM_SIZE: {
        if (m_hStatus) SendMessage(m_hStatus, WM_SIZE, 0, 0);
        UpdateScrollBars(hWnd);
        if (m_hRender3D) {
            SIZE sz = GetRenderAreaSize(hWnd);
            SetWindowPos(m_hRender3D, nullptr, 0, TOOLBAR_H, sz.cx, sz.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (m_eViewMode == ViewMode::Mode3D && m_D3DRenderer.IsInitialized()) {
                m_D3DRenderer.Resize(sz.cx, sz.cy);
            }
        }
        return 0;
    }

    case WM_HSCROLL: {
        if (m_eViewMode != ViewMode::Mode2D) return 0;
        SCROLLINFO si{ sizeof(si), SIF_ALL };
        GetScrollInfo(hWnd, SB_HORZ, &si);
        int oldX = m_nScrollX;
        switch (LOWORD(wParam)) {
        case SB_LINELEFT:      m_nScrollX -= BoardRenderer::SQUARE_SIZE; break;
        case SB_LINERIGHT:     m_nScrollX += BoardRenderer::SQUARE_SIZE; break;
        case SB_PAGELEFT:      m_nScrollX -= si.nPage; break;
        case SB_PAGERIGHT:     m_nScrollX += si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: m_nScrollX  = HIWORD(wParam); break;
        }
        m_nScrollX = std::max(0, std::min(m_nScrollX, si.nMax - (int)si.nPage));
        if (m_nScrollX != oldX) {
            SetScrollPos(hWnd, SB_HORZ, m_nScrollX, TRUE);
            // Confine the scroll to the board area (below the toolbar, above the
            // status bar) and erase the uncovered strip. DrawBoard only paints
            // the squares/labels, not the surrounding background, so without
            // SW_ERASE the freshly exposed margin pixels keep the scrolled-in
            // bits and the board looks fragmented.
            RECT rcScroll;
            GetClientRect(hWnd, &rcScroll);
            rcScroll.top    += TOOLBAR_H;
            rcScroll.bottom -= STATUSBAR_H;
            if (rcScroll.bottom < rcScroll.top) {
                rcScroll.bottom = rcScroll.top;
            }
            ScrollWindowEx(hWnd, oldX - m_nScrollX, 0,
                           &rcScroll, &rcScroll, nullptr, nullptr,
                           SW_INVALIDATE | SW_ERASE);
            UpdateWindow(hWnd);
        }
        return 0;
    }

    case WM_VSCROLL: {
        if (m_eViewMode != ViewMode::Mode2D) return 0;
        SCROLLINFO si{ sizeof(si), SIF_ALL };
        GetScrollInfo(hWnd, SB_VERT, &si);
        int oldY = m_nScrollY;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        m_nScrollY -= BoardRenderer::SQUARE_SIZE; break;
        case SB_LINEDOWN:      m_nScrollY += BoardRenderer::SQUARE_SIZE; break;
        case SB_PAGEUP:        m_nScrollY -= si.nPage; break;
        case SB_PAGEDOWN:      m_nScrollY += si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: m_nScrollY  = HIWORD(wParam); break;
        }
        m_nScrollY = std::max(0, std::min(m_nScrollY, si.nMax - (int)si.nPage));
        if (m_nScrollY != oldY) {
            SetScrollPos(hWnd, SB_VERT, m_nScrollY, TRUE);
            // See the WM_HSCROLL note: confine to the board area and erase the
            // uncovered strip so the background repaints correctly.
            RECT rcScroll;
            GetClientRect(hWnd, &rcScroll);
            rcScroll.top    += TOOLBAR_H;
            rcScroll.bottom -= STATUSBAR_H;
            if (rcScroll.bottom < rcScroll.top) {
                rcScroll.bottom = rcScroll.top;
            }
            ScrollWindowEx(hWnd, 0, oldY - m_nScrollY,
                           &rcScroll, &rcScroll, nullptr, nullptr,
                           SW_INVALIDATE | SW_ERASE);
            UpdateWindow(hWnd);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (m_eViewMode != ViewMode::Mode3D) {
            POINT ptOffset = Get2DBoardOffset(hWnd);
            // Offset by toolbar height and current scroll position.
            SetViewportOrgEx(hdc, ptOffset.x - m_nScrollX, TOOLBAR_H + ptOffset.y - m_nScrollY, nullptr);

            const CSCoord* sel = m_fHaveSelection ? &m_SelectedSquare : nullptr;
            std::vector<CSCoord> HintSquares = GetHintSquaresForRender();
            m_Renderer.DrawBoard(hdc, m_Game.GetPosition(), sel, m_rgLegalDests,
                                 HintSquares);

            SetViewportOrgEx(hdc, 0, 0, nullptr);

            // When the game is over, draw a centred result banner across the
            // top of the board so the outcome is always visible on the board.
            if (m_Game.IsGameOver()) {
                std::string strResult = m_Game.GetGameResultText();
                if (!strResult.empty()) {
                    wchar_t wide[256]{};
                    MultiByteToWideChar(CP_UTF8, 0, strResult.c_str(), -1, wide, 256);

                    RECT rcClient;
                    GetClientRect(hWnd, &rcClient);

                    HFONT hFont = CreateFontW(
                        -20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                    RECT rcText = rcClient;
                    rcText.top = TOOLBAR_H + 8;
                    rcText.bottom = rcText.top + 36;
                    DrawTextW(hdc, wide, -1, &rcText,
                              DT_CENTER | DT_CALCRECT | DT_SINGLELINE);

                    // Centre horizontally within the client area.
                    int nWidth = rcText.right - rcText.left;
                    int nCx = (rcClient.right - rcClient.left) / 2;
                    RECT rcBanner;
                    rcBanner.left = nCx - nWidth / 2 - 16;
                    rcBanner.right = nCx + nWidth / 2 + 16;
                    rcBanner.top = TOOLBAR_H + 6;
                    rcBanner.bottom = rcBanner.top + (rcText.bottom - rcText.top) + 8;

                    HBRUSH hBrush = CreateSolidBrush(RGB(32, 32, 32));
                    FillRect(hdc, &rcBanner, hBrush);
                    DeleteObject(hBrush);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 215, 0));
                    DrawTextW(hdc, wide, -1, &rcBanner,
                              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldFont);
                    DeleteObject(hFont);
                }
            }
        }
        // In 3D mode the child render window covers the render area and
        // owns the painting; nothing to do here.
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (m_eViewMode == ViewMode::Mode3D) return 0; // handled by child
        POINT ptOffset = Get2DBoardOffset(hWnd);
        // Adjust click coordinates for scroll offset and toolbar.
        POINT pt{
            GET_X_LPARAM(lParam) + m_nScrollX - ptOffset.x,
            GET_Y_LPARAM(lParam) - TOOLBAR_H + m_nScrollY - ptOffset.y
        };
        OnSquareClick(pt);
        return 0;
    }

    case WM_APP_ENGINE_MOVE:
        // Ignore a completion left over from a search that has since been
        // superseded (e.g. aborted because the user made a move).
        if (static_cast<uint32_t>(wParam) == m_Game.GetSearchGeneration())
            OnEngineMove(lParam);
        return 0;

    case WM_APP_ENGINE_HINT:
        if (static_cast<uint32_t>(wParam) == m_Game.GetSearchGeneration())
            OnEngineHint(lParam);
        return 0;

    case WM_APP_ENGINE_STRATEGY:
        if (static_cast<uint32_t>(wParam) == m_Game.GetSearchGeneration())
            OnEngineStrategy(lParam);
        return 0;

    case WM_APP_ENGINE_PROGRESS:
        // A strategy search advanced; refresh the status bar so the user sees
        // the updated "% complete" figure.
        UpdateStatusBar();
        return 0;

    case WM_TIMER:
        if (wParam == IDT_SEARCH_PROGRESS) {
            // Poll-driven refresh for the status-bar countdown. This drives both
            // single-move (engine / hint) searches and strategy computations,
            // whose remaining-time display has no engine event to push updates.
            // Stop polling once no search is running.
            if (m_Game.IsEngineRunning()) {
                UpdateStatusBar();
            } else {
                StopSearchProgressTimer();
            }
            return 0;
        }
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        switch (id) {
        case IDM_FILE_NEW:
            OnNewGame();
            break;

        case IDM_SUGGEST:
            OnSuggestMove();
            break;

        case IDM_FILE_LOAD_EPD:
            OnLoadEPDGame();
            break;

        case IDM_FILE_SAVE_EPD:
            OnSaveEPDGame();
            break;

        case IDM_FILE_LOAD_PGN:
            OnLoadPGNGame();
            break;

        case IDM_FILE_SAVE_PGN:
            OnSavePGNGame();
            break;

        case IDM_FILE_EXIT:
            DestroyWindow(hWnd);
            break;

        case IDM_VIEW_2D:
            SetViewMode(ViewMode::Mode2D);
            break;
        case IDM_VIEW_3D:
            SetViewMode(ViewMode::Mode3D);
            break;

        case IDC_BTN_VIEW_TOGGLE:
            SetViewMode(m_eViewMode == ViewMode::Mode2D
                            ? ViewMode::Mode3D : ViewMode::Mode2D);
            break;

        case IDM_VIEW_SHOW_GRIDLINES:
            if (m_D3DRenderer.IsInitialized()) {
                m_D3DRenderer.SetShowOutlines(!m_D3DRenderer.GetShowOutlines());
                UpdateOutlinesMenuItem();
            }
            break;

        case IDM_VIEW_RESET_VIEW:
            if (m_D3DRenderer.IsInitialized()) {
                m_D3DRenderer.ResetView();
            }
            break;

        case IDC_BTN_ZOOM_IN:
            if (m_D3DRenderer.IsInitialized()) m_D3DRenderer.AdjustZoom(0.85f);
            break;

        case IDC_BTN_ZOOM_OUT:
            if (m_D3DRenderer.IsInitialized()) m_D3DRenderer.AdjustZoom(1.18f);
            break;

        case IDC_BTN_ROTATE_GRID:
            if (m_D3DRenderer.IsInitialized()) {
                bool fRotate =
                    (SendMessageW(m_hBtnRotateGrid, BM_GETCHECK, 0, 0)
                     == BST_CHECKED);
                m_D3DRenderer.SetRotateGrid(fRotate);
            }
            break;

        case IDC_CB_GRID_TYPE:
            if (code == CBN_SELCHANGE) {
                int nSel = (int)SendMessageW(m_hCbGridType, CB_GETCURSEL, 0, 0);
                if (nSel != CB_ERR) {
                    SetGridType(static_cast<CUCoord::EOutlineType>(
                        static_cast<int>(CUCoord::OT_full) + nSel));
                }
            }
            break;

        case IDC_CB_SWAP_AXES:
            // 2D-view plane selector: choose which plane of the 4D board the
            // flat view renders. The swap is applied purely for rendering; all
            // board state stays in ordinary (unswapped) coordinates.
            if (code == CBN_SELCHANGE) {
                int nSel = (int)SendMessageW(m_hCbSwapAxes, CB_GETCURSEL, 0, 0);
                BoardRenderer::ViewPlane ePlane = BoardRenderer::ViewPlane::PlaneXY;
                switch (nSel) {
                case 0: ePlane = BoardRenderer::ViewPlane::PlaneXY; break; // x/y, no swap
                case 1: ePlane = BoardRenderer::ViewPlane::PlaneXZ; break; // x/z, swap Y/Z
                case 2: ePlane = BoardRenderer::ViewPlane::PlaneYZ; break; // y/z, swap X/Z
                }
                m_Renderer.SetViewPlane(ePlane);
                // The selected location is stored in canonical (unswapped)
                // board coordinates and matched for rendering by bit offset,
                // so it survives a plane change unchanged — keep the current
                // selection and its legal destinations instead of clearing.
                InvalidateRect(m_hWnd, nullptr, TRUE);
            }
            break;

        case IDM_DEPTH_1: case IDM_DEPTH_2: case IDM_DEPTH_3:
        case IDM_DEPTH_4: case IDM_DEPTH_5: case IDM_DEPTH_6:
        case IDM_DEPTH_7: case IDM_DEPTH_8: case IDM_DEPTH_9:
            SetDepthFromMenu(id - IDM_DEPTH_1 + 1);
            break;

        case IDM_GRID_FULL: case IDM_GRID_SQUARE_Z: case IDM_GRID_SQUARE_Y:
        case IDM_GRID_SQUARE_X: case IDM_GRID_HEX_1: case IDM_GRID_HEX_2:
        case IDM_GRID_HEX_3: case IDM_GRID_HEX_4:
            SetGridTypeFromMenu(id);
            break;

        case IDM_PLAYERS_0:
            SetPlayerModeAction(PlayerMode::ZeroPlayers);
            break;
        case IDM_PLAYERS_1:
            SetPlayerModeAction(PlayerMode::OnePlayer);
            break;
        case IDM_PLAYERS_2:
            SetPlayerModeAction(PlayerMode::TwoPlayers);
            break;

        case IDM_PAUSE:
            TogglePause();
            break;

        case IDM_UNDO:
            OnUndoMove();
            break;

        case IDM_STRATEGY:
            OnStrategy();
            break;

        case IDM_HIGHLIGHT_WHITE:
            SetLegalMoveHighlightSide(HighlightSide::White);
            break;

        case IDM_HIGHLIGHT_BLACK:
            SetLegalMoveHighlightSide(HighlightSide::Black);
            break;
        }
        return 0;
    }

    case WM_DESTROY:
        m_Game.PauseEngine();
        m_D3DRenderer.Shutdown();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
