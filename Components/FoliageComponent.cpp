#include "pch.h"
#include "FoliageComponent.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/TransformComponent.h"
#include "Components/TerrainComponent.h"
#include "Framework/GameContext.h"
#include "Common/ShadowSystem.h"
#include "Camera/CameraController.h"
#include "DebugingTools/DebugRenderer.h"
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <d3dcompiler.h>
#include <ImGui/imgui.h>
#include <cmath>
#include <random>
#include <algorithm>

namespace HEIN
{
	struct CBPerFrame
	{
		DirectX::XMMATRIX worldMatrix;
		DirectX::XMMATRIX viewMatrix;
		DirectX::XMMATRIX projectionMatrix;
		DirectX::XMMATRIX lightViewProj;
		DirectX::SimpleMath::Vector4 cameraPosition;
		float time;
		float windSpeed;
		float windStrength;
		float noiseScale;
		DirectX::SimpleMath::Vector2 windDirection;
		float alphaCutoff;
		float terrainWidth;
		float terrainHeight;
		DirectX::SimpleMath::Vector3 paddingPerFrame;
	};

	struct CBFoliageSettings
	{
		DirectX::SimpleMath::Vector4 grassRootColor;
		DirectX::SimpleMath::Vector4 grassTipColor;
		DirectX::SimpleMath::Vector4 flowerColor;
		DirectX::SimpleMath::Vector4 leafColor;
		DirectX::SimpleMath::Vector4 barkColor;
		DirectX::SimpleMath::Vector4 lightDirection;
		DirectX::SimpleMath::Vector4 lightColor;
		DirectX::SimpleMath::Vector4 ambientColor;
		DirectX::SimpleMath::Vector4 flags; // x: useColorMap, y: useTexture, z: hasShadows, w: unused
	};

	FoliageComponent::FoliageComponent(Actor* owner)
		: IComponent(owner)
	{
	}

	void FoliageComponent::Start()
	{
		IComponent::Start();
	}

	void FoliageComponent::Update(float deltaTime)
	{
		m_timeAccumulator += deltaTime;

		if (m_paintCooldown > 0.0f)
		{
			m_paintCooldown -= deltaTime;
		}
	}

	void FoliageComponent::AddInstance(FoliageType type, const DirectX::SimpleMath::Vector3& position, float rotation, float scale)
	{
		size_t typeIdx = static_cast<size_t>(type);
		if (typeIdx >= static_cast<size_t>(FoliageType::Count)) return;

		FoliageInstanceData inst;
		inst.worldPos = position;
		inst.rotation = rotation;
		inst.scale = scale;
		inst.type = static_cast<uint32_t>(type);

		m_instances[typeIdx].push_back(inst);
		m_bufferDirty[typeIdx] = true;
	}

	void FoliageComponent::ClearType(FoliageType type)
	{
		size_t typeIdx = static_cast<size_t>(type);
		if (typeIdx < static_cast<size_t>(FoliageType::Count))
		{
			m_instances[typeIdx].clear();
			m_bufferDirty[typeIdx] = true;
		}
	}

	void FoliageComponent::ClearAll()
	{
		for (size_t i = 0; i < static_cast<size_t>(FoliageType::Count); ++i)
		{
			m_instances[i].clear();
			m_bufferDirty[i] = true;
		}
	}

	size_t FoliageComponent::GetInstanceCount(FoliageType type) const
	{
		size_t typeIdx = static_cast<size_t>(type);
		if (typeIdx < static_cast<size_t>(FoliageType::Count))
		{
			return m_instances[typeIdx].size();
		}
		return 0;
	}

	size_t FoliageComponent::GetTotalInstanceCount() const
	{
		size_t total = 0;
		for (size_t i = 0; i < static_cast<size_t>(FoliageType::Count); ++i)
		{
			total += m_instances[i].size();
		}
		return total;
	}

