#include "pch.h"
#include <filesystem>
#include <cmath>
#include <algorithm>
#include "UIButtonComponent.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Components/StaticModelComponent.h"
#include "Components/SoundComponent.h"
#include "Entities/Actor.h"
#include "DebugingTools/DebugUIManager.h"
#include "DebugingTools/EditorUtils.h"
#if HEIN_WITH_EDITOR
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#if HEIN_WITH_EDITOR
#include "ImGui/ImGuizmo.h"
#endif
#endif

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

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_cachedEldenGlowTexture;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrCreateEldenGlowTexture(ID3D11Device* device)
    {
        if (s_cachedEldenGlowTexture)
        {
            return s_cachedEldenGlowTexture;
        }
        if (!device) return nullptr;

        constexpr UINT texW = 512;
        constexpr UINT texH = 64;
        std::vector<uint32_t> pixels(texW * texH);

        for (UINT y = 0; y < texH; ++y)
        {
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(texH) * 2.0f - 1.0f;
            float cosV = std::max(0.0f, std::cos(v * 1.57079632679f));
            float vWeight = std::pow(cosV, 2.2f);

            for (UINT x = 0; x < texW; ++x)
            {
                float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(texW) * 2.0f - 1.0f;
                float cosU = std::max(0.0f, std::cos(u * 1.57079632679f));
                float uWeight = std::pow(cosU, 1.6f);

                float alpha = std::clamp(uWeight * vWeight, 0.0f, 1.0f);
                uint8_t aByte = static_cast<uint8_t>(alpha * 255.0f);

                pixels[y * texW + x] = (static_cast<uint32_t>(aByte) << 24) | 0x00FFFFFF;
            }
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = texW;
        desc.Height = texH;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = texW * sizeof(uint32_t);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device->CreateTexture2D(&desc, &initData, tex.GetAddressOf());
        if (FAILED(hr)) return nullptr;

        hr = device->CreateShaderResourceView(tex.Get(), nullptr, s_cachedEldenGlowTexture.GetAddressOf());
        if (FAILED(hr)) return nullptr;

        return s_cachedEldenGlowTexture;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_cachedWhiteTexture;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrCreateWhiteTexture(ID3D11Device* device)
    {
        if (s_cachedWhiteTexture) return s_cachedWhiteTexture;
        if (!device) return nullptr;
        s_cachedWhiteTexture = CreateSolidTexture(device, 0xFFFFFFFF);
        return s_cachedWhiteTexture;
    }

    std::unordered_map<std::wstring, std::shared_ptr<DirectX::SpriteFont>> s_uiButtonFontCache;
    std::vector<std::wstring> s_discoveredFontPaths;
}

namespace HEIN
{

bool HEIN::UIButtonComponent::DoesFontExist(const std::wstring& path)
{
    return HEIN::EditorUtils::DoesFontFileExistOnDisk(path);
}

void HEIN::UIButtonComponent::RefreshDiscoveredFonts()
{
    s_discoveredFontPaths.clear();

    // Purge cached entries for fonts that no longer exist on disk
    for (auto it = s_uiButtonFontCache.begin(); it != s_uiButtonFontCache.end();)
    {
        if (!DoesFontExist(it->first))
        {
            it = s_uiButtonFontCache.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Dynamic scanning of font directories on disk
    std::vector<std::string> searchDirs = {
        "Resources/Font",
        "Dual/Resources/Font",
        "../Dual/Resources/Font"
    };

    for (const auto& dir : searchDirs)
    {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
        {
            try
            {
                for (const auto& entry : std::filesystem::directory_iterator(dir))
                {
                    if (entry.is_regular_file() && entry.path().extension() == ".spritefont")
                    {
                        std::wstring relPath = entry.path().wstring();
                        for (auto& ch : relPath) { if (ch == L'\\') ch = L'/'; }
                        auto pos = relPath.find(L"Resources/Font/");
                        if (pos != std::wstring::npos)
                        {
                            relPath = relPath.substr(pos);
                        }

                        bool alreadyDiscovered = false;
                        for (const auto& existing : s_discoveredFontPaths)
                        {
                            if (existing == relPath)
                            {
                                alreadyDiscovered = true;
                                break;
                            }
                        }
                        if (!alreadyDiscovered)
                        {
                            s_discoveredFontPaths.push_back(relPath);
                        }
                    }
                }
            }
            catch (...)
            {
            }
        }
    }
}

static void EnsureFontsDiscovered()
{
    HEIN::UIButtonComponent::RefreshDiscoveredFonts();
}

std::vector<std::wstring> HEIN::UIButtonComponent::GetDiscoveredFontPaths()
{
    RefreshDiscoveredFonts();
    return s_discoveredFontPaths;
}

void HEIN::UIButtonComponent::RegisterDiscoveredFont(const std::wstring& path)
{
    if (path.empty() || !DoesFontExist(path)) return;

    for (const auto& existing : s_discoveredFontPaths)
    {
        if (existing == path) return;
    }
    s_discoveredFontPaths.push_back(path);
}

std::shared_ptr<DirectX::SpriteFont> HEIN::UIButtonComponent::GetOrCreateFont(ID3D11Device* device, const std::wstring& path)
{
    if (!device || path.empty()) return nullptr;

    if (!DoesFontExist(path))
    {
        s_uiButtonFontCache.erase(path);
        return nullptr;
    }

    auto it = s_uiButtonFontCache.find(path);
    if (it != s_uiButtonFontCache.end() && it->second) return it->second;

    std::filesystem::path p(path);
    std::wstring filename = p.filename().wstring();

    std::vector<std::wstring> candidates = {
        path,
        std::wstring(L"Dual/") + path,
        std::wstring(L"../Dual/") + path,
        std::wstring(L"Resources/Font/") + filename,
        std::wstring(L"Dual/Resources/Font/") + filename,
        std::wstring(L"../Dual/Resources/Font/") + filename
    };

    for (const auto& candidate : candidates)
    {
        try
        {
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate))
            {
                auto font = std::make_shared<DirectX::SpriteFont>(device, candidate.c_str());
                if (font)
                {
                    font->SetDefaultCharacter(L'?');
                    s_uiButtonFontCache[path] = font;
                    RegisterDiscoveredFont(path);
                    return font;
                }
            }
        }
        catch (...)
        {
        }
    }
    return nullptr;
}

void HEIN::UIButtonComponent::LoadFont(ID3D11Device* device, const std::wstring& path)
{
    if (path.empty() || !DoesFontExist(path))
    {
        m_spriteFontPath = L"";
        m_spriteFont.reset();
        return;
    }

    m_spriteFontPath = path;
    if (!device)
    {
        m_spriteFont.reset();
        return;
    }

    m_spriteFont = GetOrCreateFont(device, m_spriteFontPath);
    if (!m_spriteFont)
    {
        m_spriteFontPath = L"";
    }
}

void HEIN::UIButtonComponent::SetSpriteFontPath(const std::wstring& path, ID3D11Device* device)
{
    if (path.empty() || !DoesFontExist(path))
    {
        m_spriteFontPath = L"";
        m_spriteFont.reset();
        return;
    }

    m_spriteFontPath = path;
    if (device)
    {
        LoadFont(device, path);
    }
    else
    {
        m_spriteFont.reset();
    }
}

D3D11_VIEWPORT HEIN::UIButtonComponent::s_activeUIViewport = {};

void HEIN::UIButtonComponent::SetActiveUIViewport(const D3D11_VIEWPORT& vp)
{
    s_activeUIViewport = vp;
}

D3D11_VIEWPORT HEIN::UIButtonComponent::GetActiveUIViewport()
{
    return s_activeUIViewport;
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
        HEIN::TryLoadCachedTextureFile(device, path, srv.ReleaseAndGetAddressOf());
    };

    loadTexture(m_normalTexPath, m_normalTexture);
    loadTexture(m_hoverTexPath, m_hoverTexture);
    loadTexture(m_pressedTexPath, m_pressedTexture);

    // Only create fallback background box textures if explicitly requested by the user
    if (m_elementType == UIElementType::Button && m_drawFallbackBox)
    {
        if (!m_normalTexture)  m_normalTexture = CreateSolidTexture(device, 0xEE3A2E20);
        if (!m_hoverTexture)   m_hoverTexture = CreateSolidTexture(device, 0xFF654E35);
        if (!m_pressedTexture) m_pressedTexture = CreateSolidTexture(device, 0xFF9E7E5A);
    }

    GetOrCreateEldenGlowTexture(device);

    if (!m_spriteFontPath.empty() && !DoesFontExist(m_spriteFontPath))
    {
        m_spriteFontPath = L"";
    }
    LoadFont(device, m_spriteFontPath);
}

void HEIN::UIButtonComponent::Start()
{
}

void HEIN::UIButtonComponent::Update(float deltaTime)
{
    if (!m_isVisible)
    {
        m_currentState = ButtonState::Normal;
        m_previousState = ButtonState::Normal;
        m_isClicked = false;
        return;
    }

    m_animTime += deltaTime;

    // Reset clicked flag every frame
    m_isClicked = false;

    // Non-button elements don't interact with mouse clicks
    if (m_elementType != UIElementType::Button)
    {
        m_currentState = ButtonState::Normal;
        m_previousState = ButtonState::Normal;
        m_hoverProgress = 0.0f;
        return;
    }

#if HEIN_WITH_EDITOR
    // Keyboard shortcuts configured per button (empty list = mouse-only button)
    for (int key : m_submitKeys)
    {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
        {
            m_isClicked = true;
            break;
        }
    }

    // Ignore clicks when ImGuizmo is actively using gizmo controls
    bool isUICapturingMouse = ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
    if (isUICapturingMouse)
    {
        m_wasMouseDown = false;
        m_currentState = ButtonState::Normal;
        m_previousState = ButtonState::Normal;
        float targetProgress = 0.0f;
        m_hoverProgress += (targetProgress - m_hoverProgress) * std::min(1.0f, 12.0f * deltaTime);
        return;
    }
#endif

    // Grab true client window cursor position
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool hasValidMousePos = false;

#if HEIN_WITH_EDITOR
    if (ImGui::IsMousePosValid(&ImGui::GetIO().MousePos))
    {
        mouseX = ImGui::GetIO().MousePos.x;
        mouseY = ImGui::GetIO().MousePos.y;
        hasValidMousePos = true;
    }
    else
#endif
    {
        HWND hwnd = GetActiveWindow();
        if (!hwnd) hwnd = GetForegroundWindow();
        POINT pt;
        if (hwnd && GetCursorPos(&pt) && ScreenToClient(hwnd, &pt))
        {
            mouseX = static_cast<float>(pt.x);
            mouseY = static_cast<float>(pt.y);
            hasValidMousePos = true;
        }
    }

    auto mouseState = DirectX::Mouse::Get().GetState();
    if (!hasValidMousePos)
    {
        mouseX = static_cast<float>(mouseState.x);
        mouseY = static_cast<float>(mouseState.y);
    }

    float vpX = (m_lastScaleX > 0.0001f) ? m_lastVpX : s_activeUIViewport.TopLeftX;
    float vpY = (m_lastScaleY > 0.0001f) ? m_lastVpY : s_activeUIViewport.TopLeftY;
#if HEIN_WITH_EDITOR
    float scaleX = (m_lastScaleX > 0.0001f) ? m_lastScaleX : (s_activeUIViewport.Width > 0.0f ? s_activeUIViewport.Width / kRefWidth : (ImGui::GetIO().DisplaySize.x > 0.0f ? ImGui::GetIO().DisplaySize.x / kRefWidth : 1.0f));
    float scaleY = (m_lastScaleY > 0.0001f) ? m_lastScaleY : (s_activeUIViewport.Height > 0.0f ? s_activeUIViewport.Height / kRefHeight : (ImGui::GetIO().DisplaySize.y > 0.0f ? ImGui::GetIO().DisplaySize.y / kRefHeight : 1.0f));
#else
    float scaleX = (m_lastScaleX > 0.0001f) ? m_lastScaleX : (s_activeUIViewport.Width > 0.0f ? s_activeUIViewport.Width / kRefWidth : 1.0f);
    float scaleY = (m_lastScaleY > 0.0001f) ? m_lastScaleY : (s_activeUIViewport.Height > 0.0f ? s_activeUIViewport.Height / kRefHeight : 1.0f);
#endif
    if (scaleX <= 0.0001f) scaleX = 1.0f;
    if (scaleY <= 0.0001f) scaleY = 1.0f;

    // Transform mouse coordinates from window space into virtual reference canvas (1280x720) space
    float canvasMouseX = (mouseX - vpX) / scaleX;
    float canvasMouseY = (mouseY - vpY) / scaleY;

    // 2D AABB Collision Math against 1280x720 canvas
    bool isHovering = (canvasMouseX >= m_position.x && canvasMouseX <= m_position.x + m_size.x &&
        canvasMouseY >= m_position.y && canvasMouseY <= m_position.y + m_size.y);

    bool isMouseDown = mouseState.leftButton ||
                       ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0)
#if HEIN_WITH_EDITOR
                       || ImGui::IsMouseDown(ImGuiMouseButton_Left)
#endif
                       ;

    if (isHovering)
    {
        if (isMouseDown)
        {
            m_currentState = ButtonState::Pressed;
        }
        else
        {
            m_currentState = ButtonState::Hover;

            // If mouse was down last frame, but isn't this frame (Mouse Up/Release) -> Trigger Click
            if (m_wasMouseDown)
            {
                m_isClicked = true;
            }
        }

#if HEIN_WITH_EDITOR
        // Also register immediate click if triggered this frame
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_isClicked = true;
        }
#endif
    }
    else
    {
        m_currentState = ButtonState::Normal;
    }

    m_wasMouseDown = isMouseDown;

    // - Dispatch rising-edge Hover and Click sound cues via the owning Actor's SoundComponent
    if (m_owner != nullptr)
    {
        if (auto* soundComp = m_owner->GetComponent<SoundComponent>())
        {
            if (m_isClicked && !m_clickSoundCue.empty())
            {
                soundComp->PlayCueOrFallbackPath(m_clickSoundCue, AudioBus::UISFX, 1.0f);
            }
            else if (m_previousState == ButtonState::Normal && m_currentState == ButtonState::Hover && !m_hoverSoundCue.empty())
            {
                soundComp->PlayCueOrFallbackPath(m_hoverSoundCue, AudioBus::UISFX, 1.0f);
            }
        }
    }
    m_previousState = m_currentState;

    // Smooth hover progress interpolation for fluid Elden Ring light-up transition
    float targetProgress = (m_currentState == ButtonState::Hover || m_currentState == ButtonState::Pressed) ? 1.0f : 0.0f;
    float lerpSpeed = 12.0f;
    m_hoverProgress += (targetProgress - m_hoverProgress) * std::min(1.0f, lerpSpeed * deltaTime);
}

