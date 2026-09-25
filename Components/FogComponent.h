#pragma once
#include "IComponent.h"
#include <SimpleMath.h>
#include <Common/json.hpp>

namespace HEIN
{
	class FogComponent : public IComponent
	{
	public:
		float m_fogStart = 100.0f;
		float m_fogEnd = 800.0f;
		DirectX::SimpleMath::Vector4 m_fogColor = DirectX::SimpleMath::Vector4(0.5f, 0.6f, 0.7f, 1.0f);

		FogComponent(Actor* owner);

		void Update(float deltaTime) override {}

		std::string GetComponentName() const override { return "FogComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;
	};
}
