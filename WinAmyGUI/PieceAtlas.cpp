#include "PieceAtlas.h"

#include <vector>

// Piece type ids match the engine encoding used in CPosition::m_rgPiece /
// PIECEID(Type, side). The engine's Pawn..King ordering is:
//   1 Pawn, 2 Knight, 3 Bishop, 4 Rook, 5 Queen, 6 King
// White pieces are positive, black pieces are negative.

namespace {
// Atlas column order matches that of BoardRenderer::PieceGlyph so the white
// and black glyph rows visually align.
constexpr wchar_t WHITE_GLYPHS[6] = {
    L'\u2659', L'\u2658', L'\u2657', L'\u2656', L'\u2655', L'\u2654'
}; // Pawn Knight Bishop Rook Queen King
constexpr wchar_t BLACK_GLYPHS[6] = {
    L'\u265F', L'\u265E', L'\u265D', L'\u265C', L'\u265B', L'\u265A'
};

// Pack ARGB 32-bit BGRA8 pixel.
inline uint32_t PackBGRA(BYTE bR, BYTE bG, BYTE bB, BYTE bA) {
    return (static_cast<uint32_t>(bA) << 24) |
           (static_cast<uint32_t>(bR) << 16) |
           (static_cast<uint32_t>(bG) << 8) |
            static_cast<uint32_t>(bB);
}

// Render the glyph into a freshly allocated CELL_PIXELS x CELL_PIXELS RGBA8
// buffer. White-piece tiles render the glyph in white onto a transparent
// background; black-piece tiles render the glyph in black onto a filled
// white disc so they are visible against the dark 3D scene.
void RenderGlyphCell(wchar_t wGlyph, bool bIsWhite, uint32_t* pCellPixels) {
    const int nW = PieceAtlas::CELL_PIXELS;
    const int nH = PieceAtlas::CELL_PIXELS;

    HDC      hScreenDC = GetDC(nullptr);
    HDC      hMemDC    = CreateCompatibleDC(hScreenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = nW;
    bmi.bmiHeader.biHeight      = -nH; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hMemDC, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

    // Fill background fully transparent.
    {
        uint32_t* pPx = static_cast<uint32_t*>(pvBits);
        for (int i = 0; i < nW * nH; ++i) pPx[i] = 0x00000000;
    }

    // Composite the black-piece backing disc directly into the bitmap
    // (alpha = 255, RGB = white) before drawing the glyph on top with GDI.
    if (!bIsWhite) {
        const float fCx = nW * 0.5f;
        const float fCy = nH * 0.5f;
        const float fR  = nW * 0.42f;
        const float fRSq = fR * fR;
        uint32_t* pPx = static_cast<uint32_t*>(pvBits);
        for (int y = 0; y < nH; ++y) {
            for (int x = 0; x < nW; ++x) {
                float fDx = x + 0.5f - fCx;
                float fDy = y + 0.5f - fCy;
                if (fDx * fDx + fDy * fDy <= fRSq) {
                    pPx[y * nW + x] = PackBGRA(255, 255, 255, 255);
                }
            }
        }
    }

    HFONT hFont = CreateFontW(
        nH - 8, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Symbol"
    );
    HFONT hOldFont = (HFONT)SelectObject(hMemDC, hFont);
    SetBkMode(hMemDC, TRANSPARENT);
    SetTextColor(hMemDC, bIsWhite ? RGB(255,255,255) : RGB(0,0,0));

    RECT rc{0, 0, nW, nH};
    DrawTextW(hMemDC, &wGlyph, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
    GdiFlush();

    // GDI ignores alpha; reconstruct it for glyph pixels.
    //  - White-piece pixels: alpha = max(R,G,B) of the painted glyph.
    //  - Black-piece pixels: keep the already-rendered white disc opaque
    //    and force the dark-glyph foreground to fully opaque black so
    //    anti-aliasing edges blend onto the disc rather than the scene.
    {
        uint32_t* pPx = static_cast<uint32_t*>(pvBits);
        for (int i = 0; i < nW * nH; ++i) {
            uint32_t v = pPx[i];
            BYTE bB = static_cast<BYTE>((v      ) & 0xff);
            BYTE bG = static_cast<BYTE>((v >>  8) & 0xff);
            BYTE bR = static_cast<BYTE>((v >> 16) & 0xff);
            BYTE bA = static_cast<BYTE>((v >> 24) & 0xff);
            if (bIsWhite) {
                BYTE bMax = bR;
                if (bG > bMax) bMax = bG;
                if (bB > bMax) bMax = bB;
                bA = bMax;
            } else {
                // The disc-backed tile already has alpha = 255 for the disc
                // pixels and alpha = 0 outside; the glyph itself was drawn
                // on top in black. If alpha is 0 here, leave it transparent.
                if (bA == 0) {
                    // Outside the disc — keep fully transparent regardless
                    // of what GDI happened to leave in the colour channels.
                    pPx[i] = 0x00000000;
                    continue;
                }
                // Inside the disc — keep opaque.
                bA = 255;
            }
            pPx[i] = PackBGRA(bR, bG, bB, bA);
        }
    }

    // Copy out.
    memcpy(pCellPixels, pvBits, static_cast<size_t>(nW) * nH * sizeof(uint32_t));

    SelectObject(hMemDC, hOldFont);
    DeleteObject(hFont);
    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
}
} // namespace

PieceAtlas::PieceAtlas() = default;
PieceAtlas::~PieceAtlas() { Release(); }

void PieceAtlas::Release() {
    m_pSRV.Reset();
    m_pTexture.Reset();
}

bool PieceAtlas::Build(ID3D11Device* pDevice) {
    Release();
    if (!pDevice) return false;

    std::vector<uint32_t> Pixels(static_cast<size_t>(WIDTH) * HEIGHT, 0);

    auto WriteCell = [&](int nCol, int nRow, wchar_t wGlyph, bool bIsWhite) {
        std::vector<uint32_t> Cell(static_cast<size_t>(CELL_PIXELS) * CELL_PIXELS, 0);
        RenderGlyphCell(wGlyph, bIsWhite, Cell.data());
        for (int y = 0; y < CELL_PIXELS; ++y) {
            uint32_t* pDst = Pixels.data()
                + static_cast<size_t>(nRow * CELL_PIXELS + y) * WIDTH
                + static_cast<size_t>(nCol * CELL_PIXELS);
            const uint32_t* pSrc = Cell.data() + static_cast<size_t>(y) * CELL_PIXELS;
            memcpy(pDst, pSrc, CELL_PIXELS * sizeof(uint32_t));
        }
    };

    // Row 0 = white pieces, row 1 = black pieces. Column = (type - 1).
    for (int i = 0; i < 6; ++i) {
        WriteCell(i, 0, WHITE_GLYPHS[i], true);
        WriteCell(i, 1, BLACK_GLYPHS[i], false);
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = WIDTH;
    desc.Height           = HEIGHT;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem     = Pixels.data();
    srd.SysMemPitch = WIDTH * sizeof(uint32_t);

    HRESULT hr = pDevice->CreateTexture2D(&desc, &srd, m_pTexture.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = pDevice->CreateShaderResourceView(m_pTexture.Get(), nullptr, m_pSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}

bool PieceAtlas::GetPieceUV(int8_t nPiece, float& fU0, float& fV0,
                                            float& fU1, float& fV1) const {
    if (nPiece == 0) return false;
    int nType = (nPiece > 0) ? nPiece : -nPiece;
    if (nType < 1 || nType > 6) return false;
    int nCol = nType - 1;
    int nRow = (nPiece > 0) ? 0 : 1;
    fU0 =  static_cast<float>(nCol)      / COLS;
    fU1 =  static_cast<float>(nCol + 1)  / COLS;
    fV0 =  static_cast<float>(nRow)      / ROWS;
    fV1 =  static_cast<float>(nRow + 1)  / ROWS;
    return true;
}
