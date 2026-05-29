#include "D3DBoardRenderer.h"

#include <d3dcompiler.h>
#include <DirectXMath.h>

#define NOMINMAX
#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "ucoord.h"
#include "ucoordFloat.h"
#include "chord.h"
#include "bitboard.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// HLSL — compiled at runtime via D3DCompile
// ---------------------------------------------------------------------------

namespace {
const char* kLineHlsl = R"(
cbuffer CB : register(b0) {
    row_major float4x4 g_mViewProj;
    float4             g_vColor;
};
struct VSIn { float3 pos : POSITION; };
struct VSOut { float4 pos : SV_POSITION; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), g_mViewProj);
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET {
    return g_vColor;
}
)";

const char* kSpriteHlsl = R"(
cbuffer CB : register(b0) {
    row_major float4x4 g_mViewProj;
};
Texture2D    g_Tex : register(t0);
SamplerState g_Smp : register(s0);
struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), g_mViewProj);
    o.uv  = i.uv;
    return o;
}
float4 PSMain(VSOut i) : SV_TARGET {
    float4 c = g_Tex.Sample(g_Smp, i.uv);
    if (c.a < 0.02f) discard;
    return c;
}
)";

struct alignas(16) LineCB {
    XMFLOAT4X4 mViewProj;
    XMFLOAT4   vColor;
};

struct alignas(16) SpriteCB {
    XMFLOAT4X4 mViewProj;
};

struct SpriteVertex {
    XMFLOAT3 pos;
    XMFLOAT2 uv;
};

bool Compile(const char* pSrc, const char* pEntry, const char* pTarget,
             ID3DBlob** ppOut) {
    ComPtr<ID3DBlob> pErr;
    HRESULT hr = D3DCompile(pSrc, strlen(pSrc), nullptr, nullptr, nullptr,
                            pEntry, pTarget, 0, 0, ppOut, pErr.GetAddressOf());
    if (FAILED(hr)) {
        if (pErr) {
            OutputDebugStringA(static_cast<const char*>(pErr->GetBufferPointer()));
        }
        return false;
    }
    return true;
}

inline XMFLOAT3 UCoordToFloat3(const CUCoordFloat& v) {
    return XMFLOAT3(static_cast<float>(v.GetX()),
                    static_cast<float>(v.GetY()),
                    static_cast<float>(v.GetZ()));
}
} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

D3DBoardRenderer::D3DBoardRenderer() = default;
D3DBoardRenderer::~D3DBoardRenderer() { Shutdown(); }

void D3DBoardRenderer::Shutdown() {
    if (m_pContext) m_pContext->ClearState();
    m_pTargetSRV.Reset();
    m_pTargetTex.Reset();
    m_PieceAtlas.Release();
    m_pSampler.Reset();
    m_pSpriteCB.Reset();
    m_pSpriteVB.Reset();
    m_pSpriteLayout.Reset();
    m_pSpritePS.Reset();
    m_pSpriteVS.Reset();
    m_pHighlightVB.Reset();
    m_pLineCB.Reset();
    m_pLineVB.Reset();
    m_pLineLayout.Reset();
    m_pLinePS.Reset();
    m_pLineVS.Reset();
    m_pBlendAlpha.Reset();
    m_pDepthStateSprites.Reset();
    m_pDepthStateLines.Reset();
    m_pRasterState.Reset();
    m_pDSV.Reset();
    m_pDepth.Reset();
    m_pRTV.Reset();
    m_pSwapChain.Reset();
    m_pContext.Reset();
    m_pDevice.Reset();
}

bool D3DBoardRenderer::Initialize(HWND hWnd) {
    if (m_pDevice) return true;
    m_hWnd = hWnd;

    RECT rc{};
    GetClientRect(hWnd, &rc);
    m_nClientW = (std::max)(1, (int)(rc.right - rc.left));
    m_nClientH = (std::max)(1, (int)(rc.bottom - rc.top));

    if (!CreateDeviceAndSwapChain()) { Shutdown(); return false; }
    if (!CreateBackBufferViews())    { Shutdown(); return false; }
    if (!CreatePipelines())          { Shutdown(); return false; }
    if (!m_PieceAtlas.Build(m_pDevice.Get())) { Shutdown(); return false; }
    if (!CreateTargetMarkerTexture()) { Shutdown(); return false; }

    BuildCellGeometry();

    // Upload static line vertex buffer.
    if (!m_LineVertices.empty()) {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = static_cast<UINT>(sizeof(LineVertex) * m_LineVertices.size());
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = m_LineVertices.data();
        if (FAILED(m_pDevice->CreateBuffer(&bd, &srd, m_pLineVB.GetAddressOf()))) {
            Shutdown();
            return false;
        }
    }

    // Initial camera: distance enough to see the whole board.
    m_fDefaultDistance = (std::max)(8.0f, m_BoardRadius * 2.5f);
    m_fDistance        = m_fDefaultDistance;

    return true;
}

