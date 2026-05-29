#pragma once

// PieceAtlas
//
// Pre-renders the 12 Unicode chess piece glyphs (white King..Pawn and black
// King..Pawn from the U+2654..U+265F range) into a single 4 x 3 tile texture
// atlas usable as a D3D11 shader resource. Black pieces are rendered onto a
// filled white disc so they remain visible against the dark 3D background.

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

class PieceAtlas {
public:
    // Atlas layout: 4 columns x 3 rows of cells, each cell square.
    static constexpr int CELL_PIXELS = 128;
    static constexpr int COLS = 6;
    static constexpr int ROWS = 2;
    static constexpr int WIDTH  = COLS * CELL_PIXELS;
    static constexpr int HEIGHT = ROWS * CELL_PIXELS;

    PieceAtlas();
    ~PieceAtlas();

    // Creates the GDI-rasterised texture + SRV. Safe to call once per device.
    bool Build(ID3D11Device* pDevice);

    void Release();

    ID3D11ShaderResourceView* GetSRV() const { return m_pSRV.Get(); }

    // Returns UVs for the cell holding the glyph for the given piece value.
    // The piece value uses the same encoding as CPosition::m_rgPiece
    // (positive = white, negative = black, magnitude in 1..6 with the
    // engine's Pawn..King ordering).
    // Out parameters are normalised UVs (0..1) for the tile corners.
    bool GetPieceUV(int8_t nPiece, float& fU0, float& fV0,
                                    float& fU1, float& fV1) const;

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D>         m_pTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;
};
