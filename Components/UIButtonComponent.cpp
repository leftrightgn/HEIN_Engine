#include "pch.h"
#include "UIButtonComponent.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <ImGui/ImGuizmo.h>
#include <DebugingTools/DebugUIManager.h>
#include <DebugingTools/EditorUtils.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>

namespace
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidTexture(ID3D11Device* device, uint32_t color)
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
        initData.pSysMem = &color;
        initData.SysMemPitch = sizeof(uint32_t);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
        if (FAILED(hr)) return nullptr;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        if (FAILED(hr)) return nullptr;

        return srv;
    }
}

HEIN::UIButtonComponent::UIButtonComponent(Actor* owner)
    : IComponent(owner)
{
}

void HEIN::UIButtonComponent::Initialize(GameContext& gameContext, const wchar_t* normalTex, const wchar_t* hoverTex, const wchar_t* pressedTex)
{
    if (normalTex)  m_normalTexPath = normalTex;
    if (hoverTex)   m_hoverTexPath = hoverTex;
    if (pressedTex) m_pressedTexPath = pressedTex;

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    if (!m_spriteBatch)
    {
        m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    }

    auto loadTexture = [device](const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
    {
        srv.Reset();
        if (path.empty()) return;

        HRESULT hr = DirectX::CreateDDSTextureFromFile(device, path.c_str(), nullptr, srv.ReleaseAndGetAddressOf());
        if (FAILED(hr))
        {
            hr = DirectX::CreateWICTextureFromFile(device, path.c_str(), nullptr, srv.ReleaseAndGetAddressOf());
        }
    };

    loadTexture(m_normalTexPath, m_normalTexture);
    loadTexture(m_hoverTexPath, m_hoverTexture);
    loadTexture(m_pressedTexPath, m_pressedTexture);

    // Fallback solid color textures for button defaults if no custom texture provided
    if (m_elementType == UIElementType::Button)
    {
        if (!m_normalTexture)  m_normalTexture = CreateSolidTexture(device, 0xEE3A2E20);
        if (!m_hoverTexture)   m_hoverTexture = CreateSolidTexture(device, 0xFF654E35);
        if (!m_pressedTexture) m_pressedTexture = CreateSolidTexture(device, 0xFF9E7E5A);
    }
}

void HEIN::UIButtonComponent::Start()
{
}

void HEIN::UIButtonComponent::Update(float deltaTime)
{
    if (!m_isVisible) return;

    // Reset clicked flag every frame
    m_isClicked = false;

    // Non-button elements don't interact with mouse clicks
    if (m_elementType != UIElementType::Button)
    {
        m_currentState = ButtonState::Normal;
        return;
    }

    // Ignore clicks when ImGui or ImGuizmo is capturing mouse
    bool isUICapturingMouse = ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    if (isUICapturingMouse)
    {
        m_wasMouseDown = false;
        m_currentState = ButtonState::Normal;
        return;
    }

    // Grab raw global mouse state
    auto mouseState = DirectX::Mouse::Get().GetState();
    float mouseX = static_cast<float>(mouseState.x);
    float mouseY = static_cast<float>(mouseState.y);

    float scaleX = (m_lastScaleX > 0.0001f) ? m_lastScaleX : (ImGui::GetIO().DisplaySize.x / kRefWidth);
    float scaleY = (m_lastScaleY > 0.0001f) ? m_lastScaleY : (ImGui::GetIO().DisplaySize.y / kRefHeight);
    if (scaleX <= 0.0001f) scaleX = 1.0f;
    if (scaleY <= 0.0001f) scaleY = 1.0f;

    // Transform mouse coordinates from window space into virtual reference canvas (1280x720) space
    float canvasMouseX = (mouseX - m_lastVpX) / scaleX;
    float canvasMouseY = (mouseY - m_lastVpY) / scaleY;

    // 2D AABB Collision Math against 1280x720 canvas
    bool isHovering = (canvasMouseX >= m_position.x && canvasMouseX <= m_position.x + m_size.x &&
        canvasMouseY >= m_position.y && canvasMouseY <= m_position.y + m_size.y);

    bool isMouseDown = mouseState.leftButton;

    if (isHovering)
    {
        if (isMouseDown)
        {
            m_currentState = ButtonState::Pressed;
        }
        else
        {
            m_currentState = ButtonState::Hover;

            // If the mouse was down last frame, but isn't this frame (Mouse Up/Release) -> Trigger Click!
            if (m_wasMouseDown)
            {
                m_isClicked = true;
            }
        }
    }
    else
    {
        m_currentState = ButtonState::Normal;
    }

    m_wasMouseDown = isMouseDown;
}

void HEIN::UIButtonComponent::Draw2D(GameContext& gameContext)
{
    Draw(gameContext, DirectX::SimpleMath::Matrix::Identity, DirectX::SimpleMath::Matrix::Identity, DirectX::SimpleMath::Matrix::Identity);
}

void HEIN::UIButtonComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
{
    if (!m_isVisible) return;

    // Query current active viewport to support responsive scaling in full view, windowed mode, and sub-viewports
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

    float renderPosX = m_position.x * m_lastScaleX;
    float renderPosY = m_position.y * m_lastScaleY;
    float renderSizeX = m_size.x * m_lastScaleX;
    float renderSizeY = m_size.y * m_lastScaleY;

    // 1. Draw Textures for Button or Image elements
    if ((m_elementType == UIElementType::Button || m_elementType == UIElementType::Image) && m_spriteBatch)
    {
        ID3D11ShaderResourceView* textureToDraw = m_normalTexture.Get();

        if (m_elementType == UIElementType::Button)
        {
            if (m_currentState == ButtonState::Hover && m_hoverTexture)
                textureToDraw = m_hoverTexture.Get();
            else if (m_currentState == ButtonState::Pressed && m_pressedTexture)
                textureToDraw = m_pressedTexture.Get();
        }

        if (textureToDraw)
        {
            RECT drawRect;
            drawRect.left = static_cast<LONG>(renderPosX);
            drawRect.top = static_cast<LONG>(renderPosY);
            drawRect.right = static_cast<LONG>(renderPosX + renderSizeX);
            drawRect.bottom = static_cast<LONG>(renderPosY + renderSizeY);

            DirectX::SimpleMath::Vector4 tint(m_tintColor.x, m_tintColor.y, m_tintColor.z, m_tintColor.w);

            // UI overlay uses DepthNone and NonPremultiplied alpha blend so it is always on top of 3D geometry
            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());
            m_spriteBatch->Draw(textureToDraw, drawRect, nullptr, tint);
            m_spriteBatch->End();

            // Reset depth state to default for any subsequent 3D passes
            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
        }
    }

    // 2. Draw Text (for Text elements or Buttons with labels)
    if (!m_text.empty())
    {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (drawList)
        {
            ImU32 col = IM_COL32(
                static_cast<int>(m_textColor.x * 255.0f),
                static_cast<int>(m_textColor.y * 255.0f),
                static_cast<int>(m_textColor.z * 255.0f),
                static_cast<int>(m_textColor.w * 255.0f)
            );

            float fontSize = ImGui::GetFontSize() * m_fontSize * m_lastScaleY;
            ImVec2 textSize = ImGui::CalcTextSize(m_text.c_str());
            textSize.x *= (m_fontSize * m_lastScaleY);
            textSize.y *= (m_fontSize * m_lastScaleY);

            ImVec2 textPos;
            if (m_elementType == UIElementType::Button)
            {
                // Center text inside button bounds at viewport screen position
                textPos = ImVec2(
                    m_lastVpX + renderPosX + (renderSizeX - textSize.x) * 0.5f,
                    m_lastVpY + renderPosY + (renderSizeY - textSize.y) * 0.5f
                );
            }
            else
            {
                // Top-left aligned for Text / Labels
                textPos = ImVec2(
                    m_lastVpX + renderPosX,
                    m_lastVpY + renderPosY
                );
            }

            // Drop shadow
            drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(textPos.x + 2.0f, textPos.y + 2.0f), IM_COL32(0, 0, 0, static_cast<int>(m_textColor.w * 190.0f)), m_text.c_str());
            drawList->AddText(ImGui::GetFont(), fontSize, textPos, col, m_text.c_str());
        }
    }
}

