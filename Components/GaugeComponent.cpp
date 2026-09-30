#include "pch.h"
#include "GaugeComponent.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/TransformComponent.h"
#include "Camera/CameraController.h"
#include "DebugingTools/EditorUtils.h"
#include "DebugingTools/DebugUIManager.h"
#include "HealthComponent.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <ImGui/ImGuizmo.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <algorithm>
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
        uint8_t r = static_cast<uint8_t>(std::clamp(c.x, 0.0f, 1.0f) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::clamp(c.y, 0.0f, 1.0f) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::clamp(c.z, 0.0f, 1.0f) * 255.0f);
        uint8_t a = static_cast<uint8_t>(std::clamp(c.w, 0.0f, 1.0f) * 255.0f);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    void LoadTextureFromPath(ID3D11Device* device, const std::wstring& path,
                             Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
    {
        srv.Reset();
        if (path.empty()) return;
        
        HRESULT hr = DirectX::CreateDDSTextureFromFile(
            device, path.c_str(), nullptr, srv.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            hr = DirectX::CreateWICTextureFromFile(
                device, path.c_str(), nullptr, srv.ReleaseAndGetAddressOf());
        }
    }

    const char* GaugeTypeToStr(HEIN::GaugeType t)
    {
        switch (t)
        {
            case HEIN::GaugeType::HP:     return "HP";
            case HEIN::GaugeType::Dodge:  return "Dodge";
            case HEIN::GaugeType::Block:  return "Block";
            case HEIN::GaugeType::Custom: return "Custom";
            default: return "Unknown";
        }
    }
}

// ============================================================================
// Constructor
// ============================================================================
HEIN::GaugeComponent::GaugeComponent(Actor* owner)
    : IComponent(owner)
{
}

// ============================================================================
// Initialize: Create SpriteBatch and load textures for all bars
// ============================================================================
void HEIN::GaugeComponent::Initialize(GameContext& gameContext)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    if (!m_spriteBatch)
    {
        m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    }

    for (auto& bar : m_bars)
    {
        LoadBarTextures(device, bar);
    }
}

// ============================================================================
// LoadBarTextures: Load DDS/WIC textures for a single bar definition
// ============================================================================
void HEIN::GaugeComponent::LoadBarTextures(ID3D11Device* device, GaugeBarDef& bar)
{
    LoadTextureFromPath(device, bar.backgroundTexPath, bar.backgroundTex);
    LoadTextureFromPath(device, bar.fillTexPath, bar.fillTex);
    LoadTextureFromPath(device, bar.borderTexPath, bar.borderTex);
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

    D3D11_VIEWPORT vp = gameContext.deviceResources.GetScreenViewport();
    float aspect = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : (1280.0f / 720.0f);
    float fov = gameContext.mainCamera->GetFov();
    if (fov <= 0.0f) fov = DirectX::XM_PI / 4.0f;

    DirectX::SimpleMath::Matrix proj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
        fov, aspect, 0.1f, 5000.0f);

    // Transform world -> clip space
    DirectX::SimpleMath::Matrix viewProj = view * proj;
    DirectX::SimpleMath::Vector4 clipPos = DirectX::SimpleMath::Vector4::Transform(
        DirectX::SimpleMath::Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), viewProj);

    // Behind camera check
    if (clipPos.w <= 0.001f) return false;

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
        if (pair.second->GetActorType() == HEIN::ActorType::Player)
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

void HEIN::GaugeComponent::Update(float /*deltaTime*/)
{
    // Skip range check in editor mode — always show for editing
    // (gameContext is not available in Update, range check happens in Draw)
}

// ============================================================================
// SyncBoundBars: Auto-sync bar values from data source components
// HP is pulled here (engine-side). Dodge/Block are pushed by game-side code.
// ============================================================================
void HEIN::GaugeComponent::SyncBoundBars(GameContext& gameContext)
{
    for (auto& bar : m_bars)
    {
        if (!bar.autoBind) continue;

        if (bar.gaugeType == GaugeType::HP)
        {
            // Look for HealthComponent on parent actor first, then owner
            Actor* target = GetParentActor(gameContext);
            if (!target) target = m_owner;

            HealthComponent* health = target->GetComponent<HealthComponent>();
            if (health)
            {
                bar.currentValue = health->GetCurrentHealth();
                bar.maxValue = health->GetMaxHealth();
            }
        }
        // GaugeType::Dodge and GaugeType::Block are pushed by game-side 
        // CombatBlackBoard::Update() — no engine-side pull needed.
        // GaugeType::Custom is always manual via SetValue().
    }
}

