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

		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		std::map<void*, Microsoft::WRL::ComPtr<ID3D11InputLayout>> m_inputLayouts;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbMatrices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbLighting;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
		std::vector<uint8_t> m_vsBytecode;

		void InitializeShaders(ID3D11Device* device);
	public:
		std::wstring m_modelPath;
		std::wstring m_textureDir;
		bool m_castShadows = true;

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

		void DrawShadow(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& lightViewProj
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

		void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }
		bool CastsShadows() const { return m_castShadows; }

		// Ghost Effect API
		void SetGhostMode(bool enable) { m_isGhostMode = enable; }
		bool IsGhostMode() const { return m_isGhostMode; }
		void SetGhostColor(const DirectX::SimpleMath::Vector4& col) { m_ghostColor = col; }
		const DirectX::SimpleMath::Vector4& GetGhostColor() const { return m_ghostColor; }
		void SetGhostRim(float power, float intensity) { m_ghostRimPower = power; m_ghostRimIntensity = intensity; }
		float GetGhostRimPower() const { return m_ghostRimPower; }
		float GetGhostRimIntensity() const { return m_ghostRimIntensity; }

	private:
		// Ghost / Transparency Effect Parameters
		bool m_isGhostMode = false;
		DirectX::SimpleMath::Vector4 m_ghostColor = DirectX::SimpleMath::Vector4(0.25f, 0.75f, 1.0f, 0.4f);
		float m_ghostRimPower = 3.0f;
		float m_ghostRimIntensity = 2.0f;
		float m_ghostTime = 0.0f;
	};
}
