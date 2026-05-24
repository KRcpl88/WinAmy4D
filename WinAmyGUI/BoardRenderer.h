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

    // Margin between adjacent level boards.
    static constexpr int BOARD_MARGIN = 12;

    // Height of the level label above each board.
    static constexpr int LABEL_HEIGHT = 16;

    // Levels are arranged in 4 rows of 4 (last row has 3).
    static constexpr int COLS_PER_ROW = 4;

    // Colours used for squares and highlights.
    static constexpr COLORREF CLR_LIGHT      = RGB(240, 217, 181);
    static constexpr COLORREF CLR_DARK       = RGB(181, 136,  99);
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

    // Returns the pixel origin (top-left) of the level board for 'level' (0–14).
    static POINT LevelOrigin(int level);

    // Draw a single level.
    void DrawLevel(HDC hdc, int level, const CPosition* pos,
                   const CSCoord* selectedSquare,
                   const std::vector<CSCoord>& legalDests) const;

    // Return the Unicode chess piece glyph for the given piece value.
    static wchar_t PieceGlyph(int8_t piece);
};
