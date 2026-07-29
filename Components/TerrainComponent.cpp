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
	float heightScale
)
{
	m_heightMapFilename = heightMapFilename;
	m_heightScale = heightScale;

	ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

	// Read the Bmp File and populate the HeightMap
	if (!LoadHeightMap(heightMapFilename))
	{
		OutputDebugStringA("FailedToLoadTheBmpFile!");
		return false;
	}
	// Build the vertices/ indices and create the GPU Buffer
	if (!InitializeBuffer(device))
	{
		OutputDebugStringA("FailedToCreateBuffer!");
		return false;
	}

	// Initialize BasicEffect before using it for shader bytecode
	m_effect = std::make_unique<DirectX::BasicEffect>(device);
	m_effect->SetVertexColorEnabled(true);

	// Create InputLayout and vertextPositionColor
	const void* shaderByteCode;
	size_t byteCodeLength;
	m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

	DX::ThrowIfFailed(
		device->CreateInputLayout(
			DirectX::VertexPositionColor::InputElements,
			DirectX::VertexPositionColor::InputElementCount,
			shaderByteCode,
			byteCodeLength,
			m_inputLayout.ReleaseAndGetAddressOf()
		)
	);

	return true;
}

void HEIN::TerrainComponent::Draw(
	GameContext& gameContext, 
	const DirectX::SimpleMath::Matrix& world,
	const DirectX::SimpleMath::Matrix& view, 
	const DirectX::SimpleMath::Matrix& proj
)
{
	if (!m_isVisible || !m_vertexBuffer || !m_indexBuffer) return;

	ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

	// Apply the world Tranform form Tranformcomponent
	DirectX::SimpleMath::Matrix finalworld = world;

	TransformComponent* transform = m_owner->GetComponent<HEIN::TransformComponent>();
	if (transform)
	{
		finalworld = transform->GetWorldMatrix();
	}
	m_effect->SetWorld(finalworld);
	m_effect->SetView(view);
	m_effect->SetProjection(proj);

	context->IASetInputLayout(m_inputLayout.Get());

	// If the frameWire mode is enabled , switch to rasterizer state 
	if (m_isWireFrame) context->RSSetState(gameContext.commonStates.Wireframe());
	else context->RSSetState(gameContext.commonStates.CullClockwise());

	m_effect->Apply(context);

	UINT stride = sizeof(DirectX::VertexPositionColor);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->DrawIndexed(m_indexCount, 0, 0);

	// Reset the rasterizer state to Default
	if (m_isWireFrame) context->RSSetState(gameContext.commonStates.CullClockwise());

}
nlohmann::json HEIN::TerrainComponent::Serialize()
{
	nlohmann::json data = IComponent::Serialize();
	std::string narrowPath(m_heightMapFilename.begin(), m_heightMapFilename.end());
	data["HeightMapPath"] = narrowPath;
	data["HeightScale"] = m_heightScale;
	data["isWiredFrame"] = m_isWireFrame;

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
	if (data.contains("HeightScale")) m_heightScale = data["HeightScale"];
	if (data.contains("isWiredFrame")) m_isWireFrame = data["isWiredFrame"];
}

void HEIN::TerrainComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
	if (!m_heightMapFilename.empty())
	{
		Initialize(gameContext, m_heightMapFilename.c_str(), m_heightScale);
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

		ImGui::Separator();

		std::string pathStr = std::string(m_heightMapFilename.begin(), m_heightMapFilename.end());
		if (ImGui::InputText("HeightMap File", &pathStr, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			m_heightMapFilename = std::wstring(pathStr.begin(), pathStr.end());
			Initialize(gameContext, m_heightMapFilename.c_str(), m_heightScale);
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			std::wstring selectedFile = HEIN::EditorUtils::OpenFileDialog(L"Bitmap Files\0*.bmp\0All Files\0*.*\0", windowHandle);
			if (!selectedFile.empty())
			{
				m_heightMapFilename = HEIN::EditorUtils::MakeRelativePath(selectedFile);
				Initialize(gameContext, m_heightMapFilename.c_str(), m_heightScale);
			}
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

bool HEIN::TerrainComponent::InitializeBuffer(ID3D11Device* device)
{
	// Calculate the Count 
	m_vertexCount = m_terrainWidth * m_terrainHeight;

	// Indices (6 indices per quad (2 triangles) to connect the vertices)
	m_indexCount = (m_terrainWidth - 1) * (m_terrainHeight - 1) * 6;

	std::vector<DirectX::VertexPositionColor> vertices(m_vertexCount);
	std::vector<uint32_t> indices(m_indexCount);

	float halfWidth = (float)m_terrainWidth / 2.0f;
	float halfDepth = (float)m_terrainHeight / 2.0f;

	// Create the Unique Vertices
	for (int i = 0; i < m_vertexCount; i++)
	{
		vertices[i].position.x = m_heightMap[i].x - halfWidth;
		vertices[i].position.y = m_heightMap[i].y * m_heightScale;
		vertices[i].position.z = m_heightMap[i].z - halfDepth;

		float h = m_heightMap[i].y;

		DirectX::XMVECTOR color = DirectX::XMVectorLerp(
			DirectX::Colors::DarkGreen, DirectX::Colors::White, h
		);
		DirectX::XMStoreFloat4(&vertices[i].color, color);

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
	vertexBufferDesc.ByteWidth = sizeof(DirectX::VertexPositionColor) * m_vertexCount;
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
