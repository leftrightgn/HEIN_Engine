#include "pch.h"
#include "Water.h"
#include <vector>
#include <d3dcompiler.h>

//#include "ImaseLib/DirectXTK_ImGui.h"


Water::Water() 
    : m_indexCount(0)
    , m_totalTime(0) 
    , m_waterColor(DirectX::SimpleMath::Vector3(0.024f, 0.227f, 0.506f))
    , m_fresnelPower(10.0f)
    , m_specularPower(50.0f)
    , m_waveSpeed(1.0f)
    , m_normalBlend(0.017f)
    , m_spacing(0.5f)
    , m_offset(0.007f)
    , m_uvScale(0.04f)
    , m_multiplier(1.0f)
{}

Water::~Water() {}

void Water::Initialize(GameContext& gameContext, const wchar_t* textureFilename, const wchar_t* normalMapFilename, const wchar_t* noiseMapFilename)
{
    //  GENERATE GRID MESH (100x100) ---
    int width = 500;
    int height = 500;
    m_spacing = 0.5f; // Distance between vertices

    std::vector<VertexType> vertices;
    std::vector<unsigned long> indices;

    float startX = -((float)width * m_spacing) / 2.0f;
    float startZ = -((float)height * m_spacing) / 2.0f;

    for (int z = 0; z < height; ++z)
    {
        for (int x = 0; x < width; ++x)
        {
            VertexType v;
            v.position = DirectX::SimpleMath::Vector3(startX + x * m_spacing, 0.0f, startZ + z * m_spacing);
            v.normal = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);
            v.texture = DirectX::SimpleMath::Vector2(x * 0.1f, z * 0.1f); // Tiling texture
            vertices.push_back(v);
        }
    }

    // Generate Indices (Triangles)
    for (int z = 0; z < height - 1; ++z)
    {
        for (int x = 0; x < width - 1; ++x)
        {
            int bottomLeft = z * width + x;
            int bottomRight = bottomLeft + 1;
            int topLeft = (z + 1) * width + x;
            int topRight = topLeft + 1;

            // Triangle 1
            indices.push_back(bottomLeft);
            indices.push_back(topLeft);
            indices.push_back(bottomRight);

            // Triangle 2
            indices.push_back(bottomRight);
            indices.push_back(topLeft);
            indices.push_back(topRight);
        }
    }
    m_indexCount = (int)indices.size();

    auto device = gameContext.deviceResources.GetD3DDevice();
    // Create Vertex Buffer
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(VertexType) * (UINT)vertices.size();
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertices.data();
    DX::ThrowIfFailed(device->CreateBuffer(&vbd, &vinitData, m_vertexBuffer.ReleaseAndGetAddressOf()));

    // Create Index Buffer
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(unsigned long) * (UINT)indices.size();
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = indices.data();
    DX::ThrowIfFailed(device->CreateBuffer(&ibd, &iinitData, m_indexBuffer.ReleaseAndGetAddressOf()));

    // Create Setting Buffer
    D3D11_BUFFER_DESC sbd = {};
    sbd.Usage = D3D11_USAGE_DEFAULT;
    sbd.ByteWidth = sizeof(WaterSettingsBuffer);
    sbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    DX::ThrowIfFailed(device->CreateBuffer(&sbd, nullptr, m_watersettingsBuffer.ReleaseAndGetAddressOf()));

    // LOAD SHADERS ---
   
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob;
    D3DCompileFromFile(L"Shaders/WaterVS.hlsl", nullptr, nullptr, "main", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompileFromFile(L"Shaders/WaterPS.hlsl", nullptr, nullptr, "main", "ps_4_0", 0, 0, &psBlob, nullptr);

    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.ReleaseAndGetAddressOf());
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.ReleaseAndGetAddressOf());

    // Create Input Layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_layout.ReleaseAndGetAddressOf());


    // CREATE CONSTANT BUFFERS ---
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(WaterConstantBuffer); // Must be multiple of 16
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbd, nullptr, m_constantBuffer.ReleaseAndGetAddressOf());

    cbd.ByteWidth = sizeof(LightBuffer);
    device->CreateBuffer(&cbd, nullptr, m_lightBuffer.ReleaseAndGetAddressOf());


    // LOAD TEXTURE ---
    DirectX::CreateDDSTextureFromFile(device, textureFilename, nullptr, m_texture.ReleaseAndGetAddressOf());

    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(device, normalMapFilename, nullptr, m_normalMapTexture.ReleaseAndGetAddressOf()));
    DX::ThrowIfFailed(DirectX::CreateDDSTextureFromFile(device, noiseMapFilename, nullptr, m_noiseMapTexture.ReleaseAndGetAddressOf()));

    // Create SamplerState
    D3D11_SAMPLER_DESC sampleDes = {};
    sampleDes.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampleDes.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDes.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDes.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampleDes.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampleDes.MinLOD = 0;
    sampleDes.MaxLOD = D3D11_FLOAT32_MAX;

    DX::ThrowIfFailed(device->CreateSamplerState(&sampleDes, m_samplerState.ReleaseAndGetAddressOf()));


}

