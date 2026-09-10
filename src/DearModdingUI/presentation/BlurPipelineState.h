#pragma once

#include <Platform/rendering/D3D11State.h>

namespace DearModdingUI::BackgroundBlur
{
	class BlurPipelineState
	{
	public:
		explicit BlurPipelineState(ID3D11DeviceContext* a_context) noexcept;
		~BlurPipelineState() noexcept;

		BlurPipelineState(const BlurPipelineState&) = delete;
		BlurPipelineState& operator=(const BlurPipelineState&) = delete;

	private:
		ID3D11DeviceContext* m_context;
		Rendering::RenderTargetState m_targets;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend;
		std::array<float, 4> m_blendFactor{};
		UINT m_sampleMask{};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencil;
		UINT m_stencilReference{};
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizer;
		std::array<D3D11_VIEWPORT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
			m_viewports{};
		UINT m_viewportCount{ static_cast<UINT>(m_viewports.size()) };
		std::array<D3D11_RECT, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE>
			m_scissors{};
		UINT m_scissorCount{ static_cast<UINT>(m_scissors.size()) };
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		D3D11_PRIMITIVE_TOPOLOGY m_topology{ D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED };
		Rendering::ShaderState<ID3D11VertexShader> m_vertex;
		Rendering::ShaderState<ID3D11PixelShader> m_pixel;
		Rendering::ShaderState<ID3D11GeometryShader> m_geometry;
		Rendering::ShaderState<ID3D11HullShader> m_hull;
		Rendering::ShaderState<ID3D11DomainShader> m_domain;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
		std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, 2> m_constantBuffers;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResource;
	};
}
