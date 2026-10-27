#pragma once
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "Components/IComponent.h"
#include "DebugingTools/IGizmoEditable.h"
#include "Framework/GameContext.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace HEIN
{
    /**
     * @brief Defines the type of a single gauge bar within the GaugeComponent.
     */
    enum class GaugeType
    {
        HP = 0,
        Stamina = 1,
        Dodge = 2,
        Block = 3,
        Custom = 4
    };

    /**
     * @brief Controls how the gauge is anchored on screen.
     * 
     * ScreenSpace: Fixed screen coordinates (like Unity Canvas - Screen Space Overlay).
     *              Used for player HUD elements pinned at specific screen positions.
     * 
     * WorldToScreen: Projects a 3D world position to screen coordinates each frame.
     *                Used for enemy health bars that follow the actor (e.g., above the head).
     */
    enum class GaugeAnchorMode
    {
        ScreenSpace = 0,     // Fixed screen position (player HUD)
        WorldToScreen = 1    // Follow 3D actor position projected to 2D (enemy overhead bars)
    };

    /**
     * @brief A single gauge bar definition.
     * 
     * Each GaugeBarDef describes one bar (e.g., HP bar, Dodge bar, Block bar).
     * Multiple bars can be attached to a single GaugeComponent to create
     * complex HUD layouts like a player status panel.
     */
    struct GaugeBarDef
    {
        static constexpr float DEFAULT_MAX_VALUE = 100.0f;
        static constexpr float DEFAULT_BAR_WIDTH = 200.0f;
        static constexpr float DEFAULT_BAR_HEIGHT = 20.0f;
        static constexpr float DEFAULT_LOW_THRESHOLD = 0.25f;
        static constexpr float DEFAULT_FONT_SIZE = 0.8f;
        static constexpr float DEFAULT_TRAIL_HOLD_DELAY = 0.45f;
        static constexpr float DEFAULT_TRAIL_DRAIN_SPEED = 4.0f;
        static constexpr float DEFAULT_BEVEL_INTENSITY = 0.85f;
        static constexpr float DEFAULT_MENISCUS_GLOW = 0.65f;
        static constexpr float DEFAULT_DAMAGE_DISPLAY_DURATION = 1.8f;

        // Identity
        std::string name = "HP";
        GaugeType gaugeType = GaugeType::HP;

        // Values (runtime-driven)
        float currentValue = DEFAULT_MAX_VALUE;
        float maxValue = DEFAULT_MAX_VALUE;

        // Layout: position offset relative to the component anchor
        // In ScreenSpace mode: absolute screen coordinates on 1280x720 reference canvas
        // In WorldToScreen mode: pixel offset from the projected 3D position
        DirectX::SimpleMath::Vector2 offset = { 0.0f, 0.0f };
        DirectX::SimpleMath::Vector2 size = { DEFAULT_BAR_WIDTH, DEFAULT_BAR_HEIGHT };

        // Fill direction: false = left-to-right, true = right-to-left
        bool fillRightToLeft = false;

        // Fill range (normalized 0.0 to 1.0 relative to bar size)
        // Controls where the active meter starts and ends within decorative border frames.
        // fillStartPoint: where 0% fill starts (default 0.0f = left edge)
        // fillEndPoint: where 100% fill ends (default 1.0f = right edge)
        float fillStartPoint = 0.0f;
        float fillEndPoint = 1.0f;

        // Texture clipping: when true, crops texture UVs instead of squashing/stretching
        bool clipTexture = true;

        // When true, texture UVs map to [fillStartPoint, fillEndPoint] (ideal when fill texture
        // has the same canvas/padding as the border frame). When false, texture UVs map [0.0, 1.0]
        // across the active fill area (ideal for trimmed/standalone fill textures).
        bool fillTexMatchesFrame = true;

        // Auto-bind: when true, values are automatically synced from data source components.
        // HP reads from HealthComponent; Stamina (and legacy Dodge/Block) are pushed by game-side CombatBlackBoard.
        // Custom type always requires manual SetValue().
        bool autoBind = true;

        // Colors
        DirectX::SimpleMath::Vector4 fillColor = { 0.72f, 0.14f, 0.14f, 1.0f };    // Crimson fill
        DirectX::SimpleMath::Vector4 bgColor = { 0.10f, 0.09f, 0.09f, 0.85f };     // Dark recessed trough
        DirectX::SimpleMath::Vector4 borderColor = { 0.76f, 0.64f, 0.38f, 1.0f };  // Gilded bronze/gold border

        // Low-health color threshold (fills with this color when ratio < threshold)
        float lowThreshold = DEFAULT_LOW_THRESHOLD;
        DirectX::SimpleMath::Vector4 lowColor = { 0.9f, 0.1f, 0.1f, 1.0f };        // Red for low

        // Elden Ring Shader & Delayed Damage Trail settings
        bool useEldenRingShader = true;
        bool showDamageTrail = true;
        DirectX::SimpleMath::Vector4 trailColor = { 0.95f, 0.76f, 0.26f, 0.95f };  // Warm Elden Ring gold/amber trail
        float trailHoldDelay = DEFAULT_TRAIL_HOLD_DELAY;
        float trailDrainSpeed = DEFAULT_TRAIL_DRAIN_SPEED;
        float bevelIntensity = DEFAULT_BEVEL_INTENSITY;
        float meniscusGlow = DEFAULT_MENISCUS_GLOW;
        bool useProceduralFrame = true;
        bool showDamageNumbers = true;
        float damageDisplayDuration = DEFAULT_DAMAGE_DISPLAY_DURATION;

        // Runtime-only trail & combo damage tracking state
        mutable bool trailInitialized = false;
        mutable float prevValue = DEFAULT_MAX_VALUE;
        mutable float trailValue = DEFAULT_MAX_VALUE;
        mutable float trailHoldTimer = 0.0f;
        mutable float damageAccumulator = 0.0f;
        mutable float damageDisplayTimer = 0.0f;

        // Textures (optional, override solid color rendering with textured sprites)
        std::wstring backgroundTexPath;     // Background bar texture
        std::wstring fillTexPath;           // Fill bar texture
        std::wstring borderTexPath;         // Border/frame overlay texture

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> backgroundTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fillTex;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> borderTex;

        // Label
        bool showLabel = true;
        bool showValue = true;
        float fontSize = DEFAULT_FONT_SIZE;
        DirectX::SimpleMath::Vector4 labelColor = { 0.92f, 0.88f, 0.78f, 1.0f };
        DirectX::SimpleMath::Vector2 textOffset = { 0.0f, 0.0f };

        // SpriteFont typography
        std::wstring spriteFontPath;
        std::shared_ptr<DirectX::SpriteFont> spriteFont = nullptr;

        void RemoveBackgroundTexture()
        {
            backgroundTexPath.clear();
            backgroundTex.Reset();
        }

        void RemoveFillTexture()
        {
            fillTexPath.clear();
            fillTex.Reset();
        }

        void RemoveBorderTexture()
        {
            borderTexPath.clear();
            borderTex.Reset();
        }

        void RemoveSpriteFont()
        {
            spriteFontPath.clear();
            spriteFont.reset();
        }

        void RemoveAllTextures(bool resetFillRange = true)
        {
            RemoveBackgroundTexture();
            RemoveFillTexture();
            RemoveBorderTexture();
            if (resetFillRange)
            {
                fillStartPoint = 0.0f;
                fillEndPoint = 1.0f;
            }
        }
    };

    /**
     * @class GaugeComponent
     * @brief A Unity Canvas-style HUD gauge component for rendering HP, Dodge, Block,
     *        and custom gauge bars.
     * 
     * Supports two anchor modes:
     * - ScreenSpace: Fixed screen coordinates for player HUD (HP/Dodge/Block at bottom-left, etc.)
     * - WorldToScreen: Projects from a 3D world offset to screen space, used for enemy overhead bars.
     * 
     * The component renders using SpriteBatch (textured bars) and ImGui draw lists (labels/values).
     * All settings serialize to JSON for the level editor pipeline.
     * 
     * Usage examples:
     * - Player actor: Add GaugeComponent with ScreenSpace mode, add HP/Stamina bars.
     * - Enemy actor: Add GaugeComponent with WorldToScreen mode, set worldOffset to above head.
     *   The bars follow the enemy in 3D space, projected to 2D each frame.
     */
    class GaugeComponent : public IComponent, public IGizmoEditable
    {
    private:
        struct CB_GaugeUI
        {
            DirectX::SimpleMath::Vector4 fillColor;
            DirectX::SimpleMath::Vector4 trailColor;
            DirectX::SimpleMath::Vector4 bgColor;
            DirectX::SimpleMath::Vector4 borderColor;
            float fillRatio;
            float trailRatio;
            float fillStart;
            float fillEnd;
            DirectX::SimpleMath::Vector2 barSizePixels;
            float fillRightToLeft;
            float hasFillTexture;
            float hasBgTexture;
            float clipTexture;
            float fillTexMatchesFrame;
            float useProceduralFrame;
            float bevelIntensity;
            float meniscusGlow;
            float padding0;
            float padding1;
        };

        static constexpr float kRefWidth = 1280.0f;
        static constexpr float kRefHeight = 720.0f;
        static constexpr float kDefaultScreenPosX = 50.0f;
        static constexpr float kDefaultScreenPosY = 600.0f;
        static constexpr float kDefaultWorldOffsetY = 2.5f;
        static constexpr float kDefaultActivationRange = 20.0f;
        static constexpr float kDefaultDeactivationRange = 25.0f;
        static constexpr float kHysteresisMargin = 5.0f;
        static constexpr float kValueChangeEpsilon = 0.01f;
        static constexpr float kMinTrailLinearDrainRatio = 0.15f;
        static constexpr float kDamageFadeOutWindow = 0.35f;
        static constexpr float kMaxColorChannelValue = 255.0f;
        static constexpr UINT  kDefaultMaxAnisotropy = 16;

        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_gaugePixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_gaugeCB;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_gaugeSampler;

        // Anchor settings
        GaugeAnchorMode m_anchorMode = GaugeAnchorMode::ScreenSpace;

        // For ScreenSpace mode: base position on the 1280x720 reference canvas
        DirectX::SimpleMath::Vector2 m_screenPosition = { kDefaultScreenPosX, kDefaultScreenPosY };

        // For WorldToScreen mode: 3D offset from the parent enemy actor's world position
        // e.g., (0, 2.5, 0) to place above the head
        DirectX::SimpleMath::Vector3 m_worldOffset = { 0.0f, kDefaultWorldOffsetY, 0.0f };

        // Array of gauge bar definitions
        std::vector<GaugeBarDef> m_bars;

        // Visibility
        bool m_isVisible = true;

        // --- Range-based activation ---
        // When the gauge is a child actor of an enemy, it should only appear
        // when the player is within activationRange of the parent enemy.
        bool m_useRangeActivation = true;     // Enable/disable range check
        float m_activationRange = kDefaultActivationRange;      // Distance in world units
        float m_deactivationRange = kDefaultDeactivationRange;  // Slightly larger to prevent flickering (hysteresis)
        bool m_isInRange = false;             // Runtime state: currently within range?

        void LoadBarFont(ID3D11Device* device, GaugeBarDef& bar);
        static std::shared_ptr<DirectX::SpriteFont> GetOrCreateFont(ID3D11Device* device, const std::wstring& path);

        // Cached scale factors for responsive rendering
        float m_lastScaleX = 1.0f;
        float m_lastScaleY = 1.0f;
        float m_lastVpX = 0.0f;
        float m_lastVpY = 0.0f;

        // Internal helpers
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_whiteTex;
        void EnsureWhiteTexture(ID3D11Device* device);
        void EnsureGaugeShader(ID3D11Device* device);
        static void UpdateBarValueTracking(const GaugeBarDef& bar);

        void DrawBar(GameContext& gameContext, const GaugeBarDef& bar, 
                     float anchorX, float anchorY, float scaleX, float scaleY);
        void LoadBarTextures(ID3D11Device* device, GaugeBarDef& bar);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColorTexture(
            ID3D11Device* device, const DirectX::SimpleMath::Vector4& color);

        bool WorldToScreenPoint(GameContext& gameContext,
                                const DirectX::SimpleMath::Vector3& worldPos,
                                float& outX, float& outY);

        /** @brief Finds the parent enemy actor via the owner's ParentID. */
        Actor* GetParentActor(GameContext& gameContext);

        /** @brief Finds the Player actor in the scene. */
        Actor* FindPlayerActor(GameContext& gameContext);

        /** @brief Gets the world position of the parent enemy (or self if no parent). */
        DirectX::SimpleMath::Vector3 GetTargetWorldPosition(GameContext& gameContext);

        /** @brief Auto-syncs bar values from data source components (HP from HealthComponent). */
        void SyncBoundBars(GameContext& gameContext);

    public:
        GaugeComponent(Actor* owner);

        void Initialize(GameContext& gameContext);

        void Start() override;
        void Update(float deltaTime) override;

        // Renders via Draw2D path (2D overlay on top of 3D scene)
        bool Is2D() const override { return true; }
        void Draw2D(GameContext& gameContext) override;

        // Also supports Draw() for editor preview
        void Draw(GameContext& gameContext,
            const DirectX::SimpleMath::Matrix& world,
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj) override;

        std::string GetComponentName() const override { return "GaugeComponent"; }
        nlohmann::json Serialize() override;
        void Deserialize(const nlohmann::json& data) override;
        void InitializeAfterDeserialize(GameContext& gameContext) override;
        void OnInspectorGUI(GameContext& gameContext) override;

        // IGizmoEditable: Enables visual editing of gauge positions in the editor
        void DrawGizmo(
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj,
            int operation,
            int mode
        ) override;

        // --- Runtime API for gameplay code ---

        /** @brief Adds a new gauge bar and returns its index. */
        int AddBar(const GaugeBarDef& bar);

        /** @brief Removes a bar by index. */
        void RemoveBar(int index);

        /** @brief Removes all textures (background, fill, border) from a bar by index. */
        void RemoveBarTextures(int index, bool resetFillRange = true);

        /** @brief Removes all textures from every bar in this component. */
        void RemoveAllBarsTextures(bool resetFillRange = true);

        /** @brief Gets a bar by index for modification. */
        GaugeBarDef* GetBar(int index);

        /** @brief Finds a bar by name. Returns nullptr if not found. */
        GaugeBarDef* FindBar(const std::string& name);

        /** @brief Gets the number of bars. */
        int GetBarCount() const { return static_cast<int>(m_bars.size()); }

        /** @brief Convenience: Sets the current value of the first bar matching the given type. */
        void SetValue(GaugeType type, float current, float max = -1.0f);

        /** @brief Convenience: Sets the current value of a bar by name. */
        void SetValue(const std::string& name, float current, float max = -1.0f);

        /** @brief Extensible sync delegate allowing game-specific components to feed data (Block, Dodge, etc.) to GaugeComponent. */
        using GaugeExternalSyncFunc = std::function<bool(Actor* target, GaugeType type, float& outCurrent, float& outMax)>;
        inline static GaugeExternalSyncFunc s_externalSyncHandler = nullptr;
        static void SetExternalSyncHandler(GaugeExternalSyncFunc handler) { s_externalSyncHandler = std::move(handler); }

        // Anchor mode
        GaugeAnchorMode GetAnchorMode() const { return m_anchorMode; }
        void SetAnchorMode(GaugeAnchorMode mode) { m_anchorMode = mode; }

        // Screen position (ScreenSpace mode)
        DirectX::SimpleMath::Vector2 GetScreenPosition() const { return m_screenPosition; }
        void SetScreenPosition(const DirectX::SimpleMath::Vector2& pos) { m_screenPosition = pos; }

        // World offset (WorldToScreen mode)
        DirectX::SimpleMath::Vector3 GetWorldOffset() const { return m_worldOffset; }
        void SetWorldOffset(const DirectX::SimpleMath::Vector3& offset) { m_worldOffset = offset; }

        // Visibility
        bool IsVisible() const { return m_isVisible; }
        void SetVisible(bool visible) { m_isVisible = visible; }

        // Range-based activation (for enemy child gauges)
        bool GetUseRangeActivation() const { return m_useRangeActivation; }
        void SetUseRangeActivation(bool use) { m_useRangeActivation = use; }

        float GetActivationRange() const { return m_activationRange; }
        void SetActivationRange(float range) { m_activationRange = range; }

        float GetDeactivationRange() const { return m_deactivationRange; }
        void SetDeactivationRange(float range) { m_deactivationRange = range; }

        /** @brief Returns true if the player is currently within activation range. */
        bool IsInRange() const { return m_isInRange; }
    };
}
