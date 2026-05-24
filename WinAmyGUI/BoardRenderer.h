#pragma once

#include <windows.h>

#include "dbase.h"
#include "bitboard.h"
#include "scoord.h"
#include "move.h"

#include <vector>

class BoardRenderer {
public:
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
    static constexpr COLORREF CLR_LIGHT      = RGB(240, 217, 181);
    static constexpr COLORREF CLR_DARK       = RGB(181, 136,  99);

    // Colours for odd-indexed levels (cool blue-slate).
    static constexpr COLORREF CLR_LIGHT_ALT  = RGB(195, 215, 235);
    static constexpr COLORREF CLR_DARK_ALT   = RGB( 90, 130, 170);

    // Highlight and label colours.
    static constexpr COLORREF CLR_SELECTED   = RGB( 20, 180,  20);
    static constexpr COLORREF CLR_LEGAL_MOVE = RGB(120, 200, 120);
    static constexpr COLORREF CLR_LABEL_BG   = RGB( 50,  50,  50);
    static constexpr COLORREF CLR_LABEL_FG   = RGB(230, 230, 230);
    static constexpr COLORREF CLR_BORDER     = RGB( 80,  80,  80);

    BoardRenderer();
    ~BoardRenderer();

    // Draw the entire board onto the given HDC.
    void DrawBoard(HDC hdc, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests) const;

    // Return the board square under the given client-area pixel, or an
    // invalid coord if no square is there.
    CSCoord HitTest(POINT pt) const;

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
                   const std::vector<CSCoord>& legalDests) const;

    // Return the Unicode chess piece glyph for the given piece value.
    static wchar_t PieceGlyph(int8_t piece);
};

