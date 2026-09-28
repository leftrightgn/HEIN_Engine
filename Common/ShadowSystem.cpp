#include "pch.h"
#include "ShadowSystem.h"

namespace HEIN {
    void ShadowSystem::Initialize(ID3D11Device* device, int width, int height) {
        m_width = width;
        m_height = height;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count = 1;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, m_shadowMap.ReleaseAndGetAddressOf());
        assert(SUCCEEDED(hr) && "Failed to create Shadow Map Texture2D");

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, m_shadowDSV.ReleaseAndGetAddressOf());
        assert(SUCCEEDED(hr) && "Failed to create Shadow Map DSV");

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        hr = device->CreateShaderResourceView(m_shadowMap.Get(), &srvDesc, m_shadowSRV.ReleaseAndGetAddressOf());
        assert(SUCCEEDED(hr) && "Failed to create Shadow Map SRV");

        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        sampDesc.BorderColor[0] = 1.0f;
        sampDesc.BorderColor[1] = 1.0f;
        sampDesc.BorderColor[2] = 1.0f;
        sampDesc.BorderColor[3] = 1.0f;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        device->CreateSamplerState(&sampDesc, m_shadowSampler.ReleaseAndGetAddressOf());

        m_shadowViewport.TopLeftX = 0.0f;
        m_shadowViewport.TopLeftY = 0.0f;
        m_shadowViewport.Width = static_cast<float>(width);
        m_shadowViewport.Height = static_cast<float>(height);
        m_shadowViewport.MinDepth = 0.0f;
        m_shadowViewport.MaxDepth = 1.0f;
    }

    void ShadowSystem::BindShadowMap(ID3D11DeviceContext* context) {
        // Ensure shadow map SRV is unbound from pixel shader (slot 4) to eliminate D3D11 OM hazard
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(4, 1, &nullSRV);

        // Unbind any bound render targets to ensure depth-only rendering
        context->OMSetRenderTargets(0, nullptr, m_shadowDSV.Get());
        context->RSSetViewports(1, &m_shadowViewport);
        context->ClearDepthStencilView(m_shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    void ShadowSystem::UnbindShadowMap(ID3D11DeviceContext* context) {
        context->OMSetRenderTargets(0, nullptr, nullptr);
    }
}
