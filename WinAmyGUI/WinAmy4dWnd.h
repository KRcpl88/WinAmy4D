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
#include <string>

#include "GameController.h"
#include "BoardRenderer.h"
#include "D3DBoardRenderer.h"

#include "scoord.h"
#include "ucoord.h"

class CWinAmy4dWnd {
public:
    enum class ViewMode { Mode2D, Mode3D };
    enum class HighlightSide { None, White, Black };

    CWinAmy4dWnd();
    ~CWinAmy4dWnd();

    // Register window classes, create the main window, and run the message
    // loop. Returns the WM_QUIT exit code. lpCmdLine is the (program-name
    // excluded) command line, used to enable optional file logging.
    int Run(HINSTANCE hInstance, int nCmdShow, LPSTR lpCmdLine);

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
    void CollectLegalMoveHighlightsForSide(const CPosition* pPos,
                                           HighlightSide eSide,
                                           std::vector<CSCoord>& Squares);
    static void AppendMoveHighlightSquares(std::vector<CSCoord>& Squares,
                                           const CMove& mv);
    bool TryMakeSelectedMove(const CPosition* pPos, const CSCoord& sqTo);

    // ---- Layout helpers ------------------------------------------------
    SIZE  GetRenderAreaSize(HWND hWnd) const;
    POINT Get2DBoardOffset(HWND hWnd) const;

    // ---- Command / event handlers --------------------------------------
    void OnNewGame();
    void OnLoadEPDGame();
    void OnSaveEPDGame();
    void OnLoadPGNGame();
    void OnSavePGNGame();
    void OnEngineMove(LPARAM lParam);
    void OnEngineHint(LPARAM lParam);
    void OnSuggestMove();
    void OnStrategy();
    void OnEngineStrategy(LPARAM lParam);
    void ClearHint();
    void RefreshLegalMoveHighlights();
    void SetLegalMoveHighlightSide(HighlightSide eSide);
    void UpdateLegalMoveHighlightMenu();
    void UpdateSuggestMoveButton();
    std::vector<CSCoord> GetHintSquaresForRender() const;
    // ---- Look feature helpers ------------------------------------------
    // Toggle look mode from the button's checkbox state, updating control
    // visibility and repainting.
    void OnToggleLookMode();
    // Position the Look button and its piece-type dropdown so their left edge
    // starts at nBaseX (the dropdown follows the button).
    void PositionLookControls(int nBaseX);
    // Compute every square the selected piece type attacks from the chosen
    // look location on the current board (empty if look mode is inactive).
    std::vector<CSCoord> ComputeLookAttackSquares() const;
    // The square highlighted as "selected" for rendering: the look location
    // while look mode is active, otherwise the click-to-move selection.
    const CSCoord* SelectionForRender() const;
    void OnSquareClick(POINT pt);
    void OnSquareClick3D(const CSCoord& sq);
    void MaybeStartEngine();
    void UpdateStatusBar();
    int  SearchProgressPercent() const;
    void StartSearchProgressTimer();
    void StopSearchProgressTimer();
    void MaybeAnnounceGameOver();
    void UpdatePlayerMenu();
    void UpdatePauseMenu();
    void UpdateUndoMenu();
    void OnUndoMove();
    void OnEnterMove();
    static INT_PTR CALLBACK EnterMoveDlgProc(HWND hDlg, UINT uMsg,
                                             WPARAM wParam, LPARAM lParam);
    void SetPlayerModeAction(PlayerMode mode);
    void TogglePause();
    void SetDepthFromMenu(int nDepth);
    void SetViewMode(ViewMode mode);
    void UpdateViewToggleButton();
    void UpdateOutlinesMenuItem();
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
    HWND m_hBtnViewToggle = nullptr;
    HWND m_hBtnEnterMove  = nullptr;
    HWND m_hCbGridType   = nullptr;
    HWND m_hBtnZoomIn    = nullptr;
    HWND m_hBtnZoomOut    = nullptr;
    HWND m_hBtnRotateGrid = nullptr;
    HWND m_hCbSwapAxes   = nullptr;

    // "Look" feature controls. m_hBtnLook is a push-like checkbox that toggles
    // look mode; m_hCbLookPiece selects which piece type's attacks to preview
    // and is shown only while look mode is active.
    HWND m_hBtnLook      = nullptr;
    HWND m_hCbLookPiece  = nullptr;

    // Shared toolbar x-origin for the grid-type (3D) and swap-axes (2D)
    // dropdowns so they occupy the same slot when the view mode changes.
    int  m_nDropdownX    = 0;

    // Toolbar x-origins for the Look button in each view mode. In 2D the Look
    // controls sit just right of the plane selector; in 3D they move to the far
    // right so the 3D-only controls have room. SetViewMode repositions them.
    int  m_nLookX2D      = 0;
    int  m_nLookX3D      = 0;

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
    // suggested move's from-square and to-square are highlighted as a
    // recommendation; the engine does NOT make the move.
    bool                 m_fHaveHint = false;
    bool                 m_fStrategyHints = false;
    std::vector<CSCoord> m_HintSquares;

    // Optional menu-driven highlight of all legal moves for a side.
    HighlightSide        m_eHighlightSide = HighlightSide::None;
    std::vector<CSCoord> m_LegalMoveHintSquares;

    // "Look" mode state. When m_fLookMode is set, the board highlights every
    // square the selected piece type (m_nLookPiece, a Piece enum value) would
    // attack from m_LookSquare. The piece type and location are retained even
    // while look mode is off so they are restored when it is re-enabled. The
    // location may only be picked in 2D mode (an empty 3D square cannot be
    // clicked accurately), but the highlight is shown in both views.
    bool                 m_fLookMode = false;
    bool                 m_fLookHaveSquare = false;
    CSCoord              m_LookSquare;
    int                  m_nLookPiece = Queen;

    // Scroll state (pixels scrolled from origin).
    int m_nScrollX = 0;
    int m_nScrollY = 0;

    // Self-play pause state.
    bool m_fPaused = false;

    // Tracks whether the end-of-game result has already been announced (so the
    // MessageBox is shown only once per game). Reset on each new game.
    bool m_fGameOverAnnounced = false;

    // Base status-bar text (last-move prefix + side-to-move + material) captured
    // the last time a move completed, i.e. while no search was running. While a
    // search is in progress the progress-timer refresh appends only the
    // "thinking %" suffix to this cached text, so it never has to clone the
    // board or hold m_PositionMutex on every tick (see UpdateStatusBar).
    std::wstring m_wstrStatusBase;
};