bool HEIN::UIButtonComponent::IsClicked()
{
    return m_isClicked;
}

nlohmann::json HEIN::UIButtonComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["ElementType"] = static_cast<int>(m_elementType);
    data["PosX"] = m_position.x;
    data["PosY"] = m_position.y;
    data["SizeX"] = m_size.x;
    data["SizeY"] = m_size.y;
    data["Text"] = m_text;
    data["TextColorR"] = m_textColor.x;
    data["TextColorG"] = m_textColor.y;
    data["TextColorB"] = m_textColor.z;
    data["TextColorA"] = m_textColor.w;
    data["FontSize"] = m_fontSize;
    data["TintR"] = m_tintColor.x;
    data["TintG"] = m_tintColor.y;
    data["TintB"] = m_tintColor.z;
    data["TintA"] = m_tintColor.w;
    data["NormalTex"] = std::string(m_normalTexPath.begin(), m_normalTexPath.end());
    data["HoverTex"] = std::string(m_hoverTexPath.begin(), m_hoverTexPath.end());
    data["PressedTex"] = std::string(m_pressedTexPath.begin(), m_pressedTexPath.end());
    data["IsVisible"] = m_isVisible;
    return data;
}

void HEIN::UIButtonComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("ElementType")) m_elementType = static_cast<UIElementType>(data["ElementType"].get<int>());
    if (data.contains("PosX")) m_position.x = data["PosX"];
    if (data.contains("PosY")) m_position.y = data["PosY"];
    if (data.contains("SizeX")) m_size.x = data["SizeX"];
    if (data.contains("SizeY")) m_size.y = data["SizeY"];
    if (data.contains("Text")) m_text = data["Text"];
    if (data.contains("FontSize")) m_fontSize = data["FontSize"];
    if (data.contains("IsVisible")) m_isVisible = data["IsVisible"];

    if (data.contains("TextColorR"))
    {
        m_textColor.x = data["TextColorR"];
        m_textColor.y = data["TextColorG"];
        m_textColor.z = data["TextColorB"];
        m_textColor.w = data.value("TextColorA", 1.0f);
    }
    if (data.contains("TintR"))
    {
        m_tintColor.x = data["TintR"];
        m_tintColor.y = data["TintG"];
        m_tintColor.z = data["TintB"];
        m_tintColor.w = data.value("TintA", 1.0f);
    }

    if (data.contains("NormalTex"))
    {
        std::string path = data["NormalTex"];
        m_normalTexPath = std::wstring(path.begin(), path.end());
    }
    if (data.contains("HoverTex"))
    {
        std::string path = data["HoverTex"];
        m_hoverTexPath = std::wstring(path.begin(), path.end());
    }
    if (data.contains("PressedTex"))
    {
        std::string path = data["PressedTex"];
        m_pressedTexPath = std::wstring(path.begin(), path.end());
    }
}

