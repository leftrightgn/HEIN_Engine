#include "pch.h"
#include "Skybox.h"

const std::vector<D3D11_INPUT_ELEMENT_DESC> HEIN::Skybox::INPUT_LAYOUT =
{
    // SemanticName, SemanticIndex, Format, InputSlot, AlignedByteOffset, ...
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    // DXGI_FORMAT_R32G32B32_FLOAT is data type/ R32G32B32 means three 32-bit channels
   // FLOAT means they're floating point numbers. So this reads three floats — your x, y, and z. 
   // If you needed a fourth w component it would be R32G32B32A32_FLOAT.
   // D3D11_INPUT_PER_VERTEX_DATA — tells the GPU to advance to the next vertex's data for each vertex
   // drawn, which is the normal behaviour. The alternative D3D11_INPUT_PER_INSTANCE_DATA is for a more
   // advanced technique called instancing where you draw many copies of a mesh at once.

};


void HEIN::Skybox::Initialize(GameContext& gameContext, const wchar_t* texturePath)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    m_sky = DirectX::GeometricPrimitive::CreateGeoSphere(gameContext.deviceResources.GetD3DDeviceContext(), 2.f, 3, false /*invert for being inside the shape*/);

    m_effect = std::make_unique<DX::SkyboxEffect>(gameContext.deviceResources.GetD3DDevice());

  
  
    // Get the shader bytecode from your effect
    const void* shaderByteCode = nullptr;
    size_t byteCodeLength = 0;
    m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    // Manually create the layout
    DX::ThrowIfFailed(
        device->CreateInputLayout(
            &INPUT_LAYOUT[0],
            static_cast<UINT>(INPUT_LAYOUT.size()),
            shaderByteCode,
            byteCodeLength,
            m_skyInputLayout.ReleaseAndGetAddressOf()
        )
    );
    DX::ThrowIfFailed(
        DirectX::CreateDDSTextureFromFile(device, texturePath,
            nullptr, m_cubemap.ReleaseAndGetAddressOf()));

    m_effect->SetTexture(m_cubemap.Get());
}

void HEIN::Skybox::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
{
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    m_effect->SetView(view);
    m_effect->SetProjection(proj);

    // Turn OFF depth writing so the skybox stays in the background
    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);
    context->RSSetState(gameContext.commonStates.CullNone());

    m_sky->Draw(m_effect.get(), m_skyInputLayout.Get());

    // Restore normal depth state for the rest of the game
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
}
