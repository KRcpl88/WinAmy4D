#pragma once

// D3DBoardRenderer
//
// Direct3D 11 wireframe renderer for the WinAmy 4D chess board.
//
// Geometry source: all valid CSCoord positions, each converted to a CUCoord
// (cell center in the unified 3D lattice). For each cell, CUCoord::GetOutline
// returns a deduplicated std::unordered_set<CChord> of edges. Edges are
// uploaded once into a static vertex buffer and drawn as a green line list
// on a black background.
//
// Pieces are rendered as camera-facing textured quads using a glyph atlas
// produced by PieceAtlas (white pieces in pure white; black pieces in black
// on a filled white disc so they are visible on the black background).
//
// User interaction:
//   * Left-mouse drag         => orbit (yaw + pitch)
//   * Mouse wheel             => zoom (camera distance)
//   * Left-click without drag => ray-pick a cell; the hit CSCoord is
//                                returned via HitTest3D so the host can run
//                                the same click-to-move logic used by the
//                                2D BoardRenderer.

#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <cstdint>

#include "dbase.h"
#include "scoord.h"
#include "ucoord.h"

#include "PieceAtlas.h"

class D3DBoardRenderer {
public:
    D3DBoardRenderer();
    ~D3DBoardRenderer();

    // One-time D3D11 init bound to the given HWND.
    bool Initialize(HWND hWnd);

    // Release all D3D resources.
    void Shutdown();

    bool IsInitialized() const { return m_pDevice != nullptr; }

    // Forwarded from WM_SIZE — recreates back buffer at the new size.
    void Resize(int nWidth, int nHeight);

    // Render a single frame of the given position + selection state.
    void Render(const CPosition* pPosition,
                const CSCoord* pSelectedSquare,
                const std::vector<CSCoord>& LegalDests);

    // Mouse input. Coordinates are in client-area pixels relative to the
    // top-left of the render area (the host applies any toolbar offset).
    // Returns true if the renderer consumed the event and the host should
    // skip further default handling.
    bool OnMouseDown(int nX, int nY);
    bool OnMouseMove(int nX, int nY);
    bool OnMouseUp(int nX, int nY);
    bool OnMouseWheel(int nDelta);

    // Returns true if the most recent mouse-down -> mouse-up was a click
    // (not a drag). Valid only inside the OnMouseUp call.
    bool LastInteractionWasClick() const { return m_bLastWasClick; }

    // Ray-picks the cell under the given client pixel and returns the
    // matching CSCoord. Returns an invalid CSCoord if no cell is hit.
    CSCoord HitTest3D(int nX, int nY) const;

    // Show/hide the cell wireframe (pieces and target markers remain).
    void SetShowOutlines(bool bShow);
    bool GetShowOutlines() const { return m_bShowOutlines; }

    // Select which cell outline style is drawn (full dodecahedron,
    // axial square slice, or one of the four hexagonal subsets). Safe to
    // call before Initialize — the new selection is applied to the line
    // buffer at first init or on the next call after the renderer is
    // initialized.
    void SetOutlineType(CUCoord::EOutlineType eType);
    CUCoord::EOutlineType GetOutlineType() const { return m_eOutlineType; }

    // Reset orbit yaw/pitch and zoom distance to defaults.
    void ResetView();

    // Adjust zoom by a multiplicative factor (>1.0 = zoom out, <1.0 = zoom in).
    void AdjustZoom(float fFactor);

private:
    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    // ---- D3D core ----
    ComPtr<ID3D11Device>            m_pDevice;
    ComPtr<ID3D11DeviceContext>     m_pContext;
    ComPtr<IDXGISwapChain>          m_pSwapChain;
    ComPtr<ID3D11RenderTargetView>  m_pRTV;
    ComPtr<ID3D11Texture2D>         m_pDepth;
    ComPtr<ID3D11DepthStencilView>  m_pDSV;
    ComPtr<ID3D11RasterizerState>   m_pRasterState;
    ComPtr<ID3D11DepthStencilState> m_pDepthStateLines;
    ComPtr<ID3D11DepthStencilState> m_pDepthStateSprites;
    ComPtr<ID3D11BlendState>        m_pBlendAlpha;

