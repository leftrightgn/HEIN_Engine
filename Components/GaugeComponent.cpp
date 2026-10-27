#include "pch.h"
#include "GaugeComponent.h"
#include "DDSTextureLoader.h"
#include "HealthComponent.h"
#include "StaticModelComponent.h"
#include "WICTextureLoader.h"
#include "Camera/CameraController.h"
#include "Components/TransformComponent.h"
#include "DebugingTools/DebugUIManager.h"
#include "DebugingTools/EditorUtils.h"
#include "Effect/ReadData.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#include "ImGui/ImGuizmo.h"
#include <algorithm>
#include <d3dcompiler.h>
#include <filesystem>

// ============================================================================
// Helper: Create a 1x1 solid color texture for fallback rendering
// ============================================================================
namespace
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidTex(
        ID3D11Device* device, uint32_t abgr)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &abgr;
        initData.SysMemPitch = sizeof(uint32_t);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
        if (FAILED(hr)) return nullptr;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) return nullptr;
        return srv;
    }

    uint32_t Vec4ToABGR(const DirectX::SimpleMath::Vector4& c)
    {
        constexpr float kMaxChannel = 255.0f;
        uint8_t r = static_cast<uint8_t>(std::clamp(c.x, 0.0f, 1.0f) * kMaxChannel);
        uint8_t g = static_cast<uint8_t>(std::clamp(c.y, 0.0f, 1.0f) * kMaxChannel);
        uint8_t b = static_cast<uint8_t>(std::clamp(c.z, 0.0f, 1.0f) * kMaxChannel);
        uint8_t a = static_cast<uint8_t>(std::clamp(c.w, 0.0f, 1.0f) * kMaxChannel);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    void LoadTextureFromPath(ID3D11Device* device, const std::wstring& path,
                             Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
    {
        srv.Reset();
        if (path.empty()) return;
        
        // Shared cache: multiple actors use the same gauge frame/fill textures
        HEIN::TryLoadCachedTextureFile(device, path, srv.ReleaseAndGetAddressOf());
    }

    const char* GaugeTypeToStr(HEIN::GaugeType t)
    {
        switch (t)
        {
            case HEIN::GaugeType::HP:      return "HP";
            case HEIN::GaugeType::Stamina: return "Stamina";
            case HEIN::GaugeType::Dodge:   return "Dodge";
            case HEIN::GaugeType::Block:   return "Block";
            case HEIN::GaugeType::Custom:  return "Custom";
            default: return "Unknown";
        }
    }

    void GetTextureDimensions(ID3D11ShaderResourceView* srv, UINT& width, UINT& height)
    {
        width = 0;
        height = 0;
        if (!srv) return;

        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        srv->GetResource(res.GetAddressOf());
        if (!res) return;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
        if (SUCCEEDED(res.As(&tex2D)) && tex2D)
        {
            D3D11_TEXTURE2D_DESC desc;
            tex2D->GetDesc(&desc);
            width = desc.Width;
            height = desc.Height;
        }
    }
}

namespace HEIN
{

// ============================================================================
// Constructor
// ============================================================================
HEIN::GaugeComponent::GaugeComponent(Actor* owner)
    : IComponent(owner)
{
}

// ============================================================================
// EnsureWhiteTexture: Cache a persistent 1x1 white texture for solid color drawing
// ============================================================================
void HEIN::GaugeComponent::EnsureWhiteTexture(ID3D11Device* device)
{
    if (m_whiteTex || !device) return;

    uint32_t whitePixel = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &whitePixel;
    initData.SysMemPitch = sizeof(uint32_t);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        device->CreateShaderResourceView(tex.Get(), nullptr, m_whiteTex.GetAddressOf());
    }
}

// ============================================================================
// EnsureGaugeShader: Compile/load GaugeUI_PS.hlsl and create constant buffer + sampler
// ============================================================================
void HEIN::GaugeComponent::EnsureGaugeShader(ID3D11Device* device)
{
    if (!device) return;

    if (!m_gaugePixelShader)
    {
        std::vector<uint8_t> psData;
        bool loadedCso = false;
        try
        {
            psData = DX::ReadData(L"Resources/Shaders/GaugeUI_PS.cso");
            loadedCso = true;
        }
        catch (...)
        {
            try
            {
                psData = DX::ReadData(L"GaugeUI_PS.cso");
                loadedCso = true;
            }
            catch (...)
            {
                loadedCso = false;
            }
        }

        if (loadedCso && !psData.empty())
        {
            device->CreatePixelShader(psData.data(), psData.size(), nullptr, m_gaugePixelShader.ReleaseAndGetAddressOf());
        }
        else
        {
            Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
            HRESULT hr = D3DCompileFromFile(
                L"../External/Engine/Shaders/GaugeUI_PS.hlsl",
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                "main",
                "ps_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS,
                0,
                psBlob.GetAddressOf(),
                errorBlob.GetAddressOf()
            );
            if (FAILED(hr))
            {
                hr = D3DCompileFromFile(
                    L"External/Engine/Shaders/GaugeUI_PS.hlsl",
                    nullptr,
                    D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    "main",
                    "ps_5_0",
                    D3DCOMPILE_ENABLE_STRICTNESS,
                    0,
                    psBlob.GetAddressOf(),
                    errorBlob.GetAddressOf()
                );
            }
            if (SUCCEEDED(hr) && psBlob)
            {
                device->CreatePixelShader(
                    psBlob->GetBufferPointer(),
                    psBlob->GetBufferSize(),
                    nullptr,
                    m_gaugePixelShader.ReleaseAndGetAddressOf()
                );
            }
            else if (errorBlob)
            {
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            }
        }
    }

    if (!m_gaugeCB)
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = sizeof(CB_GaugeUI);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&cbDesc, nullptr, m_gaugeCB.ReleaseAndGetAddressOf());
    }

    if (!m_gaugeSampler)
    {
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.MaxAnisotropy = kDefaultMaxAnisotropy;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0.0f;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sampDesc, m_gaugeSampler.ReleaseAndGetAddressOf());
    }
}

void HEIN::GaugeComponent::UpdateBarValueTracking(const GaugeBarDef& bar)
{
    if (!bar.trailInitialized)
    {
        bar.prevValue = bar.currentValue;
        bar.trailValue = bar.currentValue;
        bar.trailInitialized = true;
        return;
    }

    if (bar.currentValue < bar.prevValue - kValueChangeEpsilon)
    {
        float delta = bar.prevValue - bar.currentValue;
        if (bar.trailValue <= bar.prevValue + kValueChangeEpsilon)
        {
            bar.trailHoldTimer = bar.trailHoldDelay;
        }
        bar.damageAccumulator += delta;
        bar.damageDisplayTimer = bar.damageDisplayDuration;
    }

    if (bar.currentValue > bar.trailValue)
    {
        bar.trailValue = bar.currentValue;
    }

    bar.prevValue = bar.currentValue;
}

// ============================================================================
// Initialize: Create SpriteBatch and load textures for all bars
// ============================================================================
void HEIN::GaugeComponent::Initialize(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    if (!m_spriteBatch && context)
    {
        m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    }

    EnsureWhiteTexture(device);
    EnsureGaugeShader(device);

    for (auto& bar : m_bars)
    {
        LoadBarTextures(device, bar);
        LoadBarFont(device, bar);
    }
}

static std::unordered_map<std::wstring, std::shared_ptr<DirectX::SpriteFont>> s_gaugeFontCache;

std::shared_ptr<DirectX::SpriteFont> HEIN::GaugeComponent::GetOrCreateFont(ID3D11Device* device, const std::wstring& path)
{
    if (!device || path.empty()) return nullptr;

    auto it = s_gaugeFontCache.find(path);
    if (it != s_gaugeFontCache.end() && it->second) return it->second;

    std::vector<std::wstring> candidates = {
        path,
        std::wstring(L"Dual/") + path,
        std::wstring(L"../Dual/") + path,
        std::wstring(L"../../Dual/Dual/") + path,
        std::wstring(L"../") + path
    };

    for (const auto& candidate : candidates)
    {
        try
        {
            auto font = std::make_shared<DirectX::SpriteFont>(device, candidate.c_str());
            if (font)
            {
                s_gaugeFontCache[path] = font;
                return font;
            }
        }
        catch (...)
        {
        }
    }
    return nullptr;
}

void HEIN::GaugeComponent::LoadBarFont(ID3D11Device* device, GaugeBarDef& bar)
{
    if (!device || bar.spriteFontPath.empty())
    {
        bar.spriteFont.reset();
        return;
    }
    bar.spriteFont = GetOrCreateFont(device, bar.spriteFontPath);
}

// ============================================================================
// LoadBarTextures: Load DDS/WIC textures for a single bar definition
// ============================================================================
void HEIN::GaugeComponent::LoadBarTextures(ID3D11Device* device, GaugeBarDef& bar)
{
    LoadTextureFromPath(device, bar.backgroundTexPath, bar.backgroundTex);
    LoadTextureFromPath(device, bar.fillTexPath, bar.fillTex);
    LoadTextureFromPath(device, bar.borderTexPath, bar.borderTex);
    LoadBarFont(device, bar);
}

// ============================================================================
// CreateSolidColorTexture: Creates a 1x1 pixel texture from a Vector4 color
// ============================================================================
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> HEIN::GaugeComponent::CreateSolidColorTexture(
    ID3D11Device* device, const DirectX::SimpleMath::Vector4& color)
{
    return CreateSolidTex(device, Vec4ToABGR(color));
}

