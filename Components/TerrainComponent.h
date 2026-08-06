#pragma once
#include "IComponent.h"
#include <VertexTypes.h>
#include <Effects.h>
#include <string>
#include <d3dcompiler.h>

namespace HEIN
{
	class TerrainComponent : public IComponent
	{
	private:

		struct MatrixBufferType
		{
			DirectX::XMMATRIX world;
			DirectX::XMMATRIX view;
			DirectX::XMMATRIX projection;
		};

		struct LightBufferType
		{
			DirectX::SimpleMath::Vector4 diffuseColor;
			DirectX::SimpleMath::Vector3 lightDirection;
			float hasTexture;
			float textureTiling;
			float hasNormalMap;
			DirectX::SimpleMath::Vector2 padding;
		};

		// Data
		struct HeightMapType
		{
			float x, y, z;
			float nx, ny, nz;
			float tx, ty, tz; // tangent
			float bx, by, bz; // binormal
			float r, g, b;
		};

		// Custom Vertex Sturcture
		struct TerrainVertexType
		{
			DirectX::SimpleMath::Vector3 position;
			DirectX::SimpleMath::Vector2 texture;
			DirectX::SimpleMath::Vector3 normal;
			DirectX::SimpleMath::Vector3 tangent;
			DirectX::SimpleMath::Vector3 binormal;
			DirectX::SimpleMath::Vector4 color;
		};

		int m_terrainWidth;
		int m_terrainHeight;
		float m_heightScale;

		std::wstring m_heightMapFilename;

		std::vector<HeightMapType> m_heightMap;
		
		// Custom Shader Objects;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampleState;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_matrixBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightBuffer;

		// DirectX 11 Buffers
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;

		int m_vertexCount;
		int m_indexCount;

		std::wstring m_textureFilename;

		std::wstring m_colorMapFilename;

		std::wstring m_normalMapFilename;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalTexture;
		float m_texutreTiling = 1.0f;

		// Light Controls
		DirectX::SimpleMath::Vector3 m_lightDirection = DirectX::SimpleMath::Vector3(-0.5f, -1.0f, 0.5f);
		DirectX::SimpleMath::Vector3 m_diffuseColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

		bool m_isVisible = true;
		bool m_isWireFrame = true;
		bool m_needsReload = false;

	public:

		TerrainComponent(Actor* owner);

		bool Initialize(
			GameContext& gameContext,
			const wchar_t* heightMapFilename,
			const wchar_t* textureFilename = L"",
			const wchar_t* colorMapFilename = L"",
			const wchar_t* normalMapFilename = L"",
			float heightScale = 10.0f,
			float textureTiling = 1.0f
		);

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
		bool LoadRawHeightMap(const wchar_t* filename);
		bool CalculateNormals();
		bool InitializeBuffer(ID3D11Device* device);
		bool LoadColorMap(const wchar_t* filename);

	};
}