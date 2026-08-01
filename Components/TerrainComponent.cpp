#include "pch.h"
#include "TerrainComponent.h"
#include "Entities/Actor.h"
#include "Framework/GameContext.h"
#include "TransformComponent.h"
#include "DebugingTools/DebugUIManager.h"
#include "DebugingTools/EditorUtils.h"
#include <ImGui/imgui_stdlib.h>

HEIN::TerrainComponent::TerrainComponent(Actor* owner)
	: IComponent(owner)
	, m_terrainWidth(0)
	, m_terrainHeight(0)
	, m_heightScale(10.0f)
	, m_vertexCount(0)
	, m_indexCount(0)
{
}

bool HEIN::TerrainComponent::Initialize(
	GameContext& gameContext,
	const wchar_t* heightMapFilename, 
	const wchar_t* textureFilename,
	const wchar_t* colorMapFilename,
	float heightScale,
	float textureTiling
)
{
	m_heightMapFilename = heightMapFilename ? heightMapFilename : L"";
	m_textureFilename = textureFilename ? textureFilename : L"";
	m_colorMapFilename = colorMapFilename ? colorMapFilename : L"";
	m_heightScale = heightScale;
	m_texutreTiling = textureTiling;

	ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

	if (m_heightMapFilename.empty())
	{
		return false;
	}
	// Read the Bmp File and populate the HeightMap
	if (!LoadHeightMap(heightMapFilename))
	{
		OutputDebugStringA("FailedToLoadTheBmpFile!");
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

	// Create input layOut
	DX::ThrowIfFailed(
		device->CreateInputLayout(
			DirectX::VertexPositionNormalColorTexture::InputElements,
			DirectX::VertexPositionNormalColorTexture::InputElementCount,
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
		Initialize(gameContext, m_heightMapFilename.c_str(), m_textureFilename.c_str(), m_colorMapFilename.c_str(), m_heightScale, m_texutreTiling);
		m_needsReload = false;
	}

	if (!m_isVisible || !m_vertexBuffer || !m_indexBuffer) return;

	ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

	// Apply the world Tranform form Tranformcomponent
	DirectX::SimpleMath::Matrix finalworld = world;

	TransformComponent* transform = m_owner->GetComponent<HEIN::TransformComponent>();
	if (transform)
	{
		finalworld = transform->GetWorldMatrix();
	}
	context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
	// If the frameWire mode is enabled , switch to rasterizer state 
	if (m_isWireFrame) context->RSSetState(gameContext.commonStates.Wireframe());
	else context->RSSetState(gameContext.commonStates.CullClockwise());

	// Set Vertex/Index Buffer and Input LayOut

	UINT stride = sizeof(DirectX::VertexPositionNormalColorTexture);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	// Update Matix Constant Buffer
	D3D11_MAPPED_SUBRESOURCE mappdedResource;
	if (SUCCEEDED(context->Map(m_matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappdedResource)))
	{
		MatrixBufferType* dataPtr = (MatrixBufferType*)mappdedResource.pData;
		// HLSL requires matrices to be transposed 
		dataPtr->world = finalworld.Transpose();
		dataPtr->view = view.Transpose();
		dataPtr->projection = proj.Transpose();
		context->Unmap(m_matrixBuffer.Get(), 0);
	}
	context->VSSetConstantBuffers(0, 1, m_matrixBuffer.GetAddressOf());

	if (SUCCEEDED(context->Map(m_lightBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappdedResource)))
	{
		LightBufferType* dataPtr = (LightBufferType*)mappdedResource.pData;

		DirectX::SimpleMath::Vector3 safeLightDir = m_lightDirection;
		safeLightDir.Normalize();

		dataPtr->diffuseColor = DirectX::SimpleMath::Vector4(m_diffuseColor.x, m_diffuseColor.y, m_diffuseColor.z, 1.0f);
		dataPtr->lightDirection = safeLightDir;
		dataPtr->hasTexture = m_texture ? 1.0f : 0.0f;
		context->Unmap(m_lightBuffer.Get(), 0);
	}
	context->PSSetConstantBuffers(1, 1, m_lightBuffer.GetAddressOf());

	context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

	if (m_texture)
	{
		context->PSSetShaderResources(0, 1, m_texture.GetAddressOf());
	}
	context->PSSetSamplers(0, 1, m_sampleState.GetAddressOf());

	// DRAW THE TERRAIN!
	context->DrawIndexed(m_indexCount, 0, 0);
	// Reset the rasterizer state to Default
	if (m_isWireFrame) context->RSSetState(gameContext.commonStates.CullClockwise());

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

		if (ImGui::DragFloat("HeightScale", &m_heightScale, 0.5f, 1.0f, 100.0f))
		{
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
				m_heightScale,
				m_texutreTiling
			);
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"Bitmap Files\0*.bmp\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_heightMapFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				Initialize(
					gameContext,
					m_heightMapFilename.c_str(), 
					m_textureFilename.c_str(),
					m_colorMapFilename.c_str(), 
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
			// BMP images are stored upside down, so read from bottom to top
			int pixelOffset = j * rowPitch + i * bytesPerPixel;
			
			// For 24/32 bit, we just read the first channel (B) since heightmaps are usually grayscale.
			// For 8-bit, it's the raw grayscale/palette index.
			height = bitmapImage[pixelOffset];
			
			index = (m_terrainHeight - 1 - j) * m_terrainWidth + i;

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)height / 255.0f; // Normalize 0 to 1
			m_heightMap[index].z = (float)j;
		}
	}

	delete[] bitmapImage;

	bitmapImage = 0;

	return true;
}