// ============================================================================
// WorldToScreenPoint: Projects a 3D world position to 2D screen coordinates
// ============================================================================
bool HEIN::GaugeComponent::WorldToScreenPoint(
    GameContext& gameContext,
    const DirectX::SimpleMath::Vector3& worldPos,
    float& outX, float& outY)
{
    if (!gameContext.mainCamera) return false;

    DirectX::SimpleMath::Matrix view = gameContext.mainCamera->GetView();

    D3D11_VIEWPORT vp = {};
    UINT numVp = 1;
    gameContext.deviceResources.GetD3DDeviceContext()->RSGetViewports(&numVp, &vp);
    if (vp.Width <= 0.0f || vp.Height <= 0.0f)
    {
        vp = gameContext.deviceResources.GetScreenViewport();
    }
    float aspect = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : (kRefWidth / kRefHeight);
    float fov = gameContext.mainCamera->GetFov();
    if (fov <= 0.0f) fov = DirectX::XM_PIDIV4;

    constexpr float kDefaultNearClip = 0.1f;
    constexpr float kDefaultFarClip = 5000.0f;
    DirectX::SimpleMath::Matrix proj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
        fov, aspect, kDefaultNearClip, kDefaultFarClip);

    // Transform world -> clip space
    DirectX::SimpleMath::Matrix viewProj = view * proj;
    DirectX::SimpleMath::Vector4 clipPos = DirectX::SimpleMath::Vector4::Transform(
        DirectX::SimpleMath::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), viewProj);

    // Behind camera check
    constexpr float kMinClipW = 0.001f;
    if (clipPos.w <= kMinClipW) return false;

    // NDC
    float ndcX = clipPos.x / clipPos.w;
    float ndcY = clipPos.y / clipPos.w;

    // NDC to screen (reference canvas 1280x720)
    outX = (ndcX * 0.5f + 0.5f) * kRefWidth;
    outY = (-ndcY * 0.5f + 0.5f) * kRefHeight;

    return true;
}

// ============================================================================
// Helper: Get the parent enemy actor (gauge is a child actor of the enemy)
// ============================================================================
HEIN::Actor* HEIN::GaugeComponent::GetParentActor(GameContext& gameContext)
{
    if (!gameContext.actorManager) return nullptr;

    ActorID parentID = m_owner->GetParentID();
    if (parentID == INVALID_ACTOR_ID) return nullptr;

    return gameContext.actorManager->GetActor(parentID);
}

// ============================================================================
// Helper: Find the Player actor in the scene
// ============================================================================
HEIN::Actor* HEIN::GaugeComponent::FindPlayerActor(GameContext& gameContext)
{
    if (!gameContext.actorManager) return nullptr;

    for (auto& pair : gameContext.actorManager->GetAllActors())
    {
        if (pair.second && pair.second->GetActorType() == HEIN::ActorType::Player)
        {
            return pair.second.get();
        }
    }
    return nullptr;
}

// ============================================================================
// Helper: Get world position of the parent actor (or self if no parent)
// Used for WorldToScreen projection and distance checks.
// ============================================================================
DirectX::SimpleMath::Vector3 HEIN::GaugeComponent::GetTargetWorldPosition(GameContext& gameContext)
{
    // First try to use the parent enemy actor's position
    Actor* parentActor = GetParentActor(gameContext);
    Actor* targetActor = parentActor ? parentActor : m_owner;

    TransformComponent* transform = targetActor->GetComponent<TransformComponent>();
    if (transform)
    {
        return transform->GetWorldMatrix().Translation();
    }
    return DirectX::SimpleMath::Vector3::Zero;
}

// ============================================================================
// Start / Update
// ============================================================================
void HEIN::GaugeComponent::Start()
{
}

void HEIN::GaugeComponent::Update(float deltaTime)
{
    for (auto& bar : m_bars)
    {
        UpdateBarValueTracking(bar);

        if (bar.showDamageTrail)
        {
            if (bar.trailHoldTimer > 0.0f)
            {
                bar.trailHoldTimer = std::max(0.0f, bar.trailHoldTimer - deltaTime);
            }
            else if (bar.trailValue > bar.currentValue)
            {
                float diff = bar.trailValue - bar.currentValue;
                float step = (diff * bar.trailDrainSpeed + bar.maxValue * kMinTrailLinearDrainRatio) * deltaTime;
                bar.trailValue = std::max(bar.currentValue, bar.trailValue - step);
            }
        }
        else
        {
            bar.trailValue = bar.currentValue;
        }

        if (bar.damageDisplayTimer > 0.0f)
        {
            bar.damageDisplayTimer = std::max(0.0f, bar.damageDisplayTimer - deltaTime);
            if (bar.damageDisplayTimer <= 0.0f)
            {
                bar.damageAccumulator = 0.0f;
            }
        }
    }
}

// ============================================================================
// SyncBoundBars: Auto-sync bar values from data source components
// HP is pulled here (engine-side). Dodge/Block are pushed by game-side code.
// ============================================================================
void HEIN::GaugeComponent::SyncBoundBars(GameContext& gameContext)
{
    Actor* target = GetParentActor(gameContext);
    if (!target) target = m_owner;
    if (!target) return;

    for (auto& bar : m_bars)
    {
        if (!bar.autoBind) continue;

        if (bar.gaugeType == GaugeType::HP)
        {
            HealthComponent* health = target->GetComponent<HealthComponent>();
            if (health)
            {
                bar.currentValue = health->GetCurrentHealth();
                bar.maxValue = health->GetMaxHealth();
            }
        }
        else if (s_externalSyncHandler)
        {
            s_externalSyncHandler(target, bar.gaugeType, bar.currentValue, bar.maxValue);
        }

        UpdateBarValueTracking(bar);
    }
}

// ============================================================================
// Draw2D: Main rendering entry point (called by Actor::Draw2D)
// ============================================================================
void HEIN::GaugeComponent::Draw2D(GameContext& gameContext)
{
    if (!m_isVisible || m_bars.empty() || !gameContext.showUI || gameContext.isSceneViewPass) return;
    Draw(gameContext, DirectX::SimpleMath::Matrix::Identity,
         DirectX::SimpleMath::Matrix::Identity,
         DirectX::SimpleMath::Matrix::Identity);
}

void HEIN::GaugeComponent::Draw(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& /*world*/,
    const DirectX::SimpleMath::Matrix& /*view*/,
    const DirectX::SimpleMath::Matrix& /*proj*/)
{
    if (!m_isVisible || m_bars.empty() || !gameContext.showUI || gameContext.isSceneViewPass) return;

    // Auto-sync bound bar values from data source components
    SyncBoundBars(gameContext);

    // Filter ScreenSpace mode: Only the active controlled Player and Enemy boss draw ScreenSpace HUDs.
    if (m_anchorMode == GaugeAnchorMode::ScreenSpace)
    {
        if (m_owner)
        {
            HEIN::ActorType ownerType = m_owner->GetActorType();
            if (ownerType == HEIN::ActorType::Player)
            {
                // Only the currently active player actor draws the player HUD
                Actor* activePlayer = FindPlayerActor(gameContext);
                if (activePlayer && m_owner != activePlayer && (!gameContext.isEditorMode || HEIN::g_ActiveGizmoTarget != this))
                {
                    return; // Inactive or former player body - do not draw over active HUD
                }
            }
            else if (ownerType == HEIN::ActorType::Enemy)
            {
                // Enemy boss HUD is allowed
            }
            else
            {
                // Dormant vessels (ActorType::Default), corpses, props, Ghost, etc.
                // Do not draw a screen-space HUD unless actively being edited in editor.
                if (!gameContext.isEditorMode || HEIN::g_ActiveGizmoTarget != this)
                {
                    return;
                }
            }
        }
    }

    // --- Range-based activation check ---
    // When the gauge is a child of an enemy actor, only show it when
    // the player is within activation range of the parent enemy.
    if (m_useRangeActivation && !gameContext.isEditorMode)
    {
        Actor* playerActor = FindPlayerActor(gameContext);
        if (playerActor)
        {
            TransformComponent* playerTransform = playerActor->GetComponent<TransformComponent>();
            if (playerTransform)
            {
                DirectX::SimpleMath::Vector3 playerPos = playerTransform->GetWorldMatrix().Translation();
                DirectX::SimpleMath::Vector3 targetPos = GetTargetWorldPosition(gameContext);
                float distance = DirectX::SimpleMath::Vector3::Distance(playerPos, targetPos);

                // Hysteresis: use different thresholds for entering/leaving range
                // to prevent flickering when the player is near the boundary.
                if (m_isInRange)
                {
                    // Currently visible, hide when distance > deactivationRange
                    if (distance > m_deactivationRange)
                    {
                        m_isInRange = false;
                    }
                }
                else
                {
                    // Currently hidden, show when distance < activationRange
                    if (distance < m_activationRange)
                    {
                        m_isInRange = true;
                    }
                }
            }
        }
        else
        {
            // No player found: don't show enemy gauges
            m_isInRange = false;
        }

        if (!m_isInRange) return;
    }

    // Query current viewport for responsive scaling
    D3D11_VIEWPORT vp = {};
    if (gameContext.gameViewport.Width > 0.0f && gameContext.gameViewport.Height > 0.0f)
    {
        vp = gameContext.gameViewport;
    }
    else
    {
        UINT numVp = 1;
        gameContext.deviceResources.GetD3DDeviceContext()->RSGetViewports(&numVp, &vp);
        if (vp.Width <= 0.0f || vp.Height <= 0.0f)
        {
            vp = gameContext.deviceResources.GetScreenViewport();
        }
    }

    m_lastScaleX = vp.Width / kRefWidth;
    m_lastScaleY = vp.Height / kRefHeight;
    m_lastVpX = vp.TopLeftX;
    m_lastVpY = vp.TopLeftY;

    // Determine anchor position
    float anchorX = 0.0f;
    float anchorY = 0.0f;

    if (m_anchorMode == GaugeAnchorMode::ScreenSpace)
    {
        // Fixed screen position on reference canvas
        anchorX = m_screenPosition.x;
        anchorY = m_screenPosition.y;
    }
    else // WorldToScreen
    {
        // Use the PARENT enemy actor's world position + offset, not the child gauge actor
        DirectX::SimpleMath::Vector3 targetWorldPos = GetTargetWorldPosition(gameContext);
        DirectX::SimpleMath::Vector3 gaugeWorldPos = targetWorldPos + m_worldOffset;

        if (!WorldToScreenPoint(gameContext, gaugeWorldPos, anchorX, anchorY))
        {
            return; // Behind camera, don't draw
        }
    }

    // Draw each bar
    for (const auto& bar : m_bars)
    {
        DrawBar(gameContext, bar, anchorX, anchorY, m_lastScaleX, m_lastScaleY);
    }
}