bool D3DBoardRenderer::CreateDeviceAndSwapChain() {
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount  = 1;
    scd.BufferDesc.Width  = m_nClientW;
    scd.BufferDesc.Height = m_nClientH;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = m_hWnd;
    scd.SampleDesc.Count   = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed   = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = 0;

    UINT uFlags = 0;
#if defined(_DEBUG)
    uFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL flOut{};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uFlags,
        featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION, &scd,
        m_pSwapChain.GetAddressOf(),
        m_pDevice.GetAddressOf(),
        &flOut,
        m_pContext.GetAddressOf()
    );
    if (FAILED(hr)) {
        // Try WARP as a fallback.
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            featureLevels, _countof(featureLevels),
            D3D11_SDK_VERSION, &scd,
            m_pSwapChain.GetAddressOf(),
            m_pDevice.GetAddressOf(),
            &flOut,
            m_pContext.GetAddressOf()
        );
    }
    return SUCCEEDED(hr);
}

bool D3DBoardRenderer::CreateBackBufferViews() {
    m_pRTV.Reset();
    m_pDSV.Reset();
    m_pDepth.Reset();

    ComPtr<ID3D11Texture2D> pBackBuf;
    HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBuf.GetAddressOf()));
    if (FAILED(hr)) return false;
    hr = m_pDevice->CreateRenderTargetView(pBackBuf.Get(), nullptr, m_pRTV.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width  = m_nClientW;
    dd.Height = m_nClientH;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage     = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = m_pDevice->CreateTexture2D(&dd, nullptr, m_pDepth.GetAddressOf());
    if (FAILED(hr)) return false;
    hr = m_pDevice->CreateDepthStencilView(m_pDepth.Get(), nullptr, m_pDSV.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_VIEWPORT vp{};
    vp.Width    = static_cast<float>(m_nClientW);
    vp.Height   = static_cast<float>(m_nClientH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pContext->RSSetViewports(1, &vp);
    return true;
}

bool D3DBoardRenderer::CreatePipelines() {
    // Rasterizer (no culling — we draw lines and billboards).
    // AA off: hardware lines are 1px; AA tends to make them look thicker
    // and slightly blurred, so we render crisp 1-pixel lines instead.
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        rd.AntialiasedLineEnable = FALSE;
        if (FAILED(m_pDevice->CreateRasterizerState(&rd, m_pRasterState.GetAddressOf()))) return false;
    }
    // Depth states.
    {
        D3D11_DEPTH_STENCIL_DESC dd{};
        dd.DepthEnable    = TRUE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        if (FAILED(m_pDevice->CreateDepthStencilState(&dd, m_pDepthStateLines.GetAddressOf()))) return false;
        // Sprites don't write depth so they all blend in draw order.
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        if (FAILED(m_pDevice->CreateDepthStencilState(&dd, m_pDepthStateSprites.GetAddressOf()))) return false;
    }
    // Alpha blend state.
    {
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable    = TRUE;
        bd.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(m_pDevice->CreateBlendState(&bd, m_pBlendAlpha.GetAddressOf()))) return false;
    }
    // Sampler.
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter   = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(m_pDevice->CreateSamplerState(&sd, m_pSampler.GetAddressOf()))) return false;
    }

    // Line pipeline.
    {
        ComPtr<ID3DBlob> pVSBlob, pPSBlob;
        if (!Compile(kLineHlsl, "VSMain", "vs_4_0", pVSBlob.GetAddressOf())) return false;
        if (!Compile(kLineHlsl, "PSMain", "ps_4_0", pPSBlob.GetAddressOf())) return false;
        if (FAILED(m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                                 pVSBlob->GetBufferSize(), nullptr,
                                                 m_pLineVS.GetAddressOf()))) return false;
        if (FAILED(m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                                pPSBlob->GetBufferSize(), nullptr,
                                                m_pLinePS.GetAddressOf()))) return false;

        D3D11_INPUT_ELEMENT_DESC il[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(m_pDevice->CreateInputLayout(il, _countof(il),
                                                pVSBlob->GetBufferPointer(),
                                                pVSBlob->GetBufferSize(),
                                                m_pLineLayout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(LineCB);
        bd.Usage     = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_pDevice->CreateBuffer(&bd, nullptr, m_pLineCB.GetAddressOf()))) return false;
    }

    // Sprite pipeline.
    {
        ComPtr<ID3DBlob> pVSBlob, pPSBlob;
        if (!Compile(kSpriteHlsl, "VSMain", "vs_4_0", pVSBlob.GetAddressOf())) return false;
        if (!Compile(kSpriteHlsl, "PSMain", "ps_4_0", pPSBlob.GetAddressOf())) return false;
        if (FAILED(m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
                                                 pVSBlob->GetBufferSize(), nullptr,
                                                 m_pSpriteVS.GetAddressOf()))) return false;
        if (FAILED(m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
                                                pPSBlob->GetBufferSize(), nullptr,
                                                m_pSpritePS.GetAddressOf()))) return false;

        D3D11_INPUT_ELEMENT_DESC il[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,                                 D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT,     D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(m_pDevice->CreateInputLayout(il, _countof(il),
                                                pVSBlob->GetBufferPointer(),
                                                pVSBlob->GetBufferSize(),
                                                m_pSpriteLayout.GetAddressOf()))) return false;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(SpriteCB);
        bd.Usage     = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_pDevice->CreateBuffer(&bd, nullptr, m_pSpriteCB.GetAddressOf()))) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// BuildCellGeometry
