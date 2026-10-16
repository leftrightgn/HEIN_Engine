#include "pch.h"
#include "SceneManager.h"
#include "Components/StaticModelComponent.h"
#include <malloc.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    std::string GetTransitionConfigWritePath()
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        std::string target = "Resources/Data/SceneTransition.json";
        if (!fs::exists("Resources", ec) && fs::exists("Dual/Resources", ec))
        {
            target = "Dual/Resources/Data/SceneTransition.json";
        }
        fs::create_directories(fs::path(target).parent_path(), ec);
        return target;
    }
}

namespace HEIN
{
    SceneManager::SceneManager()
        : m_transitionPhase(TransitionPhase::None)
        , m_transitionAlpha(0.0f)
        , m_loadingProgress(0.0f)
        , m_isPreviewOnly(false)
        , m_hasInitialBooted(false)
    {
        if (m_defaultTransitionDesc.tips.empty())
        {
            m_defaultTransitionDesc.tips = {
                { "Vessel Possession", "When your mortal body falls, press [Q] near an unoccupied vessel to claim its strength." },
                { "Perfect Parrying", "Raise your guard right before an attack lands to execute a perfect parry and break enemy posture." },
                { "Anti-Combo Burst", "Taking three consecutive unblocked hits triggers a defensive radial burst, repelling nearby foes." },
                { "Aerial Launch", "Heavy attacks and stunning strikes launch opponents skyward, creating juggle opportunities." }
            };
        }

        LoadTransitionConfig();
    }

