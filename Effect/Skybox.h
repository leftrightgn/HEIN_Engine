#pragma once
#include "pch.h"
#include "Framework/GameContext.h"
#include "Effect/SkyboxEffect.h"
#include <GeometricPrimitive.h>

namespace HEIN
{
    class Skybox
    {
    public:
        void Initialize(GameContext& gameContext, const wchar_t* texturePath);
        void Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj);

    private:
        static const std::vector<D3D11_INPUT_ELEMENT_DESC> INPUT_LAYOUT;

        std::unique_ptr<DirectX::GeometricPrimitive> m_sky;
        std::unique_ptr<DX::SkyboxEffect> m_effect;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_skyInputLayout;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cubemap;
    };
}