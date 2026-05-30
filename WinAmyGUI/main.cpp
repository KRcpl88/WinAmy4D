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
#include "D3DBoardRenderer.h"

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

enum class ViewMode { Mode2D, Mode3D };

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

static HWND            g_hWnd       = nullptr;
static HWND            g_hRender3D  = nullptr; // Child window the D3D swap chain renders into.
static HWND            g_hStatus    = nullptr;
static HWND            g_hBtnNew    = nullptr;
static HWND            g_hBtnViewToggle = nullptr;
static HWND            g_hCbGridType    = nullptr;
static HWND            g_hBtnOutlines  = nullptr;
static HWND            g_hBtnResetView = nullptr;
static HWND            g_hBtnZoomIn    = nullptr;
static HWND            g_hBtnZoomOut   = nullptr;

static GameController  g_Game;
static BoardRenderer   g_Renderer;
static D3DBoardRenderer g_D3DRenderer;
static ViewMode        g_eViewMode  = ViewMode::Mode2D;

static const wchar_t* RENDER_CLASS = L"WinAmyGUI_Render3D";

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
static void UpdatePlayerMenu();
static void UpdatePauseMenu();
static void SetPlayerModeAction(PlayerMode mode);
static void TogglePause();
static void SetDepthFromMenu(int depth);
static void SetViewMode(ViewMode mode);
static void UpdateViewToggleButton();
static void SetGridType(CUCoord::EOutlineType eType);
static void SetGridTypeFromMenu(int nMenuId);
static void UpdateGridMenuEnabled();
static int MenuIdFromGridType(CUCoord::EOutlineType eType);
static void CreateControls(HWND hWnd);
static void UpdateScrollBars(HWND hWnd);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Returns the size of the renderable client area (below the toolbar,
// above the status bar). Used by the D3D renderer to size its swap chain.
static SIZE GetRenderAreaSize(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top - TOOLBAR_H - STATUSBAR_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return SIZE{ w, h };
}

// Handles a click on a CSCoord that came from the 3D pick. Mirrors the
// selection/move logic in OnSquareClick but skips the 2D hit-test.
static void OnSquareClick3D(const CSCoord& sq) {
    if (g_Game.IsEngineRunning() || g_Game.IsGameOver()
        || g_Game.GetPlayerMode() == PlayerMode::ZeroPlayers) return;
    const CPosition* pos = g_Game.GetPosition();
    if (!pos) return;
    if (g_Game.GetPlayerMode() == PlayerMode::OnePlayer && pos->m_nTurn == 1) return;

    if (!g_fHaveSelection) {
        uint16_t off = sq.BitOffset();
        int8_t piece = pos->m_rgPiece[off];
        bool isWhitePiece = (piece > 0);
        bool isWhiteTurn  = (pos->m_nTurn == 0);
        if (piece == 0 || isWhitePiece != isWhiteTurn) return;
        g_fHaveSelection = true;
        g_SelectedSquare = sq;
        g_LegalDests.clear();
        heap_t heap = allocate_heap();
        push_section(heap);
        const_cast<CPosition*>(pos)->LegalMoves(heap);
        for (unsigned i = heap->current_section->start;
             i < heap->current_section->end; ++i) {
            CMove mv = heap->data[i];
            if (mv.GetFromCoord() == sq) {
                g_LegalDests.push_back(mv.GetToCoord());
            }
        }
        free_heap(heap);
        InvalidateRect(g_hRender3D ? g_hRender3D : g_hWnd, nullptr, FALSE);
    } else {
        bool madeMove = false;
        for (const auto& dest : g_LegalDests) {
            if (dest == sq) {
                heap_t heap = allocate_heap();
                push_section(heap);
                const_cast<CPosition*>(pos)->LegalMoves(heap);
                for (unsigned i = heap->current_section->start;
                     i < heap->current_section->end; ++i) {
                    CMove mv = heap->data[i];
                    if (mv.GetFromCoord() == g_SelectedSquare && mv.GetToCoord() == sq) {
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
        InvalidateRect(g_hRender3D ? g_hRender3D : g_hWnd, nullptr, FALSE);
        if (madeMove) {
            UpdateStatusBar();
            if (!g_Game.IsGameOver()) MaybeStartEngine();
        }
    }
}

// Render3D child window proc — handles mouse for orbit/zoom and paint via D3D.
static LRESULT CALLBACK Render3DProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1; // suppress flicker; the swap chain owns the surface
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        if (g_D3DRenderer.IsInitialized()) {
            const CSCoord* sel = g_fHaveSelection ? &g_SelectedSquare : nullptr;
            g_D3DRenderer.Render(g_Game.GetPosition(), sel, g_LegalDests);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (g_D3DRenderer.IsInitialized())
            g_D3DRenderer.OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        if (g_D3DRenderer.IsInitialized())
            g_D3DRenderer.OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
        if (g_D3DRenderer.IsInitialized()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            g_D3DRenderer.OnMouseUp(x, y);
            if (g_D3DRenderer.LastInteractionWasClick()) {
                CSCoord sq = g_D3DRenderer.HitTest3D(x, y);
                if (sq.IsValid()) OnSquareClick3D(sq);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (g_D3DRenderer.IsInitialized())
            g_D3DRenderer.OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

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

    // Register the child render window class used for 3D mode.
    WNDCLASSEXW rwc{};
    rwc.cbSize        = sizeof(rwc);
    rwc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    rwc.lpfnWndProc   = Render3DProc;
    rwc.hInstance     = hInstance;
    rwc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    rwc.hbrBackground = nullptr; // we own the surface via the swap chain
    rwc.lpszClassName = RENDER_CLASS;
    RegisterClassExW(&rwc);

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

    g_hBtnNew = makeBtn(L"New Game", IDC_BTN_NEW_GAME);
    x += BTN_GAP * 2; // spacer

    // 2D/3D view toggle — always enabled regardless of current view mode.
    // Label is kept in sync with g_eViewMode by UpdateViewToggleButton.
    g_hBtnViewToggle = makeBtn(L"Switch to 3D", IDC_BTN_VIEW_TOGGLE, 110);

    // Grid-type dropdown — only enabled in 3D mode. The list contents map
    // 1:1 onto CUCoord::EOutlineType in declaration order, so combobox
    // index N == (EOutlineType)(OT_full + N).
    {
        int nCbH = 220; // includes dropdown extent (8 items + decorations).
        g_hCbGridType = CreateWindowExW(0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                | CBS_DROPDOWNLIST,
            x, BTN_Y, 160, nCbH, hWnd,
            (HMENU)(INT_PTR)IDC_CB_GRID_TYPE, hInst, nullptr);
        x += 160 + BTN_GAP;
        static const wchar_t* kGridLabels[] = {
            L"Full Dodecahedron",
            L"Square Z Slice",
            L"Square Y Slice",
            L"Square X Slice",
            L"Hex 1 (-X,-Y,-Z)",
            L"Hex 2 (-X,-Y,+Z)",
            L"Hex 3 (-X,+Y,-Z)",
            L"Hex 4 (+X,-Y,-Z)",
        };
        for (auto* psz : kGridLabels) {
            SendMessageW(g_hCbGridType, CB_ADDSTRING, 0, (LPARAM)psz);
        }
        int nInitial = static_cast<int>(g_D3DRenderer.GetOutlineType())
                     - static_cast<int>(CUCoord::OT_full);
        SendMessageW(g_hCbGridType, CB_SETCURSEL, (WPARAM)nInitial, 0);
        EnableWindow(g_hCbGridType, FALSE); // 2D by default.
    }

    x += BTN_GAP * 2;

    // 3D-mode controls. Search depth is configured exclusively via the
    // Options > Search Depth menu; there are no toolbar depth controls.
    g_hBtnOutlines  = makeBtn(L"Outlines: On", IDC_BTN_OUTLINES,   90);
    g_hBtnResetView = makeBtn(L"Reset View",   IDC_BTN_RESET_VIEW, 80);
    g_hBtnZoomIn    = makeBtn(L"Zoom +",       IDC_BTN_ZOOM_IN,    60);
    g_hBtnZoomOut   = makeBtn(L"Zoom -",       IDC_BTN_ZOOM_OUT,   60);
    EnableWindow(g_hBtnOutlines,  FALSE);
    EnableWindow(g_hBtnResetView, FALSE);
    EnableWindow(g_hBtnZoomIn,    FALSE);
    EnableWindow(g_hBtnZoomOut,   FALSE);

    // Status bar
    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);

    // 3D render child window — covers the area between the toolbar and the
    // status bar. Initially hidden; SetViewMode toggles visibility.
    g_hRender3D = CreateWindowExW(0, RENDER_CLASS, nullptr,
        WS_CHILD,
        0, TOOLBAR_H, 10, 10,
        hWnd, nullptr, hInst, nullptr);

    // Set initial menu / button states.
    UpdatePlayerMenu();
    UpdatePauseMenu();
    UpdateViewToggleButton();
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
    // Preserve the depth previously selected via the Options menu.
    UpdatePauseMenu();
    InvalidateRect(g_hWnd, nullptr, TRUE);
    if (g_hRender3D) InvalidateRect(g_hRender3D, nullptr, FALSE);
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
        UpdateStatusBar();
        g_Game.StartEngineSearch(g_hWnd);
        UpdatePauseMenu();
    }
}

// ---------------------------------------------------------------------------
// OnEngineMove — handle WM_APP_ENGINE_MOVE
// ---------------------------------------------------------------------------

static void OnEngineMove(LPARAM /*lParam*/) {
    CMove move = g_Game.GetBestMove();
    if (move == M_NONE) {
        UpdatePauseMenu();
        UpdateStatusBar();
        return;
    }

    g_Game.MakeMove(move);
    g_fHaveSelection = false;
    g_LegalDests.clear();

    InvalidateRect(g_hWnd, nullptr, TRUE);
    if (g_hRender3D) InvalidateRect(g_hRender3D, nullptr, FALSE);
    UpdateStatusBar();

    if (g_Game.IsGameOver()) {
        UpdatePauseMenu();
        return;
    }

    // For self-play, immediately start the engine again (other side).
    if (g_Game.GetPlayerMode() == PlayerMode::ZeroPlayers && !g_fPaused) {
        g_Game.StartEngineSearch(g_hWnd);
        UpdatePauseMenu();
    } else {
        MaybeStartEngine();
        UpdatePauseMenu();
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

        // Generate all strictly legal moves, then collect destinations from this square.
        g_LegalDests.clear();
        heap_t heap = allocate_heap();
        push_section(heap);
        const_cast<CPosition*>(pos)->LegalMoves(heap);
        for (unsigned i = heap->current_section->start;
             i < heap->current_section->end; ++i) {
            CMove mv = heap->data[i];
            if (mv.GetFromCoord() == sq) {
                g_LegalDests.push_back(mv.GetToCoord());
            }
        }
        free_heap(heap);

        InvalidateRect(g_hWnd, nullptr, TRUE);
    } else {
        // Attempt to make a move to the clicked destination.
        bool madeMove = false;
        for (const auto& dest : g_LegalDests) {
            if (dest == sq) {
                // Re-generate legal moves and pick the first legal move to this square.
                heap_t heap = allocate_heap();
                push_section(heap);
                const_cast<CPosition*>(pos)->LegalMoves(heap);
                for (unsigned i = heap->current_section->start;
                     i < heap->current_section->end; ++i) {
                    CMove mv = heap->data[i];
                    if (mv.GetFromCoord() == g_SelectedSquare
                            && mv.GetToCoord() == sq) {
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
// UpdatePlayerMenu / UpdatePauseMenu / SetPlayerModeAction / TogglePause
//
// All player-mode and pause UI now lives on the Options menu (the toolbar
// only carries view-related controls). The helpers below are the single
// source of truth for keeping those menu items in sync with engine state.
// ---------------------------------------------------------------------------

static void UpdatePlayerMenu() {
    HMENU hMenu = GetMenu(g_hWnd);
    if (!hMenu) return;
    PlayerMode mode = g_Game.GetPlayerMode();
    int nIdCheck = (mode == PlayerMode::ZeroPlayers) ? IDM_PLAYERS_0
                 : (mode == PlayerMode::OnePlayer)   ? IDM_PLAYERS_1
                                                     : IDM_PLAYERS_2;
    CheckMenuRadioItem(hMenu, IDM_PLAYERS_FIRST, IDM_PLAYERS_LAST,
                       nIdCheck, MF_BYCOMMAND);
}

static void UpdatePauseMenu() {
    HMENU hMenu = GetMenu(g_hWnd);
    if (!hMenu) return;
    // Pause is meaningful only while the engine is actively thinking, or
    // while we have explicitly paused it (so the user can resume).
    bool bEnabled = g_Game.IsEngineRunning() || g_fPaused;
    EnableMenuItem(hMenu, IDM_PAUSE,
        MF_BYCOMMAND | (bEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED)));
    CheckMenuItem(hMenu, IDM_PAUSE,
        MF_BYCOMMAND | (g_fPaused ? MF_CHECKED : MF_UNCHECKED));
}

static void SetPlayerModeAction(PlayerMode mode) {
    g_Game.SetPlayerMode(mode);
    UpdatePlayerMenu();
    if (mode == PlayerMode::TwoPlayers) {
        g_Game.PauseEngine();
        g_fPaused = false;
        UpdatePauseMenu();
    } else {
        MaybeStartEngine();
        UpdatePauseMenu();
    }
}

static void TogglePause() {
    if (g_Game.IsEngineRunning()) {
        g_fPaused = true;
        g_Game.PauseEngine();
        UpdatePauseMenu();
        UpdateStatusBar();
    } else if (g_fPaused) {
        g_fPaused = false;
        UpdatePauseMenu();
        MaybeStartEngine();
    }
}

// ---------------------------------------------------------------------------
// SetViewMode — toggle between 2D GDI rendering and 3D Direct3D 11 rendering
// ---------------------------------------------------------------------------

static void UpdateOutlinesButtonText() {
    if (!g_hBtnOutlines) return;
    bool bOn = g_D3DRenderer.IsInitialized() ? g_D3DRenderer.GetShowOutlines() : true;
    SetWindowTextW(g_hBtnOutlines, bOn ? L"Outlines: On" : L"Outlines: Off");
}

static void UpdateViewToggleButton() {
    if (!g_hBtnViewToggle) return;
    SetWindowTextW(g_hBtnViewToggle,
        g_eViewMode == ViewMode::Mode2D ? L"Switch to 3D" : L"Switch to 2D");
}

static void SetViewMode(ViewMode mode) {
    if (mode == g_eViewMode) return;
    g_eViewMode = mode;

    HMENU hMenu = GetMenu(g_hWnd);
    CheckMenuItem(hMenu, IDM_VIEW_2D, MF_BYCOMMAND | (mode == ViewMode::Mode2D ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDM_VIEW_3D, MF_BYCOMMAND | (mode == ViewMode::Mode3D ? MF_CHECKED : MF_UNCHECKED));

    if (mode == ViewMode::Mode3D) {
        // Position the child render window over the render area.
        SIZE sz = GetRenderAreaSize(g_hWnd);
        SetWindowPos(g_hRender3D, nullptr, 0, TOOLBAR_H, sz.cx, sz.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        // Lazy-create the D3D renderer the first time the user enters 3D.
        if (!g_D3DRenderer.IsInitialized()) {
            if (!g_D3DRenderer.Initialize(g_hRender3D)) {
                MessageBoxW(g_hWnd, L"Failed to initialise Direct3D 11. Reverting to 2D view.",
                            L"WinAmy 4D", MB_OK | MB_ICONERROR);
                g_eViewMode = ViewMode::Mode2D;
                CheckMenuItem(hMenu, IDM_VIEW_2D, MF_BYCOMMAND | MF_CHECKED);
                CheckMenuItem(hMenu, IDM_VIEW_3D, MF_BYCOMMAND | MF_UNCHECKED);
                UpdateViewToggleButton();
                UpdateGridMenuEnabled();
                return;
            }
        } else {
            g_D3DRenderer.Resize(sz.cx, sz.cy);
        }
        ShowWindow(g_hRender3D, SW_SHOW);
        ShowScrollBar(g_hWnd, SB_BOTH, FALSE);
        EnableWindow(g_hBtnOutlines,  TRUE);
        EnableWindow(g_hBtnResetView, TRUE);
        EnableWindow(g_hBtnZoomIn,    TRUE);
        EnableWindow(g_hBtnZoomOut,   TRUE);
        UpdateOutlinesButtonText();
        // Reflect the renderer's actual grid type in the menu checkmark
        // and combobox selection (the renderer is the source of truth —
        // the menu and combobox are just UI).
        {
            CUCoord::EOutlineType eType = g_D3DRenderer.GetOutlineType();
            CheckMenuRadioItem(hMenu, IDM_GRID_FIRST, IDM_GRID_LAST,
                               MenuIdFromGridType(eType), MF_BYCOMMAND);
            if (g_hCbGridType) {
                int nIndex = static_cast<int>(eType)
                           - static_cast<int>(CUCoord::OT_full);
                SendMessageW(g_hCbGridType, CB_SETCURSEL, (WPARAM)nIndex, 0);
            }
        }
    } else {
        ShowWindow(g_hRender3D, SW_HIDE);
        ShowScrollBar(g_hWnd, SB_BOTH, TRUE);
        UpdateScrollBars(g_hWnd);
        EnableWindow(g_hBtnOutlines,  FALSE);
        EnableWindow(g_hBtnResetView, FALSE);
        EnableWindow(g_hBtnZoomIn,    FALSE);
        EnableWindow(g_hBtnZoomOut,   FALSE);
    }
    UpdateGridMenuEnabled();
    UpdateViewToggleButton();
    InvalidateRect(g_hWnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// SetDepthFromMenu — set depth via menu checkmark
// ---------------------------------------------------------------------------

static void SetDepthFromMenu(int depth) {
    g_Game.SetDepth(depth);

    HMENU hMenu  = GetMenu(g_hWnd);
    HMENU hOpts  = GetSubMenu(hMenu, 1);
    HMENU hDepth = GetSubMenu(hOpts, 0);
    for (int i = 0; i < 9; ++i)
        CheckMenuItem(hDepth, IDM_DEPTH_1 + i, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(hDepth, IDM_DEPTH_1 + (depth - 1), MF_BYCOMMAND | MF_CHECKED);
}

// ---------------------------------------------------------------------------
// Grid (cell outline) type menu — IDM_GRID_FIRST..IDM_GRID_LAST map 1:1 onto
// CUCoord::EOutlineType OT_full..OT_hex_4. CheckMenuRadioItem gives proper
// radio behaviour across the contiguous ID range.
// ---------------------------------------------------------------------------

static CUCoord::EOutlineType GridTypeFromMenuId(int nMenuId) {
    int nOffset = nMenuId - IDM_GRID_FIRST;
    if (nOffset < 0) nOffset = 0;
    if (nOffset > (IDM_GRID_LAST - IDM_GRID_FIRST)) nOffset = (IDM_GRID_LAST - IDM_GRID_FIRST);
    return static_cast<CUCoord::EOutlineType>(
        static_cast<int>(CUCoord::OT_full) + nOffset);
}

static int MenuIdFromGridType(CUCoord::EOutlineType eType) {
    return IDM_GRID_FIRST + (static_cast<int>(eType) - static_cast<int>(CUCoord::OT_full));
}

static void SetGridType(CUCoord::EOutlineType eType) {
    if (g_D3DRenderer.IsInitialized()) {
        g_D3DRenderer.SetOutlineType(eType);
    } else {
        // Renderer not yet created — we still want subsequent UI to reflect
        // the chosen type. SetOutlineType is safe pre-init (it just caches).
        g_D3DRenderer.SetOutlineType(eType);
    }
    HMENU hMenu = GetMenu(g_hWnd);
    if (hMenu) {
        CheckMenuRadioItem(hMenu, IDM_GRID_FIRST, IDM_GRID_LAST,
                           MenuIdFromGridType(eType), MF_BYCOMMAND);
    }
    if (g_hCbGridType) {
        int nIndex = static_cast<int>(eType) - static_cast<int>(CUCoord::OT_full);
        // CB_SETCURSEL does not fire CBN_SELCHANGE, so this is safe to call
        // even when the change originated from the combobox itself.
        SendMessageW(g_hCbGridType, CB_SETCURSEL, (WPARAM)nIndex, 0);
    }
}

static void SetGridTypeFromMenu(int nMenuId) {
    if (nMenuId < IDM_GRID_FIRST || nMenuId > IDM_GRID_LAST) return;
    SetGridType(GridTypeFromMenuId(nMenuId));
}

static void UpdateGridMenuEnabled() {
    HMENU hMenu = GetMenu(g_hWnd);
    BOOL bEnabled = (g_eViewMode == ViewMode::Mode3D) ? TRUE : FALSE;
    if (hMenu) {
        UINT uState = bEnabled ? MF_ENABLED : (MF_GRAYED | MF_DISABLED);
        for (int nId = IDM_GRID_FIRST; nId <= IDM_GRID_LAST; ++nId) {
            EnableMenuItem(hMenu, nId, MF_BYCOMMAND | uState);
        }
    }
    if (g_hCbGridType) {
        EnableWindow(g_hCbGridType, bEnabled);
    }
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
        UpdateGridMenuEnabled();
        return 0;

    case WM_SIZE: {
        if (g_hStatus) SendMessage(g_hStatus, WM_SIZE, 0, 0);
        UpdateScrollBars(hWnd);
        if (g_hRender3D) {
            SIZE sz = GetRenderAreaSize(hWnd);
            SetWindowPos(g_hRender3D, nullptr, 0, TOOLBAR_H, sz.cx, sz.cy,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (g_eViewMode == ViewMode::Mode3D && g_D3DRenderer.IsInitialized()) {
                g_D3DRenderer.Resize(sz.cx, sz.cy);
            }
        }
        return 0;
    }

    case WM_HSCROLL: {
        if (g_eViewMode != ViewMode::Mode2D) return 0;
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
        if (g_eViewMode != ViewMode::Mode2D) return 0;
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

        if (g_eViewMode != ViewMode::Mode3D) {
            // Offset by toolbar height and current scroll position.
            SetViewportOrgEx(hdc, -g_scrollX, TOOLBAR_H - g_scrollY, nullptr);

            const CSCoord* sel = g_fHaveSelection ? &g_SelectedSquare : nullptr;
            g_Renderer.DrawBoard(hdc, g_Game.GetPosition(), sel, g_LegalDests);

            SetViewportOrgEx(hdc, 0, 0, nullptr);
        }
        // In 3D mode the child render window covers the render area and
        // owns the painting; nothing to do here.
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (g_eViewMode == ViewMode::Mode3D) return 0; // handled by child
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
        int code = HIWORD(wParam);
        switch (id) {
        case IDM_FILE_NEW:
        case IDC_BTN_NEW_GAME:
            OnNewGame();
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
            SetViewMode(g_eViewMode == ViewMode::Mode2D
                            ? ViewMode::Mode3D : ViewMode::Mode2D);
            break;

        case IDC_BTN_OUTLINES:
            if (g_D3DRenderer.IsInitialized()) {
                g_D3DRenderer.SetShowOutlines(!g_D3DRenderer.GetShowOutlines());
                UpdateOutlinesButtonText();
            }
            break;

        case IDC_BTN_RESET_VIEW:
            if (g_D3DRenderer.IsInitialized()) g_D3DRenderer.ResetView();
            break;

        case IDC_BTN_ZOOM_IN:
            if (g_D3DRenderer.IsInitialized()) g_D3DRenderer.AdjustZoom(0.85f);
            break;

        case IDC_BTN_ZOOM_OUT:
            if (g_D3DRenderer.IsInitialized()) g_D3DRenderer.AdjustZoom(1.18f);
            break;

        case IDC_CB_GRID_TYPE:
            if (code == CBN_SELCHANGE) {
                int nSel = (int)SendMessageW(g_hCbGridType, CB_GETCURSEL, 0, 0);
                if (nSel != CB_ERR) {
                    SetGridType(static_cast<CUCoord::EOutlineType>(
                        static_cast<int>(CUCoord::OT_full) + nSel));
                }
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
        }
        return 0;
    }

    case WM_DESTROY:
        g_Game.PauseEngine();
        g_D3DRenderer.Shutdown();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
