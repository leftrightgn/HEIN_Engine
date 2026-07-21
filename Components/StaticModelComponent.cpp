#include "pch.h"
#include "StaticModelComponent.h"

HEIN::StaticModelComponent::StaticModelComponent(Actor* owner)
	: IComponent(owner) 
{
}

void HEIN::StaticModelComponent::Initialize(
    GameContext& gameContext, 
    const wchar_t* modelPath,
    const wchar_t* textureDir
)
{
    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    m_fxFactory = std::make_unique<DirectX::EffectFactory>(device);
    static_cast<DirectX::EffectFactory*>(m_fxFactory.get())->SetDirectory(textureDir);

    m_model = DirectX::Model::CreateFromSDKMESH(device, modelPath, *m_fxFactory);
}

void HEIN::StaticModelComponent::Update(float)
{
}

void HEIN::StaticModelComponent::Draw(
    GameContext& gameContext, 
    const DirectX::SimpleMath::Matrix& world,
    const DirectX::SimpleMath::Matrix& view, 
    const DirectX::SimpleMath::Matrix& proj
)
{
    if (!m_isVisible || !m_model) return;

    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    DirectX::DX11::CommonStates& states = gameContext.commonStates;


    m_model->Draw(context, states, world, view, proj);
}

void HEIN::StaticModelComponent::ExtractMeshData(
    GameContext& gameContext, 
    std::vector<DirectX::SimpleMath::Vector3>& outVertices,
    std::vector<uint32_t>& outIndices)
{
    if (!m_model) return;

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

    uint32_t vertexOffset = 0;

    for (const auto& mesh : m_model->meshes)
    {
        for (const auto& part : mesh->meshParts)
        {
            uint32_t actualVertexCount = 0;
            // Extract the Vertices
            if (part->vertexBuffer)
            {
                D3D11_BUFFER_DESC vbDesc;

                part->vertexBuffer->GetDesc(&vbDesc);

                actualVertexCount = vbDesc.ByteWidth / part->vertexStride;

                // Reconfigure a copy of the descriptor to act as a staging Buffer
                vbDesc.Usage = D3D11_USAGE_STAGING;
                vbDesc.BindFlags = 0;
                vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

                Microsoft::WRL::ComPtr<ID3D11Buffer> stagingVB;
                device->CreateBuffer(&vbDesc, nullptr, stagingVB.GetAddressOf());

                // Tell the Gpu to copy the original model buffer to the staging Buffer
                context->CopyResource(stagingVB.Get(), part->vertexBuffer.Get());

                // Map (lock) the staging buffer so that cpu can read easily
                D3D11_MAPPED_SUBRESOURCE mappedVB;
                if (SUCCEEDED(context->Map(stagingVB.Get(), 0, D3D11_MAP_READ, 0, &mappedVB)))
                {
                    const uint8_t* basePointer = reinterpret_cast<const uint8_t*>(mappedVB.pData);

                    for (uint32_t i = 0; i < actualVertexCount; ++i)
                    {
                        const auto* pos = reinterpret_cast<const DirectX::SimpleMath::Vector3*>(basePointer + (i * part->vertexStride));

                        outVertices.push_back(*pos);
                    }

                    context->Unmap(stagingVB.Get(), 0); // Unlock the Buffer
                }

            }

            // Extract the Indices
            if (part->indexBuffer)
            {
                D3D11_BUFFER_DESC ibDesc;

                part->indexBuffer->GetDesc(&ibDesc);

                uint32_t actualVertexCount = ibDesc.ByteWidth / part->vertexStride;

                ibDesc.Usage = D3D11_USAGE_STAGING;
                ibDesc.BindFlags = 0;
                ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

                Microsoft::WRL::ComPtr<ID3D11Buffer> stagingIB;
                device->CreateBuffer(&ibDesc, nullptr, stagingIB.GetAddressOf());

                context->CopyResource(stagingIB.Get(), part->indexBuffer.Get());

                D3D11_MAPPED_SUBRESOURCE mappedIB;
                if (SUCCEEDED(context->Map(stagingIB.Get(), 0, D3D11_MAP_READ, 0, &mappedIB)))
                {
                    if (part->indexFormat == DXGI_FORMAT_R16_UINT)
                    {
                        const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(mappedIB.pData);

                        for (uint32_t i = 0; i < part->indexCount; ++i)
                        {
                            outIndices.push_back(indices16[part->startIndex + i] + vertexOffset);
                        }
                    }
                    else if (part->indexFormat = DXGI_FORMAT_R32_UINT)
                    {
                        const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(mappedIB.pData);

                        for (uint32_t i = 0; i < part->indexCount; ++i)
                        {
                            outIndices.push_back(indices32[part->startIndex + i] + vertexOffset);
                        }
                    }

                    context->Unmap(stagingIB.Get(), 0);
                }
            }
            vertexOffset += actualVertexCount;
        }
    }
}

DirectX::BoundingBox HEIN::StaticModelComponent::GetBoundingBox() const
{
    DirectX::BoundingBox totalBox;

    if (m_model != nullptr && !m_model->meshes.empty())
    {
        totalBox = m_model->meshes[0]->boundingBox;

        for (size_t i = 1; i < m_model->meshes.size(); i++) 
        {
            DirectX::BoundingBox::CreateMerged(totalBox, totalBox, m_model->meshes[i]->boundingBox);
        }
    }

    return totalBox;
}

DirectX::BoundingSphere HEIN::StaticModelComponent::GetBoundingSphere() const
{
    DirectX::BoundingSphere totalSphere;
    if (m_model != nullptr && !m_model->meshes.empty())
    {
        totalSphere = m_model->meshes[0]->boundingSphere;

        for (size_t i = 1; i < m_model->meshes.size(); i++)
        {
            DirectX::BoundingSphere::CreateMerged(totalSphere, totalSphere, m_model->meshes[i]->boundingSphere);
        }
    }
    return totalSphere;
}