// ============================================================================
// DrawBar: Renders a single gauge bar (background, fill, border, label)
// ============================================================================
void HEIN::GaugeComponent::DrawBar(
    GameContext& gameContext,
    const GaugeBarDef& bar,
    float anchorX, float anchorY,
    float scaleX, float scaleY)
{
    UpdateBarValueTracking(bar);

    // Compute screen-space rectangle
    float barX = (anchorX + bar.offset.x) * scaleX;
    float barY = (anchorY + bar.offset.y) * scaleY;
    float barW = bar.size.x * scaleX;
    float barH = bar.size.y * scaleY;

    // Calculate fill and delayed trail ratios
    float ratio = (bar.maxValue > 0.0f) ? std::clamp(bar.currentValue / bar.maxValue, 0.0f, 1.0f) : 0.0f;
    float trailRatio = (bar.maxValue > 0.0f) ? std::clamp(bar.trailValue / bar.maxValue, ratio, 1.0f) : ratio;

    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    if (!m_spriteBatch && context)
    {
        m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    }
    EnsureWhiteTexture(device);
    EnsureGaugeShader(device);

    if (device && ((!bar.fillTex && !bar.fillTexPath.empty()) ||
                   (!bar.backgroundTex && !bar.backgroundTexPath.empty()) ||
                   (!bar.borderTex && !bar.borderTexPath.empty()) ||
                   (!bar.spriteFont && !bar.spriteFontPath.empty())))
    {
        const_cast<GaugeComponent*>(this)->LoadBarTextures(device, const_cast<GaugeBarDef&>(bar));
    }

    // UI overlay uses DepthNone so it draws on top of 3D
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    DirectX::SimpleMath::Vector4 currentFillColor = bar.fillColor;
    if (ratio < bar.lowThreshold)
    {
        currentFillColor = bar.lowColor;
    }

    float startRatio = std::clamp(bar.fillStartPoint, 0.0f, 1.0f);
    float endRatio = std::clamp(bar.fillEndPoint, 0.0f, 1.0f);
    if (endRatio < startRatio) std::swap(startRatio, endRatio);

    if (m_spriteBatch)
    {
        if (bar.useEldenRingShader && m_gaugePixelShader && m_gaugeCB)
        {
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(context->Map(m_gaugeCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                CB_GaugeUI* cb = static_cast<CB_GaugeUI*>(mapped.pData);
                cb->fillColor = currentFillColor;
                cb->trailColor = bar.trailColor;
                cb->bgColor = bar.bgColor;
                cb->borderColor = bar.borderColor;
                cb->fillRatio = ratio;
                cb->trailRatio = bar.showDamageTrail ? trailRatio : ratio;
                cb->fillStart = startRatio;
                cb->fillEnd = endRatio;
                cb->barSizePixels = { barW, barH };
                cb->fillRightToLeft = bar.fillRightToLeft ? 1.0f : 0.0f;
                cb->hasFillTexture = bar.fillTex ? 1.0f : 0.0f;
                cb->hasBgTexture = bar.backgroundTex ? 1.0f : 0.0f;
                cb->clipTexture = bar.clipTexture ? 1.0f : 0.0f;
                cb->fillTexMatchesFrame = bar.fillTexMatchesFrame ? 1.0f : 0.0f;
                cb->useProceduralFrame = (bar.useProceduralFrame && !bar.borderTex) ? 1.0f : 0.0f;
                cb->bevelIntensity = bar.bevelIntensity;
                cb->meniscusGlow = bar.meniscusGlow;
                cb->padding0 = 0.0f;
                cb->padding1 = 0.0f;
                context->Unmap(m_gaugeCB.Get(), 0);
            }

            ID3D11ShaderResourceView* bgSrv = bar.backgroundTex ? bar.backgroundTex.Get() : m_whiteTex.Get();
            ID3D11ShaderResourceView* fTex = bar.fillTex ? bar.fillTex.Get() : m_whiteTex.Get();

            m_spriteBatch->Begin(
                DirectX::SpriteSortMode_Immediate,
                gameContext.commonStates.NonPremultiplied(),
                m_gaugeSampler.Get(),
                gameContext.commonStates.DepthNone(),
                nullptr,
                [&]()
                {
                    context->PSSetShader(m_gaugePixelShader.Get(), nullptr, 0);
                    context->PSSetConstantBuffers(0, 1, m_gaugeCB.GetAddressOf());
                    context->PSSetShaderResources(1, 1, &bgSrv);
                }
            );

            RECT fullRect;
            fullRect.left = static_cast<LONG>(barX);
            fullRect.top = static_cast<LONG>(barY);
            fullRect.right = static_cast<LONG>(barX + barW);
            fullRect.bottom = static_cast<LONG>(barY + barH);

            if (fTex)
            {
                m_spriteBatch->Draw(fTex, fullRect, nullptr, DirectX::Colors::White);
            }

            m_spriteBatch->End();

            ID3D11ShaderResourceView* nullSrv = nullptr;
            context->PSSetShaderResources(1, 1, &nullSrv);
        }

        m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());

        if (!bar.useEldenRingShader || !m_gaugePixelShader || !m_gaugeCB)
        {
            // Background
            {
                RECT bgRect;
                bgRect.left = static_cast<LONG>(barX);
                bgRect.top = static_cast<LONG>(barY);
                bgRect.right = static_cast<LONG>(barX + barW);
                bgRect.bottom = static_cast<LONG>(barY + barH);

                ID3D11ShaderResourceView* bgTex = bar.backgroundTex ? bar.backgroundTex.Get() : m_whiteTex.Get();
                if (bgTex)
                {
                    m_spriteBatch->Draw(bgTex, bgRect, nullptr,
                        DirectX::SimpleMath::Vector4(bar.bgColor.x, bar.bgColor.y, bar.bgColor.z, bar.bgColor.w));
                }
            }

            float startX = barX + barW * startRatio;
            float endX = barX + barW * endRatio;
            float activeW = std::max(0.0f, endX - startX);

            // Delayed Damage Trail fallback when custom shader is disabled
            if (bar.showDamageTrail && trailRatio > ratio && m_whiteTex)
            {
                float trailW = activeW * trailRatio;
                RECT trailRect;
                trailRect.top = static_cast<LONG>(barY);
                trailRect.bottom = static_cast<LONG>(barY + barH);
                if (bar.fillRightToLeft)
                {
                    trailRect.left = static_cast<LONG>(endX - trailW);
                    trailRect.right = static_cast<LONG>(endX);
                }
                else
                {
                    trailRect.left = static_cast<LONG>(startX);
                    trailRect.right = static_cast<LONG>(startX + trailW);
                }
                m_spriteBatch->Draw(m_whiteTex.Get(), trailRect, nullptr, bar.trailColor);
            }

            // Fill
            {
                float fillW = activeW * ratio;

                RECT fillRect;
                fillRect.top = static_cast<LONG>(barY);
                fillRect.bottom = static_cast<LONG>(barY + barH);

                if (bar.fillRightToLeft)
                {
                    fillRect.left = static_cast<LONG>(endX - fillW);
                    fillRect.right = static_cast<LONG>(endX);
                }
                else
                {
                    fillRect.left = static_cast<LONG>(startX);
                    fillRect.right = static_cast<LONG>(startX + fillW);
                }

                ID3D11ShaderResourceView* fTex = bar.fillTex ? bar.fillTex.Get() : m_whiteTex.Get();
                if (fTex && fillW > 0.0f)
                {
                    if (bar.clipTexture && bar.fillTex)
                    {
                        UINT texW = 0, texH = 0;
                        GetTextureDimensions(bar.fillTex.Get(), texW, texH);
                        if (texW > 0 && texH > 0)
                        {
                            float texStartX = bar.fillTexMatchesFrame ? (static_cast<float>(texW) * startRatio) : 0.0f;
                            float texEndX = bar.fillTexMatchesFrame ? (static_cast<float>(texW) * endRatio) : static_cast<float>(texW);
                            float texActiveW = std::max(0.0f, texEndX - texStartX);
                            float texFillW = texActiveW * ratio;

                            RECT srcRect;
                            srcRect.top = 0;
                            srcRect.bottom = static_cast<LONG>(texH);

                            if (bar.fillRightToLeft)
                            {
                                srcRect.left = static_cast<LONG>(texEndX - texFillW);
                                srcRect.right = static_cast<LONG>(texEndX);
                            }
                            else
                            {
                                srcRect.left = static_cast<LONG>(texStartX);
                                srcRect.right = static_cast<LONG>(texStartX + texFillW);
                            }

                            m_spriteBatch->Draw(fTex, fillRect, &srcRect, currentFillColor);
                        }
                        else
                        {
                            m_spriteBatch->Draw(fTex, fillRect, nullptr, currentFillColor);
                        }
                    }
                    else
                    {
                        m_spriteBatch->Draw(fTex, fillRect, nullptr, currentFillColor);
                    }
                }
            }
        }

        // Border/Frame overlay (drawn on top of fill)
        {
            if (bar.borderTex.Get())
            {
                RECT borderRect;
                borderRect.left = static_cast<LONG>(barX);
                borderRect.top = static_cast<LONG>(barY);
                borderRect.right = static_cast<LONG>(barX + barW);
                borderRect.bottom = static_cast<LONG>(barY + barH);

                m_spriteBatch->Draw(bar.borderTex.Get(), borderRect, nullptr,
                    DirectX::SimpleMath::Vector4(bar.borderColor.x, bar.borderColor.y, bar.borderColor.z, bar.borderColor.w));
            }
        }

        // Draw text using custom SpriteFont if available
        if (bar.spriteFont && gameContext.showUI && !gameContext.isSceneViewPass)
        {
            float uiScale = std::min(scaleX, scaleY);
            float fontScale = bar.fontSize * uiScale * 0.75f;
            DirectX::SimpleMath::Vector4 textCol = bar.labelColor;
            DirectX::SimpleMath::Vector4 shadowCol = { 0.0f, 0.0f, 0.0f, 0.85f };

            if (bar.showLabel && !bar.name.empty())
            {
                int len = MultiByteToWideChar(CP_UTF8, 0, bar.name.c_str(), -1, nullptr, 0);
                std::wstring wname(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, bar.name.c_str(), -1, &wname[0], len);
                if (!wname.empty() && wname.back() == L'\0') wname.pop_back();

                DirectX::SimpleMath::Vector2 labelPos(
                    barX + (4.0f + bar.textOffset.x) * scaleX,
                    barY + (barH * 0.5f) - (10.0f * fontScale) + bar.textOffset.y * scaleY
                );
                // Drop shadow
                bar.spriteFont->DrawString(m_spriteBatch.get(), wname.c_str(),
                    DirectX::SimpleMath::Vector2(labelPos.x + 1.5f, labelPos.y + 1.5f),
                    shadowCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
                bar.spriteFont->DrawString(m_spriteBatch.get(), wname.c_str(),
                    labelPos, textCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
            }

            if (bar.showValue)
            {
                wchar_t valBuf[64];
                swprintf_s(valBuf, L"%.0f/%.0f", bar.currentValue, bar.maxValue);
                DirectX::SimpleMath::Vector2 valMetrics = bar.spriteFont->MeasureString(valBuf);
                float valWidth = valMetrics.x * fontScale;

                DirectX::SimpleMath::Vector2 valPos(
                    barX + barW - valWidth - 4.0f * scaleX + bar.textOffset.x * scaleX,
                    barY + (barH * 0.5f) - (10.0f * fontScale) + bar.textOffset.y * scaleY
                );
                // Drop shadow
                bar.spriteFont->DrawString(m_spriteBatch.get(), valBuf,
                    DirectX::SimpleMath::Vector2(valPos.x + 1.5f, valPos.y + 1.5f),
                    shadowCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
                bar.spriteFont->DrawString(m_spriteBatch.get(), valBuf,
                    valPos, textCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
            }

            // Elden Ring Combo Damage Counter above right edge of bar
            if (bar.showDamageNumbers && bar.gaugeType == GaugeType::HP &&
                bar.damageDisplayTimer > 0.0f && bar.damageAccumulator >= 1.0f)
            {
                float dmgAlpha = std::clamp(bar.damageDisplayTimer / kDamageFadeOutWindow, 0.0f, 1.0f);
                wchar_t dmgBuf[32];
                swprintf_s(dmgBuf, L"%.0f", bar.damageAccumulator);
                DirectX::SimpleMath::Vector2 dmgMetrics = bar.spriteFont->MeasureString(dmgBuf);
                float dmgScale = fontScale * 0.9f;
                float dmgWidth = dmgMetrics.x * dmgScale;

                DirectX::SimpleMath::Vector2 dmgPos(
                    barX + barW - dmgWidth - 4.0f * scaleX,
                    barY - (20.0f * scaleY)
                );
                DirectX::SimpleMath::Vector4 dmgCol(0.96f, 0.92f, 0.80f, dmgAlpha);
                DirectX::SimpleMath::Vector4 dmgShadow(0.0f, 0.0f, 0.0f, 0.85f * dmgAlpha);

                bar.spriteFont->DrawString(m_spriteBatch.get(), dmgBuf,
                    DirectX::SimpleMath::Vector2(dmgPos.x + 1.5f, dmgPos.y + 1.5f),
                    dmgShadow, 0.0f, DirectX::SimpleMath::Vector2::Zero, dmgScale);
                bar.spriteFont->DrawString(m_spriteBatch.get(), dmgBuf,
                    dmgPos, dmgCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, dmgScale);
            }
        }

        m_spriteBatch->End();
    }

    // Reset depth state
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    // Label and Value text fallback using ImGui draw list (if no custom SpriteFont loaded)
    if (!gameContext.showUI || gameContext.isSceneViewPass) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList && !bar.spriteFont)
    {
        float uiScale = std::min(scaleX, scaleY);
        float fontSize = ImGui::GetFontSize() * bar.fontSize * uiScale;

        ImU32 labelCol = IM_COL32(
            static_cast<int>(bar.labelColor.x * kMaxColorChannelValue),
            static_cast<int>(bar.labelColor.y * kMaxColorChannelValue),
            static_cast<int>(bar.labelColor.z * kMaxColorChannelValue),
            static_cast<int>(bar.labelColor.w * kMaxColorChannelValue)
        );

        if (bar.showLabel)
        {
            // Label: drawn to the left of the bar
            ImVec2 labelPos(
                m_lastVpX + barX + (4.0f + bar.textOffset.x) * scaleX,
                m_lastVpY + barY + (barH - fontSize) * 0.5f + bar.textOffset.y * scaleY
            );

            // Drop shadow
            drawList->AddText(ImGui::GetFont(), fontSize,
                ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f),
                IM_COL32(0, 0, 0, 180), bar.name.c_str());
            drawList->AddText(ImGui::GetFont(), fontSize, labelPos, labelCol, bar.name.c_str());
        }

        if (bar.showValue)
        {
            // Value: drawn at the right side of the bar
            char valBuf[64];
            sprintf_s(valBuf, "%.0f/%.0f", bar.currentValue, bar.maxValue);

            ImVec2 baseValSize = ImGui::CalcTextSize(valBuf);
            float scaledValWidth = baseValSize.x * (fontSize / ImGui::GetFontSize());

            ImVec2 valPos(
                m_lastVpX + barX + barW - scaledValWidth - 4.0f * scaleX + bar.textOffset.x * scaleX,
                m_lastVpY + barY + (barH - fontSize) * 0.5f + bar.textOffset.y * scaleY
            );

            // Drop shadow
            drawList->AddText(ImGui::GetFont(), fontSize,
                ImVec2(valPos.x + 1.0f, valPos.y + 1.0f),
                IM_COL32(0, 0, 0, 180), valBuf);
            drawList->AddText(ImGui::GetFont(), fontSize, valPos, labelCol, valBuf);
        }

        // Elden Ring Combo Damage Counter fallback via ImGui when no SpriteFont is bound
        if (bar.showDamageNumbers && bar.gaugeType == GaugeType::HP &&
            bar.damageDisplayTimer > 0.0f && bar.damageAccumulator >= 1.0f)
        {
            float dmgAlpha = std::clamp(bar.damageDisplayTimer / kDamageFadeOutWindow, 0.0f, 1.0f);
            char dmgBuf[32];
            sprintf_s(dmgBuf, "%.0f", bar.damageAccumulator);
            ImVec2 baseDmgSize = ImGui::CalcTextSize(dmgBuf);
            float scaledDmgWidth = baseDmgSize.x * (fontSize / ImGui::GetFontSize());

            ImVec2 dmgPos(
                m_lastVpX + barX + barW - scaledDmgWidth - 4.0f * scaleX,
                m_lastVpY + barY - fontSize - 4.0f * scaleY
            );
            ImU32 dmgCol = IM_COL32(245, 235, 205, static_cast<int>(dmgAlpha * kMaxColorChannelValue));
            ImU32 dmgShadow = IM_COL32(0, 0, 0, static_cast<int>(dmgAlpha * 200.0f));

            drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(dmgPos.x + 1.0f, dmgPos.y + 1.0f), dmgShadow, dmgBuf);
            drawList->AddText(ImGui::GetFont(), fontSize, dmgPos, dmgCol, dmgBuf);
        }

        // If no border texture and procedural frame is disabled, draw a thin rectangle border via ImGui
        if (!bar.borderTex.Get() && (!bar.useEldenRingShader || !bar.useProceduralFrame))
        {
            ImU32 bCol = IM_COL32(
                static_cast<int>(bar.borderColor.x * kMaxColorChannelValue),
                static_cast<int>(bar.borderColor.y * kMaxColorChannelValue),
                static_cast<int>(bar.borderColor.z * kMaxColorChannelValue),
                static_cast<int>(bar.borderColor.w * kMaxColorChannelValue)
            );
            drawList->AddRect(
                ImVec2(m_lastVpX + barX, m_lastVpY + barY),
                ImVec2(m_lastVpX + barX + barW, m_lastVpY + barY + barH),
                bCol, 0.0f, 0, 1.5f
            );
        }
    }
}

// ============================================================================
// Runtime API
// ============================================================================
int HEIN::GaugeComponent::AddBar(const GaugeBarDef& bar)
{
    m_bars.push_back(bar);
    return static_cast<int>(m_bars.size()) - 1;
}

void HEIN::GaugeComponent::RemoveBar(int index)
{
    if (index >= 0 && index < static_cast<int>(m_bars.size()))
    {
        m_bars.erase(m_bars.begin() + index);
    }
}

void HEIN::GaugeComponent::RemoveBarTextures(int index, bool resetFillRange)
{
    if (index >= 0 && index < static_cast<int>(m_bars.size()))
    {
        m_bars[index].RemoveAllTextures(resetFillRange);
    }
}

void HEIN::GaugeComponent::RemoveAllBarsTextures(bool resetFillRange)
{
    for (auto& bar : m_bars)
    {
        bar.RemoveAllTextures(resetFillRange);
    }
}

HEIN::GaugeBarDef* HEIN::GaugeComponent::GetBar(int index)
{
    if (index >= 0 && index < static_cast<int>(m_bars.size()))
    {
        return &m_bars[index];
    }
    return nullptr;
}

HEIN::GaugeBarDef* HEIN::GaugeComponent::FindBar(const std::string& name)
{
    for (auto& bar : m_bars)
    {
        if (bar.name == name) return &bar;
    }
    return nullptr;
}

void HEIN::GaugeComponent::SetValue(GaugeType type, float current, float max)
{
    for (auto& bar : m_bars)
    {
        if (bar.gaugeType == type)
        {
            bar.currentValue = current;
            if (max >= 0.0f) bar.maxValue = max;
            UpdateBarValueTracking(bar);
            return;
        }
    }
}

void HEIN::GaugeComponent::SetValue(const std::string& name, float current, float max)
{
    GaugeBarDef* bar = FindBar(name);
    if (bar)
    {
        bar->currentValue = current;
        if (max >= 0.0f) bar->maxValue = max;
        UpdateBarValueTracking(*bar);
    }
}

// ============================================================================
// Serialize: Save all gauge data to JSON
// ============================================================================
nlohmann::json HEIN::GaugeComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();

    data["AnchorMode"] = static_cast<int>(m_anchorMode);
    data["ScreenPosX"] = m_screenPosition.x;
    data["ScreenPosY"] = m_screenPosition.y;
    data["WorldOffsetX"] = m_worldOffset.x;
    data["WorldOffsetY"] = m_worldOffset.y;
    data["WorldOffsetZ"] = m_worldOffset.z;
    data["IsVisible"] = m_isVisible;
    data["UseRangeActivation"] = m_useRangeActivation;
    data["ActivationRange"] = m_activationRange;
    data["DeactivationRange"] = m_deactivationRange;

    nlohmann::json barsArray = nlohmann::json::array();
    for (const auto& bar : m_bars)
    {
        nlohmann::json b;
        b["Name"] = bar.name;
        b["GaugeType"] = static_cast<int>(bar.gaugeType);
        b["CurrentValue"] = bar.currentValue;
        b["MaxValue"] = bar.maxValue;
        b["OffsetX"] = bar.offset.x;
        b["OffsetY"] = bar.offset.y;
        b["SizeX"] = bar.size.x;
        b["SizeY"] = bar.size.y;
        b["FillRightToLeft"] = bar.fillRightToLeft;
        b["FillStartPoint"] = bar.fillStartPoint;
        b["FillEndPoint"] = bar.fillEndPoint;
        b["ClipTexture"] = bar.clipTexture;
        b["FillTexMatchesFrame"] = bar.fillTexMatchesFrame;
        b["AutoBind"] = bar.autoBind;

        b["FillColorR"] = bar.fillColor.x;
        b["FillColorG"] = bar.fillColor.y;
        b["FillColorB"] = bar.fillColor.z;
        b["FillColorA"] = bar.fillColor.w;

        b["BgColorR"] = bar.bgColor.x;
        b["BgColorG"] = bar.bgColor.y;
        b["BgColorB"] = bar.bgColor.z;
        b["BgColorA"] = bar.bgColor.w;

        b["BorderColorR"] = bar.borderColor.x;
        b["BorderColorG"] = bar.borderColor.y;
        b["BorderColorB"] = bar.borderColor.z;
        b["BorderColorA"] = bar.borderColor.w;

        b["LowThreshold"] = bar.lowThreshold;
        b["LowColorR"] = bar.lowColor.x;
        b["LowColorG"] = bar.lowColor.y;
        b["LowColorB"] = bar.lowColor.z;
        b["LowColorA"] = bar.lowColor.w;

        b["UseEldenRingShader"] = bar.useEldenRingShader;
        b["ShowDamageTrail"] = bar.showDamageTrail;
        b["TrailColorR"] = bar.trailColor.x;
        b["TrailColorG"] = bar.trailColor.y;
        b["TrailColorB"] = bar.trailColor.z;
        b["TrailColorA"] = bar.trailColor.w;
        b["TrailHoldDelay"] = bar.trailHoldDelay;
        b["TrailDrainSpeed"] = bar.trailDrainSpeed;
        b["BevelIntensity"] = bar.bevelIntensity;
        b["MeniscusGlow"] = bar.meniscusGlow;
        b["UseProceduralFrame"] = bar.useProceduralFrame;
        b["ShowDamageNumbers"] = bar.showDamageNumbers;
        b["DamageDisplayDuration"] = bar.damageDisplayDuration;

        b["BackgroundTexPath"] = std::string(bar.backgroundTexPath.begin(), bar.backgroundTexPath.end());
        b["FillTexPath"] = std::string(bar.fillTexPath.begin(), bar.fillTexPath.end());
        b["BorderTexPath"] = std::string(bar.borderTexPath.begin(), bar.borderTexPath.end());

        b["ShowLabel"] = bar.showLabel;
        b["ShowValue"] = bar.showValue;
        b["FontSize"] = bar.fontSize;
        b["LabelColorR"] = bar.labelColor.x;
        b["LabelColorG"] = bar.labelColor.y;
        b["LabelColorB"] = bar.labelColor.z;
        b["LabelColorA"] = bar.labelColor.w;
        b["TextOffsetX"] = bar.textOffset.x;
        b["TextOffsetY"] = bar.textOffset.y;
        if (!bar.spriteFontPath.empty())
        {
            int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, bar.spriteFontPath.c_str(), static_cast<int>(bar.spriteFontPath.length()), nullptr, 0, nullptr, nullptr);
            std::string s(sizeNeeded, 0);
            WideCharToMultiByte(CP_UTF8, 0, bar.spriteFontPath.c_str(), static_cast<int>(bar.spriteFontPath.length()), &s[0], sizeNeeded, nullptr, nullptr);
            b["SpriteFontPath"] = s;
        }
        else
        {
            b["SpriteFontPath"] = "";
        }

        barsArray.push_back(b);
    }
    data["Bars"] = barsArray;

    return data;
}

