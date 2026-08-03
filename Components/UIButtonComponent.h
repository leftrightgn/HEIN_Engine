#pragma once
#include "Components/IComponent.h"
#include "Framework/GameContext.h"
#include "DebugingTools/IGizmoEditable.h"
#include <string>
#include <memory>
#include <SimpleMath.h>
#include "SpriteBatch.h"

namespace HEIN
{
    enum class UIElementType
    {
        Button = 0,
        Image = 1,
        Text = 2
    };

    class UIButtonComponent : public IComponent, public IGizmoEditable
    {
    public:
        enum class ButtonState
        {
            Normal,
            Hover,
            Pressed
        };

    private:
        std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch;

        UIElementType m_elementType = UIElementType::Button;

        // Visual States & Textures
        std::wstring m_normalTexPath;
        std::wstring m_hoverTexPath;
        std::wstring m_pressedTexPath;

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_hoverTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pressedTexture;

        ButtonState m_currentState = ButtonState::Normal;

        // Screen Space Coordinates
        DirectX::SimpleMath::Vector2 m_position = DirectX::SimpleMath::Vector2(100.0f, 100.0f);
        DirectX::SimpleMath::Vector2 m_size = DirectX::SimpleMath::Vector2(200.0f, 50.0f);

        // Text & Typography Properties
        std::string m_text = "";
        DirectX::SimpleMath::Vector4 m_textColor = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        float m_fontSize = 1.0f;

        // Tint / Color Overlay
        DirectX::SimpleMath::Vector4 m_tintColor = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        bool m_wasMouseDown = false;
        bool m_isClicked = false;
        bool m_isVisible = true;

        static constexpr float kRefWidth = 1280.0f;
        static constexpr float kRefHeight = 720.0f;

        float m_lastScaleX = 1.0f;
        float m_lastScaleY = 1.0f;
        float m_lastVpX = 0.0f;
        float m_lastVpY = 0.0f;

    public:
        UIButtonComponent(Actor* owner);

        void Initialize(GameContext& gameContext,
            const wchar_t* normalTex = nullptr,
            const wchar_t* hoverTex = nullptr,
            const wchar_t* pressedTex = nullptr);

        void Start() override;
        void Update(float deltaTime) override;
        void Draw(GameContext& gameContext,
            const DirectX::SimpleMath::Matrix& world,
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj) override;

        bool Is2D() const override { return true; }
        void Draw2D(GameContext& gameContext) override;

        std::string GetComponentName() const override { return "UIButtonComponent"; }
        nlohmann::json Serialize() override;
        void Deserialize(const nlohmann::json& data) override;
        void InitializeAfterDeserialize(GameContext& gameContext) override;
        void OnInspectorGUI(GameContext& gameContext) override;
        void DrawGizmo(
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj,
            int operation,
            int mode
        ) override;

        // Interaction checks
        bool IsClicked();

        // Getters & Setters
        UIElementType GetElementType() const { return m_elementType; }
        void SetElementType(UIElementType type) { m_elementType = type; }

        const std::string& GetText() const { return m_text; }
        void SetText(const std::string& text) { m_text = text; }

        DirectX::SimpleMath::Vector4 GetTextColor() const { return m_textColor; }
        void SetTextColor(const DirectX::SimpleMath::Vector4& color) { m_textColor = color; }

        float GetFontSize() const { return m_fontSize; }
        void SetFontSize(float size) { m_fontSize = size; }

        DirectX::SimpleMath::Vector4 GetTintColor() const { return m_tintColor; }
        void SetTintColor(const DirectX::SimpleMath::Vector4& tint) { m_tintColor = tint; }

        DirectX::SimpleMath::Vector2 GetPosition() const { return m_position; }
        DirectX::SimpleMath::Vector2 GetSize() const { return m_size; }
        void SetPosition(const DirectX::SimpleMath::Vector2& pos) { m_position = pos; }
        void SetSize(const DirectX::SimpleMath::Vector2& size) { m_size = size; }

        bool IsVisible() const { return m_isVisible; }
        void SetVisible(bool visible) { m_isVisible = visible; }
    };
}