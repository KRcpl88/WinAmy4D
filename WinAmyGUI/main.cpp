/*
    WinAmyGUI — main.cpp
    WinMain entry point, window class registration, message loop, and
    main window procedure for the WinAmy 4D chess GUI.
*/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <wchar.h>
#include <cstring>

#include "resource.h"
#include "GameController.h"
#include "BoardRenderer.h"

#include "dbase.h"
#include "heap.h"
#include "move.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static const wchar_t* APP_CLASS = L"WinAmyGUI_Window";
static const wchar_t* APP_TITLE = L"WinAmy 4D Chess";

// Toolbar / control layout
static constexpr int TOOLBAR_H   = 36;   // pixel height of the button panel
static constexpr int STATUSBAR_H = 22;
static constexpr int BTN_W       = 100;
static constexpr int BTN_H       = 28;
static constexpr int BTN_Y       = (TOOLBAR_H - BTN_H) / 2;
static constexpr int BTN_GAP     = 6;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static HWND            g_hWnd       = nullptr;
static HWND            g_hStatus    = nullptr;
static HWND            g_hBtnNew    = nullptr;
static HWND            g_hBtn0P     = nullptr;
static HWND            g_hBtn1P     = nullptr;
static HWND            g_hBtn2P     = nullptr;
static HWND            g_hBtnPause  = nullptr;
static HWND            g_hEditDepth = nullptr;
static HWND            g_hSpinDepth = nullptr;

static GameController  g_Game;
static BoardRenderer   g_Renderer;

// Click-to-move state.
static bool            g_fHaveSelection = false;
static CSCoord         g_SelectedSquare;
static std::vector<CSCoord> g_LegalDests;

// Scroll state (pixels scrolled from origin).
static int             g_scrollX = 0;
static int             g_scrollY = 0;

// Self-play pause state.
static bool            g_fPaused = false;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void OnNewGame();
static void OnEngineMove(LPARAM lParam);
static void OnSquareClick(POINT pt);
static void MaybeStartEngine();
static void UpdateStatusBar();
static void UpdatePlayerButtons();
static void SetDepthFromMenu(int depth);
static void CreateControls(HWND hWnd);
static void UpdateScrollBars(HWND hWnd);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ---------------------------------------------------------------------------
// WinMain
// ---------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Initialise common controls (needed for spin/status bar).
    INITCOMMONCONTROLSEX icce{sizeof(icce), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icce);

    // Initialise chess engine.
    GameController::InitEngine();

    // Register window class.
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.lpszClassName = APP_CLASS;
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    SIZE boardSz = BoardRenderer::GetBoardAreaSize();
    int winW = boardSz.cx + 16;
    int winH = boardSz.cy + TOOLBAR_H + STATUSBAR_H + 60;

    g_hWnd = CreateWindowExW(
        0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

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

static void CreateControls(HWND hWnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
    int x = BTN_GAP;

    auto makeBtn = [&](const wchar_t* label, int id, int w = BTN_W) -> HWND {
        HWND h = CreateWindowExW(0, L"BUTTON", label,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, BTN_Y, w, BTN_H, hWnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        x += w + BTN_GAP;
        return h;
    };

    g_hBtnNew   = makeBtn(L"New Game", IDC_BTN_NEW_GAME);
    x += BTN_GAP * 2; // spacer
    g_hBtn0P    = makeBtn(L"0 Players", IDC_BTN_0_PLAYERS, 90);
    g_hBtn1P    = makeBtn(L"1 Player",  IDC_BTN_1_PLAYER,  80);
    g_hBtn2P    = makeBtn(L"2 Players", IDC_BTN_2_PLAYERS, 90);
    x += BTN_GAP * 2;
    g_hBtnPause = makeBtn(L"Pause",     IDC_BTN_PAUSE, 70);
    x += BTN_GAP * 2;

    // Depth label + edit + spin
    CreateWindowExW(0, L"STATIC", L"Depth:",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        x, BTN_Y, 44, BTN_H, hWnd, nullptr, hInst, nullptr);
    x += 44 + 2;

    g_hEditDepth = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"3",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        x, BTN_Y, 30, BTN_H, hWnd, (HMENU)(INT_PTR)IDC_EDIT_DEPTH, hInst, nullptr);

    g_hSpinDepth = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
        0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_SPIN_DEPTH, hInst, nullptr);
    SendMessage(g_hSpinDepth, UDM_SETBUDDY,  (WPARAM)g_hEditDepth, 0);
    SendMessage(g_hSpinDepth, UDM_SETRANGE,  0, MAKELPARAM(9, 1));
    SendMessage(g_hSpinDepth, UDM_SETPOS,    0, 3);

    // Status bar
    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);

    // Set initial button states
    UpdatePlayerButtons();
    EnableWindow(g_hBtnPause, FALSE);
}