// ============================================================================
// Deserialize: Load gauge data from JSON
// ============================================================================
void HEIN::GaugeComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);

    if (data.contains("AnchorMode")) m_anchorMode = static_cast<GaugeAnchorMode>(data["AnchorMode"].get<int>());
    if (data.contains("ScreenPosX")) m_screenPosition.x = data["ScreenPosX"];
    if (data.contains("ScreenPosY")) m_screenPosition.y = data["ScreenPosY"];
    if (data.contains("WorldOffsetX")) m_worldOffset.x = data["WorldOffsetX"];
    if (data.contains("WorldOffsetY")) m_worldOffset.y = data["WorldOffsetY"];
    if (data.contains("WorldOffsetZ")) m_worldOffset.z = data["WorldOffsetZ"];
    if (data.contains("IsVisible")) m_isVisible = data["IsVisible"];
    if (data.contains("UseRangeActivation")) m_useRangeActivation = data["UseRangeActivation"];
    if (data.contains("ActivationRange")) m_activationRange = data["ActivationRange"];
    if (data.contains("DeactivationRange")) m_deactivationRange = data["DeactivationRange"];

    m_bars.clear();
    if (data.contains("Bars"))
    {
        for (const auto& b : data["Bars"])
        {
            GaugeBarDef bar;
            if (b.contains("Name")) bar.name = b["Name"];
            if (b.contains("GaugeType")) bar.gaugeType = static_cast<GaugeType>(b["GaugeType"].get<int>());
            if (bar.gaugeType == GaugeType::Stamina && bar.name == "Dodge")
            {
                bar.name = "Stamina";
            }
            if (b.contains("CurrentValue")) bar.currentValue = b["CurrentValue"];
            if (b.contains("MaxValue")) bar.maxValue = b["MaxValue"];
            if (b.contains("OffsetX")) bar.offset.x = b["OffsetX"];
            if (b.contains("OffsetY")) bar.offset.y = b["OffsetY"];
            if (b.contains("SizeX")) bar.size.x = b["SizeX"];
            if (b.contains("SizeY")) bar.size.y = b["SizeY"];
            if (b.contains("FillRightToLeft")) bar.fillRightToLeft = b["FillRightToLeft"];
            if (b.contains("FillStartPoint")) bar.fillStartPoint = b["FillStartPoint"];
            if (b.contains("FillEndPoint")) bar.fillEndPoint = b["FillEndPoint"];
            if (b.contains("ClipTexture")) bar.clipTexture = b["ClipTexture"];
            if (b.contains("FillTexMatchesFrame")) bar.fillTexMatchesFrame = b["FillTexMatchesFrame"];
            if (b.contains("AutoBind")) bar.autoBind = b["AutoBind"];

            if (b.contains("FillColorR"))
            {
                bar.fillColor.x = b["FillColorR"];
                bar.fillColor.y = b["FillColorG"];
                bar.fillColor.z = b["FillColorB"];
                bar.fillColor.w = b.value("FillColorA", 1.0f);
            }
            if (b.contains("BgColorR"))
            {
                bar.bgColor.x = b["BgColorR"];
                bar.bgColor.y = b["BgColorG"];
                bar.bgColor.z = b["BgColorB"];
                bar.bgColor.w = b.value("BgColorA", 0.8f);
            }
            if (b.contains("BorderColorR"))
            {
                bar.borderColor.x = b["BorderColorR"];
                bar.borderColor.y = b["BorderColorG"];
                bar.borderColor.z = b["BorderColorB"];
                bar.borderColor.w = b.value("BorderColorA", 1.0f);
            }
            if (b.contains("LowThreshold")) bar.lowThreshold = b["LowThreshold"];
            if (b.contains("LowColorR"))
            {
                bar.lowColor.x = b["LowColorR"];
                bar.lowColor.y = b["LowColorG"];
                bar.lowColor.z = b["LowColorB"];
                bar.lowColor.w = b.value("LowColorA", 1.0f);
            }

            if (b.contains("UseEldenRingShader")) bar.useEldenRingShader = b["UseEldenRingShader"];
            if (b.contains("ShowDamageTrail")) bar.showDamageTrail = b["ShowDamageTrail"];
            if (b.contains("TrailColorR"))
            {
                bar.trailColor.x = b["TrailColorR"];
                bar.trailColor.y = b["TrailColorG"];
                bar.trailColor.z = b["TrailColorB"];
                bar.trailColor.w = b.value("TrailColorA", 0.95f);
            }
            if (b.contains("TrailHoldDelay")) bar.trailHoldDelay = b["TrailHoldDelay"];
            if (b.contains("TrailDrainSpeed")) bar.trailDrainSpeed = b["TrailDrainSpeed"];
            if (b.contains("BevelIntensity")) bar.bevelIntensity = b["BevelIntensity"];
            if (b.contains("MeniscusGlow")) bar.meniscusGlow = b["MeniscusGlow"];
            if (b.contains("UseProceduralFrame")) bar.useProceduralFrame = b["UseProceduralFrame"];
            if (b.contains("ShowDamageNumbers")) bar.showDamageNumbers = b["ShowDamageNumbers"];
            if (b.contains("DamageDisplayDuration")) bar.damageDisplayDuration = b["DamageDisplayDuration"];

            if (b.contains("BackgroundTexPath"))
            {
                std::string p = b["BackgroundTexPath"];
                bar.backgroundTexPath = std::wstring(p.begin(), p.end());
            }
            if (b.contains("FillTexPath"))
            {
                std::string p = b["FillTexPath"];
                bar.fillTexPath = std::wstring(p.begin(), p.end());
            }
            if (b.contains("BorderTexPath"))
            {
                std::string p = b["BorderTexPath"];
                bar.borderTexPath = std::wstring(p.begin(), p.end());
            }

            if (b.contains("ShowLabel")) bar.showLabel = b["ShowLabel"];
            if (b.contains("ShowValue")) bar.showValue = b["ShowValue"];
            if (b.contains("FontSize")) bar.fontSize = b["FontSize"];
            if (b.contains("LabelColorR"))
            {
                bar.labelColor.x = b["LabelColorR"];
                bar.labelColor.y = b["LabelColorG"];
                bar.labelColor.z = b["LabelColorB"];
                bar.labelColor.w = b.value("LabelColorA", 1.0f);
            }
            if (b.contains("TextOffsetX")) bar.textOffset.x = b["TextOffsetX"];
            if (b.contains("TextOffsetY")) bar.textOffset.y = b["TextOffsetY"];
            if (b.contains("SpriteFontPath"))
            {
                std::string s = b["SpriteFontPath"];
                if (!s.empty())
                {
                    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.length()), nullptr, 0);
                    std::wstring ws(sizeNeeded, 0);
                    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.length()), &ws[0], sizeNeeded);
                    bar.spriteFontPath = ws;
                }
            }

            m_bars.push_back(bar);
        }
    }
}

