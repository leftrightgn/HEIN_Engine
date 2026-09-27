#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>

namespace HEIN {
    class ShadowSystem {
    public:
        void Initialize(ID3D11Device* device, int width, int height);
        void BindShadowMap(ID3D11DeviceContext* context);
        ID3D11ShaderResourceView* GetShadowMapSRV() const { return m_shadowSRV.Get(); }
        void SetLightViewProj(const DirectX::SimpleMath::Matrix& matrix) { m_lightViewProj = matrix; }
        DirectX::SimpleMath::Matrix GetLightViewProj() const { return m_lightViewProj; }
        ID3D11SamplerState* GetShadowSampler() const { return m_shadowSampler.Get(); }

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_shadowMap;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_shadowDSV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowSRV;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSampler;
        D3D11_VIEWPORT m_shadowViewport;
        int m_width, m_height;
        DirectX::SimpleMath::Matrix m_lightViewProj;
    };
}
