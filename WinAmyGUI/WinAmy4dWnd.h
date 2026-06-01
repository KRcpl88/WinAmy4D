/*
    WinAmyGUI — WinAmy4dWnd.h

    CWinAmy4dWnd encapsulates the entire main-window state and behaviour of the
    WinAmy 4D chess GUI. It owns the window/control handles, the game and
    renderer subsystems, and the click-to-move / view interaction state that
    were previously file-scope globals in main.cpp.

    Win32 requires a free (static) window procedure; CWinAmy4dWnd routes those
    callbacks to instance methods via a thin static thunk that stashes the
    instance pointer in GWLP_USERDATA. WinMain owns a single CWinAmy4dWnd
    instance and calls Run().
*/

#pragma once

#include <windows.h>

#include <vector>

#include "GameController.h"
#include "BoardRenderer.h"
#include "D3DBoardRenderer.h"

#include "scoord.h"
#include "ucoord.h"

class CWinAmy4dWnd {
public:
    enum class ViewMode { Mode2D, Mode3D };

    CWinAmy4dWnd();
    ~CWinAmy4dWnd();

    // Register window classes, create the main window, and run the message
    // loop. Returns the WM_QUIT exit code.
    int Run(HINSTANCE hInstance, int nCmdShow);

private:
    // ---- Win32 window-procedure plumbing -------------------------------
    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg,
                                          WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK StaticRender3DProc(HWND hWnd, UINT uMsg,
                                               WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT Render3DProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // ---- Setup ---------------------------------------------------------
    void CreateControls(HWND hWnd);

    // ---- Game / move helpers -------------------------------------------
    bool IsHumanAllowedToMove(const CPosition* pPos) const;
    void CollectLegalDestinationsForSquare(const CPosition* pPos,
                                           const CSCoord& From,
                                           std::vector<CSCoord>& rgDests);
    bool TryMakeSelectedMove(const CPosition* pPos, const CSCoord& sqTo);

    // ---- Layout helpers ------------------------------------------------
    SIZE  GetRenderAreaSize(HWND hWnd) const;
    POINT Get2DBoardOffset(HWND hWnd) const;

    // ---- Command / event handlers --------------------------------------
    void OnNewGame();
    void OnLoadEPDGame();
    void OnSaveEPDGame();
    void OnEngineMove(LPARAM lParam);
    void OnEngineHint(LPARAM lParam);
    void OnSuggestMove();
    void ClearHint();
    void OnSquareClick(POINT pt);
    void OnSquareClick3D(const CSCoord& sq);
    void MaybeStartEngine();
    void UpdateStatusBar();
    void MaybeAnnounceGameOver();
    void UpdatePlayerMenu();
    void UpdatePauseMenu();
    void UpdateUndoMenu();
    void OnUndoMove();
    void SetPlayerModeAction(PlayerMode mode);
    void TogglePause();
    void SetDepthFromMenu(int nDepth);
    void SetViewMode(ViewMode mode);
    void UpdateViewToggleButton();
    void UpdateOutlinesButtonText();
    void UpdateAxisControls();
    void SetGridType(CUCoord::EOutlineType eType);
    void SetGridTypeFromMenu(int nMenuId);
    void UpdateGridMenuEnabled();
    void UpdateScrollBars(HWND hWnd);

    // ---- Pure mapping helpers (no instance state) ----------------------
    static CUCoord::EOutlineType GridTypeFromMenuId(int nMenuId);
    static int MenuIdFromGridType(CUCoord::EOutlineType eType);

    // ---- Window / control handles (group A) ----------------------------
    HWND m_hWnd          = nullptr;
    HWND m_hRender3D     = nullptr; // Child window the D3D swap chain renders into.
    HWND m_hStatus       = nullptr;
    HWND m_hBtnNew       = nullptr;
    HWND m_hBtnHint      = nullptr;
    HWND m_hBtnViewToggle = nullptr;
    HWND m_hCbGridType   = nullptr;
    HWND m_hBtnOutlines  = nullptr;
    HWND m_hBtnResetView = nullptr;
    HWND m_hBtnZoomIn    = nullptr;
    HWND m_hBtnZoomOut   = nullptr;
    HWND m_hChkInvertX   = nullptr;
    HWND m_hChkInvertY   = nullptr;
    HWND m_hChkInvertZ   = nullptr;
    HWND m_hCbSwapAxes   = nullptr;

    // ---- Owned subsystems (group B) ------------------------------------
    GameController   m_Game;
    BoardRenderer    m_Renderer;
    D3DBoardRenderer m_D3DRenderer;

    // ---- Interaction / UI state (group C) ------------------------------
    ViewMode m_eViewMode = ViewMode::Mode2D;

    // Click-to-move state.
    bool                m_fHaveSelection = false;
    CSCoord             m_SelectedSquare;
    std::vector<CSCoord> m_rgLegalDests;

    // Engine move-suggestion (hint) state. When m_fHaveHint is set, the
    // suggested move's from-square (m_HintFrom) and to-square (m_HintTo) are
    // highlighted as a recommendation; the engine does NOT make the move.
    bool    m_fHaveHint = false;
    CSCoord m_HintFrom;
    CSCoord m_HintTo;

    // Scroll state (pixels scrolled from origin).
    int m_nScrollX = 0;
    int m_nScrollY = 0;

    // Self-play pause state.
    bool m_fPaused = false;

    // Tracks whether the end-of-game result has already been announced (so the
    // MessageBox is shown only once per game). Reset on each new game.
    bool m_fGameOverAnnounced = false;
};
