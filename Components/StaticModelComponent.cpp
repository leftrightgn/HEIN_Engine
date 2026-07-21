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
