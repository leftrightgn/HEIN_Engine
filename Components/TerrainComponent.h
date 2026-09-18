#pragma once
#include "IComponent.h"
#include <string>
#include "Common/TerrainCell.h"

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
			float hasAlphaMap;
			float hasTexture2;
			float fogStart;
			float fogEnd;
			float padding1;
			float padding2;
			DirectX::SimpleMath::Vector4 fogColor;
		};

	public:
		// Data
		struct HeightMapType
		{
			float x, y, z;
			float nx, ny, nz;
			float tx, ty, tz; // tangent
			float bx, by, bz; // binormal
			float r, g, b;
		};

	private:
		int m_terrainWidth;
		int m_terrainHeight;
		float m_heightScale;

		std::wstring m_heightMapFilename;

		std::vector<HeightMapType> m_heightMap;

		std::vector<std::unique_ptr<TerrainCell>> m_cells;
		
		// Custom Shader Objects;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampleState;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_matrixBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightBuffer;

		int m_vertexCount;

		std::wstring m_textureFilename;

		std::wstring m_colorMapFilename;

		std::wstring m_normalMapFilename;

		std::wstring m_alphaMapFilename;

		std::wstring m_texture2Filename;
	
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalTexture;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_alphaTexture;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture2;


		float m_texutreTiling = 1.0f;

		// Light Controls
		DirectX::SimpleMath::Vector3 m_lightDirection = DirectX::SimpleMath::Vector3(-0.5f, -1.0f, 0.5f);
		DirectX::SimpleMath::Vector3 m_diffuseColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

		// Fog Controls
		float m_fogStart = 100.0f;
		float m_fogEnd = 800.0f;
		DirectX::SimpleMath::Vector4 m_fogColor = DirectX::SimpleMath::Vector4(0.5f, 0.6f, 0.7f, 1.0f);

		bool m_isVisible = true;
		bool m_isWireFrame = false;
		bool m_needsReload = false;

		// Cell Visualization & Culling Controls
		bool m_showCellBounds = false;
		bool m_enableFrustumCulling = true;
		bool m_freezeFrustum = false;
		DirectX::BoundingFrustum m_frozenFrustum;
		int  m_debugSingleCell = -1; // -1 = All cells, >= 0 = isolate a specific cell
		bool m_debugCellColors = false;
		int  m_renderedCellCount = 0;

	public:

		TerrainComponent(Actor* owner);

		bool Initialize(
			GameContext& gameContext,
			const wchar_t* heightMapFilename,
			const wchar_t* textureFilename = L"",
			const wchar_t* texture2Filename = L"",
			const wchar_t* alphaMapFilename = L"",
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

		int GetTerrainWidth() const { return m_terrainWidth; }
		int GetTerrainHeight() const { return m_terrainHeight; }
		float GetHeightScale() const { return m_heightScale; }
		const std::vector<HeightMapType>& GetHeightMap() const { return m_heightMap; }

	private:

		bool LoadHeightMap(const wchar_t* filename);
		bool LoadRawHeightMap(const wchar_t* filename);
		bool CalculateNormals();
		bool InitializeBuffer(ID3D11Device* device);
		bool LoadColorMap(const wchar_t* filename);

	};
}