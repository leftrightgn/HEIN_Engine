#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cassert>

namespace HEIN
{
	/**
	 * @class OffscreenRenderTarget
	 * @brief Encapsulates a complete off-screen DirectX 11 render target canvas.
	 *
	 * Manages a color texture (with RTV and SRV) and an associated depth-stencil buffer (with DSV).
	 * Enables rendering arbitrary 3D passes off-screen into an ImTextureID shader resource view
	 * for dockable viewport panels (Scene View and Game View).
	 */
	class OffscreenRenderTarget
	{
	public:
		OffscreenRenderTarget() = default;
		~OffscreenRenderTarget() { Release(); }

		OffscreenRenderTarget(const OffscreenRenderTarget&) = delete;
		OffscreenRenderTarget& operator=(const OffscreenRenderTarget&) = delete;

		OffscreenRenderTarget(OffscreenRenderTarget&&) noexcept = default;
		OffscreenRenderTarget& operator=(OffscreenRenderTarget&&) noexcept = default;

		bool Initialize(
			ID3D11Device* device,
			int width,
			int height,
			DXGI_FORMAT colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
			DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT
		);

		bool Resize(ID3D11Device* device, int newWidth, int newHeight);
		void Release();

		void Begin(ID3D11DeviceContext* context, const float clearColor[4] = nullptr);
		void End(ID3D11DeviceContext* context);

		ID3D11ShaderResourceView* GetShaderResourceView() const noexcept { return m_colorSRV.Get(); }
		ID3D11RenderTargetView* GetRenderTargetView() const noexcept { return m_colorRTV.Get(); }
		ID3D11DepthStencilView* GetDepthStencilView() const noexcept { return m_depthDSV.Get(); }
		ID3D11Texture2D* GetColorTexture() const noexcept { return m_colorTexture.Get(); }

		int GetWidth() const noexcept { return m_width; }
		int GetHeight() const noexcept { return m_height; }
		float GetAspectRatio() const noexcept { return (m_height > 0) ? (static_cast<float>(m_width) / static_cast<float>(m_height)) : 1.777778f; }
		const D3D11_VIEWPORT& GetViewport() const noexcept { return m_viewport; }

	private:
		int m_width = 0;
		int m_height = 0;
		DXGI_FORMAT m_colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_colorTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_colorRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_colorSRV;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthDSV;

		D3D11_VIEWPORT m_viewport = {};
	};
}
