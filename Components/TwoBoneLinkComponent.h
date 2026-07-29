#pragma once
#include "IComponent.h"
#include <string>

namespace HEIN
{
	class TransformComponent;
	class SkinnedModelComponent;
	class CapsuleColliderComponent;

	class TwoBoneLinkComponent : public IComponent
	{
		SkinnedModelComponent* m_targetModel;
		std::wstring m_boneAName;
		std::wstring m_boneBName;
		int m_boneAIndex;
		int m_boneBIndex;

		// Target
		CapsuleColliderComponent* m_linkedCapsule;
		std::wstring m_linkedColliderTag;

	public:
		std::string GetComponentName() const override { return "TwoBoneLinkComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


		TwoBoneLinkComponent(Actor* owner);
	
		void Initialize(
			SkinnedModelComponent* targetModel,
			const std::wstring& boneA,
			const std::wstring& boneB
		);
		void LinkTo(CapsuleColliderComponent* capsule);

		void Start() override;
		void Update(float /*deltaTime*/) override {}
		void LateUpdate(float deltaTime) override;

		void Draw(
			GameContext& /*gameContext*/, 
			const DirectX::SimpleMath::Matrix& /*world*/, 
			const DirectX::SimpleMath::Matrix& /*view*/, 
			const DirectX::SimpleMath::Matrix& /*proj*/
		) override {}
	};
}


