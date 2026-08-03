#pragma once
#include "Components/IComponent.h"
#include "Framework/GameContext.h"
#include "pch.h"
#include <Model.h>

namespace HEIN
{
	class StaticModelComponent : public IComponent
	{
	private:
		DirectX::ModelBone::TransformArray m_drawBones;
		static std::shared_ptr<DirectX::EffectFactory> s_fxFactory;
		static std::unordered_map<std::wstring, std::weak_ptr<DirectX::Model>> s_modelCache;

		std::shared_ptr<DirectX::Model> m_model;
		bool m_isVisible = true;
		bool m_needsReload = false;
		std::string m_lastError;

	public:
		std::wstring m_modelPath;
		std::wstring m_textureDir;

		StaticModelComponent(Actor* owner);

		void Initialize(
			GameContext& gameContext,
			const wchar_t* modelPath,
			const wchar_t* textureDir = nullptr
		);
      
		void Update(float /*deltaTime*/) override;

		void Draw(
			GameContext& gameContext, 
			const DirectX::SimpleMath::Matrix& world, 
			const DirectX::SimpleMath::Matrix& view, 
			const DirectX::SimpleMath::Matrix& proj
		) override;

		std::string GetComponentName() const override { return "StaticModelComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;

		void OnInspectorGUI(GameContext& gameContext) override;

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

		DirectX::BoundingBox GetBoundingBox() const;
		DirectX::BoundingSphere GetBoundingSphere() const;

		void SetVisible(bool visible) { m_isVisible = visible; }
		bool IsVisible() const { return m_isVisible; }
	};
}
