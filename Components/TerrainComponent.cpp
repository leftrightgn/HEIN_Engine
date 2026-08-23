#include "pch.h"
#include "TerrainComponent.h"
#include "Entities/Actor.h"
#include "Framework/GameContext.h"
#include "TransformComponent.h"
#include "DebugingTools/DebugUIManager.h"
#include "DebugingTools/EditorUtils.h"
#include <ImGui/imgui_stdlib.h>
#include <cstdio>
#include <d3dcompiler.h>
#include "Camera/CameraController.h"

HEIN::TerrainComponent::TerrainComponent(Actor* owner)
	: IComponent(owner)
	, m_terrainWidth(0)
	, m_terrainHeight(0)
	, m_heightScale(10.0f)
	, m_vertexCount(0)
{
}

bool HEIN::TerrainComponent::Initialize(
	GameContext& gameContext,
	const wchar_t* heightMapFilename, 
	const wchar_t* textureFilename,
	const wchar_t* colorMapFilename,
	const wchar_t* normalMapFilename,
	float heightScale,
	float textureTiling
)
{
	m_heightMapFilename = heightMapFilename ? heightMapFilename : L"";
	m_textureFilename = textureFilename ? textureFilename : L"";
	m_colorMapFilename = colorMapFilename ? colorMapFilename : L"";
	m_normalMapFilename = normalMapFilename ? normalMapFilename : L"";
	m_heightScale = heightScale;
	m_texutreTiling = textureTiling;

	ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

	if (m_heightMapFilename.empty())
	{
		return false;
	}
	// Detect File extension(.bmp vs .raw)
	std::wstring ext = m_heightMapFilename;
	size_t exPos = ext.find_last_of(L".");
	bool isRaw = false;

	if (exPos != std::wstring::npos)
	{
		std::wstring extension = ext.substr(exPos + 1);

		std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
		if (extension == L"r16") isRaw = true;
	}
	bool success = false;
	if (isRaw) success = LoadRawHeightMap(heightMapFilename);
	else success = LoadHeightMap(heightMapFilename);

	if (!success)
	{
		OutputDebugStringA("FailedToLoadHeightMapFile");
		return false;
	}

	m_vertexCount = m_terrainWidth * m_terrainHeight;

	if (!m_colorMapFilename.empty() && LoadColorMap(m_colorMapFilename.c_str()))
	{
		// Color map successfully applied to the grid
	}
	else
	{
		for (int i = 0; i < m_vertexCount; i++)
		{
			m_heightMap[i].r = 1.0f; m_heightMap[i].g = 1.0f; m_heightMap[i].b = 1.0f;
		}
	}

	CalculateNormals();

	// Build the vertices/ indices and create the GPU Buffer
	if (!InitializeBuffer(device))
	{
		OutputDebugStringA("FailedToCreateBuffer!");
		return false;
	}

	m_texture.Reset();
	if (!m_textureFilename.empty())
	{
		HRESULT hr = DirectX::CreateDDSTextureFromFile(device, m_textureFilename.c_str(), nullptr, m_texture.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			// Fallback: If CWD changed, try relative to the most likely project directories
			std::wstring fallback = L"../Dual/" + m_textureFilename;
			hr = DirectX::CreateDDSTextureFromFile(device, fallback.c_str(), nullptr, m_texture.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				fallback = L"../../Dual/Dual/" + m_textureFilename;
				DirectX::CreateDDSTextureFromFile(device, fallback.c_str(), nullptr, m_texture.ReleaseAndGetAddressOf());
			}
		}
	}

	m_normalTexture.Reset();
	if (!m_normalMapFilename.empty())
	{
		HRESULT hrNorm = DirectX::CreateDDSTextureFromFile(device, m_normalMapFilename.c_str(), nullptr, m_normalTexture.ReleaseAndGetAddressOf());
		if (FAILED(hrNorm))
		{
			std::wstring fallback = L"../Dual/" + m_normalMapFilename;
			hrNorm = DirectX::CreateDDSTextureFromFile(device, fallback.c_str(), nullptr, m_normalTexture.ReleaseAndGetAddressOf());
			if (FAILED(hrNorm))
			{
				fallback = L"../../Dual/Dual/" + m_normalMapFilename;
				DirectX::CreateDDSTextureFromFile(device, fallback.c_str(), nullptr, m_normalTexture.ReleaseAndGetAddressOf());
			}
		}
	}

	// Compile and load Custom shaders
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// Compile Vertex Shader
	HRESULT hr = D3DCompileFromFile(
		L"../External/Engine/Shaders/Terrain_VS.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		D3DCOMPILE_ENABLE_STRICTNESS,
		0,
		&vertexShaderBlob,
		&errorBlob
	);
	if (FAILED(hr))
	{
		hr = D3DCompileFromFile(
			L"External/Engine/Shaders/Terrain_VS.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"vs_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS,
			0,
			&vertexShaderBlob,
			&errorBlob
		);
	}
	if (FAILED(hr))
	{
		if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		return false;
	}

	// Compile Pixel Shader
	hr = D3DCompileFromFile(
		L"../External/Engine/Shaders/Terrain_PS.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		D3DCOMPILE_ENABLE_STRICTNESS,
		0,
		&pixelShaderBlob,
		&errorBlob
	);
	if (FAILED(hr))
	{
		hr = D3DCompileFromFile(
			L"External/Engine/Shaders/Terrain_PS.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"ps_5_0",
			D3DCOMPILE_ENABLE_STRICTNESS,
			0,
			&pixelShaderBlob,
			&errorBlob
		);
	}
	if (FAILED(hr))
	{
		if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		return false;
	}

	device->CreateVertexShader(
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		nullptr,
		m_vertexShader.ReleaseAndGetAddressOf()
	);
	device->CreatePixelShader(
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize(),
		nullptr,
		m_pixelShader.ReleaseAndGetAddressOf()
	);

	// Create custom input layout matching TerrainVertexType
	D3D11_INPUT_ELEMENT_DESC polygonLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	DX::ThrowIfFailed(
		device->CreateInputLayout(
			polygonLayout,
			numElements,
			vertexShaderBlob->GetBufferPointer(),
			vertexShaderBlob->GetBufferSize(),
			m_inputLayout.ReleaseAndGetAddressOf()
		)
	);

	// Create Constant Buffer
	D3D11_BUFFER_DESC matrixBufferDesc = {};
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	device->CreateBuffer(&matrixBufferDesc, nullptr, m_matrixBuffer.ReleaseAndGetAddressOf());

	D3D11_BUFFER_DESC lightBufferDesc = {};
	lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightBufferDesc.ByteWidth = sizeof(LightBufferType);
	lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	device->CreateBuffer(&lightBufferDesc, nullptr, m_lightBuffer.ReleaseAndGetAddressOf());

	// Creat Sampler State
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	device->CreateSamplerState(&samplerDesc, m_sampleState.ReleaseAndGetAddressOf());

	return true;
}