    // ---- Line pipeline ----
    ComPtr<ID3D11VertexShader>      m_pLineVS;
    ComPtr<ID3D11PixelShader>       m_pLinePS;
    ComPtr<ID3D11InputLayout>       m_pLineLayout;
    ComPtr<ID3D11Buffer>            m_pLineVB;
    ComPtr<ID3D11Buffer>            m_pLineCB; // view*proj + colour

    // ---- Highlight pipeline (dynamic line buffer) ----
    ComPtr<ID3D11Buffer>            m_pHighlightVB;
    UINT                            m_uHighlightCapacityVerts{0};

    // ---- Sprite (piece) pipeline ----
    ComPtr<ID3D11VertexShader>      m_pSpriteVS;
    ComPtr<ID3D11PixelShader>       m_pSpritePS;
    ComPtr<ID3D11InputLayout>       m_pSpriteLayout;
    ComPtr<ID3D11Buffer>            m_pSpriteVB;
    ComPtr<ID3D11Buffer>            m_pSpriteCB; // view*proj
    ComPtr<ID3D11SamplerState>      m_pSampler;
    UINT                            m_uSpriteCapacityVerts{0};

    PieceAtlas                      m_PieceAtlas;

    // Target marker texture (a soft-edged yellow disc) drawn on each legal
    // destination cell when a piece is selected, to make targets easy to
    // click.
    ComPtr<ID3D11Texture2D>          m_pTargetTex;
    ComPtr<ID3D11ShaderResourceView> m_pTargetSRV;

    // ---- Cell geometry (lattice positions used for picking & lines) ----
    struct LineVertex {
        float x, y, z;
    };
    std::vector<LineVertex>         m_LineVertices;     // CPU copy of the static wireframe

    struct CellInfo {
        CSCoord       Coord;
        DirectX::XMFLOAT3 Center;
    };
    std::vector<CellInfo>           m_Cells;
    DirectX::XMFLOAT3               m_BoardCenter{0,0,0};
    float                           m_BoardRadius{1.0f};

    // ---- Camera ----
    static constexpr float kDefaultYaw   =  0.6f;
    static constexpr float kDefaultPitch = -0.4f;
    float m_fYaw{kDefaultYaw};
    float m_fPitch{kDefaultPitch};
    float m_fDistance{0.0f};
    float m_fDefaultDistance{0.0f};

    // Show/hide cell outlines.
    bool  m_bShowOutlines{true};

    // Which outline geometry to render for each cell.
    CUCoord::EOutlineType m_eOutlineType{CUCoord::OT_hex_1};

    int   m_nClientW{1}, m_nClientH{1};
    HWND  m_hWnd{nullptr};

    // ---- Mouse state ----
    bool  m_bDragging{false};
    bool  m_bLastWasClick{false};
    int   m_nMouseX0{0}, m_nMouseY0{0};
    int   m_nMouseX{0},  m_nMouseY{0};

    // ---- Helpers ----
    bool  CreateDeviceAndSwapChain();
    bool  CreateBackBufferViews();
    bool  CreatePipelines();
    bool  CreateTargetMarkerTexture();
    void  BuildCellGeometry();
    bool  RebuildLineGeometry();
    void  EnsureSpriteCapacity(UINT uNeededVerts);
    void  EnsureHighlightCapacity(UINT uNeededVerts);

    DirectX::XMMATRIX MakeView()  const;
    DirectX::XMMATRIX MakeProj()  const;

    // Returns the cell center for a CSCoord in render space.
    DirectX::XMFLOAT3 CellCenter(const CSCoord& Coord) const;

    // Render passes
    void RenderLines(const DirectX::XMMATRIX& mViewProj);
    void RenderHighlights(const DirectX::XMMATRIX& mViewProj,
                          const CPosition* pPosition,
                          const CSCoord* pSelectedSquare,
                          const std::vector<CSCoord>& LegalDests);
    void RenderPieces(const DirectX::XMMATRIX& mViewProj,
                      const CPosition* pPosition);
    void RenderTargetMarkers(const DirectX::XMMATRIX& mViewProj,
                             const std::vector<CSCoord>& LegalDests);
};
