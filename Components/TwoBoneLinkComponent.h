#pragma once
#include "IComponent.h"
#include <string>

namespace HEIN
{
	class TransformComponent;
	class SkinnedModelComponent;
	class StaticModelComponent;
	class CapsuleColliderComponent;

	/// <summary>
	/// Dynamically computes and spans a 3D capsule collider between two skeletal joints.
	/// Interpolates capsule midpoint, length, and rotation to prevent loose approximations in combat hitboxes.
	/// Mathematically derives orientation via FromToRotation and midpoint between the two bone positions.
	/// Executes during Phase C (LateUpdate) ensuring 0-frame latency with animation state.
	/// </summary>
	class TwoBoneLinkComponent : public IComponent
	{
		SkinnedModelComponent* m_targetModel;
		StaticModelComponent* m_targetStaticModel;
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
		void Initialize(
			StaticModelComponent* targetModel,
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


