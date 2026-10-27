#include "pch.h"
#include "OffscreenRenderTarget.h"

namespace HEIN
{
	bool OffscreenRenderTarget::Initialize(
		ID3D11Device* device,
		int width,
		int height,
		DXGI_FORMAT colorFormat,
		DXGI_FORMAT depthFormat
	)
	{
		if (!device || width <= 0 || height <= 0) return false;

		Release();

		m_width = width;
		m_height = height;
		m_colorFormat = colorFormat;
		m_depthFormat = depthFormat;

		// Create Color Texture
		D3D11_TEXTURE2D_DESC colorDesc = {};
		colorDesc.Width = static_cast<UINT>(m_width);
		colorDesc.Height = static_cast<UINT>(m_height);
		colorDesc.MipLevels = 1;
		colorDesc.ArraySize = 1;
		colorDesc.Format = m_colorFormat;
		colorDesc.SampleDesc.Count = 1;
		colorDesc.SampleDesc.Quality = 0;
		colorDesc.Usage = D3D11_USAGE_DEFAULT;
		colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		colorDesc.CPUAccessFlags = 0;
		colorDesc.MiscFlags = 0;

		HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, m_colorTexture.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return false;

		// Create Render Target View
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_colorFormat;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		hr = device->CreateRenderTargetView(m_colorTexture.Get(), &rtvDesc, m_colorRTV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return false;

		// Create Shader Resource View (for Dear ImGui Image rendering)
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = m_colorFormat;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		hr = device->CreateShaderResourceView(m_colorTexture.Get(), &srvDesc, m_colorSRV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return false;

		// Create Depth Texture
		D3D11_TEXTURE2D_DESC depthDesc = {};
		depthDesc.Width = static_cast<UINT>(m_width);
		depthDesc.Height = static_cast<UINT>(m_height);
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = m_depthFormat;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.MiscFlags = 0;

		hr = device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return false;

		// Create Depth Stencil View
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_depthFormat;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		hr = device->CreateDepthStencilView(m_depthTexture.Get(), &dsvDesc, m_depthDSV.ReleaseAndGetAddressOf());
		if (FAILED(hr)) return false;

		// Configure Viewport
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<float>(m_width);
		m_viewport.Height = static_cast<float>(m_height);
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		return true;
	}

	bool OffscreenRenderTarget::Resize(ID3D11Device* device, int newWidth, int newHeight)
	{
		if (!device || newWidth <= 0 || newHeight <= 0) return false;
		if (newWidth == m_width && newHeight == m_height && m_colorRTV) return true;

		return Initialize(device, newWidth, newHeight, m_colorFormat, m_depthFormat);
	}

	void OffscreenRenderTarget::Release()
	{
		m_colorTexture.Reset();
		m_colorRTV.Reset();
		m_colorSRV.Reset();
		m_depthTexture.Reset();
		m_depthDSV.Reset();
		m_width = 0;
		m_height = 0;
	}

	void OffscreenRenderTarget::Begin(ID3D11DeviceContext* context, const float clearColor[4])
	{
		if (!context || !m_colorRTV || !m_depthDSV) return;

		// Default clear color: dark studio neutral gray (0.12f, 0.12f, 0.14f, 1.0f)
		static const float kDefaultClearColor[4] = { 0.12f, 0.12f, 0.14f, 1.0f };
		const float* colorToUse = clearColor ? clearColor : kDefaultClearColor;

		// Clear color buffer and depth-stencil buffer
		context->ClearRenderTargetView(m_colorRTV.Get(), colorToUse);
		context->ClearDepthStencilView(m_depthDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// Unbind any leftover pixel shader SRVs that might be bound from previous frames
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		context->PSSetShaderResources(0, 1, nullSRV);

		// Bind Render Target and Depth Stencil
		ID3D11RenderTargetView* rtvArray[1] = { m_colorRTV.Get() };
		context->OMSetRenderTargets(1, rtvArray, m_depthDSV.Get());

		// Set rasterizer viewport
		context->RSSetViewports(1, &m_viewport);
	}

	void OffscreenRenderTarget::End(ID3D11DeviceContext* context)
	{
		if (!context) return;

		// Unbind render targets to prevent D3D11 output-merger hazards when sampled as an SRV in ImGui
		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		context->OMSetRenderTargets(1, nullRTV, nullptr);
	}
} // namespace HEIN