// ---------------------------------------------------------------------------
// OnNewGame
// ---------------------------------------------------------------------------

static void OnNewGame() {
    g_Game.PauseEngine();
    g_fPaused = false;
    g_fHaveSelection = false;
    g_LegalDests.clear();
    g_Game.NewGame();
    g_Game.SetDepth((int)SendMessage(g_hSpinDepth, UDM_GETPOS, 0, 0));
    EnableWindow(g_hBtnPause, FALSE);
    SetWindowTextW(g_hBtnPause, L"Pause");
    InvalidateRect(g_hWnd, nullptr, TRUE);
    UpdateStatusBar();
    MaybeStartEngine();
}

// ---------------------------------------------------------------------------
// MaybeStartEngine — trigger engine search if it is the engine's turn
// ---------------------------------------------------------------------------

static void MaybeStartEngine() {
    if (g_Game.IsGameOver()) return;
    if (g_Game.IsEngineRunning()) return;
    if (g_fPaused) return;

    const CPosition* pos = g_Game.GetPosition();
    if (!pos) return;

    PlayerMode mode = g_Game.GetPlayerMode();
    if (mode == PlayerMode::TwoPlayers) return;

    bool engineTurn = false;
    if (mode == PlayerMode::ZeroPlayers) {
        engineTurn = true;
    } else if (mode == PlayerMode::OnePlayer) {
        // Engine plays Black (turn 1).
        engineTurn = (pos->m_nTurn == 1);
    }

    if (engineTurn) {
        SetWindowTextW(g_hBtnPause, L"Pause");
        EnableWindow(g_hBtnPause, TRUE);
        UpdateStatusBar();
        g_Game.StartEngineSearch(g_hWnd);
    }
}

// ---------------------------------------------------------------------------
// OnEngineMove — handle WM_APP_ENGINE_MOVE
// ---------------------------------------------------------------------------

static void OnEngineMove(LPARAM /*lParam*/) {
    EnableWindow(g_hBtnPause, FALSE);

    CMove move = g_Game.GetBestMove();
    if (move == M_NONE) {
        UpdateStatusBar();
        return;
    }

    g_Game.MakeMove(move);
    g_fHaveSelection = false;
    g_LegalDests.clear();

    InvalidateRect(g_hWnd, nullptr, TRUE);
    UpdateStatusBar();

    if (g_Game.IsGameOver()) return;

    // For self-play, immediately start the engine again (other side).
    if (g_Game.GetPlayerMode() == PlayerMode::ZeroPlayers && !g_fPaused) {
        EnableWindow(g_hBtnPause, TRUE);
        g_Game.StartEngineSearch(g_hWnd);
    } else {
        MaybeStartEngine();
    }
}

// ---------------------------------------------------------------------------
// OnSquareClick — handle a left-click on the board area
// ---------------------------------------------------------------------------

