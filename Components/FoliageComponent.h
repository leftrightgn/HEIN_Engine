#pragma once
#include "IComponent.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <Common/json.hpp>

namespace HEIN
{
	class TerrainComponent;

	// ==================================================================================
	// ENUMERATIONS & DATA STRUCTURES
	// ==================================================================================

	/// <summary>
	/// Foliage species/type enumeration used for indexing into arrays and determining shader logic.
	/// </summary>
	enum class FoliageType : uint32_t
	{
		Grass = 0,
		Flower = 1,
		Tree = 2,
		Count = 3
	};

	/// <summary>
	/// Ultra-compact foliage instance representation (24 bytes per instance).
	/// Maximizes memory efficiency so tens of thousands of instances occupy minimal memory footprint.
	/// </summary>
	struct FoliageInstanceData
	{
		DirectX::SimpleMath::Vector3 worldPos; // 12 bytes: World-space position
		float rotation;                        // 4 bytes: Yaw angle in radians for variation
		float scale;                           // 4 bytes: Uniform scale multiplier
		uint32_t type;                         // 4 bytes: Identifier (0 = Grass, 1 = Flower, 2 = Tree)
	};

	/// <summary>
	/// Vertex structure for foliage cards and procedural meshes.
	/// Passed to the GPU via the static mesh buffer (Slot 0).
	/// </summary>
	struct FoliageVertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 texCoord;
		float windWeight; // 0.0 at base/root (stiff), 1.0 at blade/leaf tip (full sway)
		DirectX::XMFLOAT4 color;
	};

	/// <summary>
	/// Spatial partition cell. Contains instances localized to a specific chunk of the world.
	/// Utilized to perform rapid frustum culling, sending only visible instances to the GPU.
	/// </summary>
	struct FoliageCell
	{
		// The world-space bounding box used for camera frustum culling tests.
		DirectX::BoundingBox boundingBox;

		// Instance data arrays isolated to this specific spatial grid cell.
		std::vector<FoliageInstanceData> instances[static_cast<size_t>(FoliageType::Count)];

		// Dedicated dynamic GPU buffers for streaming this cell's data to the graphics card.
		Microsoft::WRL::ComPtr<ID3D11Buffer> instanceBuffers[static_cast<size_t>(FoliageType::Count)];
		uint32_t instanceBufferCapacities[static_cast<size_t>(FoliageType::Count)] = {};

		// Flags indicating whether the CPU instance array has changed and requires a GPU buffer update.
		bool bufferDirty[static_cast<size_t>(FoliageType::Count)] = { true, true, true };

		// Default constructor initializes an "inside-out" box so the first added instance expands it correctly.
		FoliageCell()
		{
			boundingBox.Center = DirectX::SimpleMath::Vector3::Zero;
			boundingBox.Extents = DirectX::SimpleMath::Vector3(-1.0f, -1.0f, -1.0f);
		}
	};

	// ==================================================================================
	// FOLIAGE COMPONENT
	// ==================================================================================

	/// <summary>
	/// Memory-efficient Instanced Foliage System with real-time Editor Brush,
	/// stylized wind swaying via scrolling noise, terrain color map integration,
	/// and spatial grid partitioning for high-performance frustum culling.
	/// </summary>
	class FoliageComponent : public IComponent
	{
	public:
		enum class BrushMode
		{
			Paint = 0,
			Erase = 1
		};

		// Modifiable grass shape parameters exported to the shader.
		float m_maxGrassHeight = 3.0f;
		float m_maxGrassWidth = 0.9f;
		float m_tilt = 8.0f;
		float m_bend = 0.45f;

	private:
		// Shaders & D3D11 Pipeline Resources
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_perFrameBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_settingsBuffer;

		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerWrap;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerClamp;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSampler;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;

		// Shared static meshes (geometry) per foliage type. Reused by all instances.
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_meshVB[static_cast<size_t>(FoliageType::Count)];
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_meshIB[static_cast<size_t>(FoliageType::Count)];
		uint32_t m_vertexCount[static_cast<size_t>(FoliageType::Count)] = {};
		uint32_t m_indexCount[static_cast<size_t>(FoliageType::Count)] = {};

		// Spatial Grid Map mapping 64-bit coordinate hashes to specific foliage cells.
		std::unordered_map<uint64_t, std::unique_ptr<FoliageCell>> m_cells;

		// Temporary holding array utilized during JSON deserialization before the Terrain is fully loaded.
		std::vector<FoliageInstanceData> m_tempLoadedInstances;

		// Textures & SRVs
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_foliageSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_noiseSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_colorMapSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_defaultAtlasSRV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_grassMaskSRV;

		std::vector<unsigned char> m_grassMaskData;
		int m_maskWidth = 0;
		int m_maskHeight = 0;
		bool m_grassGenerated = false;

		std::wstring m_foliageTexturePath = L"Dual/Resources/Textures/grass_diffuse.dds";
		std::wstring m_noiseTexturePath = L"Dual/Resources/Textures/waternoise.dds";
		std::wstring m_colorMapTexturePath = L"Dual/Resources/Textures/colormap.bmp";
		std::wstring m_grassMaskTexturePath = L"Dual/Resources/Textures/grass_mask.dds";

		// Wind & Animation Controls (Stylized Wind Breezing via scrolling noise)
		DirectX::SimpleMath::Vector2 m_windDirection = DirectX::SimpleMath::Vector2(1.0f, 0.35f);
		float m_windSpeed = 1.5f;
		float m_windStrength = 0.35f;
		float m_noiseScale = 0.05f;
		float m_timeAccumulator = 0.0f;

		// Color & Stylization Palette
		DirectX::SimpleMath::Vector4 m_grassRootColor = DirectX::SimpleMath::Vector4(0.08f, 0.22f, 0.06f, 1.0f);
		DirectX::SimpleMath::Vector4 m_grassTipColor = DirectX::SimpleMath::Vector4(0.55f, 0.82f, 0.20f, 1.0f);
		DirectX::SimpleMath::Vector4 m_flowerColor = DirectX::SimpleMath::Vector4(0.95f, 0.28f, 0.38f, 1.0f);
		DirectX::SimpleMath::Vector4 m_leafColor = DirectX::SimpleMath::Vector4(0.18f, 0.58f, 0.15f, 1.0f);
		DirectX::SimpleMath::Vector4 m_barkColor = DirectX::SimpleMath::Vector4(0.35f, 0.22f, 0.12f, 1.0f);
		float m_alphaCutoff = 0.35f;
		bool m_useColorMap = true;
		bool m_useTexture = true;

		// Editor Brush Settings
		bool m_brushActive = true;
		BrushMode m_brushMode = BrushMode::Paint;
		int m_selectedTypeIndex = 0; // 0: Grass, 1: Flower, 2: Tree
		float m_brushRadius = 5.0f;
		int m_brushDensity = 12;
		float m_minSpacing = 0.35f;
		float m_minScale = 0.8f;
		float m_maxScale = 1.25f;
		bool m_randomYaw = true;
		bool m_alignToNormal = true;
		bool m_eraseSelectedOnly = false;
		float m_paintCooldown = 0.0f;

		bool m_enableFrustumCulling = true;
		bool m_freezeFrustum = false;
		DirectX::BoundingFrustum m_frozenFrustum;

		// Cached camera & viewport from Draw to calculate raycasting in editor
		DirectX::SimpleMath::Matrix m_cachedView = DirectX::SimpleMath::Matrix::Identity;
		DirectX::SimpleMath::Matrix m_cachedProj = DirectX::SimpleMath::Matrix::Identity;
		D3D11_VIEWPORT m_cachedViewport = {};
		bool m_hasTerrainHit = false;
		DirectX::SimpleMath::Vector3 m_currentHitPos = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector3 m_currentHitNormal = DirectX::SimpleMath::Vector3::Up;

		bool m_isInitialized = false;

	public:
		FoliageComponent(Actor* owner);
		~FoliageComponent() = default;

		std::string GetComponentName() const override { return "FoliageComponent"; }

		void Start() override;
		void Update(float deltaTime) override;
		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		void OnInspectorGUI(GameContext& gameContext) override;
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;

		// Manual Placement & Management API
		void AddInstance(FoliageType type, const DirectX::SimpleMath::Vector3& position, float rotation, float scale, TerrainComponent* terrain);
		void ClearType(FoliageType type);
		void ClearAll();
		size_t GetInstanceCount(FoliageType type) const;
		size_t GetTotalInstanceCount() const;

	private:
		bool InitializeResources(ID3D11Device* device);
		void BuildFoliageMeshes(ID3D11Device* device);
		void BuildDefaultAtlas(ID3D11Device* device);
		bool LoadTexture(ID3D11Device* device, const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv);

		// Spatial Grid Processing Methods
		uint64_t GetTerrainAlignedCellKey(const DirectX::SimpleMath::Vector3& worldPos, TerrainComponent* terrain) const;
		void UpdateCellBuffers(ID3D11Device* device, ID3D11DeviceContext* context, FoliageCell* cell);

		// Terrain Interaction Methods
		TerrainComponent* FindTerrain(GameContext& gameContext);
		bool RaycastTerrain(
			TerrainComponent* terrain,
			const DirectX::SimpleMath::Vector3& rayOrigin,
			const DirectX::SimpleMath::Vector3& rayDir,
			DirectX::SimpleMath::Vector3& outHitPos,
			DirectX::SimpleMath::Vector3& outHitNormal
		);
		bool GetLocalTerrainHeightAndNormal(
			TerrainComponent* terrain,
			float localX,
			float localZ,
			float& outLocalHeight,
			DirectX::SimpleMath::Vector3& outLocalNormal
		);
		bool SampleTerrainHeightAndNormal(
			TerrainComponent* terrain,
			float worldX,
			float worldZ,
			float& outHeight,
			DirectX::SimpleMath::Vector3& outNormal
		);

		// Editor Painting Methods
		void HandleEditorPainting(GameContext& gameContext, TerrainComponent* terrain, const DirectX::SimpleMath::Vector3& hitPos);
		void PaintInstances(TerrainComponent* terrain, const DirectX::SimpleMath::Vector3& center);
		void EraseInstances(const DirectX::SimpleMath::Vector3& center);
	};
}