// ============================================================================
// InitializeAfterDeserialize: Load textures after JSON load
// ============================================================================
void HEIN::GaugeComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    Initialize(gameContext);
}

// ============================================================================
// OnInspectorGUI: ImGui editor panel (Unity-style inspector for gauge bars)
// ============================================================================
void HEIN::GaugeComponent::OnInspectorGUI(GameContext& gameContext)
{
    HWND windowHandle = gameContext.deviceResources.GetWindow();

    bool isActive = (HEIN::g_ActiveGizmoTarget == this);
    if (ImGui::RadioButton("Edit Gauge with Gizmo", isActive))
    {
        HEIN::g_ActiveGizmoTarget = this;
    }

    if (ImGui::CollapsingHeader("Gauge Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Visible##Gauge", &m_isVisible);

        // Anchor Mode
        const char* anchorModes[] = { "Screen Space (Player HUD)", "World To Screen (Enemy Overhead)" };
        int anchorIdx = static_cast<int>(m_anchorMode);
        if (ImGui::Combo("Anchor Mode", &anchorIdx, anchorModes, IM_ARRAYSIZE(anchorModes)))
        {
            m_anchorMode = static_cast<GaugeAnchorMode>(anchorIdx);
        }

        if (m_anchorMode == GaugeAnchorMode::ScreenSpace)
        {
            ImGui::DragFloat2("Screen Position (1280x720)", &m_screenPosition.x, 1.0f);
        }
        else
        {
            ImGui::DragFloat3("World Offset (X, Y, Z)", &m_worldOffset.x, 0.05f);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), 
                "Tip: Y=2.5 places the bar above the actor's head");
        }

        // --- Range-based activation ---
        ImGui::Separator();
        ImGui::Text("Range-Based Activation:");
        ImGui::Checkbox("Use Range Activation", &m_useRangeActivation);

        if (m_useRangeActivation)
        {
            ImGui::DragFloat("Activation Range", &m_activationRange, 0.5f, 0.0f, 500.0f, "%.1f units");
            ImGui::DragFloat("Deactivation Range", &m_deactivationRange, 0.5f, 0.0f, 500.0f, "%.1f units");

            // Auto-clamp: deactivation should always be >= activation to provide hysteresis
            if (m_deactivationRange < m_activationRange)
            {
                m_deactivationRange = m_activationRange + kHysteresisMargin;
            }

            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "Gauge appears when player enters %.0f units,\nhides when player leaves %.0f units.",
                m_activationRange, m_deactivationRange);

            // Live status indicator
            if (m_isInRange)
            {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Status: IN RANGE (visible)");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Status: OUT OF RANGE (hidden)");
            }
        }

        ImGui::Separator();
        ImGui::Text("Gauge Bars: %d", static_cast<int>(m_bars.size()));

        // Add new bar buttons
        if (ImGui::Button("+ Add HP Bar"))
        {
            GaugeBarDef bar;
            bar.name = "HP";
            bar.gaugeType = GaugeType::HP;
            bar.fillColor = { 0.9f, 0.2f, 0.2f, 1.0f };
            bar.backgroundTexPath = L"Resources/Textures/nborder.dds";
            bar.borderTexPath = L"Resources/Textures/border.png.dds";
            bar.fillTexPath = L"Resources/Textures/fill.dds";
            bar.offset = { 0.0f, -579.0f };
            bar.size = { 241.0f, 46.0f };
            bar.fillStartPoint = 0.2173f;
            bar.fillEndPoint = 1.0f;
            bar.clipTexture = true;
            bar.fillTexMatchesFrame = true;
            bar.autoBind = true;
            m_bars.push_back(bar);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Stamina Bar"))
        {
            GaugeBarDef bar;
            bar.name = "Stamina";
            bar.gaugeType = GaugeType::Stamina;
            bar.fillColor = { 0.2f, 0.85f, 0.3f, 1.0f };
            bar.backgroundTexPath = L"Resources/Textures/nborderg.dds";
            bar.borderTexPath = L"Resources/Textures/borderg.dds";
            bar.fillTexPath = L"Resources/Textures/fillg.dds";
            bar.offset = { 26.0f, -547.0f };
            bar.size = { 181.0f, 46.0f };
            bar.fillStartPoint = 0.2173f;
            bar.fillEndPoint = 1.0f;
            bar.clipTexture = true;
            bar.fillTexMatchesFrame = true;
            bar.autoBind = true;
            m_bars.push_back(bar);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Dodge Bar"))
        {
            GaugeBarDef bar;
            bar.name = "Dodge";
            bar.gaugeType = GaugeType::Dodge;
            bar.fillColor = { 0.3f, 0.5f, 0.9f, 1.0f };
            bar.offset = { 0.0f, static_cast<float>(m_bars.size()) * 28.0f };
            m_bars.push_back(bar);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Block Bar"))
        {
            GaugeBarDef bar;
            bar.name = "Block";
            bar.gaugeType = GaugeType::Block;
            bar.fillColor = { 0.85f, 0.65f, 0.2f, 1.0f };
            bar.offset = { 0.0f, static_cast<float>(m_bars.size()) * 28.0f };
            m_bars.push_back(bar);
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Add Custom"))
        {
            GaugeBarDef bar;
            bar.name = "Custom";
            bar.gaugeType = GaugeType::Custom;
            bar.offset = { 0.0f, static_cast<float>(m_bars.size()) * 28.0f };
            m_bars.push_back(bar);
        }

        ImGui::Separator();

        // Per-bar inspector
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(m_bars.size()); i++)
        {
            GaugeBarDef& bar = m_bars[i];
            ImGui::PushID(i);

            char headerLabel[128];
            sprintf_s(headerLabel, "[%d] %s (%s)###Bar%d", i, bar.name.c_str(), GaugeTypeToStr(bar.gaugeType), i);

            if (ImGui::CollapsingHeader(headerLabel))
            {
                // Name & Type
                ImGui::InputText("Name", &bar.name);
                const char* typeNames[] = { "HP", "Stamina", "Dodge", "Block", "Custom" };
                int typeIdx = static_cast<int>(bar.gaugeType);
                if (ImGui::Combo("Gauge Type", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames)))
                {
                    bar.gaugeType = static_cast<GaugeType>(typeIdx);
                }

                // Values
                if (bar.autoBind && bar.gaugeType == GaugeType::HP)
                {
                    Actor* target = GetParentActor(gameContext);
                    if (!target) target = m_owner;
                    HealthComponent* health = target ? target->GetComponent<HealthComponent>() : nullptr;
                    if (health)
                    {
                        float curHp = health->GetCurrentHealth();
                        float maxHp = health->GetMaxHealth();
                        if (ImGui::DragFloat("Current Health (Syncs with HealthComponent)", &curHp, 1.0f, 0.0f, maxHp, "%.0f"))
                        {
                            health->SetCurrentHealth(curHp);
                            bar.currentValue = curHp;
                        }
                        if (ImGui::DragFloat("Max Health (Syncs with HealthComponent)", &maxHp, 1.0f, 1.0f, 99999.0f, "%.0f"))
                        {
                            health->SetMaxHealth(maxHp);
                            bar.maxValue = maxHp;
                        }
                    }
                    else
                    {
                        ImGui::DragFloat("Current Value", &bar.currentValue, 1.0f, 0.0f, bar.maxValue, "%.0f");
                        ImGui::DragFloat("Max Value", &bar.maxValue, 1.0f, 1.0f, 99999.0f, "%.0f");
                    }
                }
                else
                {
                    ImGui::DragFloat("Current Value", &bar.currentValue, 0.5f, 0.0f, bar.maxValue);
                    ImGui::DragFloat("Max Value", &bar.maxValue, 0.5f, 1.0f, 99999.0f);
                }

                // Layout
                ImGui::DragFloat2("Offset (X, Y)", &bar.offset.x, 1.0f);
                ImGui::DragFloat2("Size (W, H)", &bar.size.x, 1.0f, 10.0f, 2000.0f);
                ImGui::Checkbox("Fill Right-to-Left", &bar.fillRightToLeft);

                // Auto-Bind
                ImGui::Checkbox("Auto-Bind", &bar.autoBind);
                if (bar.autoBind && bar.gaugeType != GaugeType::Custom)
                {
                    ImGui::SameLine();
                    const char* bindTarget = "?";
                    switch (bar.gaugeType)
                    {
                        case GaugeType::HP:      bindTarget = "HealthComponent"; break;
                        case GaugeType::Stamina: bindTarget = "CombatBlackBoard.Stamina / PlayerBlackBoard"; break;
                        case GaugeType::Dodge:   bindTarget = "CombatBlackBoard.Dodge (Legacy)"; break;
                        case GaugeType::Block:   bindTarget = "CombatBlackBoard.Block (Legacy)"; break;
                        default: break;
                    }
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "-> %s", bindTarget);
                }

                // Colors
                ImGui::ColorEdit4("Fill Color", &bar.fillColor.x);
                ImGui::ColorEdit4("Background Color", &bar.bgColor.x);
                ImGui::ColorEdit4("Border Color", &bar.borderColor.x);
                ImGui::SliderFloat("Low HP Threshold", &bar.lowThreshold, 0.0f, 1.0f, "%.2f");
                ImGui::ColorEdit4("Low HP Color", &bar.lowColor.x);

                // Elden Ring Gauge Shader & Delayed Damage Trail
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.30f, 1.0f), "Elden Ring Shader & Damage Trail:");
                ImGui::Checkbox("Use Elden Ring Shader", &bar.useEldenRingShader);
                ImGui::SameLine();
                ImGui::Checkbox("Procedural Gilded Frame", &bar.useProceduralFrame);
                ImGui::SliderFloat("Bevel Intensity", &bar.bevelIntensity, 0.0f, 1.5f, "%.2f");
                ImGui::SliderFloat("Leading-Edge Meniscus Glow", &bar.meniscusGlow, 0.0f, 2.0f, "%.2f");

                ImGui::Checkbox("Show Delayed Damage Trail", &bar.showDamageTrail);
                if (bar.showDamageTrail)
                {
                    ImGui::ColorEdit4("Trail Color", &bar.trailColor.x);
                    ImGui::SliderFloat("Trail Hold Delay (s)", &bar.trailHoldDelay, 0.0f, 2.0f, "%.2f s");
                    ImGui::SliderFloat("Trail Drain Speed", &bar.trailDrainSpeed, 0.5f, 15.0f, "%.1f");
                }
                ImGui::Checkbox("Show Combo Damage Counter", &bar.showDamageNumbers);
                if (bar.showDamageNumbers)
                {
                    ImGui::SliderFloat("Damage Counter Duration (s)", &bar.damageDisplayDuration, 0.3f, 5.0f, "%.2f s");
                }

                // Textures
                ImGui::Separator();
                ImGui::Text("Textures (Optional):");
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove All Textures (Procedural Shader)##ClearAllTex"))
                {
                    bar.RemoveAllTextures(true);
                }

                // Background Texture
                {
                    std::string pathStr(bar.backgroundTexPath.begin(), bar.backgroundTexPath.end());
                    if (ImGui::InputText("Background Texture", &pathStr))
                    {
                        bar.backgroundTexPath = std::wstring(pathStr.begin(), pathStr.end());
                        if (bar.backgroundTexPath.empty())
                        {
                            bar.backgroundTex.Reset();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##BgTex"))
                    {
                        std::wstring file = HEIN::EditorUtils::OpenFileDialog(
                            L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                        if (!file.empty())
                        {
                            bar.backgroundTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                            LoadBarTextures(gameContext.deviceResources.GetD3DDevice(), bar);
                        }
                    }
                    if (!bar.backgroundTexPath.empty() || bar.backgroundTex)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##ClearBgTex"))
                        {
                            bar.RemoveBackgroundTexture();
                        }
                    }
                }

                // Fill Texture
                {
                    std::string pathStr(bar.fillTexPath.begin(), bar.fillTexPath.end());
                    if (ImGui::InputText("Fill Texture", &pathStr))
                    {
                        bar.fillTexPath = std::wstring(pathStr.begin(), pathStr.end());
                        if (bar.fillTexPath.empty())
                        {
                            bar.fillTex.Reset();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##FillTex"))
                    {
                        std::wstring file = HEIN::EditorUtils::OpenFileDialog(
                            L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                        if (!file.empty())
                        {
                            bar.fillTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                            LoadBarTextures(gameContext.deviceResources.GetD3DDevice(), bar);
                        }
                    }
                    if (!bar.fillTexPath.empty() || bar.fillTex)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##ClearFillTex"))
                        {
                            bar.RemoveFillTexture();
                        }
                    }

                    // Fill Range / End Point controls
                    ImGui::Indent(10.0f);
                    ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "Fill Range / End Point (0.0 - 1.0):");

                    ImGui::DragFloatRange2("Range (Start / End)##FillRange",
                        &bar.fillStartPoint, &bar.fillEndPoint, 0.002f, 0.0f, 1.0f, "Start: %.4f", "End: %.4f");
                    ImGui::SliderFloat("Fill Start Point##Start", &bar.fillStartPoint, 0.0f, 1.0f, "%.4f");
                    ImGui::SliderFloat("Fill End Point##End", &bar.fillEndPoint, 0.0f, 1.0f, "%.4f");

                    ImGui::Checkbox("Clip Texture (Do Not Squash)", &bar.clipTexture);
                    ImGui::SameLine();
                    ImGui::Checkbox("Share Frame Canvas UVs", &bar.fillTexMatchesFrame);
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip(
                            "Enabled: Texture UVs align with the full border frame canvas (e.g., fill.dds matching border.dds).\n"
                            "Disabled: Texture UVs span 0.0-1.0 across the active fill area (e.g., trimmed fill textures)."
                        );
                    }

                    // Live pixel readout
                    if (bar.fillTex)
                    {
                        UINT tw = 0, th = 0;
                        GetTextureDimensions(bar.fillTex.Get(), tw, th);
                        if (tw > 0 && th > 0)
                        {
                            float pxStart = static_cast<float>(tw) * bar.fillStartPoint;
                            float pxEnd = static_cast<float>(tw) * bar.fillEndPoint;
                            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                                "Tex %ux%u -> Active Pixels: [%.0f - %.0f] px (W: %.0f px)",
                                tw, th, pxStart, pxEnd, pxEnd - pxStart);
                        }
                    }

                    float scrStart = bar.size.x * bar.fillStartPoint;
                    float scrEnd = bar.size.x * bar.fillEndPoint;
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Bar Width %.0f -> Active Range: [%.1f - %.1f] px (W: %.1f px)",
                        bar.size.x, scrStart, scrEnd, scrEnd - scrStart);

                    if (ImGui::SmallButton("Reset Range to Full (0.0 - 1.0)"))
                    {
                        bar.fillStartPoint = 0.0f;
                        bar.fillEndPoint = 1.0f;
                    }
                    ImGui::Unindent(10.0f);
                }

                // Border/Frame Texture
                {
                    std::string pathStr(bar.borderTexPath.begin(), bar.borderTexPath.end());
                    if (ImGui::InputText("Border/Frame Texture", &pathStr))
                    {
                        bar.borderTexPath = std::wstring(pathStr.begin(), pathStr.end());
                        if (bar.borderTexPath.empty())
                        {
                            bar.borderTex.Reset();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##BorderTex"))
                    {
                        std::wstring file = HEIN::EditorUtils::OpenFileDialog(
                            L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                        if (!file.empty())
                        {
                            bar.borderTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                            LoadBarTextures(gameContext.deviceResources.GetD3DDevice(), bar);
                        }
                    }
                    if (!bar.borderTexPath.empty() || bar.borderTex)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##ClearBorderTex"))
                        {
                            bar.RemoveBorderTexture();
                        }
                    }
                }

                // Label settings
                ImGui::Separator();
                ImGui::Text("Label Settings:");
                ImGui::Checkbox("Show Label", &bar.showLabel);
                ImGui::SameLine();
                ImGui::Checkbox("Show Value", &bar.showValue);
                ImGui::SliderFloat("Font Size", &bar.fontSize, 0.3f, 3.0f, "%.2fx");
                ImGui::ColorEdit4("Label Color", &bar.labelColor.x);
                ImGui::DragFloat2("Text Offset (X, Y)", &bar.textOffset.x, 0.5f);

                // SpriteFont resource selector
                {
                    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, bar.spriteFontPath.c_str(), static_cast<int>(bar.spriteFontPath.length()), nullptr, 0, nullptr, nullptr);
                    std::string fontPathStr(sizeNeeded, 0);
                    if (sizeNeeded > 0)
                    {
                        WideCharToMultiByte(CP_UTF8, 0, bar.spriteFontPath.c_str(), static_cast<int>(bar.spriteFontPath.length()), &fontPathStr[0], sizeNeeded, nullptr, nullptr);
                    }

                    if (ImGui::InputText("SpriteFont Path", &fontPathStr))
                    {
                        int wlen = MultiByteToWideChar(CP_UTF8, 0, fontPathStr.c_str(), static_cast<int>(fontPathStr.length()), nullptr, 0);
                        std::wstring ws(wlen, 0);
                        if (wlen > 0)
                        {
                            MultiByteToWideChar(CP_UTF8, 0, fontPathStr.c_str(), static_cast<int>(fontPathStr.length()), &ws[0], wlen);
                        }
                        bar.spriteFontPath = ws;
                        LoadBarFont(gameContext.deviceResources.GetD3DDevice(), bar);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##SpriteFont"))
                    {
                        std::wstring file = HEIN::EditorUtils::OpenFileDialog(
                            L"SpriteFont Files\0*.spritefont\0All Files\0*.*\0", windowHandle);
                        if (!file.empty())
                        {
                            bar.spriteFontPath = HEIN::EditorUtils::MakeRelativePath(file);
                            LoadBarFont(gameContext.deviceResources.GetD3DDevice(), bar);
                        }
                    }
                    if (!bar.spriteFontPath.empty() || bar.spriteFont)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##ClearSpriteFont"))
                        {
                            bar.RemoveSpriteFont();
                        }
                    }

                    // Resource Presets Dropdown
                    const char* fontPresets[] = {
                        "(Default / ImGui Font)",
                        "Resources/Font/GraenMetal.spritefont",
                        "Resources/Font/Shadows.spritefont",
                        "Resources/Font/SegoeUI_18.spritefont"
                    };
                    int currentPreset = 0;
                    for (int pIdx = 1; pIdx < 4; ++pIdx)
                    {
                        int wlen = MultiByteToWideChar(CP_UTF8, 0, fontPresets[pIdx], -1, nullptr, 0);
                        std::wstring pW(wlen, 0);
                        MultiByteToWideChar(CP_UTF8, 0, fontPresets[pIdx], -1, &pW[0], wlen);
                        if (!pW.empty() && pW.back() == L'\0') pW.pop_back();

                        if (bar.spriteFontPath == pW)
                        {
                            currentPreset = pIdx;
                            break;
                        }
                    }
                    if (ImGui::Combo("Font Preset", &currentPreset, fontPresets, 4))
                    {
                        if (currentPreset == 0)
                        {
                            bar.spriteFontPath.clear();
                            bar.spriteFont.reset();
                        }
                        else
                        {
                            const char* sel = fontPresets[currentPreset];
                            int wlen = MultiByteToWideChar(CP_UTF8, 0, sel, -1, nullptr, 0);
                            std::wstring pW(wlen, 0);
                            MultiByteToWideChar(CP_UTF8, 0, sel, -1, &pW[0], wlen);
                            if (!pW.empty() && pW.back() == L'\0') pW.pop_back();

                            bar.spriteFontPath = pW;
                            LoadBarFont(gameContext.deviceResources.GetD3DDevice(), bar);
                        }
                    }
                }

                ImGui::Separator();
                if (ImGui::Button("Reload Textures"))
                {
                    LoadBarTextures(gameContext.deviceResources.GetD3DDevice(), bar);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove All Textures"))
                {
                    bar.RemoveAllTextures(true);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove This Bar"))
                {
                    removeIdx = i;
                }
            }

            ImGui::PopID();
        }

        if (removeIdx >= 0)
        {
            m_bars.erase(m_bars.begin() + removeIdx);
        }
    }
}