static void OnSquareClick(POINT pt) {
    if (g_Game.IsEngineRunning()) return;
    if (g_Game.IsGameOver()) return;
    if (g_Game.GetPlayerMode() == PlayerMode::ZeroPlayers) return;

    const CPosition* pos = g_Game.GetPosition();
    if (!pos) return;

    // In 1-player mode, human plays White (turn 0).
    if (g_Game.GetPlayerMode() == PlayerMode::OnePlayer && pos->m_nTurn == 1) return;

    // pt is already in board coordinates (scroll + toolbar applied by caller).
    CSCoord sq = g_Renderer.HitTest(pt);

    if (!sq.IsValid()) {
        g_fHaveSelection = false;
        g_LegalDests.clear();
        InvalidateRect(g_hWnd, nullptr, TRUE);
        return;
    }

    if (!g_fHaveSelection) {
        // Select a piece if it belongs to the current player.
        uint16_t off = sq.BitOffset();
        int8_t piece = pos->m_rgPiece[off];
        bool isWhitePiece = (piece > 0);
        bool isWhiteTurn  = (pos->m_nTurn == 0);
        if (piece == 0 || isWhitePiece != isWhiteTurn) return;

        g_fHaveSelection = true;
        g_SelectedSquare = sq;

        // Generate legal moves and collect destinations from this square.
        g_LegalDests.clear();
        heap_t heap = allocate_heap();
        push_section(heap);
        const_cast<CPosition*>(pos)->GenFrom(sq, heap);
        for (unsigned i = heap->current_section->start;
             i < heap->current_section->end; ++i) {
            CMove mv = heap->data[i];
            if (const_cast<CPosition*>(pos)->LegalMove(mv)) {
                CSCoord dest = mv.GetToCoord();
                g_LegalDests.push_back(dest);
            }
        }
        free_heap(heap);

        InvalidateRect(g_hWnd, nullptr, TRUE);
    } else {
        // Attempt to make a move to the clicked destination.
        bool madeMove = false;
        for (const auto& dest : g_LegalDests) {
            if (dest.BitOffset() == sq.BitOffset()) {
                // Re-generate moves from selected square and pick the matching one.
                heap_t heap = allocate_heap();
                push_section(heap);
                const_cast<CPosition*>(pos)->GenFrom(g_SelectedSquare, heap);
                for (unsigned i = heap->current_section->start;
                     i < heap->current_section->end; ++i) {
                    CMove mv = heap->data[i];
                    if (mv.GetToCoord().BitOffset() == sq.BitOffset()
                        && const_cast<CPosition*>(pos)->LegalMove(mv)) {
                        g_Game.MakeMove(mv);
                        madeMove = true;
                        break;
                    }
                }
                free_heap(heap);
                break;
            }
        }

        g_fHaveSelection = false;
        g_LegalDests.clear();
        InvalidateRect(g_hWnd, nullptr, TRUE);

        if (madeMove) {
            UpdateStatusBar();
            if (!g_Game.IsGameOver())
                MaybeStartEngine();
        }
    }
}

// ---------------------------------------------------------------------------
// UpdateStatusBar
// ---------------------------------------------------------------------------

static void UpdateStatusBar() {
    if (!g_hStatus) return;

    const char* gameEnd = g_Game.GetGameEndMessage();
    if (gameEnd) {
        // Convert narrow to wide
        wchar_t wide[128]{};
        MultiByteToWideChar(CP_ACP, 0, gameEnd, -1, wide, 128);
        SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)wide);
        return;
    }

    const CPosition* pos = g_Game.GetPosition();
    if (!pos) return;

    wchar_t buf[128];
    const wchar_t* turn = (pos->m_nTurn == 0) ? L"White to move" : L"Black to move";
    if (g_Game.IsEngineRunning()) {
        swprintf_s(buf, 128, L"%s  [Engine thinking...]", turn);
    } else {
        swprintf_s(buf, 128, L"%s", turn);
    }
    SendMessageW(g_hStatus, WM_SETTEXT, 0, (LPARAM)buf);
}

// ---------------------------------------------------------------------------
// UpdatePlayerButtons — update visual state of 0/1/2 player buttons
// ---------------------------------------------------------------------------