void HEIN::TerrainComponent::Draw(
	GameContext& gameContext, 
	const DirectX::SimpleMath::Matrix& world,
	const DirectX::SimpleMath::Matrix& view, 
	const DirectX::SimpleMath::Matrix& proj
)
{
	if (m_needsReload)
	{
		Initialize(
			gameContext,
			m_heightMapFilename.c_str(),
			m_textureFilename.c_str(),
			m_colorMapFilename.c_str(),
			m_normalMapFilename.c_str(),
			m_heightScale,
			m_texutreTiling
		);
		m_needsReload = false;
	}

	if (!m_isVisible || m_cells.empty()) return;

	ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

	// Apply the world Transform from TransformComponent
	DirectX::SimpleMath::Matrix finalworld = world;

	TransformComponent* transform = m_owner->GetComponent<HEIN::TransformComponent>();
	if (transform)
	{
		finalworld = transform->GetWorldMatrix();
	}
	context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
	// If the wireFrame mode is enabled , switch to rasterizer state 
	if (m_isWireFrame) context->RSSetState(gameContext.commonStates.Wireframe());
	else context->RSSetState(gameContext.commonStates.CullCounterClockwise());

	// Set primitive topology and input layout
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Update Matrix Constant Buffer
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	if (SUCCEEDED(context->Map(m_matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		MatrixBufferType* dataPtr = (MatrixBufferType*)mappedResource.pData;
		// HLSL requires matrices to be transposed 
		dataPtr->world = finalworld.Transpose();
		dataPtr->view = view.Transpose();
		dataPtr->projection = proj.Transpose();
		context->Unmap(m_matrixBuffer.Get(), 0);
	}
	context->VSSetConstantBuffers(0, 1, m_matrixBuffer.GetAddressOf());

	if (SUCCEEDED(context->Map(m_lightBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		LightBufferType* dataPtr = (LightBufferType*)mappedResource.pData;

		DirectX::SimpleMath::Vector3 safeLightDir = m_lightDirection;
		safeLightDir.Normalize();

		dataPtr->diffuseColor = DirectX::SimpleMath::Vector4(m_diffuseColor.x, m_diffuseColor.y, m_diffuseColor.z, 1.0f);
		dataPtr->lightDirection = safeLightDir;
		dataPtr->hasTexture = m_texture ? 1.0f : 0.0f;
		dataPtr->textureTiling = m_texutreTiling;
		dataPtr->hasNormalMap = m_normalTexture ? 1.0f : 0.0f;
		dataPtr->padding = DirectX::SimpleMath::Vector2(0.0f, 0.0f);
		context->Unmap(m_lightBuffer.Get(), 0);
	}
	context->PSSetConstantBuffers(1, 1, m_lightBuffer.GetAddressOf());

	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

	ID3D11ShaderResourceView* textures[2] = {
		m_texture ? m_texture.Get() : nullptr,
		m_normalTexture ? m_normalTexture.Get() : nullptr
	};
	context->PSSetShaderResources(0, 2, textures);
	context->PSSetSamplers(0, 1, m_sampleState.GetAddressOf());

	// Build World-Space Camera Frustum
	DirectX::BoundingFrustum worldFrustum;
	if (m_freezeFrustum)
	{
		worldFrustum = m_frozenFrustum;
	}
	else
	{
		DirectX::SimpleMath::Matrix cullingView = view;
		DirectX::SimpleMath::Matrix cullingProj = proj;

		// Use Main Game Camera's View and Proj for culling
		if (gameContext.mainCamera != nullptr)
		{
			cullingView = gameContext.mainCamera->GetView();

			D3D11_VIEWPORT vp = gameContext.deviceResources.GetScreenViewport();
			float aspect = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : (1280.0f / 720.0f);
			float fov = gameContext.mainCamera->GetFov();
			if (fov <= 0.0f) fov = DirectX::XM_PI / 4.0f;

			cullingProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
				fov,
				aspect,
				0.1f,
				1000.0f
			);
		}

		// DirectXTK SimpleMath produces Right-Handed matrices (rhcoords = true)
		DirectX::BoundingFrustum localFrustum(cullingProj, true);

		DirectX::SimpleMath::Matrix camWorld;
		if (std::abs(cullingView.Determinant()) < 1e-6f)
		{
			camWorld = DirectX::SimpleMath::Matrix::Identity;
		}
		else
		{
			camWorld = cullingView.Invert();
		}

		localFrustum.Transform(worldFrustum, camWorld);

		// Normalize orientation quaternion to guarantee numerical stability
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&worldFrustum.Orientation);
		q = DirectX::XMQuaternionNormalize(q);
		DirectX::XMStoreFloat4(&worldFrustum.Orientation, q);

		m_frozenFrustum = worldFrustum;
	}

	m_renderedCellCount = 0;
	int cellIndex = 0;

	// DRAW CELLS (with World-Space Frustum Culling & OBB Visualizer)
	for (const auto& cell : m_cells)
	{
		// If single cell debugging is enabled, only draw that cell
		if (m_debugSingleCell >= 0 && cellIndex != m_debugSingleCell)
		{
			cellIndex++;
			continue;
		}

		DirectX::BoundingOrientedBox localOBB;
		DirectX::BoundingOrientedBox::CreateFromBoundingBox(localOBB, cell->GetBoundingBox());

		DirectX::BoundingOrientedBox worldOBB;
		localOBB.Transform(worldOBB, finalworld);

		// Queue Cell Bounding Box for debug rendering if enabled
		if (m_showCellBounds && gameContext.debugCollisionRenderer)
		{
			gameContext.debugCollisionRenderer->QueueOBB(worldOBB, DirectX::Colors::LimeGreen);
		}

		// World-Space Frustum Culling check
		if (m_enableFrustumCulling)
		{
			if (!worldFrustum.Intersects(worldOBB))
			{
				cellIndex++;
				continue;
			}
		}

		cell->Draw(context);
		m_renderedCellCount++;
		cellIndex++;
	}

	// Reset rasterizer state to default
	context->RSSetState(gameContext.commonStates.CullCounterClockwise());
}
nlohmann::json HEIN::TerrainComponent::Serialize()
{
	nlohmann::json data = IComponent::Serialize();
	std::string narrowPath(m_heightMapFilename.begin(), m_heightMapFilename.end());
	data["HeightMapPath"] = narrowPath;
	std::string texPath(m_textureFilename.begin(), m_textureFilename.end());
	data["TexturePath"] = texPath;
	std::string colorPath(m_colorMapFilename.begin(), m_colorMapFilename.end());
	data["ColorMapPath"] = colorPath;
	std::string normalPath(m_normalMapFilename.begin(), m_normalMapFilename.end());
	data["NormalMapPath"] = normalPath;
	data["HeightScale"] = m_heightScale;
	data["isWiredFrame"] = m_isWireFrame;
	data["TextureTiling"] = m_texutreTiling;
	
	// Added Light and Visibility Saving
	data["IsVisible"] = m_isVisible;
	data["LightDirection"] = nlohmann::json::array({ m_lightDirection.x, m_lightDirection.y, m_lightDirection.z });
	data["DiffuseColor"] = nlohmann::json::array({ m_diffuseColor.x, m_diffuseColor.y, m_diffuseColor.z });

	return data;
}

void HEIN::TerrainComponent::Deserialize(const nlohmann::json& data)
{
	IComponent::Deserialize(data);
	if (data.contains("HeightMapPath"))
	{
		std::string narrowPath = data["HeightMapPath"];
		m_heightMapFilename = std::wstring(narrowPath.begin(), narrowPath.end());
	}
	else
	{
		m_heightMapFilename = L"";
	}
	if (data.contains("TexturePath"))
	{
		std::string texPath = data["TexturePath"];
		m_textureFilename = std::wstring(texPath.begin(), texPath.end());
	}
	else
	{
		m_textureFilename = L"";
	}
	if (data.contains("ColorMapPath")) 
	{
		std::string colorPath = data["ColorMapPath"];
		m_colorMapFilename = std::wstring(colorPath.begin(), colorPath.end());
	}
	else 
	{
		m_colorMapFilename = L"";
	}
	if (data.contains("NormalMapPath"))
	{
		std::string normalPath = data["NormalMapPath"];
		m_normalMapFilename = std::wstring(normalPath.begin(), normalPath.end());
	}
	else
	{
		m_normalMapFilename = L"";
	}
	if (data.contains("HeightScale")) m_heightScale = data["HeightScale"];
	if (data.contains("isWiredFrame")) m_isWireFrame = data["isWiredFrame"];
	if (data.contains("TextureTiling")) m_texutreTiling = data["TextureTiling"];
	
	// Added Light and Visibility Loading
	if (data.contains("IsVisible")) m_isVisible = data["IsVisible"];
	if (data.contains("LightDirection")) m_lightDirection = DirectX::SimpleMath::Vector3(data["LightDirection"][0], data["LightDirection"][1], data["LightDirection"][2]);
	if (data.contains("DiffuseColor"))
	{
		m_diffuseColor = DirectX::SimpleMath::Vector3(data["DiffuseColor"][0], data["DiffuseColor"][1], data["DiffuseColor"][2]);
	}
	
	m_needsReload = true; 
}

void HEIN::TerrainComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
	if (!m_heightMapFilename.empty())
	{
		Initialize(
			gameContext,
			m_heightMapFilename.c_str(),
			m_textureFilename.c_str(),
			m_colorMapFilename.c_str(),
			m_normalMapFilename.c_str(),
			m_heightScale,
			m_texutreTiling
		);
	}
}

void HEIN::TerrainComponent::OnInspectorGUI(GameContext& gameContext)
{
	if (ImGui::CollapsingHeader("Terrain Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		HWND windowHandle = gameContext.deviceResources.GetWindow();
		ImGui::Checkbox("Visible", &m_isVisible);
		ImGui::Checkbox("WireFrame Mode", &m_isWireFrame);

		ImGui::Separator();
		ImGui::Text("Cell Management & Debugging");
		ImGui::Text("Total Cells: %d  |  Rendered Cells: %d", (int)m_cells.size(), m_renderedCellCount);
		ImGui::Checkbox("Show Cell Bounding Boxes", &m_showCellBounds);
		ImGui::Checkbox("Enable Frustum Culling", &m_enableFrustumCulling);
		ImGui::Checkbox("Freeze Culling Frustum", &m_freezeFrustum);
		
		int maxCellIndex = (int)m_cells.size() - 1;
		if (ImGui::SliderInt("Isolate Single Cell", &m_debugSingleCell, -1, maxCellIndex, m_debugSingleCell < 0 ? "All Cells (-1)" : "Cell %d"))
		{
			// Live isolate
		}

		if (ImGui::Checkbox("Debug Cell Grid Colors", &m_debugCellColors))
		{
			InitializeBuffer(gameContext.deviceResources.GetD3DDevice());
		}

		ImGui::Separator();
		if (ImGui::DragFloat("HeightScale", &m_heightScale, 0.5f, 1.0f, 100.0f))
		{
			CalculateNormals();
			// Recreate the buffer with the new scale immediately
			InitializeBuffer(gameContext.deviceResources.GetD3DDevice());
		}
		if (ImGui::DragFloat("Texture Tiling", &m_texutreTiling, 0.5f, 1.0f, 128.0f))
		{
			// Recreate the buffer to apply the new UV coordinates
			InitializeBuffer(gameContext.deviceResources.GetD3DDevice());
		}

		ImGui::Separator();
		ImGui::Text("Lighting");
		ImGui::DragFloat3("Light Direction", &m_lightDirection.x, 0.05f, -1.0f, 1.0f);
		ImGui::ColorEdit3("Diffuse Color", &m_diffuseColor.x);
		
		ImGui::Separator();

		std::string pathStr = std::string(m_heightMapFilename.begin(), m_heightMapFilename.end());
		if (ImGui::InputText("HeightMap File", &pathStr, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_heightMapFilename = std::wstring(pathStr.begin(), pathStr.end());
			Initialize(
				gameContext,
				m_heightMapFilename.c_str(),
				m_textureFilename.c_str(),
				m_colorMapFilename.c_str(),
				m_normalMapFilename.c_str(),
				m_heightScale,
				m_texutreTiling
			);
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"Bitmap Files\0*.bmp;*.r16\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_heightMapFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				Initialize(
					gameContext,
					m_heightMapFilename.c_str(), 
					m_textureFilename.c_str(),
					m_colorMapFilename.c_str(), 
					m_normalMapFilename.c_str(), 
					m_heightScale, 
					m_texutreTiling
				);
			}
		}

		ImGui::Separator();

		std::string texPathStr = std::string(m_textureFilename.begin(), m_textureFilename.end());
		if (ImGui::InputText("Texture File", &texPathStr, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_textureFilename = std::wstring(texPathStr.begin(), texPathStr.end());
			if (!m_textureFilename.empty())
			{
				DirectX::CreateDDSTextureFromFile(gameContext.deviceResources.GetD3DDevice(), m_textureFilename.c_str(), nullptr, m_texture.ReleaseAndGetAddressOf());
			}
			else
			{
				m_texture.Reset();
			}
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Browse Texture..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"DDS Files\0*.dds\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_textureFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				DirectX::CreateDDSTextureFromFile(gameContext.deviceResources.GetD3DDevice(), m_textureFilename.c_str(), nullptr, m_texture.ReleaseAndGetAddressOf());
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Remove Texture"))
		{
			m_textureFilename.clear();
			m_texture.Reset();
		}

		ImGui::Separator();
		std::string normalPathStr = std::string(m_normalMapFilename.begin(), m_normalMapFilename.end());
		if (ImGui::InputText("NormalMap File", &normalPathStr, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_normalMapFilename = std::wstring(normalPathStr.begin(), normalPathStr.end());
			if (!m_normalMapFilename.empty())
			{
				DirectX::CreateDDSTextureFromFile(gameContext.deviceResources.GetD3DDevice(), m_normalMapFilename.c_str(), nullptr, m_normalTexture.ReleaseAndGetAddressOf());
			}
			else
			{
				m_normalTexture.Reset();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Browse NormalMap..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"DDS Files\0*.dds\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_normalMapFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				DirectX::CreateDDSTextureFromFile(gameContext.deviceResources.GetD3DDevice(), m_normalMapFilename.c_str(), nullptr, m_normalTexture.ReleaseAndGetAddressOf());
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Remove NormalMap"))
		{
			m_normalMapFilename.clear();
			m_normalTexture.Reset();
		}

		ImGui::Separator();
		std::string colorPathStr = std::string(m_colorMapFilename.begin(), m_colorMapFilename.end());
		if (ImGui::InputText("ColorMap File", &colorPathStr, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_colorMapFilename = std::wstring(colorPathStr.begin(), colorPathStr.end());
			m_needsReload = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Browse ColorMap..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"Bitmap Files\0*.bmp\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_colorMapFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				m_needsReload = true; 
			}
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Remove ColorMap"))
		{
			m_colorMapFilename.clear();
			m_needsReload = true;
		}
	}
}

bool HEIN::TerrainComponent::LoadHeightMap(const wchar_t* filename)
{
	FILE* filePtr;
	BITMAPFILEHEADER bitmapFileHeader;
	BITMAPINFOHEADER bitmapInfoHeader;
	int imageSize, index, i, j;
	unsigned char* bitmapImage;
	unsigned char height;

	// Open the heightMap file in the Binary
	int error = _wfopen_s(&filePtr, filename, L"rb");
	if (error != 0) return false;

	// Read the FileHeader
	fread(&bitmapFileHeader, sizeof(bitmapFileHeader), 1, filePtr);

	// Read the FileInfo
	fread(&bitmapInfoHeader, sizeof(bitmapInfoHeader), 1, filePtr);
	
	m_terrainWidth = bitmapInfoHeader.biWidth;
	m_terrainHeight = bitmapInfoHeader.biHeight;
	
	// Support different BMP formats (8-bit, 24-bit, 32-bit)
	int bytesPerPixel = bitmapInfoHeader.biBitCount / 8;
	if (bytesPerPixel == 0) bytesPerPixel = 3; // Fallback just in case

	// BMP rows are padded to a multiple of 4 bytes
	int rowPitch = (m_terrainWidth * bytesPerPixel + 3) & ~3;
	imageSize = rowPitch * m_terrainHeight;

	bitmapImage = new unsigned char[imageSize];
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);
	fread(bitmapImage, 1, imageSize, filePtr);
	fclose(filePtr);

	m_heightMap.resize(m_terrainWidth * m_terrainHeight);

	// Read the image Data into HeightMap
	for (j = 0; j < m_terrainHeight; j++)
	{
		for (i = 0; i < m_terrainWidth; i++)
		{
			// BMP images are stored bottom-to-top in file
			int pixelOffset = (m_terrainHeight - 1 - j) * rowPitch + i * bytesPerPixel;
			
			// For 24/32 bit, we just read the first channel (B) since heightmaps are usually grayscale.
			// For 8-bit, it's the raw grayscale/palette index.
			height = bitmapImage[pixelOffset];
			
			index = j * m_terrainWidth + i;

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)height / 255.0f; // Normalize 0 to 1
			m_heightMap[index].z = (float)j;
		}
	}

	delete[] bitmapImage;
	bitmapImage = nullptr;

	return true;
}

bool HEIN::TerrainComponent::LoadRawHeightMap(const wchar_t* filename)
{
	FILE* filePtr;

	// Open the 16bit raw file in binary mode
	int error = _wfopen_s(&filePtr, filename, L"rb");
	if (error != 0) return false;

	// Automatically Calculate the grid Dimensions by checking the file size
	fseek(filePtr, 0, SEEK_END);
	long fileSize = ftell(filePtr);
	rewind(filePtr);

	// 16bit rawfile uses exactly 2 bytes per pixel
	int numPixels = fileSize / 2;
	m_terrainWidth = static_cast<int>(sqrt(numPixels));
	m_terrainHeight = m_terrainWidth;

	// Safety Check to ensure the file is a perfect square
	if (m_terrainWidth * m_terrainHeight != numPixels)
	{
		OutputDebugStringA("Raw File is not a perfect square!");
		fclose(filePtr);
		return false;
	}

	// Read the 16-bit data
	unsigned short* rawImage = new unsigned short[numPixels];
	fread(rawImage, sizeof(unsigned short), numPixels, filePtr);
	fclose(filePtr);

	m_heightMap.resize(numPixels);

	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			int rawIndex = (j * m_terrainWidth) + i;
			int index = j * m_terrainWidth + i;
			m_heightMap[index].x = (float)i;
			// Normalize by dividing by 65535 instead of 255!
			m_heightMap[index].y = (float)rawImage[rawIndex] / 65535.0f;
			m_heightMap[index].z = (float)j;
		}
	}
	delete[] rawImage;
	return true;
}

bool HEIN::TerrainComponent::CalculateNormals()
{
	if (m_terrainWidth <= 0 || m_terrainHeight <= 0 || m_heightMap.empty())
	{
		return false;
	}

	// Calculate smooth, high-precision surface normals, tangents, and binormals
	// using central differences across the heightfield.
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			int index = j * m_terrainWidth + i;

			int leftX = (i > 0) ? i - 1 : 0;
			int rightX = (i < m_terrainWidth - 1) ? i + 1 : m_terrainWidth - 1;
			int downY = (j > 0) ? j - 1 : 0;
			int upY = (j < m_terrainHeight - 1) ? j + 1 : m_terrainHeight - 1;

			float hL = m_heightMap[j * m_terrainWidth + leftX].y * m_heightScale;
			float hR = m_heightMap[j * m_terrainWidth + rightX].y * m_heightScale;
			float hD = m_heightMap[downY * m_terrainWidth + i].y * m_heightScale;
			float hU = m_heightMap[upY * m_terrainWidth + i].y * m_heightScale;

			float dx = (float)(rightX - leftX);
			float dz = (float)(upY - downY);
			if (dx <= 0.0001f) dx = 1.0f;
			if (dz <= 0.0001f) dz = 1.0f;

			float dHdX = (hR - hL) / dx;
			float dHdZ = (hU - hD) / dz;

			// Normal vector pointing UP towards sky (+Y)
			DirectX::SimpleMath::Vector3 normal(-dHdX, 1.0f, -dHdZ);
			normal.Normalize();

			// Tangent vector along +X
			DirectX::SimpleMath::Vector3 tangent(1.0f, dHdX, 0.0f);
			tangent.Normalize();

			// Binormal vector along +Z
			DirectX::SimpleMath::Vector3 binormal(0.0f, dHdZ, 1.0f);
			binormal.Normalize();

			m_heightMap[index].nx = normal.x;
			m_heightMap[index].ny = normal.y;
			m_heightMap[index].nz = normal.z;

			m_heightMap[index].tx = tangent.x;
			m_heightMap[index].ty = tangent.y;
			m_heightMap[index].tz = tangent.z;

			m_heightMap[index].bx = binormal.x;
			m_heightMap[index].by = binormal.y;
			m_heightMap[index].bz = binormal.z;
		}
	}

	return true;
}

bool HEIN::TerrainComponent::InitializeBuffer(ID3D11Device* device)
{
	m_cells.clear();

	if (m_terrainWidth <= 1 || m_terrainHeight <= 1 || m_heightMap.empty())
	{
		return false;
	}

	const int quadsPerCell = 32;

	int cellRowCount = (m_terrainWidth - 1 + quadsPerCell - 1) / quadsPerCell;
	int cellColumnCount = (m_terrainHeight - 1 + quadsPerCell - 1) / quadsPerCell;

	float halfWidth = (float)m_terrainWidth / 2.0f;
	float halfDepth = (float)m_terrainHeight / 2.0f;

	// Loop through every cell in the grid
	for (int j = 0; j < cellColumnCount; j++)
	{
		for (int i = 0; i < cellRowCount; i++)
		{
			int cellStartX = i * quadsPerCell;
			int cellStartY = j * quadsPerCell;

			int cellQuadsX = std::min(quadsPerCell, (m_terrainWidth - 1) - cellStartX);
			int cellQuadsY = std::min(quadsPerCell, (m_terrainHeight - 1) - cellStartY);

			int cellVertsX = cellQuadsX + 1;
			int cellVertsY = cellQuadsY + 1;

			std::vector<TerrainVertexType> vertices(cellVertsX * cellVertsY);
			std::vector<uint32_t> indices(cellQuadsX * cellQuadsY * 6);

			int vertexIndex = 0;

			for (int localY = 0; localY < cellVertsY; localY++)
			{
				for (int localX = 0; localX < cellVertsX; localX++)
				{
					int globalX = cellStartX + localX;
					int globalY = cellStartY + localY;

					int globalIndex = (globalY * m_terrainWidth) + globalX;

					vertices[vertexIndex].position.x = m_heightMap[globalIndex].x - halfWidth;
					vertices[vertexIndex].position.y = m_heightMap[globalIndex].y * m_heightScale;
					vertices[vertexIndex].position.z = m_heightMap[globalIndex].z - halfDepth;

					vertices[vertexIndex].normal.x = m_heightMap[globalIndex].nx;
					vertices[vertexIndex].normal.y = m_heightMap[globalIndex].ny;
					vertices[vertexIndex].normal.z = m_heightMap[globalIndex].nz;

					vertices[vertexIndex].tangent.x = m_heightMap[globalIndex].tx;
					vertices[vertexIndex].tangent.y = m_heightMap[globalIndex].ty;
					vertices[vertexIndex].tangent.z = m_heightMap[globalIndex].tz;

					vertices[vertexIndex].binormal.x = m_heightMap[globalIndex].bx;
					vertices[vertexIndex].binormal.y = m_heightMap[globalIndex].by;
					vertices[vertexIndex].binormal.z = m_heightMap[globalIndex].bz;

					float u = ((float)globalX / (float)(m_terrainWidth - 1));
					float v = ((float)globalY / (float)(m_terrainHeight - 1));

					vertices[vertexIndex].texture.x = u;
					vertices[vertexIndex].texture.y = 1.0f - v; // Flip V

					DirectX::SimpleMath::Vector3 cellTint(1.0f, 1.0f, 1.0f);
					if (m_debugCellColors)
					{
						// Distinct checkerboard / palette pattern per cell
						float cr = ((i % 3) == 0) ? 1.0f : ((i % 3) == 1 ? 0.35f : 0.7f);
						float cg = ((j % 3) == 0) ? 0.35f : ((j % 3) == 1 ? 1.0f : 0.7f);
						float cb = (((i + j) % 3) == 0) ? 0.35f : 1.0f;
						cellTint = DirectX::SimpleMath::Vector3(cr, cg, cb);
					}

					vertices[vertexIndex].color.x = m_heightMap[globalIndex].r * cellTint.x;
					vertices[vertexIndex].color.y = m_heightMap[globalIndex].g * cellTint.y;
					vertices[vertexIndex].color.z = m_heightMap[globalIndex].b * cellTint.z;
					vertices[vertexIndex].color.w = 1.0f;

					vertexIndex++;
				}
			}

			// Generate the indices to stitch this specific chunk together
			int index = 0;
			for (int localY = 0; localY < cellQuadsY; localY++)
			{
				for (int localX = 0; localX < cellQuadsX; localX++)
				{
					int index1 = (localY * cellVertsX) + localX;
					int index2 = (localY * cellVertsX) + (localX + 1);
					int index3 = ((localY + 1) * cellVertsX) + localX;
					int index4 = ((localY + 1) * cellVertsX) + (localX + 1);

					indices[index++] = index1;
					indices[index++] = index2;
					indices[index++] = index3;

					indices[index++] = index2;
					indices[index++] = index4;
					indices[index++] = index3;
				}
			}

			// Hand the memory over to the new cell and save it!
			auto cell = std::make_unique<TerrainCell>();
			cell->Initialize(device, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
			m_cells.push_back(std::move(cell));
		}
	}

	return true;
}

bool HEIN::TerrainComponent::LoadColorMap(const wchar_t* filename)
{
	FILE* filePtr;
	BITMAPFILEHEADER bitmapFileHeader;
	BITMAPINFOHEADER bitmapInfoHeader;
	int imageSize, index, i, j;
	unsigned char* bitmapImage;

	// open the color map file
	int error = _wfopen_s(&filePtr, filename, L"rb");
	if (error != 0) return false;

	fread(&bitmapFileHeader, sizeof(bitmapFileHeader), 1, filePtr);
	fread(&bitmapInfoHeader, sizeof(bitmapInfoHeader), 1, filePtr);

	if (bitmapInfoHeader.biWidth != m_terrainWidth || bitmapInfoHeader.biHeight != m_terrainHeight)
	{
		OutputDebugStringA("ColorMap dimensions do not match HeightMap!");
		fclose(filePtr);
		return false;
	}

	int bytesPerPixel = bitmapInfoHeader.biBitCount / 8;
	if (bytesPerPixel < 3) bytesPerPixel = 3;

	int rowPitch = (m_terrainWidth * bytesPerPixel + 3) & ~3;
	imageSize = rowPitch * m_terrainHeight;

	bitmapImage = new unsigned char[imageSize];
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);
	fread(bitmapImage, 1, imageSize, filePtr);
	fclose(filePtr);

	// Read the image Data into the RGB fields
	for (j = 0; j < m_terrainHeight; j++)
	{
		for (i = 0; i < m_terrainWidth; i++)
		{
			int pixelOffset = (m_terrainHeight - 1 - j) * rowPitch + i * bytesPerPixel;
			index = j * m_terrainWidth + i;

			// Windows BMP files store pixels in BGR (Blue, Green, Red) format!
			m_heightMap[index].b = (float)bitmapImage[pixelOffset] / 255.0f;
			m_heightMap[index].g = (float)bitmapImage[pixelOffset + 1] / 255.0f;
			m_heightMap[index].r = (float)bitmapImage[pixelOffset + 2] / 255.0f;
		}
	}

	delete[] bitmapImage;
	return true;
}