bool HEIN::TerrainComponent::CalculateNormals()
{
	// Initialize all normals to Zero
	for (int i = 0; i < m_vertexCount; i++)
	{
		m_heightMap[i].nx = 0.0f;
		m_heightMap[i].ny = 0.0f;
		m_heightMap[i].nz = 0.0f;
	}

	// Go Through every quad in the gird and calculate the face normals
	for (int j = 0; j < (m_terrainHeight - 1); j ++)
	{
		for (int i = 0; i < (m_terrainWidth - 1); i++)
		{
			int index1 = (m_terrainHeight - 1 - j) * m_terrainWidth + i; // Bottom Left
			int index2 = (m_terrainHeight - 1 - j) * m_terrainWidth + (i + 1); // Bottom Right
			int index3 = (m_terrainHeight - 1 - (j + 1)) * m_terrainWidth + i; // Up Left
			int index4 = (m_terrainHeight - 1 - (j + 1)) * m_terrainWidth + (i + 1); // Up Right

			// Triangle 1 
			DirectX::SimpleMath::Vector3 v1(m_heightMap[index3].x, m_heightMap[index3].y * m_heightScale, m_heightMap[index3].z);
			DirectX::SimpleMath::Vector3 v2(m_heightMap[index4].x, m_heightMap[index4].y * m_heightScale, m_heightMap[index4].z);
			DirectX::SimpleMath::Vector3 v3(m_heightMap[index1].x, m_heightMap[index1].y * m_heightScale, m_heightMap[index1].z);

			DirectX::SimpleMath::Vector3 edge1 = v2 - v1;
			DirectX::SimpleMath::Vector3 edge2 = v3 - v1;
			DirectX::SimpleMath::Vector3 normal1 = edge1.Cross(edge2); // Math to find the direction the face points

			m_heightMap[index3].nx += normal1.x; m_heightMap[index3].ny += normal1.y; m_heightMap[index3].nz += normal1.z;
			m_heightMap[index4].nx += normal1.x; m_heightMap[index4].ny += normal1.y; m_heightMap[index4].nz += normal1.z;
			m_heightMap[index1].nx += normal1.x; m_heightMap[index1].ny += normal1.y; m_heightMap[index1].nz += normal1.z;

			// Triangle 2
			v1 = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y * m_heightScale, m_heightMap[index1].z);
			v2 = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y * m_heightScale, m_heightMap[index4].z);
			v3 = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y * m_heightScale, m_heightMap[index2].z);

			edge1 = v2 - v1;
			edge2 = v3 - v1;
			normal1 = edge1.Cross(edge2);

			m_heightMap[index1].nx += normal1.x; m_heightMap[index1].ny += normal1.y; m_heightMap[index1].nz += normal1.z;
			m_heightMap[index4].nx += normal1.x; m_heightMap[index4].ny += normal1.y; m_heightMap[index4].nz += normal1.z;
			m_heightMap[index2].nx += normal1.x; m_heightMap[index2].ny += normal1.y; m_heightMap[index2].nz += normal1.z;
		}
	}
	for (int i = 0; i < m_vertexCount; i++)
	{
		DirectX::SimpleMath::Vector3 n(m_heightMap[i].nx, m_heightMap[i].ny, m_heightMap[i].nz);
		n.Normalize();
		m_heightMap[i].nx = n.x;
		m_heightMap[i].ny = n.y;
		m_heightMap[i].nz = n.z;
	}

	return true;
}