// ============================================================================
// Draw2D: Main rendering entry point (called by Actor::Draw2D)
// ============================================================================
void HEIN::GaugeComponent::Draw2D(GameContext& gameContext)
{
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
    if (!m_isVisible || m_bars.empty()) return;

    // Auto-sync bound bar values from data source components
    SyncBoundBars(gameContext);

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
    UINT numVp = 1;
    gameContext.deviceResources.GetD3DDeviceContext()->RSGetViewports(&numVp, &vp);
    if (vp.Width <= 0.0f || vp.Height <= 0.0f)
    {
        vp = gameContext.deviceResources.GetScreenViewport();
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
    // Compute screen-space rectangle
    float barX = (anchorX + bar.offset.x) * scaleX;
    float barY = (anchorY + bar.offset.y) * scaleY;
    float barW = bar.size.x * scaleX;
    float barH = bar.size.y * scaleY;

    // Calculate fill ratio
    float ratio = (bar.maxValue > 0.0f) ? std::clamp(bar.currentValue / bar.maxValue, 0.0f, 1.0f) : 0.0f;

    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    // UI overlay uses DepthNone so it draws on top of 3D
    context->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);

    if (m_spriteBatch)
    {
        m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());

        // 1) Background
        {
            RECT bgRect;
            bgRect.left = static_cast<LONG>(barX);
            bgRect.top = static_cast<LONG>(barY);
            bgRect.right = static_cast<LONG>(barX + barW);
            bgRect.bottom = static_cast<LONG>(barY + barH);

            ID3D11ShaderResourceView* bgTex = bar.backgroundTex.Get();
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fallbackBg;
            if (!bgTex)
            {
                fallbackBg = CreateSolidTex(device, Vec4ToABGR(bar.bgColor));
                bgTex = fallbackBg.Get();
            }

            if (bgTex)
            {
                m_spriteBatch->Draw(bgTex, bgRect, nullptr,
                    DirectX::SimpleMath::Vector4(bar.bgColor.x, bar.bgColor.y, bar.bgColor.z, bar.bgColor.w));
            }
        }

        // 2) Fill
        {
            DirectX::SimpleMath::Vector4 currentFillColor = bar.fillColor;
            if (ratio < bar.lowThreshold)
            {
                currentFillColor = bar.lowColor;
            }

            float fillW = barW * ratio;
            RECT fillRect;

            if (bar.fillRightToLeft)
            {
                fillRect.left = static_cast<LONG>(barX + barW - fillW);
                fillRect.right = static_cast<LONG>(barX + barW);
            }
            else
            {
                fillRect.left = static_cast<LONG>(barX);
                fillRect.right = static_cast<LONG>(barX + fillW);
            }

            fillRect.top = static_cast<LONG>(barY);
            fillRect.bottom = static_cast<LONG>(barY + barH);

            ID3D11ShaderResourceView* fTex = bar.fillTex.Get();
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fallbackFill;
            if (!fTex)
            {
                fallbackFill = CreateSolidTex(device, Vec4ToABGR(currentFillColor));
                fTex = fallbackFill.Get();
            }

            if (fTex && fillW > 0.0f)
            {
                m_spriteBatch->Draw(fTex, fillRect, nullptr, currentFillColor);
            }
        }

        // 3) Border/Frame overlay (drawn on top of fill)
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

        m_spriteBatch->End();
    }

    // Reset depth state
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);

    // 4) Label and Value text using ImGui draw list (always on top)
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (drawList)
    {
        float fontSize = ImGui::GetFontSize() * bar.fontSize * scaleY;

        ImU32 labelCol = IM_COL32(
            static_cast<int>(bar.labelColor.x * 255.0f),
            static_cast<int>(bar.labelColor.y * 255.0f),
            static_cast<int>(bar.labelColor.z * 255.0f),
            static_cast<int>(bar.labelColor.w * 255.0f)
        );

        if (bar.showLabel)
        {
            // Label: drawn to the left of the bar
            ImVec2 labelPos(m_lastVpX + barX + 4.0f, m_lastVpY + barY + (barH - fontSize) * 0.5f);

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

            ImVec2 valSize = ImGui::CalcTextSize(valBuf);
            valSize.x *= (bar.fontSize * scaleY / ImGui::GetFontSize() * ImGui::GetFontSize() / ImGui::GetFontSize());

            ImVec2 valPos(
                m_lastVpX + barX + barW - valSize.x * bar.fontSize * scaleY / ImGui::GetFontSize() - 4.0f,
                m_lastVpY + barY + (barH - fontSize) * 0.5f
            );

            // Drop shadow
            drawList->AddText(ImGui::GetFont(), fontSize,
                ImVec2(valPos.x + 1.0f, valPos.y + 1.0f),
                IM_COL32(0, 0, 0, 180), valBuf);
            drawList->AddText(ImGui::GetFont(), fontSize, valPos, labelCol, valBuf);
        }

        // If no border texture, draw a thin rectangle border via ImGui
        if (!bar.borderTex.Get())
        {
            ImU32 bCol = IM_COL32(
                static_cast<int>(bar.borderColor.x * 255.0f),
                static_cast<int>(bar.borderColor.y * 255.0f),
                static_cast<int>(bar.borderColor.z * 255.0f),
                static_cast<int>(bar.borderColor.w * 255.0f)
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
            if (b.contains("CurrentValue")) bar.currentValue = b["CurrentValue"];
            if (b.contains("MaxValue")) bar.maxValue = b["MaxValue"];
            if (b.contains("OffsetX")) bar.offset.x = b["OffsetX"];
            if (b.contains("OffsetY")) bar.offset.y = b["OffsetY"];
            if (b.contains("SizeX")) bar.size.x = b["SizeX"];
            if (b.contains("SizeY")) bar.size.y = b["SizeY"];
            if (b.contains("FillRightToLeft")) bar.fillRightToLeft = b["FillRightToLeft"];
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
                m_deactivationRange = m_activationRange + 5.0f;
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
            bar.fillColor = { 0.2f, 0.8f, 0.2f, 1.0f };
            bar.offset = { 0.0f, static_cast<float>(m_bars.size()) * 28.0f };
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
                const char* typeNames[] = { "HP", "Dodge", "Block", "Custom" };
                int typeIdx = static_cast<int>(bar.gaugeType);
                if (ImGui::Combo("Gauge Type", &typeIdx, typeNames, IM_ARRAYSIZE(typeNames)))
                {
                    bar.gaugeType = static_cast<GaugeType>(typeIdx);
                }

                // Values
                ImGui::DragFloat("Current Value", &bar.currentValue, 0.5f, 0.0f, bar.maxValue);
                ImGui::DragFloat("Max Value", &bar.maxValue, 0.5f, 1.0f, 99999.0f);

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
                        case GaugeType::HP:    bindTarget = "HealthComponent"; break;
                        case GaugeType::Dodge: bindTarget = "CombatBlackBoard.Dodge"; break;
                        case GaugeType::Block: bindTarget = "CombatBlackBoard.Block"; break;
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

                // Textures
                ImGui::Separator();
                ImGui::Text("Textures (Optional):");

                // Background Texture
                {
                    std::string pathStr(bar.backgroundTexPath.begin(), bar.backgroundTexPath.end());
                    if (ImGui::InputText("Background Texture", &pathStr))
                    {
                        bar.backgroundTexPath = std::wstring(pathStr.begin(), pathStr.end());
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
                }

                // Fill Texture
                {
                    std::string pathStr(bar.fillTexPath.begin(), bar.fillTexPath.end());
                    if (ImGui::InputText("Fill Texture", &pathStr))
                    {
                        bar.fillTexPath = std::wstring(pathStr.begin(), pathStr.end());
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
                }

                // Border/Frame Texture
                {
                    std::string pathStr(bar.borderTexPath.begin(), bar.borderTexPath.end());
                    if (ImGui::InputText("Border/Frame Texture", &pathStr))
                    {
                        bar.borderTexPath = std::wstring(pathStr.begin(), pathStr.end());
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
                }

                // Label settings
                ImGui::Separator();
                ImGui::Text("Label Settings:");
                ImGui::Checkbox("Show Label", &bar.showLabel);
                ImGui::SameLine();
                ImGui::Checkbox("Show Value", &bar.showValue);
                ImGui::SliderFloat("Font Size", &bar.fontSize, 0.3f, 3.0f, "%.2fx");
                ImGui::ColorEdit4("Label Color", &bar.labelColor.x);

                ImGui::Separator();
                if (ImGui::Button("Reload Textures"))
                {
                    LoadBarTextures(gameContext.deviceResources.GetD3DDevice(), bar);
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
    ImGuiIO& io = ImGui::GetIO();
    float displayW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : kRefWidth;
    float displayH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : kRefHeight;

    float scaleX = displayW / kRefWidth;
    float scaleY = displayH / kRefHeight;

    ImGuizmo::SetRect(0, 0, displayW, displayH);
    ImGuizmo::SetGizmoSizeClipSpace(0.12f);

    DirectX::SimpleMath::Matrix orthoView = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix orthoProj = DirectX::SimpleMath::Matrix::CreateOrthographicOffCenter(
        0.0f, displayW, displayH, 0.0f, -1000.0f, 1000.0f
    );

    // Compute bounding box of all bars combined
    float minX = 99999.0f, minY = 99999.0f;
    float maxX = -99999.0f, maxY = -99999.0f;
    for (const auto& bar : m_bars)
    {
        float bx = (m_screenPosition.x + bar.offset.x) * scaleX;
        float by = (m_screenPosition.y + bar.offset.y) * scaleY;
        float bw = bar.size.x * scaleX;
        float bh = bar.size.y * scaleY;
        minX = std::min(minX, bx);
        minY = std::min(minY, by);
        maxX = std::max(maxX, bx + bw);
        maxY = std::max(maxY, by + bh);
    }

    if (m_bars.empty())
    {
        minX = m_screenPosition.x * scaleX;
        minY = m_screenPosition.y * scaleY;
        maxX = minX + 200.0f * scaleX;
        maxY = minY + 20.0f * scaleY;
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
            float newMinX = newPos.x - std::abs(newScale.x) * 0.5f;
            float newMinY = newPos.y - std::abs(newScale.y) * 0.5f;
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