//
// Enumerates every valid CSCoord, builds:
//   * m_Cells      — list of cells with their render-space centers.
//   * m_LineVertices — flattened pairs of endpoints for every unique CChord
//                     across all cells (dedup via unordered_set<CChord>).
// Also computes m_BoardCenter and m_BoardRadius for the orbit camera.
// ---------------------------------------------------------------------------

void D3DBoardRenderer::BuildCellGeometry() {
    m_Cells.clear();
    m_LineVertices.clear();

    std::unordered_set<CChord> Chords;

    // Track bounding-box extents in render space.
    float fMinX =  1e9f, fMinY =  1e9f, fMinZ =  1e9f;
    float fMaxX = -1e9f, fMaxY = -1e9f, fMaxZ = -1e9f;

    for (uint16_t nLevel = 0; nLevel < CBitBoard::NUM_LEVELS; ++nLevel) {
        const uint16_t nWidth = CBitBoard::LEVEL_WIDTH[nLevel];
        for (uint16_t nRank = 0; nRank < nWidth; ++nRank) {
            for (uint16_t nFile = 0; nFile < nWidth; ++nFile) {
                CSCoord Coord(nLevel, nFile, nRank);
                CUCoord UCoord(Coord);

                CellInfo Info;
                Info.Coord  = Coord;
                Info.Center = UCoordToFloat3(CUCoordFloat(UCoord));
                m_Cells.push_back(Info);

                fMinX = (std::min)(fMinX, Info.Center.x);
                fMinY = (std::min)(fMinY, Info.Center.y);
                fMinZ = (std::min)(fMinZ, Info.Center.z);
                fMaxX = (std::max)(fMaxX, Info.Center.x);
                fMaxY = (std::max)(fMaxY, Info.Center.y);
                fMaxZ = (std::max)(fMaxZ, Info.Center.z);

                UCoord.GetOutline(Chords);
            }
        }
    }

    m_BoardCenter = XMFLOAT3(
        0.5f * (fMinX + fMaxX),
        0.5f * (fMinY + fMaxY),
        0.5f * (fMinZ + fMaxZ));

    float fDx = fMaxX - fMinX;
    float fDy = fMaxY - fMinY;
    float fDz = fMaxZ - fMinZ;
    m_BoardRadius = 0.5f * std::sqrt(fDx*fDx + fDy*fDy + fDz*fDz) + 1.5f;

    m_LineVertices.reserve(Chords.size() * 2);
    for (const auto& C : Chords) {
        const CUCoordFloat& A = C.GetStart();
        const CUCoordFloat& B = C.GetEnd();
        LineVertex va{ (float)A.GetX(), (float)A.GetY(), (float)A.GetZ() };
        LineVertex vb{ (float)B.GetX(), (float)B.GetY(), (float)B.GetZ() };
        m_LineVertices.push_back(va);
        m_LineVertices.push_back(vb);
    }
}