void Water::Update(float deltaTime)
{
    m_totalTime += deltaTime;
}

void Water::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection, const DirectX::SimpleMath::Vector3& cameraPos)
{
    auto context = gameContext.deviceResources.GetD3DDeviceContext();
    // Update Constant Buffer (Matrices + Time)
    WaterConstantBuffer data;

    float snapX = std::floor(cameraPos.x / 10.0f) * 10.0f;
    float snapZ = std::floor(cameraPos.z / 10.0f) * 10.0f;

    data.world = DirectX::SimpleMath::Matrix::CreateTranslation(snapX, 2.0f, snapZ);
    data.view = view.Transpose();
    data.projection = projection.Transpose();
    data.time = m_totalTime;

    // Transpose matrices for HLSL
    data.world = data.world.Transpose();

    context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &data, 0, 0);

    // Update Light Buffer
    LightBuffer lightData;
    lightData.lightDir = DirectX::SimpleMath::Vector3(0.5f, -1.0f, 0.5f); // Example light direction
    lightData.time = m_totalTime;
    lightData.camPos = cameraPos;
    lightData.padding2 = 0.0f;
    context->UpdateSubresource(m_lightBuffer.Get(), 0, nullptr, &lightData, 0, 0);

    // Update Settings Buffer
    WaterSettingsBuffer settingData;
    settingData.waterColor = m_waterColor;
    settingData.fresnelPower = m_fresnelPower;
    settingData.specularPower = m_specularPower;
    settingData.waveSpeed = m_waveSpeed;
    settingData.normalBlend = m_normalBlend;
    settingData.padding3 = 0.0f;
    settingData.uvScale = m_uvScale;
    settingData.offset = m_offset;
    settingData.multiplier = m_multiplier;
    context->UpdateSubresource(m_watersettingsBuffer.Get(), 0, nullptr, &settingData, 0, 0);

    // Set IA Layout
    UINT stride = sizeof(VertexType); // bytes between each vertex
    UINT offset = 0;                  // start from the beginning
    // Bind vertex buffer to the gpu
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    // Bind the index buffer R32_UINT mean 32 bit unsigned int 
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    // Tell the gpu how to interpret indices. 3indices = 1 independent triangle
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // Bind the input layout
    context->IASetInputLayout(m_layout.Get());

    // Set Shaders & Buffers
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    context->VSSetConstantBuffers(1, 1, m_watersettingsBuffer.GetAddressOf());

    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    context->PSSetConstantBuffers(1, 1, m_watersettingsBuffer.GetAddressOf());
    context->PSSetConstantBuffers(2, 1, m_lightBuffer.GetAddressOf());
    context->PSSetShaderResources(0, 1, m_texture.GetAddressOf());
    context->PSSetShaderResources(1, 1, m_normalMapTexture.GetAddressOf());
    context->VSSetShaderResources(0, 1, m_noiseMapTexture.GetAddressOf());
    context->VSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

   

    // Draw
    context->DrawIndexed(m_indexCount, 0, 0);
}

//void Water::DrawDebugUI()
//{
//#ifdef _DEBUG
//    ImGui::Begin("Water Settings");
//
//    ImGui::ColorEdit3("Water Color", &m_waterColor.x);
//
//    ImGui::SliderFloat("Wave Speed", &m_waveSpeed, 0.0f, 5.0f);
//    ImGui::SliderFloat("Normal Blend", &m_normalBlend, 0.0f, 1.0f);
//    ImGui::SliderFloat("Specular Power", &m_specularPower, 1.0f, 100.0f);
//    ImGui::SliderFloat("Fresnel Power", &m_fresnelPower, 1.0f, 300.0f);
//    ImGui::InputFloat("Spacing", &m_spacing, 0.0f, 10.0f);
//    ImGui::InputFloat("uvScale", &m_uvScale, 0.0f, 10.0f);
//    ImGui::InputFloat("offset", &m_offset, 0.0f, 10.0f);
//    ImGui::InputFloat("multiplier", &m_multiplier, 0.0f, 10.0f);
//
//    ImGui::End();
//#endif // _DEBUG
//
//}