	bool FoliageComponent::InitializeResources(ID3D11Device* device)
	{
		if (m_isInitialized) return true;

		// 1. Compile Shaders
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
		HRESULT hr = D3DCompileFromFile(
			L"../External/Engine/Shaders/Foliage_VS.hlsl",
			nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
			&vsBlob, &errorBlob
		);
		if (FAILED(hr))
		{
			hr = D3DCompileFromFile(
				L"External/Engine/Shaders/Foliage_VS.hlsl",
				nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				"main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
				&vsBlob, &errorBlob
			);
		}
		if (FAILED(hr))
		{
			if (errorBlob) OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
			return false;
		}

		hr = D3DCompileFromFile(
			L"../External/Engine/Shaders/Foliage_PS.hlsl",
			nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
			&psBlob, &errorBlob
		);
		if (FAILED(hr))
		{
			hr = D3DCompileFromFile(
				L"External/Engine/Shaders/Foliage_PS.hlsl",
				nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				"main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
				&psBlob, &errorBlob
			);
		}
		if (FAILED(hr))
		{
			if (errorBlob) OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
			return false;
		}

		DX::ThrowIfFailed(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.ReleaseAndGetAddressOf()));
		DX::ThrowIfFailed(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.ReleaseAndGetAddressOf()));

		// 2. Input Layout matching FoliageVertex (Stream 0) and FoliageInstanceData (Stream 1)
		D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
		{
			// Slot 0: Per-vertex shared mesh data
			{ "POSITION",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(FoliageVertex, position),   D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(FoliageVertex, normal),     D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(FoliageVertex, texCoord),   D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32_FLOAT,          0, offsetof(FoliageVertex, windWeight), D3D11_INPUT_PER_VERTEX_DATA,   0 },
			{ "COLOR",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FoliageVertex, color),      D3D11_INPUT_PER_VERTEX_DATA,   0 },

			// Slot 1: Per-instance dynamic stream data
			{ "INST_POS",    0, DXGI_FORMAT_R32G32B32_FLOAT,    1, offsetof(FoliageInstanceData, worldPos), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_ROT",    0, DXGI_FORMAT_R32_FLOAT,          1, offsetof(FoliageInstanceData, rotation), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_SCALE",  0, DXGI_FORMAT_R32_FLOAT,          1, offsetof(FoliageInstanceData, scale),    D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "INST_TYPE",   0, DXGI_FORMAT_R32_UINT,           1, offsetof(FoliageInstanceData, type),     D3D11_INPUT_PER_INSTANCE_DATA, 1 }
		};

		DX::ThrowIfFailed(device->CreateInputLayout(layoutDesc, _countof(layoutDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout.ReleaseAndGetAddressOf()));

		// 3. Constant Buffers
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		cbDesc.ByteWidth = sizeof(CBPerFrame);
		DX::ThrowIfFailed(device->CreateBuffer(&cbDesc, nullptr, m_perFrameBuffer.ReleaseAndGetAddressOf()));

		cbDesc.ByteWidth = sizeof(CBFoliageSettings);
		DX::ThrowIfFailed(device->CreateBuffer(&cbDesc, nullptr, m_settingsBuffer.ReleaseAndGetAddressOf()));

		// 4. Samplers
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		DX::ThrowIfFailed(device->CreateSamplerState(&sampDesc, m_samplerWrap.ReleaseAndGetAddressOf()));

		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		DX::ThrowIfFailed(device->CreateSamplerState(&sampDesc, m_samplerClamp.ReleaseAndGetAddressOf()));

		D3D11_SAMPLER_DESC shadowSampDesc = {};
		shadowSampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		shadowSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		shadowSampDesc.BorderColor[0] = 1.0f;
		shadowSampDesc.BorderColor[1] = 1.0f;
		shadowSampDesc.BorderColor[2] = 1.0f;
		shadowSampDesc.BorderColor[3] = 1.0f;
		shadowSampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		DX::ThrowIfFailed(device->CreateSamplerState(&shadowSampDesc, m_shadowSampler.ReleaseAndGetAddressOf()));

		// 5. Rasterizer State (Cull None for double-sided foliage leaves/cards)
		D3D11_RASTERIZER_DESC rastDesc = {};
		rastDesc.FillMode = D3D11_FILL_SOLID;
		rastDesc.CullMode = D3D11_CULL_NONE;
		rastDesc.FrontCounterClockwise = FALSE;
		rastDesc.DepthClipEnable = TRUE;
		DX::ThrowIfFailed(device->CreateRasterizerState(&rastDesc, m_rasterizerState.ReleaseAndGetAddressOf()));

		// 6. Depth Stencil State: solid depth writes enabled to eliminate transparency sorting glitches
		D3D11_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = TRUE;
		depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		DX::ThrowIfFailed(device->CreateDepthStencilState(&depthDesc, m_depthStencilState.ReleaseAndGetAddressOf()));

		// 7. Build Meshes & Default Fallback Atlas
		BuildFoliageMeshes(device);
		BuildDefaultAtlas(device);

		// 8. Load Textures
		LoadTexture(device, m_foliageTexturePath, m_foliageSRV);
		LoadTexture(device, m_noiseTexturePath, m_noiseSRV);
		LoadTexture(device, m_colorMapTexturePath, m_colorMapSRV);

		m_isInitialized = true;
		return true;
	}

	void FoliageComponent::BuildFoliageMeshes(ID3D11Device* device)
	{
		// ---------------------------------------------------------
		// Type 0: Grass Mesh (Procedual Blade Strip)
		// ---------------------------------------------------------
		{
			std::vector<FoliageVertex> vertices;
			std::vector<uint32_t> indices;

			const int numSegments = 5;
			const int numRows = numSegments + 1;

			for (int i = 0; i < numRows; ++i)
			{
				float v = static_cast<float>(i) / static_cast<float>(numSegments);
				float yCoord = 1.0f - v; // Texture V coordinate (1.0 at base, 0.0 at tip)

				uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

				// position.x holds the 'isLeft' multiplier (-1.0 for left, 1.0 for right)
				// windWeight holds 'v' (0.0 at base, 1.0 at tip)
				FoliageVertex vLeft{ { -1.0f, 0.0f, 0.0f }, { 0, 1, 0 }, { 0.0f, yCoord }, v, { 1, 1, 1, 1 } };
				FoliageVertex vRight{ {  1.0f, 0.0f, 0.0f }, { 0, 1, 0 }, { 1.0f, yCoord }, v, { 1, 1, 1, 1 } };

				vertices.push_back(vLeft);
				vertices.push_back(vRight);

				if (i < numSegments)
				{
					indices.push_back(baseIdx + 0);
					indices.push_back(baseIdx + 2);
					indices.push_back(baseIdx + 1);

					indices.push_back(baseIdx + 1);
					indices.push_back(baseIdx + 2);
					indices.push_back(baseIdx + 3);
				}
			}

			m_vertexCount[0] = static_cast<uint32_t>(vertices.size());
			m_indexCount[0] = static_cast<uint32_t>(indices.size());

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_IMMUTABLE;
			bd.ByteWidth = sizeof(FoliageVertex) * m_vertexCount[0];
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
			device->CreateBuffer(&bd, &initData, m_meshVB[0].ReleaseAndGetAddressOf());

			bd.ByteWidth = sizeof(uint32_t) * m_indexCount[0];
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			initData.pSysMem = indices.data();
			device->CreateBuffer(&bd, &initData, m_meshIB[0].ReleaseAndGetAddressOf());
		}

		// ---------------------------------------------------------
		// Type 1: Flower Mesh (Stem quads + Blossom crossed cards)
		// ---------------------------------------------------------
		{
			std::vector<FoliageVertex> vertices;
			std::vector<uint32_t> indices;

			// Stem: 2 crossed quads
			for (int p = 0; p < 2; ++p)
			{
				float angle = (static_cast<float>(p) / 2.0f) * DirectX::XM_PI;
				float dx = (0.15f * 0.5f) * std::cos(angle);
				float dz = (0.15f * 0.5f) * std::sin(angle);
				uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

				FoliageVertex v0{ { -dx, 0.0f, -dz }, { 0, 1, 0 }, { 0.5f, 0.5f }, 0.0f, { 1, 1, 1, 1 } };
				FoliageVertex v1{ {  dx, 0.0f,  dz }, { 0, 1, 0 }, { 0.75f, 0.5f }, 0.0f, { 1, 1, 1, 1 } };
				FoliageVertex v2{ { -dx, 0.8f, -dz }, { 0, 1, 0 }, { 0.5f, 0.25f }, 0.7f, { 1, 1, 1, 1 } };
				FoliageVertex v3{ {  dx, 0.8f,  dz }, { 0, 1, 0 }, { 0.75f, 0.25f }, 0.7f, { 1, 1, 1, 1 } };

				vertices.push_back(v0);
				vertices.push_back(v1);
				vertices.push_back(v2);
				vertices.push_back(v3);

				indices.push_back(baseIdx + 0);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 3);
			}

			// Blossom: 2 wider crossed cards at top
			for (int p = 0; p < 2; ++p)
			{
				float angle = (static_cast<float>(p) / 2.0f) * DirectX::XM_PI + 0.25f * DirectX::XM_PI;
				float dx = (0.55f * 0.5f) * std::cos(angle);
				float dz = (0.55f * 0.5f) * std::sin(angle);
				uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

				FoliageVertex v0{ { -dx, 0.65f, -dz }, { 0, 1, 0 }, { 0.5f, 0.5f }, 0.85f, { 1, 1, 1, 1 } };
				FoliageVertex v1{ {  dx, 0.65f,  dz }, { 0, 1, 0 }, { 1.0f, 0.5f }, 0.85f, { 1, 1, 1, 1 } };
				FoliageVertex v2{ { -dx, 1.15f, -dz }, { 0, 1, 0 }, { 0.5f, 0.0f }, 1.0f,  { 1, 1, 1, 1 } };
				FoliageVertex v3{ {  dx, 1.15f,  dz }, { 0, 1, 0 }, { 1.0f, 0.0f }, 1.0f,  { 1, 1, 1, 1 } };

				vertices.push_back(v0);
				vertices.push_back(v1);
				vertices.push_back(v2);
				vertices.push_back(v3);

				indices.push_back(baseIdx + 0);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 3);
			}

			m_vertexCount[1] = static_cast<uint32_t>(vertices.size());
			m_indexCount[1] = static_cast<uint32_t>(indices.size());

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_IMMUTABLE;
			bd.ByteWidth = sizeof(FoliageVertex) * m_vertexCount[1];
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
			device->CreateBuffer(&bd, &initData, m_meshVB[1].ReleaseAndGetAddressOf());

			bd.ByteWidth = sizeof(uint32_t) * m_indexCount[1];
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			initData.pSysMem = indices.data();
			device->CreateBuffer(&bd, &initData, m_meshIB[1].ReleaseAndGetAddressOf());
		}

		// ---------------------------------------------------------
		// Type 2: Tree Mesh (Low-poly stylized trunk + 3 foliage canopy puffs)
		// ---------------------------------------------------------
		{
			std::vector<FoliageVertex> vertices;
			std::vector<uint32_t> indices;

			// Trunk: 6-sided cylinder
			const int sides = 6;
			const float rBottom = 0.28f;
			const float rTop = 0.16f;
			const float trunkHeight = 2.4f;

			for (int i = 0; i < sides; ++i)
			{
				float a0 = (static_cast<float>(i) / static_cast<float>(sides)) * DirectX::XM_2PI;
				float a1 = (static_cast<float>(i + 1) / static_cast<float>(sides)) * DirectX::XM_2PI;

				float x0b = rBottom * std::cos(a0);
				float z0b = rBottom * std::sin(a0);
				float x1b = rBottom * std::cos(a1);
				float z1b = rBottom * std::sin(a1);

				float x0t = rTop * std::cos(a0);
				float z0t = rTop * std::sin(a0);
				float x1t = rTop * std::cos(a1);
				float z1t = rTop * std::sin(a1);

				float nxa = std::cos((a0 + a1) * 0.5f);
				float nza = std::sin((a0 + a1) * 0.5f);

				uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

				FoliageVertex v0{ { x0b, 0.0f, z0b }, { nxa, 0, nza }, { 0.0f, 1.0f }, 0.0f, { 1, 1, 1, 1 } };
				FoliageVertex v1{ { x1b, 0.0f, z1b }, { nxa, 0, nza }, { 0.5f, 1.0f }, 0.0f, { 1, 1, 1, 1 } };
				FoliageVertex v2{ { x0t, trunkHeight, z0t }, { nxa, 0, nza }, { 0.0f, 0.5f }, 0.0f, { 1, 1, 1, 1 } };
				FoliageVertex v3{ { x1t, trunkHeight, z1t }, { nxa, 0, nza }, { 0.5f, 0.5f }, 0.0f, { 1, 1, 1, 1 } };

				vertices.push_back(v0);
				vertices.push_back(v1);
				vertices.push_back(v2);
				vertices.push_back(v3);

				indices.push_back(baseIdx + 0);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 3);
			}

			// Canopy: 3 intersecting large spherical foliage puff cards (Ghibli card clusters)
			const int canopyCards = 3;
			const float canopyWidth = 3.2f;
			const float canopyBottom = 1.8f;
			const float canopyTop = 4.8f;

			for (int p = 0; p < canopyCards; ++p)
			{
				float angle = (static_cast<float>(p) / static_cast<float>(canopyCards)) * DirectX::XM_PI;
				float dx = (canopyWidth * 0.5f) * std::cos(angle);
				float dz = (canopyWidth * 0.5f) * std::sin(angle);
				uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

				FoliageVertex v0{ { -dx, canopyBottom, -dz }, { 0, 1, 0 }, { 0.5f, 1.0f }, 0.4f, { 1, 1, 1, 1 } };
				FoliageVertex v1{ {  dx, canopyBottom,  dz }, { 0, 1, 0 }, { 1.0f, 1.0f }, 0.4f, { 1, 1, 1, 1 } };
				FoliageVertex v2{ { -dx, canopyTop,    -dz }, { 0, 1, 0 }, { 0.5f, 0.5f }, 0.85f, { 1, 1, 1, 1 } };
				FoliageVertex v3{ {  dx, canopyTop,     dz }, { 0, 1, 0 }, { 1.0f, 0.5f }, 0.85f, { 1, 1, 1, 1 } };

				vertices.push_back(v0);
				vertices.push_back(v1);
				vertices.push_back(v2);
				vertices.push_back(v3);

				indices.push_back(baseIdx + 0);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 1);
				indices.push_back(baseIdx + 2);
				indices.push_back(baseIdx + 3);
			}

			m_vertexCount[2] = static_cast<uint32_t>(vertices.size());
			m_indexCount[2] = static_cast<uint32_t>(indices.size());

			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_IMMUTABLE;
			bd.ByteWidth = sizeof(FoliageVertex) * m_vertexCount[2];
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			D3D11_SUBRESOURCE_DATA initData = { vertices.data(), 0, 0 };
			device->CreateBuffer(&bd, &initData, m_meshVB[2].ReleaseAndGetAddressOf());

			bd.ByteWidth = sizeof(uint32_t) * m_indexCount[2];
			bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
			initData.pSysMem = indices.data();
			device->CreateBuffer(&bd, &initData, m_meshIB[2].ReleaseAndGetAddressOf());
		}
	}

	void FoliageComponent::BuildDefaultAtlas(ID3D11Device* device)
	{
		// Synthesizes a 128x128 RGBA texture with 4 quadrants:
		// [0,0]-[64,64]: Grass blades with alpha
		// [64,0]-[128,64]: Flower blossom petals with alpha
		// [0,64]-[64,128]: Wood trunk bark
		// [64,64]-[128,128]: Leaf cluster puff with alpha
		const int size = 128;
		std::vector<uint32_t> pixels(size * size, 0);

		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				int qx = x / 64;
				int qy = y / 64;
				int lx = x % 64;
				int ly = y % 64;
				float u = static_cast<float>(lx) / 64.0f;
				float v = static_cast<float>(ly) / 64.0f;

				uint8_t r = 255, g = 255, b = 255, a = 255;

				if (qx == 0 && qy == 0) // Grass clump quadrant
				{
					// Multiple tapered blades
					float blade1 = std::abs(u - 0.25f) - (1.0f - v) * 0.12f;
					float blade2 = std::abs(u - 0.50f) - (1.0f - v) * 0.15f;
					float blade3 = std::abs(u - 0.75f) - (1.0f - v) * 0.12f;
					if (blade1 < 0.0f || blade2 < 0.0f || blade3 < 0.0f)
					{
						r = 200; g = 255; b = 180; a = 255;
					}
					else
					{
						r = 0; g = 0; b = 0; a = 0;
					}
				}
				else if (qx == 1 && qy == 0) // Flower quadrant
				{
					float dx = u - 0.5f;
					float dy = v - 0.5f;
					float dist = std::sqrt(dx * dx + dy * dy);
					if (dist < 0.42f)
					{
						r = 255; g = 220; b = 230; a = 255;
					}
					else
					{
						r = 0; g = 0; b = 0; a = 0;
					}
				}
				else if (qx == 0 && qy == 1) // Bark quadrant
				{
					r = 180; g = 140; b = 100; a = 255;
				}
				else // Leaf canopy puff quadrant
				{
					float dx = u - 0.5f;
					float dy = v - 0.5f;
					float dist = std::sqrt(dx * dx + dy * dy);
					if (dist < 0.45f)
					{
						r = 210; g = 255; b = 190; a = 255;
					}
					else
					{
						r = 0; g = 0; b = 0; a = 0;
					}
				}

				pixels[y * size + x] = (a << 24) | (b << 16) | (g << 8) | r;
			}
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = size;
		texDesc.Height = size;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA subData = {};
		subData.pSysMem = pixels.data();
		subData.SysMemPitch = size * sizeof(uint32_t);

		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		DX::ThrowIfFailed(device->CreateTexture2D(&texDesc, &subData, tex.GetAddressOf()));
		DX::ThrowIfFailed(device->CreateShaderResourceView(tex.Get(), nullptr, m_defaultAtlasSRV.ReleaseAndGetAddressOf()));
	}

	bool FoliageComponent::LoadTexture(ID3D11Device* device, const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv)
	{
		if (path.empty()) return false;

		std::vector<std::wstring> candidates = {
			path,
			L"../" + path,
			L"../../" + path,
			L"../Dual/" + path,
			L"Dual/" + path,
			L"Resources/Textures/" + path.substr(path.find_last_of(L"/\\") + 1),
			L"../Dual/Resources/Textures/" + path.substr(path.find_last_of(L"/\\") + 1)
		};

		for (const auto& testPath : candidates)
		{
			HRESULT hr = DirectX::CreateDDSTextureFromFile(device, testPath.c_str(), nullptr, outSrv.ReleaseAndGetAddressOf());
			if (SUCCEEDED(hr)) return true;

			hr = DirectX::CreateWICTextureFromFile(device, testPath.c_str(), nullptr, outSrv.ReleaseAndGetAddressOf());
			if (SUCCEEDED(hr)) return true;
		}

		return false;
	}

	void FoliageComponent::UpdateInstanceBuffers(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		for (size_t i = 0; i < static_cast<size_t>(FoliageType::Count); ++i)
		{
			if (!m_bufferDirty[i]) continue;
			m_bufferDirty[i] = false;

			const size_t count = m_instances[i].size();
			if (count == 0) continue;

			if (m_instanceBuffer[i] == nullptr || m_instanceBufferCapacity[i] < count)
			{
				m_instanceBufferCapacity[i] = static_cast<uint32_t>(std::max(count, size_t(64)) * 3 / 2);

				D3D11_BUFFER_DESC bd = {};
				bd.Usage = D3D11_USAGE_DYNAMIC;
				bd.ByteWidth = static_cast<UINT>(sizeof(FoliageInstanceData) * m_instanceBufferCapacity[i]);
				bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

				D3D11_SUBRESOURCE_DATA initData = { m_instances[i].data(), 0, 0 };
				DX::ThrowIfFailed(device->CreateBuffer(&bd, &initData, m_instanceBuffer[i].ReleaseAndGetAddressOf()));
			}
			else
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				HRESULT hr = context->Map(m_instanceBuffer[i].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
				if (SUCCEEDED(hr))
				{
					memcpy(mapped.pData, m_instances[i].data(), sizeof(FoliageInstanceData) * count);
					context->Unmap(m_instanceBuffer[i].Get(), 0);
				}
			}
		}
	}

	void FoliageComponent::Draw(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& world,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		m_cachedView = view;
		m_cachedProj = proj;

		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

		UINT numVp = 1;
		context->RSGetViewports(&numVp, &m_cachedViewport);

		if (!InitializeResources(device)) return;

		UpdateInstanceBuffers(device, context);

		if (GetTotalInstanceCount() == 0) return;

		// 1. Update Per-Frame Constant Buffer
		TerrainComponent* terrain = FindTerrain(gameContext);
		float terW = terrain ? static_cast<float>(terrain->GetTerrainWidth()) : 256.0f;
		float terH = terrain ? static_cast<float>(terrain->GetTerrainHeight()) : 256.0f;

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(context->Map(m_perFrameBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			CBPerFrame* cb = static_cast<CBPerFrame*>(mapped.pData);
			cb->worldMatrix = DirectX::XMMatrixTranspose(world);
			cb->viewMatrix = DirectX::XMMatrixTranspose(view);
			cb->projectionMatrix = DirectX::XMMatrixTranspose(proj);

			DirectX::SimpleMath::Matrix lightViewProj = DirectX::SimpleMath::Matrix::Identity;
			if (gameContext.shadowSystem != nullptr)
			{
				lightViewProj = gameContext.shadowSystem->GetLightViewProj();
			}
			cb->lightViewProj = DirectX::XMMatrixTranspose(lightViewProj);

			cb->cameraPosition = DirectX::SimpleMath::Vector4(
				gameContext.mainCamera ? gameContext.mainCamera->GetPosition().x : 0.0f,
				gameContext.mainCamera ? gameContext.mainCamera->GetPosition().y : 0.0f,
				gameContext.mainCamera ? gameContext.mainCamera->GetPosition().z : 0.0f,
				1.0f
			);
			cb->time = m_timeAccumulator;
			cb->windSpeed = m_windSpeed;
			cb->windStrength = m_windStrength;
			cb->noiseScale = m_noiseScale;
			cb->windDirection = m_windDirection;
			cb->alphaCutoff = m_alphaCutoff;
			cb->terrainWidth = terW;
			cb->terrainHeight = terH;
			cb->paddingPerFrame = DirectX::SimpleMath::Vector3::Zero;

			context->Unmap(m_perFrameBuffer.Get(), 0);
		}

		// 2. Update Settings Constant Buffer
		if (SUCCEEDED(context->Map(m_settingsBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			CBFoliageSettings* cb = static_cast<CBFoliageSettings*>(mapped.pData);
			cb->grassRootColor = m_grassRootColor;
			cb->grassTipColor = m_grassTipColor;
			cb->flowerColor = m_flowerColor;
			cb->leafColor = m_leafColor;
			cb->barkColor = m_barkColor;

			DirectX::SimpleMath::Vector3 lightDir(-0.5f, -1.0f, 0.5f);
			lightDir.Normalize();
			cb->lightDirection = DirectX::SimpleMath::Vector4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
			cb->lightColor = DirectX::SimpleMath::Vector4(1.0f, 0.98f, 0.92f, 1.0f);
			cb->ambientColor = DirectX::SimpleMath::Vector4(0.35f, 0.38f, 0.40f, 1.0f);

			float hasTex = (m_useTexture && (m_foliageSRV != nullptr || m_defaultAtlasSRV != nullptr)) ? 1.0f : 0.0f;
			float hasColorMap = (m_useColorMap && m_colorMapSRV != nullptr) ? 1.0f : 0.0f;
			float hasShadow = (gameContext.shadowSystem != nullptr) ? 1.0f : 0.0f;

			cb->flags = DirectX::SimpleMath::Vector4(hasColorMap, hasTex, hasShadow, 0.0f);

			context->Unmap(m_settingsBuffer.Get(), 0);
		}

		// 3. Bind Pipeline State
		context->IASetInputLayout(m_inputLayout.Get());
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11Buffer* cbs[] = { m_perFrameBuffer.Get(), m_settingsBuffer.Get() };
		context->VSSetConstantBuffers(0, 2, cbs);
		context->PSSetConstantBuffers(0, 2, cbs);

		ID3D11ShaderResourceView* foliageTex = m_foliageSRV ? m_foliageSRV.Get() : m_defaultAtlasSRV.Get();
		ID3D11ShaderResourceView* noiseTex = m_noiseSRV ? m_noiseSRV.Get() : m_defaultAtlasSRV.Get();
		ID3D11ShaderResourceView* colorMapTex = m_colorMapSRV ? m_colorMapSRV.Get() : m_defaultAtlasSRV.Get();
		ID3D11ShaderResourceView* shadowTex = gameContext.shadowSystem ? gameContext.shadowSystem->GetShadowMapSRV() : nullptr;

		ID3D11ShaderResourceView* vsSrvs[] = { foliageTex, noiseTex };
		context->VSSetShaderResources(0, 2, vsSrvs);

		ID3D11ShaderResourceView* psSrvs[] = { foliageTex, noiseTex, colorMapTex, shadowTex };
		context->PSSetShaderResources(0, 4, psSrvs);

		ID3D11SamplerState* shadowSampler = (gameContext.shadowSystem && gameContext.shadowSystem->GetShadowSampler()) ?
			gameContext.shadowSystem->GetShadowSampler() : m_shadowSampler.Get();

		ID3D11SamplerState* vsSamplers[] = { m_samplerWrap.Get() };
		context->VSSetSamplers(0, 1, vsSamplers);

		ID3D11SamplerState* psSamplers[] = { m_samplerWrap.Get(), m_samplerClamp.Get(), shadowSampler };
		context->PSSetSamplers(0, 3, psSamplers);

		context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		context->RSSetState(m_rasterizerState.Get());
		context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
		context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);

		// 4. Render Each Foliage Species with a Single Instanced Draw Call
		for (size_t i = 0; i < static_cast<size_t>(FoliageType::Count); ++i)
		{
			const uint32_t instanceCount = static_cast<uint32_t>(m_instances[i].size());
			if (instanceCount == 0 || m_indexCount[i] == 0) continue;

			ID3D11Buffer* vbs[] = { m_meshVB[i].Get(), m_instanceBuffer[i].Get() };
			UINT strides[] = { sizeof(FoliageVertex), sizeof(FoliageInstanceData) };
			UINT offsets[] = { 0, 0 };

			context->IASetVertexBuffers(0, 2, vbs, strides, offsets);
			context->IASetIndexBuffer(m_meshIB[i].Get(), DXGI_FORMAT_R32_UINT, 0);

			context->DrawIndexedInstanced(m_indexCount[i], instanceCount, 0, 0, 0);
		}

		// Clean up shader resource bindings to avoid resource hazards
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		context->PSSetShaderResources(0, 4, nullSRVs);
		context->VSSetShaderResources(0, 2, nullSRVs);
	}

	TerrainComponent* FoliageComponent::FindTerrain(GameContext& gameContext)
	{
		// 1. Check if owner has TerrainComponent
		if (m_owner != nullptr)
		{
			TerrainComponent* t = m_owner->GetComponent<TerrainComponent>();
			if (t != nullptr) return t;

			// 2. Check if owner is a child of the terrain actor
			if (gameContext.actorManager != nullptr && m_owner->GetParentID() != INVALID_ACTOR_ID)
			{
				Actor* parent = gameContext.actorManager->GetActor(m_owner->GetParentID());
				if (parent != nullptr)
				{
					t = parent->GetComponent<TerrainComponent>();
					if (t != nullptr) return t;
				}
			}
		}

		// 3. Fallback: Search all actors in scene
		if (gameContext.actorManager != nullptr)
		{
			for (const auto& pair : gameContext.actorManager->GetAllActors())
			{
				if (pair.second != nullptr)
				{
					TerrainComponent* t = pair.second->GetComponent<TerrainComponent>();
					if (t != nullptr) return t;
				}
			}
		}

		return nullptr;
	}

	bool FoliageComponent::GetLocalTerrainHeightAndNormal(
		TerrainComponent* terrain,
		float localX,
		float localZ,
		float& outLocalHeight,
		DirectX::SimpleMath::Vector3& outLocalNormal
	)
	{
		if (terrain == nullptr) return false;

		int width = terrain->GetTerrainWidth();
		int height = terrain->GetTerrainHeight();
		float scale = terrain->GetHeightScale();
		const auto& heightMap = terrain->GetHeightMap();

		if (width <= 1 || height <= 1 || heightMap.empty()) return false;

		float halfWidth = static_cast<float>(width) / 2.0f;
		float halfDepth = static_cast<float>(height) / 2.0f;

		float gridX = localX + halfWidth;
		float gridZ = localZ + halfDepth;

		if (gridX < 0.0f || gridZ < 0.0f || gridX >= (width - 1.001f) || gridZ >= (height - 1.001f))
		{
			return false;
		}

		int col = static_cast<int>(std::floor(gridX));
		int row = static_cast<int>(std::floor(gridZ));

		float dx = gridX - static_cast<float>(col);
		float dz = gridZ - static_cast<float>(row);

		int idxBL = (row * width) + col;
		int idxBR = (row * width) + (col + 1);
		int idxTL = ((row + 1) * width) + col;
		int idxTR = ((row + 1) * width) + (col + 1);

		float h00 = heightMap[idxBL].y * scale;
		float h10 = heightMap[idxBR].y * scale;
		float h01 = heightMap[idxTL].y * scale;
		float h11 = heightMap[idxTR].y * scale;

		DirectX::SimpleMath::Vector3 n00(heightMap[idxBL].nx, heightMap[idxBL].ny, heightMap[idxBL].nz);
		DirectX::SimpleMath::Vector3 n10(heightMap[idxBR].nx, heightMap[idxBR].ny, heightMap[idxBR].nz);
		DirectX::SimpleMath::Vector3 n01(heightMap[idxTL].nx, heightMap[idxTL].ny, heightMap[idxTL].nz);
		DirectX::SimpleMath::Vector3 n11(heightMap[idxTR].nx, heightMap[idxTR].ny, heightMap[idxTR].nz);

		if (dx + dz <= 1.0f)
		{
			outLocalHeight = h00 + (h10 - h00) * dx + (h01 - h00) * dz;
			outLocalNormal = n00 + (n10 - n00) * dx + (n01 - n00) * dz;
		}
		else
		{
			outLocalHeight = h11 - (h11 - h10) * (1.0f - dz) - (h11 - h01) * (1.0f - dx);
			outLocalNormal = n11 - (n11 - n10) * (1.0f - dz) - (n11 - n01) * (1.0f - dx);
		}
		outLocalNormal.Normalize();

		return true;
	}

	bool FoliageComponent::SampleTerrainHeightAndNormal(
		TerrainComponent* terrain,
		float worldX,
		float worldZ,
		float& outHeight,
		DirectX::SimpleMath::Vector3& outNormal
	)
	{
		if (terrain == nullptr) return false;

		TransformComponent* terrainTrans = terrain->GetOwner()->GetComponent<TransformComponent>();
		DirectX::SimpleMath::Matrix invWorld = DirectX::SimpleMath::Matrix::Identity;
		if (terrainTrans != nullptr)
		{
			invWorld = terrainTrans->GetWorldMatrix().Invert();
		}

		DirectX::SimpleMath::Vector3 localPos = DirectX::SimpleMath::Vector3::Transform(
			DirectX::SimpleMath::Vector3(worldX, 0.0f, worldZ), invWorld
		);

		float localHeight = 0.0f;
		DirectX::SimpleMath::Vector3 localNormal = DirectX::SimpleMath::Vector3::Up;
		if (!GetLocalTerrainHeightAndNormal(terrain, localPos.x, localPos.z, localHeight, localNormal))
		{
			return false;
		}

		if (terrainTrans != nullptr)
		{
			DirectX::SimpleMath::Vector3 worldPoint = DirectX::SimpleMath::Vector3::Transform(
				DirectX::SimpleMath::Vector3(localPos.x, localHeight, localPos.z),
				terrainTrans->GetWorldMatrix()
			);
			outHeight = worldPoint.y;

			outNormal = DirectX::SimpleMath::Vector3::TransformNormal(localNormal, terrainTrans->GetWorldMatrix());
			outNormal.Normalize();
		}
		else
		{
			outHeight = localHeight;
			outNormal = localNormal;
		}

		return true;
	}

	bool FoliageComponent::RaycastTerrain(
		TerrainComponent* terrain,
		const DirectX::SimpleMath::Vector3& rayOrigin,
		const DirectX::SimpleMath::Vector3& rayDir,
		DirectX::SimpleMath::Vector3& outHitPos,
		DirectX::SimpleMath::Vector3& outHitNormal
	)
	{
		if (terrain == nullptr) return false;

		TransformComponent* terrainTrans = terrain->GetOwner()->GetComponent<TransformComponent>();
		DirectX::SimpleMath::Matrix invWorld = DirectX::SimpleMath::Matrix::Identity;
		if (terrainTrans != nullptr)
		{
			invWorld = terrainTrans->GetWorldMatrix().Invert();
		}

		DirectX::SimpleMath::Vector3 localOrigin = DirectX::SimpleMath::Vector3::Transform(rayOrigin, invWorld);
		DirectX::SimpleMath::Vector3 localDir = DirectX::SimpleMath::Vector3::TransformNormal(rayDir, invWorld);
		localDir.Normalize();

		int width = terrain->GetTerrainWidth();
		int height = terrain->GetTerrainHeight();
		float scale = terrain->GetHeightScale();

		if (width <= 1 || height <= 1) return false;

		float halfW = static_cast<float>(width) / 2.0f;
		float halfH = static_cast<float>(height) / 2.0f;

		// Generous bounding box to encompass all possible terrain altitudes
		float maxBoxY = std::max(scale * 2.0f, 1000.0f);
		DirectX::BoundingBox box(
			DirectX::SimpleMath::Vector3(0.0f, maxBoxY * 0.5f, 0.0f),
			DirectX::SimpleMath::Vector3(halfW + 10.0f, maxBoxY * 0.5f + 100.0f, halfH + 10.0f)
		);

		float tDist = 0.0f;
		if (!box.Intersects(localOrigin, localDir, tDist))
		{
			return false;
		}

		float t = std::max(tDist, 0.0f);
		float maxT = t + 1500.0f;
		const float step = 0.5f;

		float prevT = t;
		bool foundIntersection = false;

		while (t < maxT)
		{
			DirectX::SimpleMath::Vector3 p = localOrigin + localDir * t;
			if (p.x >= -halfW && p.x <= halfW && p.z >= -halfH && p.z <= halfH)
			{
				float terrainLocalY = 0.0f;
				DirectX::SimpleMath::Vector3 dummyNorm;
				if (GetLocalTerrainHeightAndNormal(terrain, p.x, p.z, terrainLocalY, dummyNorm))
				{
					if (p.y <= terrainLocalY)
					{
						foundIntersection = true;
						break;
					}
				}
			}
			else
			{
				if (t > tDist + 10.0f) break;
			}

			prevT = t;
			t += step;
		}

		if (!foundIntersection) return false;

		// Binary search refinement for sub-millimeter precision
		float t0 = prevT;
		float t1 = t;
		for (int i = 0; i < 8; ++i)
		{
			float tm = (t0 + t1) * 0.5f;
			DirectX::SimpleMath::Vector3 pm = localOrigin + localDir * tm;
			float ym = 0.0f;
			DirectX::SimpleMath::Vector3 nm;
			GetLocalTerrainHeightAndNormal(terrain, pm.x, pm.z, ym, nm);

			if (pm.y <= ym)
			{
				t1 = tm;
			}
			else
			{
				t0 = tm;
			}
		}

		DirectX::SimpleMath::Vector3 hitLocal = localOrigin + localDir * ((t0 + t1) * 0.5f);
		DirectX::SimpleMath::Vector3 localNormal = DirectX::SimpleMath::Vector3::Up;
		GetLocalTerrainHeightAndNormal(terrain, hitLocal.x, hitLocal.z, hitLocal.y, localNormal);

		if (terrainTrans != nullptr)
		{
			outHitPos = DirectX::SimpleMath::Vector3::Transform(hitLocal, terrainTrans->GetWorldMatrix());
			outHitNormal = DirectX::SimpleMath::Vector3::TransformNormal(localNormal, terrainTrans->GetWorldMatrix());
			outHitNormal.Normalize();
		}
		else
		{
			outHitPos = hitLocal;
			outHitNormal = localNormal;
		}

		return true;
	}

	void FoliageComponent::PaintInstances(TerrainComponent* terrain, const DirectX::SimpleMath::Vector3& center)
	{
		static std::mt19937 rng(1337);
		std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

		size_t typeIdx = static_cast<size_t>(m_selectedTypeIndex);
		if (typeIdx >= static_cast<size_t>(FoliageType::Count)) return;

		int spawnCount = m_brushDensity;
		for (int i = 0; i < spawnCount; ++i)
		{
			float r = m_brushRadius * std::sqrt(dist01(rng));
			float theta = dist01(rng) * DirectX::XM_2PI;

			float px = center.x + r * std::cos(theta);
			float pz = center.z + r * std::sin(theta);

			// Check minimum spacing against existing instances of this type
			bool tooClose = false;
			float spacingSq = m_minSpacing * m_minSpacing;
			for (const auto& inst : m_instances[typeIdx])
			{
				float dx = inst.worldPos.x - px;
				float dz = inst.worldPos.z - pz;
				if ((dx * dx + dz * dz) < spacingSq)
				{
					tooClose = true;
					break;
				}
			}
			if (tooClose) continue;

			float py = 0.0f;
			DirectX::SimpleMath::Vector3 norm;
			if (!SampleTerrainHeightAndNormal(terrain, px, pz, py, norm)) continue;

			float rot = m_randomYaw ? (dist01(rng) * DirectX::XM_2PI) : 0.0f;
			float sc = m_minScale + dist01(rng) * (m_maxScale - m_minScale);

			AddInstance(static_cast<FoliageType>(m_selectedTypeIndex), DirectX::SimpleMath::Vector3(px, py, pz), rot, sc);
		}
	}

	void FoliageComponent::EraseInstances(const DirectX::SimpleMath::Vector3& center)
	{
		float radiusSq = m_brushRadius * m_brushRadius;

		for (size_t t = 0; t < static_cast<size_t>(FoliageType::Count); ++t)
		{
			if (m_eraseSelectedOnly && t != static_cast<size_t>(m_selectedTypeIndex))
			{
				continue;
			}

			auto& list = m_instances[t];
			size_t beforeSize = list.size();

			list.erase(
				std::remove_if(list.begin(), list.end(), [&](const FoliageInstanceData& inst) {
					float dx = inst.worldPos.x - center.x;
					float dz = inst.worldPos.z - center.z;
					return (dx * dx + dz * dz) <= radiusSq;
				}),
				list.end()
			);

			if (list.size() != beforeSize)
			{
				m_bufferDirty[t] = true;
			}
		}
	}

	void FoliageComponent::HandleEditorPainting(GameContext& gameContext, TerrainComponent* terrain, const DirectX::SimpleMath::Vector3& hitPos)
	{
		if (!m_brushActive || terrain == nullptr) return;

		bool isMouseDown = gameContext.mouseState.leftButton || ImGui::IsMouseDown(ImGuiMouseButton_Left);
		bool isMouseCaptured = ImGui::GetIO().WantCaptureMouse;

		if (isMouseDown && !isMouseCaptured)
		{
			if (m_paintCooldown <= 0.0f)
			{
				if (m_brushMode == BrushMode::Paint)
				{
					PaintInstances(terrain, hitPos);
				}
				else
				{
					EraseInstances(hitPos);
				}
				m_paintCooldown = 0.035f;
			}
		}
	}

	void FoliageComponent::OnInspectorGUI(GameContext& gameContext)
	{
		if (!ImGui::CollapsingHeader("Foliage System (Procedural & Instanced)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		TerrainComponent* terrain = FindTerrain(gameContext);
		if (terrain != nullptr)
		{
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Linked Terrain: YES (%dx%d)", terrain->GetTerrainWidth(), terrain->GetTerrainHeight());
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "WARNING: No TerrainComponent linked! Add under terrain actor.");
		}

		ImGui::Separator();

		// Large, Clear Brush Toggle Button
		if (m_brushActive)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.65f, 0.28f, 1.0f));
			if (ImGui::Button("BRUSH: ACTIVE (CLICK TO PAUSE)", ImVec2(-1, 32)))
			{
				m_brushActive = false;
			}
			ImGui::PopStyleColor(2);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.22f, 0.12f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.28f, 0.18f, 1.0f));
			if (ImGui::Button("BRUSH: PAUSED (CLICK TO ACTIVATE)", ImVec2(-1, 32)))
			{
				m_brushActive = true;
			}
			ImGui::PopStyleColor(2);
		}

		// Mode: Paint or Erase
		int modeInt = static_cast<int>(m_brushMode);
		ImGui::RadioButton("Paint", &modeInt, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Erase", &modeInt, 1);
		m_brushMode = static_cast<BrushMode>(modeInt);

		// Foliage Species Selection
		const char* foliageTypes[] = { "Grass", "Flower", "Tree" };
		ImGui::Combo("Foliage Type", &m_selectedTypeIndex, foliageTypes, IM_ARRAYSIZE(foliageTypes));

		// Brush Parameters
		ImGui::SliderFloat("Brush Radius", &m_brushRadius, 0.5f, 30.0f, "%.1f m");
		if (m_brushMode == BrushMode::Paint)
		{
			ImGui::SliderInt("Brush Density", &m_brushDensity, 1, 50);
			ImGui::SliderFloat("Min Spacing", &m_minSpacing, 0.1f, 3.0f, "%.2f m");
			ImGui::SliderFloat("Min Scale", &m_minScale, 0.2f, 3.0f, "%.2f");
			ImGui::SliderFloat("Max Scale", &m_maxScale, 0.2f, 4.0f, "%.2f");
			ImGui::Checkbox("Random Yaw Rotation", &m_randomYaw);
		}
		else
		{
			ImGui::Checkbox("Erase Selected Type Only", &m_eraseSelectedOnly);
		}

		ImGui::Separator();

		// Stylized Wind & Breezing Controls
		if (ImGui::TreeNode("Stylized Wind & Breezing (Scrolling Noise)"))
		{
			ImGui::SliderFloat("Wind Speed", &m_windSpeed, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("Wind Strength", &m_windStrength, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Noise Scale", &m_noiseScale, 0.005f, 0.2f, "%.3f");

			float angle = std::atan2(m_windDirection.y, m_windDirection.x);
			if (ImGui::SliderAngle("Wind Direction", &angle))
			{
				m_windDirection.x = std::cos(angle);
				m_windDirection.y = std::sin(angle);
			}

			ImGui::TreePop();
		}

		// Color & Shading Palette
		if (ImGui::TreeNode("Stylized Palette & Shading"))
		{
			ImGui::Checkbox("Blend with Terrain Color Map", &m_useColorMap);
			ImGui::Checkbox("Use Texture Atlas", &m_useTexture);
			ImGui::SliderFloat("Alpha Cutoff (Depth Test)", &m_alphaCutoff, 0.1f, 0.9f, "%.2f");

			ImGui::ColorEdit3("Grass Root Color", &m_grassRootColor.x);
			ImGui::ColorEdit3("Grass Tip Color", &m_grassTipColor.x);
			ImGui::ColorEdit3("Flower Color", &m_flowerColor.x);
			ImGui::ColorEdit3("Tree Leaf Color", &m_leafColor.x);
			ImGui::ColorEdit3("Tree Bark Color", &m_barkColor.x);

			ImGui::TreePop();
		}

		// Instance Statistics & Memory Footprint
		ImGui::Separator();
		size_t grassCount = GetInstanceCount(FoliageType::Grass);
		size_t flowerCount = GetInstanceCount(FoliageType::Flower);
		size_t treeCount = GetInstanceCount(FoliageType::Tree);
		size_t totalCount = grassCount + flowerCount + treeCount;
		size_t totalMemoryBytes = totalCount * sizeof(FoliageInstanceData);

		ImGui::Text("Active Instances: %zu (Grass: %zu, Flowers: %zu, Trees: %zu)", totalCount, grassCount, flowerCount, treeCount);
		ImGui::Text("Data Usage: %.2f KB (%zu bytes, 24 bytes/instance)", static_cast<float>(totalMemoryBytes) / 1024.0f, totalMemoryBytes);

		if (ImGui::Button("Clear Current Type"))
		{
			ClearType(static_cast<FoliageType>(m_selectedTypeIndex));
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear All Foliage"))
		{
			ClearAll();
		}

		// ---------------------------------------------------------
		// Interactive 3D Brush Ring Rendering & Raycast Handling
		// ---------------------------------------------------------
		if (m_brushActive && terrain != nullptr)
		{
			RECT size = gameContext.deviceResources.GetOutputSize();
			float screenW = static_cast<float>(size.right - size.left);
			float screenH = static_cast<float>(size.bottom - size.top);

			float vpX = 0.0f;
			float vpY = 0.0f;
			float vpW = (screenW > 0.0f) ? screenW : 1280.0f;
			float vpH = (screenH > 0.0f) ? screenH : 720.0f;

			if (m_cachedViewport.Width > 0.0f && m_cachedViewport.Height > 0.0f)
			{
				vpX = m_cachedViewport.TopLeftX;
				vpY = m_cachedViewport.TopLeftY;
				vpW = m_cachedViewport.Width;
				vpH = m_cachedViewport.Height;
			}

			DirectX::SimpleMath::Viewport vp(vpX, vpY, vpW, vpH);
			DirectX::SimpleMath::Matrix view = m_cachedView;
			DirectX::SimpleMath::Matrix proj = m_cachedProj;

			// If m_cachedView is not yet populated from Draw, fallback to gameContext.mainCamera
			if (std::abs(view.Determinant()) < 1e-5f && gameContext.mainCamera != nullptr)
			{
				view = gameContext.mainCamera->GetCameraData().viewMatrix;
				proj = gameContext.mainCamera->GetCameraData().projMatrix;
			}

			ImVec2 mousePos = ImGui::GetMousePos();
			float mouseX = mousePos.x;
			float mouseY = mousePos.y;

			bool mouseInVp = (mouseX >= vpX && mouseX <= vpX + vpW && mouseY >= vpY && mouseY <= vpY + vpH);

			if (mouseInVp && std::abs(view.Determinant()) >= 1e-5f)
			{
				DirectX::SimpleMath::Vector3 rayOrigin = vp.Unproject(
					DirectX::SimpleMath::Vector3(mouseX, mouseY, 0.0f),
					proj, view, DirectX::SimpleMath::Matrix::Identity
				);
				DirectX::SimpleMath::Vector3 rayTarget = vp.Unproject(
					DirectX::SimpleMath::Vector3(mouseX, mouseY, 1.0f),
					proj, view, DirectX::SimpleMath::Matrix::Identity
				);
				DirectX::SimpleMath::Vector3 rayDir = rayTarget - rayOrigin;
				rayDir.Normalize();

				DirectX::SimpleMath::Vector3 hitPos, hitNormal;
				if (RaycastTerrain(terrain, rayOrigin, rayDir, hitPos, hitNormal))
				{
					m_hasTerrainHit = true;
					m_currentHitPos = hitPos;
					m_currentHitNormal = hitNormal;

					// Draw contour-following brush circle directly on ImGui's Foreground Draw List
					ImDrawList* drawList = ImGui::GetForegroundDrawList();
					const int segments = 40;
					std::vector<ImVec2> polyPoints;
					polyPoints.reserve(segments + 1);

					for (int i = 0; i < segments; ++i)
					{
						float angle = (static_cast<float>(i) / static_cast<float>(segments)) * DirectX::XM_2PI;
						float px = hitPos.x + m_brushRadius * std::cos(angle);
						float pz = hitPos.z + m_brushRadius * std::sin(angle);
						float py = 0.0f;
						DirectX::SimpleMath::Vector3 dummyNorm;
						SampleTerrainHeightAndNormal(terrain, px, pz, py, dummyNorm);

						DirectX::SimpleMath::Vector3 sp = vp.Project(
							DirectX::SimpleMath::Vector3(px, py + 0.08f, pz),
							proj, view, DirectX::SimpleMath::Matrix::Identity
						);

						if (sp.z > 0.0f && sp.z < 1.0f)
						{
							polyPoints.push_back(ImVec2(sp.x, sp.y));
						}
					}

					ImU32 outlineColor = (m_brushMode == BrushMode::Paint) ?
						IM_COL32(50, 255, 80, 255) : IM_COL32(255, 60, 60, 255);
					ImU32 fillColor = (m_brushMode == BrushMode::Paint) ?
						IM_COL32(50, 255, 80, 35) : IM_COL32(255, 60, 60, 35);

					if (polyPoints.size() >= 3)
					{
						drawList->AddConvexPolyFilled(polyPoints.data(), static_cast<int>(polyPoints.size()), fillColor);
						drawList->AddPolyline(polyPoints.data(), static_cast<int>(polyPoints.size()), outlineColor, ImDrawFlags_Closed, 3.0f);
					}

					// Center dot & badge
					DirectX::SimpleMath::Vector3 centerSp = vp.Project(
						DirectX::SimpleMath::Vector3(hitPos.x, hitPos.y + 0.1f, hitPos.z),
						proj, view, DirectX::SimpleMath::Matrix::Identity
					);
					if (centerSp.z > 0.0f && centerSp.z < 1.0f)
					{
						ImVec2 c(centerSp.x, centerSp.y);
						drawList->AddCircleFilled(c, 5.0f, IM_COL32(255, 255, 0, 255));
						drawList->AddCircle(c, 7.0f, IM_COL32(0, 0, 0, 220), 0, 2.0f);

						char badge[96];
						snprintf(badge, sizeof(badge), "[%s] %s (R: %.1fm)",
							(m_brushMode == BrushMode::Paint) ? "PAINT" : "ERASE",
							foliageTypes[m_selectedTypeIndex],
							m_brushRadius);
						ImVec2 txtSz = ImGui::CalcTextSize(badge);
						ImVec2 txtPos(c.x + 14, c.y - 12);
						drawList->AddRectFilled(
							ImVec2(txtPos.x - 4, txtPos.y - 2),
							ImVec2(txtPos.x + txtSz.x + 4, txtPos.y + txtSz.y + 2),
							IM_COL32(15, 15, 15, 220), 4.0f
						);
						drawList->AddText(txtPos, outlineColor, badge);
					}

					// Mouse painting / erasing
					if (!ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Left))
					{
						if (m_paintCooldown <= 0.0f)
						{
							if (m_brushMode == BrushMode::Paint)
							{
								PaintInstances(terrain, hitPos);
							}
							else
							{
								EraseInstances(hitPos);
							}
							m_paintCooldown = 0.035f;
						}
					}
				}
				else
				{
					m_hasTerrainHit = false;
				}
			}
			else
			{
				m_hasTerrainHit = false;
			}
		}
	}

	nlohmann::json FoliageComponent::Serialize()
	{
		nlohmann::json j;
		j["WindSpeed"] = m_windSpeed;
		j["WindStrength"] = m_windStrength;
		j["NoiseScale"] = m_noiseScale;
		j["WindDirX"] = m_windDirection.x;
		j["WindDirY"] = m_windDirection.y;
		j["AlphaCutoff"] = m_alphaCutoff;
		j["UseColorMap"] = m_useColorMap;
		j["UseTexture"] = m_useTexture;

		j["GrassRootColor"] = { m_grassRootColor.x, m_grassRootColor.y, m_grassRootColor.z, m_grassRootColor.w };
		j["GrassTipColor"]  = { m_grassTipColor.x, m_grassTipColor.y, m_grassTipColor.z, m_grassTipColor.w };
		j["FlowerColor"]    = { m_flowerColor.x, m_flowerColor.y, m_flowerColor.z, m_flowerColor.w };
		j["LeafColor"]      = { m_leafColor.x, m_leafColor.y, m_leafColor.z, m_leafColor.w };
		j["BarkColor"]      = { m_barkColor.x, m_barkColor.y, m_barkColor.z, m_barkColor.w };

		// Compact flat serialization of instances for minimum file size
		// Each instance: [x, y, z, rot, scale, type]
		nlohmann::json instancesJson = nlohmann::json::array();
		for (size_t t = 0; t < static_cast<size_t>(FoliageType::Count); ++t)
		{
			for (const auto& inst : m_instances[t])
			{
				instancesJson.push_back({
					inst.worldPos.x,
					inst.worldPos.y,
					inst.worldPos.z,
					inst.rotation,
					inst.scale,
					inst.type
				});
			}
		}
		j["Instances"] = instancesJson;

		return j;
	}

	void FoliageComponent::Deserialize(const nlohmann::json& data)
	{
		if (data.contains("WindSpeed")) m_windSpeed = data["WindSpeed"].get<float>();
		if (data.contains("WindStrength")) m_windStrength = data["WindStrength"].get<float>();
		if (data.contains("NoiseScale")) m_noiseScale = data["NoiseScale"].get<float>();
		if (data.contains("WindDirX") && data.contains("WindDirY"))
		{
			m_windDirection.x = data["WindDirX"].get<float>();
			m_windDirection.y = data["WindDirY"].get<float>();
		}
		if (data.contains("AlphaCutoff")) m_alphaCutoff = data["AlphaCutoff"].get<float>();
		if (data.contains("UseColorMap")) m_useColorMap = data["UseColorMap"].get<bool>();
		if (data.contains("UseTexture")) m_useTexture = data["UseTexture"].get<bool>();

		if (data.contains("GrassRootColor"))
		{
			auto c = data["GrassRootColor"];
			m_grassRootColor = DirectX::SimpleMath::Vector4(c[0], c[1], c[2], c[3]);
		}
		if (data.contains("GrassTipColor"))
		{
			auto c = data["GrassTipColor"];
			m_grassTipColor = DirectX::SimpleMath::Vector4(c[0], c[1], c[2], c[3]);
		}
		if (data.contains("FlowerColor"))
		{
			auto c = data["FlowerColor"];
			m_flowerColor = DirectX::SimpleMath::Vector4(c[0], c[1], c[2], c[3]);
		}
		if (data.contains("LeafColor"))
		{
			auto c = data["LeafColor"];
			m_leafColor = DirectX::SimpleMath::Vector4(c[0], c[1], c[2], c[3]);
		}
		if (data.contains("BarkColor"))
		{
			auto c = data["BarkColor"];
			m_barkColor = DirectX::SimpleMath::Vector4(c[0], c[1], c[2], c[3]);
		}

		ClearAll();

		if (data.contains("Instances") && data["Instances"].is_array())
		{
			for (const auto& item : data["Instances"])
			{
				if (item.is_array() && item.size() >= 6)
				{
					FoliageInstanceData inst;
					inst.worldPos.x = item[0].get<float>();
					inst.worldPos.y = item[1].get<float>();
					inst.worldPos.z = item[2].get<float>();
					inst.rotation   = item[3].get<float>();
					inst.scale      = item[4].get<float>();
					inst.type       = item[5].get<uint32_t>();

					size_t typeIdx = static_cast<size_t>(inst.type);
					if (typeIdx < static_cast<size_t>(FoliageType::Count))
					{
						m_instances[typeIdx].push_back(inst);
						m_bufferDirty[typeIdx] = true;
					}
				}
			}
		}
	}

	void FoliageComponent::InitializeAfterDeserialize(GameContext& gameContext)
	{
		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
		InitializeResources(device);
	}
}
