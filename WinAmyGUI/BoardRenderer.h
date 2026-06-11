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

    // Extra horizontal gutter inserted on the left and right of the center
    // board (level h) so the green axis labels have room to draw outside the
    // squares without obscuring any pieces.
    static constexpr int AXIS_LABEL_MARGIN = 30;

    // Gap (in pixels) between a board edge and the rank/file coordinate labels
    // drawn beside it on the XY plane view.
    static constexpr int COORD_LABEL_GAP = 4;

    // Row layout:
    //   Row 0 (top):    j(6) k(5) l(4) m(3) n(2) o(1)   — levels 9–14
    //   Row 1 (middle): g(7) h(8) i(7)                   — levels 6–8
    //   Row 2 (bottom): a(1) b(2) c(3) d(4) e(5) f(6)    — levels 0–5
    static constexpr int NUM_ROWS = 3;

    // Colours for even-indexed levels (warm brown).
    static constexpr COLORREF CLR_LIGHT      = RGB(200, 170, 150);
    static constexpr COLORREF CLR_DARK       = RGB(120, 100,  90);

    // Colours for odd-indexed levels (cool blue-slate).
    static constexpr COLORREF CLR_LIGHT_ALT  = RGB( 150, 170, 200);
    static constexpr COLORREF CLR_DARK_ALT   = RGB( 90, 100, 120);

    // Highlight and label colours.
    static constexpr COLORREF CLR_SELECTED   = RGB( 20, 180,  20);
    static constexpr COLORREF CLR_LEGAL_MOVE = RGB(120, 200, 120);
    // Cyan highlight for an engine move suggestion (both the piece to move and
    // the recommended destination).
    static constexpr COLORREF CLR_HINT       = RGB(  0, 220, 220);
    static constexpr COLORREF CLR_LABEL_BG   = RGB( 50,  50,  50);
    static constexpr COLORREF CLR_LABEL_FG   = RGB(230, 230, 230);
    static constexpr COLORREF CLR_BORDER     = RGB( 80,  80,  80);
    // Bright green used for the +x / +y / +z axis labels.
    static constexpr COLORREF CLR_AXIS_LABEL = RGB(  0, 220,   0);
    // Dark grey used for the rank/file coordinate labels drawn over the (light)
    // window background on the XY plane view.
    static constexpr COLORREF CLR_COORD_LABEL = RGB( 60,  60,  60);

    BoardRenderer();
    ~BoardRenderer();

    // Draw the entire board onto the given HDC. HintSquares marks engine move
    // suggestions or other recommendation highlights in cyan.
    void DrawBoard(HDC hdc, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests,
                   const std::vector<CSCoord>& HintSquares) const;

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
    // Bold, slightly larger font used only for the +x / +y / +z axis labels.
    HFONT m_hAxisFont{nullptr};

    // Returns the pixel origin (top-left of the grid area, below the label)
    // for level index 0–14. Also sets *outBoardW and *outBoardH if non-null.
    static POINT LevelOrigin(int level);

    // Draw a single level.
    void DrawLevel(HDC hdc, int level, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests,
                   const std::vector<CSCoord>& HintSquares) const;

    // Draw the green +x / +y / +z axis labels next to their anchor squares
    // (ha8, hh8, oa1). The labels follow the active view plane so they always
    // sit beside the true board square that defines each axis direction.
    void DrawAxisLabels(HDC hdc) const;

    // Draw the rank (left) and file (bottom) coordinate labels for one level.
    // Only meaningful on the XY plane view, where each rendered level is a true
    // board level and the file/rank letters/digits are correct. Rank labels are
    // baseline-aligned with the bottom of each rank square so it is unambiguous
    // which rank each digit belongs to.
    void DrawRankFileLabels(HDC hdc, int level) const;

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
