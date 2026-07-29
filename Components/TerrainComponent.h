#pragma once
#include "IComponent.h"
#include <VertexTypes.h>
#include <Effects.h>
#include <string>


namespace HEIN
{
	class TerrainComponent : public IComponent
	{
	private:

		// Data
		struct HeightMapType
		{
			float x, y, z;
		};

		int m_terrainWidth;
		int m_terrainHeight;
		float m_heightScale;

		std::wstring m_heightMapFilename;

		std::vector<HeightMapType> m_heightMap;
		
		// DirectX 11 Buffers
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;

		int m_vertexCount;
		int m_indexCount;

		std::unique_ptr<DirectX::BasicEffect> m_effect;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		bool m_isVisible = true;
		bool m_isWireFrame = true;
		bool m_needsReload = false;

	public:

		TerrainComponent(Actor* owner);

		bool Initialize(GameContext& gameContext, const wchar_t* heightMapFilename, float heightScale);

		void Update(float deltaTime) override {}

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		std::string GetComponentName() const override { return "TerrainComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;
		void OnInspectorGUI(GameContext& gameContext) override;

	private:

		bool LoadHeightMap(const wchar_t* filename);
		bool InitializeBuffer(ID3D11Device* device);

	};
}