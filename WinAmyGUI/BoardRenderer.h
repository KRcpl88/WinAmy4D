#pragma once

#include <windows.h>

#include "dbase.h"
#include "bitboard.h"
#include "scoord.h"
#include "ucoord.h"
#include "move.h"

#include <vector>

class BoardRenderer {
public:
    // Which plane of the 4D board the flat 2D view is showing. The default
    // (PlaneXY) is the natural board with no axis swap; the other two planes
    // are produced by swapping a pair of lattice axes (see SetViewPlane).
    //
    //   PlaneXY — x/y plane: no swap (each rendered level is a true board level a–o)
    //   PlaneXZ — x/z plane: swap lattice Y/Z
    //   PlaneYZ — y/z plane: swap lattice X/Z
    //
    // In the swapped planes the rendered "levels" no longer correspond to the
    // board levels a–o, so their labels are hidden.
    enum class ViewPlane { PlaneXY, PlaneXZ, PlaneYZ };

    // Square size in pixels (all levels share the same cell size).
    static constexpr int SQUARE_SIZE = 36;

    // Margin around and between board levels.
    static constexpr int BOARD_MARGIN = 12;

    // Height of the level label above each board.
    static constexpr int LABEL_HEIGHT = 16;

    // Row layout:
    //   Row 0 (top):    j(6) k(5) l(4) m(3) n(2) o(1)   — levels 9–14
    //   Row 1 (middle): g(7) h(8) i(7)                   — levels 6–8
    //   Row 2 (bottom): a(1) b(2) c(3) d(4) e(5) f(6)    — levels 0–5
    static constexpr int NUM_ROWS = 3;

    // Colours for even-indexed levels (warm brown).
    static constexpr COLORREF CLR_LIGHT      = RGB(120, 109,  91);
    static constexpr COLORREF CLR_DARK       = RGB(181, 136,  99);

    // Colours for odd-indexed levels (cool blue-slate).
    static constexpr COLORREF CLR_LIGHT_ALT  = RGB( 98, 108, 118);
    static constexpr COLORREF CLR_DARK_ALT   = RGB( 90, 130, 170);

    // Highlight and label colours.
    static constexpr COLORREF CLR_SELECTED   = RGB( 20, 180,  20);
    static constexpr COLORREF CLR_LEGAL_MOVE = RGB(120, 200, 120);
    // Cyan highlight for an engine move suggestion (both the piece to move and
    // the recommended destination).
    static constexpr COLORREF CLR_HINT       = RGB(  0, 220, 220);
    static constexpr COLORREF CLR_LABEL_BG   = RGB( 50,  50,  50);
    static constexpr COLORREF CLR_LABEL_FG   = RGB(230, 230, 230);
    static constexpr COLORREF CLR_BORDER     = RGB( 80,  80,  80);

    BoardRenderer();
    ~BoardRenderer();

    // Draw the entire board onto the given HDC. HintFrom/HintTo, when non-null
    // and valid, mark an engine move suggestion and are highlighted in cyan.
    void DrawBoard(HDC hdc, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests,
                   const CSCoord* HintFrom = nullptr,
                   const CSCoord* HintTo = nullptr) const;

    // Return the board square under the given client-area pixel, or an
    // invalid coord if no square is there. In a swapped view plane the
    // returned coordinate is the *original* board square (with a valid bit
    // offset), so all click / move logic continues to work in board space.
    CSCoord HitTest(POINT pt) const;

    // Select which plane of the 4D board the flat 2D view renders. Changing
    // the plane only affects how cells are laid out / which original square
    // is drawn in each slot; the engine and all board state stay in ordinary
    // (unswapped) board coordinates.
    void SetViewPlane(ViewPlane eViewPlane) { m_eViewPlane = eViewPlane; }
    ViewPlane GetViewPlane() const { return m_eViewPlane; }

    // Return the total width and height required for the board area.
    static SIZE GetBoardAreaSize();

private:
    HFONT m_hPieceFont{nullptr};
    HFONT m_hLabelFont{nullptr};

    // Returns the pixel origin (top-left of the grid area, below the label)
    // for level index 0–14. Also sets *outBoardW and *outBoardH if non-null.
    static POINT LevelOrigin(int level);

    // Draw a single level.
    void DrawLevel(HDC hdc, int level, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests,
                   const CSCoord* HintFrom,
                   const CSCoord* HintTo) const;

    // Return the Unicode chess piece glyph for the given piece value.
    static wchar_t PieceGlyph(int8_t piece);

    // Map a swapped 2D-render location (a cell slot in the currently rendered
    // grid) back to the original board square whose piece / highlight state
    // should be displayed there. For PlaneXY this is the identity. The input
    // is a CSCoordBase precisely because, in a swapped plane, that location's
    // bit offset is meaningless and must never be used; only the returned
    // original CSCoord carries a valid bit offset.
    CSCoord MapRenderToOriginal(const CSCoordBase& SwappedRenderCoord) const;

    // Currently selected view plane (defaults to the natural x/y board).
    ViewPlane m_eViewPlane{ViewPlane::PlaneXY};
};
