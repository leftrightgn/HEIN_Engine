#pragma once
#include "IComponent.h"
#include <string>

namespace HEIN
{
	class TransformComponent;
	class SkinnedModelComponent;
	class StaticModelComponent;
	class ColliderComponent;

	class BoneLinkComponent : public IComponent
	{
		SkinnedModelComponent* m_targetModel;
		StaticModelComponent* m_targetStaticModel;
		std::wstring m_targetBoneName;
		int m_targetBoneIndex;

		// Target
		TransformComponent* m_linkedTransform;
		ColliderComponent* m_linkedCollider;
		DirectX::SimpleMath::Vector3* m_linkedPosition;
		std::wstring m_linkedColliderTag;

	public:
		
		BoneLinkComponent(Actor* owner);
	
		void Initialize(SkinnedModelComponent* targetModel, const std::wstring& targetBoneName);
		void Initialize(SkinnedModelComponent* targetModel, int targetBoneIndex);
		void Initialize(StaticModelComponent* targetModel, const std::wstring& targetBoneName);
		void Initialize(StaticModelComponent* targetModel, int targetBoneIndex);

		void LinkTo(TransformComponent* transfrom);
		void LinkTo(ColliderComponent* collider);
		void LinkTo(DirectX::SimpleMath::Vector3* position);

		void Start() override;
		void Update(float /*deltaTime*/) override {}
		void LateUpdate(float deltaTime) override;

		void Draw(
			GameContext& /*gameContext*/,
			const DirectX::SimpleMath::Matrix& /*world*/,
			const DirectX::SimpleMath::Matrix& /*view*/,
			const DirectX::SimpleMath::Matrix& /*proj*/
		) override { }

		std::string GetComponentName() const override { return "BoneLinkComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


	};
}


