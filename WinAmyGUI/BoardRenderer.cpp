/*
    WinAmyGUI — BoardRenderer
    GDI-based rendering of all 15 board levels side-by-side in a 4-column grid.
    Levels are arranged:
      Row 0: a(1×1)  b(2×2)  c(3×3)  d(4×4)
      Row 1: e(5×5)  f(6×6)  g(7×7)  h(8×8)
      Row 2: i(7×7)  j(6×6)  k(5×5)  l(4×4)
      Row 3: m(3×3)  n(2×2)  o(1×1)
*/

#include "BoardRenderer.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const wchar_t* LEVEL_NAMES = L"abcdefghijklmno";

// Maximum level width for row-height calculation.
static constexpr int MAX_WIDTH = CBitBoard::MAX_LEVEL_WIDTH; // 8

// Total pixel width/height of one "cell" in the layout grid (for the widest level, 8×8).
static constexpr int CELL_W = MAX_WIDTH * BoardRenderer::SQUARE_SIZE + BoardRenderer::BOARD_MARGIN;
static constexpr int CELL_H = MAX_WIDTH * BoardRenderer::SQUARE_SIZE
                            + BoardRenderer::LABEL_HEIGHT
                            + BoardRenderer::BOARD_MARGIN;

// Top-left pixel position of the level box for the given level index (0–14).
/* static */ POINT BoardRenderer::LevelOrigin(int level) {
    int row = level / COLS_PER_ROW;
    int col = level % COLS_PER_ROW;
    POINT pt;
    pt.x = col * CELL_W + BOARD_MARGIN;
    pt.y = row * CELL_H + BOARD_MARGIN;
    return pt;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

BoardRenderer::BoardRenderer() {
    // Create a font for chess Unicode glyphs.
    m_hPieceFont = CreateFontW(
        SQUARE_SIZE - 4,        // height
        0, 0, 0,                // width, escapement, orientation
        FW_NORMAL,              // weight
        FALSE, FALSE, FALSE,    // italic, underline, strikeout
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Symbol"
    );

    m_hLabelFont = CreateFontW(
        LABEL_HEIGHT - 2,
        0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
}

BoardRenderer::~BoardRenderer() {
    if (m_hPieceFont) DeleteObject(m_hPieceFont);
    if (m_hLabelFont) DeleteObject(m_hLabelFont);
}

// ---------------------------------------------------------------------------
// Public drawing entry point
// ---------------------------------------------------------------------------

void BoardRenderer::DrawBoard(HDC hdc, const CPosition* pos,
                              const CSCoord* selectedSquare,
                              const std::vector<CSCoord>& legalDests) const {
    for (int lvl = 0; lvl < CBitBoard::NUM_LEVELS; ++lvl)
        DrawLevel(hdc, lvl, pos, selectedSquare, legalDests);
}

// ---------------------------------------------------------------------------
// DrawLevel
// ---------------------------------------------------------------------------

void BoardRenderer::DrawLevel(HDC hdc, int level, const CPosition* pos,
                               const CSCoord* selectedSquare,
                               const std::vector<CSCoord>& legalDests) const {
    const int w = CBitBoard::LEVEL_WIDTH[level];
    POINT origin = LevelOrigin(level);

    // --- Draw level label ---
    {
        RECT labelRect{
            origin.x,
            origin.y,
            origin.x + w * SQUARE_SIZE,
            origin.y + LABEL_HEIGHT
        };
        HBRUSH hbr = CreateSolidBrush(CLR_LABEL_BG);
        FillRect(hdc, &labelRect, hbr);
        DeleteObject(hbr);

        HFONT hOldFont = (HFONT)SelectObject(hdc, m_hLabelFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, CLR_LABEL_FG);
        wchar_t label[4] = { LEVEL_NAMES[level], L'\0' };
        DrawTextW(hdc, label, 1, &labelRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hOldFont);
    }

    int boardY = origin.y + LABEL_HEIGHT;

    // --- Draw squares ---
    for (int rank = w - 1; rank >= 0; --rank) {
        for (int file = 0; file < w; ++file) {
            int px = origin.x + file * SQUARE_SIZE;
            int py = boardY + (w - 1 - rank) * SQUARE_SIZE;

            CSCoord sq((uint16_t)level, (uint16_t)file, (uint16_t)rank);
            uint16_t offset = sq.BitOffset();

            // Determine background colour.
            COLORREF bg = ((file + rank) % 2 == 0) ? CLR_DARK : CLR_LIGHT;

            // Highlight selected square.
            if (selectedSquare && selectedSquare->IsValid()
                    && selectedSquare->BitOffset() == offset) {
                bg = CLR_SELECTED;
            } else {
                // Highlight legal move destinations.
                for (const auto& dest : legalDests) {
                    if (dest.IsValid() && dest.BitOffset() == offset) {
                        bg = CLR_LEGAL_MOVE;
                        break;
                    }
                }
            }

            RECT r{ px, py, px + SQUARE_SIZE, py + SQUARE_SIZE };
            HBRUSH hbr = CreateSolidBrush(bg);
            FillRect(hdc, &r, hbr);
            DeleteObject(hbr);

            // Draw piece (if any).
            if (pos) {
                int8_t piece = pos->m_rgPiece[offset];
                if (piece != 0) {
                    wchar_t glyph[2] = { PieceGlyph(piece), L'\0' };
                    HFONT hOldFont = (HFONT)SelectObject(hdc, m_hPieceFont);
                    SetBkMode(hdc, TRANSPARENT);
                    // Use dark text on light squares, light text on dark squares
                    // (piece colour is encoded in sign of 'piece').
                    SetTextColor(hdc, (piece > 0) ? RGB(240,240,240) : RGB(20,20,20));
                    DrawTextW(hdc, glyph, 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldFont);
                }
            }

            // Square border.
            HPEN hpen = CreatePen(PS_SOLID, 1, CLR_BORDER);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hpen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, px, py, px + SQUARE_SIZE, py + SQUARE_SIZE);
            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hpen);
        }
    }
}

