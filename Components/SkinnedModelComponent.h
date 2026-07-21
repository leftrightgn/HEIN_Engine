#pragma once
#include "IComponent.h"
#include "DirectXTK_Utilities/Animation.h"

namespace HEIN
{
	class SkinnedModelComponent : public IComponent
	{
	private:

		// Model And Animation Data 
		DirectX::ModelBone::TransformArray m_drawBones;
		DirectX::ModelBone::TransformArray m_skinBones;
		DirectX::ModelBone::TransformArray m_targetBones;
		DirectX::ModelBone::TransformArray m_shapShotBones;

		DirectX::ModelBone::TransformArray m_blendedLocalBones;
		//DX::AnimationSDKMESH m_animation;
		static std::shared_ptr<DirectX::EffectFactory> s_fxFactory;

		static std::unordered_map<std::wstring, std::weak_ptr<DirectX::Model>> s_modelCache;

		std::wstring m_modelPath;
		std::wstring m_textureDir;

		std::shared_ptr<DirectX::Model> m_model;

		std::unordered_map<std::string, std::unique_ptr<DX::AnimationSDKMESH>> m_animations;

		DX::AnimationSDKMESH* m_currentAnimation = nullptr;
		DX::AnimationSDKMESH* m_targetAnimation = nullptr;

		bool m_isVisible = true;
		bool m_isBlending = false;
		float m_blendTimer = 0.0f;
		float m_blendDuration = 0.0f;

	public:
		std::string GetComponentName() const override { return "SkinnedModelComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;


		SkinnedModelComponent(Actor* owner);

		void Initialize(
			GameContext& gameContext, 
			const wchar_t* modelPath, 
			const wchar_t* textureDir
		);

		void Update(float deltaTime) override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world, 
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		DirectX::SimpleMath::Vector3 GetBoneWorldPosition(
			const wchar_t* boneName,
			const DirectX::SimpleMath::Matrix& actorWorldMatrix
		);
		DirectX::SimpleMath::Vector3 GetBoneWorldPosition(
			const int boneNum,
			const DirectX::SimpleMath::Matrix& actorWorldMatrix
		);

		DirectX::SimpleMath::Matrix GetBoneWorldMatrix(
			const wchar_t* boneName,
			const DirectX::SimpleMath::Matrix& actorWorldMatrix
		);

		DirectX::SimpleMath::Matrix GetBoneWorldMatrix(
			const int boneNum,
			const DirectX::SimpleMath::Matrix& actorWorldMatrix
		);

		int GetBoneIndex(const std::wstring boneName);

		void LoadAnimation(const std::string& name, const wchar_t* animPath);
		void ChangeAnimation(const std::string& name);
		void CrossfadeAnimation(const std::string& name, float duration, bool forceRestart = false);

		void SetVisible(bool visible) { m_isVisible = visible; }
		bool IsVisible() const { return m_isVisible; }


	};
}