void HEIN::UIButtonComponent::Draw2D(GameContext& gameContext)
{
    if (!m_isVisible || !gameContext.showUI || gameContext.isSceneViewPass) return;
    Draw(gameContext, DirectX::SimpleMath::Matrix::Identity, DirectX::SimpleMath::Matrix::Identity, DirectX::SimpleMath::Matrix::Identity);
}

void HEIN::UIButtonComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& /*world*/, const DirectX::SimpleMath::Matrix& /*view*/, const DirectX::SimpleMath::Matrix& /*proj*/)
{
    if (!m_isVisible || !gameContext.showUI || gameContext.isSceneViewPass) return;

    // Query current active viewport to support responsive scaling in full view, windowed mode, and sub-viewports
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
    s_activeUIViewport = vp;

    float renderPosX = m_position.x * m_lastScaleX;
    float renderPosY = m_position.y * m_lastScaleY;
    float renderSizeX = m_size.x * m_lastScaleX;
    float renderSizeY = m_size.y * m_lastScaleY;

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    // Texture Pass: custom textures only when assigned or explicitly requested
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
            drawRect.left = static_cast<LONG>(std::floor(renderPosX + 0.5f));
            drawRect.top = static_cast<LONG>(std::floor(renderPosY + 0.5f));
            drawRect.right = static_cast<LONG>(std::floor(renderPosX + renderSizeX + 0.5f));
            drawRect.bottom = static_cast<LONG>(std::floor(renderPosY + renderSizeY + 0.5f));

            DirectX::SimpleMath::Vector4 tint = m_tintColor;

            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());
            m_spriteBatch->Draw(textureToDraw, drawRect, nullptr, tint);
            m_spriteBatch->End();
            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
        }
    }

    // Elden Ring Light-Up Horizontal Beam Pass
    if (m_elementType == UIElementType::Button && m_useEldenStyle && m_hoverProgress > 0.001f && m_spriteBatch)
    {
        auto flareTex = GetOrCreateEldenGlowTexture(device);
        if (flareTex)
        {
            float beamW = renderSizeX * m_beamWidthScale;
            float beamH = renderSizeY * m_beamHeightScale;
            float beamX = renderPosX + (renderSizeX - beamW) * 0.5f;
            float beamY = renderPosY + (renderSizeY - beamH) * 0.5f;

            RECT flareRect;
            flareRect.left = static_cast<LONG>(std::floor(beamX + 0.5f));
            flareRect.top = static_cast<LONG>(std::floor(beamY + 0.5f));
            flareRect.right = static_cast<LONG>(std::floor(beamX + beamW + 0.5f));
            flareRect.bottom = static_cast<LONG>(std::floor(beamY + beamH + 0.5f));

            float pulse = 0.88f + 0.12f * std::sin(m_animTime * 3.5f);
            if (m_currentState == ButtonState::Pressed)
            {
                pulse *= 1.35f;
            }

            DirectX::SimpleMath::Vector4 flareColor = m_glowColor;
            flareColor.w *= std::clamp(m_hoverProgress * pulse, 0.0f, 1.0f);

            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());
            m_spriteBatch->Draw(flareTex.Get(), flareRect, nullptr, flareColor);
            m_spriteBatch->End();
            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
        }
    }

    // Text Pass: Text elements or labeled buttons
    if (!m_text.empty())
    {
        float uiScale = std::min(m_lastScaleX, m_lastScaleY);

        DirectX::SimpleMath::Vector4 activeTextColor = m_textColor;
        if (m_elementType == UIElementType::Button && m_useEldenStyle)
        {
            activeTextColor = DirectX::SimpleMath::Vector4::Lerp(m_normalTextColor, m_hoverTextColor, m_hoverProgress);
        }

        if (m_spriteFont && m_spriteBatch)
        {
            int len = MultiByteToWideChar(CP_UTF8, 0, m_text.c_str(), -1, nullptr, 0);
            std::wstring wtext(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, m_text.c_str(), -1, &wtext[0], len);
            if (!wtext.empty() && wtext.back() == L'\0') wtext.pop_back();

            float fontScale = m_fontSize * uiScale;
            DirectX::SimpleMath::Vector2 rawTextSize = m_spriteFont->MeasureString(wtext.c_str());
            DirectX::SimpleMath::Vector2 textSize(rawTextSize.x * fontScale, rawTextSize.y * fontScale);

            DirectX::SimpleMath::Vector2 textPos;
            if (m_elementType == UIElementType::Button)
            {
                textPos = DirectX::SimpleMath::Vector2(
                    renderPosX + (renderSizeX - textSize.x) * 0.5f + m_textOffset.x * m_lastScaleX,
                    renderPosY + (renderSizeY - textSize.y) * 0.5f + m_textOffset.y * m_lastScaleY
                );
            }
            else
            {
                textPos = DirectX::SimpleMath::Vector2(
                    renderPosX + m_textOffset.x * m_lastScaleX,
                    renderPosY + m_textOffset.y * m_lastScaleY
                );
            }

            // Snap text position to whole pixels to prevent sub-pixel sampling blur
            textPos.x = std::floor(textPos.x + 0.5f);
            textPos.y = std::floor(textPos.y + 0.5f);

            DirectX::SimpleMath::Vector4 shadowCol(0.0f, 0.0f, 0.0f, activeTextColor.w * 0.85f);
            float shadowOff = std::max(1.0f, std::floor(1.5f * uiScale + 0.5f));

            float pulse = 0.88f + 0.12f * std::sin(m_animTime * 3.5f);

            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthNone(), 0);
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, gameContext.commonStates.NonPremultiplied());

            // Soft shadow pass behind text
            m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                DirectX::SimpleMath::Vector2(textPos.x + shadowOff, textPos.y + shadowOff),
                shadowCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);

            // Radiant golden text bloom pass for Elden Ring light-up effect
            if (m_elementType == UIElementType::Button && m_useEldenStyle && m_hoverProgress > 0.05f)
            {
                DirectX::SimpleMath::Vector4 glowPassColor = m_glowColor;
                glowPassColor.w *= (0.28f * m_hoverProgress * pulse);

                float bloomSpread = std::max(1.0f, std::floor(1.0f * uiScale + 0.5f));
                m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                    DirectX::SimpleMath::Vector2(textPos.x - bloomSpread, textPos.y),
                    glowPassColor, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
                m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                    DirectX::SimpleMath::Vector2(textPos.x + bloomSpread, textPos.y),
                    glowPassColor, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
                m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                    DirectX::SimpleMath::Vector2(textPos.x, textPos.y - bloomSpread),
                    glowPassColor, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);
                m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                    DirectX::SimpleMath::Vector2(textPos.x, textPos.y + bloomSpread),
                    glowPassColor, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);

                // Flanking diamond accents when hovered
                if (m_drawDiamonds)
                {
                    auto whiteTex = GetOrCreateWhiteTexture(device);
                    if (whiteTex)
                    {
                        DirectX::SimpleMath::Vector4 diamondColor = m_hoverTextColor;
                        diamondColor.w *= (m_hoverProgress * pulse);

                        float markerOffset = std::floor(24.0f * uiScale + 0.5f);
                        float diamondSide = std::max(4.0f, std::floor(7.0f * fontScale + 0.5f));
                        DirectX::SimpleMath::Vector2 dScale(diamondSide, diamondSide);
                        DirectX::SimpleMath::Vector2 dOrigin(0.5f, 0.5f);

                        DirectX::SimpleMath::Vector2 leftMarkerPos(textPos.x - markerOffset, textPos.y + textSize.y * 0.5f);
                        DirectX::SimpleMath::Vector2 rightMarkerPos(textPos.x + textSize.x + markerOffset, textPos.y + textSize.y * 0.5f);

                        // Soft diamond glow halo
                        DirectX::SimpleMath::Vector4 dGlow = m_glowColor;
                        dGlow.w *= (0.35f * m_hoverProgress * pulse);
                        DirectX::SimpleMath::Vector2 dGlowScale(diamondSide * 1.5f, diamondSide * 1.5f);
                        m_spriteBatch->Draw(whiteTex.Get(), leftMarkerPos, nullptr, dGlow, 0.785398f, dOrigin, dGlowScale);
                        m_spriteBatch->Draw(whiteTex.Get(), rightMarkerPos, nullptr, dGlow, 0.785398f, dOrigin, dGlowScale);

                        // Sharp diamond core
                        m_spriteBatch->Draw(whiteTex.Get(), leftMarkerPos, nullptr, diamondColor, 0.785398f, dOrigin, dScale);
                        m_spriteBatch->Draw(whiteTex.Get(), rightMarkerPos, nullptr, diamondColor, 0.785398f, dOrigin, dScale);
                    }
                }
            }

            // Razor-sharp foreground text
            m_spriteFont->DrawString(m_spriteBatch.get(), wtext.c_str(),
                textPos, activeTextColor, 0.0f, DirectX::SimpleMath::Vector2::Zero, fontScale);

            m_spriteBatch->End();
            gameContext.deviceResources.GetD3DDeviceContext()->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
        }
        else
        {
#if HEIN_WITH_EDITOR
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            if (drawList)
            {
                ImU32 col = IM_COL32(
                    static_cast<int>(activeTextColor.x * 255.0f),
                    static_cast<int>(activeTextColor.y * 255.0f),
                    static_cast<int>(activeTextColor.z * 255.0f),
                    static_cast<int>(activeTextColor.w * 255.0f)
                );

                float fontSize = ImGui::GetFontSize() * m_fontSize * uiScale;
                ImVec2 textSize = ImGui::CalcTextSize(m_text.c_str());
                textSize.x *= (m_fontSize * uiScale);
                textSize.y *= (m_fontSize * uiScale);

                ImVec2 textPos;
                if (m_elementType == UIElementType::Button)
                {
                    textPos = ImVec2(
                        m_lastVpX + renderPosX + (renderSizeX - textSize.x) * 0.5f + m_textOffset.x * m_lastScaleX,
                        m_lastVpY + renderPosY + (renderSizeY - textSize.y) * 0.5f + m_textOffset.y * m_lastScaleY
                    );
                }
                else
                {
                    textPos = ImVec2(
                        m_lastVpX + renderPosX + m_textOffset.x * m_lastScaleX,
                        m_lastVpY + renderPosY + m_textOffset.y * m_lastScaleY
                    );
                }

                textPos.x = std::floor(textPos.x + 0.5f);
                textPos.y = std::floor(textPos.y + 0.5f);

                float shadowOff = std::max(1.0f, std::floor(1.5f * uiScale + 0.5f));
                drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(textPos.x + shadowOff, textPos.y + shadowOff), IM_COL32(0, 0, 0, static_cast<int>(activeTextColor.w * 190.0f)), m_text.c_str());
                drawList->AddText(ImGui::GetFont(), fontSize, textPos, col, m_text.c_str());
            }
