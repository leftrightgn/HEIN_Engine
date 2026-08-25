#pragma once
#include "ColliderComponent.h"
#include "Components/TerrainComponent.h"

namespace HEIN
{
	class TerrainColliderComponent : public ColliderComponent
	{
	private:
		TerrainComponent* m_terrain = nullptr;

	public:
		TerrainColliderComponent(Actor* owner);
		virtual ~TerrainColliderComponent() = default;

		std::string GetComponentName() const override { return "TerrainColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		void Start() override;
		void SyncColliderState() override;
		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		bool GetHeightAtPosition(float worldX, float worldZ, float& outHeight, DirectX::SimpleMath::Vector3& outNormal);
		bool GetCellAtPosition(float worldX, float worldZ, int& outCol, int& outRow);
	};
}