void HEIN::UIButtonComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
}

void HEIN::UIButtonComponent::OnInspectorGUI(GameContext& gameContext)
{
    HWND windowHandle = gameContext.deviceResources.GetWindow();

    bool isActive = (HEIN::g_ActiveGizmoTarget == this);
    if (ImGui::RadioButton("Edit UI Element with Gizmo", isActive))
    {
        HEIN::g_ActiveGizmoTarget = this;
    }

    if (ImGui::CollapsingHeader("UI Element Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // Element Mode / Type
        const char* typeNames[] = { "Interactive Button", "Static Image / Banner", "Text Label" };
        int currentTypeIdx = static_cast<int>(m_elementType);
        if (ImGui::Combo("UI Element Type", &currentTypeIdx, typeNames, IM_ARRAYSIZE(typeNames)))
        {
            m_elementType = static_cast<UIElementType>(currentTypeIdx);
            Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
        }

        ImGui::Separator();
        ImGui::DragFloat2("Position (Screen X, Y)", &m_position.x, 1.0f);
        ImGui::DragFloat2("Size (Width, Height)", &m_size.x, 1.0f, 10.0f, 4000.0f);
        ImGui::Checkbox("Is Visible", &m_isVisible);

        // Texture & Tint Controls for Button and Image
        if (m_elementType == UIElementType::Button || m_elementType == UIElementType::Image)
        {
            ImGui::Separator();
            ImGui::Text("Texture Settings:");

            // Normal / Image Texture
            std::string normPathStr(m_normalTexPath.begin(), m_normalTexPath.end());
            if (ImGui::InputText(m_elementType == UIElementType::Image ? "Image Texture" : "Normal Texture", &normPathStr))
            {
                m_normalTexPath = std::wstring(normPathStr.begin(), normPathStr.end());
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse##NormalTex"))
            {
                std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                if (!file.empty())
                {
                    m_normalTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                    Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
                }
            }

            if (m_elementType == UIElementType::Button)
            {
                // Hover Texture
                std::string hovPathStr(m_hoverTexPath.begin(), m_hoverTexPath.end());
                if (ImGui::InputText("Hover Texture", &hovPathStr))
                {
                    m_hoverTexPath = std::wstring(hovPathStr.begin(), hovPathStr.end());
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##HoverTex"))
                {
                    std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                    if (!file.empty())
                    {
                        m_hoverTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                        Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
                    }
                }

                // Pressed Texture
                std::string pressPathStr(m_pressedTexPath.begin(), m_pressedTexPath.end());
                if (ImGui::InputText("Pressed Texture", &pressPathStr))
                {
                    m_pressedTexPath = std::wstring(pressPathStr.begin(), pressPathStr.end());
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse##PressedTex"))
                {
                    std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Image Files\0*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0", windowHandle);
                    if (!file.empty())
                    {
                        m_pressedTexPath = HEIN::EditorUtils::MakeRelativePath(file);
                        Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
                    }
                }
            }

            ImGui::ColorEdit4("Tint Color", &m_tintColor.x);
        }

        // Text & Typography Controls for Button and Text
        if (m_elementType == UIElementType::Button || m_elementType == UIElementType::Text)
        {
            ImGui::Separator();
            ImGui::Text(m_elementType == UIElementType::Button ? "Button Label Settings:" : "Text Settings:");
            ImGui::InputText(m_elementType == UIElementType::Button ? "Button Text" : "Content Text", &m_text);
            ImGui::ColorEdit4("Text Color", &m_textColor.x);
            ImGui::SliderFloat("Font Scale", &m_fontSize, 0.5f, 5.0f, "%.2fx");
        }

        ImGui::Separator();
        if (ImGui::Button("Reload Textures / Refresh"))
        {
            Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
        }
    }
}

void HEIN::UIButtonComponent::DrawGizmo(
    const DirectX::SimpleMath::Matrix& /*view*/,
    const DirectX::SimpleMath::Matrix& /*proj*/,
    int operation,
    int mode
)
{
    ImGuizmo::SetOrthographic(true);
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
    ImGuiIO& io = ImGui::GetIO();
    float displayW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : kRefWidth;
    float displayH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : kRefHeight;

    float scaleX = displayW / kRefWidth;
    float scaleY = displayH / kRefHeight;

    ImGuizmo::SetRect(0, 0, displayW, displayH);
    ImGuizmo::SetGizmoSizeClipSpace(0.12f);

    // 2D Orthographic View & Projection for pixel screen space
    DirectX::SimpleMath::Matrix orthoView = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix orthoProj = DirectX::SimpleMath::Matrix::CreateOrthographicOffCenter(
        0.0f, displayW, displayH, 0.0f, -1000.0f, 1000.0f
    );

    float screenPosX = m_position.x * scaleX;
    float screenPosY = m_position.y * scaleY;
    float screenSizeX = m_size.x * scaleX;
    float screenSizeY = m_size.y * scaleY;

    // Center position for the element matrix
    DirectX::SimpleMath::Vector3 centerPos(screenPosX + screenSizeX * 0.5f, screenPosY + screenSizeY * 0.5f, 0.0f);
    DirectX::SimpleMath::Vector3 scale(screenSizeX, screenSizeY, 1.0f);
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
            float newSizeX = std::max(10.0f, std::abs(newScale.x));
            float newSizeY = std::max(10.0f, std::abs(newScale.y));
            float newScreenPosX = newPos.x - newSizeX * 0.5f;
            float newScreenPosY = newPos.y - newSizeY * 0.5f;

            m_size.x = newSizeX / scaleX;
            m_size.y = newSizeY / scaleY;
            m_position.x = newScreenPosX / scaleX;
            m_position.y = newScreenPosY / scaleY;
        }
    }

    // Interactive Screen Space Box Overlay
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 pMin(screenPosX, screenPosY);
    ImVec2 pMax(screenPosX + screenSizeX, screenPosY + screenSizeY);

    ImU32 borderColor = IM_COL32(0, 240, 255, 255);
    const char* typeTag = "UI Button";
    if (m_elementType == UIElementType::Image)
    {
        borderColor = IM_COL32(50, 220, 100, 255);
        typeTag = "UI Image";
    }
    else if (m_elementType == UIElementType::Text)
    {
        borderColor = IM_COL32(255, 200, 50, 255);
        typeTag = "UI Text";
    }

    drawList->AddRect(pMin, pMax, borderColor, 0.0f, 0, 2.0f);

    // Corner grip handles
    float handleSize = 6.0f;
    auto drawHandle = [drawList, handleSize](float x, float y) {
        drawList->AddRectFilled(ImVec2(x - handleSize, y - handleSize), ImVec2(x + handleSize, y + handleSize), IM_COL32(255, 255, 255, 255));
        drawList->AddRect(ImVec2(x - handleSize, y - handleSize), ImVec2(x + handleSize, y + handleSize), IM_COL32(0, 160, 255, 255), 0.0f, 0, 1.5f);
    };

    drawHandle(pMin.x, pMin.y);
    drawHandle(pMax.x, pMin.y);
    drawHandle(pMin.x, pMax.y);
    drawHandle(pMax.x, pMax.y);

    // Measurement badge
    char badgeBuf[128];
    sprintf_s(badgeBuf, "%s  RefPos:(%.0f, %.0f)  RefSize:(%.0f, %.0f)", typeTag, m_position.x, m_position.y, m_size.x, m_size.y);
    ImVec2 badgeSize = ImGui::CalcTextSize(badgeBuf);
    ImVec2 bMin(pMin.x, pMin.y - badgeSize.y - 6.0f);
    ImVec2 bMax(pMin.x + badgeSize.x + 8.0f, pMin.y - 2.0f);
    drawList->AddRectFilled(bMin, bMax, IM_COL32(10, 20, 35, 220), 4.0f);
    drawList->AddText(ImVec2(bMin.x + 4.0f, bMin.y + 1.0f), borderColor, badgeBuf);
}