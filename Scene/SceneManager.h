#pragma once
#include "IScene.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

namespace HEIN
{
    // Mirrors Unity's LoadSceneMode
    enum class LoadSceneMode
    {
        Single,
        Additive
    };

    enum class LoadingBarOrientation : int
    {
        Horizontal_LeftToRight = 0,
        Horizontal_RightToLeft = 1,
        Vertical_BottomToTop   = 2,
        Vertical_TopToBottom   = 3,
        Center_Horizontal      = 4,
        Center_Vertical        = 5
    };

    enum class LoadingBarStyle : int
    {
        EldenRingNeedle = 0, // Thin line, perpendicular end-caps, and sliding needle tick
        SolidFill       = 1, // Standard filled progress bar with border
        Segmented       = 2, // Discrete blocks/segments that light up
        CustomTextured  = 3  // Uses optional .dds textures with UV source-rect clipping
    };

    struct LoadingTip
    {
        std::string title = "Vessel Possession";
        std::string description = "When your mortal body falls, press [Q] near an unoccupied vessel to claim its strength.";
    };

    /**
     * @struct SceneTransitionDesc
     * @brief Complete configuration for scene fade, Elden Ring loading bar, and lore tips.
     */
    struct SceneTransitionDesc
    {
        // Background & Transition Timing
        std::wstring texturePath = L""; // Optional .dds background image or flipbook sprite-sheet
        float fadeOutDuration = 0.45f;
        float minLoadingHoldDuration = 1.2f;
        float holdDuration = 1.2f; // Alias for backward compatibility
        float fadeInDuration = 0.50f;
        DirectX::SimpleMath::Vector4 tintColor = DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        // Flipbook / Video Sprite-Sheet support
        int columns = 1;
        int rows = 1;
        int totalFrames = 1;
        float fps = 24.0f;
        bool loopAnimation = true;

        // Versatile Loading Bar Settings
        bool showLoadingBar = true;
        LoadingBarOrientation barOrientation = LoadingBarOrientation::Horizontal_LeftToRight;
        LoadingBarStyle barStyle = LoadingBarStyle::EldenRingNeedle;
        DirectX::SimpleMath::Vector2 barNormalizedPos = DirectX::SimpleMath::Vector2(0.72f, 0.92f); // Normalized screen anchor (0..1)
        float barLength = 340.0f;        // Length in pixels along the primary axis
        float barThickness = 2.0f;       // Thickness in pixels along the perpendicular axis
        bool showEndCaps = true;
        float endCapLength = 12.0f;      // Length of perpendicular end-cap ticks
        float endCapThickness = 2.0f;
        bool showNeedle = true;
        float needleLength = 18.0f;      // Length of perpendicular sliding needle
        float needleThickness = 3.0f;
        float needlePulseSpeed = 4.0f;   // Subtle glow/pulse speed on the needle
        int segmentCount = 10;           // Used when barStyle == Segmented
        bool showPercentageText = false; // Optional "100%" readout near the bar

        // Colors
        DirectX::SimpleMath::Vector4 barTrackColor  = DirectX::SimpleMath::Vector4(0.35f, 0.32f, 0.25f, 0.85f);
        DirectX::SimpleMath::Vector4 barFillColor   = DirectX::SimpleMath::Vector4(0.78f, 0.72f, 0.52f, 1.0f);
        DirectX::SimpleMath::Vector4 barNeedleColor = DirectX::SimpleMath::Vector4(0.98f, 0.92f, 0.72f, 1.0f);

        // Optional Custom Bar Textures (.dds)
        std::wstring barTrackTexturePath = L"";
        std::wstring barFillTexturePath = L"";
        std::wstring barNeedleTexturePath = L"";

        // Lore / Tips Cycler (Bottom-Left)
        bool showTips = true;
        DirectX::SimpleMath::Vector2 tipNormalizedPos = DirectX::SimpleMath::Vector2(0.06f, 0.72f);
        std::vector<LoadingTip> tips;
    };

    /**
     * @enum TransitionPhase
     * @brief 4-phase transition state machine.
     */
    enum class TransitionPhase
    {
        None,
        FadeOut,
        LoadingSwitch,
        FadeIn
    };

    class SceneManager
    {
    private:
        using SceneFactory = std::function<std::unique_ptr<IScene>()>;

        struct ActiveScene
        {
            std::string name;
            std::unique_ptr<IScene> instance;
        };

        struct PendingLoad
        {
            std::string sceneName;
            LoadSceneMode mode;
        };

        std::unordered_map<std::string, SceneFactory> m_sceneRegistry;
        std::vector<ActiveScene> m_activeScenes;