#endif
        }
    }
}

bool HEIN::UIButtonComponent::IsClicked()
{
    return m_isClicked;
}

nlohmann::json HEIN::UIButtonComponent::Serialize()
{
    // Clean out deleted font path so it is never kept in the resource data
    if (!m_spriteFontPath.empty() && !DoesFontExist(m_spriteFontPath))
    {
        m_spriteFontPath = L"";
        m_spriteFont.reset();
    }

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
    data["TextOffsetX"] = m_textOffset.x;
    data["TextOffsetY"] = m_textOffset.y;

    data["UseEldenStyle"] = m_useEldenStyle;
    data["DrawFallbackBox"] = m_drawFallbackBox;
    data["DrawDiamonds"] = m_drawDiamonds;
    data["BeamWidthScale"] = m_beamWidthScale;
    data["BeamHeightScale"] = m_beamHeightScale;

    data["NormalTextColorR"] = m_normalTextColor.x;
    data["NormalTextColorG"] = m_normalTextColor.y;
    data["NormalTextColorB"] = m_normalTextColor.z;
    data["NormalTextColorA"] = m_normalTextColor.w;

    data["HoverTextColorR"] = m_hoverTextColor.x;
    data["HoverTextColorG"] = m_hoverTextColor.y;
    data["HoverTextColorB"] = m_hoverTextColor.z;
    data["HoverTextColorA"] = m_hoverTextColor.w;

    data["GlowColorR"] = m_glowColor.x;
    data["GlowColorG"] = m_glowColor.y;
    data["GlowColorB"] = m_glowColor.z;
    data["GlowColorA"] = m_glowColor.w;

    if (!m_spriteFontPath.empty() && DoesFontExist(m_spriteFontPath))
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, m_spriteFontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string s(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, m_spriteFontPath.c_str(), -1, &s[0], len, nullptr, nullptr);
        if (!s.empty() && s.back() == '\0') s.pop_back();
        data["SpriteFontPath"] = s;
    }
    else
    {
        data["SpriteFontPath"] = "";
    }
    data["TintR"] = m_tintColor.x;
    data["TintG"] = m_tintColor.y;
    data["TintB"] = m_tintColor.z;
    data["TintA"] = m_tintColor.w;
    data["NormalTex"] = std::string(m_normalTexPath.begin(), m_normalTexPath.end());
    data["HoverTex"] = std::string(m_hoverTexPath.begin(), m_hoverTexPath.end());
    data["PressedTex"] = std::string(m_pressedTexPath.begin(), m_pressedTexPath.end());
    data["IsVisible"] = m_isVisible;
    data["SubmitKeys"] = m_submitKeys;
    data["HoverSoundCue"] = m_hoverSoundCue;
    data["ClickSoundCue"] = m_clickSoundCue;
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
    if (data.contains("TextOffsetX")) m_textOffset.x = data["TextOffsetX"];
    if (data.contains("TextOffsetY")) m_textOffset.y = data["TextOffsetY"];

    if (data.contains("UseEldenStyle")) m_useEldenStyle = data["UseEldenStyle"];
    if (data.contains("DrawFallbackBox")) m_drawFallbackBox = data["DrawFallbackBox"];
    if (data.contains("DrawDiamonds")) m_drawDiamonds = data["DrawDiamonds"];
    if (data.contains("BeamWidthScale")) m_beamWidthScale = data["BeamWidthScale"];
    if (data.contains("BeamHeightScale")) m_beamHeightScale = data["BeamHeightScale"];

    if (data.contains("NormalTextColorR"))
    {
        m_normalTextColor.x = data["NormalTextColorR"];
        m_normalTextColor.y = data["NormalTextColorG"];
        m_normalTextColor.z = data["NormalTextColorB"];
        m_normalTextColor.w = data.value("NormalTextColorA", 0.90f);
    }
    if (data.contains("HoverTextColorR"))
    {
        m_hoverTextColor.x = data["HoverTextColorR"];
        m_hoverTextColor.y = data["HoverTextColorG"];
        m_hoverTextColor.z = data["HoverTextColorB"];
        m_hoverTextColor.w = data.value("HoverTextColorA", 1.0f);
    }
    if (data.contains("GlowColorR"))
    {
        m_glowColor.x = data["GlowColorR"];
        m_glowColor.y = data["GlowColorG"];
        m_glowColor.z = data["GlowColorB"];
        m_glowColor.w = data.value("GlowColorA", 0.70f);
    }

    if (data.contains("SpriteFontPath"))
    {
        std::string path = data["SpriteFontPath"];
        int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        m_spriteFontPath.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &m_spriteFontPath[0], len);
        if (!m_spriteFontPath.empty() && m_spriteFontPath.back() == L'\0') m_spriteFontPath.pop_back();

        // Prune path if the file does not exist on disk
        if (!m_spriteFontPath.empty() && !DoesFontExist(m_spriteFontPath))
        {
            m_spriteFontPath = L"";
        }
    }
    if (data.contains("IsVisible")) m_isVisible = data["IsVisible"];
    // Older scene files have no SubmitKeys entry and stay mouse-only
    m_submitKeys.clear();
    if (data.contains("SubmitKeys") && data["SubmitKeys"].is_array())
    {
        for (const auto& key : data["SubmitKeys"])
        {
            if (key.is_number_integer()) m_submitKeys.push_back(key.get<int>());
        }
    }

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
    if (data.contains("HoverSoundCue") && data["HoverSoundCue"].is_string())
    {
        m_hoverSoundCue = data["HoverSoundCue"].get<std::string>();
    }
    if (data.contains("ClickSoundCue") && data["ClickSoundCue"].is_string())
    {
        m_clickSoundCue = data["ClickSoundCue"].get<std::string>();
    }
}

