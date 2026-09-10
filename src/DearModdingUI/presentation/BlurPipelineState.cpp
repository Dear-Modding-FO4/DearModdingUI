#include "BlurPipelineState.h"

namespace DearModdingUI::BackgroundBlur
{
	BlurPipelineState::BlurPipelineState(ID3D11DeviceContext* a_context) noexcept :
		m_context(a_context),
		m_targets(a_context)
	{
		m_context->OMGetBlendState(
			m_blend.GetAddressOf(), m_blendFactor.data(), &m_sampleMask);
		m_context->OMGetDepthStencilState(m_depthStencil.GetAddressOf(), &m_stencilReference);
		m_context->RSGetState(m_rasterizer.GetAddressOf());
		m_context->RSGetViewports(&m_viewportCount, m_viewports.data());
		m_context->RSGetScissorRects(&m_scissorCount, m_scissors.data());
		m_context->IAGetInputLayout(m_inputLayout.GetAddressOf());
		m_context->IAGetPrimitiveTopology(&m_topology);
		m_context->VSGetShader(
			m_vertex.shader.GetAddressOf(), m_vertex.instances.data(), &m_vertex.count);
		m_context->PSGetShader(
			m_pixel.shader.GetAddressOf(), m_pixel.instances.data(), &m_pixel.count);
		m_context->GSGetShader(
			m_geometry.shader.GetAddressOf(), m_geometry.instances.data(), &m_geometry.count);
		m_context->HSGetShader(
			m_hull.shader.GetAddressOf(), m_hull.instances.data(), &m_hull.count);
		m_context->DSGetShader(
			m_domain.shader.GetAddressOf(), m_domain.instances.data(), &m_domain.count);

		m_context->PSGetSamplers(0, 1, m_sampler.GetAddressOf());
		std::array<ID3D11Buffer*, 2> constants{};
		m_context->PSGetConstantBuffers(0, static_cast<UINT>(constants.size()), constants.data());
		for (size_t index = 0; index < constants.size(); ++index)
			m_constantBuffers[index].Attach(constants[index]);
		m_context->PSGetShaderResources(0, 1, m_shaderResource.GetAddressOf());
	}

	BlurPipelineState::~BlurPipelineState() noexcept
	{
		const std::array<ID3D11Buffer*, 2> constants{
			m_constantBuffers[0].Get(), m_constantBuffers[1].Get()
		};
		auto* sampler = m_sampler.Get();
		auto* resource = m_shaderResource.Get();
		m_targets.Restore(m_context);
		m_context->OMSetBlendState(m_blend.Get(), m_blendFactor.data(), m_sampleMask);
		m_context->OMSetDepthStencilState(m_depthStencil.Get(), m_stencilReference);
		m_context->RSSetState(m_rasterizer.Get());
		m_context->RSSetViewports(
			m_viewportCount, m_viewportCount ? m_viewports.data() : nullptr);
		m_context->RSSetScissorRects(
			m_scissorCount, m_scissorCount ? m_scissors.data() : nullptr);
		m_context->IASetInputLayout(m_inputLayout.Get());
		m_context->IASetPrimitiveTopology(m_topology);
		m_context->VSSetShader(m_vertex.shader.Get(), m_vertex.instances.data(), m_vertex.count);
		m_context->PSSetShader(m_pixel.shader.Get(), m_pixel.instances.data(), m_pixel.count);
		m_context->GSSetShader(m_geometry.shader.Get(), m_geometry.instances.data(), m_geometry.count);
		m_context->HSSetShader(m_hull.shader.Get(), m_hull.instances.data(), m_hull.count);
		m_context->DSSetShader(m_domain.shader.Get(), m_domain.instances.data(), m_domain.count);
		m_context->PSSetSamplers(0, 1, &sampler);
		m_context->PSSetConstantBuffers(0, static_cast<UINT>(constants.size()), constants.data());
		m_context->PSSetShaderResources(0, 1, &resource);
	}
}