        std::vector<PendingLoad> m_pendingLoads;
        std::vector<std::string> m_pendingUnloads;

        // Transition system state
        SceneTransitionDesc m_defaultTransitionDesc;
        SceneTransitionDesc m_currentTransitionDesc;
        TransitionPhase m_transitionPhase = TransitionPhase::None;
        float m_transitionTimer = 0.0f;
        float m_transitionAlpha = 0.0f;
        float m_animTimer = 0.0f;
        float m_loadingProgress = 0.0f;
        float m_needlePulseTimer = 0.0f;
        size_t m_activeTipIndex = 0;
        bool m_hasSwappedScenes = false;
        bool m_isPreviewOnly = false;
        bool m_isTransitionPaused = false;
        std::string m_pendingTargetScene;
        LoadSceneMode m_pendingTargetMode = LoadSceneMode::Single;
        int m_loadingCoverFrames = 0;
        bool m_hasInitialBooted = false;

        // Overlay GPU resources
        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
        std::unique_ptr<DirectX::SpriteFont> m_spriteFont;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_whiteTextureSRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_transitionTextureSRV;
        std::wstring m_loadedTexturePath;
        DirectX::SimpleMath::Vector2 m_textureDimensions = DirectX::SimpleMath::Vector2(0.0f, 0.0f);

        // Custom Bar textures
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_barTrackSRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_barFillSRV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_barNeedleSRV;
        std::wstring m_loadedBarTrackPath;
        std::wstring m_loadedBarFillPath;
        std::wstring m_loadedBarNeedlePath;

        void EnsureWhiteTexture(ID3D11Device* device);
        void EnsureSpriteBatch(ID3D11DeviceContext* context);
        void EnsureFont(ID3D11Device* device);
        void EnsureTransitionTexture(ID3D11Device* device, const std::wstring& path);
        void EnsureCustomTexture(ID3D11Device* device, const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, std::wstring& loadedPath);

        void RenderLoadingBar(GameContext& context, const D3D11_VIEWPORT& vp, float alpha);
        void RenderTips(GameContext& context, const D3D11_VIEWPORT& vp, float alpha);

    public:
        SceneManager();
        ~SceneManager() = default;

        template <class TScene>
        void RegisterScene(const std::string& sceneName)
        {
            m_sceneRegistry[sceneName] = []() { return std::make_unique<TScene>(); };
        }

        void LoadScene(const std::string& sceneName, LoadSceneMode mode = LoadSceneMode::Single);
        void LoadScene(const std::string& sceneName, const SceneTransitionDesc& desc);
        void PreviewTransition(const SceneTransitionDesc& desc);
        void StartPausedPreview(const SceneTransitionDesc& desc);
        void StopTransitionPreview();
        void UnloadScene(const std::string& sceneName);

        bool IsTransitioning() const { return m_transitionPhase != TransitionPhase::None; }
        TransitionPhase GetTransitionPhase() const { return m_transitionPhase; }
        float GetTransitionAlpha() const { return m_transitionAlpha; }
        void SetTransitionAlpha(float alpha) { m_transitionAlpha = (alpha < 0.0f) ? 0.0f : ((alpha > 1.0f) ? 1.0f : alpha); }
        float GetLoadingProgress() const { return m_loadingProgress; }
        void SetLoadingProgress(float progress) { m_loadingProgress = (progress < 0.0f) ? 0.0f : ((progress > 1.0f) ? 1.0f : progress); }

        bool IsTransitionPaused() const { return m_isTransitionPaused; }
        void SetTransitionPaused(bool paused) { m_isTransitionPaused = paused; }
        void ToggleTransitionPause() { m_isTransitionPaused = !m_isTransitionPaused; }

        size_t GetActiveTipIndex() const { return m_activeTipIndex; }
        void SetActiveTipIndex(size_t index) { m_activeTipIndex = index; }

        void UpdateActiveTransitionDesc(const SceneTransitionDesc& desc) { m_currentTransitionDesc = desc; }

        const SceneTransitionDesc& GetDefaultTransition() const { return m_defaultTransitionDesc; }
        void SetDefaultTransition(const SceneTransitionDesc& desc) { m_defaultTransitionDesc = desc; }

        bool LoadTransitionConfig(const std::string& path = "Resources/Data/SceneTransition.json");
        bool SaveTransitionConfig(const std::string& path = "Resources/Data/SceneTransition.json") const;

        std::vector<std::string> GetRegisteredSceneNames() const;
        std::string GetCurrentSceneName() const;

        void Update(GameContext& context);
        void Render(GameContext& context);
        void RenderTransitionOverlay(GameContext& gameContext, const D3D11_VIEWPORT& targetViewport);

    };
}