void HEIN::UIButtonComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
    if (!m_spriteFontPath.empty() && !DoesFontExist(m_spriteFontPath))
    {
        m_spriteFontPath = L"";
    }
    LoadFont(gameContext.deviceResources.GetD3DDevice(), m_spriteFontPath);
}

#if HEIN_WITH_EDITOR
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

        // Keyboard shortcuts: keys that also activate this button (empty = mouse only)
        if (m_elementType == UIElementType::Button)
        {
            ImGui::Separator();
            ImGui::Text("Keyboard Shortcuts:");
            if (m_submitKeys.empty())
            {
                ImGui::TextDisabled("None (left mouse click only)");
            }
            for (size_t i = 0; i < m_submitKeys.size(); )
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::BulletText("%s", ImGui::GetKeyName(static_cast<ImGuiKey>(m_submitKeys[i])));
                ImGui::SameLine();
                bool removed = ImGui::SmallButton("Remove");
                ImGui::PopID();
                if (removed)
                {
                    m_submitKeys.erase(m_submitKeys.begin() + i);
                    continue;
                }
                ++i;
            }

            if (!m_isCapturingSubmitKey)
            {
                if (ImGui::Button("Add Shortcut Key")) m_isCapturingSubmitKey = true;
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Press any keyboard key...");
                ImGui::SameLine();
                if (ImGui::SmallButton("Cancel##SubmitKey"))
                {
                    m_isCapturingSubmitKey = false;
                }
                else
                {
                    // Keyboard keys occupy the named-key range before the gamepad block
                    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_GamepadStart; ++key)
                    {
                        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
                        {
                            if (std::find(m_submitKeys.begin(), m_submitKeys.end(), key) == m_submitKeys.end())
                            {
                                m_submitKeys.push_back(key);
                            }
                            m_isCapturingSubmitKey = false;
                            break;
                        }
                    }
                }
            }
        }

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

        // Elden Ring Visual Styling Controls
        if (m_elementType == UIElementType::Button)
        {
            ImGui::Separator();
            if (ImGui::TreeNodeEx("Elden Ring Light-Up Button Effect", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Checkbox("Enable Elden Ring Style", &m_useEldenStyle);
                if (m_useEldenStyle)
                {
                    ImGui::ColorEdit4("Normal Text Color (Parchment)", &m_normalTextColor.x);
                    ImGui::ColorEdit4("Focused Text Color (Luminous)", &m_hoverTextColor.x);
                    ImGui::ColorEdit4("Horizontal Beam Glow Color", &m_glowColor.x);
                    ImGui::Checkbox("Draw Flanking Diamonds (< * >)", &m_drawDiamonds);
                    ImGui::DragFloat("Beam Width Scale", &m_beamWidthScale, 0.05f, 0.5f, 3.0f, "%.2fx");
                    ImGui::DragFloat("Beam Height Scale", &m_beamHeightScale, 0.05f, 0.5f, 3.0f, "%.2fx");
                }
                ImGui::Checkbox("Draw Fallback Background Box", &m_drawFallbackBox);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Button Sound Cues (SoundComponent)", ImGuiTreeNodeFlags_DefaultOpen))
            {
                SoundComponent* soundComp = m_owner ? m_owner->GetComponent<SoundComponent>() : nullptr;
                if (!soundComp)
                {
                    ImGui::TextDisabled("No SoundComponent attached to this Button Actor yet.");
                    if (m_owner && ImGui::Button("+ Attach SoundComponent with UI Button Preset"))
                    {
                        soundComp = m_owner->AddComponent<SoundComponent>();
                        soundComp->ApplyUIButtonPreset();
                        soundComp->Start();
                    }
                }
                else
                {
                    const auto& cues = soundComp->GetCues();
                    if (!cues.empty())
                    {
                        if (ImGui::BeginCombo("Hover Sound Cue", m_hoverSoundCue.empty() ? "<None>" : m_hoverSoundCue.c_str()))
                        {
                            for (const auto& c : cues)
                            {
                                bool isSel = (m_hoverSoundCue == c.cueName);
                                if (ImGui::Selectable(c.cueName.c_str(), isSel))
                                {
                                    m_hoverSoundCue = c.cueName;
                                }
                                if (isSel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        if (ImGui::BeginCombo("Click Sound Cue", m_clickSoundCue.empty() ? "<None>" : m_clickSoundCue.c_str()))
                        {
                            for (const auto& c : cues)
                            {
                                bool isSel = (m_clickSoundCue == c.cueName);
                                if (ImGui::Selectable(c.cueName.c_str(), isSel))
                                {
                                    m_clickSoundCue = c.cueName;
                                }
                                if (isSel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                    ImGui::InputText("Hover Cue Name", &m_hoverSoundCue);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Test Hover"))
                    {
                        soundComp->PlayCueOrFallbackPath(m_hoverSoundCue, AudioBus::UISFX, 1.0f);
                    }
                    ImGui::InputText("Click Cue Name", &m_clickSoundCue);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Test Click"))
                    {
                        soundComp->PlayCueOrFallbackPath(m_clickSoundCue, AudioBus::UISFX, 1.0f);
                    }
                }
                ImGui::TreePop();
            }
        }

        // Text & Typography Controls for Button and Text
        if (m_elementType == UIElementType::Button || m_elementType == UIElementType::Text)
        {
            ImGui::Separator();
            ImGui::Text(m_elementType == UIElementType::Button ? "Button Label & Typography Settings:" : "Text & Typography Settings:");
            ImGui::InputText(m_elementType == UIElementType::Button ? "Button Text" : "Content Text", &m_text);
            if (!m_useEldenStyle || m_elementType != UIElementType::Button)
            {
                ImGui::ColorEdit4("Text Color", &m_textColor.x);
            }
            ImGui::DragFloat("Font Size / Scale", &m_fontSize, 0.02f, 0.1f, 10.0f, "%.2fx");
            ImGui::DragFloat2("Text Offset (X, Y)", &m_textOffset.x, 1.0f);

            // Dynamically refresh discovered font paths and prune any deleted files from the list
            RefreshDiscoveredFonts();

            // Verify active font path still exists on disk; if deleted, remove path keeping
            if (!m_spriteFontPath.empty() && !DoesFontExist(m_spriteFontPath))
            {
                m_spriteFontPath = L"";
                m_spriteFont.reset();
            }

            std::vector<std::string> comboItems;
            comboItems.push_back("Default (ImGui)");
            int currentFontIdx = 0;

            int lenCurrent = WideCharToMultiByte(CP_UTF8, 0, m_spriteFontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string currentPathStr(lenCurrent, 0);
            WideCharToMultiByte(CP_UTF8, 0, m_spriteFontPath.c_str(), -1, &currentPathStr[0], lenCurrent, nullptr, nullptr);
            if (!currentPathStr.empty() && currentPathStr.back() == '\0') currentPathStr.pop_back();

            for (size_t i = 0; i < s_discoveredFontPaths.size(); ++i)
            {
                int len = WideCharToMultiByte(CP_UTF8, 0, s_discoveredFontPaths[i].c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string fontName(len, 0);
                WideCharToMultiByte(CP_UTF8, 0, s_discoveredFontPaths[i].c_str(), -1, &fontName[0], len, nullptr, nullptr);
                if (!fontName.empty() && fontName.back() == '\0') fontName.pop_back();

                std::string displayName = fontName;
                auto slashPos = displayName.find_last_of("/\\");
                if (slashPos != std::string::npos)
                {
                    displayName = displayName.substr(slashPos + 1);
                }

                comboItems.push_back(displayName + " (" + fontName + ")");
                if (!m_spriteFontPath.empty() && s_discoveredFontPaths[i] == m_spriteFontPath)
                {
                    currentFontIdx = static_cast<int>(i + 1);
                }
            }

            std::vector<const char*> comboItemPtrs;
            for (const auto& item : comboItems)
            {
                comboItemPtrs.push_back(item.c_str());
            }

            if (ImGui::Combo("SpriteFont Choice", &currentFontIdx, comboItemPtrs.data(), static_cast<int>(comboItemPtrs.size())))
            {
                if (currentFontIdx == 0)
                {
                    m_spriteFontPath = L"";
                    m_spriteFont.reset();
                }
                else
                {
                    m_spriteFontPath = s_discoveredFontPaths[currentFontIdx - 1];
                    LoadFont(gameContext.deviceResources.GetD3DDevice(), m_spriteFontPath);
                }
            }

            // Function button to add a new SpriteFont file from disk: automatically imports to Resources/Font
            if (ImGui::Button("+ Add / Browse New SpriteFont File"))
            {
                std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"SpriteFont Files\0*.spritefont\0All Files\0*.*\0", windowHandle);
                if (!file.empty())
                {
                    m_spriteFontPath = HEIN::EditorUtils::ImportFontFileToResources(file);
                    RegisterDiscoveredFont(m_spriteFontPath);
                    LoadFont(gameContext.deviceResources.GetD3DDevice(), m_spriteFontPath);
                }
            }

            if (!m_spriteFontPath.empty())
            {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Active Font: %s", currentPathStr.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Clear Font"))
                {
                    m_spriteFontPath = L"";
                    m_spriteFont.reset();
                }
            }
            else
            {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Active Font: Built-in ImGui (Default)");
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Reload Textures / Refresh"))
        {
            Initialize(gameContext, m_normalTexPath.c_str(), m_hoverTexPath.c_str(), m_pressedTexPath.c_str());
        }
    }
}
#else
void HEIN::UIButtonComponent::OnInspectorGUI(GameContext& /*gameContext*/) {}
#endif

#if HEIN_WITH_EDITOR
void HEIN::UIButtonComponent::DrawGizmo(
    const DirectX::SimpleMath::Matrix& /*view*/,
    const DirectX::SimpleMath::Matrix& /*proj*/,
    int operation,
    int mode
)
{
    ImGuizmo::SetOrthographic(true);
    // Drawlist is configured by DebugUIManager for the active Scene View / Game View window
    float vpX = 0.0f, vpY = 0.0f, displayW = kRefWidth, displayH = kRefHeight;
    HEIN::DebugUIManager::GetActiveViewportRect(vpX, vpY, displayW, displayH);

    float scaleX = displayW / kRefWidth;
    float scaleY = displayH / kRefHeight;

    ImGuizmo::SetRect(vpX, vpY, displayW, displayH);
    ImGuizmo::SetGizmoSizeClipSpace(0.12f);

    // 2D Orthographic View & Projection for pixel screen space inside active viewport
    DirectX::SimpleMath::Matrix orthoView = DirectX::SimpleMath::Matrix::Identity;
    DirectX::SimpleMath::Matrix orthoProj = DirectX::SimpleMath::Matrix::CreateOrthographicOffCenter(
        vpX, vpX + displayW, vpY + displayH, vpY, -1000.0f, 1000.0f
    );

    float screenPosX = vpX + m_position.x * scaleX;
    float screenPosY = vpY + m_position.y * scaleY;
    float screenSizeX = m_size.x * scaleX;
    float screenSizeY = m_size.y * scaleY;

    // Center position for element matrix
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
            float newScreenPosX = (newPos.x - vpX) - newSizeX * 0.5f;
            float newScreenPosY = (newPos.y - vpY) - newSizeY * 0.5f;

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
#else
void HEIN::UIButtonComponent::DrawGizmo(const DirectX::SimpleMath::Matrix&, const DirectX::SimpleMath::Matrix&, int, int) {}
#endif

} // namespace HEIN
