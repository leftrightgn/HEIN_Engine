#pragma once
#include "IComponent.h"
#include "DirectXTK_Utilities/Animation.h"
#include <map>

namespace HEIN
{
	/// <summary>
	/// Manages skeletal animations, bone matrix palette calculations, and Mixamo SDKMesh rendering.
	/// Executes during Phase C (Transform Cascading & Skeletal Animation) to sync bone transformations
	/// for accurate physics and combat rendering without 1-frame delays.
	/// </summary>
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
		std::unordered_map<std::string, std::wstring> m_animationPaths;
		std::string m_lastError;

		DX::AnimationSDKMESH* m_currentAnimation = nullptr;
		DX::AnimationSDKMESH* m_targetAnimation = nullptr;

		bool m_isVisible = true;
		bool m_isBlending = false;
		float m_blendTimer = 0.0f;
		float m_blendDuration = 0.0f;
		
		bool m_needsReload = false;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout; // For CustomSkinned.hlsl
		std::map<void*, Microsoft::WRL::ComPtr<ID3D11InputLayout>> m_staticInputLayouts; // For non-skinned parts (uses void* to avoid missing includes)
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbMatrices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbLighting;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;

	public:
		
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
		) override;

		void DrawShadow(
			GameContext& gameContext, 
			const DirectX::SimpleMath::Matrix& lightViewProj
		) override;

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

		std::string GetComponentName() const override { return "SkinnedModelComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;

		void OnInspectorGUI(GameContext& gameContext) override;

		int GetBoneIndex(const std::wstring boneName);

		void LoadAnimation(const std::string& name, const wchar_t* animPath);
		void RemoveAnimation(const std::string& name);
		void ChangeAnimation(const std::string& name);
		void CrossfadeAnimation(const std::string& name, float duration, bool forceRestart = false);

		void SetVisible(bool visible) { m_isVisible = visible; }
		bool IsVisible() const { return m_isVisible; }

		const DirectX::SimpleMath::Matrix* GetCurrentLocalBones() const
		{
			if (m_isBlending)
			{
				return reinterpret_cast<const DirectX::SimpleMath::Matrix*>(m_blendedLocalBones.get());
			}
			if (m_currentAnimation)
			{
				return reinterpret_cast<const DirectX::SimpleMath::Matrix*>(m_currentAnimation->GetLocalBones());
			}
			return nullptr;
		}

		size_t GetBoneCount() const { return m_model ? m_model->bones.size() : 0; }

		std::string GetBoneName(int index) const
		{
			if (!m_model || index < 0 || index >= m_model->bones.size()) return "Unknown";
			return std::string(m_model->bones[index].name.begin(), m_model->bones[index].name.end());
		}

		int GetParentBoneIndex(int index) const
		{
			if (!m_model || index < 0 || index >= m_model->bones.size()) return -1;
			return m_model->bones[index].parentIndex;
		}

		DirectX::SimpleMath::Matrix GetBindPoseLocalMatrix(int index) const
		{
			if (!m_model || index < 0 || index >= m_model->bones.size()) return DirectX::SimpleMath::Matrix::Identity;
			return m_model->boneMatrices[index];
		}

		// This function hijacks the rendering pipeline to draw custom Editor poses
		void OverrideBones(const DirectX::SimpleMath::Matrix* localBones);
		
	};
}