    void SceneManager::EnsureWhiteTexture(ID3D11Device* device)
    {
        if (m_whiteTextureSRV || !device) return;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        uint32_t whitePixel = 0xFFFFFFFF;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &whitePixel;
        initData.SysMemPitch = sizeof(uint32_t);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(device->CreateTexture2D(&desc, &initData, tex.GetAddressOf())))
        {
            device->CreateShaderResourceView(tex.Get(), nullptr, m_whiteTextureSRV.ReleaseAndGetAddressOf());
        }
    }

    void SceneManager::EnsureSpriteBatch(ID3D11DeviceContext* context)
    {
        if (!m_spriteBatch && context)
        {
            m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
        }
    }

    void SceneManager::EnsureFont(ID3D11Device* device)
    {
        if (m_spriteFont || !device) return;

        std::vector<std::wstring> fontCandidates = {
            L"Resources/Font/SegoeUI_18.spritefont",
            L"Dual/Resources/Font/SegoeUI_18.spritefont",
            L"../Dual/Resources/Font/SegoeUI_18.spritefont"
        };

        for (const auto& candidate : fontCandidates)
        {
            try
            {
                m_spriteFont = std::make_unique<DirectX::SpriteFont>(device, candidate.c_str());
                if (m_spriteFont) break;
            }
            catch (...)
            {
                m_spriteFont.reset();
            }
        }
    }

    void SceneManager::EnsureTransitionTexture(ID3D11Device* device, const std::wstring& path)
    {
        if (path.empty() || !device) return;
        if (m_transitionTextureSRV && m_loadedTexturePath == path) return;

        m_transitionTextureSRV.Reset();
        m_loadedTexturePath = path;
        m_textureDimensions = DirectX::SimpleMath::Vector2(0.0f, 0.0f);

        std::vector<std::wstring> candidates = {
            path,
            L"Resources/" + path,
            L"Dual/Resources/" + path,
            L"../Dual/Resources/" + path
        };

        std::wstring clean = path;
        if (clean.rfind(L"Dual/", 0) == 0)
        {
            candidates.push_back(clean.substr(5));
        }

        Microsoft::WRL::ComPtr<ID3D11Resource> res;
        HRESULT hr = E_FAIL;
        for (const auto& candidate : candidates)
        {
            hr = DirectX::CreateDDSTextureFromFile(device, candidate.c_str(), res.ReleaseAndGetAddressOf(), m_transitionTextureSRV.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr))
            {
                break;
            }
        }

        if (SUCCEEDED(hr) && res)
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
            if (SUCCEEDED(res.As(&tex2D)))
            {
                D3D11_TEXTURE2D_DESC desc;
                tex2D->GetDesc(&desc);
                m_textureDimensions = DirectX::SimpleMath::Vector2(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
            }
        }
    }

    void SceneManager::EnsureCustomTexture(ID3D11Device* device, const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, std::wstring& loadedPath)
    {
        if (path.empty() || !device) return;
        if (srv && loadedPath == path) return;

        srv.Reset();
        loadedPath = path;

        std::vector<std::wstring> candidates = {
            path,
            L"Resources/" + path,
            L"Dual/Resources/" + path,
            L"../Dual/Resources/" + path
        };

        for (const auto& candidate : candidates)
        {
            HRESULT hr = DirectX::CreateDDSTextureFromFile(device, candidate.c_str(), nullptr, srv.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hr)) break;
        }
    }

    void SceneManager::LoadScene(const std::string& sceneName, LoadSceneMode mode)
    {
        // Initial boot without transition if no active scene has loaded yet
        if (!m_hasInitialBooted && m_activeScenes.empty())
        {
            m_hasInitialBooted = true;
            m_transitionPhase = TransitionPhase::None;
            m_transitionAlpha = 0.0f;
            m_loadingProgress = 0.0f;
            m_isPreviewOnly = false;
            m_pendingLoads.push_back({ sceneName, mode });
            return;
        }

        LoadScene(sceneName, m_defaultTransitionDesc);
    }

    void SceneManager::LoadScene(const std::string& sceneName, const SceneTransitionDesc& desc)
    {
        // Initial boot check: load immediately without fading out if no active scene exists
        if (!m_hasInitialBooted && m_activeScenes.empty())
        {
            m_hasInitialBooted = true;
            m_transitionPhase = TransitionPhase::None;
            m_transitionAlpha = 0.0f;
            m_loadingProgress = 0.0f;
            m_isPreviewOnly = false;
            m_pendingLoads.push_back({ sceneName, LoadSceneMode::Single });
            return;
        }

        m_hasInitialBooted = true;
        m_pendingTargetScene = sceneName;
        m_pendingTargetMode = LoadSceneMode::Single;
        m_currentTransitionDesc = desc;
        m_isPreviewOnly = false;
        m_transitionPhase = TransitionPhase::FadeOut;
        m_transitionTimer = 0.0f;
        m_transitionAlpha = 0.0f;
        m_animTimer = 0.0f;
        m_loadingProgress = 0.0f;
        m_needlePulseTimer = 0.0f;
        m_hasSwappedScenes = false;
        m_loadingCoverFrames = 0;

        if (!m_currentTransitionDesc.tips.empty())
        {
            m_activeTipIndex = rand() % m_currentTransitionDesc.tips.size();
        }
    }

    void SceneManager::PreviewTransition(const SceneTransitionDesc& desc)
    {
        m_pendingTargetScene.clear();
        m_currentTransitionDesc = desc;
        m_isPreviewOnly = true;
        m_transitionPhase = TransitionPhase::FadeOut;
        m_transitionTimer = 0.0f;
        m_transitionAlpha = 0.0f;
        m_animTimer = 0.0f;
        m_loadingProgress = 0.0f;
        m_needlePulseTimer = 0.0f;
        m_hasSwappedScenes = false;
        m_loadingCoverFrames = 0;

        if (!m_currentTransitionDesc.tips.empty())
        {
            m_activeTipIndex = rand() % m_currentTransitionDesc.tips.size();
        }
    }

    void SceneManager::UnloadScene(const std::string& sceneName)
    {
        m_pendingUnloads.push_back(sceneName);
    }

    bool SceneManager::LoadTransitionConfig(const std::string& path)
    {
        std::vector<std::string> candidates = {
            path,
            "Resources/Data/SceneTransition.json",
            "Dual/Resources/Data/SceneTransition.json",
            "../Dual/Resources/Data/SceneTransition.json"
        };

        std::ifstream file;
        for (const auto& candidate : candidates)
        {
            file.open(candidate);
            if (file.is_open()) break;
        }

        if (!file.is_open()) return false;

        try
        {
            nlohmann::json j;
            file >> j;

            if (j.contains("TexturePath"))
            {
                std::string tp = j["TexturePath"].get<std::string>();
                m_defaultTransitionDesc.texturePath = std::wstring(tp.begin(), tp.end());
            }
            if (j.contains("FadeOutDuration")) m_defaultTransitionDesc.fadeOutDuration = j["FadeOutDuration"].get<float>();
            if (j.contains("MinLoadingHoldDuration"))
            {
                m_defaultTransitionDesc.minLoadingHoldDuration = j["MinLoadingHoldDuration"].get<float>();
                m_defaultTransitionDesc.holdDuration = m_defaultTransitionDesc.minLoadingHoldDuration;
            }
            else if (j.contains("HoldDuration"))
            {
                m_defaultTransitionDesc.holdDuration = j["HoldDuration"].get<float>();
                m_defaultTransitionDesc.minLoadingHoldDuration = m_defaultTransitionDesc.holdDuration;
            }
            if (j.contains("FadeInDuration")) m_defaultTransitionDesc.fadeInDuration = j["FadeInDuration"].get<float>();
            if (j.contains("TintColor") && j["TintColor"].is_array() && j["TintColor"].size() >= 4)
            {
                m_defaultTransitionDesc.tintColor = DirectX::SimpleMath::Vector4(
                    j["TintColor"][0].get<float>(),
                    j["TintColor"][1].get<float>(),
                    j["TintColor"][2].get<float>(),
                    j["TintColor"][3].get<float>()
                );
            }
            if (j.contains("Columns")) m_defaultTransitionDesc.columns = j["Columns"].get<int>();
            if (j.contains("Rows")) m_defaultTransitionDesc.rows = j["Rows"].get<int>();
            if (j.contains("TotalFrames")) m_defaultTransitionDesc.totalFrames = j["TotalFrames"].get<int>();
            if (j.contains("Fps")) m_defaultTransitionDesc.fps = j["Fps"].get<float>();
            if (j.contains("LoopAnimation")) m_defaultTransitionDesc.loopAnimation = j["LoopAnimation"].get<bool>();

            // Loading Bar
            if (j.contains("ShowLoadingBar")) m_defaultTransitionDesc.showLoadingBar = j["ShowLoadingBar"].get<bool>();
            if (j.contains("BarOrientation")) m_defaultTransitionDesc.barOrientation = static_cast<LoadingBarOrientation>(j["BarOrientation"].get<int>());
            if (j.contains("BarStyle")) m_defaultTransitionDesc.barStyle = static_cast<LoadingBarStyle>(j["BarStyle"].get<int>());
            if (j.contains("BarNormalizedPos") && j["BarNormalizedPos"].is_array() && j["BarNormalizedPos"].size() >= 2)
            {
                m_defaultTransitionDesc.barNormalizedPos = DirectX::SimpleMath::Vector2(
                    j["BarNormalizedPos"][0].get<float>(),
                    j["BarNormalizedPos"][1].get<float>()
                );
            }
            if (j.contains("BarLength")) m_defaultTransitionDesc.barLength = j["BarLength"].get<float>();
            if (j.contains("BarThickness")) m_defaultTransitionDesc.barThickness = j["BarThickness"].get<float>();
            if (j.contains("ShowEndCaps")) m_defaultTransitionDesc.showEndCaps = j["ShowEndCaps"].get<bool>();
            if (j.contains("EndCapLength")) m_defaultTransitionDesc.endCapLength = j["EndCapLength"].get<float>();
            if (j.contains("EndCapThickness")) m_defaultTransitionDesc.endCapThickness = j["EndCapThickness"].get<float>();
            if (j.contains("ShowNeedle")) m_defaultTransitionDesc.showNeedle = j["ShowNeedle"].get<bool>();
            if (j.contains("NeedleLength")) m_defaultTransitionDesc.needleLength = j["NeedleLength"].get<float>();
            if (j.contains("NeedleThickness")) m_defaultTransitionDesc.needleThickness = j["NeedleThickness"].get<float>();
            if (j.contains("NeedlePulseSpeed")) m_defaultTransitionDesc.needlePulseSpeed = j["NeedlePulseSpeed"].get<float>();
            if (j.contains("SegmentCount")) m_defaultTransitionDesc.segmentCount = j["SegmentCount"].get<int>();
            if (j.contains("ShowPercentageText")) m_defaultTransitionDesc.showPercentageText = j["ShowPercentageText"].get<bool>();

            if (j.contains("BarTrackColor") && j["BarTrackColor"].is_array() && j["BarTrackColor"].size() >= 4)
            {
                m_defaultTransitionDesc.barTrackColor = DirectX::SimpleMath::Vector4(
                    j["BarTrackColor"][0].get<float>(),
                    j["BarTrackColor"][1].get<float>(),
                    j["BarTrackColor"][2].get<float>(),
                    j["BarTrackColor"][3].get<float>()
                );
            }
            if (j.contains("BarFillColor") && j["BarFillColor"].is_array() && j["BarFillColor"].size() >= 4)
            {
                m_defaultTransitionDesc.barFillColor = DirectX::SimpleMath::Vector4(
                    j["BarFillColor"][0].get<float>(),
                    j["BarFillColor"][1].get<float>(),
                    j["BarFillColor"][2].get<float>(),
                    j["BarFillColor"][3].get<float>()
                );
            }
            if (j.contains("BarNeedleColor") && j["BarNeedleColor"].is_array() && j["BarNeedleColor"].size() >= 4)
            {
                m_defaultTransitionDesc.barNeedleColor = DirectX::SimpleMath::Vector4(
                    j["BarNeedleColor"][0].get<float>(),
                    j["BarNeedleColor"][1].get<float>(),
                    j["BarNeedleColor"][2].get<float>(),
                    j["BarNeedleColor"][3].get<float>()
                );
            }

            if (j.contains("BarTrackTexturePath"))
            {
                std::string s = j["BarTrackTexturePath"].get<std::string>();
                m_defaultTransitionDesc.barTrackTexturePath = std::wstring(s.begin(), s.end());
            }
            if (j.contains("BarFillTexturePath"))
            {
                std::string s = j["BarFillTexturePath"].get<std::string>();
                m_defaultTransitionDesc.barFillTexturePath = std::wstring(s.begin(), s.end());
            }
            if (j.contains("BarNeedleTexturePath"))
            {
                std::string s = j["BarNeedleTexturePath"].get<std::string>();
                m_defaultTransitionDesc.barNeedleTexturePath = std::wstring(s.begin(), s.end());
            }

            // Lore / Tips
            if (j.contains("ShowTips")) m_defaultTransitionDesc.showTips = j["ShowTips"].get<bool>();
            if (j.contains("TipNormalizedPos") && j["TipNormalizedPos"].is_array() && j["TipNormalizedPos"].size() >= 2)
            {
                m_defaultTransitionDesc.tipNormalizedPos = DirectX::SimpleMath::Vector2(
                    j["TipNormalizedPos"][0].get<float>(),
                    j["TipNormalizedPos"][1].get<float>()
                );
            }
            if (j.contains("Tips") && j["Tips"].is_array())
            {
                m_defaultTransitionDesc.tips.clear();
                for (const auto& tipJson : j["Tips"])
                {
                    LoadingTip tip;
                    if (tipJson.contains("title")) tip.title = tipJson["title"].get<std::string>();
                    if (tipJson.contains("description")) tip.description = tipJson["description"].get<std::string>();
                    m_defaultTransitionDesc.tips.push_back(tip);
                }
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool SceneManager::SaveTransitionConfig(const std::string& path) const
    {
        std::string writePath = path;
        if (writePath == "Resources/Data/SceneTransition.json")
        {
            writePath = GetTransitionConfigWritePath();
        }

        try
        {
            nlohmann::json j;
            std::string tp(m_defaultTransitionDesc.texturePath.begin(), m_defaultTransitionDesc.texturePath.end());
            j["TexturePath"] = tp;
            j["FadeOutDuration"] = m_defaultTransitionDesc.fadeOutDuration;
            j["MinLoadingHoldDuration"] = m_defaultTransitionDesc.minLoadingHoldDuration;
            j["HoldDuration"] = m_defaultTransitionDesc.minLoadingHoldDuration;
            j["FadeInDuration"] = m_defaultTransitionDesc.fadeInDuration;
            j["TintColor"] = {
                m_defaultTransitionDesc.tintColor.x,
                m_defaultTransitionDesc.tintColor.y,
                m_defaultTransitionDesc.tintColor.z,
                m_defaultTransitionDesc.tintColor.w
            };
            j["Columns"] = m_defaultTransitionDesc.columns;
            j["Rows"] = m_defaultTransitionDesc.rows;
            j["TotalFrames"] = m_defaultTransitionDesc.totalFrames;
            j["Fps"] = m_defaultTransitionDesc.fps;
            j["LoopAnimation"] = m_defaultTransitionDesc.loopAnimation;

            j["ShowLoadingBar"] = m_defaultTransitionDesc.showLoadingBar;
            j["BarOrientation"] = static_cast<int>(m_defaultTransitionDesc.barOrientation);
            j["BarStyle"] = static_cast<int>(m_defaultTransitionDesc.barStyle);
            j["BarNormalizedPos"] = { m_defaultTransitionDesc.barNormalizedPos.x, m_defaultTransitionDesc.barNormalizedPos.y };
            j["BarLength"] = m_defaultTransitionDesc.barLength;
            j["BarThickness"] = m_defaultTransitionDesc.barThickness;
            j["ShowEndCaps"] = m_defaultTransitionDesc.showEndCaps;
            j["EndCapLength"] = m_defaultTransitionDesc.endCapLength;
            j["EndCapThickness"] = m_defaultTransitionDesc.endCapThickness;
            j["ShowNeedle"] = m_defaultTransitionDesc.showNeedle;
            j["NeedleLength"] = m_defaultTransitionDesc.needleLength;
            j["NeedleThickness"] = m_defaultTransitionDesc.needleThickness;
            j["NeedlePulseSpeed"] = m_defaultTransitionDesc.needlePulseSpeed;
            j["SegmentCount"] = m_defaultTransitionDesc.segmentCount;
            j["ShowPercentageText"] = m_defaultTransitionDesc.showPercentageText;

            j["BarTrackColor"] = {
                m_defaultTransitionDesc.barTrackColor.x,
                m_defaultTransitionDesc.barTrackColor.y,
                m_defaultTransitionDesc.barTrackColor.z,
                m_defaultTransitionDesc.barTrackColor.w
            };
            j["BarFillColor"] = {
                m_defaultTransitionDesc.barFillColor.x,
                m_defaultTransitionDesc.barFillColor.y,
                m_defaultTransitionDesc.barFillColor.z,
                m_defaultTransitionDesc.barFillColor.w
            };
            j["BarNeedleColor"] = {
                m_defaultTransitionDesc.barNeedleColor.x,
                m_defaultTransitionDesc.barNeedleColor.y,
                m_defaultTransitionDesc.barNeedleColor.z,
                m_defaultTransitionDesc.barNeedleColor.w
            };

            std::string sTrack(m_defaultTransitionDesc.barTrackTexturePath.begin(), m_defaultTransitionDesc.barTrackTexturePath.end());
            std::string sFill(m_defaultTransitionDesc.barFillTexturePath.begin(), m_defaultTransitionDesc.barFillTexturePath.end());
            std::string sNeedle(m_defaultTransitionDesc.barNeedleTexturePath.begin(), m_defaultTransitionDesc.barNeedleTexturePath.end());
            j["BarTrackTexturePath"] = sTrack;
            j["BarFillTexturePath"] = sFill;
            j["BarNeedleTexturePath"] = sNeedle;

            j["ShowTips"] = m_defaultTransitionDesc.showTips;
            j["TipNormalizedPos"] = { m_defaultTransitionDesc.tipNormalizedPos.x, m_defaultTransitionDesc.tipNormalizedPos.y };
            nlohmann::json tipsArr = nlohmann::json::array();
            for (const auto& tip : m_defaultTransitionDesc.tips)
            {
                nlohmann::json t;
                t["title"] = tip.title;
                t["description"] = tip.description;
                tipsArr.push_back(t);
            }
            j["Tips"] = tipsArr;

            std::ofstream out(writePath);
            if (out.is_open())
            {
                out << j.dump(4);
                return true;
            }
        }
        catch (...)
        {
        }
        return false;
    }

    std::vector<std::string> SceneManager::GetRegisteredSceneNames() const
    {
        std::vector<std::string> names;
        for (const auto& pair : m_sceneRegistry)
        {
            names.push_back(pair.first);
        }
        return names;
    }

    std::string SceneManager::GetCurrentSceneName() const
    {
        if (!m_activeScenes.empty())
        {
            return m_activeScenes[0].name;
        }
        return "";
    }

    void SceneManager::Update(GameContext& context)
    {
        float deltaTime = static_cast<float>(context.timer.GetElapsedSeconds());
        deltaTime = std::fmin(deltaTime, 0.033f); // Clamp loading spike so hold/fade are never skipped

        m_animTimer += deltaTime;
        m_needlePulseTimer += deltaTime;

        // While transitioning and alpha > 0.05f, suppress in-game UI so HUD labels never bleed through
        if (IsTransitioning() && m_transitionAlpha > 0.05f)
        {
            context.showUI = false;
        }

        // Process pending scene unloads
        if (!m_pendingUnloads.empty())
        {
            for (const std::string& nameToUnload : m_pendingUnloads)
            {
                for (auto it = m_activeScenes.begin(); it != m_activeScenes.end(); )
                {
                    if (it->name == nameToUnload)
                    {
                        it->instance->OnExit(context);
                        it = m_activeScenes.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            m_pendingUnloads.clear();

            StaticModelComponent::PruneModelCache();
            HEIN::PruneSharedTextureCache();
            if (auto d3dContext = context.deviceResources.GetD3DDeviceContext())
            {
                d3dContext->ClearState();
                d3dContext->Flush();
            }
            _heapmin();
            SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
        }

        // Process initial pending scene loads (e.g. boot without transition)
        if (!m_pendingLoads.empty())
        {
            for (const PendingLoad& load : m_pendingLoads)
            {
                if (load.mode == LoadSceneMode::Single)
                {
                    for (auto& active : m_activeScenes)
                    {
                        active.instance->OnExit(context);
                    }
                    m_activeScenes.clear();

                    StaticModelComponent::PruneModelCache();
                    HEIN::PruneSharedTextureCache();
                    if (auto d3dContext = context.deviceResources.GetD3DDeviceContext())
                    {
                        d3dContext->ClearState();
                        d3dContext->Flush();
                    }
                }

                auto it = m_sceneRegistry.find(load.sceneName);
                if (it != m_sceneRegistry.end())
                {
                    ActiveScene newScene;
                    newScene.name = load.sceneName;
                    newScene.instance = it->second();

                    newScene.instance->OnEnter(context);
                    m_activeScenes.push_back(std::move(newScene));

                    StaticModelComponent::PruneModelCache();
                    HEIN::PruneSharedTextureCache();
                    if (auto d3dContext = context.deviceResources.GetD3DDeviceContext())
                    {
                        d3dContext->ClearState();
                        d3dContext->Flush();
                    }
                    _heapmin();
                    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
                }
            }
            m_pendingLoads.clear();
        }

        // Advance Transition State Machine
        switch (m_transitionPhase)
        {
        case TransitionPhase::FadeOut:
        {
            m_transitionTimer += deltaTime;
            float duration = (m_currentTransitionDesc.fadeOutDuration > 0.0f) ? m_currentTransitionDesc.fadeOutDuration : 0.001f;
            float t = std::min(1.0f, m_transitionTimer / duration);
            m_transitionAlpha = t * t * (3.0f - 2.0f * t); // Smoothstep 0 -> 1
            m_loadingProgress = 0.0f;

            if (t >= 1.0f)
            {
                m_transitionAlpha = 1.0f;
                m_transitionPhase = TransitionPhase::LoadingSwitch;
                m_transitionTimer = 0.0f;
                m_loadingCoverFrames = 0;
            }
            break;
        }

        case TransitionPhase::LoadingSwitch:
        {
            m_loadingCoverFrames++;
            m_transitionAlpha = 1.0f;
            m_transitionTimer += deltaTime;

            float hold = (m_currentTransitionDesc.minLoadingHoldDuration > 0.05f) ? m_currentTransitionDesc.minLoadingHoldDuration : 0.05f;
            m_loadingProgress = std::clamp(m_transitionTimer / hold, 0.0f, 1.0f);

            // Once the screen is visibly 100% covered (>= 2 frames), swap the underlying scene immediately
            if (m_loadingCoverFrames >= 2 && !m_hasSwappedScenes)
            {
                m_hasSwappedScenes = true;
                if (!m_isPreviewOnly && !m_pendingTargetScene.empty())
                {
                    for (auto& active : m_activeScenes)
                    {
                        active.instance->OnExit(context);
                    }
                    m_activeScenes.clear();

                    StaticModelComponent::PruneModelCache();
                    HEIN::PruneSharedTextureCache();
                    if (auto d3dContext = context.deviceResources.GetD3DDeviceContext())
                    {
                        d3dContext->ClearState();
                        d3dContext->Flush();
                    }
                    _heapmin();
                    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

                    auto it = m_sceneRegistry.find(m_pendingTargetScene);
                    if (it != m_sceneRegistry.end())
                    {
                        ActiveScene newScene;
                        newScene.name = m_pendingTargetScene;
                        newScene.instance = it->second();

                        newScene.instance->OnEnter(context);
                        m_activeScenes.push_back(std::move(newScene));

                        StaticModelComponent::PruneModelCache();
                        HEIN::PruneSharedTextureCache();
                        if (auto d3dContext = context.deviceResources.GetD3DDeviceContext())
                        {
                            d3dContext->ClearState();
                            d3dContext->Flush();
                        }
                        _heapmin();
                        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
                    }

                    m_pendingTargetScene.clear();
                }
            }

            // After hold duration has elapsed and progress is full, transition to FadeIn
            if (m_loadingCoverFrames >= 2 && m_transitionTimer >= hold)
            {
                m_loadingProgress = 1.0f;
                m_transitionPhase = TransitionPhase::FadeIn;
                m_transitionTimer = 0.0f;
            }
            break;
        }

        case TransitionPhase::FadeIn:
        {
            m_transitionTimer += deltaTime;
            float duration = (m_currentTransitionDesc.fadeInDuration > 0.0f) ? m_currentTransitionDesc.fadeInDuration : 0.001f;
            float t = std::min(1.0f, m_transitionTimer / duration);
            m_transitionAlpha = 1.0f - (t * t * (3.0f - 2.0f * t)); // Smoothstep 1 -> 0
            m_loadingProgress = 1.0f;

            if (t >= 1.0f)
            {
                m_transitionAlpha = 0.0f;
                m_loadingProgress = 0.0f;
                m_transitionPhase = TransitionPhase::None;
                m_transitionTimer = 0.0f;
                m_isPreviewOnly = false;

                // VRAM Cleanup: free transition textures so they consume 0 MB during gameplay
                m_transitionTextureSRV.Reset();
                m_barTrackSRV.Reset();
                m_barFillSRV.Reset();
                m_barNeedleSRV.Reset();
                m_loadedTexturePath.clear();
                m_loadedBarTrackPath.clear();
                m_loadedBarFillPath.clear();
                m_loadedBarNeedlePath.clear();
                m_textureDimensions = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
            }
            break;
        }

        case TransitionPhase::None:
        default:
            break;
        }

        // Update active scenes (skip only during exact LoadingSwitch frame)
        if (m_transitionPhase != TransitionPhase::LoadingSwitch)
        {
            for (auto& active : m_activeScenes)
            {
                active.instance->Update(context);
            }
        }
    }

    void SceneManager::RenderLoadingBar(GameContext& context, const D3D11_VIEWPORT& vp, float alpha)
    {
        if (!m_currentTransitionDesc.showLoadingBar || !m_whiteTextureSRV || !m_spriteBatch) return;

        float anchorX = m_currentTransitionDesc.barNormalizedPos.x * vp.Width;
        float anchorY = m_currentTransitionDesc.barNormalizedPos.y * vp.Height;
        float length = m_currentTransitionDesc.barLength;
        float thickness = m_currentTransitionDesc.barThickness;
        float progress = std::clamp(m_loadingProgress, 0.0f, 1.0f);

        DirectX::SimpleMath::Vector4 trackCol = m_currentTransitionDesc.barTrackColor;
        DirectX::SimpleMath::Vector4 fillCol = m_currentTransitionDesc.barFillColor;
        DirectX::SimpleMath::Vector4 needleCol = m_currentTransitionDesc.barNeedleColor;

        trackCol.w *= alpha;
        fillCol.w *= alpha;
        needleCol.w *= alpha;

        // Subtle needle pulse
        float pulse = 0.88f + 0.12f * std::sinf(m_needlePulseTimer * m_currentTransitionDesc.needlePulseSpeed);
        needleCol.x = std::min(1.0f, needleCol.x * pulse);
        needleCol.y = std::min(1.0f, needleCol.y * pulse);
        needleCol.z = std::min(1.0f, needleCol.z * pulse);

        auto orientation = m_currentTransitionDesc.barOrientation;
        auto style = m_currentTransitionDesc.barStyle;

        // Helper to draw a colored solid rectangle
        auto DrawSolidRect = [&](float x, float y, float w, float h, const DirectX::SimpleMath::Vector4& col)
        {
            RECT r;
            r.left = static_cast<LONG>(x);
            r.top = static_cast<LONG>(y);
            r.right = static_cast<LONG>(x + w);
            r.bottom = static_cast<LONG>(y + h);
            m_spriteBatch->Draw(m_whiteTextureSRV.Get(), r, nullptr, col);
        };

        // Determine orientation primary axes
        bool isHorizontal = (orientation == LoadingBarOrientation::Horizontal_LeftToRight ||
                             orientation == LoadingBarOrientation::Horizontal_RightToLeft ||
                             orientation == LoadingBarOrientation::Center_Horizontal);

        float trackX = anchorX;
        float trackY = anchorY;

        if (orientation == LoadingBarOrientation::Horizontal_RightToLeft)
        {
            trackX = anchorX - length;
            trackY = anchorY - thickness * 0.5f;
        }
        else if (orientation == LoadingBarOrientation::Horizontal_LeftToRight)
        {
            trackX = anchorX;
            trackY = anchorY - thickness * 0.5f;
        }
        else if (orientation == LoadingBarOrientation::Center_Horizontal)
        {
            trackX = anchorX - length * 0.5f;
            trackY = anchorY - thickness * 0.5f;
        }
        else if (orientation == LoadingBarOrientation::Vertical_BottomToTop)
        {
            trackX = anchorX - thickness * 0.5f;
            trackY = anchorY - length;
        }
        else if (orientation == LoadingBarOrientation::Vertical_TopToBottom)
        {
            trackX = anchorX - thickness * 0.5f;
            trackY = anchorY;
        }
        else if (orientation == LoadingBarOrientation::Center_Vertical)
        {
            trackX = anchorX - thickness * 0.5f;
            trackY = anchorY - length * 0.5f;
        }

        // Draw depending on style
        if (style == LoadingBarStyle::Segmented)
        {
            int segments = std::max(2, m_currentTransitionDesc.segmentCount);
            float gap = 3.0f;
            float totalGaps = gap * (segments - 1);
            float segLen = std::max(1.0f, (length - totalGaps) / segments);

            for (int i = 0; i < segments; ++i)
            {
                float segProgressThreshold = (i + 0.5f) / static_cast<float>(segments);
                auto segCol = (progress >= segProgressThreshold) ? fillCol : trackCol;

                if (isHorizontal)
                {
                    float sx = trackX + i * (segLen + gap);
                    DrawSolidRect(sx, trackY, segLen, thickness, segCol);
                }
                else
                {
                    float sy = (orientation == LoadingBarOrientation::Vertical_BottomToTop)
                        ? (trackY + (segments - 1 - i) * (segLen + gap))
                        : (trackY + i * (segLen + gap));
                    DrawSolidRect(trackX, sy, thickness, segLen, segCol);
                }
            }
        }
        else // EldenRingNeedle, SolidFill, CustomTextured
        {
            // 1. Draw Track
            if (style == LoadingBarStyle::CustomTextured && m_barTrackSRV)
            {
                RECT r = { static_cast<LONG>(trackX), static_cast<LONG>(trackY),
                           static_cast<LONG>(trackX + (isHorizontal ? length : thickness)),
                           static_cast<LONG>(trackY + (isHorizontal ? thickness : length)) };
                m_spriteBatch->Draw(m_barTrackSRV.Get(), r, nullptr, trackCol);
            }
            else
            {
                DrawSolidRect(trackX, trackY, isHorizontal ? length : thickness, isHorizontal ? thickness : length, trackCol);
            }

            // 2. Draw Fill
            float fillLength = length * progress;
            if (fillLength > 0.5f)
            {
                float fillX = trackX;
                float fillY = trackY;
                float fillW = isHorizontal ? fillLength : thickness;
                float fillH = isHorizontal ? thickness : fillLength;

                if (orientation == LoadingBarOrientation::Horizontal_RightToLeft)
                {
                    fillX = (trackX + length) - fillLength;
                }
                else if (orientation == LoadingBarOrientation::Vertical_BottomToTop)
                {
                    fillY = (trackY + length) - fillLength;
                }
                else if (orientation == LoadingBarOrientation::Center_Horizontal)
                {
                    fillX = anchorX - fillLength * 0.5f;
                    fillW = fillLength;
                }
                else if (orientation == LoadingBarOrientation::Center_Vertical)
                {
                    fillY = anchorY - fillLength * 0.5f;
                    fillH = fillLength;
                }

                if (style == LoadingBarStyle::CustomTextured && m_barFillSRV)
                {
                    RECT r = { static_cast<LONG>(fillX), static_cast<LONG>(fillY),
                               static_cast<LONG>(fillX + fillW), static_cast<LONG>(fillY + fillH) };
                    m_spriteBatch->Draw(m_barFillSRV.Get(), r, nullptr, fillCol);
                }
                else
                {
                    DrawSolidRect(fillX, fillY, fillW, fillH, fillCol);
                }
            }

            // 3. Draw End-Caps (Elden Ring perpendicular ticks at start and end)
            if (m_currentTransitionDesc.showEndCaps)
            {
                float capLen = m_currentTransitionDesc.endCapLength;
                float capThick = m_currentTransitionDesc.endCapThickness;

                if (isHorizontal)
                {
                    // Start cap
                    DrawSolidRect(trackX - capThick * 0.5f, anchorY - capLen * 0.5f, capThick, capLen, trackCol);
                    // End cap
                    DrawSolidRect((trackX + length) - capThick * 0.5f, anchorY - capLen * 0.5f, capThick, capLen, trackCol);
                }
                else
                {
                    // Start cap
                    DrawSolidRect(anchorX - capLen * 0.5f, trackY - capThick * 0.5f, capLen, capThick, trackCol);
                    // End cap
                    DrawSolidRect(anchorX - capLen * 0.5f, (trackY + length) - capThick * 0.5f, capLen, capThick, trackCol);
                }
            }

            // 4. Draw Sliding Needle (Elden Ring sliding tick at current progress)
            if (m_currentTransitionDesc.showNeedle)
            {
                float needleLen = m_currentTransitionDesc.needleLength;
                float needleThick = m_currentTransitionDesc.needleThickness;

                if (isHorizontal)
                {
                    float needleX = trackX + length * progress;
                    if (orientation == LoadingBarOrientation::Horizontal_RightToLeft)
                    {
                        needleX = (trackX + length) - length * progress;
                    }

                    if (style == LoadingBarStyle::CustomTextured && m_barNeedleSRV)
                    {
                        RECT r = { static_cast<LONG>(needleX - needleThick * 0.5f), static_cast<LONG>(anchorY - needleLen * 0.5f),
                                   static_cast<LONG>(needleX + needleThick * 0.5f), static_cast<LONG>(anchorY + needleLen * 0.5f) };
                        m_spriteBatch->Draw(m_barNeedleSRV.Get(), r, nullptr, needleCol);
                    }
                    else
                    {
                        DrawSolidRect(needleX - needleThick * 0.5f, anchorY - needleLen * 0.5f, needleThick, needleLen, needleCol);
                    }
                }
                else
                {
                    float needleY = trackY + length * progress;
                    if (orientation == LoadingBarOrientation::Vertical_BottomToTop)
                    {
                        needleY = (trackY + length) - length * progress;
                    }

                    if (style == LoadingBarStyle::CustomTextured && m_barNeedleSRV)
                    {
                        RECT r = { static_cast<LONG>(anchorX - needleLen * 0.5f), static_cast<LONG>(needleY - needleThick * 0.5f),
                                   static_cast<LONG>(anchorX + needleLen * 0.5f), static_cast<LONG>(needleY + needleThick * 0.5f) };
                        m_spriteBatch->Draw(m_barNeedleSRV.Get(), r, nullptr, needleCol);
                    }
                    else
                    {
                        DrawSolidRect(anchorX - needleLen * 0.5f, needleY - needleThick * 0.5f, needleLen, needleThick, needleCol);
                    }
                }
            }
        }

        // 5. Optional Percentage Readout
        if (m_currentTransitionDesc.showPercentageText && m_spriteFont)
        {
            char pBuf[32];
            sprintf_s(pBuf, "%d%%", static_cast<int>(progress * 100.0f));
            DirectX::SimpleMath::Vector4 textCol = m_currentTransitionDesc.barNeedleColor;
            textCol.w *= alpha;

            float textX = trackX + length + 18.0f;
            float textY = anchorY - 10.0f;
            if (!isHorizontal)
            {
                textX = anchorX - 16.0f;
                textY = trackY + length + 14.0f;
            }

            m_spriteFont->DrawString(m_spriteBatch.get(), pBuf, DirectX::SimpleMath::Vector2(textX, textY), textCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, 0.75f);
        }
    }

    void SceneManager::RenderTips(GameContext& /*context*/, const D3D11_VIEWPORT& vp, float alpha)
    {
        if (!m_currentTransitionDesc.showTips || m_currentTransitionDesc.tips.empty() || !m_spriteFont || !m_spriteBatch) return;

        size_t idx = m_activeTipIndex % m_currentTransitionDesc.tips.size();
        const auto& tip = m_currentTransitionDesc.tips[idx];

        float tipX = m_currentTransitionDesc.tipNormalizedPos.x * vp.Width;
        float tipY = m_currentTransitionDesc.tipNormalizedPos.y * vp.Height;

        DirectX::SimpleMath::Vector4 titleCol = DirectX::SimpleMath::Vector4(0.88f, 0.82f, 0.62f, alpha);
        DirectX::SimpleMath::Vector4 descCol  = DirectX::SimpleMath::Vector4(0.72f, 0.70f, 0.62f, alpha * 0.90f);

        // Title
        m_spriteFont->DrawString(m_spriteBatch.get(), tip.title.c_str(), DirectX::SimpleMath::Vector2(tipX, tipY), titleCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, 0.85f);

        // Subtle divider underline
        if (m_whiteTextureSRV)
        {
            RECT lineRect;
            lineRect.left = static_cast<LONG>(tipX);
            lineRect.top = static_cast<LONG>(tipY + 22.0f);
            lineRect.right = static_cast<LONG>(tipX + 220.0f);
            lineRect.bottom = static_cast<LONG>(tipY + 23.0f);
            DirectX::SimpleMath::Vector4 lineCol = titleCol;
            lineCol.w *= 0.5f;
            m_spriteBatch->Draw(m_whiteTextureSRV.Get(), lineRect, nullptr, lineCol);
        }

        // Description
        m_spriteFont->DrawString(m_spriteBatch.get(), tip.description.c_str(), DirectX::SimpleMath::Vector2(tipX, tipY + 30.0f), descCol, 0.0f, DirectX::SimpleMath::Vector2::Zero, 0.70f);
    }

    void SceneManager::Render(GameContext& context)
    {
        // 1. Skip rendering active scenes when screen is 100% covered by the loading screen
        bool skipSceneRender = (m_transitionPhase == TransitionPhase::LoadingSwitch || (m_transitionAlpha >= 0.99f && !m_isPreviewOnly));
        if (!skipSceneRender)
        {
            for (auto& active : m_activeScenes)
            {
                active.instance->Render(context);
            }
        }

        // 2. In full-screen game mode, render transition overlay across main screen viewport
        if (!context.isEditorMode)
        {
            D3D11_VIEWPORT vp = context.deviceResources.GetScreenViewport();
            RenderTransitionOverlay(context, vp);
        }
    }

    void SceneManager::RenderTransitionOverlay(GameContext& context, const D3D11_VIEWPORT& targetViewport)
    {
        if (m_transitionPhase == TransitionPhase::None || m_transitionAlpha <= 0.001f)
        {
            return;
        }

        if (targetViewport.Width <= 0.0f || targetViewport.Height <= 0.0f)
        {
            return;
        }

        ID3D11Device* device = context.deviceResources.GetD3DDevice();
        ID3D11DeviceContext* d3dContext = context.deviceResources.GetD3DDeviceContext();
        if (!device || !d3dContext) return;

        EnsureWhiteTexture(device);
        EnsureSpriteBatch(d3dContext);
        EnsureFont(device);

        if (!m_currentTransitionDesc.texturePath.empty())
        {
            EnsureTransitionTexture(device, m_currentTransitionDesc.texturePath);
        }
        if (!m_currentTransitionDesc.barTrackTexturePath.empty())
        {
            EnsureCustomTexture(device, m_currentTransitionDesc.barTrackTexturePath, m_barTrackSRV, m_loadedBarTrackPath);
        }
        if (!m_currentTransitionDesc.barFillTexturePath.empty())
        {
            EnsureCustomTexture(device, m_currentTransitionDesc.barFillTexturePath, m_barFillSRV, m_loadedBarFillPath);
        }
        if (!m_currentTransitionDesc.barNeedleTexturePath.empty())
        {
            EnsureCustomTexture(device, m_currentTransitionDesc.barNeedleTexturePath, m_barNeedleSRV, m_loadedBarNeedlePath);
        }

        d3dContext->RSSetViewports(1, &targetViewport);

        RECT destRect = { 0, 0, static_cast<LONG>(targetViewport.Width), static_cast<LONG>(targetViewport.Height) };

        ID3D11ShaderResourceView* drawSRV = m_whiteTextureSRV.Get();
        RECT sourceRect = {};
        RECT* pSourceRect = nullptr;

        DirectX::SimpleMath::Vector4 drawColor = m_currentTransitionDesc.tintColor;
        drawColor.w *= m_transitionAlpha;

        if (m_transitionTextureSRV && m_textureDimensions.x > 0.0f && m_textureDimensions.y > 0.0f)
        {
            drawSRV = m_transitionTextureSRV.Get();

            int cols = std::max(1, m_currentTransitionDesc.columns);
            int rows = std::max(1, m_currentTransitionDesc.rows);
            int totalFrames = std::max(1, m_currentTransitionDesc.totalFrames);
            float fps = (m_currentTransitionDesc.fps > 0.0f) ? m_currentTransitionDesc.fps : 24.0f;

            int frame = static_cast<int>(m_animTimer * fps);
            if (m_currentTransitionDesc.loopAnimation)
            {
                frame = frame % totalFrames;
            }
            else
            {
                frame = std::min(frame, totalFrames - 1);
            }

            int cellX = frame % cols;
            int cellY = frame / cols;
            if (cellY >= rows) cellY = rows - 1;

            float cellW = m_textureDimensions.x / static_cast<float>(cols);
            float cellH = m_textureDimensions.y / static_cast<float>(rows);

            sourceRect.left = static_cast<LONG>(cellX * cellW);
            sourceRect.top = static_cast<LONG>(cellY * cellH);
            sourceRect.right = static_cast<LONG>((cellX + 1) * cellW);
            sourceRect.bottom = static_cast<LONG>((cellY + 1) * cellH);
            pSourceRect = &sourceRect;
        }

        if (m_spriteBatch)
        {
            m_spriteBatch->Begin(DirectX::SpriteSortMode_Deferred, context.commonStates.NonPremultiplied());

            // 2A. Draw background overlay
            if (drawSRV)
            {
                m_spriteBatch->Draw(drawSRV, destRect, pSourceRect, drawColor);
            }

            // 2B. Draw Elden Ring Loading Bar
            RenderLoadingBar(context, targetViewport, m_transitionAlpha);

            // 2C. Draw Lore / Gameplay Tips
            RenderTips(context, targetViewport, m_transitionAlpha);

            m_spriteBatch->End();

            // Restore D3D11 pipeline states after SpriteBatch
            d3dContext->OMSetBlendState(context.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
            d3dContext->OMSetDepthStencilState(context.commonStates.DepthDefault(), 0);
            d3dContext->RSSetState(context.commonStates.CullCounterClockwise());
        }
    }
}