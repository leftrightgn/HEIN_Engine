#pragma once
#include "Components/IComponent.h"
#include <SimpleMath.h>

namespace HEIN
{
    // Defines the Unity-style light types
    enum class LightType : int
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    class LightComponent : public IComponent
    {
    private:
        LightType m_lightType = LightType::Directional;
        DirectX::SimpleMath::Vector4 m_color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        float m_intensity = 1.0f;

        // Properties used specifically for Point and Spot lights
        float m_range = 50.0f;
        float m_spotAngle = 45.0f;

        bool m_castShadows = true;

    public:
        LightComponent(Actor* owner);
        ~LightComponent() override = default;

        void Start() override {}
        void Update(float deltaTime) override {}
        void Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj) override;

        // Getters for the Render System
        LightType GetLightType() const { return m_lightType; }
        DirectX::SimpleMath::Vector4 GetColor() const { return m_color; }
        float GetIntensity() const { return m_intensity; }
        float GetRange() const { return m_range; }
        float GetSpotAngle() const { return m_spotAngle; }
        bool CastsShadows() const { return m_castShadows; }

        // Core Engine Integrations
        std::string GetComponentName() const override { return "LightComponent"; }
        nlohmann::json Serialize() override;
        void Deserialize(const nlohmann::json& data) override;
        void OnInspectorGUI(GameContext& gameContext) override;
    };
}