#pragma once
#include "IComponent.h"
#include "Common/TerrainCell.h"
#include <string>

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
			DirectX::XMMATRIX lightViewProj;
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
		static constexpr int QUADS_PER_CELL = 32;
		static constexpr int INDICES_PER_QUAD = 6;
		static constexpr int ISOLATE_ALL_CELLS = -1;

		static constexpr float MAX_UINT8_HEIGHT = 255.0f;
		static constexpr float MAX_UINT16_HEIGHT = 65535.0f;
		static constexpr int BITS_PER_BYTE = 8;
		static constexpr int DEFAULT_BMP_BYTES_PER_PIXEL = 3;
		static constexpr int BMP_ROW_ALIGNMENT = 4;
		static constexpr int RAW16_BYTES_PER_PIXEL = 2;

		static constexpr float DEFAULT_HEIGHT_SCALE = 10.0f;
		static constexpr float MIN_HEIGHT_SCALE = 1.0f;
		static constexpr float MAX_HEIGHT_SCALE = 100.0f;
		static constexpr float DEFAULT_TEXTURE_TILING = 1.0f;
		static constexpr float MIN_TEXTURE_TILING = 1.0f;
		static constexpr float MAX_TEXTURE_TILING = 128.0f;
		static constexpr float DRAG_SPEED_SCALE = 0.5f;
		static constexpr float DRAG_SPEED_LIGHT_DIR = 0.05f;

		static constexpr UINT DEFAULT_MAX_ANISOTROPY = 16;
		static constexpr float DEFAULT_MIN_LOD = 0.0f;

		static constexpr float DISABLED_FOG_START = 100000.0f;
		static constexpr float DISABLED_FOG_END = 200000.0f;

		static constexpr float DEFAULT_VIEWPORT_WIDTH = 1280.0f;
		static constexpr float DEFAULT_VIEWPORT_HEIGHT = 720.0f;
		static constexpr float DEFAULT_CAMERA_FOV = DirectX::XM_PIDIV4;
		static constexpr float CULLING_NEAR_PLANE = 0.1f;
		static constexpr float CULLING_FAR_PLANE = 5000.0f;
		static constexpr float MATRIX_DETERMINANT_EPSILON = 1e-6f;
		static constexpr float NORMAL_DELTA_EPSILON = 0.0001f;
		static constexpr int FRUSTUM_PLANE_COUNT = 6;
		static constexpr int BOX_CORNER_COUNT = 8;

		// Data - Streamlined for collision (16 bytes vs original 60 bytes)
		struct HeightMapType
		{
			float y;
			float nx, ny, nz;
		};

	private:
		int m_terrainWidth;
		int m_terrainHeight;
		float m_heightScale;

		std::wstring m_heightMapFilename;

		std::vector<HeightMapType> m_heightMap;
		std::vector<DirectX::SimpleMath::Vector3> m_colorMap;

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


		float m_texutreTiling = DEFAULT_TEXTURE_TILING;

		// Light Controls
		DirectX::SimpleMath::Vector3 m_lightDirection = DirectX::SimpleMath::Vector3(-0.5f, -1.0f, 0.5f);
		DirectX::SimpleMath::Vector3 m_diffuseColor = DirectX::SimpleMath::Vector3(1.0f, 1.0f, 1.0f);

		// Fog controls are now separated into FogComponent

		bool m_isVisible = true;
		bool m_castShadows = true;
		bool m_isWireFrame = false;
		bool m_needsReload = false;

		// Cell Visualization & Culling Controls
		bool m_showCellBounds = false;
		bool m_enableFrustumCulling = true;
		bool m_freezeFrustum = false;
		DirectX::BoundingFrustum m_frozenFrustum;
		int  m_debugSingleCell = ISOLATE_ALL_CELLS; // -1 = All cells, >= 0 = isolate a specific cell
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
			float heightScale = DEFAULT_HEIGHT_SCALE,
			float textureTiling = DEFAULT_TEXTURE_TILING
		);

		void Update(float /*deltaTime*/) override {}

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

		void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }
		bool CastsShadows() const { return m_castShadows; }

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