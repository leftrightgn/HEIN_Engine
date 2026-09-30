#pragma once
#include "Components/IComponent.h"
#include "Framework/GameContext.h"
#include "DebugingTools/IGizmoEditable.h"
#include <string>
#include <vector>
#include <memory>
#include <SimpleMath.h>
#include "SpriteBatch.h"

namespace HEIN
{
    /**
     * @brief Defines the type of a single gauge bar within the GaugeComponent.
     */
    enum class GaugeType
    {
        HP = 0,
        Dodge = 1,
        Block = 2,
        Custom = 3
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
        // Identity
        std::string name = "HP";
        GaugeType gaugeType = GaugeType::HP;

        // Values (runtime-driven)
        float currentValue = 100.0f;
        float maxValue = 100.0f;

        // Layout: position offset relative to the component anchor
        // In ScreenSpace mode: absolute screen coordinates on 1280x720 reference canvas
        // In WorldToScreen mode: pixel offset from the projected 3D position
        DirectX::SimpleMath::Vector2 offset = { 0.0f, 0.0f };
        DirectX::SimpleMath::Vector2 size = { 200.0f, 20.0f };

        // Fill direction: false = left-to-right, true = right-to-left
        bool fillRightToLeft = false;

        // Auto-bind: when true, values are automatically synced from data source components.
        // HP reads from HealthComponent; Dodge/Block are pushed by game-side CombatBlackBoard.
        // Custom type always requires manual SetValue().
        bool autoBind = true;

        // Colors
        DirectX::SimpleMath::Vector4 fillColor = { 0.2f, 0.8f, 0.2f, 1.0f };    // Green fill
        DirectX::SimpleMath::Vector4 bgColor = { 0.15f, 0.15f, 0.15f, 0.8f };    // Dark background
        DirectX::SimpleMath::Vector4 borderColor = { 0.9f, 0.9f, 0.9f, 1.0f };   // White border

        // Low-health color threshold (fills with this color when ratio < threshold)
        float lowThreshold = 0.25f;
        DirectX::SimpleMath::Vector4 lowColor = { 0.9f, 0.1f, 0.1f, 1.0f };      // Red for low

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
        float fontSize = 0.8f;
        DirectX::SimpleMath::Vector4 labelColor = { 1.0f, 1.0f, 1.0f, 1.0f };
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
     * - Player actor: Add GaugeComponent with ScreenSpace mode, add HP/Dodge/Block bars.
     * - Enemy actor: Add GaugeComponent with WorldToScreen mode, set worldOffset to above head.
     *   The bars follow the enemy in 3D space, projected to 2D each frame.
     */
    class GaugeComponent : public IComponent, public IGizmoEditable
    {
    private:
        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;

        // Anchor settings
        GaugeAnchorMode m_anchorMode = GaugeAnchorMode::ScreenSpace;

        // For ScreenSpace mode: base position on the 1280x720 reference canvas
        DirectX::SimpleMath::Vector2 m_screenPosition = { 50.0f, 600.0f };

        // For WorldToScreen mode: 3D offset from the parent enemy actor's world position
        // e.g., (0, 2.5, 0) to place above the head
        DirectX::SimpleMath::Vector3 m_worldOffset = { 0.0f, 2.5f, 0.0f };

        // Array of gauge bar definitions
        std::vector<GaugeBarDef> m_bars;

        // Visibility
        bool m_isVisible = true;

        // --- Range-based activation ---
        // When the gauge is a child actor of an enemy, it should only appear
        // when the player is within activationRange of the parent enemy.
        bool m_useRangeActivation = true;     // Enable/disable range check
        float m_activationRange = 20.0f;      // Distance in world units
        float m_deactivationRange = 25.0f;    // Slightly larger to prevent flickering (hysteresis)
        bool m_isInRange = false;             // Runtime state: currently within range?

        // Reference canvas (matches UIButtonComponent for consistent scaling)
        static constexpr float kRefWidth = 1280.0f;
        static constexpr float kRefHeight = 720.0f;

        // Cached scale factors for responsive rendering
        float m_lastScaleX = 1.0f;
        float m_lastScaleY = 1.0f;
        float m_lastVpX = 0.0f;
        float m_lastVpY = 0.0f;

        // Internal helpers
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