// ---------------------------------------------------------------------------
// HitTest
// ---------------------------------------------------------------------------

CSCoord BoardRenderer::HitTest(POINT pt) const {
    for (int lvl = 0; lvl < CBitBoard::NUM_LEVELS; ++lvl) {
        const int w = CBitBoard::LEVEL_WIDTH[lvl];
        POINT origin = LevelOrigin(lvl);
        int boardY = origin.y + LABEL_HEIGHT;

        int boardW = w * SQUARE_SIZE;
        int boardH = w * SQUARE_SIZE;

        if (pt.x >= origin.x && pt.x < origin.x + boardW
         && pt.y >= boardY    && pt.y < boardY + boardH) {
            int file = (pt.x - origin.x) / SQUARE_SIZE;
            int rankFromTop = (pt.y - boardY) / SQUARE_SIZE;
            int rank = (w - 1) - rankFromTop;
            if (file >= 0 && file < w && rank >= 0 && rank < w) {
                return CSCoord((uint16_t)lvl, (uint16_t)file, (uint16_t)rank);
            }
        }
    }
    return InvalidSquareCoord();
}

// ---------------------------------------------------------------------------
// GetBoardAreaSize
// ---------------------------------------------------------------------------

/* static */ SIZE BoardRenderer::GetBoardAreaSize() {
    // 4 columns × CELL_W + margins, 4 rows × CELL_H + margins
    SIZE sz;
    sz.cx = COLS_PER_ROW * CELL_W + BOARD_MARGIN;
    sz.cy = 4 * CELL_H + BOARD_MARGIN;
    return sz;
}

// ---------------------------------------------------------------------------
// PieceGlyph
// ---------------------------------------------------------------------------

/* static */ wchar_t BoardRenderer::PieceGlyph(int8_t piece) {
    // Positive = White, Negative = Black.
    // Piece type values: Pawn=1, Knight=2, Bishop=3, Rook=4, Queen=5, King=6, BPawn=7
    bool isWhite = (piece > 0);
    int type = (piece > 0) ? piece : -piece;

    // White pieces: ♔♕♖♗♘♙   U+2654..U+2659
    // Black pieces: ♚♛♜♝♞♟   U+265A..U+265F
    static const wchar_t WHITE_GLYPHS[] = { 0,
        L'\u2659', // Pawn
        L'\u2658', // Knight
        L'\u2657', // Bishop
        L'\u2656', // Rook
        L'\u2655', // Queen
        L'\u2654', // King
        L'\u2659'  // BPawn (treat same as Pawn glyph)
    };
    static const wchar_t BLACK_GLYPHS[] = { 0,
        L'\u265F', // Pawn
        L'\u265E', // Knight
        L'\u265D', // Bishop
        L'\u265C', // Rook
        L'\u265B', // Queen
        L'\u265A', // King
        L'\u265F'  // BPawn
    };

    if (type < 1 || type > 7) return L' ';
    return isWhite ? WHITE_GLYPHS[type] : BLACK_GLYPHS[type];
}