XMFLOAT3 D3DBoardRenderer::CellCenter(const CSCoord& Coord) const {
    for (const auto& C : m_Cells) {
        if (C.Coord == Coord) return C.Center;
    }
    // Fallback: compute on the fly.
    return UCoordToFloat3(CUCoordFloat(CUCoord(Coord)));
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void D3DBoardRenderer::Resize(int nWidth, int nHeight) {
    if (!m_pSwapChain) return;
    nWidth  = (std::max)(1, nWidth);
    nHeight = (std::max)(1, nHeight);
    if (nWidth == m_nClientW && nHeight == m_nClientH && m_pRTV) return;

    m_pContext->OMSetRenderTargets(0, nullptr, nullptr);
    m_pRTV.Reset();
    m_pDSV.Reset();
    m_pDepth.Reset();

    m_nClientW = nWidth;
    m_nClientH = nHeight;
    HRESULT hr = m_pSwapChain->ResizeBuffers(0, m_nClientW, m_nClientH,
                                             DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return;
    CreateBackBufferViews();
}

// ---------------------------------------------------------------------------
// Camera math
// ---------------------------------------------------------------------------

XMMATRIX D3DBoardRenderer::MakeView() const {
    float fCp = std::cos(m_fPitch);
    float fSp = std::sin(m_fPitch);
    float fCy = std::cos(m_fYaw);
    float fSy = std::sin(m_fYaw);
    XMVECTOR vEye = XMVectorSet(
        m_BoardCenter.x + m_fDistance * fCp * fCy,
        m_BoardCenter.y + m_fDistance * fCp * fSy,
        m_BoardCenter.z + m_fDistance * fSp,
        1.0f);
    XMVECTOR vCenter = XMVectorSet(m_BoardCenter.x, m_BoardCenter.y, m_BoardCenter.z, 1.0f);
    XMVECTOR vUp     = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    return XMMatrixLookAtLH(vEye, vCenter, vUp);
}

XMMATRIX D3DBoardRenderer::MakeProj() const {
    float fAspect = (float)m_nClientW / (float)m_nClientH;
    if (fAspect <= 0.0f) fAspect = 1.0f;
    return XMMatrixPerspectiveFovLH(XM_PIDIV4, fAspect, 0.1f, 1000.0f);
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void D3DBoardRenderer::Render(const CPosition* pPosition,
                              const CSCoord* pSelectedSquare,
                              const std::vector<CSCoord>& LegalDests) {
    if (!m_pDevice || !m_pRTV) return;

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_pContext->OMSetRenderTargets(1, m_pRTV.GetAddressOf(), m_pDSV.Get());
    m_pContext->ClearRenderTargetView(m_pRTV.Get(), clearColor);
    m_pContext->ClearDepthStencilView(m_pDSV.Get(),
                                      D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                      1.0f, 0);

    m_pContext->RSSetState(m_pRasterState.Get());

    XMMATRIX mView = MakeView();
    XMMATRIX mProj = MakeProj();
    XMMATRIX mViewProj = mView * mProj;

    RenderLines(mViewProj);
    RenderTargetMarkers(mViewProj, LegalDests);
    RenderHighlights(mViewProj, pPosition, pSelectedSquare, LegalDests);
    RenderPieces(mViewProj, pPosition);

    m_pSwapChain->Present(1, 0);
}

// ---------------------------------------------------------------------------
// Outline visibility, view reset, and zoom controls
// ---------------------------------------------------------------------------

void D3DBoardRenderer::SetShowOutlines(bool bShow) {
    if (m_bShowOutlines == bShow) return;
    m_bShowOutlines = bShow;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void D3DBoardRenderer::ResetView() {
    m_fYaw      = kDefaultYaw;
    m_fPitch    = kDefaultPitch;
    m_fDistance = m_fDefaultDistance > 0.0f ? m_fDefaultDistance : (m_BoardRadius * 2.5f);
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

void D3DBoardRenderer::AdjustZoom(float fFactor) {
    if (fFactor <= 0.0f) return;
    m_fDistance *= fFactor;
    if (m_fDistance < 1.5f)   m_fDistance = 1.5f;
    if (m_fDistance > 500.0f) m_fDistance = 500.0f;
    if (m_hWnd) InvalidateRect(m_hWnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// CreateTargetMarkerTexture — soft-edged filled yellow disc on transparent
// background, used as a billboard sprite on each legal-destination cell so
// the targets read clearly and are easy to click.
// ---------------------------------------------------------------------------

bool D3DBoardRenderer::CreateTargetMarkerTexture() {
    constexpr int kSize = 64;
    std::vector<uint32_t> Pixels(static_cast<size_t>(kSize) * kSize, 0);
    const float fCenter = kSize * 0.5f - 0.5f;
    const float fInner  = kSize * 0.40f; // fully opaque
    const float fOuter  = kSize * 0.46f; // edge softening
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            float fDx = x - fCenter;
            float fDy = y - fCenter;
            float fR  = std::sqrt(fDx * fDx + fDy * fDy);
            float fA  = 1.0f;
            if (fR >= fOuter)      fA = 0.0f;
            else if (fR >  fInner) fA = (fOuter - fR) / (fOuter - fInner);
            uint8_t a = static_cast<uint8_t>(fA * 0.85f * 255.0f); // ~85% peak so background still bleeds through
            // BGRA: yellow = (B=0, G=255, R=255)
            Pixels[static_cast<size_t>(y) * kSize + x] =
                (static_cast<uint32_t>(a) << 24) |
                (255u << 16) | // R
                (255u <<  8) | // G
                (  0u);        // B
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = kSize;
    desc.Height           = kSize;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem     = Pixels.data();
    srd.SysMemPitch = kSize * sizeof(uint32_t);
    if (FAILED(m_pDevice->CreateTexture2D(&desc, &srd, m_pTargetTex.GetAddressOf()))) return false;
    if (FAILED(m_pDevice->CreateShaderResourceView(m_pTargetTex.Get(), nullptr, m_pTargetSRV.GetAddressOf()))) return false;
    return true;
}

void D3DBoardRenderer::RenderLines(const XMMATRIX& mViewProj) {
    if (!m_bShowOutlines) return;
    if (!m_pLineVB || m_LineVertices.empty()) return;

    LineCB cb;
    XMStoreFloat4x4(&cb.mViewProj, mViewProj);
    // Dim green so the wireframe reads as a fine, low-contrast guide rather
    // than a dominant element of the scene.
    cb.vColor = XMFLOAT4(0.05f, 0.50f, 0.05f, 1.0f);

    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(m_pContext->Map(m_pLineCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    memcpy(map.pData, &cb, sizeof(cb));
    m_pContext->Unmap(m_pLineCB.Get(), 0);

    UINT uStride = sizeof(LineVertex);
    UINT uOffset = 0;
    m_pContext->IASetInputLayout(m_pLineLayout.Get());
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_pContext->IASetVertexBuffers(0, 1, m_pLineVB.GetAddressOf(), &uStride, &uOffset);
    m_pContext->VSSetShader(m_pLineVS.Get(), nullptr, 0);
    m_pContext->VSSetConstantBuffers(0, 1, m_pLineCB.GetAddressOf());
    m_pContext->PSSetShader(m_pLinePS.Get(), nullptr, 0);
    m_pContext->PSSetConstantBuffers(0, 1, m_pLineCB.GetAddressOf());
    m_pContext->OMSetDepthStencilState(m_pDepthStateLines.Get(), 0);
    float blendFactor[4]{};
    m_pContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

    m_pContext->Draw(static_cast<UINT>(m_LineVertices.size()), 0);
}

void D3DBoardRenderer::EnsureHighlightCapacity(UINT uNeededVerts) {
    if (uNeededVerts <= m_uHighlightCapacityVerts && m_pHighlightVB) return;
    UINT uNew = (std::max<UINT>)(64, m_uHighlightCapacityVerts ? m_uHighlightCapacityVerts * 2 : 256);
    while (uNew < uNeededVerts) uNew *= 2;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(LineVertex) * uNew;
    bd.Usage     = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_pHighlightVB.Reset();
    if (SUCCEEDED(m_pDevice->CreateBuffer(&bd, nullptr, m_pHighlightVB.GetAddressOf()))) {
        m_uHighlightCapacityVerts = uNew;
    }
}

void D3DBoardRenderer::RenderHighlights(const XMMATRIX& mViewProj,
                                         const CPosition* /*pPosition*/,
                                         const CSCoord* pSelectedSquare,
                                         const std::vector<CSCoord>& LegalDests) {
    // Build a flat list of CChord endpoints for the selected square (in one
    // colour) and each legal-destination square (in another colour). We do
    // this in two passes since both share the line pipeline / constant
    // buffer colour parameter.

    auto BuildBufferForCell = [&](const CSCoord& Coord, std::vector<LineVertex>& Out) {
        std::unordered_set<CChord> Chords;
        CUCoord(Coord).GetOutline(Chords);
        for (const auto& C : Chords) {
            const auto& A = C.GetStart();
            const auto& B = C.GetEnd();
            Out.push_back({ (float)A.GetX(), (float)A.GetY(), (float)A.GetZ() });
            Out.push_back({ (float)B.GetX(), (float)B.GetY(), (float)B.GetZ() });
        }
    };

    auto DrawCellList = [&](const std::vector<LineVertex>& Verts, const XMFLOAT4& Color) {
        if (Verts.empty()) return;
        EnsureHighlightCapacity(static_cast<UINT>(Verts.size()));
        if (!m_pHighlightVB) return;

        D3D11_MAPPED_SUBRESOURCE map{};
        if (FAILED(m_pContext->Map(m_pHighlightVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
        memcpy(map.pData, Verts.data(), Verts.size() * sizeof(LineVertex));
        m_pContext->Unmap(m_pHighlightVB.Get(), 0);

        LineCB cb;
        XMStoreFloat4x4(&cb.mViewProj, mViewProj);
        cb.vColor = Color;
        if (FAILED(m_pContext->Map(m_pLineCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
        memcpy(map.pData, &cb, sizeof(cb));
        m_pContext->Unmap(m_pLineCB.Get(), 0);

        UINT uStride = sizeof(LineVertex);
        UINT uOffset = 0;
        m_pContext->IASetInputLayout(m_pLineLayout.Get());
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        m_pContext->IASetVertexBuffers(0, 1, m_pHighlightVB.GetAddressOf(), &uStride, &uOffset);
        m_pContext->VSSetShader(m_pLineVS.Get(), nullptr, 0);
        m_pContext->VSSetConstantBuffers(0, 1, m_pLineCB.GetAddressOf());
        m_pContext->PSSetShader(m_pLinePS.Get(), nullptr, 0);
        m_pContext->PSSetConstantBuffers(0, 1, m_pLineCB.GetAddressOf());
        // Draw highlights with depth disabled-write so they remain visible
        // through any near geometry.
        m_pContext->OMSetDepthStencilState(m_pDepthStateSprites.Get(), 0);
        float blendFactor[4]{};
        m_pContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
        m_pContext->Draw(static_cast<UINT>(Verts.size()), 0);
    };

    // Legal destinations are now shown as filled yellow discs by
    // RenderTargetMarkers, which reads more clearly than wireframe outlines
    // and gives a larger pick target. No cyan outline pass needed.
    (void)LegalDests;

    // Selected square (yellow, drawn last so it sits on top).
    if (pSelectedSquare && pSelectedSquare->IsValid()) {
        std::vector<LineVertex> Verts;
        Verts.reserve(48);
        BuildBufferForCell(*pSelectedSquare, Verts);
        DrawCellList(Verts, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
    }
}

void D3DBoardRenderer::EnsureSpriteCapacity(UINT uNeededVerts) {
    if (uNeededVerts <= m_uSpriteCapacityVerts && m_pSpriteVB) return;
    UINT uNew = (std::max<UINT>)(64, m_uSpriteCapacityVerts ? m_uSpriteCapacityVerts * 2 : 256);
    while (uNew < uNeededVerts) uNew *= 2;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(SpriteVertex) * uNew;
    bd.Usage     = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_pSpriteVB.Reset();
    if (SUCCEEDED(m_pDevice->CreateBuffer(&bd, nullptr, m_pSpriteVB.GetAddressOf()))) {
        m_uSpriteCapacityVerts = uNew;
    }
}

void D3DBoardRenderer::RenderPieces(const XMMATRIX& mViewProj, const CPosition* pPosition) {
    if (!pPosition) return;
    if (!m_PieceAtlas.GetSRV()) return;

    // Compute camera-space right / up so quads face the camera.
    XMMATRIX mView = MakeView();
    XMVECTOR vDet;
    XMMATRIX mInvView = XMMatrixInverse(&vDet, mView);
    XMFLOAT3 vRight, vUp;
    XMStoreFloat3(&vRight, mInvView.r[0]);
    XMStoreFloat3(&vUp,    mInvView.r[1]);

    const float fHalf = 0.55f;

    std::vector<SpriteVertex> Verts;
    Verts.reserve(64);

    for (const auto& Cell : m_Cells) {
        int8_t nPiece = pPosition->m_rgPiece[Cell.Coord.BitOffset()];
        if (nPiece == 0) continue;

        float u0, v0, u1, v1;
        if (!m_PieceAtlas.GetPieceUV(nPiece, u0, v0, u1, v1)) continue;

        XMFLOAT3 c = Cell.Center;
        XMFLOAT3 r{ vRight.x * fHalf, vRight.y * fHalf, vRight.z * fHalf };
        XMFLOAT3 u{ vUp.x    * fHalf, vUp.y    * fHalf, vUp.z    * fHalf };

        XMFLOAT3 p00{ c.x - r.x - u.x, c.y - r.y - u.y, c.z - r.z - u.z }; // bottom-left
        XMFLOAT3 p10{ c.x + r.x - u.x, c.y + r.y - u.y, c.z + r.z - u.z }; // bottom-right
        XMFLOAT3 p01{ c.x - r.x + u.x, c.y - r.y + u.y, c.z - r.z + u.z }; // top-left
        XMFLOAT3 p11{ c.x + r.x + u.x, c.y + r.y + u.y, c.z + r.z + u.z }; // top-right

        // Note: atlas is top-down (row 0 = top), so flip V so the glyph
        // isn't rendered upside down.
        // Tri 1: TL, TR, BL
        Verts.push_back({ p01, { u0, v0 } });
        Verts.push_back({ p11, { u1, v0 } });
        Verts.push_back({ p00, { u0, v1 } });
        // Tri 2: TR, BR, BL
        Verts.push_back({ p11, { u1, v0 } });
        Verts.push_back({ p10, { u1, v1 } });
        Verts.push_back({ p00, { u0, v1 } });
    }

    if (Verts.empty()) return;
    EnsureSpriteCapacity(static_cast<UINT>(Verts.size()));
    if (!m_pSpriteVB) return;

    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(m_pContext->Map(m_pSpriteVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    memcpy(map.pData, Verts.data(), Verts.size() * sizeof(SpriteVertex));
    m_pContext->Unmap(m_pSpriteVB.Get(), 0);

    SpriteCB cb;
    XMStoreFloat4x4(&cb.mViewProj, mViewProj);
    if (FAILED(m_pContext->Map(m_pSpriteCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    memcpy(map.pData, &cb, sizeof(cb));
    m_pContext->Unmap(m_pSpriteCB.Get(), 0);

    UINT uStride = sizeof(SpriteVertex);
    UINT uOffset = 0;
    m_pContext->IASetInputLayout(m_pSpriteLayout.Get());
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pContext->IASetVertexBuffers(0, 1, m_pSpriteVB.GetAddressOf(), &uStride, &uOffset);
    m_pContext->VSSetShader(m_pSpriteVS.Get(), nullptr, 0);
    m_pContext->VSSetConstantBuffers(0, 1, m_pSpriteCB.GetAddressOf());
    m_pContext->PSSetShader(m_pSpritePS.Get(), nullptr, 0);
    auto* pSRV = m_PieceAtlas.GetSRV();
    m_pContext->PSSetShaderResources(0, 1, &pSRV);
    m_pContext->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
    m_pContext->OMSetDepthStencilState(m_pDepthStateSprites.Get(), 0);
    float blendFactor[4]{};
    m_pContext->OMSetBlendState(m_pBlendAlpha.Get(), blendFactor, 0xffffffff);

    m_pContext->Draw(static_cast<UINT>(Verts.size()), 0);
}

// ---------------------------------------------------------------------------
// RenderTargetMarkers — draw a soft yellow disc on each legal-destination
// cell to make targets read clearly and give the user a larger pick target.
// Re-uses the sprite pipeline; only the bound texture differs.
// ---------------------------------------------------------------------------

void D3DBoardRenderer::RenderTargetMarkers(const XMMATRIX& mViewProj,
                                            const std::vector<CSCoord>& LegalDests) {
    if (LegalDests.empty()) return;
    if (!m_pTargetSRV) return;

    XMMATRIX mView = MakeView();
    XMVECTOR vDet;
    XMMATRIX mInvView = XMMatrixInverse(&vDet, mView);
    XMFLOAT3 vRight, vUp;
    XMStoreFloat3(&vRight, mInvView.r[0]);
    XMStoreFloat3(&vUp,    mInvView.r[1]);

    // Slightly larger than the piece quads (0.55) so the disc is visible
    // around any piece sitting on the target square.
    const float fHalf = 0.65f;

    std::vector<SpriteVertex> Verts;
    Verts.reserve(LegalDests.size() * 6);

    for (const auto& Dest : LegalDests) {
        if (!Dest.IsValid()) continue;
        XMFLOAT3 c = CellCenter(Dest);
        XMFLOAT3 r{ vRight.x * fHalf, vRight.y * fHalf, vRight.z * fHalf };
        XMFLOAT3 u{ vUp.x    * fHalf, vUp.y    * fHalf, vUp.z    * fHalf };
        XMFLOAT3 p00{ c.x - r.x - u.x, c.y - r.y - u.y, c.z - r.z - u.z };
        XMFLOAT3 p10{ c.x + r.x - u.x, c.y + r.y - u.y, c.z + r.z - u.z };
        XMFLOAT3 p01{ c.x - r.x + u.x, c.y - r.y + u.y, c.z - r.z + u.z };
        XMFLOAT3 p11{ c.x + r.x + u.x, c.y + r.y + u.y, c.z + r.z + u.z };
        Verts.push_back({ p01, { 0.0f, 0.0f } });
        Verts.push_back({ p11, { 1.0f, 0.0f } });
        Verts.push_back({ p00, { 0.0f, 1.0f } });
        Verts.push_back({ p11, { 1.0f, 0.0f } });
        Verts.push_back({ p10, { 1.0f, 1.0f } });
        Verts.push_back({ p00, { 0.0f, 1.0f } });
    }
    if (Verts.empty()) return;

    EnsureSpriteCapacity(static_cast<UINT>(Verts.size()));
    if (!m_pSpriteVB) return;

    D3D11_MAPPED_SUBRESOURCE map{};
    if (FAILED(m_pContext->Map(m_pSpriteVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    memcpy(map.pData, Verts.data(), Verts.size() * sizeof(SpriteVertex));
    m_pContext->Unmap(m_pSpriteVB.Get(), 0);

    SpriteCB cb;
    XMStoreFloat4x4(&cb.mViewProj, mViewProj);
    if (FAILED(m_pContext->Map(m_pSpriteCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) return;
    memcpy(map.pData, &cb, sizeof(cb));
    m_pContext->Unmap(m_pSpriteCB.Get(), 0);

    UINT uStride = sizeof(SpriteVertex);
    UINT uOffset = 0;
    m_pContext->IASetInputLayout(m_pSpriteLayout.Get());
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pContext->IASetVertexBuffers(0, 1, m_pSpriteVB.GetAddressOf(), &uStride, &uOffset);
    m_pContext->VSSetShader(m_pSpriteVS.Get(), nullptr, 0);
    m_pContext->VSSetConstantBuffers(0, 1, m_pSpriteCB.GetAddressOf());
    m_pContext->PSSetShader(m_pSpritePS.Get(), nullptr, 0);
    auto* pSRV = m_pTargetSRV.Get();
    m_pContext->PSSetShaderResources(0, 1, &pSRV);
    m_pContext->PSSetSamplers(0, 1, m_pSampler.GetAddressOf());
    m_pContext->OMSetDepthStencilState(m_pDepthStateSprites.Get(), 0);
    float blendFactor[4]{};
    m_pContext->OMSetBlendState(m_pBlendAlpha.Get(), blendFactor, 0xffffffff);

    m_pContext->Draw(static_cast<UINT>(Verts.size()), 0);
}

// ---------------------------------------------------------------------------
// Mouse / picking
// ---------------------------------------------------------------------------

bool D3DBoardRenderer::OnMouseDown(int nX, int nY) {
    m_bDragging   = true;
    m_bLastWasClick = false;
    m_nMouseX0    = m_nMouseX = nX;
    m_nMouseY0    = m_nMouseY = nY;
    SetCapture(m_hWnd);
    return true;
}

bool D3DBoardRenderer::OnMouseMove(int nX, int nY) {
    if (!m_bDragging) return false;
    int nDx = nX - m_nMouseX;
    int nDy = nY - m_nMouseY;
    m_nMouseX = nX;
    m_nMouseY = nY;

    const float kSens = 0.008f;
    m_fYaw   -= nDx * kSens;
    m_fPitch += nDy * kSens;

    // Clamp pitch a little inside ±pi/2 to avoid view-up degeneracy.
    const float kLim = 1.55f;
    if (m_fPitch >  kLim) m_fPitch =  kLim;
    if (m_fPitch < -kLim) m_fPitch = -kLim;

    InvalidateRect(m_hWnd, nullptr, FALSE);
    return true;
}

bool D3DBoardRenderer::OnMouseUp(int nX, int nY) {
    if (!m_bDragging) return false;
    ReleaseCapture();
    m_bDragging = false;
    int nDx = nX - m_nMouseX0;
    int nDy = nY - m_nMouseY0;
    m_bLastWasClick = (nDx * nDx + nDy * nDy) < (4 * 4);
    return true;
}

bool D3DBoardRenderer::OnMouseWheel(int nDelta) {
    float fFactor = std::pow(0.9f, static_cast<float>(nDelta) / 120.0f);
    m_fDistance *= fFactor;
    if (m_fDistance < 1.5f)  m_fDistance = 1.5f;
    if (m_fDistance > 500.f) m_fDistance = 500.f;
    InvalidateRect(m_hWnd, nullptr, FALSE);
    return true;
}

CSCoord D3DBoardRenderer::HitTest3D(int nX, int nY) const {
    if (!m_pDevice || m_Cells.empty()) {
        return CSCoord();
    }

    // Convert mouse to NDC.
    float fNdcX = (2.0f * nX / (float)m_nClientW) - 1.0f;
    float fNdcY = 1.0f - (2.0f * nY / (float)m_nClientH);

    XMMATRIX mView = MakeView();
    XMMATRIX mProj = MakeProj();
    XMMATRIX mVP   = mView * mProj;
    XMVECTOR vDet;
    XMMATRIX mInv  = XMMatrixInverse(&vDet, mVP);

    XMVECTOR vNear = XMVector3TransformCoord(XMVectorSet(fNdcX, fNdcY, 0.0f, 1.0f), mInv);
    XMVECTOR vFar  = XMVector3TransformCoord(XMVectorSet(fNdcX, fNdcY, 1.0f, 1.0f), mInv);
    XMVECTOR vDir  = XMVector3Normalize(XMVectorSubtract(vFar, vNear));

    XMFLOAT3 vOrigin, vDirF;
    XMStoreFloat3(&vOrigin, vNear);
    XMStoreFloat3(&vDirF,   vDir);

    // Ray-sphere intersect each cell. Cell octahedra in render space span
    // ±1 along each axis, but neighbouring cells sit at distance 1 (Z) or
    // sqrt(2) (file/rank) apart. Use radius 0.7 so the inscribed picking
    // sphere covers most of each cell's visible outline without significant
    // overlap into neighbours. Ties are broken by nearest hit (smallest t).
    const float kRadius = 0.7f;
    const float kRadiusSq = kRadius * kRadius;
    float fBestT = 1e9f;
    int   nBest  = -1;
    for (size_t i = 0; i < m_Cells.size(); ++i) {
        XMFLOAT3 c = m_Cells[i].Center;
        XMFLOAT3 oc{ vOrigin.x - c.x, vOrigin.y - c.y, vOrigin.z - c.z };
        float b = oc.x * vDirF.x + oc.y * vDirF.y + oc.z * vDirF.z;
        float cs = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - kRadiusSq;
        float disc = b * b - cs;
        if (disc < 0.0f) continue;
        float t = -b - std::sqrt(disc);
        if (t < 0.01f) t = -b + std::sqrt(disc);
        if (t < 0.01f) continue;
        if (t < fBestT) {
            fBestT = t;
            nBest  = static_cast<int>(i);
        }
    }
    if (nBest < 0) return CSCoord();
    return m_Cells[nBest].Coord;
}