// ============================================================================
// DrawGizmo: Visual editing of gauge position in the editor viewport
// ============================================================================
void HEIN::GaugeComponent::DrawGizmo(
    const DirectX::SimpleMath::Matrix& /*view*/,
    const DirectX::SimpleMath::Matrix& /*proj*/,
    int operation,
    int mode)
{
    if (m_anchorMode != GaugeAnchorMode::ScreenSpace) return;

    ImGuizmo::SetOrthographic(true);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    float vpX = 0.0f, vpY = 0.0f, displayW = kRefWidth, displayH = kRefHeight;
    HEIN::DebugUIManager::GetActiveViewportRect(vpX, vpY, displayW, displayH);

    float scaleX = displayW / kRefWidth;
    float scaleY = displayH / kRefHeight;

    ImGuizmo::SetRect(vpX, vpY, displayW, displayH);
    ImGuizmo::SetGizmoSizeClipSpace(0.12f);

    DirectX::SimpleMath::Matrix orthoView = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix orthoProj = DirectX::SimpleMath::Matrix::CreateOrthographicOffCenter(
        vpX, vpX + displayW, vpY + displayH, vpY, -1000.0f, 1000.0f
    );

    // Compute bounding box of all bars combined
    float minX = 99999.0f, minY = 99999.0f;
    float maxX = -99999.0f, maxY = -99999.0f;
    for (const auto& bar : m_bars)
    {
        float bx = vpX + (m_screenPosition.x + bar.offset.x) * scaleX;
        float by = vpY + (m_screenPosition.y + bar.offset.y) * scaleY;
        float bw = bar.size.x * scaleX;
        float bh = bar.size.y * scaleY;
        minX = std::min(minX, bx);
        minY = std::min(minY, by);
        maxX = std::max(maxX, bx + bw);
        maxY = std::max(maxY, by + bh);
    }

    if (m_bars.empty())
    {
        minX = vpX + m_screenPosition.x * scaleX;
        minY = vpY + m_screenPosition.y * scaleY;
        maxX = minX + GaugeBarDef::DEFAULT_BAR_WIDTH * scaleX;
        maxY = minY + GaugeBarDef::DEFAULT_BAR_HEIGHT * scaleY;
    }

    float groupW = maxX - minX;
    float groupH = maxY - minY;

    DirectX::SimpleMath::Vector3 centerPos((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, 0.0f);
    DirectX::SimpleMath::Vector3 scale(groupW, groupH, 1.0f);
    DirectX::SimpleMath::Matrix worldMat = DirectX::SimpleMath::Matrix::CreateScale(scale) *
                                           DirectX::SimpleMath::Matrix::CreateTranslation(centerPos);

    ImGuizmo::Manipulate(
        (float*)&orthoView.m[0][0],
        (float*)&orthoProj.m[0][0],
        (ImGuizmo::OPERATION)operation,
        (ImGuizmo::MODE)mode,
        (float*)&worldMat.m[0][0]
    );

    if (ImGuizmo::IsUsing())
    {
        DirectX::SimpleMath::Vector3 newScale, newPos;
        DirectX::SimpleMath::Quaternion newRot;
        if (worldMat.Decompose(newScale, newRot, newPos))
        {
            float newMinX = (newPos.x - vpX) - std::abs(newScale.x) * 0.5f;
            float newMinY = (newPos.y - vpY) - std::abs(newScale.y) * 0.5f;
            m_screenPosition.x = newMinX / scaleX - (m_bars.empty() ? 0.0f : m_bars[0].offset.x);
            m_screenPosition.y = newMinY / scaleY - (m_bars.empty() ? 0.0f : m_bars[0].offset.y);
        }
    }

    // Draw editor overlay
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImU32 borderColor = IM_COL32(255, 180, 50, 255);
    drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), borderColor, 0.0f, 0, 2.0f);

    char badgeBuf[128];
    sprintf_s(badgeBuf, "Gauge  Pos:(%.0f, %.0f)  Bars:%d",
              m_screenPosition.x, m_screenPosition.y, static_cast<int>(m_bars.size()));
    ImVec2 badgeSize = ImGui::CalcTextSize(badgeBuf);
    ImVec2 bMin(minX, minY - badgeSize.y - 6.0f);
    ImVec2 bMax(minX + badgeSize.x + 8.0f, minY - 2.0f);
    drawList->AddRectFilled(bMin, bMax, IM_COL32(10, 20, 35, 220), 4.0f);
    drawList->AddText(ImVec2(bMin.x + 4.0f, bMin.y + 1.0f), borderColor, badgeBuf);
}


} // namespace HEIN