static void UpdatePlayerButtons() {
    PlayerMode mode = g_Game.GetPlayerMode();
    // We simulate "checked" state by enabling/disabling differently
    // A simple approach: bold text via WM_SETFONT is complex; just change button text.
    SetWindowTextW(g_hBtn0P, mode == PlayerMode::ZeroPlayers ? L"[0 Players]" : L"0 Players");
    SetWindowTextW(g_hBtn1P, mode == PlayerMode::OnePlayer   ? L"[1 Player]"  : L"1 Player");
    SetWindowTextW(g_hBtn2P, mode == PlayerMode::TwoPlayers  ? L"[2 Players]" : L"2 Players");
}

// ---------------------------------------------------------------------------
// SetDepthFromMenu — set depth via menu checkmark
// ---------------------------------------------------------------------------

static void SetDepthFromMenu(int depth) {
    g_Game.SetDepth(depth);
    SendMessage(g_hSpinDepth, UDM_SETPOS, 0, depth);

    HMENU hMenu  = GetMenu(g_hWnd);
    HMENU hOpts  = GetSubMenu(hMenu, 1);
    HMENU hDepth = GetSubMenu(hOpts, 0);
    for (int i = 0; i < 9; ++i)
        CheckMenuItem(hDepth, IDM_DEPTH_1 + i, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(hDepth, IDM_DEPTH_1 + (depth - 1), MF_BYCOMMAND | MF_CHECKED);
}

// ---------------------------------------------------------------------------
// UpdateScrollBars — recompute scroll range from board and client sizes
// ---------------------------------------------------------------------------

static void UpdateScrollBars(HWND hWnd) {
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
    if (g_scrollX > boardSz.cx - (int)si.nPage) g_scrollX = max(0, boardSz.cx - (int)si.nPage);
    si.nPos  = g_scrollX;
    SetScrollInfo(hWnd, SB_HORZ, &si, TRUE);

    // Vertical
    si.nMin  = 0;
    si.nMax  = boardSz.cy;
    si.nPage = (UINT)clientH;
    if (g_scrollY > boardSz.cy - (int)si.nPage) g_scrollY = max(0, boardSz.cy - (int)si.nPage);
    si.nPos  = g_scrollY;
    SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
}

// ---------------------------------------------------------------------------
// Window Procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        CreateControls(hWnd);
        SetDepthFromMenu(3);
        UpdateScrollBars(hWnd);
        return 0;

    case WM_SIZE: {
        if (g_hStatus) SendMessage(g_hStatus, WM_SIZE, 0, 0);
        UpdateScrollBars(hWnd);
        return 0;
    }

    case WM_HSCROLL: {
        SCROLLINFO si{ sizeof(si), SIF_ALL };
        GetScrollInfo(hWnd, SB_HORZ, &si);
        int oldX = g_scrollX;
        switch (LOWORD(wParam)) {
        case SB_LINELEFT:      g_scrollX -= BoardRenderer::SQUARE_SIZE; break;
        case SB_LINERIGHT:     g_scrollX += BoardRenderer::SQUARE_SIZE; break;
        case SB_PAGELEFT:      g_scrollX -= si.nPage; break;
        case SB_PAGERIGHT:     g_scrollX += si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: g_scrollX  = HIWORD(wParam); break;
        }
        g_scrollX = max(0, min(g_scrollX, si.nMax - (int)si.nPage));
        if (g_scrollX != oldX) {
            SetScrollPos(hWnd, SB_HORZ, g_scrollX, TRUE);
            ScrollWindowEx(hWnd, oldX - g_scrollX, 0,
                           nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE);
            UpdateWindow(hWnd);
        }
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si{ sizeof(si), SIF_ALL };
        GetScrollInfo(hWnd, SB_VERT, &si);
        int oldY = g_scrollY;
        switch (LOWORD(wParam)) {
        case SB_LINEUP:        g_scrollY -= BoardRenderer::SQUARE_SIZE; break;
        case SB_LINEDOWN:      g_scrollY += BoardRenderer::SQUARE_SIZE; break;
        case SB_PAGEUP:        g_scrollY -= si.nPage; break;
        case SB_PAGEDOWN:      g_scrollY += si.nPage; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: g_scrollY  = HIWORD(wParam); break;
        }
        g_scrollY = max(0, min(g_scrollY, si.nMax - (int)si.nPage));
        if (g_scrollY != oldY) {
            SetScrollPos(hWnd, SB_VERT, g_scrollY, TRUE);
            ScrollWindowEx(hWnd, 0, oldY - g_scrollY,
                           nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE);
            UpdateWindow(hWnd);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Offset by toolbar height and current scroll position.
        SetViewportOrgEx(hdc, -g_scrollX, TOOLBAR_H - g_scrollY, nullptr);

        const CSCoord* sel = g_fHaveSelection ? &g_SelectedSquare : nullptr;
        g_Renderer.DrawBoard(hdc, g_Game.GetPosition(), sel, g_LegalDests);

        SetViewportOrgEx(hdc, 0, 0, nullptr);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        // Adjust click coordinates for scroll offset and toolbar.
        POINT pt{
            GET_X_LPARAM(lParam) + g_scrollX,
            GET_Y_LPARAM(lParam) - TOOLBAR_H + g_scrollY
        };
        OnSquareClick(pt);
        return 0;
    }

    case WM_APP_ENGINE_MOVE:
        OnEngineMove(lParam);
        return 0;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case IDM_FILE_NEW:
        case IDC_BTN_NEW_GAME:
            OnNewGame();
            break;

        case IDM_FILE_EXIT:
            DestroyWindow(hWnd);
            break;

        case IDM_DEPTH_1: case IDM_DEPTH_2: case IDM_DEPTH_3:
        case IDM_DEPTH_4: case IDM_DEPTH_5: case IDM_DEPTH_6:
        case IDM_DEPTH_7: case IDM_DEPTH_8: case IDM_DEPTH_9:
            SetDepthFromMenu(id - IDM_DEPTH_1 + 1);
            break;

        case IDC_BTN_0_PLAYERS:
            g_Game.SetPlayerMode(PlayerMode::ZeroPlayers);
            UpdatePlayerButtons();
            MaybeStartEngine();
            break;

        case IDC_BTN_1_PLAYER:
            g_Game.SetPlayerMode(PlayerMode::OnePlayer);
            UpdatePlayerButtons();
            MaybeStartEngine();
            break;

        case IDC_BTN_2_PLAYERS:
            g_Game.SetPlayerMode(PlayerMode::TwoPlayers);
            g_Game.PauseEngine();
            UpdatePlayerButtons();
            EnableWindow(g_hBtnPause, FALSE);
            break;

        case IDC_BTN_PAUSE:
            if (g_Game.IsEngineRunning()) {
                g_fPaused = true;
                g_Game.PauseEngine();
                SetWindowTextW(g_hBtnPause, L"Resume");
            } else if (g_fPaused) {
                g_fPaused = false;
                SetWindowTextW(g_hBtnPause, L"Pause");
                MaybeStartEngine();
            }
            break;

        case IDC_EDIT_DEPTH: {
            if (HIWORD(wParam) == EN_CHANGE) {
                int d = (int)SendMessage(g_hSpinDepth, UDM_GETPOS, 0, 0);
                if (d >= 1 && d <= 9) {
                    g_Game.SetDepth(d);
                    // Update menu checkmarks.
                    HMENU hMenu  = GetMenu(hWnd);
                    HMENU hOpts  = GetSubMenu(hMenu, 1);
                    HMENU hDepth = GetSubMenu(hOpts, 0);
                    for (int i = 0; i < 9; ++i)
                        CheckMenuItem(hDepth, IDM_DEPTH_1 + i, MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(hDepth, IDM_DEPTH_1 + (d - 1), MF_BYCOMMAND | MF_CHECKED);
                }
            }
            break;
        }
        }
        return 0;
    }

    case WM_DESTROY:
        g_Game.PauseEngine();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
