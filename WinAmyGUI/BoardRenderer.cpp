/*
    WinAmyGUI — BoardRenderer
    GDI-based rendering of all 15 board levels in 3 rows with proportional spacing.

    Row layout (top → bottom):
      Row 0: j(6) k(5) l(4) m(3) n(2) o(1)   levels 9–14
      Row 1: g(7) h(8) i(7)                   levels 6–8
      Row 2: a(1) b(2) c(3) d(4) e(5) f(6)    levels 0–5

    X positions within each row are proportional to level widths (no fixed cell width).
    Odd-indexed levels use a cool blue-slate colour scheme; even use warm brown.
*/

#include "BoardRenderer.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Level → row/col mapping
// ---------------------------------------------------------------------------

static const wchar_t* LEVEL_NAMES = L"abcdefghijklmno";

// Row 0: levels 9,10,11,12,13,14  (j k l m n o)
// Row 1: levels 6,7,8             (g h i)
// Row 2: levels 0,1,2,3,4,5       (a b c d e f)
struct LevelPlacement { int row; int col; };
static constexpr LevelPlacement PLACEMENT[CBitBoard::NUM_LEVELS] = {
    {2,0},{2,1},{2,2},{2,3},{2,4},{2,5}, // a b c d e f  (levels 0-5)
    {1,0},{1,1},{1,2},                   // g h i        (levels 6-8)
    {0,0},{0,1},{0,2},{0,3},{0,4},{0,5}  // j k l m n o  (levels 9-14)
};

// Levels present in each row, in order (used to compute X offsets).
static constexpr int ROW_LEVELS[BoardRenderer::NUM_ROWS][6] = {
    { 9, 10, 11, 12, 13, 14 }, // row 0: j k l m n o
    { 6,  7,  8, -1, -1, -1 }, // row 1: g h i
    { 0,  1,  2,  3,  4,  5 }, // row 2: a b c d e f
};
static constexpr int ROW_COUNT[BoardRenderer::NUM_ROWS] = { 6, 3, 6 };

// Height (in squares) of the tallest level in each row.
static constexpr int ROW_MAX_WIDTH[BoardRenderer::NUM_ROWS] = { 6, 8, 6 };

// Pixel height of one row (board area only, not including top margin).
static int RowBoardH(int row) {
    return ROW_MAX_WIDTH[row] * BoardRenderer::SQUARE_SIZE;
}
static int RowTotalH(int row) {
    return RowBoardH(row) + BoardRenderer::LABEL_HEIGHT + BoardRenderer::BOARD_MARGIN;
}

// Y pixel origin of a row (top of its label).
static int RowOriginY(int row) {
    int y = BoardRenderer::BOARD_MARGIN;
    for (int r = 0; r < row; ++r)
        y += RowTotalH(r) + BoardRenderer::BOARD_MARGIN;
    return y;
}

// X pixel origin of the level box (left edge of its label/grid).
static int LevelOriginX(int level) {
    const LevelPlacement& p = PLACEMENT[level];
    int x = BoardRenderer::BOARD_MARGIN;
    for (int c = 0; c < p.col; ++c) {
        int lvl = ROW_LEVELS[p.row][c];
        x += CBitBoard::LEVEL_WIDTH[lvl] * BoardRenderer::SQUARE_SIZE
           + BoardRenderer::BOARD_MARGIN;
    }
    return x;
}

/* static */ POINT BoardRenderer::LevelOrigin(int level) {
    POINT pt;
    pt.x = LevelOriginX(level);
    pt.y = RowOriginY(PLACEMENT[level].row);
    return pt;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

BoardRenderer::BoardRenderer() {
    m_hPieceFont = CreateFontW(
        SQUARE_SIZE - 4, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Symbol"
    );
    m_hLabelFont = CreateFontW(
        LABEL_HEIGHT - 2, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
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
    bool isOdd = (level % 2) != 0;

    // Square colours for this level.
    COLORREF clrLight = isOdd ? CLR_LIGHT_ALT : CLR_LIGHT;
    COLORREF clrDark  = isOdd ? CLR_DARK_ALT  : CLR_DARK;

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
        wchar_t label[2] = { LEVEL_NAMES[level], L'\0' };
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

            COLORREF bg = ((file + rank) % 2 == 0) ? clrDark : clrLight;

            if (selectedSquare && selectedSquare->IsValid()
                    && selectedSquare->BitOffset() == offset) {
                bg = CLR_SELECTED;
            } else {
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

            if (pos) {
                int8_t piece = pos->m_rgPiece[offset];
                if (piece != 0) {
                    wchar_t glyph[2] = { PieceGlyph(piece), L'\0' };
                    HFONT hOldFont = (HFONT)SelectObject(hdc, m_hPieceFont);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, (piece > 0) ? RGB(240,240,240) : RGB(20,20,20));
                    DrawTextW(hdc, glyph, 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(hdc, hOldFont);
                }
            }

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
            int file       = (pt.x - origin.x) / SQUARE_SIZE;
            int rankFromTop = (pt.y - boardY)   / SQUARE_SIZE;
            int rank       = (w - 1) - rankFromTop;
            if (file >= 0 && file < w && rank >= 0 && rank < w)
                return CSCoord((uint16_t)lvl, (uint16_t)file, (uint16_t)rank);
        }
    }
    return InvalidSquareCoord();
}

// ---------------------------------------------------------------------------
// GetBoardAreaSize
// ---------------------------------------------------------------------------

/* static */ SIZE BoardRenderer::GetBoardAreaSize() {
    // Width: widest row.  All three rows happen to be the same (840 px).
    // Compute explicitly to be safe.
    int maxW = 0;
    for (int r = 0; r < NUM_ROWS; ++r) {
        int rowW = BOARD_MARGIN; // left margin
        for (int c = 0; c < ROW_COUNT[r]; ++c) {
            int lvl = ROW_LEVELS[r][c];
            rowW += CBitBoard::LEVEL_WIDTH[lvl] * SQUARE_SIZE + BOARD_MARGIN;
        }
        if (rowW > maxW) maxW = rowW;
    }

    // Height: sum of all row heights plus margins.
    int totalH = BOARD_MARGIN;
    for (int r = 0; r < NUM_ROWS; ++r)
        totalH += RowTotalH(r) + BOARD_MARGIN;

    SIZE sz{ maxW, totalH };
    return sz;
}

// ---------------------------------------------------------------------------
// PieceGlyph
// ---------------------------------------------------------------------------

/* static */ wchar_t BoardRenderer::PieceGlyph(int8_t piece) {
    bool isWhite = (piece > 0);
    int type = (piece > 0) ? piece : -piece;

    static const wchar_t WHITE_GLYPHS[] = { 0,
        L'\u2659', L'\u2658', L'\u2657', L'\u2656', L'\u2655', L'\u2654', L'\u2659'
    };
    static const wchar_t BLACK_GLYPHS[] = { 0,
        L'\u265F', L'\u265E', L'\u265D', L'\u265C', L'\u265B', L'\u265A', L'\u265F'
    };

    if (type < 1 || type > 7) return L' ';
    return isWhite ? WHITE_GLYPHS[type] : BLACK_GLYPHS[type];
}