bool HEIN::TerrainComponent::InitializeBuffer(ID3D11Device* device)
{
	// Indices (6 indices per quad (2 triangles) to connect the vertices)
	m_indexCount = (m_terrainWidth - 1) * (m_terrainHeight - 1) * 6;

	std::vector<DirectX::VertexPositionNormalColorTexture> vertices(m_vertexCount);
	std::vector<uint32_t> indices(m_indexCount);

	float halfWidth = (float)m_terrainWidth / 2.0f;
	float halfDepth = (float)m_terrainHeight / 2.0f;

	// Create the Unique Vertices
	for (int i = 0; i < m_vertexCount; i++)
	{
		vertices[i].position.x = m_heightMap[i].x - halfWidth;
		vertices[i].position.y = m_heightMap[i].y * m_heightScale;
		vertices[i].position.z = m_heightMap[i].z - halfDepth;

		vertices[i].normal.x = m_heightMap[i].nx;
		vertices[i].normal.y = m_heightMap[i].ny;
		vertices[i].normal.z = m_heightMap[i].nz;

		int gridX = i % m_terrainWidth;
		int gridY = i / m_terrainWidth;

		float u = ((float)gridX / (float)(m_terrainWidth - 1));
		float v = ((float)gridY / (float)(m_terrainHeight - 1));

		// To flip Vertically(Upside Down)
		v = 1.0f - v;

		// To flip Horizonally(Mirroed)
		//u = 1.0f - u;

		vertices[i].textureCoordinate.x = u * m_texutreTiling;
		vertices[i].textureCoordinate.y = v * m_texutreTiling;

		// Maps exactly 1 full texture per terrain quad
		/*vertices[i].textureCoordinate.x = (float)gridX;
		vertices[i].textureCoordinate.y = (float)gridY;*/

		// Pass the exact painted color to the GPU
		vertices[i].color.x = m_heightMap[i].r;
		vertices[i].color.y = m_heightMap[i].g;
		vertices[i].color.z = m_heightMap[i].b;
		vertices[i].color.w = 1.0f; // Alpha

	}

	// Create the Indices
	int index = 0;
	for (int j = 0; j < (m_terrainHeight - 1); j++)
	{
		for (int i = 0; i < (m_terrainWidth - 1); i++)
		{
			int index1 = (m_terrainHeight - 1 - j) * m_terrainWidth + i; // Bottom Left
			int index2 = (m_terrainHeight - 1 - j) * m_terrainWidth + (i + 1); // Bottom Right
			int index3 = (m_terrainHeight - 1 - (j + 1)) * m_terrainWidth + i; // Up Left
			int index4 = (m_terrainHeight - 1 - (j + 1)) * m_terrainWidth + (i + 1); // Up Right

			indices[index++] = index3;
			indices[index++] = index4;
			indices[index++] = index1;

			indices[index++] = index1;
			indices[index++] = index4;
			indices[index++] = index2;
		}
	}

	// Create the DirectX Buffer
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(DirectX::VertexPositionNormalColorTexture) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = vertices.data();
	DX::ThrowIfFailed(
		device->CreateBuffer(&vertexBufferDesc, &vertexData, m_vertexBuffer.ReleaseAndGetAddressOf())
	);

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(uint32_t) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices.data();
	DX::ThrowIfFailed(
		device->CreateBuffer(&indexBufferDesc, &indexData, m_indexBuffer.ReleaseAndGetAddressOf())
	);

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

	// Calculate the "Row Pitch" (the actual length of a row in bytes).
	// RULE: The BMP file format requires every row of pixels to be padded with 
	// empty bytes so that its total length is always a multiple of 4.
	// The math `(x + 3) & ~3` is a fast bitwise trick that rounds the byte count 
	// UP to the nearest multiple of 4.
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
			int pixelOffset = j * rowPitch + i * bytesPerPixel;
			index = (m_terrainHeight - 1 - j) * m_terrainWidth + i;

			// Windows BMP files store pixels in BGR (Blue, Green, Red) format!
			m_heightMap[index].b = (float)bitmapImage[pixelOffset] / 255.0f;
			m_heightMap[index].g = (float)bitmapImage[pixelOffset + 1] / 255.0f;
			m_heightMap[index].r = (float)bitmapImage[pixelOffset + 2] / 255.0f;
		}
	}

	delete[] bitmapImage;
	return true;
}
